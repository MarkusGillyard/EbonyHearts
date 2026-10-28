#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/SlopAI.h"
#include "Tests/SlopSimHarness.h"
#include "Core/SlopLog.h"

namespace SlopAITestSuite
{
using namespace Slop;
using namespace Slop::Test;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
	FSimEvent HitEv(int32 By, const TCHAR* Move, int32 Combo)
	{
		FSimEvent E;
		E.Type = EEvent::Hit;
		E.By = By;
		E.Move = Move;
		E.Combo = Combo;
		return E;
	}

	FName RivalOf(FName Id)
	{
		const TSharedPtr<FJsonObject> Fighters = Json::Obj(GetContent().Story, TEXT("fighters"));
		return FName(*Json::Str(Json::Obj(Fighters, *Id.ToString()), TEXT("rival"), TEXT("gorm")));
	}

	// Whether `Notation` can begin now (or within the ticks its motion takes to enter).
	bool CanStart(const FFighter& F, const FString& Notation)
	{
		if (F.IsActionable())
		{
			return true;
		}
		if (F.State != EState::Move || !F.Move || F.Hitstop > 0)
		{
			return false;
		}
		const FMoveDef& M = *F.Move;
		// A link is dialled once the blow before it has landed (a player confirms the hit).
		if (F.MoveContact.IsNone())
		{
			return false;
		}
		for (const FChainLink& Link : M.Chain)
		{
			if (Link.Input == Notation && F.MF >= Link.From - 1 && F.MF <= Link.To - 1)
			{
				return true;
			}
		}
		if (M.Cancel != FIntPoint::ZeroValue)
		{
			const FCommand C = FCommand::Parse(Notation);
			const int32 Lead = C.Steps.Num() + 2;
			for (const FCommand& Cmd : F.Def->Commands)
			{
				const FMoveDef* Target = F.Def->Move(Cmd.Move);
				if (Cmd.Text == Notation && Target && Target->Kind == EMoveKind::Special && F.MF >= M.Cancel.X - Lead && F.MF <= M.Cancel.Y - 1)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool Perform(FName Id, FName Rival, const TArray<FString>& Steps, const TArray<FName>& Moves)
	{
		FSimHarness H(GetContent().Fighter(Id), GetContent().Fighter(Rival));
		H.Sim.bInfiniteRun = true;
		// From touching distance, where a player dials a combo.
		H.Place(0.4);
		const FPadState Zero[2];
		H.Sim.Step(Zero);
		FTrialCheck Check;
		Check.Moves = Moves;
		Check.Who = 0;
		FPadIntent Intent;
		int32 Si = 0;
		int32 Waited = 0;
		for (int32 Tick = 0; Tick < 700; ++Tick)
		{
			FFighter& F = H.F(0);
			if (!Intent.Busy() && Si < Steps.Num())
			{
				if (CanStart(F, Steps[Si]))
				{
					Intent.Command(Steps[Si]);
					++Si;
					Waited = 0;
				}
				else if (++Waited > 150)
				{
					return false;
				}
			}
			FPadState Pads[2];
			Pads[0] = Intent.Busy() ? Intent.Next(F.Facing) : FPadIntent::Pad(5, F.Facing);
			H.Sim.Step(Pads);
			for (const FSimEvent& E : H.Sim.Events)
			{
				if (Check.Feed(E) == FTrialCheck::EResult::Done)
				{
					return true;
				}
			}
		}
		return false;
	}
}

SLOP_TEST(FSlopTrialChecker, "Trials.CheckerCountsInOrderAndDrops")
bool FSlopTrialChecker::RunTest(const FString&)
{
	using R = FTrialCheck::EResult;
	FTrialCheck C;
	C.Moves = {TEXT("hp"), TEXT("hp2"), TEXT("ashka_elbow_chain")};
	TestTrue("first", C.Feed(HitEv(0, TEXT("hp"), 1)) == R::Hit);
	TestTrue("the other fighter's hits do not count", C.Feed(HitEv(1, TEXT("hp2"), 2)) == R::None);
	TestTrue("the wrong move drops the attempt", C.Feed(HitEv(0, TEXT("lp"), 2)) == R::Dropped);
	TestEqual("reset", C.Progress, 0);
	C.Feed(HitEv(0, TEXT("hp"), 1));
	C.Feed(HitEv(0, TEXT("hp2"), 2));
	FSimEvent End;
	End.Type = EEvent::ComboEnd;
	End.Who = 1;
	TestTrue("the combo ending drops it", C.Feed(End) == R::Dropped);
	C.Feed(HitEv(0, TEXT("hp"), 1));
	C.Feed(HitEv(0, TEXT("hp2"), 2));
	TestTrue("done", C.Feed(HitEv(0, TEXT("ashka_elbow_chain"), 3)) == R::Done);
	FTrialCheck One;
	One.Moves = {TEXT("vat_press")};
	TestTrue("a one-hit trial", One.Feed(HitEv(0, TEXT("vat_press"), 1)) == R::Done);
	return true;
}

SLOP_TEST(FSlopTrialComboEnd, "Trials.ComboEndIsReported")
bool FSlopTrialComboEnd::RunTest(const FString&)
{
	FSimHarness H;
	H.Place(0.9);
	H.Step(0, HP);
	H.Idle(12);
	TestEqual("one hit", H.Count(EEvent::Hit), 1);
	H.Idle(40);
	TestEqual("the combo ends when the hit fighter returns to neutral", H.Count(EEvent::ComboEnd), 1);
	return true;
}

SLOP_TEST(FSlopTrialsPossible, "Trials.EveryTrialIsPossible")
bool FSlopTrialsPossible::RunTest(const FString&)
{
	int32 Total = 0;
	for (const FName& Id : FContent::Roster())
	{
		const FName Rival = RivalOf(Id);
		const TArray<TSharedPtr<FJsonValue>>* List = Json::Arr(GetContent().Trials, *Id.ToString());
		if (!List)
		{
			AddError(Id.ToString() + TEXT(" has no trials"));
			continue;
		}
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> T = V->AsObject();
			TArray<FString> Steps;
			TArray<FName> Moves;
			for (const TSharedPtr<FJsonValue>& S : *Json::Arr(T, TEXT("steps")))
			{
				Steps.Add(S->AsString());
			}
			for (const TSharedPtr<FJsonValue>& M : *Json::Arr(T, TEXT("moves")))
			{
				Moves.Add(FName(*M->AsString()));
			}
			++Total;
			TestTrue(FString::Printf(TEXT("%s: %s (%s)"), *Id.ToString(), *Json::Str(T, TEXT("name")), *FString::Join(Steps, TEXT(", "))),
				Perform(Id, Rival, Steps, Moves));
		}
	}
	TestTrue(FString::Printf(TEXT("the roster has trials (%d)"), Total), Total >= 24);
	return true;
}

SLOP_TEST(FSlopCpuAllPairs, "AI.CpuAgainstCpuEveryPairing")
bool FSlopCpuAllPairs::RunTest(const FString&)
{
	// Every pairing, each side, at every difficulty: a whole match runs to its end, nobody is
	// stuck in one state for long, nobody leaves the stage.
	const TArray<FName>& Roster = FContent::Roster();
	int32 Matches = 0, Stuck = 0, Outside = 0, Unfinished = 0;
	int32 Wins[6] = {0, 0, 0, 0, 0, 0};
	for (int32 Level = 0; Level < 3; ++Level)
	{
		for (int32 A = 0; A < Roster.Num(); ++A)
		{
			for (int32 B = 0; B < Roster.Num(); ++B)
			{
				FFightSim Sim;
				Sim.bIntro = false;
				Sim.Setup(GetContent().Fighter(Roster[A]), GetContent().Fighter(Roster[B]));
				FCpuBrain C0(Sim, 0, Level, 100 + A * 7 + B);
				FCpuBrain C1(Sim, 1, Level, 200 + B * 7 + A);
				int32 SameState[2] = {0, 0};
				EState Last[2] = {EState::Idle, EState::Idle};
				bool bEnded = false;
				for (int32 T = 0; T < 60 * 60 * 6 && !bEnded; ++T)
				{
					FPadState Pads[2] = {C0.Think(), C1.Think()};
					Sim.Step(Pads);
					bEnded = Sim.Phase == EPhase::MatchEnd;
					for (int32 I = 0; I < 2; ++I)
					{
						const FFighter& F = Sim.Fighters[I];
						if (FMath::Abs(F.Pos.X) > FFightSim::StageHalf)
						{
							++Outside;
						}
						// Idle, crouching, blocking and dizzy may last; any other state over 20 s is stuck.
						const bool bRestful = F.IsActionable() || F.State == EState::Dizzy || F.State == EState::Win || F.State == EState::KO;
						SameState[I] = (F.State == Last[I] && !bRestful) ? SameState[I] + 1 : 0;
						Last[I] = F.State;
						if (SameState[I] == 20 * 60)
						{
							++Stuck;
							AddWarning(FString::Printf(TEXT("%s v %s level %d: fighter %d stuck in %s"), *Roster[A].ToString(), *Roster[B].ToString(), Level, I, StateName(F.State)));
						}
					}
				}
				++Matches;
				if (!bEnded)
				{
					++Unfinished;
				}
				else if (Sim.MatchWinner >= 0)
				{
					++Wins[Sim.MatchWinner == 0 ? A : B];
				}
			}
		}
	}
	TestEqual("no fighter stuck", Stuck, 0);
	TestEqual("never outside the stage", Outside, 0);
	TestEqual("every match ends", Unfinished, 0);
	FString Summary;
	for (int32 I = 0; I < Roster.Num(); ++I)
	{
		Summary += FString::Printf(TEXT("%s %d  "), *Roster[I].ToString(), Wins[I]);
	}
	UE_LOG(LogSlop, Display, TEXT("CPU all pairings: %d matches; match wins: %s"), Matches, *Summary);
	return true;
}

SLOP_TEST(FSlopCpuNoInstantReaction, "AI.CpuNeverReactsBeforeItsDelay")
bool FSlopCpuNoInstantReaction::RunTest(const FString&)
{
	// Two identical matches with identically seeded CPUs; in one the opponent starts a strike.
	// Until the CPU's reaction time has passed, its inputs must be the same in both: it cannot
	// answer something it has not had time to see.
	for (int32 Level = 0; Level < 3; ++Level)
	{
		const int32 React = FCpuBrain::Level(Level).React;
		int32 Diverged = 0;
		for (int32 Trial = 0; Trial < 30; ++Trial)
		{
			FSimHarness HA, HB;
			HA.Place(1.4 + 0.05 * Trial);
			HB.Place(1.4 + 0.05 * Trial);
			FCpuBrain CA(HA.Sim, 1, Level, 1000 + Trial);
			FCpuBrain CB(HB.Sim, 1, Level, 1000 + Trial);
			for (int32 I = 0; I < 60; ++I)
			{
				FPadState PA[2] = {FPadState(), CA.Think()};
				FPadState PB[2] = {FPadState(), CB.Think()};
				HA.Sim.Step(PA);
				HB.Sim.Step(PB);
			}
			// Out of the strike's reach, so the CPU's own state cannot change either.
			HA.Place(2.4 + 0.03 * Trial);
			HB.Place(2.4 + 0.03 * Trial);
			for (int32 I = 0; I < React; ++I)
			{
				FPadState PA[2] = {I == 0 ? FPadIntent::Pad(5, HA.F(0).Facing, BtnHK) : FPadState(), CA.Think()};
				FPadState PB[2] = {FPadState(), CB.Think()};
				if (!(PA[1] == PB[1]))
				{
					++Diverged;
					AddError(FString::Printf(TEXT("level %d trial %d: the CPU answered %d ticks after the strike began (reaction %d)"), Level, Trial, I, React));
					break;
				}
				HA.Sim.Step(PA);
				HB.Sim.Step(PB);
			}
		}
		TestEqual(FString::Printf(TEXT("level %d never answers within %d ticks"), Level, React), Diverged, 0);
	}
	return true;
}

SLOP_TEST(FSlopBossPatterns, "AI.BossFightsToAnEnd")
bool FSlopBossPatterns::RunTest(const FString&)
{
	for (int32 Level = 0; Level < 3; ++Level)
	{
		for (const FName& Id : FContent::Roster())
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.RoundsToWin = 1;
			Sim.Setup(GetContent().Fighter(Id), GetContent().Fighter(TEXT("slopbat")));
			FCpuBrain C0(Sim, 0, Level, 55);
			FBossBrain Boss(Sim, 1, Level);
			int32 T = 0;
			for (; T < 60 * 60 * 5 && Sim.Phase != EPhase::MatchEnd; ++T)
			{
				FPadState Pads[2] = {C0.Think(), Boss.Think()};
				Sim.Step(Pads);
			}
			TestTrue(FString::Printf(TEXT("%s against the Slopbat at level %d ends"), *Id.ToString(), Level), Sim.Phase == EPhase::MatchEnd);
		}
	}
	return true;
}

// Record and playback: what the player did with the dummy comes back the same, and still walks
// toward the opponent when the dummy plays it from the other side of the screen.
SLOP_TEST(FSlopDummyRecord, "AI.DummyRecordsAndPlaysBack")
bool FSlopDummyRecord::RunTest(const FString&)
{
	const FFighterDef* Ashka = GetContent().Fighter(TEXT("ashka"));
	const FFighterDef* Bram = GetContent().Fighter(TEXT("bram"));
	const int32 RecordMode = FTrainingDummy::Modes().IndexOfByKey(TEXT("Record"));
	const int32 PlayMode = FTrainingDummy::Modes().IndexOfByKey(TEXT("Playback"));
	TestTrue("the modes exist", RecordMode >= 0 && PlayMode >= 0);
	const auto Place = [](FSimHarness& H, bool bSwapped)
	{
		H.Place(2.6);
		if (bSwapped)
		{
			H.F(0).Pos.X = 1.3;
			H.F(1).Pos.X = -1.3;
			H.F(0).Facing = -1;
			H.F(1).Facing = 1;
		}
	};
	for (int32 Swap = 0; Swap < 2; ++Swap)
	{
		FSimHarness H(Ashka, Bram);
		FTrainingDummy D(H.Sim, 1);
		D.SetMode(RecordMode);
		Place(H, false);
		// Walk the dummy in (screen left, toward the player), then a high punch, then nothing.
		FName Recorded;
		for (int32 T = 0; T < 60; ++T)
		{
			FPadState P;
			P.Dirs = T < 24 ? PadLeft : 0;
			P.Pressed = P.Held = T == 24 ? BtnHP : 0;
			TestTrue("the player's pad drives the dummy while recording", D.TakesPlayerPad());
			FPadState Pads[2] = {FPadState(), D.Record(P)};
			H.Sim.Step(Pads);
			for (const FSimEvent& E : H.Sim.Events)
			{
				Recorded = E.Type == EEvent::Move && E.Who == 1 && Recorded.IsNone() ? E.Move : Recorded;
			}
		}
		TestFalse("a move was recorded", Recorded.IsNone());
		D.SetMode(PlayMode);
		Place(H, Swap == 1);
		const double Gap0 = FMath::Abs(H.F(1).Pos.X - H.F(0).Pos.X);
		FName Played;
		for (int32 T = 0; T < 60; ++T)
		{
			FPadState Pads[2] = {FPadState(), D.Think()};
			H.Sim.Step(Pads);
			if (T == 22)
			{
				TestTrue(FString::Printf(TEXT("%s: the dummy walks toward the player"), Swap ? TEXT("swapped") : TEXT("same side")),
					FMath::Abs(H.F(1).Pos.X - H.F(0).Pos.X) < Gap0 - 0.3);
			}
			for (const FSimEvent& E : H.Sim.Events)
			{
				Played = E.Type == EEvent::Move && E.Who == 1 && Played.IsNone() ? E.Move : Played;
			}
		}
		TestEqual(FString::Printf(TEXT("%s: the same move comes back"), Swap ? TEXT("swapped") : TEXT("same side")), Played, Recorded);
	}
	return true;
}

// The dummy's two practice modes: hit once, it blocks the next blow that follows a gap; held, it
// breaks the throw.
SLOP_TEST(FSlopDummyPractice, "AI.DummyBlocksAfterHitAndBreaksThrows")
bool FSlopDummyPractice::RunTest(const FString&)
{
	const FFighterDef* Ashka = GetContent().Fighter(TEXT("ashka"));
	const FFighterDef* Bram = GetContent().Fighter(TEXT("bram"));
	FSimHarness H(Ashka, Bram);
	FTrainingDummy D(H.Sim, 1);
	const auto Tick = [&H, &D](int32 Dirs, int32 Press)
	{
		FPadState Pads[2];
		Pads[0] = {Dirs, Press, Press};
		Pads[1] = D.Think();
		H.Sim.Step(Pads);
		H.Log.Append(H.Sim.Events);
	};
	D.SetMode(FTrainingDummy::Modes().IndexOfByKey(TEXT("Block After Hit")));
	H.Place(0.75);
	int32 At = H.Log.Num();
	Tick(0, HP);
	for (int32 T = 0; T < 30; ++T) Tick(0, 0);
	TestEqual("the first jab lands on a dummy standing still", H.Count(EEvent::Hit, At), 1);
	At = H.Log.Num();
	Tick(0, HP);
	for (int32 T = 0; T < 30; ++T) Tick(0, 0);
	TestEqual("a jab after a gap is blocked", H.Count(EEvent::Block, At), 1);
	TestEqual("and does not land", H.Count(EEvent::Hit, At), 0);
	D.SetMode(FTrainingDummy::Modes().IndexOfByKey(TEXT("Break Throws")));
	H.Place(0.7);
	At = H.Log.Num();
	Tick(PadRight, LP);
	for (int32 T = 0; T < 20; ++T) Tick(0, 0);
	TestEqual("a throw on it is broken", H.Count(EEvent::ThrowBreak, At), 1);
	return true;
}

// A grab beats a block, so against a player who only blocks the Bite was most of the damage the
// Slopbat did. It now waits between bites (and the bite can be broken, see Rounds).
SLOP_TEST(FSlopBossBites, "AI.BossWaitsBetweenBites")
bool FSlopBossBites::RunTest(const FString&)
{
	const int32 Wait[] = {300, 240, 180};
	for (int32 Level = 0; Level < 3; ++Level)
	{
		int32 Bites = 0, BiteDamage = 0, Damage = 0, MinGap = MAX_int32, Ticks = 0;
		for (const FName& Id : FContent::Roster())
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.RoundsToWin = 1;
			Sim.Setup(GetContent().Fighter(Id), GetContent().Fighter(TEXT("slopbat")));
			FTrainingDummy Blocker(Sim, 0);
			Blocker.SetMode(2);
			FBossBrain Boss(Sim, 1, Level);
			int32 LastAt = -1;
			for (int32 T = 0; T < 60 * 60 * 2 && Sim.Phase != EPhase::MatchEnd; ++T)
			{
				FPadState Pads[2] = {Blocker.Think(), Boss.Think()};
				Sim.Step(Pads);
				++Ticks;
				for (const FSimEvent& E : Sim.Events)
				{
					if (E.Type == EEvent::Move && E.Who == 1 && E.Move == TEXT("bite"))
					{
						++Bites;
						if (LastAt >= 0)
						{
							MinGap = FMath::Min(MinGap, E.Tick - LastAt);
						}
						LastAt = E.Tick;
					}
					if ((E.Type == EEvent::Hit || E.Type == EEvent::Block) && E.Who == 0)
					{
						Damage += E.Damage;
						BiteDamage += E.Move == TEXT("bite") ? E.Damage : 0;
					}
				}
			}
		}
		UE_LOG(LogSlop, Display, TEXT("Slopbat at level %d against a blocker: %d bites, %.0f damage a minute of which %.0f%% bites, shortest gap %d ticks"),
			Level, Bites, Damage * 3600.0 / FMath::Max(Ticks, 1), 100.0 * BiteDamage / FMath::Max(Damage, 1), MinGap);
		TestTrue(FString::Printf(TEXT("level %d still bites"), Level), Bites > 0);
		TestTrue(FString::Printf(TEXT("level %d waits at least %d ticks between bites"), Level, Wait[Level]), MinGap >= Wait[Level]);
	}
	return true;
}

