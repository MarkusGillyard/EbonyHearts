#pragma once

#include "Combat/SlopData.h"

// Key poses, the pose library and the rig solver. All of this works in "rig space" as the poses
// were authored: right-handed, x to the fighter's right, y up, -z toward the opponent, metres.
// The view layer maps the solved joints into Unreal's space at the end.
namespace Slop::Anim
{
	enum class ERot : uint8 { Hips, Spine, Chest, Neck, Head, ClavL, ClavR, WristL, WristR, Count };
	enum class EPos : uint8 { Hips, HandL, HandR, ElbowL, ElbowR, FootL, FootR, KneeL, KneeR, FingersL, FingersR, PalmL, PalmR, Count };
	enum class EVal : uint8 { FistL, FistR, YawL, YawR, ToeL, ToeR, FreeL, FreeR, Prop, Staff, Glow, AimL, AimR, EyeAlt, Count };

	// A Godot-style rigid transform (parent * child order), so the port reads like the original.
	struct FXf
	{
		FQuat Q = FQuat::Identity;
		FVector P = FVector::ZeroVector;
		FXf() = default;
		FXf(const FQuat& InQ, const FVector& InP) : Q(InQ), P(InP) {}
		FVector operator*(const FVector& V) const { return P + Q.RotateVector(V); }
		FXf operator*(const FXf& C) const { return FXf(Q * C.Q, P + Q.RotateVector(C.P)); }
		FXf Inverse() const { const FQuat I = Q.Inverse(); return FXf(I, I.RotateVector(-P)); }
		FVector X() const { return Q.RotateVector(FVector(1, 0, 0)); }
		FVector Y() const { return Q.RotateVector(FVector(0, 1, 0)); }
		FVector Z() const { return Q.RotateVector(FVector(0, 0, 1)); }
	};

	FQuat EulerDeg(double X, double Y, double Z);
	FQuat FromAxes(const FVector& X, const FVector& Y, const FVector& Z);
	FQuat AxisAngle(const FVector& Axis, double Rad);

	// One full-body key pose. Spine and head are rotations (FK); hands and feet are IK targets in
	// the fighter's root space. Data writes positions as [forward, up, right] and angles in degrees.
	struct FPose
	{
		FQuat Rot[(int32)ERot::Count];
		FVector Pos[(int32)EPos::Count];
		double Val[(int32)EVal::Count];

		FPose();
		FQuat& R(ERot K) { return Rot[(int32)K]; }
		FVector& P(EPos K) { return Pos[(int32)K]; }
		double& V(EVal K) { return Val[(int32)K]; }
		const FQuat& R(ERot K) const { return Rot[(int32)K]; }
		const FVector& P(EPos K) const { return Pos[(int32)K]; }
		double V(EVal K) const { return Val[(int32)K]; }

		static FPose FromJson(const TSharedPtr<FJsonObject>& D, const FPose* Base, double Scale);
		// Left-right mirror, for showing a fighter facing the other way.
		void MirrorInto(FPose& Out) const;
		// Blend of A and B at T into this pose (T = 0 gives A).
		void BlendInto(const FPose& A, const FPose& B, double T);
		static double Ease(FName Kind, double T);
	};

	struct FKey
	{
		double F = 0;
		const FPose* Pose = nullptr;
		FName Ease;
	};

	// Every pose one fighter can show: shared poses (authored for a 1.8 m body and scaled to the
	// fighter) overridden by the fighter's own, plus the keyframes of its moves.
	class FPoseLibrary
	{
	public:
		void Build(const FFighterDef& Def, const TSharedPtr<FJsonObject>& Common);
		bool Has(FName Name) const { return Src.Contains(Name); }
		const FPose& Get(FName Name);
		void Keys(const TArray<FAnimKey>& Raw, TArray<FKey>& Out);
		const TArray<FKey>* MoveKeys(FName MoveId) const { return MoveKeyMap.Find(MoveId); }
		const TArray<FName>& Missing() const { return MissingNames; }
		// Pose at fractional frame T of a key list, starting from `From` at frame 0.
		static void Sample(const TArray<FKey>& Keys, double T, const FPose& From, FPose& Out);

	private:
		const FPose& CommonPose(FName Name);
		TMap<FName, TUniquePtr<FPose>> Poses;
		TMap<FName, TUniquePtr<FPose>> CommonCache;
		TMap<FName, TSharedPtr<FJsonObject>> Src;
		TMap<FName, double> SrcScale;
		TSharedPtr<FJsonObject> Common;
		double CommonScale = 1.0;
		TMap<FName, TArray<FKey>> MoveKeyMap;
		TArray<FName> MissingNames;
		FPose Default;
	};

	enum class EJoint : uint8
	{
		Hips, Spine, Chest, Neck, Head, ClavL, UpperArmL, ForearmL, HandL, ClavR, UpperArmR, ForearmR, HandR,
		ThighL, ShinL, FootL, ThighR, ShinR, FootR, Count
	};
	const TCHAR* JointName(EJoint J);
	EJoint JointParent(EJoint J);

	// A fighter's skeleton posed by forward kinematics for the spine and two-bone IK for the limbs.
	class FRig
	{
	public:
		double H = 1.8;
		double UpperLen = 0.3, ForeLen = 0.27, ThighLen = 0.39, ShinLen = 0.43, AnkleH = 0.08;
		FVector Rest[(int32)EJoint::Count];
		// Rig-space joint transforms from the last Apply().
		FXf Xf[(int32)EJoint::Count];
		// Bend axes of the limbs from the last solve (arms l/r, legs l/r).
		FVector Bend[4];
		// How closed each hand is: 1 a fist, 0 an open palm.
		double Fist[2] = {1.0, 1.0};

		// Proportions as the original game built them from height and the look's build.
		void Build(const FFighterDef& Def);
		// Poses the body. `Feet` are rig-space ankle targets and `FootYaw` the yaw of a planted foot.
		// `Extra` adds per-joint rotations on top of the pose (keyed by joint; identity when absent).
		void Apply(const FPose& P, const FVector Feet[2], const double FootYaw[2], const FQuat Extra[(int32)EJoint::Count]);
		const FXf& J(EJoint Joint) const { return Xf[(int32)Joint]; }
		FVector HandTip(int32 Side) const;
		FVector FootTip(int32 Side) const;

		static void TwoBone(const FVector& Root, const FVector& Target, const FVector& Pole, double A, double B, FVector& Mid, FVector& End, FVector& Axis);
		static FQuat AimBasis(const FVector& Along, const FVector& BendAxis);

	private:
		void SetLocal(EJoint J, const FXf& Local);
		void SetGlobal(EJoint J, const FXf& G) { Xf[(int32)J] = G; }
		void Arm(int32 Side, const FPose& P);
		void Leg(int32 Side, const FVector& Ankle, const FVector& Pole, double Yaw, double Free, double ToeDeg);
	};
}
