#pragma once

#include "Combat/SlopSim.h"
#include "Math/RandomStream.h"

namespace Slop
{
	// Turns intentions written relative to a fighter (forward, back, motions, buttons) into the
	// screen-space pad states FFightSim::Step reads, one tick at a time. CPU brains, the training
	// dummy, the tutorial and the stranger drive their fighters through the same input path as a
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

		FFighter& Me() const { return Sim.Fighters[Index]; }
		FFighter& Opp() const { return Sim.Fighters[1 - Index]; }

	private:
		const FSeen& Perceive();
		void Plan(FFighter& M, const FSeen& O, double Dist);
		void PlanAntiAir(FFighter& M, const FSeen& Seen);
		void NoteHit(FFighter& M);
		// A parry taken as a read into the attacker's next blow (see the definition); true when
		// one was started.
		bool ParryRead(const FFighter& M, const FSeen& Seen);
		void StartCombo(bool bPunish);
		FPadState Finish();
		double Reach(FName MoveId);
		FString AttackFor(double Dist);
		TArray<FName> MovesTagged(FName Tag) const;
		FString Notation(FName MoveId) const;
		int32 CountHitBy(FName Kind) const;
		const TArray<TSharedPtr<FJsonValue>>* AiArr(const TCHAR* Key) const;

		FFightSim& Sim;
		int32 Index;
		FCpuLevel L;
		FRandomStream Rng;
		FPadIntent Intent;
		TSharedPtr<FJsonObject> Ai;
		TArray<FSeen> SeenHistory;
		int32 ThinkT = 0;
		int32 BlockT = 0;
		bool bBlockLow = false;
		int32 Walk = 5;
		int32 WalkT = 0;
		int32 RunT = 0;
		bool bWasBlocking = false;
		TMap<FName, double> ReachCache;
		TArray<FName> Recent;
		// Tick at which to start an anti-air timed to meet a jump seen coming in (-1: none planned).
		int32 AaAt = -1;
		FName AaMove;
		bool bAaReady = true;
		// How the CPU was hit lately ("low", "jump" or "other", newest first), so it adapts.
		TArray<FName> HitBy;
		int32 LastDamage = 0;
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
		FTrainingDummy(FFightSim& InSim, int32 InIndex);
		void NextMode() { SetMode((Mode + 1) % Modes().Num()); }
		void SetMode(int32 M);
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

	// The optional encounter with the stranger in white. He cannot be hurt and does not fight; he
	// answers. His memory (`Stage`) only moves forward:
	//   0 never met  1 met  2 warned  3 struck down  4 asked  5 finished properly  6 recognised
	// Holding Block bows out; standing idle long enough, he sends the player on.
	class FStrangerRules : public ISimHooks, public IBrain
	{
	public:
		static constexpr int32 BowTicks = 50;
		static constexpr int32 IdleTicks = 25 * 60;

		// `OnStage` is called with every new memory stage so the caller can save it at once.
		FStrangerRules(FFightSim& InSim, int32 InStage, const FString& BowHint, TFunction<void(int32)> InOnStage);
		virtual bool IgnoreContact(FFightSim& S, const FContact& C) override;
		virtual void OnTick(FFightSim& S) override;
		virtual FPadState Think() override { return FPadState(); }
		int32 GetStage() const { return Stage; }
		const FString& LastLine() const { return LastLineText; }
		bool Ended() const { return bEnded; }

	private:
		void Say(int32 At, const FString& Text, bool bHint = false) { Lines.Add({At, Text, bHint}); }
		void SetStage(int32 S);
		void Line(const FString& Text, bool bHint = false);
		void Evade(const FString& Text);
		void Catch(int32 StunTicks);
		void Strike(const FString& Text);
		void Crack();
		void End(const FString& Text);

		struct FLine { int32 At; FString Text; bool bHint; };
		FFightSim& Sim;
		int32 Stage = 0;
		TFunction<void(int32)> OnStage;
		TArray<FLine> Lines;
		int32 T = 0;
		int32 Bow = 0;
		int32 IdleT = 0;
		bool bEnded = false;
		bool bBusy = false;
		FString LastLineText;
	};
}
