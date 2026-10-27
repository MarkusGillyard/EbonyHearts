#include "Misc/AutomationTest.h"
#include "HAL/PlatformFileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SlopFlow.h"
#include "Core/SlopSave.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/SlopSimHarness.h"
#include "View/SlopMatch.h"

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace SlopGameTestSuite
{
using namespace Slop;
using namespace Slop::Test;

// --- The fixed clock --------------------------------------------------------------------------------

// Drives a simulation from a scripted list of timed presses (seconds) with rendered frames of the
// given lengths, the way the match does: input that arrives during a frame goes to the first tick
// run after it. Returns the tick on which each scripted move started.
static TArray<int32> RunScripted(const TArray<double>& FrameLengths, int32 Frames)
{
	FSimHarness H;
	H.Place(1.0);
	FFixedClock Clock;
	struct FPress { double At; int32 Dirs; int32 Btn; };
	const TArray<FPress> Script = {{0.10, 0, BtnHP}, {0.62, 0, BtnLK}, {1.30, PadDown, BtnHP}, {2.10, 0, BtnHK}};
	int32 Next = 0;
	double Time = 0;
	FPadState Pending;
	TArray<int32> Starts;
	for (int32 F = 0; F < Frames; ++F)
	{
		const double Dt = FrameLengths[F % FrameLengths.Num()];
		Time += Dt;
		while (Next < Script.Num() && Script[Next].At <= Time)
		{
			Pending.Pressed |= Script[Next].Btn;
			Pending.Held |= Script[Next].Btn;
			Pending.Dirs = Script[Next].Dirs;
			++Next;
		}
		const int32 N = Clock.Advance(Dt);
		for (int32 I = 0; I < N; ++I)
		{
			FPadState Pads[2];
			Pads[0] = Pending;
			Pending.Pressed = 0;
			Pending.Held = 0;
			H.Sim.Step(Pads);
			for (const FSimEvent& E : H.Sim.Events)
			{
				if (E.Type == EEvent::Move && E.Who == 0)
				{
					Starts.Add(E.Tick);
				}
			}
			Pending.Dirs = 0;
		}
	}
	return Starts;
}

SLOP_TEST(FSlopClockRenderRates, "Timing.SameInputsAtAnyFrameRate")
bool FSlopClockRenderRates::RunTest(const FString&)
{
	// The same timed presses at 30, 60, 144 and an uneven 50-90 fps: every move starts, and on the
	// same tick to within the one-frame delay a slower frame adds before a press is read.
	const TArray<int32> At60 = RunScripted({1.0 / 60.0}, 60 * 4);
	const TArray<int32> At144 = RunScripted({1.0 / 144.0}, 144 * 4);
	const TArray<int32> At30 = RunScripted({1.0 / 30.0}, 30 * 4);
	const TArray<int32> Uneven = RunScripted({1.0 / 50.0, 1.0 / 90.0, 1.0 / 70.0, 1.0 / 61.0}, 280);
	TestEqual("four moves at 60 fps", At60.Num(), 4);
	for (const TArray<int32>* R : {&At144, &At30, &Uneven})
	{
		TestEqual("four moves at every rate", R->Num(), 4);
		for (int32 I = 0; I < FMath::Min(R->Num(), At60.Num()); ++I)
		{
			TestTrue(FString::Printf(TEXT("move %d starts within a frame of 60 fps (%d vs %d)"), I, (*R)[I], At60[I]), FMath::Abs((*R)[I] - At60[I]) <= 2);
		}
	}
	// Exactly the same ticks when the frames line up with ticks, however they are grouped.
	const TArray<int32> Doubled = RunScripted({1.0 / 30.0, 0.0}, 30 * 4 * 2);
	TestEqual("grouping ticks into frames changes nothing", Doubled, At30);
	return true;
}

SLOP_TEST(FSlopClockCatchUp, "Timing.ClockCatchUpAndOverload")
bool FSlopClockCatchUp::RunTest(const FString&)
{
	FFixedClock C;
	TestEqual("one 60 Hz frame is one tick", C.Advance(1.0 / 60.0), 1);
	TestEqual("a 144 Hz frame is usually none", C.Advance(1.0 / 144.0), 0);
	C.Reset();
	int32 Total = 0;
	for (int32 I = 0; I < 144; ++I)
	{
		Total += C.Advance(1.0 / 144.0);
	}
	TestTrue("a second of 144 Hz frames is 60 ticks", Total >= 59 && Total <= 60);
	C.Reset();
	TestEqual("a long hitch runs at most four ticks", C.Advance(0.5), FFixedClock::MaxSteps);
	TestTrue("and drops the rest instead of spiralling", C.Accum < DT);
	TestTrue("render fraction stays in range", C.Frac() >= 0.0 && C.Frac() <= 1.0);
	return true;
}

SLOP_TEST(FSlopHitstopKeepsPresses, "Timing.PressDuringHitstopIsKept")
bool FSlopHitstopKeepsPresses::RunTest(const FString&)
{
	// A chain press made while frozen in hit-stop comes out when the freeze ends: frozen ticks
	// do not age the buffer.
	FSimHarness H;
	H.Place(0.85);
	H.Step(0, HP);
	H.Until([&] { return H.F(0).Hitstop > 0; }, 20);
	TestTrue("in hit-stop", H.F(0).Hitstop > 0);
	const int32 Frozen = H.F(0).Hitstop;
	TestTrue("hit-stop longer than the buffer would allow otherwise", Frozen + 2 > FInputBuffer::BufferTicks);
	const int32 At = H.Log.Num();
	H.Step(0, HP);
	H.Idle(40);
	TestTrue("the press is not lost: HP, HP chains", H.Count(EEvent::Chain, At) == 1);
	return true;
}

// --- Saves ----------------------------------------------------------------------------------------------

SLOP_TEST(FSlopSettingsRoundTrip, "Saves.SettingsRoundTripAndBadValues")
bool FSlopSettingsRoundTrip::RunTest(const FString&)
{
	FSlopSettings S;
	S.MasterVolume = 0.3;
	S.WindowMode = 0;
	S.Resolution = FIntPoint(1600, 900);
	S.Gore = TEXT("off");
	S.DeadZone = 0.2;
	S.RebindKey(0, EGameAction::HP, EKeys::Y);
	S.RebindPad(EGameAction::BL, EKeys::Gamepad_LeftThumbstick);
	S.Assign[2] = 1;
	FSlopSettings R;
	R.FromJson(S.ToJson());
	TestEqual("volume", R.MasterVolume, 0.3);
	TestEqual("window", R.WindowMode, 0);
	TestEqual("resolution", R.Resolution, FIntPoint(1600, 900));
	TestEqual("gore", R.Gore, FString(TEXT("off")));
	TestEqual("key", R.Keys[0][(int32)EGameAction::HP], EKeys::Y);
	TestEqual("pad", R.PadButtons[(int32)EGameAction::BL], EKeys::Gamepad_LeftThumbstick);
	TestEqual("assign", R.Assign[2], 1);
	// Nonsense values keep their defaults.
	TSharedPtr<FJsonObject> Bad = FContent::ParseJson(TEXT(R"({"master_volume": 7, "window_mode": 9, "res_x": 5, "gore": "lots", "frame_cap": 33, "glyphs": 4, "keys_left": {"hp": "NotAKey"}})"));
	FSlopSettings B;
	B.FromJson(Bad);
	const FSlopSettings D;
	TestEqual("volume clamped", B.MasterVolume, 1.0);
	TestEqual("window clamped", B.WindowMode, 2);
	TestEqual("resolution default", B.Resolution, D.Resolution);
	TestEqual("gore default", B.Gore, D.Gore);
	TestEqual("cap default", B.FrameCap, D.FrameCap);
	// The old default cap in a version 1 file follows the display now; chosen in version 2, it stays.
	FSlopSettings V1, V2;
	V1.FromJson(FContent::ParseJson(TEXT(R"({"version": 1, "frame_cap": 60})")));
	V2.FromJson(FContent::ParseJson(TEXT(R"({"version": 2, "frame_cap": 60})")));
	TestEqual("version 1's 60 follows the display", V1.FrameCap, -1);
	TestEqual("a chosen 60 stays", V2.FrameCap, 60);
	TestEqual("bad key ignored", B.Keys[0][(int32)EGameAction::HP], D.Keys[0][(int32)EGameAction::HP]);
	return true;
}

SLOP_TEST(FSlopRebindSwaps, "Saves.RebindingNeverBindsTwice")
bool FSlopRebindSwaps::RunTest(const FString&)
{
	FSlopSettings S;
	const FKey OldHP = S.Keys[0][(int32)EGameAction::HP];
	const FKey LKey = S.Keys[1][(int32)EGameAction::LK];
	// Giving the left half's high punch the right half's low kick swaps them.
	S.RebindKey(0, EGameAction::HP, LKey);
	TestEqual("bound", S.Keys[0][(int32)EGameAction::HP], LKey);
	TestEqual("the other side took the old key", S.Keys[1][(int32)EGameAction::LK], OldHP);
	TSet<FKey> Seen;
	for (int32 H = 0; H < 2; ++H)
	{
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			bool bDup = false;
			Seen.Add(S.Keys[H][A], &bDup);
			TestFalse("no key on two actions", bDup);
		}
	}
	TestFalse("Start cannot be bound", S.RebindPad(EGameAction::HP, EKeys::Gamepad_Special_Right));
	S.ResetBindings();
	TestEqual("reset", S.Keys[0][(int32)EGameAction::HP], OldHP);
	return true;
}

