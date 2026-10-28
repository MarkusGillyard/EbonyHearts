#include "Anim/SlopAnimator.h"

namespace Slop::Anim
{
	static constexpr double Tau = 2.0 * UE_DOUBLE_PI;

	static FQuat EulerRad(const FVector& V)
	{
		return AxisAngle(FVector(0, 1, 0), V.Y) * AxisAngle(FVector(1, 0, 0), V.X) * AxisAngle(FVector(0, 0, 1), V.Z);
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
		// Each fighter keeps its own rhythm in the stance: taller bodies a little slower, and the
		// two sides out of step with each other.
		// Hashed from the name's text, which is the same every run (a name's own hash follows load order).
		const uint32 Hash = FCrc::StrCrc32(*InFighter.Def->Id.ToString()) + uint32(InFighter.Index) * 2654435761u;
		Pace = 1.15 * FMath::Sqrt(1.8 / FMath::Max(InFighter.Def->Height, 0.5)) * (0.92 + 0.16 * double(Hash % 1000) / 1000.0) * InFighter.Def->Motion.Pace;
		IdlePhase = double((Hash / 1000) % 628) / 100.0;
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
			PoseLead = 0.0;
		}
		else if (LastState != F.State)
		{
			PoseLead = 0.0;
		}
		if (LastState != F.State)
		{
			CycleDist = 0.0;
		}
		LastState = F.State;
		LastMove = F.Move;
		if (bTeleport)
		{
			ResetFeet();
			bLagValid = false;
			TurnT = 1.0;
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
			// Crossed over while standing, walking or crouching: turn round over a few frames, the
			// chest passing the camera. Anything else (an attack, a hit, the air) shows the new side
			// at once, as the simulation already has it.
			TurnFrom = Facing;
			Facing = F.Facing;
			TurnT = Delta > 0.0 && CanTurn() ? 0.0 : 1.0;
			if (TurnT >= 1.0)
			{
				ResetFeet();
			}
			else
			{
				const FQuat From = FromAxes(FVector(0, 0, TurnFrom), FVector(0, 1, 0), FVector(-TurnFrom, 0, 0));
				TurnSign = (AxisAngle(FVector(0, 1, 0), 0.5 * UE_DOUBLE_PI) * From).RotateVector(FVector(0, 0, -1)).Z > 0.0 ? 1.0 : -1.0;
			}
		}
		else if (TurnT < 1.0)
		{
			if (!CanTurn())
			{
				TurnT = 1.0;
			}
			else if (!bFrozen)
			{
				TurnT += Delta / 0.12;
			}
			if (TurnT >= 1.0)
			{
				TurnT = 1.0;
				ResetFeet();
			}
		}
		if (bFrozen && Shake > 0.0)
		{
			// Sakurai's recipe: the struck body shakes, fading over the freeze, side to side on the
			// ground and up and down in the air (so the feet stay out of the floor).
			const double Amp = Shake * FMath::Clamp(double(F.Hitstop) / ShakeFrames, 0.0, 1.0);
			const double S = FMath::Sin(Time * 95.0) * Amp;
			if (F.IsAirborne())
			{
				P.Y += S;
			}
			else
			{
				P.X += S;
			}
		}
		if (bFrozen && Delta > 0.0)
		{
			const bool bStruck = F.State == EState::HitStun || F.State == EState::BlockStun;
			PoseLead = FMath::Min(PoseLead + Delta * 60.0 * (bStruck ? 0.5 : 0.3), bStruck ? 2.5 : 1.0);
		}
		if (Delta > 0.0)
		{
			VisVel = FMath::Lerp(VisVel, (P - LastRoot) / Delta, FMath::Clamp(Delta * 12.0, 0.0, 1.0));
		}
		LastRoot = P;
		// Basis.looking_at(facing, up): the rig's -z points along +x times facing.
		FQuat Look = FromAxes(FVector(0, 0, Facing), FVector(0, 1, 0), FVector(-Facing, 0, 0));
		const double TurnE = FPose::Ease(TEXT("inout"), TurnT);
		if (TurnT < 1.0)
		{
			const FQuat From = FromAxes(FVector(0, 0, TurnFrom), FVector(0, 1, 0), FVector(-TurnFrom, 0, 0));
			Look = AxisAngle(FVector(0, 1, 0), TurnSign * UE_DOUBLE_PI * TurnE) * From;
		}
		if (F.Def->Motion.CircleWalk > 0.0 && WalkW > 0.0)
		{
			// Walking the circle: the shown body arcs toward and away from the camera along its path.
			P.Z += FMath::Sin(P2.X / (0.9 * Rig.H) * UE_DOUBLE_PI) * 0.05 * Rig.H * WalkW * F.Def->Motion.CircleWalk;
		}
		Root = FXf(Look, P + LiftExtra) * AirTilt(Delta, bFrozen);
		WorldScale = ExtraScale;
		if (Delta > 0.0)
		{
			const double Moved = FMath::Abs(P2.X - CycleX);
			CycleDist += Moved;
			StillTime = Moved > 1e-5 ? 0.0 : StillTime + Delta;
		}
		CycleX = P2.X;
		bCycleFeet = false;
		EvaluateSource(Frac);
		if (Delta > 0.0)
		{
			BlendT += Delta * 60.0 * (bFrozen ? 0.0 : 1.0);
		}
		const double W = BlendLen > 0.0 ? FPose::Ease(TEXT("inout"), BlendT / BlendLen) : 1.0;
		Out.BlendInto(Snap, Src, W);
		FollowThrough(Delta, bFrozen);
		// The side toward the camera; through a turn, the old side blends into the new one (the
		// guard passes through the middle as the body comes round).
		if (TurnT < 1.0)
		{
			FPose M;
			Lagged.MirrorInto(M);
			Display.BlendInto(TurnFrom < 0 ? M : Lagged, Facing < 0 ? M : Lagged, TurnE);
		}
		else if (Facing < 0)
		{
			Lagged.MirrorInto(Display);
		}
		else
		{
			Display = Lagged;
		}
		const FXf Torso = Rig.TorsoXf(Display, nullptr);
		FQuat Extra[(int32)EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		Procedural(Delta, bFrozen, Extra);
		Life(Delta, bFrozen, P2.X, Display, Extra);
		CarryArms(Torso, Rig.TorsoXf(Display, Extra), Display);
		if (F.Def->Motion.bHeadLocked)
		{
			// A helmet bolted to its breastplate: the head goes only where the chest goes.
			Display.R(ERot::Neck) = Display.R(ERot::Head) = FQuat::Identity;
			Extra[(int32)EJoint::Neck] = Extra[(int32)EJoint::Head] = FQuat::Identity;
		}
		FVector Ankles[2];
		double Yaws[2];
		PlantFeet(Delta, Display, Ankles, Yaws);
		Rig.Apply(Display, Ankles, Yaws, Extra);
		Glow = Display.V(EVal::Glow);
		if (FlashAmount > 0.0)
		{
			FlashAmount = FMath::Max(0.0, FlashAmount - Delta * 6.0);
		}
	}

