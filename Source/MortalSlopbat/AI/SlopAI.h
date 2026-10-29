#pragma once

#include "Combat/SlopSim.h"
#include "Math/RandomStream.h"

namespace Slop
{
	// Turns intentions written relative to a fighter (forward, back, motions, buttons) into the
	// screen-space pad states FFightSim::Step reads, one tick at a time. CPU brains, the training
	// dummy and the tutorial drive their fighters through the same input path as a
	// player's keys.
	class FPadIntent
	{
	public:
		static int32 ToScreen(int32 RelDir, int32 Facing);
		static FPadState Pad(int32 RelDir, int32 Facing, int32 Press = 0, int32 Held = 0);

		bool Busy() const { return Queue.Num() > 0; }
		void Clear() { Queue.Reset(); }
		// Queues a command in arcade notation: motion directions one tick each (rolling through the
		// diagonal between down and forward or back, as a player would), then the buttons.
		void Command(const FString& Notation, int32 HoldTicks = 0);
		// Holds a relative direction (and buttons) for `Ticks` ticks.
		void Hold(int32 RelDir, int32 Ticks, int32 Buttons = 0);
		void Press(int32 RelDir, int32 Buttons) { Queue.Add({RelDir, Buttons, 0}); }
		FPadState Next(int32 Facing, int32 IdleDir = 5, int32 IdleHeld = 0);
		int32 HeldButtons = 0;

	private:
		struct FStep { int32 Dir; int32 Press; int32 Held; };
		TArray<FStep> Queue;
		int32 Head = 0;
	};

	// Anything that produces one fighter's pad state each tick.
	class IBrain
	{
	public:
		virtual ~IBrain() = default;
		virtual FPadState Think() = 0;
		// Training's reset put the fighters back: forget reads of what came before.
		virtual void Forget() {}
	};

	// Difficulty parameters of a CPU opponent.
	struct FCpuLevel
	{
		int32 React;      // ticks of delay between something happening and the CPU seeing it
		double Block;     // chance of guarding an attack it has seen coming
		double AntiAir;
		double Punish;
		double Combo;     // chance of completing a planned string
		double Aggr;
		double Tech;      // chance of breaking a throw
		int32 Think;      // ticks between plans
		double Mistake;   // chance a plan is a wasted walk
		double Finish;    // chance of entering the finisher rather than settling for a blow
	};

	// A CPU opponent. It sees the other fighter only through a delayed copy of the match (its
	// reaction time), so it never answers anything before it has been visible for that long. It
	// plays its fighter's real moves through FPadIntent, under the same rules as a player.
	class FCpuBrain : public IBrain
	{
	private:
		// What the CPU has seen of the opponent.
		struct FSeen
		{
			EState State = EState::Idle;
			const FMoveDef* Move = nullptr;
			int32 MF = 0;
			FVector2D Pos = FVector2D::ZeroVector;
			FVector2D Vel = FVector2D::ZeroVector;
			bool bAir = false;
			bool bCrouch = false;
			bool bActionable = true;
			EStun StunKind = EStun::None;
			int32 Stun = 0;
			int32 JugglePoints = 0;
			int32 Tick = 0;
		};

	public:
		static const FCpuLevel& Level(int32 Difficulty);

		FCpuBrain(FFightSim& InSim, int32 InIndex, int32 Difficulty, int32 Seed = 0);
		virtual FPadState Think() override;
		virtual void Forget() override;

		FFighter& Me() const { return Sim.Fighters[Index]; }
		FFighter& Opp() const { return Sim.Fighters[1 - Index]; }

