#pragma once

#include "Combat/SlopInput.h"
#include "Dom/JsonObject.h"

namespace Slop
{
	enum class EMoveKind : uint8 { Normal, Special, Throw, Grab, Finisher, Taunt, Air, Chain };

	// A rectangle live on frames [From, To] of a move, in the fighter's [forward, up] space.
	struct FFrameBox
	{
		int32 From = 1;
		int32 To = 1;
		FRect Rect;
	};

	// A dial-a-combo link: pressing `Input` on frames [From, To] starts `Move` for `Cost` run meter.
	struct FChainLink
	{
		FString Input;
		int32 Mask = 0;
		TArray<int32> Steps;
		FName Move;
		int32 From = 1;
		int32 To = 1;
		double Cost = 12.0;
	};

	struct FProjectileDef
	{
		bool bHas = false;
		int32 Frame = 1;
		FName Kind = TEXT("orb");
		FVector2D Offset = FVector2D(0.6, 1.3);
		bool bAtOpponent = false;
		FVector2D AtOpponent = FVector2D::ZeroVector;
		double Speed = 6.0;
		double Vy = 0.0;
		double Gravity = 0.0;
		double Decel = 0.0;
		double MinSpeed = 0.0;
		FVector2D Size = FVector2D(0.3, 0.3);
		int32 Life = 90;
		int32 Hits = 1;
		int32 Strength = 1;
		ELevel Level = ELevel::High;
		int32 Damage = 60;
		int32 Chip = 12;
		int32 Hitstun = 24;
		int32 Blockstun = 14;
		int32 Hitstop = 8;
		FName Effect = TEXT("stagger");
		FVector2D Launch = FVector2D(1.5, 0.0);
		FVector2D Juggle = FVector2D(1.8, 4.0);
		double PushHit = 0.5;
		double PushBlock = 0.45;
		FName Ground = TEXT("die");
		bool bReturns = false;
		bool bTether = false;
		// The share of its damage that is magical (data "magic": 0 physical, 1 magical, between
		// them mixed); defences read it (FFighterDef::PhysDef, MagicDef).
		double Magic = 0.0;
		// Shared basics are authored for a 1.8 m body; this converts offsets and sizes.
		double Scale = 1.0;
	};

	struct FThrowDef
	{
		bool bHas = false;
		int32 Frames = 40;
		int32 DamageFrame = 20;
		int32 Damage = 100;
		FVector2D Release = FVector2D(2.0, 3.0);
		int32 Recover = 10;
		bool bSwap = false;
		// Whether a command grab can be broken like a throw (throws always can).
		bool bBreakable = false;
		// [tick, forward, up] keys of the victim's position relative to the thrower.
		TArray<FVector> Path;
	};

	// One key of a move's animation: reach pose `Pose` at frame `Frame` with easing `Ease`.
	struct FAnimKey
	{
		int32 Frame = 0;
		FName Pose;
		FName Ease;
	};

	// One attack, special, throw or action. Frames are 1-based: frame `Startup` is the first active
	// frame. Boxes are [forward, up, width, height] in metres from the fighter's root.
	struct FMoveDef
	{
		FName Id;
		FString Name;
		EMoveKind Kind = EMoveKind::Normal;
		int32 Startup = 1;
		int32 Active = 0;
		int32 Recovery = 0;
		int32 Total = 1;
		ELevel Level = ELevel::None;
		int32 Damage = 0;
		int32 Chip = 0;
		int32 Hitstun = 18;
		int32 Blockstun = 12;
		int32 Hitstop = -1;
		FName Effect = TEXT("stagger");
		FVector2D Launch = FVector2D(1.5, 0.0);
		FVector2D Juggle = FVector2D(1.6, 4.2);
		int32 JuggleCost = 1;
		double PushHit = 0.35;
		double PushBlock = 0.45;
		TArray<FFrameBox> Hitboxes;
		TArray<FFrameBox> Hurtboxes;
		TArray<FVector2D> Motion;
		TArray<FVector2D> Lift;
		FVector Track = FVector::ZeroVector;
		bool bAir = false;
		FIntPoint Invuln = FIntPoint::ZeroValue;
		FIntPoint ProjInvuln = FIntPoint::ZeroValue;
		FIntPoint Armor = FIntPoint::ZeroValue;
		FIntPoint LowProfile = FIntPoint::ZeroValue;
		FIntPoint Airborne = FIntPoint::ZeroValue;
		bool bEndsCrouched = false;
		TArray<FChainLink> Chain;
		FIntPoint Cancel = FIntPoint::ZeroValue;
		// How many times in a row the move may cancel into itself (data "self_cancels"; 0, the
		// default: never). Ashka's Elbow Rush cancelling into itself was an infinite (from the
		// original's data); a fighter meant to have that string gets a count instead.
		int32 SelfCancels = 0;
		FProjectileDef Projectile;
		FThrowDef Throw;
		FName Requires;
		double RunCost = 0.0;
		bool bFinisher = false;
		TArray<FName> Tags;