SLOP_TEST(FSlopReservedInputs, "Saves.ReservedInputsAreRefused")
bool FSlopReservedInputs::RunTest(const FString&)
{
	FSlopSettings S;
	const FKey Old = S.PadButtons[(int32)EGameAction::HP];
	// Directions, the sticks' directions, the right stick's click (next) and the menu buttons
	// never become fighting buttons, and pad directions are not rebindable at all.
	TestFalse("D-pad refused", S.RebindPad(EGameAction::HP, EKeys::Gamepad_DPad_Up));
	TestFalse("stick direction refused", S.RebindPad(EGameAction::HP, EKeys::Gamepad_LeftStick_Up));
	TestFalse("right stick click refused", S.RebindPad(EGameAction::HP, EKeys::Gamepad_RightThumbstick));
	TestFalse("Select refused", S.RebindPad(EGameAction::HP, EKeys::Gamepad_Special_Left));
	TestFalse("directions stay the D-pad's", S.RebindPad(EGameAction::Up, EKeys::Gamepad_FaceButton_Top));
	TestEqual("unchanged", S.PadButtons[(int32)EGameAction::HP], Old);
	TestTrue("left stick click allowed", S.RebindPad(EGameAction::HP, EKeys::Gamepad_LeftThumbstick));
	for (const FKey& K : {EKeys::Tab, EKeys::SpaceBar, EKeys::Escape, EKeys::Enter, EKeys::F1, EKeys::F4, EKeys::F8})
	{
		TestTrue(*FString::Printf(TEXT("%s reserved"), *K.ToString()), FSlopSettings::IsReservedKey(K));
	}
	TestFalse("letters are free", FSlopSettings::IsReservedKey(EKeys::Q));
	// A saved file naming a reserved key or a direction button keeps the default.
	TSharedPtr<FJsonObject> J = FContent::ParseJson(TEXT(R"({"version": 1, "keys_left": {"hp": "Tab"}, "pad": {"hp": "Gamepad_DPad_Up", "up": "Gamepad_FaceButton_Top"}})"));
	FSlopSettings R;
	R.FromJson(J);
	const FSlopSettings D;
	TestEqual("reserved key ignored", R.Keys[0][(int32)EGameAction::HP], D.Keys[0][(int32)EGameAction::HP]);
	TestEqual("direction button ignored", R.PadButtons[(int32)EGameAction::HP], D.PadButtons[(int32)EGameAction::HP]);
	TestEqual("pad up stays the D-pad", R.PadButtons[(int32)EGameAction::Up], EKeys::Gamepad_DPad_Up);
	return true;
}

