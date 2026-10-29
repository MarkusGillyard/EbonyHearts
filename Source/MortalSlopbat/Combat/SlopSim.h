#pragma once

#include "Combat/SlopData.h"

namespace Slop
{
	class FFightSim;

	// A thrown or conjured attack that travels on its own.
	struct FProjectile
	{
		int32 Owner = 0;
		const FMoveDef* Move = nullptr;
		FName Kind;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Vel = FVector2D::ZeroVector;
		double Gravity = 0.0;
		double Decel = 0.0;
		double MinSpeed = 0.0;
		FVector2D Size = FVector2D(0.3, 0.3);
		int32 Facing = 1;
		int32 Life = 90;
		int32 Age = 0;
		int32 HitsLeft = 1;
		int32 Strength = 1;
		bool bDead = false;
		// Stable per match: presentation keys its visuals on it.
		int32 Serial = 0;

		const FProjectileDef& Def() const { return Move->Projectile; }
		FRect Rect() const { return FRect(Pos.X - Size.X * 0.5, Pos.Y - Size.Y * 0.5, Size.X, Size.Y); }
		void Step(const FVector2D& Home);
	};

	// The Grandmaster's shapes: his own (Qhi), spent (Qhi exhausted: he boxes), or
	// borrowed from the opponent. Every other fighter stays Own.
	enum class EForm : uint8 { Own, Spent, Borrowed };
	const TCHAR* FormName(EForm F);

	// One fighter's state in a match: position, the state machine, health, run meter and the
	// move in progress. Driven by FFightSim at 60 Hz; reads only its input buffer and the sim.
	class FFighter
	{
	public:
		static constexpr int32 PrejumpFrames = 3;
		static constexpr int32 LandFrames = 3;
		static constexpr int32 LandFramesAttack = 5;
		static constexpr int32 DownFrames = 38;
		static constexpr int32 WakeupFrames = 24;
		static constexpr double RunMax = 100.0;
		// Running empties a full meter in about 1.7 seconds.
		static constexpr double RunDrain = 1.0;
		static constexpr double RunRegen = 1.6;
		// The meter refills once it has been left alone this long (ticks).
		static constexpr int32 RunRegenDelay = 30;
		static constexpr double RunMinStart = 12.0;
		// Juggle points a body can absorb before further hits pass through it.
		static constexpr int32 JuggleLimit = 7;

