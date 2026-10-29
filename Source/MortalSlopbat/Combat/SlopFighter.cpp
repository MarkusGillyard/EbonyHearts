#include "Combat/SlopSim.h"

namespace Slop
{

	// Hurtboxes for a 1.8 m fighter: [back, up, width, height] from the root, facing right.
	static const FRect BoxStand[] = {FRect(-0.2, 0.0, 0.42, 0.95), FRect(-0.22, 0.9, 0.46, 0.62), FRect(-0.13, 1.45, 0.28, 0.38)};
	static const FRect BoxCrouch[] = {FRect(-0.24, 0.0, 0.5, 1.12)};
	static const FRect BoxAir[] = {FRect(-0.22, 0.25, 0.46, 1.25)};

	static bool IsGuard(EStun K) { return K == EStun::Guard || K == EStun::CrouchGuard; }

	const TCHAR* FormName(EForm F)
	{
		switch (F)
		{
		case EForm::Own: return TEXT("own");
		case EForm::Spent: return TEXT("spent");
		case EForm::Borrowed: return TEXT("borrowed");
		}
		return TEXT("?");
	}

	void FFighter::Setup(FFightSim* InSim, int32 InIndex, const FFighterDef* InDef)
	{
		Sim = InSim;
		Index = InIndex;
		Def = InDef;
		BaseDef = InDef;
		Form = EForm::Own;
		Health = InDef->Health;
		Qhi = UsesQhi() ? QhiRules().Max : 0.0;
	}

	FFighter& FFighter::Opponent() const { return Sim->Fighters[1 - Index]; }

	void FFighter::SetState(EState S)
	{
		if (State == S)
		{
			return;
		}
		State = S;
		SF = 0;
	}

	// --- Queries ----------------------------------------------------------------------------------

	bool FFighter::IsAirborne() const
	{
		switch (State)
		{
		case EState::Air:
		case EState::Juggle:
		case EState::Fall:
			return true;
		case EState::Move:
			return (Move && (Move->bAir || FMoveDef::InRange(Move->Airborne, MF))) || Pos.Y > 0.05;
		case EState::Thrown:
		case EState::Victim:
			return Pos.Y > 0.05;
		default:
			return false;
		}
	}

	bool FFighter::IsCrouching() const
	{
		switch (State)
		{
		case EState::Crouch:
		case EState::CrouchBlock:
			return true;
		case EState::BlockStun:
		case EState::HitStun:
			return bCrouchStun;
		case EState::Move:
			return Move && (Move->HasTag(TEXT("crouch")) || FMoveDef::InRange(Move->LowProfile, MF));
		default:
			return false;
		}
	}

	bool FFighter::IsBlocking() const
	{
		return State == EState::Block || State == EState::CrouchBlock || (State == EState::BlockStun && IsGuard(StunKind));
	}

	bool FFighter::IsInvulnerable() const
	{
		if (Invuln > 0)
		{
			return true;
		}
		switch (State)
		{
		case EState::Down:
		case EState::WakeUp:
		case EState::KO:
		case EState::Intro:
		case EState::Win:
		case EState::Finisher:
		case EState::Victim:
		case EState::Thrown:
		case EState::Fall:
			return true;
		case EState::Juggle:
			return JugglePoints >= JuggleLimit;
		case EState::Move:
			return Move && FMoveDef::InRange(Move->Invuln, MF);
		default:
			return false;
		}
	}

	bool FFighter::IsProjInvulnerable() const
	{
		return IsInvulnerable() || (State == EState::Move && Move && FMoveDef::InRange(Move->ProjInvuln, MF));
	}

	bool FFighter::IsActionable() const
	{
		switch (State)
		{
		case EState::Idle:
		case EState::WalkF:
		case EState::WalkB:
		case EState::Crouch:
		case EState::Block:
		case EState::CrouchBlock:
		case EState::Run:
			return true;
		default:
			return false;
		}
	}

	FRect FFighter::ToWorld(const FRect& R) const
	{
		const double X0 = Pos.X + (Facing > 0 ? R.X : -(R.X + R.W));
		return FRect(X0, Pos.Y + R.Y, R.W, R.H);
	}

	void FFighter::Hurtboxes(TArray<FRect>& Out) const
	{
		Out.Reset();
		if (IsInvulnerable())
		{
			return;
		}
		const double K = Scale();
		TArrayView<const FRect> Src(BoxStand);
		if (IsAirborne())
		{
			Src = TArrayView<const FRect>(BoxAir);
		}
		else if (IsCrouching())
		{
			Src = TArrayView<const FRect>(BoxCrouch);
		}
		for (const FRect& R : Src)
		{
			Out.Add(ToWorld(R.Scaled(K)));
		}
		if (State == EState::Move && Move)
		{
			for (const FFrameBox& B : Move->Hurtboxes)
			{
				if (MF >= B.From && MF <= B.To)
				{
					Out.Add(ToWorld(B.Rect));
				}
			}
		}
	}