SLOP_TEST(FSlopSaveFilesRecover, "Saves.InterruptedAndDamagedFiles")
bool FSlopSaveFilesRecover::RunTest(const FString&)
{
	const FString Saved = FCommandLine::Get();
	FCommandLine::Set(*(Saved + TEXT(" -profile=autotest")));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	const FString SPath = FSlopSettings::Path();
	for (const FString& F : {SPath, SPath + TEXT(".tmp"), SPath + TEXT(".bad")})
	{
		PF.DeleteFile(*F);
	}
	// A write that stopped after the new file was flushed but before it replaced the old one.
	FSlopSettings S;
	S.MasterVolume = 0.25;
	TestTrue("saved", S.Save());
	PF.MoveFile(*(SPath + TEXT(".tmp")), *SPath);
	{
		FSlopSettings L;
		TestTrue("recovered from the flushed temporary file", L.Load());
		TestEqual("its contents", L.MasterVolume, 0.25);
	}
	// A damaged settings file is set aside, the defaults are used, and the player is told.
	FFileHelper::SaveStringToFile(TEXT("not json at all"), *SPath);
	{
		FSlopSettings L;
		TestFalse("not loaded", L.Load());
		TestTrue("kept as .bad", FPaths::FileExists(SPath + TEXT(".bad")));
		TestFalse("note for the player", L.LoadNote.IsEmpty());
		TestEqual("defaults", L.MasterVolume, FSlopSettings().MasterVolume);
	}
	// Progress written by a later version is read, and the file is copied aside before this
	// version saves over it.
	const FString PPath = FSlopProgress::Path();
	const FString Keep = PPath + TEXT(".v2");
	PF.DeleteFile(*Keep);
	FFileHelper::SaveStringToFile(TEXT(R"({"version": 2, "survival": {"gorm": 9}, "future_field": [1, 2, 3]})"), *PPath);
	{
		FSlopProgress P;
		P.Load();
		TestEqual("what is understood is read", P.BestSurvival(TEXT("gorm")), 9);
		TestTrue("copy kept", FPaths::FileExists(Keep));
		FString Copy;
		FFileHelper::LoadFileToString(Copy, *Keep);
		TestTrue("the copy has the later version's fields", Copy.Contains(TEXT("future_field")));
	}
	FCommandLine::Set(*Saved);
	return true;
}

