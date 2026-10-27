#include "Core/SlopGameInstance.h"
#include "Core/SlopAudio.h"
#include "Core/SlopLog.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Scalability.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/SlopHud.h"
#include "UI/SlopScreen.h"
#include "UI/SlopScreens.h"
#include "UI/SlopStyle.h"
#include "View/SlopMatch.h"
#include "View/SlopShowroom.h"
#include "View/SlopDevTour.h"
#include "View/SlopFighterActor.h"
#include "Widgets/SWindow.h"
#if PLATFORM_LINUX
THIRD_PARTY_INCLUDES_START
#include <SDL3/SDL.h>
THIRD_PARTY_INCLUDES_END
#endif

#define LOCTEXT_NAMESPACE "SlopGame"

using namespace Slop;

USlopGameInstance* USlopGameInstance::Get(const UObject* Context)
{
	UWorld* W = Context ? Context->GetWorld() : nullptr;
	return W ? Cast<USlopGameInstance>(W->GetGameInstance()) : Get();
}

USlopGameInstance* USlopGameInstance::Get()
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetGameInstance())
	{
		return Cast<USlopGameInstance>(GEngine->GameViewport->GetGameInstance());
	}
	return nullptr;
}

double USlopGameInstance::Now() const { return FPlatformTime::Seconds(); }

void USlopGameInstance::Init()
{
	Super::Init();
	FContent& C = GetContent();
	for (const FString& E : C.Errors)
	{
		UE_LOG(LogSlop, Error, TEXT("Game data: %s"), *E);
	}
	Settings.Load();
	Progress.Load();
	for (const FString& N : {Settings.LoadNote, Progress.LoadNote})
	{
		if (!N.IsEmpty())
		{
			UE_LOG(LogSlop, Log, TEXT("%s"), *N);
		}
	}
	Router = MakeShared<FSlopInputRouter>(Settings);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterInputPreProcessor(Router, 0);
		ActiveHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &USlopGameInstance::OnActivation);
	}
	ConnHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &USlopGameInstance::OnDeviceConnection);
	Audio = NewObject<USlopAudio>(this);
	Audio->Init(this);
	Flow.SetNow(Now());
#if !UE_BUILD_SHIPPING
	bDebugKeys = FParse::Param(FCommandLine::Get(), TEXT("debugkeys"));
#endif
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USlopGameInstance::Tick));
	UE_LOG(LogSlop, Log, TEXT("Mortal Slopbat started: %d fighters, settings %s, progress %s"), C.Fighters.Num(), *FSlopSettings::Path(), *FSlopProgress::Path());
}

void USlopGameInstance::Shutdown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnHandle);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Router);
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActiveHandle);
	}
	if (Audio)
	{
		Audio->StopAll();
	}
	Super::Shutdown();
}

void USlopGameInstance::OnStart()
{
	Super::OnStart();
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	StartedAt = Now();
	BuildRoot();
	ApplySettings(true);
	Audio->Preload();
	if (FParse::Param(FCommandLine::Get(), TEXT("SlopFrameStats")))
	{
		const FString Path = SaveDir() / FString::Printf(TEXT("framestats-%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		FrameStats = MakeUnique<FFrameStats>(Path);
		UE_LOG(LogSlop, Log, TEXT("Frame stats to %s"), *Path);
	}
	TestCommandLine();
	DevCommandLine();
	if (!Screen.IsValid())
	{
		Goto(GetContent().Errors.Num() > 0 ? SlopScreens::DataError() : SlopScreens::Title());
	}
	// What happened to the save files, if anything did, once, over the title.
	const FString Notes = FString::Join(TArray<FString>({Settings.LoadNote, Progress.LoadNote}).FilterByPredicate([](const FString& N) { return !N.IsEmpty(); }), TEXT("\n"));
	if (!Notes.IsEmpty())
	{
		Notice(FText::FromString(Notes), 8.0);
	}
}

void USlopGameInstance::BuildRoot()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	SAssignNew(Root, SOverlay)
	+ SOverlay::Slot()[SAssignNew(ScreenSlot, SBox)]
	+ SOverlay::Slot()[SAssignNew(HudSlot, SBox)]
	+ SOverlay::Slot()[SAssignNew(OverlaySlot, SBox)]
	+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Center).Padding(0, 24)
	[
		SAssignNew(NoticeSlot, SBox)
		[
			SNew(STextBlock).Font(SlopUI::Body(28.f)).ColorAndOpacity(SlopUI::Amber)
			.ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))
			.Text_Lambda([this]() { return Now() < NoticeUntil ? NoticeText : FText::GetEmpty(); })
		]
	];
	GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

