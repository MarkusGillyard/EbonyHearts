#pragma once

#include "Anim/SlopPose.h"
#include "Combat/SlopSim.h"

namespace Slop::Anim
{
	// Animates one simulated fighter. Every rendered frame it picks the pose the simulation state
	// calls for, crossfades from what was on screen, mirrors it when the fighter faces left (so the
	// same side of the body always faces the camera, as in a sprite game), plants the feet, adds
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
		// Kicks the hit-reaction springs. `Dir` is +1 when the blow pushes toward +x.
		void Impact(double Dir, double Strength, ELevel Level, bool bBlocked);
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

		// The shown pose after mirroring (rig space), for props and debug views.
		const FPose& Shown() const { return Facing < 0 ? Mirror : Out; }
		FVector WorldPoint(const FVector& RigPoint) const { return Root * RigPoint; }
		FVector StrikePoint(FName Which) const;

	private:
		bool SetSource(const FString& Key, double BlendTicks);
		void HoldPose(FName Name, double Blend);
		void EvaluateSource(double Frac);
		void RunCycle();
		FPose ReactionPose(FName React, FName Back, double StunTotal);
		void Procedural(double Delta, bool bFrozen, FQuat Extra[(int32)EJoint::Count]);
		FXf AirTilt(double Delta, bool bFrozen);
		void PlantFeet(double Delta, const FPose& Pose, FVector OutAnkles[2], double OutYaw[2]);
		double MoveSink(double Frac) const;
		void AnimKeys(const TArray<FAnimKey>& Raw, TArray<FKey>& OutKeys) { Lib.Keys(Raw, OutKeys); }

		struct FFoot
		{
			FVector Pos = FVector::ZeroVector;
			double Yaw = 0;
			double Step = -1;
			FVector From = FVector::ZeroVector;
			double FromYaw = 0;
			double Dur = 0.15;
			bool bValid = false;
		};

		FVector2D PrevPos = FVector2D::ZeroVector;
		FVector2D CurPos = FVector2D::ZeroVector;
		int32 Facing = 1;
		FPose Out, Mirror, Snap, Src;
		FString Key;
		double BlendT = 0;
		double BlendLen = 0;
		TArray<FKey> Keys;
		TArray<FKey> OwnedKeys;
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
		int32 MoveTick = 0;
		EState LastState = EState::Count;
		const FMoveDef* LastMove = nullptr;
		// Whole-body rotation about the hips while airborne (degrees, + pitches backward).
		double AirPitch = 0;
		double AirPitchV = 0;
		double Flip = 0;
		double RunPhase = 0;
	};
}