	void FFighter::ActiveHitboxes(TArray<TPair<int32, FRect>>& Out) const
	{
		Out.Reset();
		if (State != EState::Move || !Move || !Move->IsAttack() || Move->Level == ELevel::Throw)
		{
			return;
		}
		for (int32 I = 0; I < Move->Hitboxes.Num(); ++I)
		{
			const FFrameBox& B = Move->Hitboxes[I];
			if (MF >= B.From && MF <= B.To && !HitGroups.Contains(I))
			{
				Out.Emplace(I, ToWorld(B.Rect));
			}
		}
	}

	FRect FFighter::Pushbox() const
	{
		const double K = Scale();
		const double W = Def->PushW;
		if (IsAirborne())
		{
			return FRect(Pos.X - W * 0.85, Pos.Y + 0.3 * K, W * 1.7, 0.75 * K);
		}
		const double H = (IsCrouching() ? 1.05 : 1.55) * K;
		return FRect(Pos.X - W, Pos.Y, W * 2.0, H);
	}

	bool FFighter::HasPushbox() const
	{
		return State != EState::Down && State != EState::KO && State != EState::Thrown && State != EState::Finisher && State != EState::Victim;
	}

	// --- Tick ---------------------------------------------------------------------------------------

	void FFighter::Tick()
	{
		if (Hitstop > 0)
		{
			--Hitstop;
			return;
		}
		++SF;
		if (Invuln > 0)
		{
			--Invuln;
		}
		if (BuffTicks > 0)
		{
			--BuffTicks;
			if (BuffTicks == 0 || BuffHits <= 0)
			{
				BuffTicks = 0;
				Buff = NAME_None;
				FSimEvent E;
				E.Type = EEvent::BuffEnd;
				E.Who = Index;
				Sim->Emit(E);
			}
		}
		RegenRun();
		const bool bWasMoving = State == EState::Move;
		QhiTick();
		if (State == EState::Move && !bWasMoving)
		{
			// The body gave out or steadied itself this tick: that move has begun on its first frame.
			ApplyPush();
			return;
		}
		switch (State)
		{
		case EState::Intro:
		case EState::Win:
		case EState::Dizzy:
			Vel = FVector2D::ZeroVector;
			GravitySettle();
			break;
		case EState::Idle:
		case EState::WalkF:
		case EState::WalkB:
		case EState::Crouch:
		case EState::Block:
		case EState::CrouchBlock:
			Neutral();
			break;
		case EState::Run:
			RunTick();
			break;
		case EState::PreJump:
			// Motions ending in up (D,U+HK) arrive a tick or two after the jump began.
			if (Sim->CanAct() && TryCommands(Def->Commands))
			{
				return;
			}
			if (SF >= PrejumpFrames)
			{
				Jump();
			}
			break;
		case EState::Air:
			AirTick();
			break;
		case EState::Land:
			if (SF >= LandFramesNow)
			{
				ToNeutral();
			}
			break;
		case EState::Move:
			MoveTick();
			break;
		case EState::BlockStun:
		case EState::HitStun:
			StunTick();
			break;
		case EState::Juggle:
		case EState::Fall:
			FallTick();
			break;
		case EState::Down:
			if (SF >= DownFrames && Health > 0)
			{
				SetState(EState::WakeUp);
			}
			break;
		case EState::WakeUp:
			if (SF >= WakeupFrames)
			{
				ToNeutral();
			}
			break;
		case EState::KO:
			GravitySettle();
			break;
		default:
			break;
		}
		ApplyPush();
	}

	void FFighter::RegenRun()
	{
		if (State == EState::Run)
		{
			RunIdle = 0;
			return;
		}
		++RunIdle;
		if (Sim->bInfiniteRun)
		{
			RunMeter = RunMax;
		}
		else if (RunIdle > RunRegenDelay)
		{
			RunMeter = FMath::Min(RunMax, RunMeter + RunRegen);
		}
	}

	void FFighter::SpendRun(double Amount)
	{
		RunMeter = FMath::Max(0.0, RunMeter - Amount);
		RunIdle = 0;
	}

	// --- Qhi and shapes -------------------------------------------------------------------------------

	void FFighter::SpendQhi(double Amount)
	{
		Qhi = FMath::Max(0.0, Qhi - Amount);
		QhiIdle = 0;
	}

	void FFighter::DefendGain()
	{
		// Only his own body earns Qhi back: a borrowed shape's parries give nothing, so a copy
		// can never feed the meter that holds it.
		if (!UsesQhi() || Form == EForm::Borrowed || Sim->TickCount < QhiGainAt)
		{
			return;
		}
		const FQhiRules& R = QhiRules();
		Qhi = FMath::Min(R.Max, Qhi + (Form == EForm::Spent ? R.SpentDefendGain : R.DefendGain));
		QhiGainAt = Sim->TickCount + R.DefendGainEvery;
	}