void USlopGameInstance::Goto(const TSharedPtr<FSlopScreen>& S)
{
	if (Screen.IsValid())
	{
		Screen->Leave();
	}
	Screen = S;
	if (!Screen.IsValid() || !ScreenSlot.IsValid())
	{
		return;
	}
	Screen->GI = this;
	Note(TEXT("Screen: ") + Screen->ScreenName);
	ScreenSlot->SetContent(Screen->Build());
	Screen->Enter();
	if (!Screen->ShowsMatch() && Match)
	{
		EndMatch();
	}
}

void USlopGameInstance::RebuildScreen()
{
	if (Screen.IsValid() && ScreenSlot.IsValid())
	{
		ScreenSlot->SetContent(Screen->Build());
	}
	if (Overlay.IsValid() && OverlaySlot.IsValid())
	{
		OverlaySlot->SetContent(Overlay->Build());
	}
}

void USlopGameInstance::ShowOverlay(const TSharedPtr<FSlopScreen>& S)
{
	if (Overlay.IsValid())
	{
		Overlay->Leave();
	}
	Overlay = S;
	if (!OverlaySlot.IsValid())
	{
		return;
	}
	if (Overlay.IsValid())
	{
		Overlay->GI = this;
		Note(TEXT("Overlay: ") + Overlay->ScreenName);
		OverlaySlot->SetContent(Overlay->Build());
		Overlay->Enter();
	}
	else
	{
		OverlaySlot->SetContent(SNullWidget::NullWidget);
	}
}

void USlopGameInstance::CloseOverlay() { ShowOverlay(nullptr); }

void USlopGameInstance::Notice(const FText& Text, double Seconds)
{
	NoticeText = Text;
	NoticeUntil = Now() + Seconds;
}

FString USlopGameInstance::MeshFor(FName Fighter)
{
	const FString Id = Fighter.ToString();
	return FString::Printf(TEXT("/Game/Slop/Fighters/%s/%s.%s"), *Id, *Id, *Id);
}

void USlopGameInstance::StartMatch()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	EndMatch();
	CloseOverlay();
	Router->SetSolo(!Flow.TwoPlayers());
	Router->ResetAll();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Match = W->SpawnActor<ASlopMatch>(P);
	if (!Match || !Match->Start(Flow.Config))
	{
		Notice(LOCTEXT("MatchFailed", "The match could not start."));
		if (Match)
		{
			Match->Destroy();
			Match = nullptr;
		}
		Goto(SlopScreens::MainMenu());
		return;
	}
	Match->OnFinished = [this](const FMatchResult& R) { OnMatchFinished(R); };
	Router->bFightActive = true;
	SAssignNew(Hud, SSlopHud).GI(this).Match(Match);
	if (HudSlot.IsValid())
	{
		HudSlot->SetContent(Hud.ToSharedRef());
	}
	Goto(SlopScreens::MatchScreen());
}

void USlopGameInstance::EndMatch()
{
	Router->bFightActive = false;
	if (HudSlot.IsValid())
	{
		HudSlot->SetContent(SNullWidget::NullWidget);
	}
	Hud.Reset();
	if (Match)
	{
		Match->Destroy();
		Match = nullptr;
	}
}