		// How a Qhi fighter's CPU (the Grandmaster's) used its meter, counted in development builds
		// only (left empty in Shipping): logged by the AI tests and, with -SlopAiLog, at a match's end.
		struct FQhiTelemetry
		{
			int32 Ticks = 0;
			double Spent = 0.0;
			double Gained = 0.0;
			// Shapes taken, by source ("own": Own Face during the fight), and how many of the changes
			// were taken inside a combo.
			TMap<FName, int32> Forms;
			int32 ComboShifts = 0;
			int32 Spents = 0;
			int32 Breaks = 0;
			// Instant kills, by move: ticks it could have begun (for a counter: ticks its Qhi was in
			// hand), times begun, and kills.
			TMap<FName, int32> KillChances;
			TMap<FName, int32> KillTries;
			TMap<FName, int32> Kills;
			// Time in each form ("own", "borrowed", "spent"), and the damage dealt and taken in it.
			TMap<FName, int32> FormTicks;
			// His combos: how many, by length ("2", "3", ... "6+").
			TMap<FName, int32> Combos;
			TMap<FName, int32> Dealt;
			TMap<FName, int32> Taken;
			// Qhi left in hand as rounds ended, and rounds lost, by the form he was in.
			double Unused = 0.0;
			int32 Rounds = 0;
			TMap<FName, int32> Losses;
			void Add(const FQhiTelemetry& O);
			FString Summary() const;
		};
		const FQhiTelemetry& Telemetry() const { return Tele; }

	private:
		const FSeen& Perceive();
		void Plan(FFighter& M, const FSeen& O, double Dist);
		void PlanAntiAir(FFighter& M, const FSeen& Seen);
		void NoteHit(FFighter& M);
		// A parry taken as a read into the attacker's next blow (see the definition); true when
		// one was started.
		bool ParryRead(const FFighter& M, const FSeen& Seen);
		// A blocked blow noted as it lands, and the punish it leaves when the guard ends (see the
		// definitions); PunishRead is true when one was started.
		void NoteBlocked(const FFighter& M);
		bool PunishRead(const FFighter& M);
		// The heaviest blow that lands within Gap ticks and reaches Dist, of those this fighter can
		// start now (none when nothing fits).
		FName PunisherFor(int32 Gap, double Dist);
		// Ticks from now until a move started now first hits: its inputs and a tick, then its startup.
		int32 TicksToHit(FName MoveId) const;
		// One of the fighter's AI combos (bLongest: its longest, to carry a combo on as far as it goes).
		void StartCombo(bool bPunish, bool bLongest = false);
		// The fighter took another shape (the Grandmaster): its moves and AI notes are that shape's.
		void SyncForm();
		// The Grandmaster's Qhi: when to borrow the opponent's shape, when to give it back before
		// the meter runs dry, and how far to back off while spent (fighter AI "qhi", by
		// difficulty). True when a command was started.
		bool QhiPlan(const FFighter& M, const FSeen& Seen, double Dist);
		// Whether a move leaves the Qhi the CPU means to keep (always, for moves that cost none).
		bool Affordable(const FMoveDef& Mv) const;
		double QhiSetting(const TCHAR* Key, double Default) const;
		// The Qhi kept back: the difficulty's reserve ("keep"), more when the CPU is in danger, less
		// when the opponent is nearly beaten.
		double Reserve() const;
		// Which shape move to take now (none: stay as he is): the opponent's own, enhanced, with the
		// meter high; the Fragment's with a full meter at the best levels; a remembered one chosen
		// for the matchup inside a combo (or where the copy is refused).
		FName PickForm(const FFighter& M, bool bInCombo) const;
		// A blow of his has just landed: change shape inside the combo (fighter AI qhi
		// "combo_shift", a chance by difficulty). True when a command was started.
		bool ComboShift(FFighter& M);
		// The instant kills: Heart Stop when a combo allows it, Find the Crack against an opponent
		// standing guard within reach. True when a command was started.
		bool KillPlan(FFighter& M, const FSeen& Seen, double Dist);
		void Observe(const FFighter& M);
		FPadState Finish();
		double Reach(FName MoveId);
		FString AttackFor(double Dist);
		TArray<FName> MovesTagged(FName Tag) const;
		FString Notation(FName MoveId) const;
		int32 CountHitBy(FName Kind) const;
		const TArray<TSharedPtr<FJsonValue>>* AiArr(const TCHAR* Key) const;

