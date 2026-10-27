#include "Anim/SlopPose.h"

namespace Slop::Anim
{
	static const TCHAR* RotKeys[] = {TEXT("hips_rot"), TEXT("spine"), TEXT("chest"), TEXT("neck"), TEXT("head"), TEXT("clav_l"), TEXT("clav_r"), TEXT("wrist_l"), TEXT("wrist_r")};
	static const TCHAR* PosKeys[] = {TEXT("hips"), TEXT("hand_l"), TEXT("hand_r"), TEXT("elbow_l"), TEXT("elbow_r"), TEXT("foot_l"), TEXT("foot_r"),
		TEXT("knee_l"), TEXT("knee_r"), TEXT("fingers_l"), TEXT("fingers_r"), TEXT("palm_l"), TEXT("palm_r")};
	static const TCHAR* ValKeys[] = {TEXT("fist_l"), TEXT("fist_r"), TEXT("yaw_l"), TEXT("yaw_r"), TEXT("toe_l"), TEXT("toe_r"), TEXT("free_l"), TEXT("free_r"),
		TEXT("prop"), TEXT("staff"), TEXT("glow"), TEXT("aim_l"), TEXT("aim_r"), TEXT("eye_alt")};
	static_assert(UE_ARRAY_COUNT(RotKeys) == (int32)ERot::Count, "rot keys");
	static_assert(UE_ARRAY_COUNT(PosKeys) == (int32)EPos::Count, "pos keys");
	static_assert(UE_ARRAY_COUNT(ValKeys) == (int32)EVal::Count, "val keys");
	// Left-right partner of each key, for mirroring.
	static const int32 RotSwap[] = {0, 1, 2, 3, 4, 6, 5, 8, 7};
	static const int32 PosSwap[] = {0, 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11};
	static const int32 ValSwap[] = {1, 0, 3, 2, 5, 4, 7, 6, 8, 9, 10, 12, 11, 13};
	static bool IsDirKey(int32 I) { return I >= (int32)EPos::FingersL; }

	FQuat AxisAngle(const FVector& Axis, double Rad) { return FQuat(Axis.GetSafeNormal(), Rad); }

	// Godot's Quaternion.from_euler: YXZ order, i.e. q = Y(y) * X(x) * Z(z).
	FQuat EulerDeg(double X, double Y, double Z)
	{
		return AxisAngle(FVector(0, 1, 0), FMath::DegreesToRadians(Y)) * AxisAngle(FVector(1, 0, 0), FMath::DegreesToRadians(X))
			* AxisAngle(FVector(0, 0, 1), FMath::DegreesToRadians(Z));
	}

	// A rotation from its column axes (Godot's Basis(x, y, z)).
	FQuat FromAxes(const FVector& X, const FVector& Y, const FVector& Z)
	{
		FMatrix M(FPlane(X.X, X.Y, X.Z, 0), FPlane(Y.X, Y.Y, Y.Z, 0), FPlane(Z.X, Z.Y, Z.Z, 0), FPlane(0, 0, 0, 1));
		FQuat Q(M);
		Q.Normalize();
		return Q;
	}

	// --- Pose ------------------------------------------------------------------------------------------

	FPose::FPose()
	{
		for (FQuat& Q : Rot)
		{
			Q = FQuat::Identity;
		}
		P(EPos::Hips) = FVector::ZeroVector;
		P(EPos::HandL) = FVector(-0.25, 0.85, 0.0);
		P(EPos::HandR) = FVector(0.25, 0.85, 0.0);
		P(EPos::ElbowL) = FVector(-0.5, 1.1, 0.4);
		P(EPos::ElbowR) = FVector(0.5, 1.1, 0.4);
		P(EPos::FootL) = FVector(-0.12, 0.0, 0.0);
		P(EPos::FootR) = FVector(0.12, 0.0, 0.0);
		P(EPos::KneeL) = FVector(-0.15, 0.5, -0.8);
		P(EPos::KneeR) = FVector(0.15, 0.5, -0.8);
		P(EPos::FingersL) = FVector(0, -1, 0);
		P(EPos::FingersR) = FVector(0, -1, 0);
		P(EPos::PalmL) = FVector(1, 0, 0);
		P(EPos::PalmR) = FVector(-1, 0, 0);
		for (double& V : Val)
		{
			V = 0.0;
		}
		V(EVal::FistL) = 1.0;
		V(EVal::FistR) = 1.0;
	}

