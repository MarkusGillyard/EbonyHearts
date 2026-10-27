#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SlopSimHarness.h"

namespace SlopCombatTestSuite
{
using namespace Slop;
using namespace Slop::Test;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

// --- Commands ---------------------------------------------------------------------------------------

SLOP_TEST(FSlopCmdMotionEitherSide, "Combat.Commands.MotionFromEitherSide")
bool FSlopCmdMotionEitherSide::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Motion(0, {D, D | R, R}, HP);
	TestTrue("D,F+HP facing right fires the projectile", H.Started(0, TEXT("orb")));
	H.Place(3.0);
	int32 At = H.Log.Num();
	H.Motion(1, {D, D | L, L}, HP);
	TestTrue("D,F+HP facing left reads forward as screen-left", H.Started(1, TEXT("orb"), At));
	H.Place(3.0);
	At = H.Log.Num();
	H.Motion(1, {D, R}, HP);
	TestFalse("the unmirrored motion does not fire for the left side", H.Started(1, TEXT("orb"), At));
	TestTrue("HP while holding back is still the high punch", H.Started(1, TEXT("hp"), At));
	return true;
}

SLOP_TEST(FSlopCmdCrossover, "Combat.Commands.MotionSurvivesCrossover")
bool FSlopCmdCrossover::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(D);
	H.Step(D);
	// The opponent moves behind mid-motion: the fighter turns before forward is pressed.
	H.F(1).Pos.X = H.F(0).Pos.X - 1.5;
	H.Step(D | R);
	H.Step(R, HP);
	TestTrue("a motion begun before the crossover still comes out", H.Started(0, TEXT("orb")));
	return true;
}

SLOP_TEST(FSlopCmdWindow, "Combat.Commands.MotionWindowExpires")
bool FSlopCmdWindow::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(D);
	H.Idle(FCommand::StepGap + 6);
	H.Step(R);
	H.Step(R, HP);
	TestFalse("a down tap long before forward is not a motion", H.Started(0, TEXT("orb")));
	return true;
}

SLOP_TEST(FSlopCmdHeld, "Combat.Commands.HeldDirections")
bool FSlopCmdHeld::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(D, HP);
	TestTrue("D+HP is the uppercut", H.Started(0, TEXT("uppercut")));
	H.Place(3.0);
	int32 At = H.Log.Num();
	H.Step(L, LK);
	TestTrue("B+LK is the sweep", H.Started(0, TEXT("sweep"), At));
	H.Place(3.0);
	At = H.Log.Num();
	H.Step(L | D, LK);
	TestTrue("down-back + LK is a crouching kick, not a sweep", H.Started(0, TEXT("crouch_lk"), At));
	return true;
}

SLOP_TEST(FSlopCmdBuffered, "Combat.Commands.PressBufferedDuringRecovery")
bool FSlopCmdBuffered::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(0, HK);
	const FMoveDef* M = H.F(0).Move;
	if (!TestTrue("HK started", M && M->Id == TEXT("hk")))
	{
		return false;
	}
	for (int32 I = 0; I < M->Total - 3; ++I)
	{
		H.Step();
	}
	const int32 At = H.Log.Num();
	H.Step(0, LK);
	H.Idle(5);
	TestTrue("LK pressed three frames before recovery ends still comes out", H.Started(0, TEXT("lk"), At));
	return true;
}

SLOP_TEST(FSlopCmdBufferExpires, "Combat.Commands.BufferExpires")
bool FSlopCmdBufferExpires::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(0, HK);
	const FMoveDef* M = H.F(0).Move;
	for (int32 I = 0; I < M->Total - (FInputBuffer::BufferTicks + 4); ++I)
	{
		H.Step();
	}
	const int32 At = H.Log.Num();
	H.Step(0, LK);
	H.Idle(12);
	TestFalse("a press older than the buffer when the fighter becomes free is dropped", H.Started(0, TEXT("lk"), At));
	return true;
}

