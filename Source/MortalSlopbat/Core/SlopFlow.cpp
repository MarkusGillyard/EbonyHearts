#include "Core/SlopFlow.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "SlopFlow"

namespace Slop
{
	const TCHAR* ModeName(EMode M)
	{
		switch (M)
		{
		case EMode::Arcade: return TEXT("arcade");
		case EMode::Versus: return TEXT("versus");
		case EMode::Cpu: return TEXT("cpu");
		case EMode::Training: return TEXT("training");
		case EMode::Tutorial: return TEXT("tutorial");
		case EMode::Stranger: return TEXT("stranger");
		case EMode::Watch: return TEXT("watch");
		case EMode::Survival: return TEXT("survival");
		case EMode::Trials: return TEXT("trials");
		}
		return TEXT("?");
	}

	const TArray<FName>& FGameFlow::Arenas()
	{
		static const TArray<FName> A = {TEXT("rendering_yards"), TEXT("drowned_chapel"), TEXT("stilt_market")};
		return A;
	}

	const FGameFlow::FTower& FGameFlow::Tower(int32 I)
	{
		static const FTower T[] = {{TEXT("Novice"), 3, false}, {TEXT("Warrior"), 4, false}, {TEXT("Master"), 4, true}};
		return T[FMath::Clamp(I, 0, 2)];
	}

	FText FGameFlow::DifficultyName(int32 I)
	{
		switch (FMath::Clamp(I, 0, 2))
		{
		case 0: return LOCTEXT("Novice", "Novice");
		case 1: return LOCTEXT("Warrior", "Warrior");
		default: return LOCTEXT("Master", "Master");
		}
	}

	bool FGameFlow::IsCpu(int32 Side) const
	{
		switch (Config.Mode)
		{
		case EMode::Versus: return false;
		case EMode::Watch: return true;
		default: return Side == 1;
		}
	}

	static FName StoryOf(FName Fighter, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject> F = Json::Obj(Json::Obj(GetContent().Story, TEXT("fighters")), *Fighter.ToString());
		return FName(*Json::Str(F, Field));
	}

	TArray<FRung> FGameFlow::PlanArcade(FName Fighter, int32 TowerIndex, int32 Seed) const
	{
		const FTower& T = Tower(TowerIndex);
		const FName Rival = StoryOf(Fighter, TEXT("rival"));
		TArray<FName> Others;
		for (const FName& Id : FContent::Roster())
		{
			if (Id != Fighter && Id != Rival && GetContent().Fighter(Id))
			{
				Others.Add(Id);
			}
		}
		FRandomStream Rng(Seed);
		for (int32 I = Others.Num() - 1; I > 0; --I)
		{
			Others.Swap(I, Rng.RandRange(0, I));
		}
		TArray<FRung> Rungs;
		for (int32 I = 0; I < FMath::Min(T.Fights, Others.Num()); ++I)
		{
			Rungs.Add({Others[I], Arenas()[I % Arenas().Num()], TEXT("fight")});
		}
		if (!Rival.IsNone() && GetContent().Fighter(Rival))
		{
			const FName Home = StoryOf(Rival, TEXT("home"));
			Rungs.Add({Rival, Home.IsNone() ? Arenas()[0] : Home, TEXT("rival")});
		}
		if (T.bMirror)
		{
			Rungs.Add({Fighter, TEXT("drowned_chapel"), TEXT("mirror")});
		}
		if (GetContent().Fighter(TEXT("slopbat")))
		{
			Rungs.Add({TEXT("slopbat"), BossArena(), TEXT("boss")});
		}
		return Rungs;
	}

	void FGameFlow::StartArcade(FName Fighter, int32 TowerIndex, const TArray<FRung>& Rungs)
	{
		Arcade = FArcade();
		Arcade.Fighter = Fighter;
		Arcade.Tower = TowerIndex;
		Arcade.Rungs = Rungs;
		Arcade.StartedAt = Now;
		Survival = FSurvival();
		bStrangerPending = false;
		// The player's colours outlive the fresh configuration (here and below: a stranger met in
		// Arcade, Survival's Try Again had dropped them).
		const bool bCostume = Config.bCostume[0];
		Config = FMatchConfig();
		Config.bCostume[0] = bCostume;
		Config.Mode = EMode::Arcade;
		Config.P1 = Fighter;
		Config.Difficulty = TowerIndex;
		ConfigureRung();
	}

	void FGameFlow::ConfigureRung()
	{
		const FRung& R = Arcade.Rungs[Arcade.Rung];
		Config.P2 = R.Opponent;
		Config.Stage = R.Stage;
		Config.bMirrorDark = R.Kind == TEXT("mirror");
		// The boss fight is a single long round, as the last stand should be.
		const bool bBoss = R.Kind == TEXT("boss");
		Config.RoundsToWin = bBoss ? 1 : 2;
		Config.P2Health = bBoss ? BossHealth[FMath::Clamp(Arcade.Tower, 0, 2)] : 0;
		Config.P1Health = 0;
		Config.Mode = EMode::Arcade;
		Config.Difficulty = Arcade.Tower;
	}

