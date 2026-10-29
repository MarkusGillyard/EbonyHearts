#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SlopGmHarness.h"
#include "AI/SlopAI.h"
#include "Combat/SlopCoverage.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// The Grandmaster's form library (every fighter's shape and the Ebony Heart Fragment's), his
// changes of shape inside a combo, his instant kills and the move-coverage report. The prices and
// frames checked here are the ones Docs/Design.md sets out:
//   (a) the opponent facing him, Borrowed Face: exactly half the meter (50), Qhi-enhanced, frame 30;
//   (b) a remembered shape (form library): 30, not enhanced, frame 24;
//   (c) inside a combo of his: the shape's own price, taken on frame 3, at most two a combo;
//   (d) the Fragment: 60 remembered (or 50 copied when it faces him), upkeep doubled, no run meter;
//   (e) Own Face, back to his own body: free, no Qhi back, frame 14 (frame 3 inside a combo).
namespace SlopFormTestSuite
{
using namespace Slop;
using namespace Slop::Test;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

FName ShapeMove(const TCHAR* Id) { return FName(*(FString(TEXT("shape_")) + Id)); }

// Ticks from the start of `Move` to the change of shape that followed it.
int32 TookTicks(const FGmMatch& H, FName Move, FName Kind, int32 Since)
{
	int32 Start = -1;
	for (int32 I = Since; I < H.Log.Num(); ++I)
	{
		const FSimEvent& E = H.Log[I];
		if (E.Type == EEvent::Move && E.Who == 0 && E.Move == Move)
		{
			Start = E.Tick;
		}
		else if (Start >= 0 && E.Type == EEvent::Form && E.Who == 0 && E.Kind == Kind)
		{
			return E.Tick - Start + 1;
		}
	}
	return -1;
}

// The Grandmaster's jab, or another button's blow (in whatever shape), lands on the right-hand fighter as the combo's next hit
// and its hitstop is over: the moment a change of shape can be pressed inside the combo.
bool LandJab(FGmMatch& H, int32 Button = HP)
{
	const int32 Was = H.F(1).ComboHits;
	H.Step(0, Button);
	const bool bHit = H.Until([&H, Was]() { return H.F(1).ComboHits > Was; }, 20);
	H.Until([&H]() { return H.Gm().Hitstop == 0; }, 30);
	return bHit;
}

SLOP_TEST(FSlopGmLibrary, "Fighters.Grandmaster.FormLibraryHoldsEveryFighterAndTheFragment")
bool FSlopGmLibrary::RunTest(const FString&)
{
	const FFighterDef* G = Fighter(TEXT("grandmaster"));
	TArray<FName> Targets = FContent::Roster();
	Targets.Add(TEXT("slopbat"));
	TSet<FString> Inputs;
	for (const FName& Id : Targets)
	{
		const FString Who = Id.ToString();
		const FMoveDef* M = G->Move(ShapeMove(*Who));
		if (!TestNotNull(Who + TEXT(": a shape in the library"), M))
		{
			continue;
		}
		TestTrue(Who + TEXT(": a remembered shape of that fighter, not enhanced"), M->bShapeshift && M->FormTarget == Id && !M->bFormEnhanced);
		TestEqual(Who + TEXT(": its price"), M->FormCost, Id == TEXT("slopbat") ? 60.0 : 30.0);
		TestEqual(Who + TEXT(": taken on frame 24"), M->FormFrame, 24);
		TestTrue(Who + TEXT(": its source linked"), G->FormSources.FindRef(Id) == Fighter(*Who));
		const FString Input = G->NotationOf(M->Id);
		TestFalse(Who + TEXT(": on an input of its own"), Input.IsEmpty() || Inputs.Contains(Input));
		Inputs.Add(Input);
	}
	TestNull("no shape of himself", G->Move(ShapeMove(TEXT("grandmaster"))));
	TestTrue("the Fragment's shape is hard to hold", Fighter(TEXT("slopbat"))->ShapeUpkeep == 2.0);
	// Every shape keeps the way out, the way into the others and Heart Stop.
	const TSharedRef<FFighterDef> Shape = MakeBorrowedShape(*Fighter(TEXT("bram")), *G, false);
	for (const FName& Id : Targets)
	{
		TestNotNull(FString::Printf(TEXT("Bram's shape keeps the way into %s's"), *Id.ToString()), Shape->Move(ShapeMove(*Id.ToString())));
	}
	TestTrue("and Own Face, and Heart Stop", Shape->Move(TEXT("own_face")) && Shape->Move(TEXT("heart_stop")));
	return true;
}

SLOP_TEST(FSlopGmRemembered, "Fighters.Grandmaster.RememberedShapeCostsThirtyAndIsNotEnhanced")
bool FSlopGmRemembered::RunTest(const FString&)
{
	// (b) Nix's shape against Ashka: D + Run + LP.
	FGmMatch H(TEXT("ashka"));
	const int32 At = H.Log.Num();
	H.Step(D, RN | LP);
	TestTrue("the shape takes", H.Until([&H]() { return H.Gm().Form == EForm::Borrowed; }, 60));
	const FFighterDef* Nix = Fighter(TEXT("nix"));
	TestTrue("Nix's shape, though Ashka faces him", H.Gm().Def->CopyOf == Nix);
	TestEqual("30 Qhi", H.Gm().Qhi, 70.0);
	TestEqual("taken on frame 24", TookTicks(H, ShapeMove(TEXT("nix")), TEXT("borrowed"), At), 24);
	TestEqual("not Qhi-enhanced", H.Gm().Def->Power, Nix->Power);
	const FSimEvent* Took = H.FormEvent(TEXT("borrowed"), At);
	TestTrue("said to be remembered", Took && Took->How == TEXT("remembered"));
	TestTrue("with a run meter", H.Gm().HasRunMeter());
	TestEqual("his own health", H.Gm().MaxHealth(), Fighter(TEXT("grandmaster"))->Health);
	// The same shape again: refused, nothing paid.
	H.Idle(20);
	const double Before = H.Gm().Qhi;
	H.Step(D, RN | LP);
	H.Idle(30);
	const FSimEvent* E = H.FormEvent(TEXT("refused"), At);
	TestTrue("the same shape again: refused", E && E->How == TEXT("same"));
	TestTrue("nothing more paid", H.Gm().Qhi <= Before && H.Gm().Qhi > Before - 5.0);
	// From one remembered shape straight into another: Bram's, for its own price.
	H.Gm().Qhi = 100.0;
	H.Idle(5);
	H.Step(0, RN | HP);
	double Prev = H.Gm().Qhi;
	for (int32 T = 0; T < 60 && H.Gm().Def->CopyOf != Fighter(TEXT("bram")); ++T)
	{
		Prev = H.Gm().Qhi;
		H.Idle(1);
	}
	TestTrue("into Bram's", H.Gm().Def->CopyOf == Fighter(TEXT("bram")));
	// (Nix's shape costs its upkeep until the change, as any shape does.)
	TestTrue(FString::Printf(TEXT("for 30 (%.2f)"), Prev - H.Gm().Qhi), FMath::IsNearlyEqual(Prev - H.Gm().Qhi, 30.0, 0.2));
	// (a) Against that, the opponent's shape, from his own body: 50, enhanced, frame 30.
	FGmMatch C(TEXT("ashka"));
	const int32 CAt = C.Log.Num();
	TestTrue("Borrowed Face", C.Borrow());
	TestEqual("50 Qhi, exactly half", C.Gm().Qhi, 50.0);
	TestEqual("taken on frame 30", TookTicks(C, TEXT("borrowed_face"), TEXT("borrowed"), CAt), 30);
	TestEqual("Qhi-enhanced", C.Gm().Def->Power, Fighter(TEXT("ashka"))->Power * C.Gm().QhiRules().CopyPower);
	const FSimEvent* Copied = C.FormEvent(TEXT("borrowed"), CAt);
	TestTrue("said to be copied", Copied && Copied->How == TEXT("copied"));
	return true;
}

SLOP_TEST(FSlopGmOwnFace, "Fighters.Grandmaster.OwnFaceIsFreeAndInABorrowedShapeAlwaysMeansOwnFace")
bool FSlopGmOwnFace::RunTest(const FString&)
{
	// (e) In a remembered shape, facing someone else, D,B,F + Block is Own Face, not a copy.
	FGmMatch H(TEXT("ashka"));
	H.Step(0, RN | HK);
	TestTrue("Gorm's shape", H.Until([&H]() { return H.Gm().Def->CopyOf == Fighter(TEXT("gorm")); }, 60));
	H.Idle(15);
	const double Before = H.Gm().Qhi;
	const int32 At = H.Log.Num();
	H.Shapeshift();
	TestTrue("his own body again", H.Until([&H]() { return H.Gm().Form == EForm::Own; }, 40));
	TestNull("no copy of Ashka on the way", H.FormEvent(TEXT("borrowed"), At));
	TestNull("and no refusal called out", H.FormEvent(TEXT("refused"), At));
	TestEqual("taken on frame 14", TookTicks(H, TEXT("own_face"), TEXT("own"), At), 14);
	TestTrue(FString::Printf(TEXT("free, and nothing back (%.2f, was %.2f)"), H.Gm().Qhi, Before), H.Gm().Qhi <= Before && H.Gm().Qhi > Before - 2.0);
	return true;
}

SLOP_TEST(FSlopGmFragment, "Fighters.Grandmaster.FragmentShapeCostsSixtyHasNoRunMeterAndDoubleUpkeep")
bool FSlopGmFragment::RunTest(const FString&)
{
	const FQhiRules& Q = Fighter(TEXT("grandmaster"))->Qhi;
	// (d) Remembered: D + Run + HK, against Ashka.
	FGmMatch H(TEXT("ashka"));
	H.Step(D, RN | HK);
	TestTrue("the Fragment's shape takes", H.Until([&H]() { return H.Gm().Form == EForm::Borrowed; }, 60));
	TestTrue("the Fragment's", H.Gm().Def->CopyOf == Fighter(TEXT("slopbat")));
	TestEqual("60 Qhi", H.Gm().Qhi, 40.0);
	TestFalse("no run meter: the Fragment flies", H.Gm().HasRunMeter());
	TestEqual("no second meter at all", H.Gm().Def->Resource, EResource::None);
	TestFalse("he is no boss inside it", H.Gm().Def->bBoss || H.Gm().Def->bUnthrowable);
	TestEqual("his own health, not the Fragment's", H.Gm().MaxHealth(), Fighter(TEXT("grandmaster"))->Health);
	H.Idle(12);
	double Was = H.Gm().Qhi;
	H.Idle(60);
	TestTrue(FString::Printf(TEXT("double upkeep (%.2f a second)"), Was - H.Gm().Qhi), FMath::IsNearlyEqual(Was - H.Gm().Qhi, 60 * Q.Upkeep * 2.0, 0.01));
	H.Place(2.5);
	const int32 At = H.Log.Num();
	H.Motion(0, {D, D | R, R}, HP);
	H.Idle(5);
	TestTrue("the Fragment's own moves: Tar Spit", H.Started(0, TEXT("tar_spit"), At));
	// Facing the Fragment itself, Borrowed Face is the opponent's price: exactly half, enhanced,
	// and its upkeep is still doubled.
	FGmMatch B(TEXT("slopbat"));
	TestTrue("copied from the boss facing him", B.Borrow());
	TestEqual("for exactly half", B.Gm().Qhi, 50.0);
	TestTrue("enhanced", B.Gm().Def->CopyOf == Fighter(TEXT("slopbat")) && B.Gm().Def->Power == Fighter(TEXT("slopbat"))->Power * Q.CopyPower);
	TestEqual("its upkeep doubled", B.Gm().UpkeepScale, 2.0);
	B.Idle(12);
	Was = B.Gm().Qhi;
	B.Idle(60);
	TestTrue("double upkeep there too", FMath::IsNearlyEqual(Was - B.Gm().Qhi, 60 * Q.Upkeep * 2.0, 0.01));
	return true;
}

SLOP_TEST(FSlopGmInCombo, "Fighters.Grandmaster.ChangesShapeInsideACombo")
bool FSlopGmInCombo::RunTest(const FString&)
{
	// (c) Jab, into Ashka's shape, her jab, back to his own body, his jab: one combo throughout.
	FGmMatch H(TEXT("bram"));
	H.Place(0.6);
	if (!TestTrue("his jab lands", LandJab(H)))
	{
		return false;
	}
	int32 At = H.Log.Num();
	H.Step(0, RN | LP);
	TestTrue("Ashka's shape, inside the combo", H.Until([&H]() { return H.Gm().Form == EForm::Borrowed; }, 10));
	TestEqual("taken on frame 3", TookTicks(H, ShapeMove(TEXT("ashka")), TEXT("borrowed"), At), 3);
	TestEqual("for the shape's own price", H.Gm().Qhi, 70.0);
	TestEqual("one change in this combo", H.Gm().FormShifts, 1);
	TestTrue("the opponent still caught", H.F(1).State == EState::HitStun);
	TestTrue("ready at once", H.Gm().IsActionable());
	// Her jab; D,B,F is buffered in its hitstop and Block pressed as the hitstop ends.
	H.Step(0, HP);
	TestTrue("her jab lands in the same combo", H.Until([&H]() { return H.F(1).ComboHits == 2; }, 20));
	for (const int32 Dir : {D, D | L, L, R})
	{
		H.Step(Dir);
	}
	H.Until([&H]() { return H.Gm().Hitstop == 0; }, 30, R);
	At = H.Log.Num();
	H.Step(R, BL);
	TestTrue("Own Face, inside the combo", H.Until([&H]() { return H.Gm().Form == EForm::Own; }, 10));
	TestEqual("frame 3 here too", TookTicks(H, TEXT("own_face"), TEXT("own"), At), 3);
	TestEqual("two changes", H.Gm().FormShifts, 2);
	// (Not the jab or the low kick: D,B,F is still in the buffer, so HP is Still Palm, LK Drift.)
	TestTrue("his high kick, the third hit", LandJab(H, HK) && H.F(1).ComboHits == 3);
	TestTrue("two shapes in this combo", H.Gm().ComboShapes.Num() == 2);
	// A third change in one combo is too many.
	At = H.Log.Num();
	H.Step(0, RN | HP);
	H.Idle(6);
	TestFalse("no third change", H.Started(0, ShapeMove(TEXT("bram")), At));
	TestEqual("still his own body", H.Gm().Form, EForm::Own);
	// When the combo is over the count starts again.
	H.Until([&H]() { return H.F(1).ComboHits == 0; }, 120);
	H.Idle(2);
	TestEqual("counted afresh", H.Gm().FormShifts, 0);
	return true;
}

SLOP_TEST(FSlopGmTwoFingers, "Fighters.Grandmaster.TwoFingersOneStrikeKillsAndHasCounterplay")
bool FSlopGmTwoFingers::RunTest(const FString&)
{
	// F,B + HP catches a high or mid blow; with 75 Qhi, One Strike ends the round.
	{
		FGmMatch H(TEXT("ashka"));
		H.Place(0.8);
		H.Motion(0, {R, L}, HP);
		H.Step(0, 0, 0, HP);
		TestTrue("caught and held", H.Until([&H]() { return H.Gm().Move && H.Gm().Move->Id == TEXT("one_strike"); }, 30));
		TestEqual("75 Qhi", H.Gm().Qhi, 25.0);
		H.Idle(150);
		const FSimEvent* Hit = H.Find(EEvent::Hit);
		TestTrue("One Strike: an instant kill", Hit && Hit->bKill && Hit->Move == TEXT("one_strike") && Hit->Who == 1);
		TestEqual("whatever her health was", H.F(1).Health, 0);
		TestNotEqual("the round is over", H.Sim.Phase, EPhase::Fight);
	}
	// Without the 75 to pay it is only a parry.
	{
		FGmMatch H(TEXT("ashka"));
		H.Gm().Qhi = 60.0;
		H.Place(0.8);
		H.Motion(0, {R, L}, HP);
		H.Step(0, 0, 0, HP);
		H.Idle(90);
		TestNotNull("caught", H.Find(EEvent::Parry));
		TestFalse("not struck", H.Started(0, TEXT("one_strike")));
		TestEqual("her health untouched", H.F(1).Health, H.F(1).MaxHealth());
	}
	// A low blow is not caught: he is hit.
	{
		FGmMatch H(TEXT("ashka"));
		H.Place(0.8);
		H.Motion(0, {R, L}, HP);
		H.Step(0, 0, D, LK);
		H.Idle(60, 0, D);
		TestNull("a low blow goes under the fingers", H.Find(EEvent::Parry));
		TestTrue("and lands", H.Gm().Health < H.Gm().MaxHealth());
		TestEqual("no kill", H.F(1).Health, H.F(1).MaxHealth());
	}
	// The Fragment cannot be held: caught, it is only parried.
	{
		FGmMatch H(TEXT("slopbat"));
		H.Place(1.0);
		H.Motion(0, {R, L}, HP);
		H.Step(0, 0, 0, HP);
		H.Idle(90);
		TestFalse("no hold on the Fragment", H.Started(0, TEXT("one_strike")));
		TestTrue("it lives", H.F(1).Health > 0);
	}
	return true;
}

SLOP_TEST(FSlopGmCrack, "Fighters.Grandmaster.FindTheCrackKillsAStandingOpponentUnlessInterrupted")
bool FSlopGmCrack::RunTest(const FString&)
{
	// B,F,B,F + HP against a standing opponent who does nothing: the round ends.
	{
		FGmMatch H(TEXT("gorm"));
		H.Place(1.0);
		H.Motion(0, {L, R, L, R}, HP);
		TestTrue("begun", H.Started(0, TEXT("find_the_crack")));
		TestEqual("90 Qhi", H.Gm().Qhi, 10.0);
		H.Idle(90);
		const FSimEvent* Hit = H.Find(EEvent::Hit);
		TestTrue("an instant kill, through the guard", Hit && Hit->bKill && Hit->Move == TEXT("find_the_crack"));
		TestEqual("Gorm's health gone", H.F(1).Health, 0);
	}
	// Any blow in its long startup breaks it; the Qhi is gone all the same.
	{
		FGmMatch H(TEXT("gorm"));
		H.Place(0.8);
		H.Motion(0, {L, R, L, R}, HP);
		H.Idle(10);
		H.Step(0, 0, 0, HP);
		H.Idle(90);
		TestFalse("no kill", H.Log.ContainsByPredicate([](const FSimEvent& E) { return E.bKill; }));
		TestTrue("Gorm lives", H.F(1).Health > 0);
		TestTrue("no refund", H.Gm().Qhi < 20.0);
	}
	// Never begun against an opponent off the ground.
	{
		FGmMatch H(TEXT("gorm"));
		H.Place(1.0);
		H.Step(0, 0, U, 0);
		TestTrue("Gorm is in the air", H.Until([&H]() { return H.F(1).IsAirborne(); }, 12));
		H.Motion(0, {L, R, L, R}, HP);
		TestFalse("not begun", H.Started(0, TEXT("find_the_crack")));
		TestEqual("nothing paid", H.Gm().Qhi, 100.0);
	}
	return true;
}

SLOP_TEST(FSlopGmHeartStop, "Fighters.Grandmaster.HeartStopEndsOnlyALongComboOfTwoShapes")
bool FSlopGmHeartStop::RunTest(const FString&)
{
	auto Caught = [](FGmMatch& H, int32 Hits, std::initializer_list<const TCHAR*> Shapes)
	{
		H.Place(0.7);
		H.F(1).SetState(EState::HitStun);
		H.F(1).Stun = 40;
		H.F(1).ComboHits = Hits;
		H.Gm().ComboShapes.Reset();
		for (const TCHAR* S : Shapes)
		{
			H.Gm().ComboShapes.Add(FName(S));
		}
	};
	{
		FGmMatch H(TEXT("hollis"));
		H.Place(0.7);
		H.Motion(0, {R, D, R}, HP);
		TestFalse("not from a standstill", H.Started(0, TEXT("heart_stop")));
	}
	{
		FGmMatch H(TEXT("hollis"));
		Caught(H, 7, {TEXT("grandmaster")});
		H.Motion(0, {R, D, R}, HP);
		TestFalse("not in a combo of one shape", H.Started(0, TEXT("heart_stop")));
	}
	{
		FGmMatch H(TEXT("hollis"));
		Caught(H, 4, {TEXT("grandmaster"), TEXT("ashka")});
		H.Motion(0, {R, D, R}, HP);
		TestFalse("not in a short combo", H.Started(0, TEXT("heart_stop")));
	}
	{
		FGmMatch H(TEXT("hollis"));
		H.Gm().Qhi = 64.0;
		Caught(H, 5, {TEXT("grandmaster"), TEXT("ashka")});
		H.Motion(0, {R, D, R}, HP);
		TestTrue("five hits, two shapes: begun", H.Started(0, TEXT("heart_stop")));
		TestEqual("for all his Qhi", H.Gm().Qhi, 0.0);
		H.Idle(60);
		const FSimEvent* Hit = H.Find(EEvent::Hit);
		TestTrue("an instant kill", Hit && Hit->bKill && Hit->Move == TEXT("heart_stop"));
		TestEqual("Hollis's health gone", H.F(1).Health, 0);
	}
	return true;
}

SLOP_TEST(FSlopGmCoverage, "Fighters.Grandmaster.EveryMoveIsCovered")
bool FSlopGmCoverage::RunTest(const FString&)
{
	const TArray<FCoverageRow> Rows = BuildMoveCoverage(GetContent());
	int32 Counted = 0;
	TSet<FName> Fighters;
	for (const FCoverageRow& R : Rows)
	{
		Fighters.Add(R.Fighter);
		if (R.Via != TEXT("excluded"))
		{
			++Counted;
			TestTrue(FString::Printf(TEXT("%s %s (%s): %s"), *R.Fighter.ToString(), *R.Move.ToString(), *R.Via.ToString(), *R.Detail), R.bCovered);
		}
	}
	for (const FName& Id : FContent::Roster())
	{
		TestTrue(Id.ToString() + TEXT(" listed"), Fighters.Contains(Id));
	}
	TestTrue("the Fragment listed", Fighters.Contains(TEXT("slopbat")));
	TestTrue("Daxion's techniques listed", Fighters.Contains(TEXT("daxion")));
	TestTrue("hundreds of moves", Counted > 150);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Reports/MoveCoverage.md");
	FFileHelper::SaveStringToFile(MoveCoverageReport(Rows), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	AddInfo(FString::Printf(TEXT("%d moves counted; report: %s"), Counted, *Path));
	return true;
}

// Daxion is out of Ebony Hearts: nothing the game loads names him, and what he was is kept whole in
// the archive for Slopperfall (his techniques live on as the Grandmaster's, checked above).
SLOP_TEST(FSlopDaxionArchived, "Content.DaxionIsArchivedNotInTheGame")
bool FSlopDaxionArchived::RunTest(const FString&)
{
	const FContent& C = GetContent();
	TestNull("no fighter", C.Fighter(TEXT("daxion")));
	TArray<FName> All = FContent::Roster();
	All.Append(FContent::Secret());
	All.Append(FContent::Special());
	All.Append(FContent::Forms());
	for (const FName& Id : All)
	{
		const FFighterDef* D = C.Fighter(Id);
		TestTrue(Id.ToString() + TEXT(": not wearing his body"), D && D->Body != TEXT("daxion"));
	}
	TestFalse("no encounter in the story", C.Story.IsValid() && C.Story->HasField(TEXT("stranger")));
	FString Mocap;
	FFileHelper::LoadFileToString(Mocap, *(FPaths::ProjectContentDir() / TEXT("Data/poses_mocap.json")));
	TestFalse("no poses of his", Mocap.Contains(TEXT("mc_dax_")));
	TestFalse("no body cooked", FPaths::DirectoryExists(FPaths::ProjectContentDir() / TEXT("Slop/Fighters/daxion")));
	const FString Archive = FPaths::ProjectDir() / TEXT("Archive/Slopperfall/Daxion");
	for (const TCHAR* F : {TEXT("README.md"), TEXT("Data/fighter.json"), TEXT("Data/look.json"), TEXT("Data/poses.json"),
		TEXT("Data/poses_mocap_evade.json"), TEXT("Data/story.json"), TEXT("Content/Fighters_daxion/daxion.uasset"), TEXT("portrait.png")})
	{
		TestTrue(FString::Printf(TEXT("archived: %s"), F), FPaths::FileExists(Archive / F));
	}
	return true;
}

// His CPU at Master and at the secret challenge's tier, over a set of matches, uses every one of
// his arts (counted by the development telemetry): the fighters' shapes (copied and remembered),
// the Fragment's shape, changes of shape inside combos, Own Face and his instant kills.
SLOP_TEST(FSlopGmCpuArts, "AI.GrandmasterCpuUsesHisArts")
bool FSlopGmCpuArts::RunTest(const FString&)
{
	FCpuBrain::FQhiTelemetry T;
	int32 Matches = 0;
	for (int32 Level = 2; Level <= 3; ++Level)
	{
		for (const FName& Opp : FContent::Roster())
		{
			for (int32 Seed = 0; Seed < 3; ++Seed)
			{
				FFightSim Sim;
				Sim.bIntro = false;
				Sim.Setup(Fighter(TEXT("grandmaster")), GetContent().Fighter(Opp));
				FCpuBrain Gm(Sim, 0, Level, 300 + Seed * 7 + Level);
				FCpuBrain Other(Sim, 1, 2, 400 + Seed * 11);
				for (int32 Tick = 0; Tick < 60 * 60 * 6 && Sim.Phase != EPhase::MatchEnd; ++Tick)
				{
					FPadState Pads[2] = {Gm.Think(), Other.Think()};
					Sim.Step(Pads);
				}
				T.Add(Gm.Telemetry());
				++Matches;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d matches: %s"), Matches, *T.Summary()));
	int32 Shapes = 0;
	for (const FName& Id : FContent::Roster())
	{
		Shapes += T.Forms.FindRef(Id);
	}
	TestTrue("he takes the fighters' shapes", Shapes > Matches);
	TestTrue("takes the Fragment's shape", T.Forms.FindRef(TEXT("slopbat")) > 0);
	TestTrue(FString::Printf(TEXT("changes shape inside combos (%d)"), T.ComboShifts), T.ComboShifts >= Matches / 4);
	TestTrue("gives shapes back with Own Face", T.Forms.FindRef(TEXT("own")) > 0);
	int32 Tries = 0;
	for (const TPair<FName, int32>& KV : T.KillTries)
	{
		Tries += KV.Value;
	}
	TestTrue(FString::Printf(TEXT("tries his instant kills (%d)"), Tries), Tries > 0);
	TestTrue("rarely breaks a shape at these levels", T.Breaks < Matches);
	return true;
}

#undef SLOP_TEST
}

#endif