	FPose FPose::FromJson(const TSharedPtr<FJsonObject>& D, const FPose* Base, double Scale)
	{
		FPose Out = Base ? *Base : FPose();
		if (!D.IsValid())
		{
			return Out;
		}
		for (int32 I = 0; I < (int32)ERot::Count; ++I)
		{
			if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(D, RotKeys[I]))
			{
				if (A->Num() == 3)
				{
					Out.Rot[I] = EulerDeg((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
				}
			}
		}
		for (int32 I = 0; I < (int32)EPos::Count; ++I)
		{
			if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(D, PosKeys[I]))
			{
				if (A->Num() == 3)
				{
					// Data [forward, up, right] to rig space (x right, y up, -z forward).
					const double K = IsDirKey(I) ? 1.0 : Scale;
					Out.Pos[I] = FVector((*A)[2]->AsNumber(), (*A)[1]->AsNumber(), -(*A)[0]->AsNumber()) * K;
				}
			}
		}
		for (int32 I = 0; I < (int32)EVal::Count; ++I)
		{
			double V;
			if (D->TryGetNumberField(ValKeys[I], V))
			{
				Out.Val[I] = V;
			}
		}
		if (D->HasField(TEXT("fingers_l")) && !D->HasField(TEXT("aim_l")))
		{
			Out.V(EVal::AimL) = 1.0;
		}
		if (D->HasField(TEXT("fingers_r")) && !D->HasField(TEXT("aim_r")))
		{
			Out.V(EVal::AimR) = 1.0;
		}
		return Out;
	}

	void FPose::MirrorInto(FPose& Out) const
	{
		for (int32 I = 0; I < (int32)ERot::Count; ++I)
		{
			const FQuat& Q = Rot[I];
			Out.Rot[RotSwap[I]] = FQuat(Q.X, -Q.Y, -Q.Z, Q.W);
		}
		for (int32 I = 0; I < (int32)EPos::Count; ++I)
		{
			const FVector& V = Pos[I];
			Out.Pos[PosSwap[I]] = FVector(-V.X, V.Y, V.Z);
		}
		for (int32 I = 0; I < (int32)EVal::Count; ++I)
		{
			// A foot turned one way turns the other way in the mirror.
			const bool bYaw = I == (int32)EVal::YawL || I == (int32)EVal::YawR;
			Out.Val[ValSwap[I]] = bYaw ? -Val[I] : Val[I];
		}
	}

	void FPose::BlendInto(const FPose& A, const FPose& B, double T)
	{
		for (int32 I = 0; I < (int32)ERot::Count; ++I)
		{
			Rot[I] = FQuat::Slerp(A.Rot[I], B.Rot[I], T);
		}
		for (int32 I = 0; I < (int32)EPos::Count; ++I)
		{
			Pos[I] = FMath::Lerp(A.Pos[I], B.Pos[I], T);
		}
		for (int32 I = 0; I < (int32)EVal::Count; ++I)
		{
			Val[I] = FMath::Lerp(A.Val[I], B.Val[I], T);
		}
	}

	double FPose::Ease(FName Kind, double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		static const FName Linear(TEXT("linear")), In(TEXT("in")), Out(TEXT("out")), Snap(TEXT("snap")), Hold(TEXT("hold"));
		if (Kind == Linear) return T;
		if (Kind == In) return T * T;
		if (Kind == Out) return 1.0 - (1.0 - T) * (1.0 - T);
		if (Kind == Snap) return 1.0 - FMath::Pow(1.0 - T, 3.0);
		if (Kind == Hold) return T < 1.0 ? 0.0 : 1.0;
		return T * T * (3.0 - 2.0 * T);
	}

	// --- Pose library --------------------------------------------------------------------------------------

	static const TCHAR* PatchedPoses[] = {TEXT("guard_hit"), TEXT("guard_hit_low"), TEXT("hit_head"), TEXT("hit_body"), TEXT("hit_gut"), TEXT("hit_low"),
		TEXT("hit_back"), TEXT("parried"), TEXT("crumple"), TEXT("throw_break"), TEXT("thrown_hold"), TEXT("rise_2")};