SLOP_TEST(FSlopProgressFile, "Saves.ProgressRecordsCorruptionAndMigration")
bool FSlopProgressFile::RunTest(const FString&)
{
	// Works on a test profile (progress_autotest.json) next to, never in, the real files.
	const FString Saved = FCommandLine::Get();
	FCommandLine::Set(*(Saved + TEXT(" -profile=autotest")));
	const FString Path = FSlopProgress::Path();
	const FString OldPath = FPaths::GetPath(SaveDir()) / TEXT("progress_autotest.json");
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bad")));
	IFileManager::Get().Delete(*OldPath);
	{
		FSlopProgress P;
		P.Load();
		TestFalse("first launch: nothing cleared", P.AnyCleared());
		P.RecordArcade(TEXT("ashka"), 1, 300.0, 50000);
		P.RecordArcade(TEXT("ashka"), 1, 250.0, 40000);
		TestTrue("survival best", P.RecordSurvival(TEXT("gorm"), 7));
		TestFalse("not a new best", P.RecordSurvival(TEXT("gorm"), 3));
		TestTrue("trial first time", P.TrialDone(TEXT("nix"), TEXT("Tide")));
		TestFalse("trial again", P.TrialDone(TEXT("nix"), TEXT("Tide")));
		P.SetStranger(3);
		P.SetStranger(1);
	}
	{
		FSlopProgress P;
		P.Load();
		TestEqual("best score kept", P.BestScore(TEXT("ashka"), 1), 50000);
		TestEqual("best time kept", P.BestTime(TEXT("ashka"), 1), 250.0);
		TestEqual("clears", P.Clears(TEXT("ashka"), 1), 2);
		TestTrue("costume unlocked", P.IsUnlocked(TEXT("costume:ashka")));
		TestTrue("roost unlocked", P.IsUnlocked(TEXT("arena:roost")));
		TestTrue("ending seen", P.EndingSeen(TEXT("ashka")));
		TestEqual("survival", P.BestSurvival(TEXT("gorm")), 7);
		TestEqual("stranger memory only moves forward", P.Stranger(), 3);
	}
	// A damaged file is set aside and a fresh profile used.
	FFileHelper::SaveStringToFile(TEXT("{ not json"), *Path);
	{
		FSlopProgress P;
		P.Load();
		TestFalse("fresh after damage", P.AnyCleared());
		TestTrue("damaged file kept as .bad", FPaths::FileExists(Path + TEXT(".bad")));
		TestFalse("note for the player", P.LoadNote.IsEmpty());
	}
	// The original game's progress is read once, and the original file is left alone.
	IFileManager::Get().Delete(*Path);
	const FString OldText = TEXT(R"({"version": 1, "arcade": {"bram:2": {"clears": 1, "best": 400.0, "score": 77000}}, "endings": ["ending:bram"], "unlocks": ["costume:bram"], "stranger": 5, "stats": {"matches": 12}})");
	FFileHelper::SaveStringToFile(OldText, *OldPath);
	{
		FSlopProgress P;
		P.Load();
		TestEqual("old record brought over", P.BestScore(TEXT("bram"), 2), 77000);
		TestEqual("stranger brought over", P.Stranger(), 5);
		FString After;
		FFileHelper::LoadFileToString(After, *OldPath);
		TestEqual("the original file is untouched", After, OldText);
	}
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bad")));
	IFileManager::Get().Delete(*OldPath);
	FCommandLine::Set(*Saved);
	return true;
}

// --- Flow -------------------------------------------------------------------------------------------------