		FFightSim* Sim = nullptr;
		int32 Index = 0;
		// The definition fought with now: the fighter's own, or (the Grandmaster) the form or
		// borrowed shape he is in. BaseDef is who the fighter is, whatever shape it takes.
		const FFighterDef* Def = nullptr;
		const FFighterDef* BaseDef = nullptr;
		EForm Form = EForm::Own;
		// The Qhi meter (fighters whose BaseDef has one). It is never refilled by a change of
		// shape: only by its slow recovery, by caught blows (bounded) and at the start of a round.
		double Qhi = 0.0;
		int32 QhiIdle = 0;
		// The tick from which a caught blow may give Qhi back again.
		int32 QhiGainAt = 0;
		// A borrowed shape whose Qhi ran out: it breaks at the next moment the body is free.
		bool bShapeBreaking = false;
		// The shape's upkeep, as a multiple of the rules' (the Fragment's shape burns twice as fast).
		double UpkeepScale = 1.0;
		// Inside the current combo: the changes of shape made in it, and the shapes that landed blows
		// (an instant kill that ends a combo asks for two).
		int32 FormShifts = 0;
		TArray<FName, TInlineAllocator<4>> ComboShapes;
		// The change of shape in progress was begun inside a combo (it takes at once).
		bool bQuickShift = false;
		FInputBuffer Input;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Vel = FVector2D::ZeroVector;
		// +1 facing right (toward +x), -1 facing left.
		int32 Facing = 1;
		EState State = EState::Idle;
		// Ticks spent in the current state, 1 on the first.
		int32 SF = 0;
		const FMoveDef* Move = nullptr;
		// Frame of the move in progress, 1-based.
		int32 MF = 0;
		int32 MoveCmdPriority = 0;
		FVector2D MoveOrigin = FVector2D::ZeroVector;
		// None, "hit" or "block" for the move in progress.
		FName MoveContact;
		TSet<int32> HitGroups;
		int32 Health = 1000;
		// Full health for this match: the definition's unless the match sets it.
		int32 HealthMax = 0;
		double RunMeter = RunMax;
		int32 RunIdle = 0;
		// Times in a row the current move was entered by cancelling into itself.
		int32 SelfCancelsUsed = 0;
		int32 Stun = 0;
		EStun StunKind = EStun::None;
		int32 Hitstop = 0;
		double PushV = 0.0;
		double PushDec = 0.0;
		// The fighter whose hit is pushing this one (gets the overflow at a wall).
		int32 PushFrom = -1;
		int32 JugglePoints = 0;
		int32 ComboHits = 0;
		// Dazed once in the current combo: another daze in it is an ordinary hit (no loops).
		bool bDazedThisCombo = false;
		int32 ComboDamage = 0;
		bool bAirAttackUsed = false;
		// Relative direction of the jump: -1 back, 0 straight up, 1 forward.
		int32 JumpDir = 0;
		int32 Invuln = 0;
		int32 ArmorLeft = 0;
		int32 ProjectilesOut = 0;
		int32 DamageTaken = 0;
		// Throw in progress: indices of the victim / thrower, and the frame of the throw sequence.
		int32 ThrowVictim = -1;
		int32 ThrownBy = -1;
		int32 ThrowT = 0;
		// The move that last hit this fighter, for the reaction pose.
		const FMoveDef* LastHitMove = nullptr;
		ELevel LastHitLevel = ELevel::Mid;
		bool bCrouchStun = false;
		int32 LandFramesNow = LandFrames;
		// A temporary power-up from a move: its name, ticks left, hits it absorbs.
		FName Buff;
		int32 BuffTicks = 0;
		int32 BuffHits = 0;
		// Training: the dummy holds block automatically ("stand" or "crouch").
		FName AutoBlock;

		void Setup(FFightSim* InSim, int32 InIndex, const FFighterDef* InDef);
		FFighter& Opponent() const;
		void SetState(EState S);

		bool IsAirborne() const;
		bool IsCrouching() const;
		bool IsBlocking() const;
		bool IsInvulnerable() const;
		bool IsProjInvulnerable() const;
		bool IsActionable() const;
		int32 MaxHealth() const { return HealthMax > 0 ? HealthMax : BaseDef->Health; }
		bool UsesQhi() const { return BaseDef && BaseDef->Resource == EResource::Qhi; }
		bool HasRunMeter() const { return Def->Resource == EResource::Run; }
		const FQhiRules& QhiRules() const { return BaseDef->Qhi; }
		// Why the opponent's shape cannot be borrowed now ("shapeless", "mirror", "same"), or None.
		FName BorrowRefusal() const;
		// The definition the opponent's shape would be borrowed from.
		const FFighterDef* BorrowTarget() const;
		// A shapeshift move's target (the opponent's shape, or the one its form names), why it is
		// refused (as above), and its price in Qhi.
		const FFighterDef* ShapeTarget(const FMoveDef& M) const;
		FName ShapeRefusal(const FMoveDef& M) const;
		double ShapeCost(const FMoveDef& M) const;
		// Whether an instant kill's conditions hold now (see FMoveDef::KillRequires).
		bool KillAllowed(const FMoveDef& M) const;
		void SpendQhi(double Amount);
		// A caught blow's Qhi, at most once every DefendGainEvery ticks.
		void DefendGain();
		double Scale() const { return Def->Scale(); }
		FRect ToWorld(const FRect& R) const;
		void Hurtboxes(TArray<FRect>& Out) const;
		// Active hitboxes this frame that have not connected yet.
		void ActiveHitboxes(TArray<TPair<int32, FRect>>& Out) const;
		FRect Pushbox() const;
		bool HasPushbox() const;