	bool FFighter::FormSafe() const
	{
		if (Hitstop > 0 || ThrowVictim >= 0 || ThrownBy >= 0)
		{
			return false;
		}
		switch (State)
		{
		case EState::Idle:
		case EState::WalkF:
		case EState::WalkB:
		case EState::Crouch:
		case EState::Block:
		case EState::CrouchBlock:
		case EState::Run:
		case EState::Land:
			return true;
		default:
			return false;
		}
	}

	void FFighter::QhiTick()
	{
		if (!UsesQhi())
		{
			return;
		}
		const FQhiRules& R = QhiRules();
		if (Sim->bInfiniteRun)
		{
			Qhi = R.Max;
		}
		++QhiIdle;
		// A combo of his has ended: its changes of shape and its shapes are counted afresh.
		if (Opponent().ComboHits == 0)
		{
			FormShifts = 0;
			ComboShapes.Reset();
		}
		switch (Form)
		{
		case EForm::Own:
			// An empty meter does not refill on its own: the body is spent first.
			if (Qhi <= 0.0)
			{
				if (FormSafe() && Sim->Phase == EPhase::Fight)
				{
					Exhaust(TEXT("spent"));
				}
			}
			else if (QhiIdle > R.RegenDelay)
			{
				Qhi = FMath::Min(R.Max, Qhi + R.Regen);
			}
			break;
		case EForm::Spent:
			Qhi = FMath::Min(R.Max, Qhi + R.SpentRegen);
			if (Qhi >= R.RecoverAt && FormSafe())
			{
				SetForm(EForm::Own, BaseDef, TEXT("recentred"));
				if (const FMoveDef* M = Def->Move(TEXT("qhi_recentre")); M && State != EState::Run)
				{
					StartMove(*M);
				}
			}
			break;
		case EForm::Borrowed:
			if (Sim->Phase == EPhase::Fight)
			{
				Qhi = FMath::Max(0.0, Qhi - R.Upkeep * UpkeepScale);
			}
			if (Qhi <= 0.0)
			{
				bShapeBreaking = true;
			}
			if (FormSafe())
			{
				if (bShapeBreaking && Sim->Phase == EPhase::Fight)
				{
					Exhaust(TEXT("broken"));
				}
				else if (Sim->Phase != EPhase::Fight)
				{
					// The fight is decided: the shape is let go, at no cost and with no refund.
					SetForm(EForm::Own, BaseDef, TEXT("own"));
				}
			}
			break;
		}
	}

	void FFighter::Exhaust(FName Kind)
	{
		Qhi = 0.0;
		bShapeBreaking = false;
		SetForm(EForm::Spent, BaseDef->SpentForm ? BaseDef->SpentForm : BaseDef, Kind);
		Vel.X = 0.0;
		if (const FMoveDef* M = Def->Move(TEXT("qhi_spent")))
		{
			StartMove(*M);
		}
	}

	void FFighter::SetForm(EForm F, const FFighterDef* D, FName Kind, FName How)
	{
		Form = F;
		Def = D;
		QhiIdle = 0;
		FSimEvent E;
		E.Type = EEvent::Form;
		E.Who = Index;
		E.Kind = Kind;
		E.How = How;
		E.Move = D->CopyOf ? D->CopyOf->Id : NAME_None;
		Sim->Emit(E);
	}

	const FFighterDef* FFighter::BorrowTarget() const
	{
		// A fighter in a borrowed shape is copied from the shape's source, never from the copy.
		const FFighter& O = Opponent();
		return O.Def && O.Def->CopyOf ? O.Def->CopyOf : O.BaseDef;
	}

	const FFighterDef* FFighter::ShapeTarget(const FMoveDef& M) const
	{
		return M.FormTarget.IsNone() ? BorrowTarget() : BaseDef->FormSources.FindRef(M.FormTarget);
	}

	static FName RefusalFor(const FFighterDef* T, const FFighterDef* Base, const FFighterDef* Now, bool bEnhanced)
	{
		if (!T)
		{
			return TEXT("shapeless");
		}
		// A Grandmaster copied is a mirror: it shows only itself.
		if (T->Id == Base->Id || T->Resource == EResource::Qhi)
		{
			return TEXT("mirror");
		}
		if (!T->bCopyable)
		{
			return TEXT("shapeless");
		}
		// Already wearing that very shape (copied or remembered alike).
		if (Now->CopyOf == T && (Now->Power != T->Power) == bEnhanced)
		{
			return TEXT("same");
		}
		return NAME_None;
	}

	FName FFighter::BorrowRefusal() const
	{
		return RefusalFor(BorrowTarget(), BaseDef, Def, true);
	}

	FName FFighter::ShapeRefusal(const FMoveDef& M) const
	{
		return RefusalFor(ShapeTarget(M), BaseDef, Def, M.bFormEnhanced);
	}