SLOP_TEST(FSlopFlowArcade, "Flow.ArcadeLadderScoringAndEnd")
bool FSlopFlowArcade::RunTest(const FString&)
{
	FGameFlow F;
	for (int32 Tower = 0; Tower < 3; ++Tower)
	{
		const TArray<FRung> R = F.PlanArcade(TEXT("ashka"), Tower, 42);
		const int32 Expected = FGameFlow::Tower(Tower).Fights + 1 + (FGameFlow::Tower(Tower).bMirror ? 1 : 0) + 1;
		TestEqual(FString::Printf(TEXT("tower %d length"), Tower), R.Num(), Expected);
		TestEqual("rival second to last before mirror/boss", R[FGameFlow::Tower(Tower).Fights].Kind, FName(TEXT("rival")));
		TestEqual("boss last", R.Last().Kind, FName(TEXT("boss")));
		TestEqual("boss in the roost", R.Last().Stage, FGameFlow::BossArena());
		TSet<FName> Seen;
		for (int32 I = 0; I < FGameFlow::Tower(Tower).Fights; ++I)
		{
			TestNotEqual("never yourself in the ordinary fights", R[I].Opponent, FName(TEXT("ashka")));
			bool bDup = false;
			Seen.Add(R[I].Opponent, &bDup);
			TestFalse("no opponent twice", bDup);
		}
	}
	const TArray<FRung> R = F.PlanArcade(TEXT("ashka"), 2, 7);
	F.StartArcade(TEXT("ashka"), 2, R);
	TestEqual("first opponent", F.Config.P2, R[0].Opponent);
	FMatchResult Win;
	Win.Winner = 0;
	Win.Health[0] = 500;
	Win.TimeLeft = 40;
	const int32 Pts = F.AddArcadeScore(Win);
	TestEqual("score = win + health + time", Pts, 10000 + 20 * 500 + 100 * 40);
	Win.bFlawless = true;
	Win.Finisher = TEXT("last_light");
	TestEqual("flawless and Sloptality", FGameFlow::FightScore(Win), 10000 + 20 * 500 + 25000 + 15000 + 4000);
	Win.Finisher = TEXT("stage_vat");
	TestEqual("stage finisher", FGameFlow::FightScore(Win), 10000 + 20 * 500 + 25000 + 10000 + 4000);
	int32 Steps = 0;
	while (F.ArcadeAdvance(false, 100.0 + Steps))
	{
		++Steps;
		if (F.CurrentRung()->Kind == TEXT("mirror"))
		{
			TestTrue("the Reflection is dark", F.Config.bMirrorDark);
			TestEqual("against yourself", F.Config.P2, FName(TEXT("ashka")));
		}
		if (F.CurrentRung()->Kind == TEXT("boss"))
		{
			TestEqual("one long round", F.Config.RoundsToWin, 1);
			TestEqual("Master boss health", F.Config.P2Health, 1900);
		}
	}
	TestEqual("climbed every rung", Steps, R.Num() - 1);
	F.ArcadeRetry();
	TestEqual("a continue is counted", F.ArcadeContinues(), 1);
	return true;
}

SLOP_TEST(FSlopFlowSurvival, "Flow.SurvivalWavesHealthAndBoss")
bool FSlopFlowSurvival::RunTest(const FString&)
{
	FGameFlow F;
	F.StartSurvival(TEXT("mirela"), 99);
	const int32 Full = GetContent().Fighter(TEXT("mirela"))->Health;
	FName Last;
	for (int32 Wave = 0; Wave < 12; ++Wave)
	{
		TestEqual("one round a fight", F.Config.RoundsToWin, 1);
		TestEqual("difficulty by wave", F.Config.Difficulty, FMath::Clamp(Wave / 3, 0, 2));
		if (FGameFlow::SurvivalBossWave(Wave))
		{
			TestEqual("the Slopbat every fifth wave", F.Config.P2, FName(TEXT("slopbat")));
		}
		else
		{
			TestNotEqual("never the same opponent twice running", F.Config.P2, Last);
		}
		Last = F.Config.P2;
		F.SurvivalAdvance(Wave % 2 == 0 ? 200 : Full, Wave == 3);
		TestTrue("health carried, capped", F.Config.P1Health > 0 && F.Config.P1Health <= Full);
	}
	TestEqual("waves counted", F.SurvivalWaves(), 12);
	F.StartSurvival(TEXT("mirela"), 99);
	F.SurvivalAdvance(100, false);
	TestEqual("30% back after a win", F.Config.P1Health, 100 + (int32)(Full * 0.3));
	F.StartSurvival(TEXT("mirela"), 99);
	F.SurvivalAdvance(100, true);
	TestEqual("45% back after a flawless one", F.Config.P1Health, 100 + (int32)(Full * (FGameFlow::SurvivalRefill + FGameFlow::SurvivalFlawlessRefill)));
	return true;
}

}

#undef SLOP_TEST

#endif