void USlopGameInstance::OnMatchFinished(const FMatchResult& R)
{
	LastResult = R;
	Note(FString::Printf(TEXT("Result: %s %d-%d %s, winner %d%s%s, %d s left"), *R.P1.ToString(), R.Wins[0], R.Wins[1], *R.P2.ToString(), R.Winner,
		R.bFlawless ? TEXT(", flawless") : TEXT(""), R.Finisher.IsNone() ? TEXT("") : *(TEXT(", ") + R.Finisher.ToString()), R.TimeLeft));
	Router->bFightActive = false;
	Progress.Count(TEXT("matches"));
	if (R.Winner == 0 && Flow.Config.Mode != EMode::Versus)
	{
		Progress.Count(TEXT("wins"));
	}
	if (!R.Finisher.IsNone())
	{
		Progress.Count(TEXT("finishers"));
	}
	if (R.bFlawless)
	{
		Progress.Count(TEXT("flawless"));
	}
	Progress.Save();
	Goto(SlopScreens::Results(R));
}

void USlopGameInstance::ApplySettings(bool bDisplay)
{
	if (Audio)
	{
		Audio->SetVolumes((float)Settings.MasterVolume, (float)Settings.MusicVolume, (float)Settings.EffectsVolume, (float)Settings.VoiceVolume);
	}
	static float AppliedTextScale = 1.f;
	if ((float)Settings.UIScale != AppliedTextScale)
	{
		AppliedTextScale = (float)Settings.UIScale;
		SlopUI::SetTextScale(AppliedTextScale);
		if (Screen.IsValid())
		{
			RebuildScreen();
		}
	}
	if (!bDisplay || !GEngine)
	{
		return;
	}
	if (UGameUserSettings* U = GEngine->GetGameUserSettings())
	{
		// A window given on the command line (development and capture runs) keeps its mode and
		// size; the frame cap, VSync and quality always follow the settings. (Uncapped and with
		// nothing presented, as offscreen, the CPU runs frames far ahead of the GPU and the
		// driver's memory balloons.)
		const bool bWindowFromCommandLine = FParse::Param(FCommandLine::Get(), TEXT("windowed")) || FParse::Param(FCommandLine::Get(), TEXT("fullscreen"));
		if (!bWindowFromCommandLine)
		{
			const EWindowMode::Type Mode = Settings.WindowMode == 0 ? EWindowMode::Windowed : (Settings.WindowMode == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Fullscreen);
			U->SetFullscreenMode(Mode);
			const FIntPoint Desktop = U->GetDesktopResolution();
			U->SetScreenResolution(Mode == EWindowMode::WindowedFullscreen && Desktop.X > 0 && Desktop.Y > 0 ? Desktop : Settings.Resolution);
		}
		U->SetVSyncEnabled(Settings.bVSync);
		const double Target = TargetFrameRate();
		U->SetFrameRateLimit((float)Target);
		// Dynamic resolution keeps the GPU inside the frame's time: the render scale drops (to
		// half at most) when the frame would run over, and TSR rebuilds the image to full size.
		const double Budget = 1000.0 / (Target > 0.0 ? Target : FMath::Max(DisplayRefresh(), 60.0));
		const auto SetCVar = [](const TCHAR* Name, float V)
		{
			if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name))
			{
				C->Set(V, ECVF_SetByGameSetting);
			}
		};
		SetCVar(TEXT("r.DynamicRes.OperationMode"), Settings.bDynamicRes ? 2.f : 0.f);
		SetCVar(TEXT("r.DynamicRes.FrameTimeBudget"), (float)Budget);
		SetCVar(TEXT("r.DynamicRes.MinScreenPercentage"), 50.f);
		SetCVar(TEXT("r.DynamicRes.MaxScreenPercentage"), 100.f);
		UE_LOG(LogSlop, Log, TEXT("Display: %.2f Hz, frame rate target %.1f, render resolution %s"), DisplayRefresh(), Target,
			Settings.bDynamicRes ? TEXT("dynamic") : TEXT("full"));
		// The engine's High level for "high": Epic costs a quarter of the render scale dynamic
		// resolution can hold at a high refresh rate, for detail that barely shows side-on.
		const int32 Q = Settings.Graphics == TEXT("low") ? 1 : (Settings.Graphics == TEXT("epic") ? 3 : 2);
		U->SetOverallScalabilityLevel(Q);
		if (bWindowFromCommandLine)
		{
			U->ApplyNonResolutionSettings();
			U->SaveSettings();
		}
		else
		{
			U->ApplySettings(false);
		}
	}
}