	// --- Follow-through -------------------------------------------------------------------------------------

	// How quickly each part of the body chases the authored pose (angular frequency, per second):
	// the hips almost at once, then up the spine and out to the head and hands. Zero follows the
	// pose exactly (the feet and knees, which the planting and the legs' own targets handle).
	static double RotRate(int32 K)
	{
		switch ((ERot)K)
		{
		case ERot::Hips: return 50.0;
		case ERot::Spine: return 42.0;
		case ERot::Chest: return 34.0;
		case ERot::Neck: return 29.0;
		case ERot::Head: return 24.0;
		case ERot::ClavL: case ERot::ClavR: return 30.0;
		default: return 28.0;
		}
	}

	static double PosRate(int32 K)
	{
		switch ((EPos)K)
		{
		case EPos::Hips: return 48.0;
		case EPos::HandL: case EPos::HandR: return 34.0;
		case EPos::ElbowL: case EPos::ElbowR: return 30.0;
		default: return 0.0;
		}
	}

	// The arm that carries an attack stays exactly on its authored pose until the blow is over
	// (the hitbox is where the pose puts it), and so do both arms in a throw. `AuthoredSide` is 0
	// for the pose's left arm.
	bool FAnimator::ArmExempt(int32 AuthoredSide) const
	{
		const FFighter& F = *Fighter;
		if (F.State == EState::Thrown || ((F.State == EState::Move || F.State == EState::Finisher) && F.ThrowVictim >= 0))
		{
			return true;
		}
		if (F.State != EState::Move || !F.Move)
		{
			return false;
		}
		const FMoveDef& M = *F.Move;
		if (F.MF > (M.Active > 0 ? M.LastActive() : M.Startup + 6))
		{
			return false;
		}
		if (M.Tip.IsNone())
		{
			return true;
		}
		return M.Tip == (AuthoredSide == 0 ? FName(TEXT("hand_l")) : FName(TEXT("hand_r")));
	}