		void Tick();
		void SpendRun(double Amount);
		// Pushes this fighter `Dist` metres along world x over about 8 ticks.
		void Push(double Dist, int32 From);
		void ToNeutral();
		void ComboReset();
		// bSpecialsOnly: a cancel (specials only, the same move only as its self_cancels allow);
		// bFeint: a feint's cancel (any other ground command). Neither starts a change of shape.
		// bShift: a change of shape inside a combo (only changes of shape).
		bool TryCommands(const TArray<FCommand>& List, int32 MinPriority = -1, bool bSpecialsOnly = false, bool bFeint = false, bool bShift = false);
		bool CanUse(const FMoveDef& M) const;
		void StartMove(const FMoveDef& M);
		void ReleaseThrow();
		// Sends this fighter flying: `V` is [away from the hitter, up] in m/s. Juggle: hittable on
		// the way down; otherwise a knockdown, untouchable.
		void Knock(const FVector2D& V, bool bJuggle, int32 FromFacing = 0);
		void TakeStun(int32 Frames, EStun Kind);
		// The move in progress is cut short by a blow or a grab (a shapeshift broken in its startup
		// pays its small failure cost here).
		void BreakMove();
		// Puts on a definition: `Kind` names the change for the event ("borrowed", "own", "spent",
		// "broken", "recentred").
		void SetForm(EForm F, const FFighterDef* D, FName Kind, FName How = NAME_None);

	private:
		void RegenRun();
		void QhiTick();
		bool CanStartRun() const;
		// Whether the body may change shape this tick (free, or between blows).
		bool FormSafe() const;
		void Refuse(FName Why);
		// The move's change of shape, on its form frame; the move ends there.
		void ChangeShape(const FMoveDef& M);
		// The Qhi is gone: the body falls into its spent form (Kind "spent", or "broken" from a
		// borrowed shape).
		void Exhaust(FName Kind);
		int32 RefusedAt = -100;
		void GravitySettle();
		void ApplyPush();
		void Neutral();
		void RunTick();
		void Jump();
		void AirTick();
		void IntegrateAir(double G);
		bool Landed() const { return Pos.Y <= 0.0 && Vel.Y <= 0.0; }
		void LandNow(int32 Frames);
		void MoveTick();
		bool TryChain();
		void MoveFrame();
		void SpawnCheck();
		void StunTick();
		void FallTick();
	};

	// A contact found this tick, applied after all are gathered so trades are symmetric.
	struct FContact
	{
		int32 Att = 0;
		int32 Def = 1;
		const FMoveDef* Move = nullptr;
		int32 Group = -1;
		int32 Proj = -1;
		bool bThrow = false;
		FVector2D Point = FVector2D::ZeroVector;
	};

	// A match on the fighting plane. Step() takes both players' device state for one 60 Hz tick
	// and advances everything; every outcome is reported as an event.
	class FFightSim
	{
	public:
		// Half the width of the walkable floor, in metres.
		static constexpr double StageHalf = 7.0;
		// The camera never lets the fighters drift further apart than this.
		static constexpr double MaxSep = 5.6;
		static constexpr double StartGap = 3.0;
		static constexpr int32 IntroTicks = 110;
		static constexpr int32 ReadyTicks = 100;
		static constexpr int32 RoundTicks = 90 * 60;
		static constexpr int32 RoundEndTicks = 170;
		static constexpr int32 FinishTicks = 420;
		static constexpr int32 ThrowBreakWindow = 8;
		// Push-back share from the second hit of a combo on, and for a dial-a-combo's opener.
		static constexpr double ComboPush = 0.35;
		static constexpr double ChainOpenerPush = 0.6;