double USlopGameInstance::DisplayRefresh() const
{
#if PLATFORM_LINUX
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsRenderingOffScreen() && GEngine && GEngine->GameViewport)
	{
		const TSharedPtr<SWindow> W = GEngine->GameViewport->GetWindow();
		SDL_Window* H = W.IsValid() && W->GetNativeWindow().IsValid() ? static_cast<SDL_Window*>(W->GetNativeWindow()->GetOSWindowHandle()) : nullptr;
		const SDL_DisplayID D = H ? SDL_GetDisplayForWindow(H) : SDL_GetPrimaryDisplay();
		if (const SDL_DisplayMode* M = D ? SDL_GetCurrentDisplayMode(D) : nullptr)
		{
			return M->refresh_rate;
		}
	}
#endif
	return 0.0;
}

double USlopGameInstance::TargetFrameRate() const
{
	if (Settings.FrameCap >= 0)
	{
		return Settings.FrameCap;
	}
	const double R = DisplayRefresh();
	if (R <= 1.0)
	{
		return 60.0;
	}
	// A frame on every refresh up to 100 Hz; above that every second (or third) refresh, an even
	// pace the GPU can hold at a fair render scale.
	const int32 N = FMath::Max(1, FMath::CeilToInt(R / 100.0 - 0.01));
	return R / N;
}

// The engine sets the game window's mode while the window is still hidden, and some X11 window
// managers (Cinnamon's, for one) can miss that request: the window comes up as an ordinary one
// while the engine, and at times SDL too, records it as full screen, so asking the engine again
// for the same mode does nothing. For the first seconds after start, compare the window's actual
// placement with its display's, and ask the window system again (leaving full screen first when
// SDL believes it is already there).
void USlopGameInstance::KeepWindowMode()
{
#if PLATFORM_LINUX
	if (!bStarted || WindowRequests >= 3 || Now() < WindowCheckAt || Now() > StartedAt + 15.0 || !GEngine || !GEngine->GameViewport
		|| FSlateApplication::Get().IsRenderingOffScreen())
	{
		return;
	}
	WindowCheckAt = Now() + 0.5;
	const TSharedPtr<SWindow> W = GEngine->GameViewport->GetWindow();
	if (!W.IsValid() || !W->IsVisible() || !W->GetNativeWindow().IsValid() || W->GetWindowMode() == EWindowMode::Windowed)
	{
		return;
	}
	SDL_Window* H = static_cast<SDL_Window*>(W->GetNativeWindow()->GetOSWindowHandle());
	const SDL_DisplayID Display = H ? SDL_GetDisplayForWindow(H) : 0;
	SDL_Rect B;
	int X = 0, Y = 0, Wd = 0, Ht = 0;
	if (!Display || !SDL_GetDisplayBounds(Display, &B) || !SDL_GetWindowPosition(H, &X, &Y) || !SDL_GetWindowSize(H, &Wd, &Ht))
	{
		return;
	}
	if (X <= B.x && Y <= B.y && X + Wd >= B.x + B.w && Y + Ht >= B.y + B.h)
	{
		return;
	}
	++WindowRequests;
	const bool bSdlFull = (SDL_GetWindowFlags(H) & SDL_WINDOW_FULLSCREEN) != 0;
	UE_LOG(LogSlop, Log, TEXT("The game window is %dx%d at %d,%d on a %dx%d display (SDL full screen: %d); asking again for full screen"),
		Wd, Ht, X, Y, B.w, B.h, bSdlFull ? 1 : 0);
	if (bSdlFull)
	{
		SDL_SetWindowFullscreen(H, false);
		SDL_SyncWindow(H);
	}
	SDL_SetWindowFullscreen(H, true);
	SDL_SyncWindow(H);
#endif
}

void USlopGameInstance::Quit()
{
	if (Audio)
	{
		Audio->StopAll();
	}
	Settings.Save();
	UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
}

