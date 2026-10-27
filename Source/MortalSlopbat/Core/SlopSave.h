#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Dom/JsonObject.h"

namespace Slop
{
	// The ten things a fighter can be told to do, in the order the settings screen lists them.
	enum class EGameAction : uint8 { Up, Down, Left, Right, HP, LP, HK, LK, BL, RN, Count };
	const TCHAR* ActionId(EGameAction A);
	FText ActionLabel(EGameAction A);

	// Input devices the game distinguishes: the two halves of the keyboard (laid out like the
	// arcade panel), and gamepads in the order they were first seen.
	enum class EDevice : int32 { KeyboardLeft = 0, KeyboardRight = 1, FirstPad = 2 };
	constexpr int32 MaxPads = 4;
	constexpr int32 NumDevices = 2 + MaxPads;

	// Where this game keeps its files: ~/.local/share/MortalSlopbat/UE5 (beside, never inside, the
	// original game's files). --profile=name keeps test runs apart.
	FString SaveDir();
	FString ProfileSuffix();
	// Writes text to a temporary file and renames it over the target, so a crash mid-write never
	// leaves half a file.
	bool WriteFileAtomic(const FString& Path, const FString& Text);

	struct FSlopSettings
	{
		static constexpr int32 Version = 2;
		// Set by Load when the file had to be set aside; shown to the player once.
		FString LoadNote;

		// Display. Window mode: 0 window, 1 borderless full screen, 2 exclusive full screen.
		int32 WindowMode = 1;
		FIntPoint Resolution = FIntPoint(1920, 1080);
		bool bVSync = true;
		// -1 follows the display (its refresh rate, or every second or third refresh above 100 Hz);
		// 0 means no cap beyond VSync.
		int32 FrameCap = -1;
		// The render resolution drops (to half at most) as far as needed to hold the frame rate, the
		// image rebuilt to full size; or it always renders at full size.
		bool bDynamicRes = true;
		// "high", "epic" (the engine's highest scalability level) or "low".
		FString Graphics = TEXT("high");
		double UIScale = 1.0;
		// Audio (0..1).
		double MasterVolume = 0.8;
		double MusicVolume = 0.5;
		double EffectsVolume = 0.9;
		double VoiceVolume = 0.9;
		// Comfort.
		bool bScreenShake = true;
		bool bFlashes = true;
		// "full" or "reduced".
		FString Effects = TEXT("full");
		// "full", "low" or "off".
		FString Gore = TEXT("full");
		bool bPostFx = true;
		bool bMotionSmoothing = true;
		bool bInputDisplay = false;
		int32 Difficulty = 1;
		// Controllers.
		double DeadZone = 0.3;
		bool bVibration = true;
		// "auto", "playstation", "xbox".
		FString Glyphs = TEXT("auto");
		// Bindings: one key per action for each keyboard half; one pad button per action shared by
		// every gamepad (directions also always work on the D-pad and the left stick).
		FKey Keys[2][(int32)EGameAction::Count];
		FKey PadButtons[(int32)EGameAction::Count];
		// Which side each device controls in two-player modes (-1 nobody).
		int32 Assign[NumDevices] = {0, 1, 0, 1, -1, -1};

		FSlopSettings();
		void ResetBindings();
		void ResetAll();
		// Rebinds a keyboard action. A key already used by another action, in either half, swaps
		// places, so no key drives two actions and no action is left unbound.
		void RebindKey(int32 Half, EGameAction Action, const FKey& Key);
		// Rebinds a pad action, swapping with any action already on that button. Returns false for
		// buttons that are reserved (Start pauses).
		bool RebindPad(EGameAction Action, const FKey& Button);
		static bool IsReservedPadButton(const FKey& K);
		static bool IsReservedKey(const FKey& K);
		static bool DirBitOf(EGameAction A) { return A == EGameAction::Up || A == EGameAction::Down || A == EGameAction::Left || A == EGameAction::Right; }

		TSharedRef<FJsonObject> ToJson() const;
		// Reads key by key: anything missing or invalid keeps its default.
		void FromJson(const TSharedPtr<FJsonObject>& J);
		bool Load();
		bool Save() const;
		static FString Path();
	};

	// Arcade records, unlocks, endings seen, trials done and the stranger's memory.
	class FSlopProgress
	{
	public:
		static constexpr int32 Version = 1;
		FSlopProgress() { Reset(); }

		void Reset();
		// Loads the file; an unreadable one is moved aside to .bad and a fresh profile is used.
		// The first time, a progress file of the original game is read (never written) and merged.
		void Load();
		bool Save() const;
		static FString Path();

		void RecordArcade(FName Fighter, int32 Difficulty, double Seconds, int32 Score);
		// Returns true for a new best.
		bool RecordSurvival(FName Fighter, int32 Waves);
		// Returns true the first time.
		bool TrialDone(FName Fighter, const FString& TrialName);
		TArray<FString> TrialsDone(FName Fighter) const;
		int32 BestSurvival(FName Fighter) const;
		int32 BestScore(FName Fighter, int32 Difficulty) const;
		double BestTime(FName Fighter, int32 Difficulty) const;
		int32 Clears(FName Fighter, int32 Difficulty) const;
		bool Cleared(FName Fighter, int32 Difficulty = -1) const;
		bool AnyCleared() const;
		void Unlock(const FString& What);
		bool IsUnlocked(const FString& What) const;
		bool EndingSeen(FName Fighter) const;
		void Count(const FString& Stat, int32 N = 1);
		int32 Stat(const FString& Stat) const;
		// The stranger's memory only ever moves forward.
		void SetStranger(int32 Stage);
		int32 Stranger() const;
		bool TutorialDone() const;
		void SetTutorialDone();

		TSharedPtr<FJsonObject> Data;
		FString LoadNote;

	private:
		void MergeOriginal(const TSharedPtr<FJsonObject>& Old);
	};
}
