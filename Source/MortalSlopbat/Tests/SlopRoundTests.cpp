#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SlopSimHarness.h"

namespace SlopRoundTestSuite
{
using namespace Slop;
using namespace Slop::Test;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
	TUniquePtr<FSimHarness> MakeMatch(const FFighterDef* D1 = nullptr, const FFighterDef* D2 = nullptr)
	{
		return MakeUnique<FSimHarness>(D1, D2, false);
	}

	void KnockOut(FSimHarness& H, int32 Loser)
	{
		H.F(Loser).Health = 1;
		H.Place(0.85);
		Loser == 1 ? H.Step(0, HP) : H.Step(0, 0, 0, HP);
		H.Idle(10);
	}

	const FFighterDef* Real(const TCHAR* Id) { return GetContent().Fighter(Id); }
}

SLOP_TEST(FSlopRoundKO, "Rounds.KOEndsRoundAndStartsNext")
bool FSlopRoundKO::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	KnockOut(*H, 1);
	TestEqual("a knockout in the first round ends it", (int32)H->Sim.Phase, (int32)EPhase::RoundEnd);
	TestEqual("one win", H->Sim.Wins[0], 1);
	TestTrue("the next round begins", H->Until([&] { return H->Sim.RoundNum == 2; }, 600));
	TestEqual("health is restored for round two", H->F(1).Health, H->F(1).Def->Health);
	TestEqual("no projectiles", H->Sim.Projectiles.Num(), 0);
	return true;
}

SLOP_TEST(FSlopRoundFinish, "Rounds.DecidingKOFinishAndFinisherEndsMatch")
bool FSlopRoundFinish::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.Wins[0] = 1;
	KnockOut(*H, 1);
	TestEqual("the deciding knockout leaves the loser to be finished", (int32)H->Sim.Phase, (int32)EPhase::Finish);
	TestTrue("the loser stands up dizzy", H->Until([&] { return H->F(1).State == EState::Dizzy; }, 300));
	const FSimEvent* P = H->Find(EEvent::FinishPrompt);
	if (TestNotNull("prompt", P))
	{
		TestEqual("pronoun", P->Kind.ToString(), H->F(1).Def->Pronoun);
	}
	H->Place(0.9);
	H->F(1).SetState(EState::Dizzy);
	H->F(1).Health = 0;
	H->Motion(0, {R, 0, R, D}, HP);
	TestEqual("the finisher begins", (int32)H->Sim.Phase, (int32)EPhase::Finisher);
	TestTrue("and the match ends after it", H->Until([&] { return H->Sim.Phase == EPhase::MatchEnd; }, 900));
	const FSimEvent* M = H->Find(EEvent::MatchEnd);
	if (TestNotNull("match end", M))
	{
		TestEqual("winner", M->Winner, 0);
		TestEqual("finisher", M->Move, FName(TEXT("fin")));
	}
	return true;
}

SLOP_TEST(FSlopRoundFinishRange, "Rounds.FinisherNeedsItsRange")
bool FSlopRoundFinishRange::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.Wins[0] = 1;
	KnockOut(*H, 1);
	H->Until([&] { return H->F(1).State == EState::Dizzy; }, 300);
	H->Place(3.0);
	H->F(1).SetState(EState::Dizzy);
	H->Motion(0, {R, 0, R, D}, HP);
	TestNotEqual("a close-range finisher does nothing from across the stage", (int32)H->Sim.Phase, (int32)EPhase::Finisher);
	TestEqual("and says why", H->Count(EEvent::FinisherRange), 1);
	return true;
}

SLOP_TEST(FSlopRoundFinishTimeout, "Rounds.FinishTimesOutOrABlowEndsIt")
bool FSlopRoundFinishTimeout::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.Wins[0] = 1;
	KnockOut(*H, 1);
	H->Until([&] { return H->F(1).State == EState::Dizzy; }, 300);
	TestTrue("the dizzy spell runs out", H->Until([&] { return H->Sim.Phase == EPhase::RoundEnd; }, FFightSim::FinishTicks + 60));
	TUniquePtr<FSimHarness> H2 = MakeMatch();
	H2->Sim.Wins[0] = 1;
	KnockOut(*H2, 1);
	H2->Until([&] { return H2->F(1).State == EState::Dizzy; }, 300);
	H2->Place(0.85);
	H2->F(1).SetState(EState::Dizzy);
	H2->Step(0, HP);
	H2->Idle(8);
	TestEqual("any blow on the dizzy fighter ends the match", (int32)H2->Sim.Phase, (int32)EPhase::RoundEnd);
	return true;
}