	int32 FGameFlow::FightScore(const FMatchResult& R)
	{
		int32 S = 10000;
		S += 20 * FMath::Max(0, R.Health[0]);
		if (R.bFlawless)
		{
			S += 25000;
		}
		if (!R.Finisher.IsNone())
		{
			S += R.Finisher.ToString().StartsWith(TEXT("stage_")) ? 10000 : 15000;
		}
		S += 100 * FMath::Max(0, R.TimeLeft);
		return S;
	}

	int32 FGameFlow::AddArcadeScore(const FMatchResult& R)
	{
		const int32 Pts = FightScore(R);
		Arcade.Score += Pts;
		return Pts;
	}

	bool FGameFlow::ArcadeAdvance(bool bFlawless, double NowSeconds)
	{
		Now = NowSeconds;
		if (bFlawless)
		{
			++Arcade.Flawless;
		}
		++Arcade.Rung;
		if (Arcade.Rung >= Arcade.Rungs.Num())
		{
			Arcade.Rung = Arcade.Rungs.Num() - 1;
			Arcade.Seconds = Now - Arcade.StartedAt;
			bStrangerPending = false;
			return false;
		}
		const FRung* R = CurrentRung();
		bStrangerPending = bFlawless && R && R->Kind != TEXT("boss") && GetContent().Fighter(TEXT("daxion")) != nullptr;
		ConfigureRung();
		return true;
	}

	void FGameFlow::ArcadeRetry()
	{
		++Arcade.Continues;
		ConfigureRung();
	}

	void FGameFlow::StartTrials(FName Fighter)
	{
		FName Rival = StoryOf(Fighter, TEXT("rival"));
		if (!GetContent().Fighter(Rival))
		{
			Rival = Fighter != TEXT("gorm") ? FName(TEXT("gorm")) : FName(TEXT("bram"));
		}
		Arcade = FArcade();
		Survival = FSurvival();
		Config = FMatchConfig();
		Config.Mode = EMode::Trials;
		Config.P1 = Fighter;
		Config.P2 = Rival;
		const FName Home = StoryOf(Rival, TEXT("home"));
		Config.Stage = Home.IsNone() ? Arenas()[0] : Home;
		Config.RoundsToWin = 1;
	}

	void FGameFlow::StartSurvival(FName Fighter, int32 Seed)
	{
		Survival = FSurvival();
		Survival.Fighter = Fighter;
		Survival.Seed = Seed;
		Arcade = FArcade();
		bStrangerPending = false;
		const bool bCostume = Config.bCostume[0];
		Config = FMatchConfig();
		Config.bCostume[0] = bCostume;
		Config.Mode = EMode::Survival;
		Config.P1 = Fighter;
		ConfigureWave();
	}

	// The next survival fight: the opponent (never the same twice running, the Slopbat on its
	// waves), the stage, the difficulty and the health carried in.
	void FGameFlow::ConfigureWave()
	{
		const int32 Wave = Survival.Wave;
		FRandomStream Rng(Survival.Seed * 131 + Wave);
		const int32 Diff = SurvivalDifficulty(Wave);
		const bool bBoss = SurvivalBossWave(Wave) && GetContent().Fighter(TEXT("slopbat"));
		if (bBoss)
		{
			Config.P2 = TEXT("slopbat");
			Config.Stage = BossArena();
			Config.P2Health = BossHealth[Diff] - 200;
		}
		else
		{
			TArray<FName> Pool;
			for (const FName& Id : FContent::Roster())
			{
				if (Id != Survival.Last)
				{
					Pool.Add(Id);
				}
			}
			Config.P2 = Pool[Rng.RandRange(0, Pool.Num() - 1)];
			Config.Stage = Arenas()[Rng.RandRange(0, Arenas().Num() - 1)];
			Config.P2Health = 0;
		}
		Config.P1Health = Survival.Health;
		Config.Difficulty = Diff;
		Config.RoundsToWin = 1;
		Config.bMirrorDark = false;
		Config.Mode = EMode::Survival;
	}

	void FGameFlow::SurvivalAdvance(int32 HealthLeft, bool bFlawless)
	{
		const FFighterDef* F = GetContent().Fighter(Survival.Fighter);
		const int32 Full = F ? F->Health : 1000;
		const double Back = SurvivalRefill + (bFlawless ? SurvivalFlawlessRefill : 0.0);
		Survival.Health = FMath::Min(Full, FMath::Max(1, HealthLeft) + (int32)(Full * Back));
		Survival.Last = Config.P2;
		++Survival.Wave;
		ConfigureWave();
	}

	void FGameFlow::StartStranger(bool bFromArcade)
	{
		const FName P1 = Config.P1;
		const bool bCostume = Config.bCostume[0];
		Config = FMatchConfig();
		Config.bCostume[0] = bCostume;
		Config.Mode = EMode::Stranger;
		Config.P1 = P1.IsNone() ? FName(TEXT("ashka")) : P1;
		Config.P2 = TEXT("daxion");
		Config.Stage = TEXT("stilt_market");
		Config.RoundsToWin = 1;
		Config.bFromArcade = bFromArcade;
	}
}

#undef LOCTEXT_NAMESPACE