// The Slopbat keeps its distance with tar and wind, but it should not keep firing: after each
// one it waits a while and never repeats the same one straight away, against a CPU that comes
// at it or a player who only blocks.
SLOP_TEST(FSlopBossRanged, "AI.BossDoesNotSpamRangedAttacks")
bool FSlopBossRanged::RunTest(const FString&)
{
	const int32 Wait[] = {150, 120, 95};
	for (int32 Opp = 0; Opp < 2; ++Opp)
	{
		for (int32 Level = 0; Level < 3; ++Level)
		{
			int32 Ranged = 0, Repeats = 0, MinGap = MAX_int32, Ticks = 0;
			for (const FName& Id : FContent::Roster())
			{
				FFightSim Sim;
				Sim.bIntro = false;
				Sim.RoundsToWin = 1;
				Sim.Setup(GetContent().Fighter(Id), GetContent().Fighter(TEXT("slopbat")));
				FCpuBrain Cpu(Sim, 0, Level, 55);
				FTrainingDummy Blocker(Sim, 0);
				Blocker.SetMode(2);
				FBossBrain Boss(Sim, 1, Level);
				FName Last;
				int32 LastAt = -1;
				for (int32 T = 0; T < 60 * 60 * 2 && Sim.Phase != EPhase::MatchEnd; ++T)
				{
					FPadState Pads[2] = {Opp == 0 ? Cpu.Think() : Blocker.Think(), Boss.Think()};
					Sim.Step(Pads);
					++Ticks;
					for (const FSimEvent& E : Sim.Events)
					{
						if (E.Type != EEvent::Move || E.Who != 1)
						{
							continue;
						}
						if (E.Move == TEXT("tar_spit") || E.Move == TEXT("wing_gust"))
						{
							++Ranged;
							Repeats += E.Move == Last;
							if (LastAt >= 0)
							{
								MinGap = FMath::Min(MinGap, E.Tick - LastAt);
							}
							LastAt = E.Tick;
						}
						Last = E.Move;
					}
				}
			}
			const TCHAR* Who = Opp == 0 ? TEXT("a CPU") : TEXT("a blocker");
			UE_LOG(LogSlop, Display, TEXT("Slopbat at level %d against %s: %.1f ranged attacks a minute, shortest gap %d ticks"), Level, Who, Ranged * 3600.0 / FMath::Max(Ticks, 1), MinGap);
			TestTrue(FString::Printf(TEXT("level %d against %s fires at all"), Level, Who), Ranged > 0);
			TestEqual(FString::Printf(TEXT("level %d against %s never fires the same one twice running"), Level, Who), Repeats, 0);
			TestTrue(FString::Printf(TEXT("level %d against %s waits at least %d ticks between them"), Level, Who, Wait[Level]), MinGap >= Wait[Level]);
		}
	}
	return true;
}