		// Effects the simulation reads.
		int32 ArmorHits = 1;
		double HealthBelow = -1.0;
		FName Buff;
		int32 BuffFrame = 1;
		int32 BuffTicks = 240;
		int32 BuffHits = 2;
		bool bTeleport = false;
		int32 TeleportFrame = 20;
		double TeleportDist = 0.9;
		bool bAirVel = false;
		FVector AirVel = FVector::ZeroVector;
		bool bParry = false;
		FIntPoint ParryFrames = FIntPoint(3, 20);
		int32 ParryStun = 34;
		FName ParryCounter;
		// The levels it catches (none listed: high, mid and overhead), and the damage a catch does
		// the attacker (a shin check stings the kicking leg; it never ends a round).
		TArray<ELevel> ParryLevels;
		int32 ParryDamage = 0;
		bool Parries(ELevel L) const { return ParryLevels.Num() > 0 ? ParryLevels.Contains(L) : (L == ELevel::High || L == ELevel::Mid || L == ELevel::Overhead); }
		bool bAirKnock = false;
		FString StageInput;
		FName StageId;
		// The share of the damage that is magical (data "magic"; see FProjectileDef::Magic).
		double Magic = 0.0;
		// Qhi spent when the move starts (data "qhi"); only a fighter with a Qhi meter can use it.
		double QhiCost = 0.0;
		// A feint (props "feint"): frames on which the move cancels into any other ground command,
		// without having made contact.
		FIntPoint Feint = FIntPoint::ZeroValue;
		// A ward (props "ward"): frames on which projectiles touching the fighter break up harmlessly.
		FIntPoint Ward = FIntPoint::ZeroValue;
		// A caught blow leaves the attacker stumbling (EStun::Break) rather than dazed: a weave
		// slips the blow instead of trapping the arm (parry "stagger").
		bool bParryStagger = false;
		// A caught blow is caught for good: the parrier takes hold of the attacker at once and the
		// counter (a grab with a throw sequence) plays on them, unbreakable (parry "catch").
		bool bParryCatch = false;
		// The Grandmaster's shapeshift (effects "shapeshift": the frame it takes) and its reversal
		// (effects "revert"). The move ends on that frame, in the new shape.
		bool bShapeshift = false;
		bool bRevert = false;
		int32 FormFrame = 0;
		// Presentation-only effect ranges ("sink", "whip").
		FIntPoint Sink = FIntPoint::ZeroValue;
		FIntPoint Whip = FIntPoint::ZeroValue;

		// Presentation: animation keys for the fighter, while holding a throw victim, and for the victim.
		TArray<FAnimKey> AnimKeys;
		TArray<FAnimKey> HoldKeys;
		TArray<FAnimKey> VictimKeys;
		FName Tip;
		FName Swing;
		// What lands the blow, for its sound: wood, metal, blade or chain (none: the body).
		FName HitSound;

		bool IsAttack() const { return Level != ELevel::None; }
		int32 LastActive() const { return Startup + Active - 1; }
		bool InActive(int32 Frame) const { return Active > 0 && Frame >= Startup && Frame <= LastActive(); }
		bool HasTag(FName T) const { return Tags.Contains(T); }
		// Frame advantage on block / hit when the first active frame connects (move lists, tests).
		// Ticks the attacker is free before the defender after a block on the first active frame. The
		// stun's last tick is the one the defender is free on, so the count is Active + Recovery (the
		// original game's lists said Active - 1 + Recovery, a frame too kind).
		int32 BlockAdvantage() const { return Blockstun - (Active + Recovery); }
		int32 HitAdvantage() const;
		bool IsThrowKind() const { return Kind == EMoveKind::Throw || Kind == EMoveKind::Grab; }

		static double CurveAt(const TArray<FVector2D>& Curve, double Frame);
		static bool InRange(const FIntPoint& R, int32 Frame) { return R != FIntPoint::ZeroValue && Frame >= R.X && Frame <= R.Y; }
	};

