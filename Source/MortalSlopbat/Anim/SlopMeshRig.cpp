#include "Anim/SlopMeshRig.h"

namespace Slop::Anim
{
	FVector WorldToUE(const FVector& G) { return FVector(-G.Z, G.X, G.Y) * 100.0; }
	FVector UEToWorld(const FVector& U) { return FVector(U.Y, U.Z, -U.X) / 100.0; }
	FVector2D StageToUE2D(const FVector2D& G) { return G * 100.0; }

	static FVector DirToUE(const FVector& G) { return FVector(-G.Z, G.X, G.Y); }

	struct FBoneNames
	{
		EJoint Joint;
		const TCHAR* Name;
	};

	static const FBoneNames JointBones[] = {
		{EJoint::Hips, TEXT("pelvis")}, {EJoint::Spine, TEXT("spine_01")}, {EJoint::Chest, TEXT("spine_02")},
		{EJoint::Neck, TEXT("neck_01")}, {EJoint::Head, TEXT("head")},
		{EJoint::ClavL, TEXT("clavicle_l")}, {EJoint::UpperArmL, TEXT("upperarm_l")}, {EJoint::ForearmL, TEXT("lowerarm_l")}, {EJoint::HandL, TEXT("hand_l")},
		{EJoint::ClavR, TEXT("clavicle_r")}, {EJoint::UpperArmR, TEXT("upperarm_r")}, {EJoint::ForearmR, TEXT("lowerarm_r")}, {EJoint::HandR, TEXT("hand_r")},
		{EJoint::ThighL, TEXT("thigh_l")}, {EJoint::ShinL, TEXT("calf_l")}, {EJoint::FootL, TEXT("foot_l")},
		{EJoint::ThighR, TEXT("thigh_r")}, {EJoint::ShinR, TEXT("calf_r")}, {EJoint::FootR, TEXT("foot_r")},
	};

	struct FHalfJointName
	{
		const TCHAR* Name;
		EJoint Upper, Lower;
		// A twist at this joint runs along the bone between Lower and Tip.
		EJoint Tip;
	};

	// Tools/blender/fighter_joints.py builds these.
	static const FHalfJointName HalfJointNames[] = {
		{TEXT("shoulder_half_l"), EJoint::ClavL, EJoint::UpperArmL, EJoint::ForearmL}, {TEXT("elbow_half_l"), EJoint::UpperArmL, EJoint::ForearmL, EJoint::HandL}, {TEXT("wrist_half_l"), EJoint::ForearmL, EJoint::HandL, EJoint::ForearmL},
		{TEXT("shoulder_half_r"), EJoint::ClavR, EJoint::UpperArmR, EJoint::ForearmR}, {TEXT("elbow_half_r"), EJoint::UpperArmR, EJoint::ForearmR, EJoint::HandR}, {TEXT("wrist_half_r"), EJoint::ForearmR, EJoint::HandR, EJoint::ForearmR},
		{TEXT("hip_half_l"), EJoint::Hips, EJoint::ThighL, EJoint::ShinL}, {TEXT("knee_half_l"), EJoint::ThighL, EJoint::ShinL, EJoint::FootL}, {TEXT("ankle_half_l"), EJoint::ShinL, EJoint::FootL, EJoint::ShinL},
		{TEXT("hip_half_r"), EJoint::Hips, EJoint::ThighR, EJoint::ShinR}, {TEXT("knee_half_r"), EJoint::ThighR, EJoint::ShinR, EJoint::FootR}, {TEXT("ankle_half_r"), EJoint::ShinR, EJoint::FootR, EJoint::ShinR},
	};