	double FFighter::ShapeCost(const FMoveDef& M) const
	{
		// Copying the opponent facing him is the owner's fixed price: half the meter's capacity.
		return M.FormCost < 0.0 || M.FormTarget.IsNone() ? QhiRules().ShapeshiftCost() : M.FormCost;
	}

	bool FFighter::KillAllowed(const FMoveDef& M) const
	{
		const FFighter& O = Opponent();
		if (M.KillRequires == TEXT("standing"))
		{
			// Begun only against an opponent free on the ground: its long telegraph must be seen.
			return O.IsActionable() && !O.IsAirborne() && O.Health > 0;
		}
		if (M.KillRequires == TEXT("combo"))
		{
			// The end of a long combo that took at least two of his shapes.
			return (O.State == EState::HitStun || O.State == EState::Juggle) && O.ComboHits >= M.KillCombo && ComboShapes.Num() >= 2;
		}
		return true;
	}

	void FFighter::Refuse(FName Why)
	{
		if (Sim->TickCount - RefusedAt < 30)
		{
			return;
		}
		RefusedAt = Sim->TickCount;
		FSimEvent E;
		E.Type = EEvent::Form;
		E.Who = Index;
		E.Kind = TEXT("refused");
		E.How = Why;
		Sim->Emit(E);
	}

	void FFighter::ChangeShape(const FMoveDef& M)
	{
		const bool bQuick = bQuickShift;
		bQuickShift = false;
		if (M.bShapeshift)
		{
			const FName Why = ShapeRefusal(M);
			const double Cost = ShapeCost(M);
			if (Sim->Phase != EPhase::Fight)
			{
				// The round ended during the startup: nothing happens, nothing is paid.
			}
			else if (!Why.IsNone())
			{
				Refuse(Why);
			}
			else if (Qhi < Cost)
			{
				// Spent on something else meanwhile (a blow taken in a borrowed shape).
				Refuse(TEXT("qhi"));
			}
			else
			{
				SpendQhi(Cost);
				RunMeter = RunMax;
				RunIdle = 0;
				bShapeBreaking = false;
				const FFighterDef* Target = ShapeTarget(M);
				UpkeepScale = M.FormUpkeep * Target->ShapeUpkeep;
				SetForm(EForm::Borrowed, Sim->ShapeOf(*Target, *BaseDef, M.bFormEnhanced), TEXT("borrowed"), M.bFormEnhanced ? FName(TEXT("copied")) : FName(TEXT("remembered")));
				FormShifts += bQuick ? 1 : 0;
			}
		}
		else if (Form == EForm::Borrowed)
		{
			UpkeepScale = 1.0;
			SetForm(EForm::Own, BaseDef, TEXT("own"));
			FormShifts += bQuick ? 1 : 0;
		}
		Move = nullptr;
		MF = 0;
		Vel = FVector2D::ZeroVector;
		if (bQuick)
		{
			// Inside a combo the new shape carries straight on: the opponent's stun runs as it was.
			ToNeutral();
			return;
		}
		// From a standstill the new body steadies itself before it can act.
		SetState(EState::Land);
		LandFramesNow = 10;
	}

	void FFighter::BreakMove()
	{
		if (Move && State == EState::Move && Move->bShapeshift && MF < Move->FormFrame)
		{
			SpendQhi(QhiRules().FailCost);
			FSimEvent E;
			E.Type = EEvent::Form;
			E.Who = Index;
			E.Kind = TEXT("failed");
			Sim->Emit(E);
		}
		Move = nullptr;
	}

	bool FFighter::CanStartRun() const
	{
		switch (Def->Resource)
		{
		case EResource::Run: return RunMeter >= RunMinStart;
		case EResource::Qhi: return Qhi >= QhiRules().GlideMin;
		default: return false;
		}
	}

	void FFighter::GravitySettle()
	{
		if (Pos.Y > 0.0)
		{
			Vel.Y -= Def->Gravity * DT;
			Pos.Y = FMath::Max(0.0, Pos.Y + Vel.Y * DT);
		}
	}

	void FFighter::ApplyPush()
	{
		if (PushV != 0.0)
		{
			Pos.X += PushV * DT;
			PushV = MoveToward(PushV, 0.0, PushDec);
		}
	}

	void FFighter::Push(double Dist, int32 From)
	{
		PushV = Dist * 15.0;
		PushDec = FMath::Abs(PushV) / 8.0;
		PushFrom = From;
	}

	void FFighter::ToNeutral()
	{
		Move = nullptr;
		MF = 0;
		SelfCancelsUsed = 0;
		Vel.X = 0.0;
		ComboReset();
		const int32 D = Input.Dir();
		if ((Input.Held() & BtnBL) && Sim->CanAct())
		{
			SetState(FInputBuffer::IsDown(D) ? EState::CrouchBlock : EState::Block);
		}
		else if (FInputBuffer::IsDown(D) && Sim->CanAct())
		{
			SetState(EState::Crouch);
		}
		else
		{
			SetState(EState::Idle);
		}
	}