SLOP_TEST(FSlopRoundTimeout, "Rounds.TimeoutAndDraw")
bool FSlopRoundTimeout::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->F(1).Health = 600;
	H->Sim.Timer = 2;
	H->Idle(4);
	const FSimEvent* T = H->Find(EEvent::Timeout);
	TestTrue("the healthier fighter wins on time", T && T->Winner == 0);
	TUniquePtr<FSimHarness> H2 = MakeMatch();
	H2->Sim.Timer = 2;
	H2->Idle(4);
	const FSimEvent* T2 = H2->Find(EEvent::Timeout);
	TestTrue("equal health is a draw", T2 && T2->Winner == -1);
	TestTrue("nobody scores", H2->Sim.Wins[0] == 0 && H2->Sim.Wins[1] == 0);
	return true;
}

// Versus can be played on a shorter clock or none.
SLOP_TEST(FSlopRoundClock, "Rounds.ClockCanBeShortOrOff")
bool FSlopRoundClock::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.RoundLength = 0;
	H->Sim.Timer = 2;
	H->Idle(4);
	TestNull("no clock, no time out", H->Find(EEvent::Timeout));
	TestEqual("and the clock stands still", H->Sim.Timer, 2);
	TUniquePtr<FSimHarness> H2 = MakeMatch();
	H2->Sim.RoundLength = 30 * 60;
	KnockOut(*H2, 1);
	H2->Idle(FFightSim::RoundEndTicks + FFightSim::IntroTicks + FFightSim::ReadyTicks + 30);
	TestEqual("the next round has begun", H2->Sim.RoundNum, 2);
	TestTrue(FString::Printf(TEXT("on the short clock (%d ticks left)"), H2->Sim.Timer), H2->Sim.Timer <= 30 * 60 && H2->Sim.Timer > 25 * 60);
	return true;
}

SLOP_TEST(FSlopRoundDoubleKO, "Rounds.DoubleKO")
bool FSlopRoundDoubleKO::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->F(0).Health = 1;
	H->F(1).Health = 1;
	H->Place(0.85);
	H->Step(0, HP, 0, HP);
	H->Idle(10);
	const FSimEvent* E = H->Find(EEvent::RoundEnd);
	TestTrue("double knockout", E && E->How == TEXT("double_ko"));
	TestTrue("nobody wins a double knockout", H->Sim.Wins[0] == 0 && H->Sim.Wins[1] == 0);
	return true;
}

SLOP_TEST(FSlopRoundNoLoop, "Rounds.MatchCannotLoopForever")
bool FSlopRoundNoLoop::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.RoundNum = H->Sim.MaxRounds();
	H->Sim.Timer = 2;
	H->Idle(4);
	TestTrue("after the last round the match ends without a winner", H->Until([&] { return H->Sim.Phase == EPhase::MatchEnd; }, 900));
	const FSimEvent* E = H->Find(EEvent::MatchEnd);
	TestTrue("no winner", E && E->Winner == -1);
	// First to five at 4-3 after a drawn round is not over at the ninth (a fixed cap of nine
	// ended it drawn).
	TUniquePtr<FSimHarness> H5 = MakeMatch();
	H5->Sim.RoundsToWin = 5;
	H5->Sim.RoundNum = 9;
	H5->Sim.Wins[0] = 4;
	H5->Sim.Wins[1] = 3;
	H5->Sim.Timer = 2;
	H5->Idle(4);
	TestTrue("a longer match plays on", H5->Until([&] { return H5->Sim.RoundNum == 10; }, 900));
	TestNull("not ended", H5->Find(EEvent::MatchEnd));
	return true;
}