void USlopGameInstance::OnDeviceConnection(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
{
	const bool bConnected = State == EInputDeviceConnectionState::Connected;
	// Only the pad that came or went loses what it held; everyone else's inputs carry on.
	Router->OnConnectionChanged(Device, bConnected);
	Notice(bConnected ? LOCTEXT("PadOn", "Controller connected") : LOCTEXT("PadOff", "Controller disconnected"));
	if (!bConnected && Match && !Match->IsPaused() && !Match->HasEnded())
	{
		Match->SetPaused(true);
		ShowOverlay(SlopScreens::Pause());
	}
}

void USlopGameInstance::OnActivation(bool bActive)
{
	Router->ResetAll();
	if (!bActive && Match && !Match->IsPaused() && !Match->HasEnded() && Flow.Config.Mode != EMode::Watch && QuitAtTick < 0 && ShotTicks.Num() == 0 && AutoPlayLevel < 0)
	{
		Match->SetPaused(true);
		ShowOverlay(SlopScreens::Pause());
	}
}

void USlopGameInstance::DispatchUi()
{
	TArray<FUiEvent> Events = Router->TakeUi();
	// Test automation (any build; see TestCommandLine).
	if (UiScript.Num() > 0 && Now() >= UiScriptAt + UiScript[0].Delay)
	{
		FUiEvent E;
		E.Action = UiScript[0].Action;
		E.Side = UiScript[0].Side;
		E.Device = E.Side;
		Events.Add(E);
		UE_LOG(LogSlop, Log, TEXT("Ui script: %d (side %d)"), (int32)E.Action, E.Side);
		UiScriptAt = Now();
		UiScript.RemoveAt(0);
	}
	if (AutoPlayLevel >= 0 && UiScript.Num() == 0 && !(Match && !Match->HasEnded()) && !Overlay.IsValid() && Now() >= AutoConfirmAt)
	{
		FUiEvent E;
		E.Action = EUi::Confirm;
		Events.Add(E);
		AutoConfirmAt = Now() + 2.0;
	}
	for (const FUiEvent& E : Events)
	{
		if (Overlay.IsValid())
		{
			Overlay->OnUi(E);
		}
		else if (Screen.IsValid())
		{
			Screen->OnUi(E);
		}
	}
}

void USlopGameInstance::HandleDebugKeys()
{
	TArray<FKey> Keys = Router->TakeDebugKeys();
	if (!Match || Keys.Num() == 0)
	{
		return;
	}
	const EMode M = Flow.Config.Mode;
	const bool bTraining = M == EMode::Training || M == EMode::Trials || M == EMode::Tutorial;
	for (const FKey& K : Keys)
	{
		if (K == EKeys::F4 && (M == EMode::Training || M == EMode::Trials))
		{
			Match->ResetPositions();
			continue;
		}
		if (!bTraining && !bDebugKeys)
		{
			continue;
		}
		if (K == EKeys::F1) Match->bShowBoxes = !Match->bShowBoxes;
		else if (K == EKeys::F2 && Hud) Hud->bShowFrames = !Hud->bShowFrames;
		else if (K == EKeys::F3 && Hud) Hud->bShowStates = !Hud->bShowStates;
		else if (K == EKeys::F5) Match->bDebugPause = !Match->bDebugPause;
		else if (K == EKeys::F6) Match->bDebugStep = true;
		else if (K == EKeys::F7) Match->bDebugSlow = !Match->bDebugSlow;
		else if (K == EKeys::F8 && Hud) Hud->bInputDisplay = !Hud->bInputDisplay;
	}
}

bool USlopGameInstance::Tick(float Dt)
{
	if (!Router.IsValid())
	{
		return true;
	}
	if (Router->ConnectionSerial() != LastSerial)
	{
		LastSerial = Router->ConnectionSerial();
		if (Screen.IsValid())
		{
			Screen->Tick(0.f);
		}
	}
	DispatchUi();
	HandleDebugKeys();
	KeepWindowMode();
	if (FrameStats)
	{
		if (Match)
		{
			if (MatchStartAt < 0.0)
			{
				MatchStartAt = Now();
			}
			const FMatchConfig& C = Match->Config;
			FrameStats->Add(Dt, Now() - MatchStartAt, FString::Printf(TEXT("%s %s %s"), *C.Stage.ToString(), *C.P1.ToString(), *C.P2.ToString()));
		}
		else if (MatchStartAt >= 0.0)
		{
			MatchStartAt = -1.0;
			FrameStats->Flush();
		}
	}
	if (Screen.IsValid())
	{
		Screen->Tick(Dt);
	}
	if (Overlay.IsValid())
	{
		Overlay->Tick(Dt);
	}
#if !UE_BUILD_SHIPPING
	if (Tour && Match && static_cast<FMoveTour*>(Tour)->Done() && ShotTicks.Num() == 0 && QuitAtSeconds < 0.0)
	{
		QuitAtSeconds = Now() + 1.0;
	}
	if (PoseCycle.Num() > 0 && Match && Match->Views[0] && Now() >= PoseCycleAt)
	{
		const FName Pose = PoseCycle[PoseCycleIndex++ % PoseCycle.Num()];
		Match->Views[0]->Anim.PoseOverride = Pose;
		PoseCycleAt = Now() + 1.5;
		UE_LOG(LogSlop, Log, TEXT("Pose: %s"), *Pose.ToString());
	}
	for (int32 I = ExecAt.Num() - 1; I >= 0; --I)
	{
		if (Now() >= ExecAt[I].Key && GEngine)
		{
			UE_LOG(LogSlop, Log, TEXT("Exec: %s"), *ExecAt[I].Value);
			GEngine->Exec(GetWorld(), *ExecAt[I].Value);
			ExecAt.RemoveAt(I);
		}
	}
	if (Match && (ShotTicks.Num() > 0 || QuitAtTick >= 0))
	{
		const int32 T = Match->Sim.TickCount;
		if (ShotTicks.Num() > 0 && T >= ShotTicks[0].Key)
		{
			const FString Name = ShotTicks[0].Value.IsEmpty() ? FString::Printf(TEXT("%s_%05d.png"), *ShotPrefix, T) : ShotTicks[0].Value;
			FScreenshotRequest::RequestScreenshot(Name, true, false);
			UE_LOG(LogSlop, Log, TEXT("Screenshot %s"), *Name);
			ShotTicks.RemoveAt(0);
		}
		if (QuitAtTick >= 0 && T >= QuitAtTick && ShotTicks.Num() == 0)
		{
			QuitAtTick = -1;
			QuitAtSeconds = Now() + 1.0;
		}
	}
	if (PortraitQueue.Num() > 0 && Showroom)
	{
		if (PortraitWait > 0)
		{
			if (--PortraitWait == 0)
			{
				PortraitQueue.RemoveAt(0);
				if (PortraitQueue.Num() == 0)
				{
					QuitAtSeconds = Now() + 1.0;
				}
			}
		}
		else if (Showroom->Portrait(PortraitQueue[0]))
		{
			const FString Name = FString::Printf(TEXT("portrait_%s.png"), *PortraitQueue[0].ToString());
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			UE_LOG(LogSlop, Log, TEXT("Screenshot %s"), *Name);
			PortraitWait = 6;
		}
	}
	if (ShotEvery > 0.0 && Now() >= NextShotAt)
	{
		NextShotAt = Now() + ShotEvery;
		const FString Name = FString::Printf(TEXT("%s_%03d.png"), *ShotPrefix, ShotIndex++);
		FScreenshotRequest::RequestScreenshot(Name, true, false);
	}
#endif
	if (QuitAtSeconds > 0.0 && Now() >= QuitAtSeconds)
	{
		QuitAtSeconds = -1.0;
		Note(TEXT("Quit after the set time"));
		Quit();
	}
	return true;
}

void USlopGameInstance::Note(const FString& Line)
{
	UE_LOG(LogSlop, Log, TEXT("%s"), *Line);
	// Shipping builds keep no log; an unattended run's events go beside its frame record.
	if (FrameStats)
	{
		FrameStats->Note(Line);
	}
}

ASlopShowroom* USlopGameInstance::OpenShowroom()
{
	if (!Showroom)
	{
		if (UWorld* W = GetWorld())
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Showroom = W->SpawnActor<ASlopShowroom>(P);
			Showroom->Open(UGameplayStatics::GetPlayerController(W, 0));
		}
	}
	return Showroom;
}