	void FFighter::ComboReset()
	{
		JugglePoints = 0;
		if (ComboHits > 0 && Sim)
		{
			FSimEvent E;
			E.Type = EEvent::ComboEnd;
			E.Who = Index;
			E.Hits = ComboHits;
			E.Damage = ComboDamage;
			Sim->Emit(E);
		}
		ComboHits = 0;
		ComboDamage = 0;
		bDazedThisCombo = false;
	}

	// --- Neutral, walking, running, jumping -----------------------------------------------------------

	void FFighter::Neutral()
	{
		int32 D = Input.Dir();
		int32 HeldNow = Input.Held();
		if (!Sim->CanAct() || Health <= 0)
		{
			Vel.X = 0.0;
			SetState(EState::Idle);
			return;
		}
		if (Sim->TryFinisher(*this))
		{
			return;
		}
		if (TryCommands(Def->Commands))
		{
			return;
		}
		if (!AutoBlock.IsNone())
		{
			HeldNow |= BtnBL;
			D = AutoBlock == TEXT("crouch") ? 2 : 5;
		}
		if (HeldNow & BtnBL)
		{
			Vel.X = 0.0;
			SetState(FInputBuffer::IsDown(D) ? EState::CrouchBlock : EState::Block);
			return;
		}
		if (FInputBuffer::IsUp(D))
		{
			JumpDir = D == 9 ? 1 : (D == 7 ? -1 : 0);
			Vel.X = 0.0;
			SetState(EState::PreJump);
			return;
		}
		if (FInputBuffer::IsDown(D))
		{
			Vel.X = 0.0;
			SetState(EState::Crouch);
			return;
		}
		if (D == 6 && (HeldNow & BtnRN) && CanStartRun())
		{
			SetState(EState::Run);
			FSimEvent E;
			E.Type = EEvent::Run;
			E.Who = Index;
			Sim->Emit(E);
			RunTick();
			return;
		}
		if (D == 6)
		{
			SetState(EState::WalkF);
			Vel.X = Def->WalkF * Facing;
		}
		else if (D == 4)
		{
			SetState(EState::WalkB);
			Vel.X = -Def->WalkB * Facing;
		}
		else
		{
			SetState(EState::Idle);
			Vel.X = 0.0;
		}
		Pos.X += Vel.X * DT;
	}

	void FFighter::RunTick()
	{
		const int32 D = Input.Dir();
		if (TryCommands(Def->Commands))
		{
			return;
		}
		// The Grandmaster's own body does not run: it glides on Qhi.
		const bool bGlide = Def->Resource == EResource::Qhi;
		if (D != 6 || !(Input.Held() & BtnRN) || (bGlide ? Qhi <= 0.0 : RunMeter <= 0.0) || (Input.Held() & BtnBL) || Def->Resource == EResource::None)
		{
			ToNeutral();
			return;
		}
		if (bGlide)
		{
			SpendQhi(QhiRules().GlideDrain);
		}
		else if (!Sim->bInfiniteRun)
		{
			RunMeter = FMath::Max(0.0, RunMeter - RunDrain);
		}
		Vel.X = Def->RunSpeed * Facing;
		Pos.X += Vel.X * DT;
	}

	void FFighter::Jump()
	{
		Vel = FVector2D(Def->JumpVx * JumpDir * Facing, Def->JumpVy);
		bAirAttackUsed = false;
		SetState(EState::Air);
		FSimEvent E;
		E.Type = EEvent::Jump;
		E.Who = Index;
		Sim->Emit(E);
		AirTick();
	}

	void FFighter::AirTick()
	{
		if (!bAirAttackUsed && Sim->CanAct() && TryCommands(Def->AirCommands))
		{
			bAirAttackUsed = true;
			return;
		}
		IntegrateAir(Def->Gravity);
		if (Landed())
		{
			LandNow(LandFrames);
		}
	}

	void FFighter::IntegrateAir(double G)
	{
		Vel.Y -= G * DT;
		Pos += Vel * DT;
	}

	void FFighter::LandNow(int32 Frames)
	{
		Pos.Y = 0.0;
		Move = nullptr;
		SetState(EState::Land);
		LandFramesNow = Frames;
		Vel = FVector2D::ZeroVector;
		FSimEvent E;
		E.Type = EEvent::Land;
		E.Who = Index;
		E.bSoft = true;
		Sim->Emit(E);
	}

	// --- Commands and moves ---------------------------------------------------------------------------