SLOP_TEST(FSlopCmdOpposites, "Combat.Commands.OppositeDirectionsCancel")
bool FSlopCmdOpposites::RunTest(const FString&)
{
	TestEqual("left and right together are neutral", FInputBuffer::ScreenFrom(true, true, false, false), 5);
	TestEqual("up and down together are neutral", FInputBuffer::ScreenFrom(false, false, true, true), 5);
	TestEqual("all four are neutral", FInputBuffer::ScreenFrom(true, true, true, true), 5);
	TestEqual("down-right is 3", FInputBuffer::ScreenFrom(true, false, false, true), 3);
	FSimHarness H;
	H.Place(3.0);
	H.Idle(6, L | R);
	TestEqual("holding both stands still", (int32)H.F(0).State, (int32)EState::Idle);
	return true;
}

SLOP_TEST(FSlopCmdOnePressOneMove, "Combat.Commands.OnePressStartsOneMove")
bool FSlopCmdOnePressOneMove::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(3.0);
	H.Step(0, LP);
	H.Idle(60);
	TestEqual("one press of LP starts one low punch", H.Count(EEvent::Move), 1);
	// Holding a button never repeats a move: only presses start moves.
	H.Hold[0] = HP;
	H.Idle(60);
	TestEqual("holding HP starts nothing more", H.Count(EEvent::Move), 1);
	H.Hold[0] = 0;
	return true;
}

// --- Timing, hit and block ------------------------------------------------------------------------

SLOP_TEST(FSlopHitStartup, "Combat.Hit.ConnectsOnStartupFrame")
bool FSlopHitStartup::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.9);
	H.Step(0, HP);
	const FMoveDef* M = H.F(0).Def->Move(TEXT("hp"));
	for (int32 I = 0; I < M->Startup - 2; ++I)
	{
		H.Step();
	}
	TestEqual("no hit before the first active frame", H.Count(EEvent::Hit), 0);
	H.Step();
	TestEqual("the high punch hits on its startup frame", H.Count(EEvent::Hit), 1);
	TestEqual("full damage on the first hit", H.F(1).Health, 1000 - M->Damage);
	H.Idle(10);
	TestEqual("one active strike never hits twice", H.Count(EEvent::Hit), 1);
	return true;
}

SLOP_TEST(FSlopHitWhiff, "Combat.Hit.OutOfRangeWhiffs")
bool FSlopHitWhiff::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(2.5);
	H.Step(0, HP);
	H.Idle(20);
	TestEqual("no hit", H.Count(EEvent::Hit), 0);
	return true;
}

SLOP_TEST(FSlopHitLevels, "Combat.Block.HighsLowsAndGuards")
bool FSlopHitLevels::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.9);
	H.Step(0, HP, D, 0);
	H.Idle(12, 0, D);
	TestEqual("a high punch passes over a crouching fighter", H.Count(EEvent::Hit) + H.Count(EEvent::Block), 0);
	H.Place(0.9);
	H.Hold[1] = BL;
	H.Step(L, LK);
	H.Idle(14);
	TestEqual("a sweep is not stopped by a standing block", H.Count(EEvent::Hit), 1);
	H.Place(0.9);
	const int32 At = H.Log.Num();
	H.Step(L, LK, D, 0);
	H.Idle(14, 0, D);
	TestEqual("a crouching block stops the sweep", H.Count(EEvent::Block, At), 1);
	H.Hold[1] = 0;
	return true;
}

SLOP_TEST(FSlopOverhead, "Combat.Block.OverheadBeatsCrouchingBlock")
bool FSlopOverhead::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(2.4);
	H.Hold[1] = BL;
	// A forward jump and an early kick come down on the crouching blocker.
	H.Step(U | R, 0, D, 0);
	H.Idle(3, 0, D);
	for (int32 I = 0; I < 14; ++I)
	{
		H.Step(0, I == 2 ? HK : 0, D, 0);
	}
	TestTrue("the jump kick reaches", H.Until([&] { return H.Count(EEvent::Hit) + H.Count(EEvent::Block) > 0; }, 40, 0, D));
	TestEqual("a jump kick is not stopped by a crouching block", H.Count(EEvent::Hit), 1);
	H.Hold[1] = 0;
	return true;
}