// Swept again and again, the CPU Ashka learns to lift the shin check into the next sweep, and is
// swept no more often for it than she would be guarding low alone.
SLOP_TEST(FSlopAshkaChecks, "AI.SweptAshkaChecksWithoutLosingHerGuard")
bool FSlopAshkaChecks::RunTest(const FString&)
{
	const FFighterDef* Ashka = GetContent().Fighter(TEXT("ashka"));
	const FFighterDef* Bram = GetContent().Fighter(TEXT("bram"));
	if (!TestNotNull("ashka", Ashka) || !TestNotNull("bram", Bram) || !TestTrue("ashka has ai", Ashka->AI.IsValid()))
	{
		return false;
	}
	FFighterDef NoChecks = *Ashka;
	NoChecks.AI = MakeShared<FJsonObject>(*Ashka->AI);
	NoChecks.AI->RemoveField(TEXT("checks"));
	int32 Swept[2] = {0, 0}, Sweeps[2] = {0, 0}, Caught[2] = {0, 0};
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Seed = 0; Seed < 4; ++Seed)
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(Bram, Pass == 0 ? Ashka : &NoChecks);
			FCpuBrain Cpu(Sim, 1, 2, 300 + Seed);
			for (int32 T = 0; T < 60 * 60; ++T)
			{
				// Bram sweeps whenever he can from close in, and otherwise walks in.
				const FFighter& B = Sim.Fighters[0];
				const double Gap = FMath::Abs(Sim.Fighters[1].Pos.X - B.Pos.X);
				FPadState Pads[2];
				Pads[0] = B.IsActionable() ? (Gap < 1.2 ? FPadIntent::Pad(4, B.Facing, BtnLK, 0) : FPadIntent::Pad(6, B.Facing)) : FPadState();
				Pads[1] = Cpu.Think();
				Sim.Step(Pads);
				for (const FSimEvent& E : Sim.Events)
				{
					Sweeps[Pass] += E.Type == EEvent::Move && E.Who == 0 && E.Move == TEXT("sweep");
					Swept[Pass] += E.Type == EEvent::Hit && E.Who == 1 && E.Move == TEXT("sweep");
					Caught[Pass] += E.Type == EEvent::Parry && E.Who == 1;
				}
			}
		}
	}
	UE_LOG(LogSlop, Display, TEXT("Ashka swept %d of %d with the check, %d of %d without; %d caught"), Swept[0], Sweeps[0], Swept[1], Sweeps[1], Caught[0]);
	TestTrue("she checks some sweeps", Caught[0] > 0);
	TestEqual("never without a check to use", Caught[1], 0);
	const double With = double(Swept[0]) / FMath::Max(1, Sweeps[0]);
	const double Without = double(Swept[1]) / FMath::Max(1, Sweeps[1]);
	TestTrue(FString::Printf(TEXT("swept no more often (%.2f of sweeps with the check, %.2f without)"), With, Without), With <= Without + 0.03);
	return true;
}