	bool FFighter::TryCommands(const TArray<FCommand>& List, int32 MinPriority, bool bSpecialsOnly, bool bFeint, bool bShift)
	{
		if (Input.PressAge(BtnAll) < 0)
		{
			return false;
		}
		for (const FCommand& C : List)
		{
			if (C.Priority() <= MinPriority)
			{
				continue;
			}
			const FMoveDef* M = Def->Move(C.Move);
			if (!M || (bSpecialsOnly && M->Kind != EMoveKind::Special))
			{
				continue;
			}
			// A change of shape is only ever started from a standstill, never cancelled into; a
			// feint never cancels into itself.
			if ((bSpecialsOnly || bFeint) && (M->bShapeshift || M->bRevert))
			{
				continue;
			}
			if (bFeint && M == Move)
			{
				continue;
			}
			if (bShift && !(M->bShapeshift || M->bRevert))
			{
				continue;
			}
			// A cancel into the same move only as many times running as the move allows: a rush
			// that cancels into itself without end is an infinite.
			const bool bSelf = bSpecialsOnly && M == Move;
			if (bSelf && SelfCancelsUsed >= M->SelfCancels)
			{
				continue;
			}
			const int32 Age = Input.ChordAge(C.Mask);
			if (Age < 0 || !C.Matches(Input, Age, Facing))
			{
				continue;
			}
			if (!CanUse(*M))
			{
				if (M->bShapeshift && Form != EForm::Spent && Sim->Phase == EPhase::Fight && !(M->FormTarget.IsNone() && Form == EForm::Borrowed))
				{
					// Said aloud, so the player knows why nothing happened.
					const FName Why = ShapeRefusal(*M);
					Refuse(Why.IsNone() ? FName(TEXT("qhi")) : Why);
				}
				continue;
			}
			Input.Consume(C.Mask);
			MoveCmdPriority = C.Priority();
			const int32 Used = bSelf ? SelfCancelsUsed + 1 : 0;
			StartMove(*M);
			SelfCancelsUsed = Used;
			if (bShift)
			{
				// Taken inside the combo: it takes on the shift frame (MoveFrame has run frame 1).
				bQuickShift = true;
			}
			return true;
		}
		return false;
	}

	bool FFighter::CanUse(const FMoveDef& M) const
	{
		if (M.bFinisher)
		{
			return false;
		}
		if (M.Projectile.bHas && ProjectilesOut > 0)
		{
			return false;
		}
		if (M.RunCost > 0.0 && RunMeter < M.RunCost && !Sim->bInfiniteRun)
		{
			return false;
		}
		if (M.QhiCost > 0.0 && (!UsesQhi() || Qhi < M.QhiCost))
		{
			return false;
		}
		if (M.bShapeshift && (Form == EForm::Spent || Sim->Phase != EPhase::Fight || Qhi < ShapeCost(M) || !ShapeRefusal(M).IsNone()))
		{
			return false;
		}
		if (M.bShapeshift && M.FormTarget.IsNone() && Form == EForm::Borrowed)
		{
			// The opponent's shape is copied from his own body: in a borrowed shape the same input
			// is Own Face.
			return false;
		}
		if (M.bKill && (!UsesQhi() || !KillAllowed(M)))
		{
			return false;
		}
		if (M.bRevert && Form != EForm::Borrowed)
		{
			return false;
		}
		if (M.bAir != (State == EState::Air))
		{
			return false;
		}
		if (M.HealthBelow >= 0.0 && double(Health) / MaxHealth() >= M.HealthBelow)
		{
			return false;
		}
		if (!M.Buff.IsNone() && BuffTicks > 0)
		{
			return false;
		}
		if (M.Requires == TEXT("close"))
		{
			const FFighter& O = Opponent();
			if (FMath::Abs(O.Pos.X - Pos.X) > Def->ThrowRange || O.IsAirborne() || O.IsInvulnerable())
			{
				return false;
			}
			if (O.Def->bUnthrowable && M.IsThrowKind())
			{
				return false;
			}
			if (O.State == EState::HitStun || O.State == EState::BlockStun || O.State == EState::Thrown || O.State == EState::Juggle
				|| (O.State == EState::Move && O.Move && O.Move->IsThrowKind() && O.MF > O.Move->Startup))
			{
				return false;
			}
		}
		else if (M.Requires == TEXT("straight"))
		{
			if (JumpDir != 0)
			{
				return false;
			}
		}
		else if (M.Requires == TEXT("angled"))
		{
			if (JumpDir == 0)
			{
				return false;
			}
		}
		return true;
	}

	void FFighter::StartMove(const FMoveDef& M)
	{
		Move = &M;
		MF = 1;
		MoveOrigin = Pos;
		MoveContact = NAME_None;
		HitGroups.Reset();
		ArmorLeft = M.Armor != FIntPoint::ZeroValue ? M.ArmorHits : 0;
		if (M.RunCost > 0.0 && !Sim->bInfiniteRun)
		{
			SpendRun(M.RunCost);
		}
		if (M.QhiCost > 0.0 || M.bQhiAll)
		{
			SpendQhi(M.bQhiAll ? FMath::Max(Qhi, M.QhiCost) : M.QhiCost);
		}
		bQuickShift = false;
		if (!M.bAir)
		{
			Vel = FVector2D::ZeroVector;
		}
		SetState(EState::Move);
		FSimEvent E;
		E.Type = EEvent::Move;
		E.Who = Index;
		E.Move = M.Id;
		Sim->Emit(E);
		MoveFrame();
	}

