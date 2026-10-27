#pragma once

#include "CoreMinimal.h"

// Shared types of the fighting simulation. The simulation is plain C++: it never reads the
// world, the clock or an input device, and nothing visual changes it. Positions are metres on
// the fighting plane (x along the stage, y up from the floor); time is whole 60 Hz ticks.
namespace Slop
{
	constexpr double DT = 1.0 / 60.0;

	// Moves From toward To by at most Delta (Godot's move_toward, which the rules were written with).
	inline double MoveToward(double From, double To, double Delta)
	{
		return FMath::Abs(To - From) <= Delta ? To : From + FMath::Sign(To - From) * Delta;
	}
	constexpr int32 TicksPerSecond = 60;

	// Buttons, as bits. Order follows the arcade panel.
	constexpr int32 BtnHP = 1;
	constexpr int32 BtnLP = 2;
	constexpr int32 BtnHK = 4;
	constexpr int32 BtnLK = 8;
	constexpr int32 BtnBL = 16;
	constexpr int32 BtnRN = 32;
	constexpr int32 BtnAttacks = BtnHP | BtnLP | BtnHK | BtnLK;
	constexpr int32 BtnAll = BtnAttacks | BtnBL | BtnRN;

	// Direction bits of a pad sample, as seen on screen.
	constexpr int32 PadRight = 1;
	constexpr int32 PadLeft = 2;
	constexpr int32 PadUp = 4;
	constexpr int32 PadDown = 8;

	// One player's device state for one tick.
	struct FPadState
	{
		int32 Dirs = 0;
		int32 Held = 0;
		int32 Pressed = 0;
		bool operator==(const FPadState& O) const { return Dirs == O.Dirs && Held == O.Held && Pressed == O.Pressed; }
	};

	// An axis-aligned rectangle: origin at the lower-left corner. Matches the original's Rect2
	// semantics, where rectangles that only touch do not intersect.
	struct FRect
	{
		double X = 0, Y = 0, W = 0, H = 0;
		FRect() = default;
		FRect(double InX, double InY, double InW, double InH) : X(InX), Y(InY), W(InW), H(InH) {}
		double EndX() const { return X + W; }
		double EndY() const { return Y + H; }
		bool Intersects(const FRect& B) const
		{
			return !(X >= B.EndX() || EndX() <= B.X || Y >= B.EndY() || EndY() <= B.Y);
		}
		FRect Intersection(const FRect& B) const
		{
			const double X0 = FMath::Max(X, B.X), Y0 = FMath::Max(Y, B.Y);
			const double X1 = FMath::Min(EndX(), B.EndX()), Y1 = FMath::Min(EndY(), B.EndY());
			return FRect(X0, Y0, FMath::Max(0.0, X1 - X0), FMath::Max(0.0, Y1 - Y0));
		}
		FVector2D Center() const { return FVector2D(X + W * 0.5, Y + H * 0.5); }
		FRect Scaled(double K) const { return FRect(X * K, Y * K, W * K, H * K); }
	};

	enum class ELevel : uint8 { High, Mid, Low, Overhead, Unblockable, Throw, None };

	ELevel ParseLevel(const FString& S);
	const TCHAR* LevelName(ELevel L);

	// Fighter states, in the original's order (traces and tests name them).
	enum class EState : uint8
	{
		Intro, Idle, WalkF, WalkB, Crouch, Run, Block, CrouchBlock, PreJump, Air, Land, Move,
		BlockStun, HitStun, Juggle, Fall, Down, WakeUp, Thrown, Dizzy, KO, Win, Finisher, Victim,
		Count
	};
	const TCHAR* StateName(EState S);

	// Kinds of stun a hit or block leaves.
	enum class EStun : uint8 { None, High, Mid, Low, Crouch, Guard, CrouchGuard, Crumple, Dazed, Pulled, Break };

	enum class EPhase : uint8 { Intro, Ready, Fight, Finish, Finisher, RoundEnd, MatchEnd };

	enum class EEvent : uint8
	{
		RoundStart, Fight, Move, Chain, Jump, Land, Run, Hit, Block, Armor, Parry, Throw, ThrowBreak,
		Projectile, ProjectileEnd, Clash, Teleport, Buff, BuffEnd, ComboEnd, KO, FinishPrompt,
		FinishTimeout, FinisherRange, Finisher, FinisherEnd, Timeout, RoundEnd, WinPose, MatchEnd,
		// The stranger encounter: a spoken line (Text; bHint for instructions), his evade, palm and strike.
		Line, StrangerEvade, StrangerStrike, StrangerCrack
	};
	const TCHAR* EventName(EEvent E);

	// One outcome of a tick. A flat record: each event type fills the fields it needs.
	struct FSimEvent
	{
		EEvent Type = EEvent::Fight;
		int32 Tick = 0;
		int32 Who = -1;
		int32 By = -1;
		FName Move;
		int32 Damage = 0;
		ELevel Level = ELevel::None;
		FName Effect;
		FVector2D Point = FVector2D::ZeroVector;
		int32 Combo = 0;
		bool bProj = false;
		FName Kind;
		bool bCounter = false;
		bool bKO = false;
		bool bThrow = false;
		bool bSoft = false;
		bool bFinish = false;
		bool bBoss = false;
		bool bFlawless = false;
		bool bStage = false;
		int32 Winner = -1;
		int32 Victim = -1;
		int32 Round = 0;
		int32 Hits = 0;
		FName How;
		FName After;
		FName Want;
		double Gap = 0;
		int32 Wins[2] = {0, 0};
		FString Text;
		bool bHint = false;
	};
}
