#pragma once

#include "Combat/SlopData.h"

namespace Slop
{
	enum class EMode : uint8 { Arcade, Versus, Cpu, Training, Tutorial, Watch, Survival, Trials };
	const TCHAR* ModeName(EMode M);

	// Everything a match needs to start.
	struct FMatchConfig
	{
		EMode Mode = EMode::Cpu;
		FName P1 = TEXT("ashka");
		FName P2 = TEXT("gorm");
		FName Stage = TEXT("rendering_yards");
		int32 Difficulty = 1;
		int32 RoundsToWin = 2;
		// Seconds on the clock each round (0: no clock).
		int32 RoundSeconds = 90;
		// The Reflection: a slop-dark mirror match in the Master tower.
		bool bMirrorDark = false;
		// Health overrides: the Slopbat's by tower; survival's carried health (0 for full).
		int32 P2Health = 0;
		int32 P1Health = 0;
		bool bCostume[2] = {false, false};
		bool bFromArcade = false;
		int32 TrialIndex = 0;
		// A CPU tier beyond the difficulty for the CPU's fighter (the Grandmaster's challenge: 3,
		// FCpuBrain::Level); -1 plays at the difficulty.
		int32 CpuTier = -1;
	};

	// The result a match reports when it ends.
	struct FMatchResult
	{
		int32 Winner = -1;
		int32 Wins[2] = {0, 0};
		FName P1, P2;
		FName Finisher;
		bool bFlawless = false;
		EMode Mode = EMode::Cpu;
		int32 Health[2] = {0, 0};
		int32 TimeLeft = 0;
	};

	struct FRung
	{
		FName Opponent;
		FName Stage;
		// "fight", "rival", "mirror" or "boss".
		FName Kind;
	};

	// Which match comes next, and the arcade and survival runs in progress.
	class FGameFlow
	{
	public:
		static const TArray<FName>& Arenas();
		static FName BossArena() { return TEXT("roost"); }
		static constexpr int32 BossHealth[3] = {1400, 1650, 1900};
		static constexpr int32 SurvivalBossEvery = 5;
		static constexpr double SurvivalRefill = 0.3;
		static constexpr double SurvivalFlawlessRefill = 0.15;
		static constexpr int32 SurvivalWavesPerLevel = 3;
		struct FTower { const TCHAR* Name; int32 Fights; bool bMirror; };
		static const FTower& Tower(int32 I);
		static FText DifficultyName(int32 I);

		FMatchConfig Config;

		bool IsCpu(int32 Side) const;
		bool TwoPlayers() const { return Config.Mode == EMode::Versus; }

		// Arcade: ordinary opponents, the rival, the Reflection (Master only) and the boss.
		TArray<FRung> PlanArcade(FName Fighter, int32 TowerIndex, int32 Seed) const;
		void StartArcade(FName Fighter, int32 TowerIndex, const TArray<FRung>& Rungs);
		bool InArcade() const { return Arcade.Rungs.Num() > 0; }
		const FRung* CurrentRung() const { return InArcade() ? &Arcade.Rungs[Arcade.Rung] : nullptr; }
		int32 RungIndex() const { return Arcade.Rung; }
		const TArray<FRung>& Rungs() const { return Arcade.Rungs; }
		int32 ArcadeTower() const { return Arcade.Tower; }
		FName ArcadeFighter() const { return Arcade.Fighter; }
		int32 ArcadeScore() const { return Arcade.Score; }
		int32 ArcadeContinues() const { return Arcade.Continues; }
		double ArcadeSeconds() const { return Arcade.Seconds; }
		// Points for a won arcade fight (health and time left, flawless, finisher).
		static int32 FightScore(const FMatchResult& R);
		int32 AddArcadeScore(const FMatchResult& R);
		// After a won arcade match: advances the ladder; false when the run is complete.
		bool ArcadeAdvance(bool bFlawless, double NowSeconds);
		// The Grandmaster's secret challenge: a run that beat the Slopbat without a continue meets him
		// on the still water of the Drowned Chapel, at Master, as one more rung ("secret").
		static FName SecretFighter() { return TEXT("grandmaster"); }
		bool SecretEarned() const;
		void AddSecretRung();
		// Development: a run already at the Slopbat, beaten without a continue (to reach the secret
		// challenge without climbing a tower).
		void SkipToBossCleared(FName Fighter, int32 TowerIndex);
		bool OnSecretRung() const { const FRung* R = CurrentRung(); return R && R->Kind == TEXT("secret"); }
		void ArcadeRetry();
		void EndArcade() { Arcade = FArcade(); }

		void StartTrials(FName Fighter);
		// Who a fighter's trials are performed against: its rival, else the partner trials.json
		// names for it, else Gorm (Bram for Gorm).
		static FName TrialPartner(FName Fighter);
		void StartSurvival(FName Fighter, int32 Seed);
		static bool SurvivalBossWave(int32 Wave) { return (Wave + 1) % SurvivalBossEvery == 0; }
		static int32 SurvivalDifficulty(int32 Wave) { return FMath::Clamp(Wave / SurvivalWavesPerLevel, 0, 2); }
		void SurvivalAdvance(int32 HealthLeft, bool bFlawless);
		bool InSurvival() const { return !Survival.Fighter.IsNone(); }
		int32 SurvivalWaves() const { return Survival.Wave; }
		FName SurvivalFighter() const { return Survival.Fighter; }
		void EndSurvival() { Survival = FSurvival(); }

	private:
		void ConfigureRung();
		void ConfigureWave();
		struct FArcade
		{
			FName Fighter;
			int32 Tower = 1;
			TArray<FRung> Rungs;
			int32 Rung = 0;
			int32 Continues = 0;
			double StartedAt = 0;
			double Seconds = 0;
			int32 Flawless = 0;
			int32 Score = 0;
		} Arcade;
		struct FSurvival
		{
			FName Fighter;
			int32 Wave = 0;
			int32 Health = 0;
			FName Last;
			int32 Seed = 0;
		} Survival;
		double Now = 0;
	public:
		// The clock the arcade's time is measured with (seconds); the owner sets it.
		void SetNow(double Seconds) { Now = Seconds; }
	};
}