	bool FMeshRig::Init(const FReferenceSkeleton& Ref, double Height, FString& Error)
	{
		H = Height;
		const int32 N = Ref.GetNum();
		Parent.SetNum(N);
		RefLocal.SetNum(N);
		RefCS.SetNum(N);
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		for (int32 B = 0; B < N; ++B)
		{
			Parent[B] = Ref.GetParentIndex(B);
			RefLocal[B] = Pose[B];
			RefCS[B] = Parent[B] >= 0 ? RefLocal[B] * RefCS[Parent[B]] : RefLocal[B];
		}
		auto Find = [&](const TCHAR* Name) { return Ref.FindBoneIndex(FName(Name)); };
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			BoneOf[J] = INDEX_NONE;
		}
		for (const FBoneNames& BN : JointBones)
		{
			BoneOf[(int32)BN.Joint] = Find(BN.Name);
			if (BoneOf[(int32)BN.Joint] == INDEX_NONE)
			{
				Error = FString::Printf(TEXT("bone %s missing"), BN.Name);
				return false;
			}
		}
		HalfJoints.Reset();
		for (const FHalfJointName& HN : HalfJointNames)
		{
			const int32 Bone = Find(HN.Name);
			if (Bone != INDEX_NONE)
			{
				const FVector Along = RefCS[BoneOf[(int32)HN.Tip]].GetLocation() - RefCS[BoneOf[(int32)HN.Lower]].GetLocation();
				HalfJoints.Add({Bone, HN.Upper, HN.Lower, Along.GetSafeNormal()});
			}
		}
		auto CS = [&](EJoint J) { return RefCS[BoneOf[(int32)J]].GetLocation(); };
		// The mesh's own axes: up is +Z; right runs from the left shoulder to the right; forward is
		// where the toes point.
		Up = FVector(0, 0, 1);
		FVector R = CS(EJoint::UpperArmR) - CS(EJoint::UpperArmL);
		R.Z = 0;
		Right = R.GetSafeNormal();
		Fwd = FVector::CrossProduct(Up, Right).GetSafeNormal();
		const int32 Ball = Find(TEXT("ball_l"));
		if (Ball != INDEX_NONE)
		{
			FVector Toe = RefCS[Ball].GetLocation() - CS(EJoint::FootL);
			Toe.Z = 0;
			if (FVector::DotProduct(Toe, Fwd) < 0)
			{
				Fwd = -Fwd;
			}
		}
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			RestPos[J] = RestRig(BoneOf[J]);
		}
		auto P = [&](EJoint J) { return RestPos[(int32)J]; };
		Lens[0] = 0.5 * (FVector::Dist(P(EJoint::UpperArmL), P(EJoint::ForearmL)) + FVector::Dist(P(EJoint::UpperArmR), P(EJoint::ForearmR)));
		Lens[1] = 0.5 * (FVector::Dist(P(EJoint::ForearmL), P(EJoint::HandL)) + FVector::Dist(P(EJoint::ForearmR), P(EJoint::HandR)));
		Lens[2] = 0.5 * (FVector::Dist(P(EJoint::ThighL), P(EJoint::ShinL)) + FVector::Dist(P(EJoint::ThighR), P(EJoint::ShinR)));
		Lens[3] = 0.5 * (FVector::Dist(P(EJoint::ShinL), P(EJoint::FootL)) + FVector::Dist(P(EJoint::ShinR), P(EJoint::FootR)));
		AnkleH = 0.5 * (P(EJoint::FootL).Y + P(EJoint::FootR).Y);
		// Bind frames: spine, head and collarbones unturned; limbs aimed along the mesh's own bones
		// with the bend axes the poses use (elbows bend backward, knees forward).
		for (FQuat& Q : BindFrame)
		{
			Q = FQuat::Identity;
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const EJoint Up_ = Side == 0 ? EJoint::UpperArmL : EJoint::UpperArmR;
			const EJoint Fore = Side == 0 ? EJoint::ForearmL : EJoint::ForearmR;
			const EJoint Hand = Side == 0 ? EJoint::HandL : EJoint::HandR;
			FVector Mid, End, Axis;
			FRig::TwoBone(P(Up_), P(Hand), P(Fore) + FVector(0, 0, 0.25), Lens[0], Lens[1], Mid, End, Axis);
			BindFrame[(int32)Up_] = FRig::AimBasis(P(Fore) - P(Up_), Axis);
			BindFrame[(int32)Fore] = FRig::AimBasis(P(Hand) - P(Fore), Axis);
			BindFrame[(int32)Hand] = BindFrame[(int32)Fore];
			const EJoint Th = Side == 0 ? EJoint::ThighL : EJoint::ThighR;
			const EJoint Sh = Side == 0 ? EJoint::ShinL : EJoint::ShinR;
			const EJoint Ft = Side == 0 ? EJoint::FootL : EJoint::FootR;
			FRig::TwoBone(P(Th), P(Ft), P(Sh) + FVector(0, 0, -0.25), Lens[2], Lens[3], Mid, End, Axis);
			BindFrame[(int32)Th] = FRig::AimBasis(P(Sh) - P(Th), Axis);
			BindFrame[(int32)Sh] = FRig::AimBasis(P(Ft) - P(Sh), Axis);
		}
		// Fingers curl toward the palm by the pose's fist value.
		Fingers.Reset();
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const TCHAR* S = Side == 0 ? TEXT("_l") : TEXT("_r");
			const int32 HandB = BoneOf[(int32)(Side == 0 ? EJoint::HandL : EJoint::HandR)];
			const int32 Idx1 = Find(*(FString(TEXT("index_01")) + S));
			const int32 Pin1 = Find(*(FString(TEXT("pinky_01")) + S));
			const int32 Mid1 = Find(*(FString(TEXT("middle_01")) + S));
			const int32 Mid2 = Find(*(FString(TEXT("middle_02")) + S));
			const int32 Th2 = Find(*(FString(TEXT("thumb_02")) + S));
			if (Idx1 == INDEX_NONE || Pin1 == INDEX_NONE || Mid1 == INDEX_NONE || Mid2 == INDEX_NONE || Th2 == INDEX_NONE)
			{
				continue;
			}
			const FVector Across = (RefCS[Pin1].GetLocation() - RefCS[Idx1].GetLocation()).GetSafeNormal();
			const FVector Along = (RefCS[Mid2].GetLocation() - RefCS[Mid1].GetLocation()).GetSafeNormal();
			FVector Palm = FVector::CrossProduct(Across, Along).GetSafeNormal();
			// The thumb sits on the palm's side of the knuckles.
			if (FVector::DotProduct(RefCS[Th2].GetLocation() - RefCS[Idx1].GetLocation(), Palm) < 0)
			{
				Palm = -Palm;
			}
			(void)HandB;
			static const TCHAR* Names[] = {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb")};
			static const double Fist[2][3] = {{1.35, 1.6, 1.15}, {0.35, 0.6, 0.75}};
			for (int32 F = 0; F < 5; ++F)
			{
				for (int32 K = 1; K <= 3; ++K)
				{
					const int32 B = Find(*FString::Printf(TEXT("%s_%02d%s"), Names[F], K, S));
					if (B == INDEX_NONE)
					{
						continue;
					}
					const int32 Next = Find(*FString::Printf(TEXT("%s_%02d%s"), Names[F], K + 1, S));
					const int32 Prev = Find(*FString::Printf(TEXT("%s_%02d%s"), Names[F], K - 1, S));
					FVector Dir = Next != INDEX_NONE ? RefCS[Next].GetLocation() - RefCS[B].GetLocation()
						: (Prev != INDEX_NONE ? RefCS[B].GetLocation() - RefCS[Prev].GetLocation() : Along);
					Dir = Dir.GetSafeNormal();
					const FVector AxisCS = FVector::CrossProduct(Dir, Palm).GetSafeNormal();
					FFinger Fg;
					Fg.Bone = B;
					Fg.Side = Side;
					Fg.AxisLocal = RefCS[B].GetRotation().Inverse().RotateVector(AxisCS);
					Fg.FistRad = Fist[F == 4 ? 1 : 0][K - 1];
					Fingers.Add(Fg);
				}
			}
		}
		bValid = true;
		return true;
	}

	void FMeshRig::ConfigureRig(FRig& Rig) const
	{
		auto P = [&](EJoint J) { return RestPos[(int32)J]; };
		Rig.H = H;
		Rig.UpperLen = Lens[0];
		Rig.ForeLen = Lens[1];
		Rig.ThighLen = Lens[2];
		Rig.ShinLen = Lens[3];
		Rig.AnkleH = AnkleH;
		Rig.ShinBind[0] = BindFrame[(int32)EJoint::ShinL];
		Rig.ShinBind[1] = BindFrame[(int32)EJoint::ShinR];
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			const EJoint Par = JointParent((EJoint)J);
			Rig.Rest[J] = Par == EJoint::Count ? P((EJoint)J) : P((EJoint)J) - P(Par);
		}
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			Rig.Xf[J] = FXf(FQuat::Identity, P((EJoint)J));
		}
	}

	FQuat FMeshRig::RigToMesh(const FQuat& Q) const
	{
		const FVector X = MapV(Q.RotateVector(UnmapV(FVector(1, 0, 0))));
		const FVector Y = MapV(Q.RotateVector(UnmapV(FVector(0, 1, 0))));
		const FVector Z = MapV(Q.RotateVector(UnmapV(FVector(0, 0, 1))));
		return FromAxes(X, Y, Z);
	}

	FQuat FMeshRig::HalfTurn(const FQuat& Turn, const FVector& TwistAxis)
	{
		FQuat Swing, Twist;
		Turn.ToSwingTwist(TwistAxis, Swing, Twist);
		if (Twist.W < 0.0)
		{
			Twist = -1.0 * Twist;
		}
		// A half turn of a twist flips sides as the twist passes 180 degrees, so the share eases to
		// nothing before it gets there: the joint then wrings a little instead of popping.
		const double Angle = 2.0 * FMath::Atan2(FVector::DotProduct(FVector(Twist.X, Twist.Y, Twist.Z), TwistAxis), Twist.W);
		const double Share = 0.5 * (1.0 - FMath::SmoothStep(FMath::DegreesToRadians(110.0), UE_DOUBLE_PI, FMath::Abs(Angle)));
		return FQuat::Slerp(FQuat::Identity, Swing, 0.5) * FQuat(TwistAxis, Angle * Share);
	}

	void FMeshRig::HalfJointSpread(const FRig& Rig, int32 Index, double& OutSwing, double& OutTwist) const
	{
		const FHalfJoint& Half = HalfJoints[Index];
		const FQuat TurnU = RigToMesh(Rig.J(Half.Upper).Q * BindFrame[(int32)Half.Upper].Inverse());
		const FQuat TurnL = RigToMesh(Rig.J(Half.Lower).Q * BindFrame[(int32)Half.Lower].Inverse());
		FQuat Swing, Twist;
		(TurnU.Inverse() * TurnL).GetNormalized().ToSwingTwist(Half.TwistAxis, Swing, Twist);
		OutSwing = FMath::RadiansToDegrees(Swing.GetAngle());
		OutTwist = FMath::RadiansToDegrees(Twist.GetAngle());
		OutSwing = OutSwing > 180.0 ? 360.0 - OutSwing : OutSwing;
		OutTwist = OutTwist > 180.0 ? 360.0 - OutTwist : OutTwist;
	}

	void FMeshRig::HalfJointBones(int32 Index, int32& OutBone, int32& OutUpper, int32& OutLower) const
	{
		const FHalfJoint& Half = HalfJoints[Index];
		OutBone = Half.Bone;
		OutUpper = BoneOf[(int32)Half.Upper];
		OutLower = BoneOf[(int32)Half.Lower];
	}

	FTransform FMeshRig::ComponentTransform(const FXf& Root, const FVector& WorldScale) const
	{
		const FVector X = DirToUE(Root.Q.RotateVector(UnmapV(FVector(1, 0, 0))));
		const FVector Y = DirToUE(Root.Q.RotateVector(UnmapV(FVector(0, 1, 0))));
		const FVector Z = DirToUE(Root.Q.RotateVector(UnmapV(FVector(0, 0, 1))));
		const double Hs = 0.5 * (WorldScale.X + WorldScale.Z);
		return FTransform(FromAxes(X, Y, Z), WorldToUE(Root.P), FVector(Hs, Hs, WorldScale.Y));
	}

	void FMeshRig::Pose(const FRig& Rig, TArray<FTransform>& OutLocal) const
	{
		const int32 N = RefLocal.Num();
		TArray<FTransform, TInlineAllocator<96>> CS;
		CS.SetNum(N);
		TArray<int32, TInlineAllocator<96>> JointOfBone;
		JointOfBone.Init(-1, N);
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			JointOfBone[BoneOf[J]] = J;
		}
		TArray<FQuat, TInlineAllocator<96>> Curl;
		Curl.Init(FQuat::Identity, N);
		for (const FFinger& F : Fingers)
		{
			Curl[F.Bone] = FQuat(F.AxisLocal, F.FistRad * FMath::Clamp(Rig.Fist[F.Side], 0.0, 1.0));
		}
		for (int32 B = 0; B < N; ++B)
		{
			const int32 J = JointOfBone[B];
			if (J >= 0)
			{
				const FQuat Delta = RigToMesh(Rig.Xf[J].Q * BindFrame[J].Inverse());
				CS[B] = FTransform(Delta * RefCS[B].GetRotation(), RigToMeshPos(Rig.Xf[J].P));
			}
			else
			{
				FTransform L = RefLocal[B];
				L.SetRotation(L.GetRotation() * Curl[B]);
				CS[B] = Parent[B] >= 0 ? L * CS[Parent[B]] : L;
			}
		}
		for (const FHalfJoint& Half : HalfJoints)
		{
			const int32 U = BoneOf[(int32)Half.Upper], L = BoneOf[(int32)Half.Lower];
			const FQuat TurnU = CS[U].GetRotation() * RefCS[U].GetRotation().Inverse();
			const FQuat TurnL = CS[L].GetRotation() * RefCS[L].GetRotation().Inverse();
			const FQuat Turn = TurnU * HalfTurn((TurnU.Inverse() * TurnL).GetNormalized(), Half.TwistAxis);
			CS[Half.Bone] = FTransform(Turn * RefCS[Half.Bone].GetRotation(), CS[L].GetLocation());
		}
		OutLocal.SetNum(N);
		for (int32 B = 0; B < N; ++B)
		{
			OutLocal[B] = Parent[B] >= 0 ? CS[B].GetRelativeTransform(CS[Parent[B]]) : CS[B];
			OutLocal[B].SetScale3D(RefLocal[B].GetScale3D());
		}
	}
}