SLOP_TEST(FSlopChip, "Combat.Block.ChipOnlyFromSpecialsAndPushback")
bool FSlopChip::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.9);
	H.Hold[1] = BL;
	const double X0 = H.F(1).Pos.X;
	H.Step(0, HK);
	H.Idle(16);
	TestEqual("blocked", H.Count(EEvent::Block), 1);
	TestEqual("normals do no chip damage", H.F(1).Health, 1000);
	TestTrue("blocking pushes the defender back", H.F(1).Pos.X > X0 + 0.2);
	H.Place(3.0);
	H.Motion(0, {D, R}, HP);
	H.Until([&] { return H.Count(EEvent::Block) >= 2; }, 80);
	TestTrue("a blocked special chips", H.F(1).Health < 1000);
	H.Hold[1] = 0;
	return true;
}

SLOP_TEST(FSlopBlockBehind, "Combat.Block.WorksFromEitherSide")
bool FSlopBlockBehind::RunTest(const FString&)
{
	// Block is a button: a blocker turned the wrong way still blocks (no cross-ups).
	FSimHarness H;
	H.Place(0.9);
	H.Hold[1] = BL;
	H.F(1).Facing = 1;
	H.Step(0, HK);
	H.Idle(16);
	TestEqual("blocked from behind", H.Count(EEvent::Block), 1);
	TestEqual("no hit", H.Count(EEvent::Hit), 0);
	H.Hold[1] = 0;
	return true;
}

SLOP_TEST(FSlopTrade, "Combat.Hit.TradesHitBoth")
bool FSlopTrade::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.85);
	H.Step(0, HP, 0, HP);
	H.Idle(8);
	TestEqual("strikes landing on the same frame both connect", H.Count(EEvent::Hit), 2);
	TestTrue("both hurt", H.F(0).Health < 1000 && H.F(1).Health < 1000);
	return true;
}

SLOP_TEST(FSlopCounterHit, "Combat.Hit.CounterHitDuringWindup")
bool FSlopCounterHit::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.85);
	// P2 starts a slow roundhouse; P1's quick punch lands during its wind-up.
	H.Step(0, 0, R, HK);
	H.Step(0, HP);
	H.Until([&] { return H.Count(EEvent::Hit) > 0; }, 20);
	const FSimEvent* E = H.Find(EEvent::Hit);
	if (TestNotNull("a hit landed", E))
	{
		TestTrue("it is a counter hit", E->bCounter);
		const int32 Base = H.F(0).Def->Move(TEXT("hp"))->Damage;
		TestEqual("15% more damage", E->Damage, FMath::RoundToInt(Base * 1.15));
		// The victim alone freezes 4 frames longer, in place of the 4 frames of stun a counter
		// used to add: the blow reads bigger and the frame advantage is unchanged.
		const FMoveDef* M = H.F(0).Def->Move(TEXT("hp"));
		TestEqual("the victim freezes longer", H.F(1).Hitstop - H.F(0).Hitstop, 4);
		TestEqual("advantage as before: freeze plus stun", H.F(1).Hitstop + H.F(1).Stun - H.F(0).Hitstop, M->Hitstun + 4);
	}
	return true;
}

// --- Damage, chains and run -----------------------------------------------------------------------

SLOP_TEST(FSlopChainMeter, "Combat.Chains.CostRunMeterAndNeedIt")
bool FSlopChainMeter::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.85);
	H.Step(0, HP);
	H.Idle(5);
	H.Step(0, HP);
	H.Until([&] { return H.Count(EEvent::Chain) > 0; }, 12);
	TestEqual("HP, HP chains", H.MoveId(0), FName(TEXT("hp2")));
	TestTrue("the chain link spent run meter", H.F(0).RunMeter < FFighter::RunMax);
	H.Place(3.0);
	H.F(0).RunMeter = 0.0;
	H.F(0).RunIdle = 0;
	H.Step(0, HP);
	H.Idle(5);
	H.Step(0, HP);
	H.Idle(2);
	TestNotEqual("no chain without run meter", H.MoveId(0), FName(TEXT("hp2")));
	return true;
}