	void FPoseLibrary::Build(const FFighterDef& Def, const TSharedPtr<FJsonObject>& InCommon)
	{
		Common = InCommon;
		CommonScale = Def.Height / 1.8;
		const TSharedPtr<FJsonObject> Patch = Json::Obj(Def.Raw, TEXT("pose_patch"));
		if (Common.IsValid())
		{
			for (const auto& KV : Common->Values)
			{
				const FString Key(*KV.Key);
				TSharedPtr<FJsonObject> D = KV.Value->AsObject();
				bool bPatched = false;
				for (const TCHAR* P : PatchedPoses)
				{
					bPatched |= Key == P;
				}
				if (Patch.IsValid() && bPatched)
				{
					TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
					Merged->Values = D->Values;
					for (const auto& PV : Patch->Values)
					{
						Merged->SetField(FString(*PV.Key), PV.Value);
					}
					D = Merged;
				}
				Src.Add(FName(*Key), D);
				SrcScale.Add(FName(*Key), CommonScale);
			}
		}
		if (Def.Poses.IsValid())
		{
			for (const auto& KV : Def.Poses->Values)
			{
				Src.Add(FName(*KV.Key), KV.Value->AsObject());
				SrcScale.Add(FName(*KV.Key), 1.0);
			}
		}
		TArray<FName> Names;
		Src.GetKeys(Names);
		for (const FName& N : Names)
		{
			Get(N);
		}
		for (const auto& KV : Def.Moves)
		{
			TArray<FPoseKey>& Keys = MoveKeyMap.Add(KV.Key);
			this->Keys(KV.Value->AnimKeys, Keys);
		}
	}

	const FPose& FPoseLibrary::Get(FName Name)
	{
		if (const TUniquePtr<FPose>* Found = Poses.Find(Name))
		{
			return **Found;
		}
		const TSharedPtr<FJsonObject>* DP = Src.Find(Name);
		if (!DP)
		{
			MissingNames.AddUnique(Name);
			const FName Stance(TEXT("stance"));
			return (Name != Stance && Src.Contains(Stance)) ? Get(Stance) : Default;
		}
		const TSharedPtr<FJsonObject> D = *DP;
		const double Scale = SrcScale.FindRef(Name);
		FPose P;
		FString Mirror, BaseName;
		if (D->TryGetStringField(TEXT("mirror"), Mirror))
		{
			FPose M;
			Get(FName(*Mirror)).MirrorInto(M);
			P = FPose::FromJson(D, &M, Scale);
		}
		else if (D->TryGetStringField(TEXT("base"), BaseName))
		{
			// A fighter's pose named like a shared one may refine that shared pose.
			const FPose& Base = BaseName == Name.ToString() ? CommonPose(Name) : Get(FName(*BaseName));
			P = FPose::FromJson(D, &Base, Scale);
		}
		else
		{
			P = FPose::FromJson(D, nullptr, Scale);
		}
		TUniquePtr<FPose>& Slot = Poses.Add(Name, MakeUnique<FPose>(P));
		return *Slot;
	}

	const FPose& FPoseLibrary::CommonPose(FName Name)
	{
		if (const TUniquePtr<FPose>* Found = CommonCache.Find(Name))
		{
			return **Found;
		}
		const TSharedPtr<FJsonObject> D = Json::Obj(Common, *Name.ToString());
		FPose P;
		FString BaseName;
		if (D.IsValid() && D->TryGetStringField(TEXT("base"), BaseName))
		{
			const FPose& Base = BaseName == Name.ToString() ? Default : Get(FName(*BaseName));
			P = FPose::FromJson(D, &Base, CommonScale);
		}
		else
		{
			P = FPose::FromJson(D, nullptr, CommonScale);
		}
		TUniquePtr<FPose>& Slot = CommonCache.Add(Name, MakeUnique<FPose>(P));
		return *Slot;
	}

	void FPoseLibrary::Keys(const TArray<FAnimKey>& Raw, TArray<FPoseKey>& Out)
	{
		Out.Reset();
		for (const FAnimKey& K : Raw)
		{
			Out.Add({(double)K.Frame, &Get(K.Pose), K.Ease});
		}
	}