		FFighter Fighters[2];
		TArray<FProjectile> Projectiles;
		TArray<FSimEvent> Events;
		int32 TickCount = 0;
		EPhase Phase = EPhase::Intro;
		int32 PhaseT = 0;
		int32 Timer = RoundTicks;
		// Ticks on the clock at the start of each round; 0 plays without a clock.
		int32 RoundLength = RoundTicks;
		int32 RoundNum = 1;
		int32 RoundsToWin = 2;
		// Rounds after which a match nobody has won yet ends drawn: the longest match and a few
		// drawn rounds (9 for first to two).
		int32 MaxRounds() const { return 2 * RoundsToWin + 5; }
		int32 Wins[2] = {0, 0};
		int32 RoundWinner = -1;
		FName RoundHow;
		int32 MatchWinner = -1;
		int32 FinishWinner = -1;
		bool bFinishDizzy = false;
		int32 FinishT0 = 0;
		// The finisher in progress or performed: who, which move, whether it was the stage's.
		int32 FinisherWho = -1;
		FName FinisherMove;
		bool bFinisherStage = false;
		bool bTraining = false;
		// Training: run meter and Qhi stay full.
		bool bInfiniteRun = false;
		// Borrowed shapes made this match, by the fighter copied (MakeBorrowedShape).
		TMap<FName, TSharedRef<FFighterDef>> Shapes;
		const FFighterDef* ShapeOf(const FFighterDef& Target, const FFighterDef& Copier, bool bEnhanced = true);
		// Training: health refills once a combo ends.
		bool bRefillHealth = false;
		bool bFinishersEnabled = true;
		// The arena's own finisher, when it has one (a move id in the shared basics).
		FName StageFinisher;
		bool bIntro = true;
		bool Flawless[2] = {false, false};
		// Damage taken over the whole match: a flawless match is one the winner finished unhurt.
		int32 MatchDamage[2] = {0, 0};
		int32 NextProjectileSerial = 1;

		void Setup(const FFighterDef* D1, const FFighterDef* D2);
		void Emit(FSimEvent E);
		bool CanAct() const { return Phase == EPhase::Fight || Phase == EPhase::Finish; }
		double Distance() const { return FMath::Abs(Fighters[1].Pos.X - Fighters[0].Pos.X); }
		int32 TimerSeconds() const { return FMath::CeilToInt(Timer / 60.0); }
		// Starts the current round over (training reset).
		void RestartRound() { StartRound(false); }
		void Step(const FPadState Pads[2]);
		bool AwaitingDizzy(const FFighter& F) const { return Phase == EPhase::Finish && !bFinishDizzy && F.Index != FinishWinner; }
		bool CanBlock(const FFighter& D, ELevel Level) const;
		void SpawnProjectile(FFighter& Owner, const FMoveDef& M);
		bool TryFinisher(FFighter& F);
		void BeginFinisher(FFighter& F, FName MoveId);
		void SkipFinisher();

	private:
		void StartRound(bool bWithIntro);
		void PhaseTick();
		void UpdateFacing();
		void Separate();
		void Clamp();
		void StepProjectiles();
		void Resolve();
		void ClashProjectiles();
		void Apply(const FContact& C);
		void ResolveThrows();
		void BreakThrow(FFighter& A, FFighter& D);
		void ThrowTick(FFighter& A);
		void CheckRound();
		bool FinishAllowed(int32 Winner) const;
		void EndRound(int32 Winner, FName How);
		void RoundEndTick();
		void FinisherTick();
	};

	// Watches a fight's events for one combo trial: the listed hits, landed by one fighter as a
	// single combo, in order. A hit out of order, or the combo ending, starts the trial over.
	struct FTrialCheck
	{
		enum class EResult : uint8 { None, Hit, Done, Dropped };
		TArray<FName> Moves;
		int32 Who = 0;
		int32 Progress = 0;
		bool bDone = false;

		EResult Feed(const FSimEvent& E);
		void Reset() { Progress = 0; bDone = false; }
	};

	// Frame advantage of the last exchange, as training shows it: once a blow lands or is blocked,
	// the ticks from the attacker being free to act again to the defender being free (positive: the
	// attacker moves first). A knockdown is reported as such rather than as a count.
	struct FAdvantageMeter
	{
		bool bHas = false;
		// Who struck, and by how many ticks they are free first.
		int32 By = -1;
		int32 Value = 0;
		bool bBlocked = false;
		bool bKnockdown = false;
		// A parry: `By` caught the other's blow.
		bool bParried = false;

		// Call after each step of the simulation.
		void Update(const FFightSim& Sim);
		void Reset() { *this = FAdvantageMeter(); }

	private:
		int32 Attacker = -1;
		int32 FreeAt[2] = {-1, -1};
		bool bWatchBlocked = false;
		bool bWatchParried = false;
		bool bWatchDown = false;
	};

	// Throw path keys [tick, forward, up] interpolated at tick T.
	FVector2D PathAt(const TArray<FVector>& Path, int32 T);
}