SLOP_TEST(FSlopStranger, "AI.StrangerMemoryOnlyMovesForward")
bool FSlopStranger::RunTest(const FString&)
{
	const FFighterDef* Dax = GetContent().Fighter(TEXT("daxion"));
	const FFighterDef* Ashka = GetContent().Fighter(TEXT("ashka"));
	int32 Saved = 0;
	TArray<int32> Stages;
	for (int32 Meeting = 0; Meeting < 4; ++Meeting)
	{
		FFightSim Sim;
		Sim.bIntro = false;
		Sim.RoundsToWin = 1;
		Sim.bFinishersEnabled = false;
		Sim.Setup(Ashka, Dax);
		FStrangerRules Rules(Sim, Saved, TEXT("Hold Block to bow out."), [&](int32 S) { Saved = S; });
		Sim.Hooks = &Rules;
		const FPadState Zero[2];
		while (Sim.Phase != EPhase::Fight)
		{
			Sim.Step(Zero);
		}
		// Walk in and punch until the encounter ends.
		int32 T = 0;
		for (; T < 60 * 40 && Sim.Phase != EPhase::MatchEnd; ++T)
		{
			FPadState Pads[2];
			const FFighter& P = Sim.Fighters[0];
			const double Gap = FMath::Abs(Sim.Fighters[1].Pos.X - P.Pos.X);
			Pads[0] = Gap > 0.9 ? FPadIntent::Pad(6, P.Facing) : FPadIntent::Pad(5, P.Facing, (T % 20 == 0) ? BtnHP : 0);
			Sim.Step(Pads);
			if (Sim.Fighters[1].Health < Sim.Fighters[1].MaxHealth())
			{
				AddError(TEXT("the stranger was hurt"));
				break;
			}
		}
		Stages.Add(Saved);
	}
	TestTrue("first meeting: warned, then struck down", Stages[0] >= 2);
	for (int32 I = 1; I < Stages.Num(); ++I)
	{
		TestTrue("memory never goes back", Stages[I] >= Stages[I - 1]);
	}
	TestEqual("in the end he recognises the player", Stages.Last(), 6);
	// Bowing out: hold Block.
	FFightSim Sim;
	Sim.bIntro = false;
	Sim.RoundsToWin = 1;
	Sim.bFinishersEnabled = false;
	Sim.Setup(Ashka, Dax);
	int32 S2 = 6;
	FStrangerRules Rules(Sim, S2, TEXT(""), [&](int32 S) { S2 = S; });
	Sim.Hooks = &Rules;
	for (int32 T = 0; T < 600 && Sim.Phase != EPhase::MatchEnd; ++T)
	{
		FPadState Pads[2];
		Pads[0] = FPadIntent::Pad(5, Sim.Fighters[0].Facing, 0, BtnBL);
		Sim.Step(Pads);
	}
	TestEqual("holding Block bows out", (int32)Sim.Phase, (int32)EPhase::MatchEnd);
	TestEqual("with his line", Rules.LastLine(), FString(TEXT("Go.")));
	return true;
}

}

#undef SLOP_TEST

#endif