	void FPoseLibrary::Sample(const TArray<FPoseKey>& Keys, double T, const FPose& From, FPose& Out)
	{
		if (Keys.Num() == 0)
		{
			Out = From;
			return;
		}
		double PrevF = 0.0;
		const FPose* PrevP = &From;
		for (const FPoseKey& K : Keys)
		{
			if (T <= K.F)
			{
				const double Span = FMath::Max(K.F - PrevF, 0.001);
				Out.BlendInto(*PrevP, *K.Pose, FPose::Ease(K.Ease, (T - PrevF) / Span));
				return;
			}
			PrevF = K.F;
			PrevP = K.Pose;
		}
		Out = *PrevP;
	}

	// --- Rig --------------------------------------------------------------------------------------------------

	const TCHAR* JointName(EJoint J)
	{
		static const TCHAR* Names[] = {TEXT("hips"), TEXT("spine"), TEXT("chest"), TEXT("neck"), TEXT("head"), TEXT("clav_l"), TEXT("upperarm_l"),
			TEXT("forearm_l"), TEXT("hand_l"), TEXT("clav_r"), TEXT("upperarm_r"), TEXT("forearm_r"), TEXT("hand_r"), TEXT("thigh_l"), TEXT("shin_l"),
			TEXT("foot_l"), TEXT("thigh_r"), TEXT("shin_r"), TEXT("foot_r")};
		return Names[(int32)J];
	}

	EJoint JointParent(EJoint J)
	{
		switch (J)
		{
		case EJoint::Spine: return EJoint::Hips;
		case EJoint::Chest: return EJoint::Spine;
		case EJoint::Neck: return EJoint::Chest;
		case EJoint::Head: return EJoint::Neck;
		case EJoint::ClavL: case EJoint::ClavR: return EJoint::Chest;
		case EJoint::UpperArmL: return EJoint::ClavL;
		case EJoint::ForearmL: return EJoint::UpperArmL;
		case EJoint::HandL: return EJoint::ForearmL;
		case EJoint::UpperArmR: return EJoint::ClavR;
		case EJoint::ForearmR: return EJoint::UpperArmR;
		case EJoint::HandR: return EJoint::ForearmR;
		case EJoint::ThighL: case EJoint::ThighR: return EJoint::Hips;
		case EJoint::ShinL: return EJoint::ThighL;
		case EJoint::FootL: return EJoint::ShinL;
		case EJoint::ShinR: return EJoint::ThighR;
		case EJoint::FootR: return EJoint::ShinR;
		default: return EJoint::Count;
		}
	}

	void FRig::Build(const FFighterDef& Def)
	{
		H = Def.Height;
		const TSharedPtr<FJsonObject> B = Json::Obj(Def.Look, TEXT("build"));
		const double SW = Json::Num(B, TEXT("shoulders"), 1.0);
		const double HW = Json::Num(B, TEXT("hips"), 1.0);
		UpperLen = 0.172 * H;
		ForeLen = 0.152 * H;
		ThighLen = 0.232 * H;
		ShinLen = 0.238 * H;
		AnkleH = 0.042 * H;
		auto R = [this](EJoint J, double X, double Y, double Z) { Rest[(int32)J] = FVector(X, Y, Z); };
		R(EJoint::Hips, 0, 0.53 * H, 0);
		R(EJoint::Spine, 0, 0.05 * H, 0);
		R(EJoint::Chest, 0, 0.08 * H, 0);
		R(EJoint::Neck, 0, 0.185 * H, 0);
		R(EJoint::Head, 0, 0.04 * H, 0);
		R(EJoint::ClavL, -0.022 * H, 0.15 * H, 0.005 * H);
		R(EJoint::UpperArmL, -0.093 * H * SW, 0, 0);
		R(EJoint::ForearmL, 0, -UpperLen, 0);
		R(EJoint::HandL, 0, -ForeLen, 0);
		R(EJoint::ClavR, 0.022 * H, 0.15 * H, 0.005 * H);
		R(EJoint::UpperArmR, 0.093 * H * SW, 0, 0);
		R(EJoint::ForearmR, 0, -UpperLen, 0);
		R(EJoint::HandR, 0, -ForeLen, 0);
		R(EJoint::ThighL, -0.055 * H * HW, -0.025 * H, 0);
		R(EJoint::ShinL, 0, -ThighLen, 0);
		R(EJoint::FootL, 0, -ShinLen, 0);
		R(EJoint::ThighR, 0.055 * H * HW, -0.025 * H, 0);
		R(EJoint::ShinR, 0, -ThighLen, 0);
		R(EJoint::FootR, 0, -ShinLen, 0);
		for (int32 I = 0; I < (int32)EJoint::Count; ++I)
		{
			Xf[I] = FXf(FQuat::Identity, Rest[I]);
		}
	}

