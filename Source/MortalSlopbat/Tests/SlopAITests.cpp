#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/SlopAI.h"
#include "Tests/SlopSimHarness.h"

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
	AddInfo(FString::Printf(TEXT("%d matches; match wins: %s"), Matches, *Summary));
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

#endif