SLOP_TEST(FSlopScaling, "Combat.Combos.DamageScales")
bool FSlopScaling::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.85);
	H.F(1).ComboHits = 5;
	H.F(1).SetState(EState::HitStun);
	H.F(1).Stun = 30;
	H.Step(0, HP);
	H.Idle(5);
	const FSimEvent* E = H.Find(EEvent::Hit);
	if (TestNotNull("hit", E))
	{
		TestEqual("sixth hit", E->Combo, 6);
		TestTrue("the sixth hit of a combo does reduced damage", E->Damage < H.F(0).Def->Move(TEXT("hp"))->Damage);
	}
	return true;
}

SLOP_TEST(FSlopRunMeter, "Combat.Run.DrainsAndRefills")
bool FSlopRunMeter::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(5.0);
	H.Hold[0] = RN;
	H.Idle(30, R);
	TestEqual("running", (int32)H.F(0).State, (int32)EState::Run);
	TestTrue("running drains the meter", H.F(0).RunMeter < FFighter::RunMax - 20.0);
	H.Hold[0] = 0;
	const double Low = H.F(0).RunMeter;
	H.Idle(FFighter::RunRegenDelay - 2);
	TestEqual("no refill right after running", H.F(0).RunMeter, Low);
	H.Idle(60);
	TestTrue("the meter refills once left alone", H.F(0).RunMeter >= FMath::Min(FFighter::RunMax, Low + 30.0));
	return true;
}

SLOP_TEST(FSlopRunEmpty, "Combat.Run.StopsWhenEmpty")
bool FSlopRunEmpty::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(5.6, -3.0);
	H.Hold[0] = RN;
	H.F(0).RunMeter = 10.0;
	H.F(0).RunIdle = 0;
	H.Idle(12, R);
	H.Idle(4, R);
	TestNotEqual("an empty meter ends the run", (int32)H.F(0).State, (int32)EState::Run);
	H.Hold[0] = 0;
	return true;
}

// --- Throws -----------------------------------------------------------------------------------------

SLOP_TEST(FSlopThrowBreak, "Combat.Throws.ThrowAndBreak")
bool FSlopThrowBreak::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.7);
	H.Step(R, LP);
	H.Idle(4);
	TestEqual("forward + LP up close throws", H.Count(EEvent::Throw), 1);
	H.Idle(60);
	TestEqual("throw damage", H.F(1).Health, 1000 - H.F(0).Def->Move(TEXT("throw_f"))->Throw.Damage);
	H.Place(0.7);
	int32 At = H.Log.Num();
	H.Step(R, LP);
	H.Idle(4);
	H.Step(0, 0, 0, LP);
	H.Idle(3);
	TestEqual("a punch just after being grabbed breaks free", H.Count(EEvent::ThrowBreak, At), 1);
	H.Place(0.7);
	At = H.Log.Num();
	H.Step(R, LP, L, LP);
	H.Idle(4);
	TestEqual("two throws at once break apart", H.Count(EEvent::ThrowBreak, At), 1);
	return true;
}

SLOP_TEST(FSlopThrowRange, "Combat.Throws.RangeAndBackThrow")
bool FSlopThrowRange::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(2.0);
	H.Step(R, LP);
	H.Idle(4);
	TestEqual("no throw from across the screen", H.Count(EEvent::Throw), 0);
	TestEqual("it is a low punch instead", H.MoveId(0), FName(TEXT("lp")));
	H.Place(0.7);
	H.Step(L, LP);
	H.Idle(70);
	TestTrue("a back throw lands the victim on the other side", H.F(1).Pos.X < H.F(0).Pos.X);
	return true;
}

