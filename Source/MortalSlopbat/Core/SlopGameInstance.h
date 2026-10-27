#pragma once

#include "Engine/GameInstance.h"
#include "Containers/Ticker.h"
#include "Core/SlopFlow.h"
#include "Core/SlopFrameStats.h"
#include "Core/SlopInputRouter.h"
#include "Core/SlopSave.h"
#include "SlopGameInstance.generated.h"

class ASlopMatch;
class USlopAudio;
class SOverlay;
class SBox;
class SWidget;
class FSlopScreen;
class SSlopHud;
namespace Slop { class IBrain; }

// The game's one long-lived object: settings, progress, the flow from screen to screen, the
// input router, audio, and the match when one is running. Screens are Slate widgets hosted in
// the game viewport; menus are driven only through the router, never by Slate focus.
UCLASS()
class USlopGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	static USlopGameInstance* Get(const UObject* Context);
	static USlopGameInstance* Get();

	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void OnStart() override;

	// Screens.
	void Goto(const TSharedPtr<FSlopScreen>& Screen);
	void ShowOverlay(const TSharedPtr<FSlopScreen>& Screen);
	void CloseOverlay();
	void RebuildScreen();
	TSharedPtr<FSlopScreen> CurrentScreen() const { return Screen; }
	TSharedPtr<FSlopScreen> CurrentOverlay() const { return Overlay; }
	// A short message over everything for a few seconds (controller connected, saved...).
	void Notice(const FText& Text, double Seconds = 3.5);
	// A line for the log (and, in unattended runs, the events file beside the frame record).
	void Note(const FString& Line);

	// Matches.
	void StartMatch();
	void EndMatch();
	ASlopMatch* GetMatch() const { return Match; }
	SSlopHud* GetHud() const { return Hud.Get(); }
	void OnMatchFinished(const Slop::FMatchResult& R);
	// The skeletal mesh a fighter wears.
	static FString MeshFor(FName Fighter);
	// The menu figures behind the select screen (spawned on first use, taking the view).
	class ASlopShowroom* OpenShowroom();
	void CloseShowroom();

	// Settings.
	void ApplySettings(bool bDisplay = true);
	// The frame rate the cap aims for (0: none beyond VSync); with the cap following the display,
	// its refresh rate, or every second or third refresh above 100 Hz.
	double TargetFrameRate() const;
	// The refresh rate of the display the game window is on (0 when unknown, as offscreen).
	double DisplayRefresh() const;
	void SaveSettings() { Settings.Save(); }
	void Quit();
	double Now() const;

	Slop::FSlopSettings Settings;
	Slop::FSlopProgress Progress;
	Slop::FGameFlow Flow;
	TSharedPtr<Slop::FSlopInputRouter> Router;
	UPROPERTY()
	TObjectPtr<USlopAudio> Audio;
	// The main menu item chosen last, so coming back lands there.
	int32 MenuChoice = 0;
	bool bDebugKeys = false;
	// Test automation: the CPU plays the first side at this level, and idle menus are confirmed,
	// so whole modes run unattended (-SlopAutoPlay[=0..2], any build). -1 when off.
	int32 AutoPlayLevel = -1;
	TOptional<Slop::FMatchResult> LastResult;

private:
	bool Tick(float Dt);
	void DispatchUi();
	void HandleDebugKeys();
	void BuildRoot();
	void OnDeviceConnection(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);
	void OnActivation(bool bActive);
	void TestCommandLine();
	void DevCommandLine();
	void KeepWindowMode();

	FTSTicker::FDelegateHandle TickHandle;
	UPROPERTY()
	TObjectPtr<ASlopMatch> Match;
	UPROPERTY()
	TObjectPtr<class ASlopShowroom> Showroom;
	TSharedPtr<SOverlay> Root;
	TSharedPtr<SBox> ScreenSlot;
	TSharedPtr<SBox> HudSlot;
	TSharedPtr<SBox> OverlaySlot;
	TSharedPtr<SBox> NoticeSlot;
	TSharedPtr<FSlopScreen> Screen;
	TSharedPtr<FSlopScreen> Overlay;
	TSharedPtr<SSlopHud> Hud;
	FText NoticeText;
	double NoticeUntil = 0.0;
	int32 LastSerial = 0;
	// Development: screenshots at given ticks (with names) and an exit tick (non-shipping builds).
	TArray<TPair<int32, FString>> ShotTicks;
	Slop::IBrain* Tour = nullptr;
	FName TourFighter;
	void RequestShot(int32 Tick, const FString& Name);
	FString ShotPrefix;
	double ShotEvery = 0.0;
	double NextShotAt = 0.0;
	int32 ShotIndex = 0;
	int32 QuitAtTick = -1;
	// Development: console commands run at set times after start (-SlopExec=40:ProfileGPU;60:stat unit).
	TArray<TPair<double, FString>> ExecAt;
	// Development: the first fighter shows these poses in turn, 1.5 s each (-SlopPoses=down,rise_1).
	TArray<FName> PoseCycle;
	double PoseCycleAt = 0.0;
	int32 PoseCycleIndex = 0;
	double QuitAtSeconds = -1.0;
	// Development: the fighters whose portraits are still to be rendered.
	TArray<FName> PortraitQueue;
	int32 PortraitWait = 0;
	// Test automation: scripted menu actions (-SlopUi=delay:action,...), dispatched as if pressed.
	struct FUiStep { double Delay; Slop::EUi Action; int32 Side; };
	TArray<FUiStep> UiScript;
	double UiScriptAt = 0.0;
	double AutoConfirmAt = 0.0;
	// The performance record (-SlopFrameStats, any build): match frames to a CSV in the save folder.
	TUniquePtr<Slop::FFrameStats> FrameStats;
	double MatchStartAt = -1.0;
	FDelegateHandle ConnHandle;
	FDelegateHandle ActiveHandle;
	bool bStarted = false;
	double StartedAt = 0.0;
	// Full screen asked of the window system again after start (KeepWindowMode).
	double WindowCheckAt = 0.0;
	int32 WindowRequests = 0;
};