	void FRig::SetLocal(EJoint J, const FXf& Local)
	{
		const EJoint P = JointParent(J);
		Xf[(int32)J] = P == EJoint::Count ? Local : Xf[(int32)P] * Local;
	}

	void FRig::Apply(const FPose& P, const FVector Feet[2], const double FootYaw[2], const FQuat Extra[(int32)EJoint::Count])
	{
		SetLocal(EJoint::Hips, FXf(P.R(ERot::Hips) * Extra[(int32)EJoint::Hips], Rest[(int32)EJoint::Hips] + P.P(EPos::Hips)));
		const TPair<EJoint, ERot> Fk[] = {{EJoint::Spine, ERot::Spine}, {EJoint::Chest, ERot::Chest}, {EJoint::Neck, ERot::Neck},
			{EJoint::Head, ERot::Head}, {EJoint::ClavL, ERot::ClavL}, {EJoint::ClavR, ERot::ClavR}};
		for (const TPair<EJoint, ERot>& F : Fk)
		{
			SetLocal(F.Key, FXf(P.R(F.Value) * Extra[(int32)F.Key], Rest[(int32)F.Key]));
		}
		Arm(0, P);
		Arm(1, P);
		Leg(0, Feet[0], P.P(EPos::KneeL), FootYaw[0], P.V(EVal::FreeL), P.V(EVal::ToeL));
		Leg(1, Feet[1], P.P(EPos::KneeR), FootYaw[1], P.V(EVal::FreeR), P.V(EVal::ToeR));
		Fist[0] = P.V(EVal::FistL);
		Fist[1] = P.V(EVal::FistR);
	}

	FXf FRig::TorsoXf(const FPose& P, const FQuat* Extra) const
	{
		const auto E = [Extra](EJoint J) { return Extra ? Extra[(int32)J] : FQuat::Identity; };
		return FXf(P.R(ERot::Hips) * E(EJoint::Hips), Rest[(int32)EJoint::Hips] + P.P(EPos::Hips))
			* FXf(P.R(ERot::Spine) * E(EJoint::Spine), Rest[(int32)EJoint::Spine])
			* FXf(P.R(ERot::Chest) * E(EJoint::Chest), Rest[(int32)EJoint::Chest]);
	}

	void FRig::Arm(int32 Side, const FPose& P)
	{
		const EJoint Clav = Side == 0 ? EJoint::ClavL : EJoint::ClavR;
		const EJoint Up = Side == 0 ? EJoint::UpperArmL : EJoint::UpperArmR;
		const EJoint Fore = Side == 0 ? EJoint::ForearmL : EJoint::ForearmR;
		const EJoint Hand = Side == 0 ? EJoint::HandL : EJoint::HandR;
		const FVector Shoulder = Xf[(int32)Clav] * Rest[(int32)Up];
		const FVector Target = P.P(Side == 0 ? EPos::HandL : EPos::HandR);
		const FVector Pole = P.P(Side == 0 ? EPos::ElbowL : EPos::ElbowR);
		FVector Elbow, Wrist, Axis;
		TwoBone(Shoulder, Target, Pole, UpperLen, ForeLen, Elbow, Wrist, Axis);
		Bend[Side] = Axis;
		SetGlobal(Up, FXf(AimBasis(Elbow - Shoulder, Axis), Shoulder));
		SetGlobal(Fore, FXf(AimBasis(Wrist - Elbow, Axis), Elbow));
		FQuat HB = Xf[(int32)Fore].Q * P.R(Side == 0 ? ERot::WristL : ERot::WristR);
		const double Aim = P.V(Side == 0 ? EVal::AimL : EVal::AimR);
		if (Aim > 0.0)
		{
			const FVector Y = -P.P(Side == 0 ? EPos::FingersL : EPos::FingersR).GetSafeNormal();
			FVector Z = P.P(Side == 0 ? EPos::PalmL : EPos::PalmR);
			Z = (Z - Y * FVector::DotProduct(Z, Y)).GetSafeNormal();
			if (Z.Size() > 0.5)
			{
				const FQuat Explicit = FromAxes(FVector::CrossProduct(Y, Z), Y, Z);
				HB = FQuat::Slerp(HB.GetNormalized(), Explicit, FMath::Clamp(Aim, 0.0, 1.0));
			}
		}
		SetGlobal(Hand, FXf(HB, Wrist));
	}