	void FAnimator::FollowThrough(double Delta, bool bFrozen)
	{
		if (!bLagValid)
		{
			Lagged = Out;
			for (FVector& V : LagW)
			{
				V = FVector::ZeroVector;
			}
			for (FVector& V : LagV)
			{
				V = FVector::ZeroVector;
			}
			bLagValid = true;
			return;
		}
		const bool bExempt[2] = {ArmExempt(0), ArmExempt(1)};
		const auto ArmOf = [](int32 K, bool bRot) -> int32
		{
			if (bRot)
			{
				return K == (int32)ERot::WristL ? 0 : K == (int32)ERot::WristR ? 1 : -1;
			}
			return K == (int32)EPos::HandL || K == (int32)EPos::ElbowL ? 0 : K == (int32)EPos::HandR || K == (int32)EPos::ElbowR ? 1 : -1;
		};
		// Stuns and the start of an attack commit faster; the rest settles at the usual pace.
		const FFighter& F = *Fighter;
		double Stiff = 1.0;
		if (F.State == EState::HitStun || F.State == EState::BlockStun)
		{
			Stiff = 1.5;
		}
		else if (F.State == EState::Move && F.Move && F.MF <= F.Move->Startup)
		{
			Stiff = 1.3;
		}
		const double Dt = FMath::Min(Delta, 0.05);
		const int32 Steps = (bFrozen || Dt <= 0.0) ? 0 : FMath::Max(1, FMath::CeilToInt(Dt * 240.0));
		const double H = Steps > 0 ? Dt / Steps : 0.0;
		constexpr double Zeta = 0.72;
		for (int32 K = 0; K < (int32)ERot::Count; ++K)
		{
			const int32 Arm = ArmOf(K, true);
			const double Rate = RotRate(K) * Stiff;
			if (Arm >= 0 && bExempt[Arm])
			{
				Lagged.Rot[K] = Out.Rot[K];
				LagW[K] = FVector::ZeroVector;
				continue;
			}
			for (int32 I = 0; I < Steps; ++I)
			{
				FQuat E = Out.Rot[K] * Lagged.Rot[K].Inverse();
				if (E.W < 0.0)
				{
					E = FQuat(-E.X, -E.Y, -E.Z, -E.W);
				}
				FVector Axis;
				double Angle;
				E.ToAxisAndAngle(Axis, Angle);
				const FVector Err = Angle > 1e-6 ? Axis * Angle : FVector::ZeroVector;
				LagW[K] += (Err * Rate * Rate - LagW[K] * 2.0 * Zeta * Rate) * H;
				const double Turn = LagW[K].Size() * H;
				if (Turn > 1e-9)
				{
					Lagged.Rot[K] = (FQuat(LagW[K].GetSafeNormal(), Turn) * Lagged.Rot[K]).GetNormalized();
				}
			}
		}
		for (int32 K = 0; K < (int32)EPos::Count; ++K)
		{
			const int32 Arm = ArmOf(K, false);
			const double Rate = PosRate(K) * Stiff;
			if (Rate <= 0.0 || (Arm >= 0 && bExempt[Arm]))
			{
				Lagged.Pos[K] = Out.Pos[K];
				LagV[K] = FVector::ZeroVector;
				continue;
			}
			for (int32 I = 0; I < Steps; ++I)
			{
				LagV[K] += ((Out.Pos[K] - Lagged.Pos[K]) * Rate * Rate - LagV[K] * 2.0 * Zeta * Rate) * H;
				Lagged.Pos[K] += LagV[K] * H;
			}
		}
		for (int32 K = 0; K < (int32)EVal::Count; ++K)
		{
			Lagged.Val[K] = Out.Val[K];
		}
	}