SLOP_TEST(FSlopGrabBlock, "Combat.Throws.CommandGrabBeatsBlock")
bool FSlopGrabBlock::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.8);
	H.Hold[1] = BL;
	H.Motion(0, {L, R}, LP);
	H.Idle(12);
	TestEqual("a command grab takes a blocking opponent", H.Count(EEvent::Throw), 1);
	H.Hold[1] = 0;
	return true;
}

SLOP_TEST(FSlopThrowLetGo, "Combat.Throws.ThrowerHitLetsGo")
bool FSlopThrowLetGo::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.7);
	H.Step(R, LP);
	H.Idle(8);
	TestEqual("the throw has hold of its victim", (int32)H.F(1).State, (int32)EState::Thrown);
	H.F(0).TakeStun(20, EStun::Mid);
	H.Idle(2);
	TestEqual("the thrower lets go", H.F(0).ThrowVictim, -1);
	TestEqual("no thrower", H.F(1).ThrownBy, -1);
	TestNotEqual("the victim is free", (int32)H.F(1).State, (int32)EState::Thrown);
	TestTrue("and back on their feet", H.Until([&] { return H.F(1).IsActionable(); }, 400));
	return true;
}

// --- Projectiles ------------------------------------------------------------------------------------

SLOP_TEST(FSlopProjOne, "Combat.Projectiles.OneAtATimeAndClash")
bool FSlopProjOne::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(5.6);
	H.Motion(0, {D, R}, HP);
	H.Idle(20, 0, D);
	TestEqual("one in flight", H.Sim.Projectiles.Num(), 1);
	H.Idle(12, 0, D);
	const int32 At = H.Log.Num();
	H.Motion(0, {D, R}, HP);
	H.Idle(4);
	TestFalse("a second projectile waits for the first", H.Started(0, TEXT("orb"), At));
	H.Place(5.0);
	H.Motion(0, {D, R}, HP);
	H.Motion(1, {D, L}, HP);
	H.Until([&] { return H.Count(EEvent::Clash) > 0; }, 60);
	TestEqual("projectiles meeting head-on destroy each other", H.Count(EEvent::Clash), 1);
	H.Idle(2);
	TestEqual("none left", H.Sim.Projectiles.Num(), 0);
	return true;
}

SLOP_TEST(FSlopProjLevels, "Combat.Projectiles.HighDuckedLowBlockedCrouching")
bool FSlopProjLevels::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(4.0);
	H.Motion(0, {D, R}, HP);
	H.Idle(60, 0, D);
	TestEqual("crouching under a high projectile", H.Count(EEvent::Hit) + H.Count(EEvent::Block), 0);
	H.Place(4.0);
	H.Hold[1] = BL;
	H.Motion(0, {D, L}, LP);
	H.Idle(60, 0, D);
	TestEqual("a low projectile is blocked crouching", H.Count(EEvent::Block), 1);
	H.Hold[1] = 0;
	return true;
}

// --- Juggles and determinism -------------------------------------------------------------------------

SLOP_TEST(FSlopJuggle, "Combat.Combos.UppercutLaunchesJuggleLimitHolds")
bool FSlopJuggle::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.8);
	H.Step(D, HP);
	H.Idle(10);
	TestEqual("the uppercut launches", (int32)H.F(1).State, (int32)EState::Juggle);
	TestTrue("high into the air", H.Until([&] { return H.F(1).Pos.Y > 1.5; }, 40));
	H.F(1).JugglePoints = FFighter::JuggleLimit;
	TestTrue("a body past the juggle limit cannot be hit again", H.F(1).IsInvulnerable());
	H.Until([&] { return H.F(1).State == EState::Down; }, 200);
	TestEqual("it lands knocked down", (int32)H.F(1).State, (int32)EState::Down);
	TestTrue("and gets back up", H.Until([&] { return H.F(1).IsActionable(); }, 120));
	return true;
}