	// Static description of one fighter: data/fighters/<id>.json on top of the shared basic moves
	// in data/basics.json (authored for a 1.8 m body and scaled to this fighter).
	// How a fighter's stance and walk move: the animator's stance and walk layers, scaled per style
	// (fighter JSON "motion"; 1 is the shared default). Presentation only.
	struct FMotionStyle
	{
		// The stance: how far the weight sinks and rises, how far it rocks between the feet, and the
		// rhythm's rate.
		double Bounce = 1.0;
		double Rock = 1.0;
		double Pace = 1.0;
		// The walk: how much the body rides each step, how high the feet lift, how far the hips and
		// shoulders counter-turn.
		double WalkBob = 1.0;
		double StepLift = 1.0;
		double WalkTwist = 1.0;
		// The shown body curves toward and away from the camera as it walks (Baguazhang's circle).
		double CircleWalk = 0.0;
		// The head moves only with the chest (a diving helmet fixed to its breastplate).
		bool bHeadLocked = false;
		// Walking and running take the captured step cycles (cycles_mocap.json) where there are
		// any; a style with its own footwork (sumo's sliding feet, a diving suit) keeps the
		// procedural steps.
		bool bWalkCycle = true;
		bool bRunCycle = true;
	};

	// One captured step cycle: poses over one stride, played by distance covered.
	struct FCycleDef
	{
		double Stride = 1.0;
		TArray<FName> Poses;
	};

	// What a fighter's second bar holds: the run meter, the Grandmaster's Qhi, or nothing (his
	// spent form, which has neither).
	enum class EResource : uint8 { Run, Qhi, None };

	// The Grandmaster's Qhi (fighter JSON "qhi"). Per tick unless named otherwise.
	struct FQhiRules
	{
		// Fixed by the owner: a shapeshift that takes costs half of the meter's capacity.
		static constexpr double ShapeshiftShare = 0.5;
		double Max = 100.0;
		// Passive recovery once nothing has been spent for RegenDelay ticks.
		double Regen = 0.05;
		int32 RegenDelay = 60;
		// Spent: the meter creeps back at this rate, and the body stays spent until it reaches RecoverAt.
		double SpentRegen = 0.04;
		double RecoverAt = 30.0;
		// The glide that takes the place of running drains this much, and needs GlideMin to start.
		double GlideDrain = 0.25;
		double GlideMin = 5.0;
		// A dial-a-combo link costs this share of its run cost in Qhi.
		double ChainShare = 0.35;
		// A borrowed shape is held by Qhi: Upkeep a tick, and HitDrain for each point of damage taken.
		double Upkeep = 0.067;
		double HitDrain = 0.1;
		// A parry or weave that catches a blow gives back a little, at most once every DefendGainEvery ticks.
		double DefendGain = 5.0;
		double SpentDefendGain = 3.0;
		int32 DefendGainEvery = 120;
		// A shapeshift broken before it takes costs this (the focus spent), never the full price.
		double FailCost = 8.0;
		// The Qhi laid into a borrowed shape: damage dealt, damage taken, walk and run speed.
		double CopyPower = 1.15;
		double CopyGuard = 0.9;
		double CopySpeed = 1.08;

		double ShapeshiftCost() const { return Max * ShapeshiftShare; }
	};

	struct FFighterDef
	{
		static constexpr double BaseHeight = 1.8;

		FName Id;
		FString Name;
		FString Title;
		FString Pronoun = TEXT("him");
		FString Style;
		FString Blurb;
		double Height = 1.8;
		int32 Health = 1000;
		double WalkF = 2.4;
		double WalkB = 1.9;
		double RunSpeed = 5.2;
		double JumpVy = 8.6;
		double JumpVx = 2.6;
		double Gravity = 20.0;
		double PushW = 0.28;
		double ThrowRange = 0.95;
		bool bBoss = false;
		bool bHidden = false;
		bool bUnthrowable = false;
		// Damage taken from physical and magical blows (stats "phys_def", "magic_def"; 1 as
		// authored, above 1 takes more), and dealt by the fighter's physical and magical blows
		// ("power", "arcana"). A mixed blow is weighed by its Magic share.
		double PhysDef = 1.0;
		double MagicDef = 1.0;
		double Power = 1.0;
		double Arcana = 1.0;
		EResource Resource = EResource::Run;
		FQhiRules Qhi;
		// Whether the Grandmaster can borrow this fighter's shape (data "copyable"; bosses and the
		// fighters that are not in the roster cannot be).
		bool bCopyable = true;
		// The mesh and look worn (data "body"; the fighter's own id by default). A form of another
		// fighter wears that fighter's body.
		FName Body;
		// For a Qhi fighter: the form his body falls into when the meter is spent (data
		// "spent_form", linked once every fighter is loaded).
		FName SpentFormId;
		const FFighterDef* SpentForm = nullptr;
		// A borrowed shape: the fighter it was copied from (null for everyone's own definition).
		const FFighterDef* CopyOf = nullptr;
		FMotionStyle Motion;
		// "walk_f", "walk_b", "run": the captured cycles this fighter uses (by its Motion).
		TMap<FName, FCycleDef> Cycles;
		TMap<FName, TSharedRef<FMoveDef>> Moves;
		// Ground and air commands, highest priority first.
		TArray<FCommand> Commands;
		TArray<FCommand> AirCommands;
		bool bHasFinisher = false;
		FCommand FinisherCmd;
		FName FinisherRange = TEXT("close");
		TSharedPtr<FJsonObject> Poses;
		// Fields laid over named poses after the port (poses/<id>_style.json), in those poses' units.
		TSharedPtr<FJsonObject> PoseStyle;
		// Keyed animation for a state in place of its held pose ("intro", "win"), frames counted
		// from entering the state (fighter JSON "state_anim"; written by Tools/mocap).
		TMap<FName, TArray<FAnimKey>> StateAnims;
		TSharedPtr<FJsonObject> Look;
		TSharedPtr<FJsonObject> AI;
		TSharedPtr<FJsonObject> Raw;