	bool FAnimator::CanTurn() const
	{
		const FFighter& F = *Fighter;
		return !F.IsAirborne() && In(F.State, {EState::Idle, EState::WalkF, EState::WalkB, EState::Crouch, EState::Run, EState::Land, EState::Block, EState::CrouchBlock});
	}

	// --- Stance and walk -------------------------------------------------------------------------------------

	// A fighter's weight sinks and rises in the stance and rocks between the feet; walking, the hips
	// and shoulders counter each other and the body rides up and down on each step. The legs take up
	// the difference (the feet are planted) and the hands trail the chest a little.
	void FAnimator::Life(double Delta, bool bFrozen, double X, FPose& Pose, FQuat Extra[(int32)EJoint::Count])
	{
		const FFighter& F = *Fighter;
		const bool bWalk = F.State == EState::WalkF || F.State == EState::WalkB;
		double IdleTarget = 0.0;
		if (F.State == EState::Idle)
		{
			IdleTarget = 1.0;
		}
		else if (In(F.State, {EState::Intro, EState::Win}))
		{
			IdleTarget = 0.45;
		}
		else if (F.State == EState::Crouch)
		{
			IdleTarget = 0.35;
		}
		else if (F.State == EState::Block || F.State == EState::CrouchBlock)
		{
			IdleTarget = 0.2;
		}
		if (Delta > 0.0 && !bFrozen)
		{
			IdleW = MoveToward(IdleW, IdleTarget, Delta * 4.0);
			WalkW = MoveToward(WalkW, bWalk && !bCycleFeet ? 1.0 : 0.0, Delta * 7.0);
			IdlePhase = FMath::Fmod(IdlePhase + Delta * Tau * Pace, Tau * 2.0);
		}
		const double H = Rig.H;
		const double Side = Facing > 0 ? 1.0 : -1.0;
		const FMotionStyle& Style = F.Def->Motion;
		HandOff[0] = HandOff[1] = FVector::ZeroVector;
		if (IdleW > 0.0)
		{
			const double Down = 0.5 - 0.5 * FMath::Cos(IdlePhase);
			const double Late = 0.5 - 0.5 * FMath::Cos(IdlePhase - 0.9);
			const double Bounce = IdleW * Style.Bounce;
			const double Rock = IdleW * Style.Rock;
			Pose.P(EPos::Hips).Y -= Down * 0.013 * H * Bounce;
			Pose.P(EPos::Hips).Z += FMath::Sin(IdlePhase * 0.5) * 0.007 * H * Rock;
			Extra[(int32)EJoint::Chest] = Extra[(int32)EJoint::Chest] * EulerRad(FVector(-Late * 0.045 * Bounce + FMath::Sin(IdlePhase * 0.5 - 0.4) * 0.02 * Rock,
				FMath::Sin(IdlePhase * 0.5) * 0.03 * Rock * Side, 0.0));
			Extra[(int32)EJoint::Head] = Extra[(int32)EJoint::Head] * EulerRad(FVector(Late * 0.03 * Bounce, 0.0, 0.0));
			for (int32 S = 0; S < 2; ++S)
			{
				const double Ph = IdlePhase - 1.1 - 0.5 * S;
				HandOff[S] += FVector(0.0, -(0.5 - 0.5 * FMath::Cos(Ph)) * 0.012 * H * Bounce, FMath::Sin(Ph) * 0.005 * H * IdleW);
			}
		}
		if (WalkW > 0.0)
		{
			const double Stride = 0.6 * H / 1.8;
			const double Ph = FMath::Abs(X) / Stride * UE_DOUBLE_PI;
			Pose.P(EPos::Hips).Y += (FMath::Abs(FMath::Sin(Ph)) - 0.5) * 0.024 * H * WalkW * Style.WalkBob;
			const double Twist = FMath::Sin(Ph) * WalkW * Style.WalkTwist;
			Extra[(int32)EJoint::Hips] = Extra[(int32)EJoint::Hips] * EulerRad(FVector(0.0, -Twist * 0.07 * Side, Twist * 0.035));
			Extra[(int32)EJoint::Chest] = Extra[(int32)EJoint::Chest] * EulerRad(FVector(0.0, Twist * 0.11 * Side, -Twist * 0.03));
			Extra[(int32)EJoint::Head] = Extra[(int32)EJoint::Head] * EulerRad(FVector(0.0, -Twist * 0.05 * Side, 0.0));
			for (int32 S = 0; S < 2; ++S)
			{
				HandOff[S] += FVector(0.0, -FMath::Abs(FMath::Sin(Ph - 0.7)) * 0.012 * H, 0.0) * WalkW;
			}
		}
	}

