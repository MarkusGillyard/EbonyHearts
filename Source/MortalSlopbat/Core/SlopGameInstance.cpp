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
#include "UI/SlopWidgets.h"
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
	UE_LOG(LogSlop, Log, TEXT("Ebony Hearts started: %d fighters, settings %s, progress %s"), C.Fighters.Num(), *FSlopSettings::Path(), *FSlopProgress::Path());
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
	WindowWatchUntil = StartedAt + 15.0;
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
		Curtain(1.f, 0.9f);
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
	+ SOverlay::Slot()
	[
		// Collapsed when clear: a blended quad over the whole screen is not free at 3440 x 1440.
		SNew(SImage).Image(SlopUI::Box()).Visibility_Lambda([this]() { return CurtainA > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.ColorAndOpacity_Lambda([this]() { return SlopUI::Ink.CopyWithNewOpacity(CurtainA); })
	]
	+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Center).Padding(0, 30)
	[
		SAssignNew(NoticeSlot, SBox)
		.Visibility_Lambda([this]() { return Now() < NoticeUntil ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SlopUI::PanelBox(SNew(STextBlock).Font(SlopUI::Strong(26.f)).ColorAndOpacity(SlopUI::Text).Text_Lambda([this]() { return NoticeText; }), FMargin(26.f, 10.f))
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
	// Leaving by another way (the mouse, which the router does not hold back) overrides a queued fade.
	CurtainThen = nullptr;
	CurtainTarget = 0.f;
	if (!Screen->ShowsMatch())
	{
		// The hall is behind every menu; each screen says who stands in it.
		if (ASlopShowroom* Room = OpenShowroom())
		{
			Room->Clear();
		}
	}
	ScreenSlot->SetContent(Screen->Build());
	Screen->Enter();
	if (!Screen->ShowsMatch())
	{
		Curtain(0.4f, 0.2f);
	}
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

void USlopGameInstance::FadeThen(TFunction<void()> Then, float Seconds)
{
	if (CurtainThen)
	{
		return;
	}
	CurtainThen = MoveTemp(Then);
	CurtainTarget = 1.f;
	CurtainRate = 1.f / FMath::Max(Seconds, 0.01f);
}

void USlopGameInstance::Curtain(float From, float Seconds)
{
	CurtainA = FMath::Max(CurtainA, From);
	CurtainTarget = 0.f;
	CurtainRate = CurtainA / FMath::Max(Seconds, 0.01f);
}

void USlopGameInstance::Notice(const FText& Text, double Seconds)
{
	NoticeText = Text;
	NoticeUntil = Now() + Seconds;
}

bool USlopGameInstance::IsUnlocked(FName Fighter) const
{
	if (FContent::Roster().Contains(Fighter))
	{
		return true;
	}
	return FContent::Secret().Contains(Fighter) && (Progress.IsUnlocked(UnlockKey(Fighter)) || DevUnlocked.Contains(Fighter));
}

TArray<FName> USlopGameInstance::SelectOrder()
{
	TArray<FName> Out;
	for (const TArray<FName>* L : {&FContent::Roster(), &FContent::Secret()})
	{
		for (const FName& Id : *L)
		{
			if (GetContent().Fighter(Id))
			{
				Out.Add(Id);
			}
		}
	}
	return Out;
}

TArray<FName> USlopGameInstance::Playable() const
{
	TArray<FName> Out = SelectOrder();
	Out.RemoveAll([this](const FName& Id) { return !IsUnlocked(Id); });
	return Out;
}

FString USlopGameInstance::MeshFor(FName Fighter)
{
	const FString Id = Fighter.ToString();
	return FString::Printf(TEXT("/Game/Slop/Fighters/%s/%s.%s"), *Id, *Id, *Id);
}

FString USlopGameInstance::MeshFor(const FFighter& Fighter)
{
	static const FName Fragment(TEXT("slopbat")), Grandmaster(TEXT("grandmaster"));
	if (Fighter.Def->Body == Fragment && (Fighter.BaseDef->Id == Grandmaster || Fighter.Opponent().BaseDef->Id == Grandmaster))
	{
		// He sees a worm, and when he wears its shape, that is the shape he wears.
		return MeshFor(FName(TEXT("slopbat_worm")));
	}
	if (!Fighter.OutfitBody.IsNone() && Fighter.Def == Fighter.BaseDef)
	{
		return MeshFor(Fighter.OutfitBody);
	}
	return MeshFor(Fighter.Def->Body);
}

TArray<FName> USlopGameInstance::OutfitsOf(FName Fighter) const
{
	TArray<FName> Out;
	if (Fighter == TEXT("wren"))
	{
		// (Her uniform is Bram Ketch's to give: she has it once she has beaten him in the story.)
		Out = {FName(TEXT("wren")), FName(TEXT("wren_mma")), FName(TEXT("wren_muay_thai")), FName(TEXT("wren_gi"))};
		if (Progress.IsUnlocked(TEXT("outfit:wren_uniform")))
		{
			Out.Add(TEXT("wren_uniform"));
		}
	}
	else if (Fighter == TEXT("bram"))
	{
		// (The coat hides his tattoo.)
		Out = {FName(TEXT("bram")), FName(TEXT("bram_coat_off"))};
	}
	return Out;
}

void USlopGameInstance::RestartMatch()
{
	FMatchConfig& C = Flow.Config;
	if (C.Mode == EMode::Cpu || C.Mode == EMode::Training)
	{
		C.Difficulty = FMath::Clamp(Settings.Difficulty, 0, 2);
	}
	if (C.Mode == EMode::Cpu || C.Mode == EMode::Versus)
	{
		C.RoundsToWin = FMath::Clamp(Settings.VersusRounds, 1, 5);
		C.RoundSeconds = Settings.RoundSeconds;
	}
	FadeThen([this]() { StartMatch(); });
}

void USlopGameInstance::EnterFight(bool bPrelude)
{
	FadeThen([this, bPrelude]() { StartMatch(bPrelude); });
}

void USlopGameInstance::StartMatch(bool bPrelude)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	EndMatch();
	CloseOverlay();
	CloseShowroom();
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
	// Through the staging the keys stay menu keys, so a punch skips it as it always skipped the face-off.
	Router->bFightActive = !bPrelude;
	SAssignNew(Hud, SSlopHud).GI(this).Match(Match);
	if (HudSlot.IsValid())
	{
		HudSlot->SetContent(Hud.ToSharedRef());
	}
	Goto(bPrelude ? SlopScreens::FaceOff() : SlopScreens::MatchScreen());
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
	// The records count real fights, as the original did: not the tutorial or a
	// watched CPU match; a Versus win is a win for whichever player took it.
	const EMode Mode = Flow.Config.Mode;
	if (Mode != EMode::Tutorial && Mode != EMode::Watch)
	{
		Progress.Count(TEXT("matches"));
		if (R.Winner == 0 || (Mode == EMode::Versus && R.Winner >= 0))
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
	}
	Progress.Save();
	FadeThen([this, R]() { Goto(SlopScreens::Results(R)); }, 0.3f);
	if (SoakConfig)
	{
		SoakRestartAt = Now() + 3.0;
	}
}

void USlopGameInstance::ApplySettings(bool bDisplay)
{
	if (Audio)
	{
		Audio->SetVolumes((float)Settings.MasterVolume, (float)Settings.MusicVolume, (float)Settings.EffectsVolume, (float)Settings.VoiceVolume);
		Audio->SetSpeakers(Settings.Speakers);
	}
	SlopUI::SetScreenMargin((float)Settings.ScreenMargin);
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
	SlopUI::SetReducedMotion(Settings.Effects == TEXT("reduced"));
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
			const FIntPoint Chosen = ScreenSize();
			const FIntPoint Desktop = Chosen.X > 0 ? Chosen : U->GetDesktopResolution();
			U->SetScreenResolution(Mode == EWindowMode::WindowedFullscreen && Desktop.X > 0 && Desktop.Y > 0 ? Desktop : Settings.Resolution);
			// KeepWindowMode carries the window to the chosen display and holds it there.
			WindowRequests = 0;
			WindowWatchUntil = Now() + 15.0;
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
		// The floor is set by HoldRenderFloor, which follows the output's size.
		RenderFloor = 0.f;
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

#if PLATFORM_LINUX
// The display of that name, or the main one when it is empty or no longer connected.
static SDL_DisplayID ChosenDisplay(const FString& Name)
{
	SDL_DisplayID Found = SDL_GetPrimaryDisplay();
	int Count = 0;
	if (SDL_DisplayID* List = SDL_GetDisplays(&Count))
	{
		for (int I = 0; I < Count; ++I)
		{
			if (!Name.IsEmpty() && Name == UTF8_TO_TCHAR(SDL_GetDisplayName(List[I])))
			{
				Found = List[I];
			}
		}
		SDL_free(List);
	}
	return Found;
}
#endif

TArray<FString> USlopGameInstance::ScreenNames() const
{
	TArray<FString> Out;
#if PLATFORM_LINUX
	int Count = 0;
	if (SDL_DisplayID* List = SDL_GetDisplays(&Count))
	{
		const SDL_DisplayID Main = SDL_GetPrimaryDisplay();
		for (int I = 0; I < Count; ++I)
		{
			if (List[I] != Main)
			{
				Out.Add(UTF8_TO_TCHAR(SDL_GetDisplayName(List[I])));
			}
		}
		SDL_free(List);
	}
#endif
	return Out;
}

FIntPoint USlopGameInstance::ScreenSize() const
{
#if PLATFORM_LINUX
	SDL_Rect B;
	if (const SDL_DisplayID D = ChosenDisplay(Settings.Screen))
	{
		if (SDL_GetDisplayBounds(D, &B))
		{
			return FIntPoint(B.w, B.h);
		}
	}
#endif
	return FIntPoint::ZeroValue;
}

void USlopGameInstance::WarnOfSlowDisplay()
{
	// Many 4K TVs prefer 30 Hz over HDMI even when they offer 60, and it reads as a slow game.
	const double Hz = DisplayRefresh();
	if (!bWarnedSlowDisplay && Hz > 1.0 && Hz < 50.0 && Now() > WindowWatchUntil)
	{
		bWarnedSlowDisplay = true;
		Notice(FText::Format(LOCTEXT("SlowDisplay", "This display is set to {0} Hz: choose 60 Hz in your system's display settings for smooth play"), FMath::RoundToInt(Hz)), 10.0);
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
	if (!bStarted || WindowRequests >= 3 || Now() < WindowCheckAt || Now() > WindowWatchUntil || !GEngine || !GEngine->GameViewport
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
	SDL_DisplayID Display = H ? SDL_GetDisplayForWindow(H) : 0;
	SDL_Rect B;
	// The engine opens on the main display and goes full screen wherever the window stands, so
	// another display is reached by taking the window there first.
	const SDL_DisplayID Want = ChosenDisplay(Settings.Screen);
	if (H && Display && Want && Want != Display && SDL_GetDisplayBounds(Want, &B))
	{
		++WindowRequests;
		UE_LOG(LogSlop, Log, TEXT("Moving the game to %hs (%dx%d at %d,%d)"), SDL_GetDisplayName(Want), B.w, B.h, B.x, B.y);
		SDL_SetWindowFullscreen(H, false);
		SDL_SyncWindow(H);
		SDL_SetWindowPosition(H, B.x + 32, B.y + 32);
		SDL_SyncWindow(H);
		SDL_SetWindowFullscreen(H, true);
		SDL_SyncWindow(H);
		Display = Want;
	}
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

void USlopGameInstance::HoldRenderFloor()
{
	FVector2D Size;
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	GEngine->GameViewport->GetViewportSize(Size);
	// Upscaling keeps the picture at 4K from less than half its height, and the output's own cost
	// there leaves little for the scene: at the usual floor a mid-range card just misses 60.
	const float Floor = Size.Y >= 2000.0 ? 45.f : 50.f;
	if (Floor != RenderFloor)
	{
		RenderFloor = Floor;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.MinScreenPercentage")))
		{
			V->Set(Floor, ECVF_SetByCode);
		}
	}
}

void USlopGameInstance::OnDeviceConnection(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
{
	const bool bConnected = State == EInputDeviceConnectionState::Connected;
	if (Slop::FSlopInputRouter::IgnoresPads())
	{
		return;
	}
	// Only the pad that came or went loses what it held; everyone else's inputs carry on.
	const Slop::EGlyphs Shown = Router->Glyphs();
	Router->OnConnectionChanged(Device, bConnected);
	// Prompts follow by themselves; a page that wrote the buttons into its text needs writing again.
	if (Router->Glyphs() != Shown)
	{
		RebuildScreen();
	}
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
		if (CurtainThen)
		{
			break;
		}
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
	if (Audio)
	{
		Audio->Tick(Dt, !Match || Match->HasEnded());
	}
	HoldRenderFloor();
	WarnOfSlowDisplay();
	if (CurtainHold > 0)
	{
		--CurtainHold;
	}
	else
	{
		CurtainA = FMath::FInterpConstantTo(CurtainA, CurtainTarget, Dt, CurtainRate);
		if (CurtainThen && CurtainA >= 1.f)
		{
			// Under full ink: whatever this loads is never seen arriving.
			TFunction<void()> Then = MoveTemp(CurtainThen);
			CurtainThen = nullptr;
			Then();
			Dt = 0.f;
			CurtainHold = 3;
			Curtain(1.f, 0.42f);
		}
	}
	// A screen being left no longer runs: its timers (the continue countdown) must not act behind a
	// choice already made.
	if (!CurtainThen)
	{
		if (Screen.IsValid())
		{
			Screen->Tick(Dt);
		}
		if (Overlay.IsValid())
		{
			Overlay->Tick(Dt);
		}
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
	if (MoveCycle.Num() > 0 && Match && Now() >= MoveCycleAt)
	{
		Slop::FFighter& A = Match->Sim.Fighters[0];
		Slop::FFighter& B = Match->Sim.Fighters[1];
		if (A.IsActionable() && B.IsActionable())
		{
			if (bMoveCycleFlip && A.Facing > 0)
			{
				B.Pos.X = A.Pos.X - 0.8;
				MoveCycleAt = Now() + 0.5;
			}
			else
			{
				B.Pos.X = A.Pos.X + A.Facing * 0.8;
				if (const Slop::FMoveDef* M = A.Def->Move(MoveCycle[MoveCycleIndex++ % MoveCycle.Num()]))
				{
					A.StartMove(*M);
				}
				MoveCycleAt = Now() + 3.0;
			}
		}
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
			// -SlopShotsNoUi: the picture alone (stage pictures for the stage screen).
			static const bool bNoUi = FParse::Param(FCommandLine::Get(), TEXT("SlopShotsNoUi"));
			FScreenshotRequest::RequestScreenshot(Name, !bNoUi, false);
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
	if (SoakConfig && SoakRestartAt >= 0.0 && Now() >= SoakRestartAt)
	{
		SoakRestartAt = -1.0;
		Flow.Config = *SoakConfig;
		StartMatch();
	}
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
	FString SoakArg;
	if (FParse::Value(Cmd, TEXT("SlopSoakMatch="), SoakArg, false))
	{
		TArray<FString> Parts;
		SoakArg.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 2 && GetContent().Fighter(FName(*Parts[0])) && GetContent().Fighter(FName(*Parts[1])))
		{
			FMatchConfig C;
			C.P1 = FName(*Parts[0]);
			C.P2 = FName(*Parts[1]);
			C.Mode = EMode::Watch;
			C.Stage = Parts.Num() >= 3 ? FName(*Parts[2]) : FName(TEXT("rendering_yards"));
			C.Difficulty = 2;
			SoakConfig = C;
			Flow.Config = C;
			Note(FString::Printf(TEXT("Soak: %s v %s at %s, again and again"), *C.P1.ToString(), *C.P2.ToString(), *C.Stage.ToString()));
			StartMatch();
		}
		else
		{
			UE_LOG(LogSlop, Error, TEXT("-SlopSoakMatch=%s: expected two fighter ids and a stage"), *SoakArg);
		}
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
	// Development: -SlopSecret=<fighter> starts at the Grandmaster's challenge, as if <fighter> had
	// just beaten the Fragment without a continue (the face-off, the fight, the results and, won,
	// the unlock, saved to the profile in use).
	FString SecretArg;
	if (FParse::Value(Cmd, TEXT("SlopSecret="), SecretArg) && GetContent().Fighter(FName(*SecretArg)))
	{
		Flow.SkipToBossCleared(FName(*SecretArg), 1);
		if (Flow.SecretEarned())
		{
			Flow.AddSecretRung();
			StartMatch(true);
		}
	}
	// Development: -SlopResults=p1,p2,mode,winner[,flawless][,finisher move] shows that result screen
	// with no fight before it (versus, cpu, survival, tutorial; arcade only as a loss, since a win
	// needs a running ladder).
	FString ResultsArg;
	if (FParse::Value(Cmd, TEXT("SlopResults="), ResultsArg, false))
	{
		TArray<FString> Parts;
		ResultsArg.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 4)
		{
			FMatchResult R;
			R.P1 = FName(*Parts[0]);
			R.P2 = FName(*Parts[1]);
			const FString Mode = Parts[2];
			R.Mode = Mode == TEXT("versus") ? EMode::Versus : Mode == TEXT("survival") ? EMode::Survival : Mode == TEXT("tutorial") ? EMode::Tutorial
				: Mode == TEXT("arcade") ? EMode::Arcade : EMode::Cpu;
			R.Winner = FCString::Atoi(*Parts[3]);
			R.bFlawless = Parts.Contains(TEXT("flawless"));
			if (Parts.Num() >= 5 && Parts.Last() != TEXT("flawless"))
			{
				R.Finisher = FName(*Parts.Last());
			}
			Flow.Config.P1 = R.P1;
			Flow.Config.P2 = R.P2;
			Flow.Config.Mode = R.Mode;
			if (R.Mode == EMode::Survival)
			{
				Flow.StartSurvival(R.P1, 7);
			}
			Goto(SlopScreens::Results(R));
		}
	}
	FString UnlockArg;
	if (FParse::Value(Cmd, TEXT("SlopUnlock="), UnlockArg, false))
	{
		TArray<FString> Parts;
		UnlockArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			DevUnlocked.Add(FName(*P));
		}
	}
	FString MatchArg;
	if (FParse::Value(Cmd, TEXT("SlopMatch="), MatchArg, false))
	{
		// p1,p2,mode[,stage[,difficulty[,p1 outfit]]]
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
				: Mode == TEXT("trials") ? EMode::Trials : EMode::Cpu;
			if (Parts.Num() >= 4)
			{
				C.Stage = FName(*Parts[3]);
			}
			if (Parts.Num() >= 5)
			{
				C.Difficulty = FCString::Atoi(*Parts[4]);
			}
			if (Parts.Num() >= 6)
			{
				C.Outfit[0] = FName(*Parts[5]);
			}
			StartMatch();
		}
	}
	// Development: -SlopStoryFrom=<rung> starts Story mode at that fight, to reach its end quickly.
	int32 StoryFrom = -1;
	if (FParse::Value(Cmd, TEXT("SlopStoryFrom="), StoryFrom))
	{
		Flow.StartStory(1);
		for (int32 I = 0; I < StoryFrom; ++I)
		{
			Flow.ArcadeAdvance(false, Now());
		}
		StartMatch(true);
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
			TUniquePtr<FMoveTour> T = MakeUnique<FMoveTour>(Match->Sim, 0);
			FParse::Value(Cmd, TEXT("SlopTourGap="), T->Gap);
			Match->Brains[0] = MoveTemp(T);
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
	// A script for the first side's fighter against a standing opponent (see FScriptBrain), with
	// -SlopMatch: "-SlopScript=D+RN+LP;w40;shot:nix".
	FString ScriptArg;
	if (Match && FParse::Value(Cmd, TEXT("SlopScript="), ScriptArg, false))
	{
		Match->Brains[0] = MakeUnique<FScriptBrain>(Match->Sim, 0, ScriptArg, [this](const FString& Name)
		{
			if (Match)
			{
				RequestShot(Match->Sim.TickCount + 1, Name + TEXT(".png"));
			}
		});
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
			PortraitQueue.Append(FContent::Secret());
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
	// Development: -SlopMoveCycle=a,b plays those moves of P1's in turn from close in, to look at throws in motion.
	FString MoveArg;
	if (FParse::Value(Cmd, TEXT("SlopMoveCycle="), MoveArg, false))
	{
		TArray<FString> Parts;
		MoveArg.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts)
		{
			MoveCycle.Add(FName(*P));
		}
		MoveCycleAt = Now() + 4.0;
		bMoveCycleFlip = FParse::Param(Cmd, TEXT("SlopMoveCycleFlip"));
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
	// Development: -SlopEnding=<fighter> opens that fighter's arcade ending (from Extras, so it
	// returns there), to look at endings without clearing a tower.
	FString EndingArg;
	if (FParse::Value(Cmd, TEXT("SlopEnding="), EndingArg) && GetContent().Fighter(FName(*EndingArg)))
	{
		Goto(SlopScreens::Ending(FName(*EndingArg), false));
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