	void FFighter::MoveTick()
	{
		if (ThrowVictim >= 0)
		{
			// The simulation drives the throw sequence.
			return;
		}
		++MF;
		// Input only shapes a move while the fight is live: after a knockout nothing chains on.
		const bool bLive = Sim->CanAct();
		// A plain button that turns out to be the first half of a chord or a motion upgrades to the
		// more specific command within the chord window.
		if (bLive && MF <= FInputBuffer::ChordTicks + 1 && Move->Kind == EMoveKind::Normal && !Move->bAir)
		{
			if (TryCommands(Def->Commands, MoveCmdPriority))
			{
				return;
			}
		}
		if (bLive && TryChain())
		{
			return;
		}
		if (bLive && FMoveDef::InRange(Move->Feint, MF) && TryCommands(Def->Commands, -1, false, true))
		{
			return;
		}
		// The Grandmaster changes shape inside his combos: once a blow of his has landed, any time
		// after its first active frame while the opponent is still caught, a limited number of times.
		if (bLive && UsesQhi() && Form != EForm::Spent && MoveContact == TEXT("hit") && MF > Move->Startup && FormShifts < QhiRules().MaxShifts
			&& (Opponent().State == EState::HitStun || Opponent().State == EState::Juggle) && TryCommands(Def->Commands, -1, false, false, true))
		{
			return;
		}
		if (bLive && Move->Cancel != FIntPoint::ZeroValue && !MoveContact.IsNone() && FMoveDef::InRange(Move->Cancel, MF))
		{
			if (TryCommands(Def->Commands, -1, true))
			{
				return;
			}
		}
		if (Move->bAir)
		{
			if (Move->Lift.Num() == 0)
			{
				IntegrateAir(Def->Gravity);
			}
			else
			{
				MoveFrame();
			}
			if (Landed() && MF > 2)
			{
				LandNow(MoveContact.IsNone() ? LandFramesAttack : LandFrames);
				return;
			}
			if (MF > Move->Total)
			{
				// The attack is spent; the rest of the jump continues without it.
				Move = nullptr;
				SetState(EState::Air);
				bAirAttackUsed = true;
				return;
			}
			if (Move->Lift.Num() > 0)
			{
				return;
			}
			SpawnCheck();
			return;
		}
		if (MF > Move->Total)
		{
			const bool bCrouched = Move->bEndsCrouched;
			Move = nullptr;
			ToNeutral();
			if (bCrouched && FInputBuffer::IsDown(Input.Dir()))
			{
				SetState(EState::Crouch);
			}
			return;
		}
		MoveFrame();
	}

	bool FFighter::TryChain()
	{
		for (const FChainLink& L : Move->Chain)
		{
			if (MF < L.From || MF > L.To)
			{
				continue;
			}
			const int32 Age = Input.ChordAge(L.Mask);
			if (Age < 0)
			{
				continue;
			}
			if (L.Steps.Num() == 1 && !FCommand::HeldOk(L.Steps[0], Input.DirAs(Age, Facing)))
			{
				continue;
			}
			// Links spend run meter; the Grandmaster's own body pays a share of it in Qhi, and his
			// spent body, which has neither, pays nothing.
			const bool bQhiLink = Def->Resource == EResource::Qhi;
			const double Cost = bQhiLink ? L.Cost * QhiRules().ChainShare : (Def->Resource == EResource::Run ? L.Cost : 0.0);
			if (!Sim->bInfiniteRun && (bQhiLink ? Qhi < Cost : RunMeter < Cost))
			{
				continue;
			}
			const FMoveDef* M = Def->Move(L.Move);
			if (!M)
			{
				continue;
			}
			Input.Consume(L.Mask);
			if (bQhiLink)
			{
				SpendQhi(Cost);
			}
			else if (!Sim->bInfiniteRun)
			{
				SpendRun(Cost);
			}
			const FName Contact = MoveContact;
			StartMove(*M);
			FSimEvent E;
			E.Type = EEvent::Chain;
			E.Who = Index;
			E.Move = M->Id;
			E.After = Contact;
			Sim->Emit(E);
			return true;
		}
		return false;
	}