SLOP_TEST(FSlopRoundRestart, "Rounds.RestartClearsEverything")
bool FSlopRoundRestart::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Motion(0, {D, R}, HP);
	H.Idle(14);
	H.Step(0, LP);
	TestTrue("a projectile is out", H.Sim.Projectiles.Num() > 0);
	H.F(1).Health = 300;
	H.F(0).RunMeter = 10.0;
	H.F(0).Buff = TEXT("armor");
	H.F(0).BuffTicks = 100;
	H.Sim.RestartRound();
	TestEqual("no projectile survives a restart", H.Sim.Projectiles.Num(), 0);
	for (FFighter& F : H.Sim.Fighters)
	{
		TestEqual("health", F.Health, F.Def->Health);
		TestEqual("run meter", F.RunMeter, FFighter::RunMax);
		TestEqual("input history is cleared", F.Input.PressAge(BtnAll, 60), -1);
		TestNull("no move", F.Move);
		TestTrue("no buff", F.Buff.IsNone());
		TestEqual("no projectiles out", F.ProjectilesOut, 0);
	}
	return true;
}

// Ashka's shin check (back, down + low kick) catches a low: the kicker is stung and left open, and
// she moves first. It does nothing against a high blow, which goes through it.
SLOP_TEST(FSlopRoundShinCheck, "Rounds.ShinCheckCatchesLowsOnly")
bool FSlopRoundShinCheck::RunTest(const FString&)
{
	const FFighterDef* Ashka = Real(TEXT("ashka"));
	if (!TestNotNull("ashka", Ashka))
	{
		return false;
	}
	TSharedPtr<FFighterDef> A = FSimHarness::DummyDef(TEXT("a"));
	const FMoveDef* Check = Ashka->Move(TEXT("shin_check"));
	if (!TestNotNull("shin check", Check))
	{
		return false;
	}
	{
		FSimHarness H(A.Get(), Ashka);
		H.Place(2.5);
		H.Motion(1, {R, D}, LK);
		TestEqual("back, down + low kick starts the check", H.MoveId(1), FName(TEXT("shin_check")));
	}
	{
		FSimHarness H(A.Get(), Ashka);
		H.Place(0.85);
		const FMoveDef* Sweep = H.F(0).Def->Move(TEXT("sweep"));
		if (!TestNotNull("sweep", Sweep))
		{
			return false;
		}
		H.F(0).StartMove(*Sweep);
		H.F(1).StartMove(*Check);
		FAdvantageMeter Meter;
		for (int32 T = 0; T < 14; ++T)
		{
			H.Idle(1);
			Meter.Update(H.Sim);
		}
		TestEqual("the check catches a sweep", H.Count(EEvent::Parry), 1);
		TestEqual("and is not hit by it", H.Count(EEvent::Hit), 0);
		TestEqual("the kicker is stung", H.F(0).Health, H.F(0).Def->Health - 30);
		int32 Free[2] = {-1, -1};
		for (int32 T = 0; T < 90 && (Free[0] < 0 || Free[1] < 0); ++T)
		{
			H.Idle(1);
			Meter.Update(H.Sim);
			for (int32 I = 0; I < 2; ++I)
			{
				if (Free[I] < 0 && H.F(I).IsActionable())
				{
					Free[I] = T;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("Ashka moves first (free at %d, the kicker at %d)"), Free[1], Free[0]), Free[1] >= 0 && Free[1] < Free[0]);
		TestTrue("the frame view measures the parry", Meter.bHas && Meter.bParried && Meter.By == 1);
		TestEqual("as Ashka's lead", Meter.Value, Free[0] - Free[1]);
	}
	{
		FSimHarness H(A.Get(), Ashka);
		H.Place(0.85);
		H.F(1).StartMove(*Check);
		H.Idle(2);
		H.Step(0, HP);
		H.Idle(8);
		TestEqual("a high punch is not caught", H.Count(EEvent::Parry), 0);
		TestEqual("it lands", H.Count(EEvent::Hit), 1);
	}
	return true;
}

SLOP_TEST(FSlopRoundParryArmor, "Rounds.ParryCountersAndArmourAbsorbs")
bool FSlopRoundParryArmor::RunTest(const FString&)
{
	const FFighterDef* Bram = Real(TEXT("bram"));
	const FFighterDef* Gorm = Real(TEXT("gorm"));
	if (!TestNotNull("bram", Bram) || !TestNotNull("gorm", Gorm))
	{
		return false;
	}
	TSharedPtr<FFighterDef> A = FSimHarness::DummyDef(TEXT("a"));
	FSimHarness H(A.Get(), Bram);
	H.Place(0.85);
	H.F(1).StartMove(*Bram->Move(TEXT("stand_down")));
	H.Idle(2);
	H.Step(0, HP);
	H.Idle(4);
	TestEqual("Stand Down catches a high punch", H.Count(EEvent::Parry), 1);
	TestEqual("and answers with the baton", H.MoveId(1), FName(TEXT("stand_down_strike")));
	H.Idle(20);
	TestTrue("the counter lands", H.F(0).Health < 1000);
	FSimHarness G(A.Get(), Gorm);
	G.Place(3.0);
	G.F(1).StartMove(*Gorm->Move(TEXT("tallow_hide")));
	G.Idle(40);
	TestEqual("Tallow Hide hardens Gorm", G.F(1).Buff, FName(TEXT("armor")));
	G.Place(0.95);
	G.F(1).Buff = TEXT("armor");
	G.F(1).BuffTicks = 200;
	G.F(1).BuffHits = 2;
	G.Step(0, HP);
	G.Idle(8);
	TestEqual("the first blow is absorbed", G.Count(EEvent::Armor), 1);
	TestNotEqual("without flinching", (int32)G.F(1).State, (int32)EState::HitStun);
	return true;
}

SLOP_TEST(FSlopRoundSinkstep, "Rounds.SinkstepReappearsBehind")
bool FSlopRoundSinkstep::RunTest(const FString&)
{
	const FFighterDef* Nix = Real(TEXT("nix"));
	if (!TestNotNull("nix", Nix))
	{
		return false;
	}
	TSharedPtr<FFighterDef> B = FSimHarness::DummyDef(TEXT("b"));
	FSimHarness H(Nix, B.Get());
	H.Place(2.0);
	H.F(0).StartMove(*Nix->Move(TEXT("sinkstep")));
	H.Until([&] { return H.Count(EEvent::Teleport) > 0; }, 60);
	TestTrue("Nix comes up on the far side", H.F(0).Pos.X > H.F(1).Pos.X);
	TestEqual("facing the opponent", H.F(0).Facing, -1);
	return true;
}

SLOP_TEST(FSlopRoundStorm, "Rounds.StormNeedsHalfHealth")
bool FSlopRoundStorm::RunTest(const FString&)
{
	const FFighterDef* Bat = Real(TEXT("slopbat"));
	if (!TestNotNull("slopbat", Bat))
	{
		return false;
	}
	TSharedPtr<FFighterDef> A = FSimHarness::DummyDef(TEXT("a"));
	FSimHarness H(A.Get(), Bat);
	const FMoveDef* Storm = Bat->Move(TEXT("storm_shroud"));
	TestFalse("no storm at full health", H.F(1).CanUse(*Storm));
	H.F(1).Health = (int32)(Bat->Health * 0.4);
	TestTrue("the storm comes below half health", H.F(1).CanUse(*Storm));
	H.Place(3.0);
	H.F(1).StartMove(*Storm);
	H.Idle(10);
	H.Step(0, LP);
	H.F(0).Pos.X = H.F(1).Pos.X - 0.9;
	H.Step(0, HP);
	H.Idle(10);
	TestNotEqual("a blow during the charge breaks it", H.MoveId(1), FName(TEXT("storm_shroud")));
	return true;
}

SLOP_TEST(FSlopRoundBossThrow, "Rounds.BossCannotBeThrown")
bool FSlopRoundBossThrow::RunTest(const FString&)
{
	const FFighterDef* Bat = Real(TEXT("slopbat"));
	TSharedPtr<FFighterDef> A = FSimHarness::DummyDef(TEXT("a"));
	FSimHarness H(A.Get(), Bat);
	H.Place(0.9);
	H.Step(R, LP);
	H.Idle(5);
	TestEqual("no throw", H.Count(EEvent::Throw), 0);
	return true;
}

// The Slopbat's bite is a grab (a block does not stop it), but a punch as it closes breaks it.
SLOP_TEST(FSlopRoundBossBite, "Rounds.BossBiteCanBeBroken")
bool FSlopRoundBossBite::RunTest(const FString&)
{
	const FFighterDef* Bat = Real(TEXT("slopbat"));
	TSharedPtr<FFighterDef> A = FSimHarness::DummyDef(TEXT("a"));
	for (int32 Break = 0; Break < 2; ++Break)
	{
		FSimHarness H(A.Get(), Bat);
		const int32 Health = H.F(0).Health;
		H.Place(0.9);
		H.Motion(1, {L, 0, L}, LP);
		int32 T = 0;
		for (; T < 30 && H.F(0).ThrownBy < 0; ++T)
		{
			H.Idle(1);
		}
		TestTrue("the bite catches a fighter standing close", H.F(0).ThrownBy == 1);
		H.Idle(2);
		if (Break)
		{
			H.Step(0, LP);
		}
		H.Idle(70);
		if (Break)
		{
			TestEqual("a punch just after the bite breaks free", H.Count(EEvent::ThrowBreak), 1);
			TestEqual("and costs nothing", H.F(0).Health, Health);
		}
		else
		{
			TestEqual("unbroken, the bite does its damage", H.F(0).Health, Health - Bat->Move(TEXT("bite"))->Throw.Damage);
		}
	}
	return true;
}

SLOP_TEST(FSlopRoundLoserShots, "Rounds.DecidingKOClearsLosersProjectiles")
bool FSlopRoundLoserShots::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->Sim.Wins[0] = 1;
	H->Place(5.0);
	H->Motion(1, {D, L}, HP);
	H->Idle(14);
	TestEqual("the loser-to-be has a shot in flight", H->Sim.Projectiles.Num(), 1);
	H->F(1).Health = 0;
	H->Idle(1);
	TestEqual("finish", (int32)H->Sim.Phase, (int32)EPhase::Finish);
	H->Idle(4);
	TestEqual("it dies with the round", H->Sim.Projectiles.Num(), 0);
	TestEqual("and the winner is untouched", H->F(0).Health, H->F(0).MaxHealth());
	return true;
}