		FFightSim& Sim;
		int32 Index;
		int32 Diff = 1;
		FCpuLevel L;
		FRandomStream Rng;
		FPadIntent Intent;
		TSharedPtr<FJsonObject> Ai;
		const FFighterDef* AiDef = nullptr;
		TArray<FSeen> SeenHistory;
		int32 ThinkT = 0;
		int32 BlockT = 0;
		bool bBlockLow = false;
		int32 Walk = 5;
		int32 WalkT = 0;
		int32 RunT = 0;
		// The blow last blocked (null once judged): the tick its attacker is free again, and how
		// far apart the push leaves the two.
		const FMoveDef* Blocked = nullptr;
		// The start tick (as seen) of the last whiffed move judged for a punish, and the tick before
		// which none is (a blocked move already judged is not judged again as a whiff).
		int32 WhiffJudged = -1;
		int32 NoWhiffBefore = 0;
		int32 BlockedFreeAt = 0;
		double BlockedDist = 0.0;
		TMap<FName, double> ReachCache;
		TArray<FName> Recent;
		// Tick at which to start an anti-air timed to meet a jump seen coming in (-1: none planned).
		int32 AaAt = -1;
		FName AaMove;
		bool bAaReady = true;
		// How the CPU was hit lately ("low", "jump" or "other", newest first), so it adapts.
		TArray<FName> HitBy;
		int32 LastDamage = 0;
		// The Grandmaster: a change of shape inside a combo wants the new shape's blows at once; the
		// blow a change was tried in (one try a blow); telemetry.
		bool bFollowUp = false;
		int32 ShiftTriedAt = -1;
		FQhiTelemetry Tele;
		double LastQhi = -1.0;
		int32 LastDealt = 0;
		int32 LastTaken = 0;
		double FightQhi = 0.0;
		EForm LastForm = EForm::Own;
		// The start tick (as seen) of the last unblockable blow judged, so it is answered once.
		int32 UnblockJudged = -1;
	};

	// The Slopbat's mind: readable patterns rather than reflexes. Claws up close and now and then a
	// bite, tar and wind from afar (each followed by a wait, never the same twice running), the Dive
	// on a cooldown, and below half health the Storm Shroud.
	class FBossBrain : public IBrain
	{
	public:
		FBossBrain(FFightSim& InSim, int32 InIndex, int32 Difficulty);
		virtual FPadState Think() override;

	private:
		struct FLevel { int32 React, DiveCd, StormCd; double Aa; int32 Think; double Block; int32 RangedCd, BiteCd; };
		void Do(const TCHAR* Command);
		FFightSim& Sim;
		int32 Index;
		FLevel L;
		FRandomStream Rng;
		FPadIntent Intent;
		TArray<bool> SeenAir;
		int32 ThinkT = 60;
		int32 DiveCd = 420;
		int32 StormCd = 240;
		int32 RangedCd = 0;
		int32 BiteCd = 0;
		FString LastCommand;
		int32 Walk = 5;
		int32 WalkT = 0;
	};

	// The training partner: stands, crouches, blocks (all, random height, low only), jumps in
	// place, fights back as the CPU at Warrior, or plays back what the player recorded on it; it can
	// also stand until hit and then block (so a gap in a combo shows), or break every throw.
	// Recording hands the player's own pad to the dummy for up to ten seconds (the player's fighter
	// stands meanwhile); directions are kept relative to the way the dummy faced, so a recording
	// still walks in and out the right way after the fighters change sides.
	class FTrainingDummy : public IBrain
	{
	public:
		static const TArray<FString>& Modes();
		// Level is the difficulty its CPU mode plays at.
		FTrainingDummy(FFightSim& InSim, int32 InIndex, int32 InLevel = 1);
		void NextMode() { SetMode((Mode + 1) % Modes().Num()); }
		void SetMode(int32 M);
		// A new difficulty for the CPU mode (Settings changed mid-session).
		void SetLevel(int32 InLevel);
		virtual void Forget() override;
		int32 GetMode() const { return Mode; }
		FString ModeName() const;
		virtual FPadState Think() override;
		// While recording, the player's pad drives the dummy: Record takes it and returns what the
		// dummy plays this tick.
		bool TakesPlayerPad() const;
		FPadState Record(const FPadState& Player);
		static constexpr int32 MaxTape = 600;
		static constexpr int32 LoopGap = 40;

	private:
		FFightSim& Sim;
		int32 Index;
		int32 Level = 1;
		int32 Mode = 0;
		TUniquePtr<FCpuBrain> Cpu;
		FRandomStream Rng;
		bool bLow = false;
		int32 SwitchT = 0;
		TArray<FPadState> Tape;
		int32 PlayAt = 0;
		int32 GuardT = 0;
		FPadState BlockAll() const;
	};

}
