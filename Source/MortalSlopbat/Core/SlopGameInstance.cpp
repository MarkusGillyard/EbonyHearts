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
	if (!Progress.LoadNote.IsEmpty())
	{
		UE_LOG(LogSlop, Log, TEXT("%s"), *Progress.LoadNote);
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
	BuildRoot();
	ApplySettings(true);
	DevCommandLine();
	if (!Screen.IsValid())
	{
		Goto(GetContent().Errors.Num() > 0 ? SlopScreens::DataError() : SlopScreens::Title());
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
	UE_LOG(LogSlop, Log, TEXT("Screen: %s"), *Screen->ScreenName);
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
		UE_LOG(LogSlop, Log, TEXT("Overlay: %s"), *Overlay->ScreenName);
		OverlaySlot->SetContent(Overlay->Build());
		Overlay->Enter();
	}
	else
	{
		OverlaySlot->SetContent(SNullWidget::NullWidget);
	}
}

void USlopGameInstance::CloseOverlay() { ShowOverlay(nullptr); }

void USlopGameInstance::Notice(const FText& Text)
{
	NoticeText = Text;
	NoticeUntil = Now() + 3.5;
}

FString USlopGameInstance::MeshFor(FName Fighter)
{
	const FString Id = Fighter.ToString();
	const FString Pkg = FString::Printf(TEXT("/Game/Slop/Fighters/%s/%s"), *Id, *Id);
	if (FPackageName::DoesPackageExist(Pkg))
	{
		return Pkg + TEXT(".") + Id;
	}
	// Until every fighter is built, the development bodies stand in.
	const FFighterDef* D = GetContent().Fighter(Fighter);
	const bool bHer = D && D->Pronoun == TEXT("her");
	return bHer ? TEXT("/Game/Slop/Fighters/body_woman_athletic_174/woman_athletic_174.woman_athletic_174")
		: TEXT("/Game/Slop/Fighters/body_man_heavy_212/man_heavy_212.man_heavy_212");
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
	if (!bDisplay || !GEngine)
	{
		return;
	}
	// A window size given on the command line (development and capture runs) wins.
	if (FParse::Param(FCommandLine::Get(), TEXT("windowed")) || FParse::Param(FCommandLine::Get(), TEXT("fullscreen")))
	{
		return;
	}
	if (UGameUserSettings* U = GEngine->GetGameUserSettings())
	{
		const EWindowMode::Type Mode = Settings.WindowMode == 0 ? EWindowMode::Windowed : (Settings.WindowMode == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Fullscreen);
		U->SetFullscreenMode(Mode);
		if (Mode == EWindowMode::WindowedFullscreen)
		{
			U->SetScreenResolution(U->GetDesktopResolution());
		}
		else
		{
			U->SetScreenResolution(Settings.Resolution);
		}
		U->SetVSyncEnabled(Settings.bVSync);
		U->SetFrameRateLimit((float)Settings.FrameCap);
		const int32 Q = Settings.Graphics == TEXT("low") ? 1 : 3;
		U->SetOverallScalabilityLevel(Q);
		U->ApplySettings(false);
	}
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
	Router->OnConnectionChanged(Device, bConnected);
	Router->ResetAll();
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
	if (!bActive && Match && !Match->IsPaused() && !Match->HasEnded() && Flow.Config.Mode != EMode::Watch && QuitAtTick < 0 && ShotTicks.Num() == 0)
	{
		Match->SetPaused(true);
		ShowOverlay(SlopScreens::Pause());
	}
}

void USlopGameInstance::DispatchUi()
{
	TArray<FUiEvent> Events = Router->TakeUi();
#if !UE_BUILD_SHIPPING
	if (UiScript.Num() > 0 && Now() >= UiScriptAt + UiScript[0].Key)
	{
		FUiEvent E;
		E.Action = UiScript[0].Value;
		Events.Add(E);
		UE_LOG(LogSlop, Log, TEXT("Ui script: %d"), (int32)E.Action);
		UiScriptAt = Now();
		UiScript.RemoveAt(0);
	}
#endif
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
	if (QuitAtSeconds > 0.0 && Now() >= QuitAtSeconds)
	{
		QuitAtSeconds = -1.0;
		Quit();
	}
#endif
	return true;
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
		// each move's first active frame.
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
			TFunction<void(const FSimEvent&)> HudFn = Match->OnEvent;
			Match->OnEvent = [this, HudFn](const FSimEvent& E)
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
					RequestShot(Match->Sim.TickCount + At - 1, FString::Printf(TEXT("tour_%s_%s_%02d_%s.png"), *TourFighter.ToString(), T->Pass == 0 ? TEXT("L") : TEXT("R"), T->Step, *E.Move.ToString()));
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
	FString UiArg;
	if (FParse::Value(Cmd, TEXT("SlopUi="), UiArg, false))
	{
		// delay:action pairs, the delay in seconds after the previous one; actions as named below.
		static const TMap<FString, EUi> Names = {
			{TEXT("up"), EUi::Up}, {TEXT("down"), EUi::Down}, {TEXT("left"), EUi::Left}, {TEXT("right"), EUi::Right},
			{TEXT("confirm"), EUi::Confirm}, {TEXT("back"), EUi::Back}, {TEXT("pause"), EUi::Pause}, {TEXT("next"), EUi::Next},
			{TEXT("prev"), EUi::Prev}, {TEXT("reset"), EUi::Reset}, {TEXT("alt"), EUi::Alt}};
		TArray<FString> Parts;
		UiArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			FString D, A;
			if (P.Split(TEXT(":"), &D, &A) && Names.Contains(A))
			{
				UiScript.Add({FCString::Atod(*D), Names[A]});
			}
		}
		UiScriptAt = Now();
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
	double QuitSecs = -1.0;
	if (FParse::Value(Cmd, TEXT("SlopQuitAfter="), QuitSecs))
	{
		QuitAtSeconds = Now() + QuitSecs;
	}
#endif
}

#undef LOCTEXT_NAMESPACE