		const FMoveDef* Move(FName MoveId) const
		{
			const TSharedRef<FMoveDef>* M = Moves.Find(MoveId);
			return M ? &M->Get() : nullptr;
		}
		double Scale() const { return Height / BaseHeight; }
		// The notation a player types for a move (first ground or air command that starts it).
		FString NotationOf(FName MoveId) const;
	};

	// All game data, loaded once and read-only afterwards.
	class FContent
	{
	public:
		// Selectable roster in select-screen order; fighters that exist but are not selectable.
		static const TArray<FName>& Roster();
		// Fighters that are selectable once unlocked (the Grandmaster).
		static const TArray<FName>& Secret();
		static const TArray<FName>& Special();
		// Definitions that are another fighter's form, never picked (the Grandmaster spent).
		static const TArray<FName>& Forms();
		// Every definition a fighter fights with: the roster, the secret fighters and their forms.
		static TArray<FName> Fighting();

		// Loads every file under DataDir. Returns false (with Errors filled) when anything is
		// malformed; a match must not start on bad data.
		bool Load(const FString& DataDir);
		const FFighterDef* Fighter(FName Id) const
		{
			const TSharedRef<FFighterDef>* F = Fighters.Find(Id);
			return F ? &F->Get() : nullptr;
		}

		TMap<FName, TSharedRef<FFighterDef>> Fighters;
		TSharedPtr<FJsonObject> Basics;
		TSharedPtr<FJsonObject> Story;
		// The shared poses, with the style refinements made since the port laid over them
		// (poses_common_style.json); CommonPosesOriginal is the original game's set.
		TSharedPtr<FJsonObject> CommonPoses;
		TSharedPtr<FJsonObject> CommonPosesOriginal;
		// Per fighter, fields from motion capture laid over its data (fighters_mocap.json).
		TSharedPtr<FJsonObject> FightersMocap;
		// The captured step cycles (cycles_mocap.json, from Tools/mocap).
		TSharedPtr<FJsonObject> CyclesMocap;
		TSharedPtr<FJsonObject> Trials;
		TArray<FString> Errors;
		FString Dir;

		static TSharedPtr<FJsonObject> LoadJson(const FString& Path, TArray<FString>* Errors = nullptr);
		static TSharedPtr<FJsonObject> ParseJson(const FString& Text);

		// Builds a fighter from a definition object on top of the loaded basics (also used by tests).
		TSharedPtr<FFighterDef> ParseFighter(const TSharedPtr<FJsonObject>& D);
		void Validate(const FFighterDef& F);
	};

	// The shape the Grandmaster takes on borrowing `Target`'s: the target's whole fighting
	// definition (moves, commands, body, AI), strengthened by Qhi (`Copier`'s rules: power, guard,
	// speed), with a run meter, no finisher of its own, and the copier's reversal added.
	TSharedRef<FFighterDef> MakeBorrowedShape(const FFighterDef& Target, const FFighterDef& Copier);

	// The game's data, loaded from the project's Content/Data on first use.
	FContent& GetContent();

	// Shared JSON accessors with defaults.
	namespace Json
	{
		double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default);
		int32 Int(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Default);
		bool Bool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default);
		FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Default = FString());
		FVector2D Vec2(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FVector2D& Default);
		TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key);
		const TArray<TSharedPtr<FJsonValue>>* Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key);
		FVector2D ArrVec2(const TArray<TSharedPtr<FJsonValue>>& A);
	}
}