void USlopGameInstance::CloseShowroom()
{
	if (Showroom)
	{
		Showroom->Show(0, NAME_None, false);
		Showroom->Show(1, NAME_None, false);
		Showroom->Destroy();
		Showroom = nullptr;
	}
}

void USlopGameInstance::RequestShot(int32 Tick, const FString& Name)
{
	ShotTicks.Add({Tick, Name});
	ShotTicks.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key < B.Key; });
}

void USlopGameInstance::TestCommandLine()
{
	// Unattended runs of any build, the Shipping package included: scripted menu actions and the
	// CPU playing the first side. Command line only; nothing in the game's own menus.
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("SlopAutoPlay")) || FParse::Value(Cmd, TEXT("SlopAutoPlay="), AutoPlayLevel))
	{
		FParse::Value(Cmd, TEXT("SlopAutoPlay="), AutoPlayLevel);
		AutoPlayLevel = FMath::Clamp(AutoPlayLevel < 0 ? 2 : AutoPlayLevel, 0, 2);
		AutoConfirmAt = Now() + 12.0;
	}
	double QuitSecs = -1.0;
	if (FParse::Value(Cmd, TEXT("SlopQuitAfter="), QuitSecs))
	{
		QuitAtSeconds = Now() + QuitSecs;
	}
	FString UiArg;
	if (FParse::Value(Cmd, TEXT("SlopUi="), UiArg, false))
	{
		// delay:action pairs, the delay in seconds after the previous one; actions as named below,
		// "@1" after one for the second player's side ("confirm@1").
		static const TMap<FString, EUi> Names = {
			{TEXT("up"), EUi::Up}, {TEXT("down"), EUi::Down}, {TEXT("left"), EUi::Left}, {TEXT("right"), EUi::Right},
			{TEXT("confirm"), EUi::Confirm}, {TEXT("back"), EUi::Back}, {TEXT("pause"), EUi::Pause}, {TEXT("next"), EUi::Next},
			{TEXT("prev"), EUi::Prev}, {TEXT("reset"), EUi::Reset}, {TEXT("alt"), EUi::Alt}};
		TArray<FString> Parts;
		UiArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			FString D, A, Side;
			if (P.Split(TEXT(":"), &D, &A))
			{
				int32 S = 0;
				if (A.Split(TEXT("@"), &A, &Side))
				{
					S = FMath::Clamp(FCString::Atoi(*Side), 0, 1);
				}
				if (const EUi* U = Names.Find(A))
				{
					UiScript.Add({FCString::Atod(*D), *U, S});
				}
			}
		}
		UiScriptAt = Now();
	}
}