	void FRig::Leg(int32 Side, const FVector& AnkleTarget, const FVector& Pole, double Yaw, double Free, double ToeDeg)
	{
		const EJoint Thigh = Side == 0 ? EJoint::ThighL : EJoint::ThighR;
		const EJoint Shin = Side == 0 ? EJoint::ShinL : EJoint::ShinR;
		const EJoint Foot = Side == 0 ? EJoint::FootL : EJoint::FootR;
		const FVector Hip = Xf[(int32)EJoint::Hips] * Rest[(int32)Thigh];
		FVector Knee, Ankle, Axis;
		TwoBone(Hip, AnkleTarget, Pole, ThighLen, ShinLen, Knee, Ankle, Axis);
		Bend[2 + Side] = Axis;
		const FQuat ShinB = AimBasis(Ankle - Knee, Axis);
		SetGlobal(Thigh, FXf(AimBasis(Knee - Hip, Axis), Hip));
		SetGlobal(Shin, FXf(ShinB, Knee));
		const FQuat Flat = AxisAngle(FVector(0, 1, 0), Yaw);
		const FQuat Pointed = ShinB * AxisAngle(FVector(1, 0, 0), FMath::DegreesToRadians(-ToeDeg));
		SetGlobal(Foot, FXf(FQuat::Slerp(Flat, Pointed, FMath::Clamp(Free, 0.0, 1.0)), Ankle));
	}

	void FRig::TwoBone(const FVector& Root, const FVector& Target, const FVector& Pole, double A, double B, FVector& Mid, FVector& End, FVector& Axis)
	{
		const FVector To = Target - Root;
		const double D = FMath::Clamp(To.Size(), FMath::Abs(A - B) + 0.001, A + B - 0.0005);
		const FVector Dir = To.Size() > 1e-5 ? To.GetSafeNormal() : FVector(0, -1, 0);
		const double CosA = FMath::Clamp((A * A + D * D - B * B) / (2.0 * A * D), -1.0, 1.0);
		const double SinA = FMath::Sqrt(1.0 - CosA * CosA);
		const FVector PV = Pole - Root;
		FVector Perp = PV - Dir * FVector::DotProduct(PV, Dir);
		if (Perp.Size() < 1e-5)
		{
			const FVector Back(0, 0, 1);
			Perp = Back - Dir * FVector::DotProduct(Back, Dir);
		}
		Perp = Perp.GetSafeNormal();
		Mid = Root + Dir * (A * CosA) + Perp * (A * SinA);
		End = Root + Dir * D;
		Axis = FVector::CrossProduct(Perp, Dir).GetSafeNormal();
	}

	// Basis whose -Y points along `Along` and whose X is the bend axis.
	FQuat FRig::AimBasis(const FVector& Along, const FVector& BendAxis)
	{
		const FVector Y = -Along.GetSafeNormal();
		FVector X = BendAxis - Y * FVector::DotProduct(BendAxis, Y);
		if (X.Size() < 1e-5)
		{
			X = FVector(1, 0, 0) - Y * Y.X;
		}
		X = X.GetSafeNormal();
		const FVector Z = FVector::CrossProduct(X, Y);
		return FromAxes(X, Y, Z);
	}

	FVector FRig::HandTip(int32 Side) const
	{
		return Xf[(int32)(Side == 0 ? EJoint::HandL : EJoint::HandR)] * FVector(0, -0.045 * H, 0);
	}

	FVector FRig::FootTip(int32 Side) const
	{
		return Xf[(int32)(Side == 0 ? EJoint::FootL : EJoint::FootR)] * FVector(0, -0.015 * H, -0.06 * H);
	}
}
