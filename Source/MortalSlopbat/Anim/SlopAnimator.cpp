#include "Anim/SlopAnimator.h"

namespace Slop::Anim
{
	static constexpr double Tau = 2.0 * UE_DOUBLE_PI;

	static FQuat EulerRad(const FVector& V)
	{
		return AxisAngle(FVector(0, 1, 0), V.Y) * AxisAngle(FVector(1, 0, 0), V.X) * AxisAngle(FVector(0, 0, 1), V.Z);
	}

	static double MoveToward(double From, double To, double Delta)
	{
		return FMath::Abs(To - From) <= Delta ? To : From + FMath::Sign(To - From) * Delta;
	}

	static double AngleDifference(double From, double To)
	{
		const double D = FMath::Fmod(To - From, Tau);
		return FMath::Fmod(2.0 * D, Tau) - D;
	}

	static double LerpAngle(double From, double To, double W) { return From + AngleDifference(From, To) * W; }

	static bool In(EState S, std::initializer_list<EState> L)
	{
		for (EState X : L)
		{
			if (X == S)
			{
				return true;
			}
		}
		return false;
	}

	static const TCHAR* StunKey(EStun K)
	{
		switch (K)
		{
		case EStun::High: return TEXT("high");
		case EStun::Mid: return TEXT("mid");
		case EStun::Low: return TEXT("low");
		case EStun::Crouch: return TEXT("crouch");
		case EStun::Guard: return TEXT("guard");
		case EStun::CrouchGuard: return TEXT("crouch_guard");
		case EStun::Crumple: return TEXT("crumple");
		case EStun::Dazed: return TEXT("dazed");
		case EStun::Pulled: return TEXT("pulled");
		case EStun::Break: return TEXT("break");
		default: return TEXT("");
		}
	}

	static FName ReactionFor(EStun K)
	{
		switch (K)
		{
		case EStun::High: return TEXT("hit_head");
		case EStun::Crouch: return TEXT("hit_low");
		case EStun::Guard: return TEXT("block_hit");
		case EStun::CrouchGuard: return TEXT("block_low_hit");
		case EStun::Dazed: return TEXT("dazed");
		case EStun::Break: return TEXT("throw_break");
		case EStun::Crumple: return TEXT("hit_gut");
		default: return TEXT("hit_body");
		}
	}

	void FAnimator::Setup(const FFighter& InFighter, const TSharedPtr<FJsonObject>& CommonPoses)
	{
		Fighter = &InFighter;
		Lib.Build(*InFighter.Def, CommonPoses);
		Rig.Build(*InFighter.Def);
		for (FVector (&S)[2] : Spring)
		{
			S[0] = S[1] = FVector::ZeroVector;
		}
		Out = Lib.Get(TEXT("stance"));
		SyncFromSim(true);
		Update(1.0, 0.0);
	}

	void FAnimator::ResetFeet()
	{
		for (FFoot& F : Feet)
		{
			F.bValid = false;
		}
	}

	void FAnimator::SyncFromSim(bool bTeleport)
	{
		const FFighter& F = *Fighter;
		PrevPos = bTeleport ? F.Pos : CurPos;
		CurPos = F.Pos;
		if ((F.State == EState::Move || F.State == EState::Finisher) && (F.Move != LastMove || LastState != F.State))
		{
			MoveTick = F.Sim ? F.Sim->TickCount : 0;
		}
		LastState = F.State;
		LastMove = F.Move;
		if (bTeleport)
		{
			ResetFeet();
		}
	}