void USlopGameInstance::DevCommandLine()
{
#if !UE_BUILD_SHIPPING
	const TCHAR* Cmd = FCommandLine::Get();
	FString MatchArg;
	if (FParse::Value(Cmd, TEXT("SlopMatch="), MatchArg, false))
	{
		// p1,p2,mode[,stage[,difficulty]]
		TArray<FString> Parts;
		MatchArg.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			FMatchConfig& C = Flow.Config;
			C = FMatchConfig();
			C.P1 = FName(*Parts[0]);
			C.P2 = FName(*Parts[1]);
			const FString Mode = Parts[2];
			C.Mode = Mode == TEXT("versus") ? EMode::Versus : Mode == TEXT("training") ? EMode::Training : Mode == TEXT("watch") ? EMode::Watch
				: Mode == TEXT("stranger") ? EMode::Stranger : Mode == TEXT("trials") ? EMode::Trials : EMode::Cpu;
			if (Parts.Num() >= 4)
			{
				C.Stage = FName(*Parts[3]);
			}
			if (Parts.Num() >= 5)
			{
				C.Difficulty = FCString::Atoi(*Parts[4]);
			}
			StartMatch();
		}
	}
	FString TourArg;
	if (FParse::Value(Cmd, TEXT("SlopTour="), TourArg))
	{
		// Every ground command of one fighter, from both sides, with boxes and a screenshot at
		// each move's first active frame (or -SlopTourDelay= frames after it).
		FMatchConfig& C = Flow.Config;
		C = FMatchConfig();
		C.Mode = EMode::Training;
		C.P1 = FName(*TourArg);
		C.P2 = C.P1 == TEXT("gorm") ? FName(TEXT("bram")) : FName(TEXT("gorm"));
		C.RoundsToWin = 1;
		StartMatch();
		if (Match)
		{
			TourFighter = C.P1;
			Match->Brains[0] = MakeUnique<FMoveTour>(Match->Sim, 0);
			Tour = Match->Brains[0].Get();
			Match->bShowBoxes = FParse::Param(Cmd, TEXT("SlopBoxes"));
			int32 Delay = 0;
			FParse::Value(Cmd, TEXT("SlopTourDelay="), Delay);
			TFunction<void(const FSimEvent&)> HudFn = Match->OnEvent;
			Match->OnEvent = [this, HudFn, Delay](const FSimEvent& E)
			{
				if (HudFn)
				{
					HudFn(E);
				}
				if (E.Type == EEvent::Move && E.Who == 0 && Match)
				{
					const FMoveDef* M = Match->Sim.Fighters[0].Def->Move(E.Move);
					const FMoveTour* T = static_cast<FMoveTour*>(Tour);
					const int32 At = M ? (M->Active > 0 ? M->Startup : (M->Projectile.bHas ? M->Projectile.Frame + 6 : M->Startup + 8)) : 1;
					RequestShot(Match->Sim.TickCount + At - 1 + Delay, FString::Printf(TEXT("tour_%s_%s_%02d_%s.png"), *TourFighter.ToString(), T->Pass == 0 ? TEXT("L") : TEXT("R"), T->Step, *E.Move.ToString()));
				}
			};
		}
	}
	// Select-screen portraits, head and shoulders, one screenshot per fighter (cropped and
	// imported by Tools/portraits.py).
	FString PortraitArg;
	if (FParse::Value(Cmd, TEXT("SlopPortraits="), PortraitArg, false) || FParse::Param(Cmd, TEXT("SlopPortraits")))
	{
		TArray<FString> Parts;
		PortraitArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& S : Parts)
		{
			PortraitQueue.Add(FName(*S));
		}
		if (PortraitQueue.Num() == 0)
		{
			PortraitQueue = FContent::Roster();
			PortraitQueue.Append(FContent::Special());
		}
		Root->SetVisibility(EVisibility::Collapsed);
		OpenShowroom();
	}
	FString Shots;
	if (FParse::Value(Cmd, TEXT("SlopShots="), Shots, false))
	{
		TArray<FString> Parts;
		Shots.ParseIntoArray(Parts, TEXT(","));
		for (const FString& S : Parts)
		{
			ShotTicks.Add({FCString::Atoi(*S), FString()});
		}
		ShotTicks.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key < B.Key; });
	}
	ShotPrefix = TEXT("shot");
	FParse::Value(Cmd, TEXT("SlopShotPrefix="), ShotPrefix);
	FParse::Value(Cmd, TEXT("SlopQuitAt="), QuitAtTick);
	if (FParse::Value(Cmd, TEXT("SlopShotEvery="), ShotEvery))
	{
		NextShotAt = Now() + ShotEvery;
	}
	FString PoseArg;
	if (FParse::Value(Cmd, TEXT("SlopPoses="), PoseArg, false))
	{
		TArray<FString> Parts;
		PoseArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			PoseCycle.Add(FName(*P));
		}
		PoseCycleAt = Now() + 3.0;
	}
	FString Exec;
	if (FParse::Value(Cmd, TEXT("SlopExec="), Exec, false))
	{
		TArray<FString> Parts;
		Exec.ParseIntoArray(Parts, TEXT(";"));
		for (const FString& P : Parts)
		{
			FString At, Command;
			if (P.Split(TEXT(":"), &At, &Command))
			{
				ExecAt.Add({Now() + FCString::Atod(*At), Command});
			}
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
