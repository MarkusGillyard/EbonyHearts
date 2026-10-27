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
		bool bAirKnock = false;
		FString StageInput;
		FName StageId;
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
		static const TArray<FName>& Special();

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