	void FAnimator::Update(double Frac, double Delta)
	{
		const FFighter& F = *Fighter;
		const bool bFrozen = F.Hitstop > 0;
		if (!bSmoothing || bFrozen)
		{
			Frac = 1.0;
		}
		Time += Delta;
		const FVector2D P2 = FMath::Lerp(PrevPos, CurPos, Frac);
		FVector P(P2.X, P2.Y - Sink - MoveSink(Frac), 0.0);
		if (F.Facing != Facing)
		{
			Facing = F.Facing;
			ResetFeet();
		}
		if (bFrozen && Shake > 0.0)
		{
			P.X += FMath::Sin(Time * 95.0) * Shake;
		}
		if (Delta > 0.0)
		{
			VisVel = FMath::Lerp(VisVel, (P - LastRoot) / Delta, FMath::Clamp(Delta * 12.0, 0.0, 1.0));
		}
		LastRoot = P;
		// Basis.looking_at(facing, up): the rig's -z points along +x times facing.
		const FQuat Look = FromAxes(FVector(0, 0, Facing), FVector(0, 1, 0), FVector(-Facing, 0, 0));
		Root = FXf(Look, P + LiftExtra) * AirTilt(Delta, bFrozen);
		WorldScale = ExtraScale;
		EvaluateSource(Frac);
		if (Delta > 0.0)
		{
			BlendT += Delta * 60.0 * (bFrozen ? 0.0 : 1.0);
		}
		const double W = BlendLen > 0.0 ? FPose::Ease(TEXT("inout"), BlendT / BlendLen) : 1.0;
		Out.BlendInto(Snap, Src, W);
		FPose* ShownP = &Out;
		if (Facing < 0)
		{
			Out.MirrorInto(Mirror);
			ShownP = &Mirror;
		}
		// Life in the hips: a step's rise and fall while walking, a slow settle while standing. The
		// feet are planted, so only the body moves and the legs take up the difference.
		double Bob = 0.0;
		if (F.State == EState::WalkF || F.State == EState::WalkB)
		{
			const double Stride = 0.6 * Rig.H / 1.8;
			Bob = (FMath::Abs(FMath::Sin(FMath::Abs(P2.X) / Stride * UE_DOUBLE_PI)) - 0.5) * 0.02 * Rig.H;
		}
		else if (In(F.State, {EState::Idle, EState::Intro, EState::Win}))
		{
			Bob = FMath::Sin(Time * Tau * 1.1) * 0.004 * Rig.H;
		}
		ShownP->P(EPos::Hips).Y += Bob;
		FQuat Extra[(int32)EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		Procedural(Delta, bFrozen, Extra);
		FVector Ankles[2];
		double Yaws[2];
		PlantFeet(Delta, *ShownP, Ankles, Yaws);
		Rig.Apply(*ShownP, Ankles, Yaws, Extra);
		Glow = ShownP->V(EVal::Glow);
		if (FlashAmount > 0.0)
		{
			FlashAmount = FMath::Max(0.0, FlashAmount - Delta * 6.0);
		}
	}

	bool FAnimator::SetSource(const FString& InKey, double BlendTicks)
	{
		if (InKey == Key)
		{
			return false;
		}
		Snap = Out;
		Key = InKey;
		BlendT = 0.0;
		BlendLen = BlendTicks;
		return true;
	}

	void FAnimator::HoldPose(FName Name, double Blend)
	{
		const FName Nm = Lib.Has(Name) ? Name : FName(TEXT("stance"));
		SetSource(TEXT("pose:") + Nm.ToString(), Blend);
		Src = Lib.Get(Nm);
	}

	void FAnimator::EvaluateSource(double Frac)
	{
		const FFighter& F = *Fighter;
		const int32 SimTick = F.Sim ? F.Sim->TickCount : 0;
		const double T = double(F.SF) - 1.0 + Frac;
		if (!PoseOverride.IsNone())
		{
			HoldPose(PoseOverride, 6.0);
			return;
		}
		switch (F.State)
		{
		case EState::Idle:
		case EState::WalkF:
		case EState::WalkB:
			HoldPose(TEXT("stance"), 7.0);
			break;
		case EState::Intro:
			HoldPose(TEXT("intro"), 10.0);
			break;
		case EState::Win:
			HoldPose(TEXT("win"), 14.0);
			break;
		case EState::Crouch:
		case EState::PreJump:
		case EState::Land:
			HoldPose(TEXT("crouch"), F.State != EState::Crouch ? 3.0 : 4.0);
			break;
		case EState::Block:
			HoldPose(TEXT("block"), 3.0);
			break;
		case EState::CrouchBlock:
			HoldPose(TEXT("block_low"), 3.0);
			break;
		case EState::Run:
			RunCycle();
			break;
		case EState::Air:
			HoldPose(F.JumpDir != 0 ? FName(TEXT("jump_tuck")) : FName(TEXT("jump")), F.JumpDir != 0 ? 4.0 : 5.0);
			break;
		case EState::Move:
		case EState::Finisher:
			if (!F.Move)
			{
				HoldPose(TEXT("stance"), 6.0);
			}
			else if (F.ThrowVictim >= 0)
			{
				if (SetSource(FString::Printf(TEXT("throw:%d"), MoveTick), 0.0))
				{
					AnimKeys(F.Move->HoldKeys, OwnedKeys);
				}
				FPoseLibrary::Sample(OwnedKeys, double(F.ThrowT) - 1.0 + Frac, Snap, Src);
			}
			else
			{
				if (SetSource(FString::Printf(TEXT("move:%s:%d"), *F.Move->Id.ToString(), MoveTick), 0.0))
				{
					const TArray<FKey>* MK = Lib.MoveKeys(F.Move->Id);
					OwnedKeys = MK ? *MK : TArray<FKey>();
				}
				FPoseLibrary::Sample(OwnedKeys, double(F.MF) - 1.0 + Frac, Snap, Src);
			}
			break;
		case EState::BlockStun:
		case EState::HitStun:
		{
			const FName React = ReactionFor(F.StunKind);
			const double Total = double(F.Stun + F.SF);
			if (SetSource(FString::Printf(TEXT("stun:%d:%s"), SimTick - F.SF, StunKey(F.StunKind)), 0.0))
			{
				FName Back = F.bCrouchStun ? FName(TEXT("crouch")) : FName(TEXT("stance"));
				if (F.StunKind == EStun::Guard || F.StunKind == EStun::CrouchGuard)
				{
					Back = F.bCrouchStun ? FName(TEXT("block_low")) : FName(TEXT("block"));
				}
				OwnedKeys.Reset();
				if (F.StunKind == EStun::Dazed)
				{
					OwnedKeys.Add({4.0, &Lib.Get(TEXT("dazed")), TEXT("out")});
					OwnedKeys.Add({Total, &Lib.Get(TEXT("dazed")), TEXT("linear")});
				}
				else if (F.StunKind == EStun::Crumple)
				{
					OwnedKeys.Add({3.0, &Lib.Get(TEXT("hit_gut")), TEXT("out")});
					OwnedKeys.Add({Total, &Lib.Get(TEXT("crumple")), TEXT("inout")});
				}
				else
				{
					// Recoil from the current pose, then ease straight back; bigger stuns rock further.
					OwnedPoses[0] = ReactionPose(React, Back, Total);
					OwnedPoses[1].BlendInto(OwnedPoses[0], Lib.Get(Back), 0.45);
					OwnedKeys.Add({2.0, &OwnedPoses[0], TEXT("snap")});
					OwnedKeys.Add({FMath::Max(3.0, Total * 0.4), &OwnedPoses[1], TEXT("out")});
					OwnedKeys.Add({Total, &Lib.Get(Back), TEXT("inout")});
				}
			}
			FPoseLibrary::Sample(OwnedKeys, T, Snap, Src);
			break;
		}
		case EState::Juggle:
			HoldPose(F.Vel.Y > 0.0 ? FName(TEXT("air_hit")) : FName(TEXT("air_fall")), 5.0);
			break;
		case EState::Fall:
		{
			const bool bSwept = F.LastHitMove && F.LastHitMove->Effect == TEXT("sweep");
			HoldPose(bSwept ? FName(TEXT("swept")) : FName(TEXT("air_fall")), 4.0);
			break;
		}
		case EState::Down:
		case EState::KO:
			HoldPose(TEXT("down"), 6.0);
			break;
		case EState::WakeUp:
			if (SetSource(FString::Printf(TEXT("wake:%d"), SimTick - F.SF), 0.0))
			{
				OwnedKeys.Reset();
				OwnedKeys.Add({8.0, &Lib.Get(TEXT("rise_1")), TEXT("out")});
				OwnedKeys.Add({16.0, &Lib.Get(TEXT("rise_2")), TEXT("inout")});
				OwnedKeys.Add({24.0, &Lib.Get(TEXT("stance")), TEXT("inout")});
			}
			FPoseLibrary::Sample(OwnedKeys, T, Snap, Src);
			break;
		case EState::Thrown:
		{
			const FFighter* Th = (F.ThrownBy >= 0 && F.Sim) ? &F.Sim->Fighters[F.ThrownBy] : nullptr;
			if (Th && Th->Move)
			{
				if (SetSource(FString::Printf(TEXT("thrown:%s:%d"), *Th->Move->Id.ToString(), SimTick - F.SF), 0.0))
				{
					AnimKeys(Th->Move->VictimKeys, OwnedKeys);
				}
				FPoseLibrary::Sample(OwnedKeys, double(Th->ThrowT) - 1.0 + Frac, Snap, Src);
			}
			break;
		}
		case EState::Dizzy:
		case EState::Victim:
			HoldPose(TEXT("dizzy"), 12.0);
			break;
		default:
			break;
		}
	}

	// A two-pose stride cycle timed to the distance covered, so the feet keep pace with the ground.
	void FAnimator::RunCycle()
	{
		SetSource(TEXT("run"), 4.0);
		const double Stride = 1.25 * Rig.H / 1.8;
		RunPhase = FMath::Fmod(FMath::Abs(Fighter->Pos.X) / Stride, 1.0);
		const double U = 0.5 - 0.5 * FMath::Cos(RunPhase * Tau);
		Src.BlendInto(Lib.Get(TEXT("run_a")), Lib.Get(TEXT("run_b")), U);
	}

	// The reaction pose for a fresh stun, only partly reached for short stuns.
	FPose FAnimator::ReactionPose(FName React, FName Back, double StunTotal)
	{
		FPose R;
		R.BlendInto(Lib.Get(Back), Lib.Get(React), FMath::Clamp(0.45 + StunTotal / 40.0, 0.55, 1.0));
		return R;
	}

	void FAnimator::Preview(FName PoseName)
	{
		const FPose& P = Lib.Get(PoseName);
		Key = TEXT("preview:") + PoseName.ToString();
		Snap = Src = Out = P;
		BlendLen = 0.0;
		ResetFeet();
		FVector Ankles[2];
		double Yaws[2];
		PlantFeet(0.0, Out, Ankles, Yaws);
		FQuat Extra[(int32)EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		Rig.Apply(Out, Ankles, Yaws, Extra);
	}

	// --- Procedural layers ---------------------------------------------------------------------------------

	void FAnimator::Procedural(double Delta, bool bFrozen, FQuat Extra[(int32)EJoint::Count])
	{
		const FFighter& F = *Fighter;
		FVector Target = FVector::ZeroVector;
		const FVector LocalV = Root.Q.Inverse().RotateVector(VisVel);
		if (F.State == EState::WalkF || F.State == EState::WalkB)
		{
			Target = FVector(FMath::Clamp(-LocalV.Z * 0.045, -0.2, 0.25), 0.0, 0.0);
		}
		if (!bFrozen && Delta > 0.0)
		{
			const FVector Acc = (Target - Lean) * 160.0 - LeanV * 22.0;
			LeanV += Acc * Delta;
			Lean += LeanV * Delta;
			for (FVector (&S)[2] : Spring)
			{
				const FVector A = -S[0] * 260.0 - S[1] * 16.0;
				S[1] += A * Delta;
				S[0] += S[1] * Delta;
			}
		}
		const double Breath = In(F.State, {EState::Idle, EState::Intro, EState::Crouch, EState::Win, EState::Block}) ? FMath::Sin(Time * Tau * 0.3) * 0.02 : 0.0;
		FVector Sway = FVector::ZeroVector;
		if (F.State == EState::Dizzy || F.State == EState::Victim || (F.State == EState::HitStun && F.StunKind == EStun::Dazed))
		{
			// Unsteady on their feet: a slow figure-eight of the upper body.
			Sway = FVector(FMath::Sin(Time * 2.1) * 0.12, FMath::Sin(Time * 1.05) * 0.18, FMath::Cos(Time * 2.1) * 0.08);
		}
		const double Side = Facing > 0 ? 1.0 : -1.0;
		const FVector& Chest = Spring[0][0];
		const FVector& Head = Spring[1][0];
		const FVector& Hips = Spring[2][0];
		Extra[(int32)EJoint::Hips] = EulerRad(Hips * 0.6);
		Extra[(int32)EJoint::Spine] = EulerRad(FVector(-Lean.X * 0.5 + Sway.X * 0.5, Sway.Y * 0.4 * Side, Sway.Z * 0.5 * Side));
		Extra[(int32)EJoint::Chest] = EulerRad(FVector(-Lean.X * 0.5 + Breath + Sway.X * 0.5, Sway.Y * 0.3 * Side, Sway.Z * 0.4 * Side) + Chest);
		Extra[(int32)EJoint::Head] = EulerRad(Head + FVector(Sway.X * 0.6, 0, Sway.Z * Side));
	}

	// Whole-body rotation about the hips: somersaults on angled jumps, backward tumbles when juggled
	// or knocked down. Returns the transform to append to the root.
	FXf FAnimator::AirTilt(double Delta, bool bFrozen)
	{
		const FFighter& F = *Fighter;
		FVector Pivot(0, 0.5 * Rig.H, 0);
		if (F.State == EState::Air && F.JumpDir != 0)
		{
			// A full forward somersault over the jump, timed by the vertical speed.
			const double U = FMath::Clamp(1.0 - (F.Vel.Y + F.Def->JumpVy) / (2.0 * F.Def->JumpVy), 0.0, 1.0);
			Flip = -360.0 * FPose::Ease(TEXT("inout"), FMath::Clamp((U - 0.08) / 0.8, 0.0, 1.0)) * double(F.JumpDir);
		}
		else if (Delta > 0.0)
		{
			Flip = MoveToward(Flip, FMath::Abs(Flip) < 180.0 ? 0.0 : FMath::Sign(Flip) * 360.0, Delta * 900.0);
			if (FMath::Abs(Flip) >= 359.0)
			{
				Flip = 0.0;
			}
		}
		if (F.State == EState::Juggle || F.State == EState::Fall)
		{
			if (Delta > 0.0 && !bFrozen)
			{
				double Target = FMath::Clamp(-F.Vel.Y * 6.0, -20.0, 40.0);
				if (F.State == EState::Fall && FMath::Abs(F.Vel.X) > 3.0)
				{
					Target = 80.0;
				}
				const double Acc = (Target - AirPitch) * 80.0 - AirPitchV * 11.0;
				AirPitchV += Acc * Delta;
				AirPitch += AirPitchV * Delta;
			}
			Pivot = FVector(0, 0.35 * Rig.H, 0);
		}
		else
		{
			AirPitchV = 0.0;
			AirPitch = MoveToward(AirPitch, 0.0, Delta * 300.0);
		}
		const double Spin = Flip + AirPitch + SpinExtra;
		if (FMath::Abs(Spin) < 0.01)
		{
			return FXf();
		}
		return FXf(FQuat::Identity, Pivot) * FXf(AxisAngle(FVector(1, 0, 0), FMath::DegreesToRadians(Spin)), FVector::ZeroVector) * FXf(FQuat::Identity, -Pivot);
	}

	void FAnimator::Impact(double Dir, double Strength, ELevel Level, bool bBlocked)
	{
		if (Fighter->IsAirborne())
		{
			AirPitchV += 180.0 * FMath::Clamp(Strength, 0.3, 1.5);
		}
		// Pushed away from the attacker: in rig space that is backward (+z) when the attacker is in front.
		const double Back = FMath::Sign(Dir) == double(-Facing) ? 1.0 : -1.0;
		const double S = Strength * (bBlocked ? 0.35 : 1.0) * Back;
		const double HeadK = Level == ELevel::High ? 1.6 : 0.7;
		const double ChestK = Level == ELevel::Low ? 0.4 : 1.0;
		Spring[1][1] += FVector(-S * HeadK * 9.0, 0, 0);
		Spring[0][1] += FVector(-S * ChestK * 6.0, 0, 0);
		Spring[2][1] += FVector(-S * 2.0 * (Level == ELevel::Low ? 1.6 : 0.6), 0, 0);
		Shake = FMath::Clamp(0.004 + Strength * 0.014, 0.0, 0.024);
		if (!bBlocked)
		{
			// Every blow lights the body for a frame or two; heavy ones whiten it.
			Flash(0.05 + FMath::Clamp(Strength, 0.0, 1.3) * 0.14);
		}
	}

	// --- Feet ------------------------------------------------------------------------------------------------

	// Planted feet stay put in the world until the pose's target drifts too far, then take a short
	// arcing step. Outputs rig-space ankle targets and rig-space foot yaws.
	void FAnimator::PlantFeet(double Delta, const FPose& Pose, FVector OutAnkles[2], double OutYaw[2])
	{
		const FFighter& F = *Fighter;
		const FXf& G = Root;
		const FXf GI = G.Inverse();
		const FVector GZ = G.Z();
		const double RootYaw = FMath::Atan2(-GZ.X, -GZ.Z);
		const double Speed = FMath::Abs(VisVel.X);
		const bool bLoose = In(F.State, {EState::Air, EState::Juggle, EState::Fall, EState::Down, EState::KO, EState::Thrown, EState::Run, EState::WakeUp})
			|| F.IsAirborne() || FMath::Abs(Flip) > 1.0 || FMath::Abs(AirPitch) > 1.0;
		const double Thresh = 0.085 * Rig.H;
		for (int32 I = 0; I < 2; ++I)
		{
			FFoot& Ft = Feet[I];
			const FVector WantLocal = Pose.P(I == 0 ? EPos::FootL : EPos::FootR) + FVector(0, Rig.AnkleH, 0);
			const FVector Want = G * WantLocal;
			const double WantYaw = RootYaw + FMath::DegreesToRadians(Pose.V(I == 0 ? EVal::YawL : EVal::YawR));
			const bool bFree = Pose.V(I == 0 ? EVal::FreeL : EVal::FreeR) >= 0.5 || bLoose || WantLocal.Y > Rig.AnkleH + 0.06;
			if (bFree || !Ft.bValid || Delta <= 0.0)
			{
				Ft.Pos = Want;
				Ft.Yaw = WantYaw;
				Ft.Step = -1.0;
				Ft.bValid = !bFree;
			}
			else if (Ft.Step >= 0.0)
			{
				Ft.Step += Delta / Ft.Dur;
				const double K = FMath::Clamp(Ft.Step, 0.0, 1.0);
				const double E = FPose::Ease(TEXT("inout"), K);
				const FVector Lead = FVector(VisVel.X, 0, 0) * Ft.Dur * (1.0 - K) * 0.5;
				FVector P = FMath::Lerp(Ft.From, Want + Lead, E);
				P.Y = Want.Y + FMath::Sin(K * UE_DOUBLE_PI) * FMath::Min(0.05 + Speed * 0.02, 0.1) * Rig.H / 1.8;
				Ft.Pos = P;
				Ft.Yaw = LerpAngle(Ft.FromYaw, WantYaw, E);
				if (K >= 1.0)
				{
					Ft.Step = -1.0;
					Ft.Pos = FVector(P.X, Want.Y, P.Z);
				}
			}
			else
			{
				const double Err = FVector2D(Want.X - Ft.Pos.X, Want.Z - Ft.Pos.Z).Size();
				const double YawErr = FMath::Abs(AngleDifference(Ft.Yaw, WantYaw));
				const bool bOtherBusy = Feet[1 - I].Step >= 0.0;
				if ((Err > Thresh || YawErr > 0.6) && (!bOtherBusy || Err > Thresh * 1.6))
				{
					Ft.Step = 0.0;
					Ft.From = Ft.Pos;
					Ft.FromYaw = Ft.Yaw;
					Ft.Dur = F.State != EState::Move ? FMath::Clamp(0.17 - Speed * 0.03, 0.08, 0.17) : 0.09;
				}
				Ft.Pos = FVector(Ft.Pos.X, Want.Y, Ft.Pos.Z);
			}
			OutAnkles[I] = GI * Ft.Pos;
			OutYaw[I] = Ft.Yaw - RootYaw;
		}
	}

	// Moves that sink into the floor lower the whole body, then raise it.
	double FAnimator::MoveSink(double Frac) const
	{
		const FFighter& F = *Fighter;
		if (F.State != EState::Move || !F.Move || F.Move->Sink == FIntPoint::ZeroValue)
		{
			return 0.0;
		}
		const double T = double(F.MF) - 1.0 + Frac;
		const double A = F.Move->Sink.X;
		const double B = F.Move->Sink.Y;
		if (T < A || T > B)
		{
			return 0.0;
		}
		const double U = (T - A) / FMath::Max(B - A, 1.0);
		return Rig.H * 1.05 * FMath::Clamp(FMath::Min(U * 4.0, (1.0 - U) * 4.0), 0.0, 1.0);
	}

	FVector FAnimator::StrikePoint(FName Which) const
	{
		const FString W = Which.ToString();
		if (W == TEXT("hand_l")) return Root * Rig.HandTip(0);
		if (W == TEXT("hand_r")) return Root * Rig.HandTip(1);
		if (W == TEXT("foot_l")) return Root * Rig.FootTip(0);
		if (W == TEXT("foot_r")) return Root * Rig.FootTip(1);
		if (W == TEXT("knee_l")) return Root * Rig.J(EJoint::ShinL).P;
		if (W == TEXT("knee_r")) return Root * Rig.J(EJoint::ShinR).P;
		return Root.P;
	}
}