	void FFighter::MoveFrame()
	{
		const FMoveDef& M = *Move;
		if ((M.bShapeshift || M.bRevert) && MF == (bQuickShift ? QhiRules().ShiftFrame : M.FormFrame))
		{
			ChangeShape(M);
			return;
		}
		if (M.Motion.Num() > 0)
		{
			const double DX = FMoveDef::CurveAt(M.Motion, MF) - FMoveDef::CurveAt(M.Motion, MF - 1);
			Pos.X += DX * Facing;
		}
		if (M.Lift.Num() > 0)
		{
			Pos.Y = FMath::Max(0.0, MoveOrigin.Y + FMoveDef::CurveAt(M.Lift, MF));
		}
		if (M.Track != FVector::ZeroVector && MF >= (int32)M.Track.X && MF <= (int32)M.Track.Y)
		{
			Pos.X = MoveToward(Pos.X, Opponent().Pos.X, M.Track.Z * DT);
		}
		if (M.bTeleport && MF == M.TeleportFrame)
		{
			// Reappears on the far side of the opponent, facing them.
			const FFighter& O = Opponent();
			const double D = M.TeleportDist * Scale();
			const int32 Side = -O.Facing;
			const double Lo = -FFightSim::StageHalf + Def->PushW;
			const double Hi = FFightSim::StageHalf - Def->PushW;
			Pos.X = FMath::Clamp(O.Pos.X + Side * D, Lo, Hi);
			if (FMath::Abs(Pos.X - O.Pos.X) < D * 0.5)
			{
				Pos.X = FMath::Clamp(O.Pos.X - Side * D, Lo, Hi);
			}
			Facing = O.Pos.X > Pos.X ? 1 : -1;
			FSimEvent E;
			E.Type = EEvent::Teleport;
			E.Who = Index;
			Sim->Emit(E);
		}
		if (M.bAirVel && MF == (int32)M.AirVel.X)
		{
			Vel = FVector2D(M.AirVel.Y * Facing, M.AirVel.Z);
		}
		if (!M.Buff.IsNone() && MF == M.BuffFrame)
		{
			Buff = M.Buff;
			BuffTicks = M.BuffTicks;
			BuffHits = M.BuffHits;
			FSimEvent E;
			E.Type = EEvent::Buff;
			E.Who = Index;
			E.Kind = Buff;
			Sim->Emit(E);
		}
		SpawnCheck();
	}

	void FFighter::SpawnCheck()
	{
		if (Move && Move->Projectile.bHas && MF == Move->Projectile.Frame)
		{
			Sim->SpawnProjectile(*this, *Move);
		}
	}

	// --- Being hit --------------------------------------------------------------------------------------

	void FFighter::StunTick()
	{
		--Stun;
		if (State == EState::BlockStun)
		{
			// The guard can change height while it holds.
			bCrouchStun = FInputBuffer::IsDown(Input.Dir()) || AutoBlock == TEXT("crouch");
			StunKind = bCrouchStun ? EStun::CrouchGuard : EStun::Guard;
		}
		if (StunKind == EStun::Pulled)
		{
			// Reeled in toward the puller until close, then held briefly.
			const FFighter& O = Opponent();
			const double Gap = FMath::Abs(O.Pos.X - Pos.X);
			if (Gap > 0.85 * (Scale() + O.Scale()) * 0.5 + 0.35)
			{
				Pos.X = MoveToward(Pos.X, O.Pos.X, 9.0 * DT);
			}
		}
		if (Pos.Y > 0.0)
		{
			IntegrateAir(Def->Gravity);
			Pos.Y = FMath::Max(Pos.Y, 0.0);
		}
		if (Stun <= 0)
		{
			if (StunKind == EStun::Crumple)
			{
				Knock(FVector2D(0.6, 1.2), false);
				return;
			}
			ToNeutral();
		}
	}

	void FFighter::FallTick()
	{
		const double G = Def->Gravity * (1.0 + 0.12 * JugglePoints);
		IntegrateAir(G);
		if (Landed())
		{
			Pos.Y = 0.0;
			Vel = FVector2D::ZeroVector;
			ComboReset();
			SetState(Health <= 0 && !Sim->AwaitingDizzy(*this) ? EState::KO : EState::Down);
			FSimEvent E;
			E.Type = EEvent::Land;
			E.Who = Index;
			E.bSoft = false;
			Sim->Emit(E);
		}
	}

	void FFighter::ReleaseThrow()
	{
		if (ThrowVictim < 0)
		{
			return;
		}
		FFighter& V = Sim->Fighters[ThrowVictim];
		ThrowVictim = -1;
		V.ThrownBy = -1;
		V.Knock(FVector2D(1.0, 2.0), false, Facing);
	}

	void FFighter::Knock(const FVector2D& V, bool bJuggle, int32 FromFacing)
	{
		ReleaseThrow();
		const int32 Away = FromFacing != 0 ? FromFacing : -Facing;
		Vel = FVector2D(V.X * Away, V.Y);
		if (Pos.Y <= 0.0 && V.Y > 0.0)
		{
			Pos.Y = 0.01;
		}
		BreakMove();
		SetState(bJuggle ? EState::Juggle : EState::Fall);
	}

	void FFighter::TakeStun(int32 Frames, EStun Kind)
	{
		ReleaseThrow();
		BreakMove();
		Stun = Frames;
		StunKind = Kind;
		bCrouchStun = Kind == EStun::Crouch || Kind == EStun::CrouchGuard;
		SetState(IsGuard(Kind) ? EState::BlockStun : EState::HitStun);
	}
}
