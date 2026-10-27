#pragma once

#include "Anim/SlopPose.h"
#include "Combat/SlopSim.h"

namespace Slop::Anim
{
	// Animates one simulated fighter. Every rendered frame it picks the pose the simulation state
	// calls for, crossfades from what was on screen, lets the body follow through (each part chases
	// the pose on its own spring, the core stiffest, so motion ripples out from the hips), mirrors it
	// when the fighter faces left (so the same side of the body always faces the camera, as in a
	// sprite game; crossing over while standing turns round over a few frames), gives the stance and
	// the walk their weight and rhythm, carries the hands with the chest, plants the feet, adds
	// hit-reaction springs and flips and spins airborne bodies. It only reads the simulation.
	//
	// World space here is the original's: x along the stage, y up, +z toward the camera, metres.
	class FAnimator
	{
	public:
		void Setup(const FFighter& InFighter, const TSharedPtr<FJsonObject>& CommonPoses);
		// Call right after every simulation tick.
		void SyncFromSim(bool bTeleport = false);
		// `Frac` is how far rendering is between the previous tick and the current one.
		void Update(double Frac, double Delta);
		// Kicks the hit-reaction springs. `Dir` is +1 when the blow pushes toward +x; `StrikeDir`, the
		// world direction the striking hand or foot was travelling (zero when unknown), turns the head
		// for a hook and lifts the chin for an uppercut.
		void Impact(double Dir, double Strength, ELevel Level, bool bBlocked, const FVector& StrikeDir = FVector::ZeroVector, bool bCounter = false);
		// The world direction the current attack's striking hand or foot travels into the blow.
		FVector StrikeDirection() const;
		// The jolt back through the attacker's body when a blow lands or is blocked.
		void Recoil(double Strength);
		void Flash(double Amount) { FlashAmount = FMath::Max(FlashAmount, Amount); }
		// Shows a named pose directly, bypassing the simulation (previews, menus, portraits).
		void Preview(FName PoseName);
		void ResetFeet();

		const FFighter* Fighter = nullptr;
		FPoseLibrary Lib;
		FRig Rig;
		bool bSmoothing = true;

		// Results of the last Update: the rig's world transform (rigid) and a world-axis scale.
		FXf Root;
		FVector WorldScale = FVector::OneVector;
		double FlashAmount = 0.0;
		double Glow = 0.0;
		// Presentation overrides set by the finisher director.
		FVector ExtraScale = FVector::OneVector;
		double Sink = 0.0;
		FVector LiftExtra = FVector::ZeroVector;
		double SpinExtra = 0.0;
		FName PoseOverride;

		// The pose last given to the rig (rig space, after mirroring and the procedural layers).
		const FPose& Shown() const { return Display; }
		// The authored pose before follow-through (rig space, not mirrored), for tests.
		const FPose& Authored() const { return Out; }
		FVector WorldPoint(const FVector& RigPoint) const { return Root * RigPoint; }
		FVector StrikePoint(FName Which) const;

	private:
		bool SetSource(const FString& Key, double BlendTicks);
		void HoldPose(FName Name, double Blend);
		void EvaluateSource(double Frac);
		void RunCycle();
		bool PlayCycle(FName Name, double BlendTicks);
		FPose ReactionPose(FName React, FName Back, double StunTotal);
		void Procedural(double Delta, bool bFrozen, FQuat Extra[(int32)EJoint::Count]);
		FXf AirTilt(double Delta, bool bFrozen);
		void PlantFeet(double Delta, const FPose& Pose, FVector OutAnkles[2], double OutYaw[2]);
		double MoveSink(double Frac) const;
		void FollowThrough(double Delta, bool bFrozen);
		void DriveThrough(double T);
		bool StrikeKeys(EPos& OutTip, int32& OutHit) const;
		bool ArmExempt(int32 AuthoredSide) const;
		bool CanTurn() const;
		void Life(double Delta, bool bFrozen, double X, FPose& Pose, FQuat Extra[(int32)EJoint::Count]);
		void CarryArms(const FXf& From, const FXf& To, FPose& Pose) const;
		void AnimKeys(const TArray<FAnimKey>& Raw, TArray<FPoseKey>& OutKeys) { Lib.Keys(Raw, OutKeys); }

		struct FFoot
		{
			FVector Pos = FVector::ZeroVector;
			double Yaw = 0;
			double Step = -1;
			FVector From = FVector::ZeroVector;
			double FromYaw = 0;
			double Dur = 0.15;
			bool bValid = false;
			// In a captured step cycle: held where it touched down, then eased back onto the path.
			bool bLocked = false;
			double Release = 0;
		};

		FVector2D PrevPos = FVector2D::ZeroVector;
		FVector2D CurPos = FVector2D::ZeroVector;
		int32 Facing = 1;
		FPose Out, Snap, Src;
		// Follow-through: the authored pose chased by springs (rotations with angular velocities,
		// positions with velocities), then mirrored and layered into what the rig is given.
		FPose Lagged, Display;
		FVector LagW[(int32)ERot::Count];
		FVector LagV[(int32)EPos::Count];
		bool bLagValid = false;
		// Turning round in place after a cross-over: 0..1 through the turn, from the old facing.
		double TurnT = 1.0;
		int32 TurnFrom = 1;
		double TurnSign = 1.0;
		// The stance and walk layers, eased in and out with the state, and their rhythm.
		double IdleW = 0.0;
		double WalkW = 0.0;
		double IdlePhase = 0.0;
		double Pace = 1.0;
		FVector HandOff[2];
		FString Key;
		double BlendT = 0;
		double BlendLen = 0;
		TArray<FPoseKey> Keys;
		TArray<FPoseKey> OwnedKeys;
		FPose OwnedPoses[3];
		FFoot Feet[2];
		FVector Lean = FVector::ZeroVector;
		FVector LeanV = FVector::ZeroVector;
		// Hit springs: chest, head, hips (position, velocity).
		FVector Spring[3][2];
		double Time = 0;
		FVector VisVel = FVector::ZeroVector;
		FVector LastRoot = FVector::ZeroVector;
		double Shake = 0;
		double ShakeFrames = 1;
		// Frames the shown pose runs ahead of the simulation after a freeze: hit-stop holds the
		// simulation, but a struck body starts into its recoil and a striking one keeps driving.
		double PoseLead = 0;
		int32 MoveTick = 0;
		EState LastState = EState::Count;
		const FMoveDef* LastMove = nullptr;
		// Whole-body rotation about the hips while airborne (degrees, + pitches backward).
		double AirPitch = 0;
		double AirPitchV = 0;
		double Flip = 0;
		double RunPhase = 0;
		// Distance covered in the current walk or run, for the captured step cycles; whether one
		// drives the legs this frame (the feet then follow it instead of being planted).
		double CycleDist = 0;
		double CycleX = 0;
		// Seconds the body has not moved: walking into a wall or the distance limit stops the cycle,
		// which is played by distance, so the fighter settles into the stance instead of hanging
		// mid-stride with a foot in the air.
		double StillTime = 0;
		// The lowest each foot comes in the cycle playing (pose space): a foot near it is down.
		double CycleFloor[2] = {0, 0};
		bool bCycleFeet = false;
	};
}