SLOP_TEST(FSlopDeterminism, "Combat.Determinism.SameInputsSameMatch")
bool FSlopDeterminism::RunTest(const FString&)
{
	TArray<FString> Traces;
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FSimHarness H;
		H.Place(2.0);
		FRandomStream Rng(1234);
		const int32 D1s[] = {0, R, L, D, U, D | R};
		const int32 D2s[] = {0, R, L, D};
		const int32 B1s[] = {0, 0, 0, HP, LK, HK, LP};
		const int32 B2s[] = {0, 0, BL, LP};
		FString Out;
		for (int32 I = 0; I < 900; ++I)
		{
			H.Step(D1s[Rng.RandRange(0, 5)], B1s[Rng.RandRange(0, 6)], D2s[Rng.RandRange(0, 3)], B2s[Rng.RandRange(0, 3)]);
			Out += FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d;"), H.F(0).Pos.X, H.F(0).Pos.Y, H.F(1).Pos.X, H.F(1).Pos.Y,
				H.F(0).Health, H.F(1).Health, (int32)H.F(0).State, (int32)H.F(1).State);
		}
		Traces.Add(Out);
	}
	TestEqual("the same inputs give the same match", Traces[0], Traces[1]);
	return true;
}

SLOP_TEST(FSlopHarmless, "Combat.Hit.HarmlessStaysHarmless")
bool FSlopHarmless::RunTest(const FString&)
{
	TSharedPtr<FFighterDef> Pusher = FSimHarness::DummyDef(TEXT("pusher"), TEXT(R"({"moves": {"gust": {"kind": "special", "startup": 3, "active": 3,
		"recovery": 10, "level": "mid", "damage": 0, "hitstun": 10, "hitboxes": [{"rect": [0.2, 0.5, 1.0, 1.0]}]}},
		"commands": [["F,F+HK", "gust"]]})"));
	FSimHarness H(Pusher.Get());
	H.Place(1.0);
	H.Motion(0, {R, 0, R}, HK);
	H.Idle(10);
	TestEqual("the gust connects", H.Count(EEvent::Hit), 1);
	TestEqual("and does no damage", H.F(1).Health, 1000);
	return true;
}

SLOP_TEST(FSlopNoOverlap, "Combat.Plane.BodiesNeverOverlapOrLeave")
bool FSlopNoOverlap::RunTest(const FString&)
{
	// Long random play: pushboxes keep bodies apart on the ground and nobody leaves the stage.
	FSimHarness H;
	H.Place(2.0);
	for (FFighter& F : H.Sim.Fighters)
	{
		F.HealthMax = F.Health = 1000000;
	}
	FRandomStream Rng(77);
	const int32 Dirs[] = {0, R, L, D, U | R, U | L, R, L};
	const int32 Btns[] = {0, 0, 0, HP, LP, HK, LK, BL, RN};
	int32 Overlaps = 0, Outside = 0;
	for (int32 I = 0; I < 6000; ++I)
	{
		H.Step(Dirs[Rng.RandRange(0, 7)], Btns[Rng.RandRange(0, 8)], Dirs[Rng.RandRange(0, 7)], Btns[Rng.RandRange(0, 8)]);
		for (FFighter& F : H.Sim.Fighters)
		{
			if (FMath::Abs(F.Pos.X) > FFightSim::StageHalf - F.Def->PushW + 1e-6)
			{
				++Outside;
			}
		}
		FFighter& A = H.F(0);
		FFighter& B = H.F(1);
		if (!A.IsAirborne() && !B.IsAirborne() && A.HasPushbox() && B.HasPushbox() && A.Pushbox().Intersection(B.Pushbox()).W > 0.02)
		{
			++Overlaps;
		}
	}
	TestEqual("never outside the stage", Outside, 0);
	TestEqual("grounded bodies do not overlap", Overlaps, 0);
	return true;
}

}

#undef SLOP_TEST

#endif