SLOP_TEST(FSlopRoundFlawless, "Rounds.FlawlessMatchMeansNoDamageAllMatch")
bool FSlopRoundFlawless::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	H->F(0).Health -= 50;
	H->F(0).DamageTaken += 50;
	KnockOut(*H, 1);
	TestTrue("round two", H->Until([&] { return H->Sim.RoundNum == 2 && H->Sim.Phase == EPhase::Fight; }, 900));
	KnockOut(*H, 1);
	TestEqual("the second knockout decides it", (int32)H->Sim.Phase, (int32)EPhase::Finish);
	TestTrue("the loser stands dizzy", H->Until([&] { return H->F(1).State == EState::Dizzy; }, 400));
	H->Place(0.85);
	H->F(1).SetState(EState::Dizzy);
	H->Step(0, HP);
	TestTrue("a blow ends the match", H->Until([&] { return H->Sim.Phase == EPhase::MatchEnd; }, 1500));
	const FSimEvent* M = H->Find(EEvent::MatchEnd);
	if (TestNotNull("match end", M))
	{
		TestEqual("winner", M->Winner, 0);
		TestFalse("a last round without a scratch is not a flawless match", M->bFlawless);
	}
	return true;
}

SLOP_TEST(FSlopRoundPostKOInput, "Rounds.InputAfterKOChangesNothing")
bool FSlopRoundPostKOInput::RunTest(const FString&)
{
	TUniquePtr<FSimHarness> H = MakeMatch();
	KnockOut(*H, 1);
	const int32 Health0 = H->F(0).Health;
	const int32 Health1 = H->F(1).Health;
	const int32 Moves = H->Count(EEvent::Move);
	for (int32 I = 0; I < 120; ++I)
	{
		H->Step(R, I % 3 == 0 ? HP : 0, L, I % 4 == 0 ? HK : 0);
	}
	TestEqual("the winner is not hurt after the knockout", H->F(0).Health, Health0);
	TestEqual("the loser is not hurt again", H->F(1).Health, Health1);
	TestEqual("no moves start during the round end", H->Count(EEvent::Move), Moves);
	return true;
}

SLOP_TEST(FSlopContentValid, "Content.AllDataLoadsAndValidates")
bool FSlopContentValid::RunTest(const FString&)
{
	FContent& C = GetContent();
	for (const FString& E : C.Errors)
	{
		AddError(E);
	}
	TestEqual("roster fighters loaded", C.Fighters.Num(), FContent::Roster().Num() + FContent::Secret().Num() + FContent::Special().Num() + FContent::Forms().Num());
	for (const auto& KV : C.Fighters)
	{
		TestTrue(KV.Key.ToString() + " has moves", KV.Value->Moves.Num() > 5);
		TestTrue(KV.Key.ToString() + " has commands", KV.Value->Commands.Num() > 0);
	}
	return C.Errors.Num() == 0;
}

}

#undef SLOP_TEST

#endif