	// The hands and elbows are placed in the fighter's root space, so a body that sinks, twists or
	// recoils would leave them hanging where the pose put them: carry them with the chest instead,
	// then let them trail it (HandOff). An attacking arm stays where the attack puts it.
	void FAnimator::CarryArms(const FXf& From, const FXf& To, FPose& Pose) const
	{
		const FXf D = To * From.Inverse();
		for (int32 S = 0; S < 2; ++S)
		{
			// Display sides are mirrored when facing left.
			if (ArmExempt(Facing < 0 ? 1 - S : S))
			{
				continue;
			}
			FVector& Hand = Pose.P(S == 0 ? EPos::HandL : EPos::HandR);
			FVector& Elbow = Pose.P(S == 0 ? EPos::ElbowL : EPos::ElbowR);
			FVector& Fingers = Pose.P(S == 0 ? EPos::FingersL : EPos::FingersR);
			FVector& Palm = Pose.P(S == 0 ? EPos::PalmL : EPos::PalmR);
			Hand = D * Hand + HandOff[S];
			Elbow = D * Elbow + HandOff[S];
			Fingers = D.Q.RotateVector(Fingers);
			Palm = D.Q.RotateVector(Palm);
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
		case EState::WalkF:
		case EState::WalkB:
			if (StillTime > 0.1 || !PlayCycle(F.State == EState::WalkF ? FName(TEXT("walk_f")) : FName(TEXT("walk_b")), 6.0))
			{
				HoldPose(TEXT("stance"), 7.0);
			}
			break;
		case EState::Idle:
			HoldPose(TEXT("stance"), 7.0);
			break;
		case EState::Intro:
		case EState::Win:
		{
			// A captured performance where the fighter has one (going into the stance, a bow, a
			// taunt), else the held pose.
			const FName State = F.State == EState::Win ? FName(TEXT("win")) : FName(TEXT("intro"));
			if (const TArray<FAnimKey>* Raw = F.Def->StateAnims.Find(State))
			{
				if (SetSource(FString::Printf(TEXT("state:%s:%d"), *State.ToString(), SimTick - F.SF), 0.0))
				{
					AnimKeys(*Raw, OwnedKeys);
				}
				FPoseLibrary::Sample(OwnedKeys, T, Snap, Src);
			}
			else
			{
				HoldPose(State, F.State == EState::Win ? 14.0 : 10.0);
			}
			break;
		}
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
			if (StillTime > 0.1)
			{
				HoldPose(TEXT("stance"), 6.0);
			}
			else if (!PlayCycle(TEXT("run"), 4.0))
			{
				RunCycle();
			}
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
					const TArray<FPoseKey>* MK = Lib.MoveKeys(F.Move->Id);
					OwnedKeys = MK ? *MK : TArray<FPoseKey>();
					// The key a blow lands on arrives at speed instead of easing to a stop.
					static const FName SnapEase(TEXT("snap")), StrikeEase(TEXT("strike"));
					for (FPoseKey& K : OwnedKeys)
					{
						if (K.Ease == SnapEase && FMath::Abs(K.F - double(F.Move->Startup)) < 0.5)
						{
							K.Ease = StrikeEase;
						}
					}
				}
				const double TM = double(F.MF) - 1.0 + Frac + PoseLead;
				FPoseLibrary::Sample(OwnedKeys, TM, Snap, Src);
				DriveThrough(TM);
			}
			break;
		case EState::BlockStun:
		case EState::HitStun:
		{
			// Standing and kicked in the legs, the leg buckles under the blow (the original showed a
			// body flinch); a library built without the style poses keeps the original's reaction.
			const FName React = F.StunKind == EStun::Low && Lib.Has(TEXT("hit_leg")) ? FName(TEXT("hit_leg")) : ReactionFor(F.StunKind);
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
			FPoseLibrary::Sample(OwnedKeys, T + PoseLead, Snap, Src);
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
			HoldPose(TEXT("down"), 6.0);
			break;
		case EState::KO:
			// Knocked out by a blow to the head, the body falls as a unit and one forearm stays
			// rigidly raised (the "fencing response" seen in about two-thirds of filmed knockouts).
			HoldPose(F.LastHitLevel == ELevel::High && Lib.Has(TEXT("ko_fence")) ? FName(TEXT("ko_fence")) : FName(TEXT("down")), 6.0);
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

	// A captured step cycle, played by the distance covered so the feet keep pace with the ground.
	bool FAnimator::PlayCycle(FName Name, double BlendTicks)
	{
		const FCycleDef* C = Fighter->Def->Cycles.Find(Name);
		if (!C)
		{
			return false;
		}
		SetSource(TEXT("cycle:") + Name.ToString(), BlendTicks);
		const double Stride = C->Stride * Rig.H / 1.8;
		const double Phase = FMath::Fmod(CycleDist / Stride, 1.0) * C->Poses.Num();
		const int32 I0 = FMath::FloorToInt(Phase) % C->Poses.Num();
		const int32 I1 = (I0 + 1) % C->Poses.Num();
		Src.BlendInto(Lib.Get(C->Poses[I0]), Lib.Get(C->Poses[I1]), Phase - FMath::FloorToDouble(Phase));
		CycleFloor[0] = CycleFloor[1] = 1e9;
		for (const FName& P : C->Poses)
		{
			const FPose& K = Lib.Get(P);
			CycleFloor[0] = FMath::Min(CycleFloor[0], K.P(EPos::FootL).Y);
			CycleFloor[1] = FMath::Min(CycleFloor[1], K.P(EPos::FootR).Y);
		}
		bCycleFeet = true;
		return true;
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
		Snap = Src = Out = Lagged = Display = P;
		bLagValid = false;
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
			// Stepped in short slices like the follow-through, so a long frame cannot make them diverge.
			const double Dt = FMath::Min(Delta, 0.05);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dt * 240.0));
			const double H = Dt / Steps;
			for (int32 I = 0; I < Steps; ++I)
			{
				LeanV += ((Target - Lean) * 160.0 - LeanV * 22.0) * H;
				Lean += LeanV * H;
				for (FVector (&S)[2] : Spring)
				{
					S[1] += (-S[0] * 260.0 - S[1] * 16.0) * H;
					S[0] += S[1] * H;
				}
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
				const double Dt = FMath::Min(Delta, 0.05);
				const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dt * 240.0));
				for (int32 I = 0; I < Steps; ++I)
				{
					AirPitchV += ((Target - AirPitch) * 80.0 - AirPitchV * 11.0) * (Dt / Steps);
					AirPitch += AirPitchV * (Dt / Steps);
				}
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

	void FAnimator::Impact(double Dir, double Strength, ELevel Level, bool bBlocked, const FVector& StrikeDir, bool bCounter)
	{
		PoseLead = 0.0;
		ShakeFrames = FMath::Max(1.0, double(Fighter->Hitstop));
		if (Fighter->IsAirborne())
		{
			AirPitchV += 180.0 * FMath::Clamp(Strength, 0.3, 1.5);
		}
		// Pushed away from the attacker: in rig space that is backward (+z) when the attacker is in front.
		const double Back = FMath::Sign(Dir) == double(-Facing) ? 1.0 : -1.0;
		const double S = Strength * (bBlocked ? 0.35 : 1.0) * Back;
		const double HeadK = Level == ELevel::High ? 1.6 : 0.7;
		const double ChestK = Level == ELevel::Low ? 0.4 : 1.0;
		// Positive pitch tips a joint back, away from the attacker (the port had these signs reversed,
		// so a blow nodded the head into the fist; Anim.BlowsMoveTheHeadTheWayTheyTravel holds them).
		// A body shot moves the head less than a blow to it.
		const double HeadPitch = Level == ELevel::Mid && !bBlocked ? 0.4 : 1.0;
		Spring[1][1] += FVector(S * HeadK * 9.0 * HeadPitch, 0, 0);
		Spring[0][1] += FVector(S * ChestK * 6.0, 0, 0);
		Spring[2][1] += FVector(S * 2.0 * (Level == ELevel::Low ? 1.6 : 0.6), 0, 0);
		if (!bBlocked && !StrikeDir.IsNearlyZero())
		{
			// The geometry of the blow, in the struck body's own frame: a hook (travelling across the
			// body) turns the head away and bends it to the side, the largest rotational load; an
			// uppercut (travelling up) throws the chin back; a straight drives the head back alone.
			const FVector L = Root.Q.Inverse().RotateVector(StrikeDir.GetSafeNormal());
			const double K = FMath::Abs(S);
			if (FMath::Abs(L.X) > 0.35 && Level != ELevel::Low)
			{
				const double Side = FMath::Sign(L.X) * FMath::Min(1.0, FMath::Abs(L.X) * 1.4);
				const double HeadOnly = Level == ELevel::High ? 1.0 : 0.45;
				Spring[1][1] += FVector(0, -Side * K * 11.0 * HeadOnly, -Side * K * 5.0 * HeadOnly);
				Spring[0][1] += FVector(0, -Side * K * 3.5, -Side * K * 1.5);
			}
			if (L.Y > 0.4)
			{
				Spring[1][1] += FVector(K * 7.0 * L.Y, 0, 0);
				Spring[0][1] += FVector(K * 2.0 * L.Y, 0, 0);
			}
		}
		if (!bBlocked && Level == ELevel::Mid)
		{
			// A body shot folds the torso around the fist and nods the head forward.
			Spring[0][1] += FVector(-FMath::Abs(S) * 9.0, 0, 0);
			Spring[1][1] += FVector(-FMath::Abs(S) * 4.0, 0, 0);
		}
		Shake = FMath::Clamp(0.004 + Strength * 0.014, 0.0, 0.024);
		if (!bBlocked && (bCounter || Strength >= 0.8))
		{
			// Counters and heavy blows light the body for a frame or two.
			Flash(0.05 + FMath::Clamp(Strength, 0.0, 1.3) * 0.14);
		}
	}

	bool FAnimator::StrikeKeys(EPos& OutTip, int32& OutHit) const
	{
		const FFighter& F = *Fighter;
		if (F.State != EState::Move || !F.Move || F.ThrowVictim >= 0)
		{
			return false;
		}
		const FName Tip = F.Move->Tip;
		if (Tip == FName(TEXT("hand_l"))) OutTip = EPos::HandL;
		else if (Tip == FName(TEXT("hand_r"))) OutTip = EPos::HandR;
		else if (Tip == FName(TEXT("foot_l"))) OutTip = EPos::FootL;
		else if (Tip == FName(TEXT("foot_r"))) OutTip = EPos::FootR;
		else return false;
		for (int32 I = 1; I < OwnedKeys.Num(); ++I)
		{
			if (FMath::Abs(OwnedKeys[I].F - double(F.Move->Startup)) < 0.5)
			{
				OutHit = I;
				return true;
			}
		}
		return false;
	}

	// A blow drives on through the target instead of stopping on it: across the active frames the
	// striking hand or foot travels on a few centimetres along its line and the hips follow, then
	// both settle back onto the authored pose. Only the shown pose moves; the hitboxes are the data's.
	void FAnimator::DriveThrough(double T)
	{
		EPos Tip;
		int32 Hit;
		const FMoveDef* M = Fighter->Move;
		if (!M || M->Active <= 0 || !StrikeKeys(Tip, Hit))
		{
			return;
		}
		const FVector D = OwnedKeys[Hit].Pose->P(Tip) - OwnedKeys[Hit - 1].Pose->P(Tip);
		const double Len = D.Size();
		const double U = (T - double(M->Startup)) / (double(M->Active) + 1.0);
		if (Len < 0.02 || U <= 0.0 || U >= 1.0)
		{
			return;
		}
		const double Reach = FMath::Min(0.055 * Rig.H / 1.8, 0.14 * Len) * FMath::Sin(UE_DOUBLE_PI * FMath::Pow(U, 0.6));
		const FVector Along = D / Len * Reach;
		Src.P(Tip) += Along;
		Src.P(EPos::Hips) += FVector(Along.X, 0.0, Along.Z) * 0.3;
	}

	FVector FAnimator::StrikeDirection() const
	{
		EPos Tip;
		int32 Hit;
		if (!StrikeKeys(Tip, Hit))
		{
			return FVector::ZeroVector;
		}
		FVector D = OwnedKeys[Hit].Pose->P(Tip) - OwnedKeys[Hit - 1].Pose->P(Tip);
		if (Facing < 0)
		{
			D.X = -D.X;
		}
		return Root.Q.RotateVector(D).GetSafeNormal();
	}

	void FAnimator::Recoil(double Strength)
	{
		// The blow's reaction back through the striker: the chest and hips give a little.
		const double S = FMath::Clamp(Strength, 0.0, 1.3);
		Spring[0][1] += FVector(S * 2.4, 0, 0);
		Spring[2][1] += FVector(S * 0.8, 0, 0);
		Spring[1][1] += FVector(S * 1.2, 0, 0);
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
		const bool bLoose = In(F.State, {EState::Air, EState::Juggle, EState::Fall, EState::Down, EState::KO, EState::Thrown, EState::WakeUp})
			|| (F.State == EState::Run && !bCycleFeet) || F.IsAirborne() || FMath::Abs(Flip) > 1.0 || FMath::Abs(AirPitch) > 1.0 || TurnT < 1.0;
		const double Thresh = 0.085 * Rig.H;
		for (int32 I = 0; I < 2; ++I)
		{
			FFoot& Ft = Feet[I];
			const FVector WantLocal = Pose.P(I == 0 ? EPos::FootL : EPos::FootR) + FVector(0, Rig.AnkleH, 0);
			const FVector Want = G * WantLocal;
			const double WantYaw = RootYaw + FMath::DegreesToRadians(Pose.V(I == 0 ? EVal::YawL : EVal::YawR));
			if (bCycleFeet && !bLoose)
			{
				// A captured step cycle: a foot that is down stays where it touched the floor (the
				// capture's pelvis surges and slows within a stride, while the body here moves
				// evenly), and once it lifts it eases back onto the captured path.
				if (Delta > 0.0 || !Ft.bValid)
				{
					const bool bDown = WantLocal.Y - Rig.AnkleH < CycleFloor[I] + 0.015 * Rig.H / 1.8;
					if (bDown)
					{
						if (!Ft.bLocked || !Ft.bValid)
						{
							Ft.Pos = Want;
						}
						Ft.bLocked = true;
						Ft.Pos.Y = Want.Y;
						Ft.From = Ft.Pos;
						Ft.Release = 1.0;
					}
					else
					{
						Ft.bLocked = false;
						Ft.Release = FMath::Max(0.0, Ft.Release - Delta / 0.12);
						Ft.Pos = FMath::Lerp(Want, FVector(Ft.From.X, Want.Y, Ft.From.Z), FPose::Ease(TEXT("inout"), Ft.Release));
					}
					Ft.Yaw = WantYaw;
					Ft.Step = -1.0;
					Ft.bValid = true;
				}
				OutAnkles[I] = GI * Ft.Pos;
				OutYaw[I] = Ft.Yaw - RootYaw;
				continue;
			}
			Ft.bLocked = false;
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
				P.Y = Want.Y + FMath::Sin(K * UE_DOUBLE_PI) * FMath::Min(0.05 + Speed * 0.02, 0.1) * Rig.H / 1.8 * F.Def->Motion.StepLift;
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
