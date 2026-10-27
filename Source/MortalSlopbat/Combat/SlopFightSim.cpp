#include "Combat/SlopSim.h"

namespace Slop
{

	static double SignD(double V) { return V > 0.0 ? 1.0 : (V < 0.0 ? -1.0 : 0.0); }

	static FSimEvent Ev(EEvent T, int32 Who = -1, int32 By = -1)
	{
		FSimEvent E;
		E.Type = T;
		E.Who = Who;
		E.By = By;
		return E;
	}

	// --- Projectiles --------------------------------------------------------------------------------

	void FProjectile::Step(const FVector2D& Home)
	{
		const FProjectileDef& D = Def();
		++Age;
		if (D.bReturns && Age > Life / 2)
		{
			const FVector2D To = Home - Pos;
			const double Speed = FMath::Abs(Vel.X) > 0.1 ? FMath::Abs(Vel.X) : 8.0;
			if (To.Size() < Speed * DT * 1.5)
			{
				bDead = true;
				return;
			}
			Vel = To.GetSafeNormal() * Speed;
		}
		else
		{
			Vel.Y -= Gravity * DT;
			if (Decel > 0.0 && FMath::Abs(Vel.X) > MinSpeed)
			{
				Vel.X = MoveToward(Vel.X, MinSpeed * Facing, Decel * DT);
			}
		}
		Pos += Vel * DT;
		if (Pos.Y - Size.Y * 0.5 < 0.0 && Vel.Y <= 0.0)
		{
			if (D.Ground == TEXT("die"))
			{
				if (Gravity > 0.0)
				{
					bDead = true;
				}
			}
			else if (D.Ground == TEXT("roll"))
			{
				Pos.Y = Size.Y * 0.5;
				Vel.Y = 0.0;
			}
			else if (D.Ground == TEXT("bounce"))
			{
				Pos.Y = Size.Y * 0.5;
				Vel.Y = FMath::Abs(Vel.Y) * 0.45;
			}
		}
		if (Age >= Life)
		{
			bDead = true;
		}
	}

	FVector2D PathAt(const TArray<FVector>& Path, int32 T)
	{
		FVector Prev = Path[0];
		for (const FVector& K : Path)
		{
			if (T <= (int32)K.X)
			{
				const double Span = FMath::Max(K.X - Prev.X, 1.0);
				const double U = FMath::Clamp((T - Prev.X) / Span, 0.0, 1.0);
				return FVector2D(FMath::Lerp(Prev.Y, K.Y, U), FMath::Lerp(Prev.Z, K.Z, U));
			}
			Prev = K;
		}
		return FVector2D(Prev.Y, Prev.Z);
	}

	// --- Setup and rounds ----------------------------------------------------------------------------

	void FFightSim::Setup(const FFighterDef* D1, const FFighterDef* D2)
	{
		MatchDamage[0] = MatchDamage[1] = 0;
		for (int32 I = 0; I < 2; ++I)
		{
			Fighters[I] = FFighter();
			Fighters[I].Setup(this, I, I == 0 ? D1 : D2);
		}
		StartRound(bIntro);
	}

	void FFightSim::Emit(FSimEvent E)
	{
		E.Tick = TickCount;
		Events.Add(E);
	}

	void FFightSim::StartRound(bool bWithIntro)
	{
		Projectiles.Reset();
		for (FFighter& F : Fighters)
		{
			F.Health = F.MaxHealth();
			F.RunMeter = FFighter::RunMax;
			F.RunIdle = 0;
			F.Pos = FVector2D(StartGap * (F.Index == 0 ? -0.5 : 0.5), 0.0);
			F.Facing = F.Index == 0 ? 1 : -1;
			F.Vel = FVector2D::ZeroVector;
			F.Move = nullptr;
			F.MF = 0;
			F.Stun = 0;
			F.Hitstop = 0;
			F.PushV = 0.0;
			F.Invuln = 0;
			F.JugglePoints = 0;
			F.ComboHits = 0;
			F.ComboDamage = 0;
			F.ProjectilesOut = 0;
			F.DamageTaken = 0;
			F.ThrowVictim = -1;
			F.ThrownBy = -1;
			F.Buff = NAME_None;
			F.BuffTicks = 0;
			F.Input.Clear();
			F.SetState(EState::Intro);
			F.SF = 0;
		}
		Timer = RoundTicks;
		RoundWinner = -1;
		RoundHow = NAME_None;
		FinishWinner = -1;
		bFinishDizzy = false;
		FinisherWho = -1;
		FinisherMove = NAME_None;
		bFinisherStage = false;
		Flawless[0] = Flawless[1] = false;
		Phase = bWithIntro ? EPhase::Intro : EPhase::Ready;
		PhaseT = 0;
		if (!bWithIntro)
		{
			FSimEvent E = Ev(EEvent::RoundStart);
			E.Round = RoundNum;
			Emit(E);
		}
	}

	void FFightSim::Step(const FPadState Pads[2])
	{
		Events.Reset();
		++TickCount;
		++PhaseT;
		for (int32 I = 0; I < 2; ++I)
		{
			FFighter& F = Fighters[I];
			const FPadState& P = Pads[I];
			const int32 Screen = FInputBuffer::ScreenFrom((P.Dirs & PadRight) != 0, (P.Dirs & PadLeft) != 0, (P.Dirs & PadUp) != 0, (P.Dirs & PadDown) != 0);
			F.Input.Push(Screen, F.Facing, P.Held, P.Pressed, F.Hitstop > 0);
		}
		PhaseTick();
		UpdateFacing();
		if (Phase == EPhase::Finisher)
		{
			FinisherTick();
		}
		else
		{
			for (FFighter& F : Fighters)
			{
				F.Tick();
			}
			for (FFighter& F : Fighters)
			{
				if (F.ThrowVictim >= 0 && F.Hitstop <= 0)
				{
					ThrowTick(F);
				}
			}
		}
		StepProjectiles();
		Separate();
		Clamp();
		if (Phase == EPhase::Fight || Phase == EPhase::Finish)
		{
			Resolve();
		}
		if (Hooks)
		{
			Hooks->OnTick(*this);
		}
		CheckRound();
		if (bRefillHealth)
		{
			for (FFighter& F : Fighters)
			{
				if (F.Health < F.MaxHealth() && F.IsActionable() && F.SF > 30 && F.Opponent().IsActionable())
				{
					F.Health = F.MaxHealth();
				}
			}
		}
	}

	void FFightSim::PhaseTick()
	{
		switch (Phase)
		{
		case EPhase::Intro:
			if (PhaseT >= IntroTicks)
			{
				Phase = EPhase::Ready;
				PhaseT = 0;
				FSimEvent E = Ev(EEvent::RoundStart);
				E.Round = RoundNum;
				Emit(E);
			}
			break;
		case EPhase::Ready:
			for (FFighter& F : Fighters)
			{
				if (F.State == EState::Intro && PhaseT > 20)
				{
					F.SetState(EState::Idle);
				}
			}
			if (PhaseT >= ReadyTicks)
			{
				Phase = EPhase::Fight;
				PhaseT = 0;
				for (FFighter& F : Fighters)
				{
					F.SetState(EState::Idle);
				}
				Emit(Ev(EEvent::Fight));
			}
			break;
		case EPhase::Fight:
			if (!bTraining)
			{
				Timer = FMath::Max(0, Timer - 1);
			}
			break;
		default:
			break;
		}
	}

	void FFightSim::UpdateFacing()
	{
		for (FFighter& F : Fighters)
		{
			switch (F.State)
			{
			case EState::Idle:
			case EState::WalkF:
			case EState::WalkB:
			case EState::Crouch:
			case EState::Block:
			case EState::CrouchBlock:
			case EState::Land:
			case EState::Intro:
			case EState::Dizzy:
			case EState::PreJump:
			{
				const double DX = F.Opponent().Pos.X - F.Pos.X;
				if (FMath::Abs(DX) > 0.02)
				{
					const int32 Want = DX > 0.0 ? 1 : -1;
					if (Want != F.Facing)
					{
						F.Facing = Want;
						if (F.State == EState::WalkF || F.State == EState::WalkB)
						{
							F.SetState(EState::Idle);
						}
					}
				}
				break;
			}
			default:
				break;
			}
		}
	}

	// --- Movement constraints ---------------------------------------------------------------------------

	void FFightSim::Separate()
	{
		FFighter& A = Fighters[0];
		FFighter& B = Fighters[1];
		if (!A.HasPushbox() || !B.HasPushbox())
		{
			return;
		}
		const FRect RA = A.Pushbox();
		const FRect RB = B.Pushbox();
		if (!RA.Intersects(RB))
		{
			return;
		}
		double Overlap = FMath::Min(RA.EndX(), RB.EndX()) - FMath::Max(RA.X, RB.X);
		if (Overlap <= 0.0)
		{
			return;
		}
		double Dir = SignD(B.Pos.X - A.Pos.X);
		if (Dir == 0.0)
		{
			Dir = A.Facing;
		}
		// A grounded body holds its place against one landing on it, which slides off over a few
		// ticks rather than popping aside; two grounded bodies share the push.
		double ShareA = 0.5;
		if (A.IsAirborne() != B.IsAirborne())
		{
			ShareA = A.IsAirborne() ? 1.0 : 0.0;
			Overlap = FMath::Min(Overlap, 0.07);
		}
		A.Pos.X -= Dir * Overlap * ShareA;
		B.Pos.X += Dir * Overlap * (1.0 - ShareA);
	}

	void FFightSim::Clamp()
	{
		for (FFighter& F : Fighters)
		{
			const double Lo = -StageHalf + F.Def->PushW;
			const double Hi = StageHalf - F.Def->PushW;
			const double Want = F.Pos.X;
			F.Pos.X = FMath::Clamp(F.Pos.X, Lo, Hi);
			const double Overflow = Want - F.Pos.X;
			if (Overflow != 0.0 && F.PushV != 0.0 && F.PushFrom >= 0 && F.PushFrom != F.Index)
			{
				// Pushed into the wall: the attacker is pushed back instead.
				FFighter& Att = Fighters[F.PushFrom];
				if (!Att.IsAirborne())
				{
					Att.Pos.X -= Overflow;
				}
			}
		}
		FFighter& A = Fighters[0];
		FFighter& B = Fighters[1];
		const double Sep = FMath::Abs(B.Pos.X - A.Pos.X);
		if (Sep > MaxSep)
		{
			const double Excess = Sep - MaxSep;
			const double Dir = SignD(B.Pos.X - A.Pos.X);
			const bool bAAway = SignD(A.Vel.X + A.PushV) == -Dir;
			const bool bBAway = SignD(B.Vel.X + B.PushV) == Dir;
			double KA = 0.5;
			if (bAAway && !bBAway)
			{
				KA = 1.0;
			}
			else if (bBAway && !bAAway)
			{
				KA = 0.0;
			}
			A.Pos.X += Dir * Excess * KA;
			B.Pos.X -= Dir * Excess * (1.0 - KA);
			for (FFighter& F : Fighters)
			{
				F.Pos.X = FMath::Clamp(F.Pos.X, -StageHalf + F.Def->PushW, StageHalf - F.Def->PushW);
			}
		}
	}

	// --- Projectiles -------------------------------------------------------------------------------------

	void FFightSim::SpawnProjectile(FFighter& Owner, const FMoveDef& M)
	{
		const FProjectileDef& D = M.Projectile;
		const double K = D.Scale;
		FProjectile P;
		P.Owner = Owner.Index;
		P.Move = &M;
		P.Kind = D.Kind;
		P.Facing = Owner.Facing;
		P.Pos = Owner.Pos + FVector2D(D.Offset.X * K * Owner.Facing, D.Offset.Y * K);
		if (D.bAtOpponent)
		{
			// Appears over the opponent's position (falling anchors), clamped to a reach.
			const FFighter& Opp = Owner.Opponent();
			const double DX = FMath::Clamp((Opp.Pos.X - Owner.Pos.X) * Owner.Facing, D.AtOpponent.X, D.AtOpponent.Y);
			P.Pos.X = Owner.Pos.X + DX * Owner.Facing;
		}
		P.Vel = FVector2D(D.Speed * Owner.Facing, D.Vy);
		P.Gravity = D.Gravity;
		P.Decel = D.Decel;
		P.MinSpeed = D.MinSpeed;
		P.Size = D.Size * K;
		P.Life = D.Life;
		P.HitsLeft = D.Hits;
		P.Strength = D.Strength;
		P.Serial = NextProjectileSerial++;
		Projectiles.Add(P);
		++Owner.ProjectilesOut;
		FSimEvent E = Ev(EEvent::Projectile, Owner.Index);
		E.Move = M.Id;
		E.Kind = P.Kind;
		E.Hits = P.Serial;
		Emit(E);
	}

	void FFightSim::StepProjectiles()
	{
		for (FProjectile& P : Projectiles)
		{
			if (P.bDead)
			{
				continue;
			}
			const FFighter& O = Fighters[P.Owner];
			if (P.Def().bTether && (O.State == EState::HitStun || O.State == EState::Juggle || O.State == EState::Fall
				|| O.State == EState::Thrown || O.State == EState::BlockStun))
			{
				P.bDead = true;
				continue;
			}
			P.Step(O.Pos + FVector2D(0.35 * O.Facing, 1.3 * O.Scale()));
			if (FMath::Abs(P.Pos.X) > StageHalf + 2.0 || P.Pos.Y > 12.0)
			{
				P.bDead = true;
			}
		}
		for (int32 I = 0; I < Projectiles.Num();)
		{
			const FProjectile& P = Projectiles[I];
			if (P.bDead)
			{
				FFighter& O = Fighters[P.Owner];
				O.ProjectilesOut = FMath::Max(0, O.ProjectilesOut - 1);
				FSimEvent E = Ev(EEvent::ProjectileEnd, P.Owner);
				E.Kind = P.Kind;
				E.Point = P.Pos;
				E.Hits = P.Serial;
				Emit(E);
				Projectiles.RemoveAt(I);
			}
			else
			{
				++I;
			}
		}
	}

	// --- Contact ------------------------------------------------------------------------------------------

	void FFightSim::Resolve()
	{
		TArray<FContact, TInlineAllocator<8>> Contacts;
		TArray<TPair<int32, FRect>> HB;
		TArray<FRect> Hurt;
		for (FFighter& A : Fighters)
		{
			FFighter& D = A.Opponent();
			A.ActiveHitboxes(HB);
			if (HB.Num() == 0)
			{
				continue;
			}
			D.Hurtboxes(Hurt);
			bool bFound = false;
			for (const TPair<int32, FRect>& H : HB)
			{
				for (const FRect& R : Hurt)
				{
					if (H.Value.Intersects(R))
					{
						FContact C;
						C.Att = A.Index;
						C.Def = D.Index;
						C.Move = A.Move;
						C.Group = H.Key;
						C.Point = H.Value.Intersection(R).Center();
						Contacts.Add(C);
						bFound = true;
						break;
					}
				}
				if (bFound)
				{
					break;
				}
			}
		}
		for (int32 I = 0; I < Projectiles.Num(); ++I)
		{
			const FProjectile& P = Projectiles[I];
			if (P.bDead)
			{
				continue;
			}
			FFighter& D = Fighters[1 - P.Owner];
			if (D.IsProjInvulnerable())
			{
				continue;
			}
			D.Hurtboxes(Hurt);
			for (const FRect& R : Hurt)
			{
				if (P.Rect().Intersects(R))
				{
					FContact C;
					C.Att = P.Owner;
					C.Def = D.Index;
					C.Move = P.Move;
					C.Proj = I;
					C.Point = P.Pos;
					Contacts.Add(C);
					break;
				}
			}
		}
		ClashProjectiles();
		for (const FContact& C : Contacts)
		{
			if (Hooks && Hooks->IgnoreContact(*this, C))
			{
				continue;
			}
			Apply(C);
		}
		ResolveThrows();
	}

	void FFightSim::ClashProjectiles()
	{
		for (int32 I = 0; I < Projectiles.Num(); ++I)
		{
			FProjectile& P = Projectiles[I];
			for (int32 J = I + 1; J < Projectiles.Num(); ++J)
			{
				FProjectile& Q = Projectiles[J];
				if (P.bDead || Q.bDead || P.Owner == Q.Owner || !P.Rect().Intersects(Q.Rect()))
				{
					continue;
				}
				const int32 SP = P.Strength;
				P.Strength -= Q.Strength;
				Q.Strength -= SP;
				P.bDead = P.Strength <= 0;
				Q.bDead = Q.Strength <= 0;
				FSimEvent E = Ev(EEvent::Clash);
				E.Point = (P.Pos + Q.Pos) * 0.5;
				Emit(E);
			}
		}
	}

	bool FFightSim::CanBlock(const FFighter& D, ELevel Level) const
	{
		if (!D.IsBlocking())
		{
			return false;
		}
		switch (Level)
		{
		case ELevel::Low: return D.IsCrouching();
		case ELevel::Overhead: return !D.IsCrouching();
		case ELevel::Unblockable:
		case ELevel::Throw: return false;
		default: return true;
		}
	}

	void FFightSim::Apply(const FContact& C)
	{
		FFighter& A = Fighters[C.Att];
		FFighter& D = Fighters[C.Def];
		FProjectile* P = C.Proj >= 0 ? &Projectiles[C.Proj] : nullptr;
		const FMoveDef& M = *C.Move;
		const FProjectileDef* PD = P ? &P->Def() : nullptr;
		const ELevel Level = PD ? PD->Level : M.Level;
		int32 Damage = PD ? PD->Damage : M.Damage;
		const int32 Chip = PD ? PD->Chip : M.Chip;
		FName Effect = PD ? PD->Effect : M.Effect;
		int32 HS = PD ? PD->Hitstop : M.Hitstop;
		const bool bFlying = Effect == TEXT("launch") || Effect == TEXT("knockdown") || Effect == TEXT("sweep");
		if (HS < 0)
		{
			HS = Damage < 50 ? 5 : (Damage < 95 ? 8 : 11);
			if (bFlying)
			{
				// Blows that send a body flying hang on the contact a touch longer.
				HS += 2;
			}
		}
		// A counter hit: the blow lands while the other fighter winds up an attack of their own. The
		// victim alone freezes longer, so the blow reads bigger; the stun it would have added instead
		// is left off, which keeps the frame advantage of a counter exactly as it was.
		const bool bCounter = !P && D.State == EState::Move && D.Move && D.Move->IsAttack() && D.MF < D.Move->Startup && D.MoveContact.IsNone();
		const int32 CounterFreeze = bCounter ? 4 : 0;
		if (bCounter)
		{
			Damage = FMath::RoundToInt(Damage * 1.15);
		}
		if (P)
		{
			--P->HitsLeft;
			P->bDead = P->HitsLeft <= 0;
		}
		else
		{
			A.HitGroups.Add(C.Group);
		}
		double Dir = SignD(D.Pos.X - A.Pos.X);
		if (Dir == 0.0)
		{
			Dir = A.Facing;
		}
		const int32 IDir = (int32)Dir;
		if (D.State == EState::Dizzy)
		{
			// Any blow on a dizzy fighter ends it.
			D.Health = 0;
			D.Knock(FVector2D(1.6, 3.0), false, IDir);
			if (!P)
			{
				A.MoveContact = TEXT("hit");
				A.Hitstop = HS;
			}
			D.Hitstop = HS;
			FSimEvent E = Ev(EEvent::Hit, D.Index, A.Index);
			E.Move = M.Id;
			E.Level = Level;
			E.Point = C.Point;
			E.Combo = 1;
			E.bProj = P != nullptr;
			Emit(E);
			return;
		}
		if (!P && D.State == EState::Move && D.Move && D.Move->bParry && (Level == ELevel::High || Level == ELevel::Mid || Level == ELevel::Overhead))
		{
			if (D.MF >= D.Move->ParryFrames.X && D.MF <= D.Move->ParryFrames.Y)
			{
				// Caught: the attacker is left open and the parrier answers at once.
				A.Move = nullptr;
				A.TakeStun(D.Move->ParryStun, EStun::Dazed);
				A.Hitstop = 10;
				D.Hitstop = 10;
				if (const FMoveDef* Counter = D.Def->Move(D.Move->ParryCounter))
				{
					D.StartMove(*Counter);
				}
				FSimEvent E = Ev(EEvent::Parry, D.Index, A.Index);
				E.Move = M.Id;
				E.Point = C.Point;
				Emit(E);
				return;
			}
		}
		const bool bBuffed = D.Buff == TEXT("armor") && D.BuffHits > 0 && Level != ELevel::Throw;
		if (bBuffed || (D.State == EState::Move && D.Move && FMoveDef::InRange(D.Move->Armor, D.MF) && D.ArmorLeft > 0))
		{
			if (bBuffed)
			{
				--D.BuffHits;
			}
			else
			{
				--D.ArmorLeft;
			}
			const int32 Absorbed = (int32)(Damage * 0.5);
			D.Health = FMath::Max(0, D.Health - Absorbed);
			D.DamageTaken += Absorbed;
			D.Hitstop = 6;
			if (!P)
			{
				A.Hitstop = 6;
				A.MoveContact = TEXT("block");
			}
			FSimEvent E = Ev(EEvent::Armor, D.Index, A.Index);
			E.Move = M.Id;
			E.Damage = Absorbed;
			E.Point = C.Point;
			Emit(E);
			if (D.Health <= 0)
			{
				D.Knock(FVector2D(1.5, 3.5), false, IDir);
			}
			return;
		}
		if (CanBlock(D, Level))
		{
			const int32 ChipDmg = (P || M.Kind != EMoveKind::Normal) ? Chip : 0;
			if (ChipDmg > 0)
			{
				D.Health = FMath::Max(0, D.Health - ChipDmg);
				D.DamageTaken += ChipDmg;
			}
			D.TakeStun(PD ? PD->Blockstun : M.Blockstun, D.IsCrouching() ? EStun::CrouchGuard : EStun::Guard);
			D.Push((PD ? PD->PushBlock : M.PushBlock) * Dir, A.Index);
			D.Hitstop = FMath::Max(4, HS - 2);
			if (!P)
			{
				A.Hitstop = D.Hitstop;
				A.MoveContact = TEXT("block");
			}
			FSimEvent E = Ev(EEvent::Block, D.Index, A.Index);
			E.Move = M.Id;
			E.Damage = ChipDmg;
			E.Point = C.Point;
			E.bProj = P != nullptr;
			E.Level = Level;
			Emit(E);
			if (D.Health <= 0)
			{
				D.Knock(FVector2D(1.6, 3.0), false, IDir);
			}
			return;
		}
		const int32 N = D.ComboHits + 1;
		const double K = N <= 2 ? 1.0 : FMath::Max(0.4, 1.0 - 0.1 * (N - 2));
		// Scaling never takes a damaging hit below 1, but a hit that does none stays harmless.
		const int32 Dmg = Damage > 0 ? FMath::Max(1, FMath::RoundToInt(Damage * K)) : 0;
		D.Health = FMath::Max(0, D.Health - Dmg);
		D.DamageTaken += Dmg;
		D.ComboHits = N;
		D.ComboDamage += Dmg;
		D.LastHitMove = &M;
		D.LastHitLevel = Level;
		FVector2D Launch = PD ? PD->Launch : M.Launch;
		const FVector2D Jug = PD ? PD->Juggle : M.Juggle;
		const int32 Hitstun = PD ? PD->Hitstun : M.Hitstun;
		if (D.IsAirborne())
		{
			D.JugglePoints += P ? 1 : M.JuggleCost;
			if (!P && M.bAirKnock)
			{
				// Air-to-air strikes knock the other jumper out of the sky.
				Effect = TEXT("knockdown");
				Launch = FVector2D(2.0, 2.5);
			}
			if (Effect == TEXT("launch"))
			{
				D.Knock(Launch, true, IDir);
			}
			else if (Effect == TEXT("knockdown") || Effect == TEXT("sweep"))
			{
				D.Knock(Launch, false, IDir);
			}
			else
			{
				D.Knock(Jug, true, IDir);
			}
		}
		else if (Effect == TEXT("launch"))
		{
			D.JugglePoints += P ? 1 : M.JuggleCost;
			D.Knock(Launch, true, IDir);
		}
		else if (Effect == TEXT("knockdown") || Effect == TEXT("sweep"))
		{
			D.Knock(Launch, false, IDir);
		}
		else if (Effect == TEXT("crumple"))
		{
			D.TakeStun(Hitstun, EStun::Crumple);
		}
		else if (Effect == TEXT("pull"))
		{
			D.TakeStun(Hitstun, EStun::Pulled);
		}
		else if (Effect == TEXT("daze"))
		{
			D.TakeStun(Hitstun, EStun::Dazed);
		}
		else
		{
			const EStun Kind = D.IsCrouching() ? EStun::Crouch : (Level == ELevel::High ? EStun::High : EStun::Mid);
			D.TakeStun(Hitstun, Kind);
			// The first blow of a combo pushes the full distance; later ones push less, so a
			// dial-a-combo's later links reach the body the first ones drove back.
			double PushK = 1.0;
			if (N > 1)
			{
				PushK = ComboPush;
			}
			else if (!P && M.Chain.Num() > 0)
			{
				PushK = ChainOpenerPush;
			}
			D.Push((PD ? PD->PushHit : M.PushHit) * Dir * PushK, A.Index);
		}
		D.Hitstop = HS + CounterFreeze;
		if (!P)
		{
			A.Hitstop = HS;
			A.MoveContact = TEXT("hit");
		}
		FSimEvent E = Ev(EEvent::Hit, D.Index, A.Index);
		E.Move = M.Id;
		E.Damage = Dmg;
		E.Level = Level;
		E.Effect = Effect;
		E.Point = C.Point;
		E.Combo = N;
		E.bProj = P != nullptr;
		E.Kind = P ? P->Kind : FName();
		E.bCounter = bCounter;
		E.bKO = D.Health <= 0;
		Emit(E);
	}

	void FFightSim::ResolveThrows()
	{
		TArray<int32, TInlineAllocator<2>> Attempts;
		TArray<FRect> Hurt;
		for (FFighter& A : Fighters)
		{
			if (A.State != EState::Move || A.ThrowVictim >= 0 || !A.MoveContact.IsNone() || !A.Move)
			{
				continue;
			}
			const FMoveDef& M = *A.Move;
			if (!M.IsThrowKind() || !M.InActive(A.MF))
			{
				continue;
			}
			const FFighter& D = A.Opponent();
			if (D.IsInvulnerable() || D.IsAirborne() || D.Def->bUnthrowable)
			{
				continue;
			}
			if (D.State == EState::HitStun || D.State == EState::BlockStun || D.State == EState::Thrown)
			{
				continue;
			}
			bool bOk = false;
			if (M.Kind == EMoveKind::Throw)
			{
				bOk = FMath::Abs(D.Pos.X - A.Pos.X) <= A.Def->ThrowRange + 0.05;
			}
			else
			{
				D.Hurtboxes(Hurt);
				for (const FFrameBox& B : M.Hitboxes)
				{
					if (A.MF < B.From || A.MF > B.To)
					{
						continue;
					}
					for (const FRect& R : Hurt)
					{
						if (A.ToWorld(B.Rect).Intersects(R))
						{
							bOk = true;
						}
					}
				}
			}
			if (bOk)
			{
				Attempts.Add(A.Index);
			}
		}
		if (Attempts.Num() == 2)
		{
			// Both grabbed at once: they break apart.
			BreakThrow(Fighters[Attempts[0]], Fighters[Attempts[1]]);
			return;
		}
		for (int32 AI : Attempts)
		{
			FFighter& A = Fighters[AI];
			FFighter& D = A.Opponent();
			FContact C;
			C.Att = A.Index;
			C.Def = D.Index;
			C.Move = A.Move;
			C.bThrow = true;
			C.Point = D.Pos;
			if (Hooks && Hooks->IgnoreContact(*this, C))
			{
				continue;
			}
			A.ThrowVictim = D.Index;
			A.ThrowT = 0;
			A.MoveContact = TEXT("hit");
			D.ThrownBy = A.Index;
			D.Move = nullptr;
			D.Vel = FVector2D::ZeroVector;
			D.SetState(EState::Thrown);
			FSimEvent E = Ev(EEvent::Throw, D.Index, A.Index);
			E.Move = A.Move->Id;
			Emit(E);
		}
	}

	void FFightSim::BreakThrow(FFighter& A, FFighter& D)
	{
		double Dir = SignD(D.Pos.X - A.Pos.X);
		if (Dir == 0.0)
		{
			Dir = A.Facing;
		}
		A.ThrowVictim = -1;
		D.ThrownBy = -1;
		A.TakeStun(16, EStun::Break);
		D.TakeStun(16, EStun::Break);
		A.Push(-0.7 * Dir, D.Index);
		D.Push(0.7 * Dir, A.Index);
		Emit(Ev(EEvent::ThrowBreak, D.Index, A.Index));
	}

	void FFightSim::ThrowTick(FFighter& A)
	{
		if (!A.Move)
		{
			A.ReleaseThrow();
			return;
		}
		FFighter& D = Fighters[A.ThrowVictim];
		const FThrowDef& TD = A.Move->Throw;
		++A.ThrowT;
		if (A.ThrowT <= ThrowBreakWindow && D.Input.PressAge(BtnLP | BtnHP, 1) == 0 && (A.Move->Kind == EMoveKind::Throw || TD.bBreakable))
		{
			BreakThrow(A, D);
			return;
		}
		static const TArray<FVector> DefaultPath = {FVector(0, 0.7, 0.0)};
		const FVector2D Off = PathAt(TD.Path.Num() ? TD.Path : DefaultPath, A.ThrowT) * A.Scale();
		D.Pos = FVector2D(A.Pos.X + Off.X * A.Facing, FMath::Max(0.0, A.Pos.Y + Off.Y));
		D.Facing = -A.Facing;
		if (A.ThrowT == TD.DamageFrame)
		{
			const int32 Dmg = TD.Damage;
			D.Health = FMath::Max(0, D.Health - Dmg);
			D.DamageTaken += Dmg;
			D.ComboHits += 1;
			D.ComboDamage += Dmg;
			FSimEvent E = Ev(EEvent::Hit, D.Index, A.Index);
			E.Move = A.Move->Id;
			E.Damage = Dmg;
			E.Level = ELevel::Throw;
			E.Point = D.Pos + FVector2D(0, 1.0);
			E.Combo = D.ComboHits;
			E.bThrow = true;
			E.bKO = D.Health <= 0;
			Emit(E);
		}
		if (A.ThrowT >= TD.Frames)
		{
			const int32 Away = TD.bSwap ? -A.Facing : A.Facing;
			A.ThrowVictim = -1;
			D.ThrownBy = -1;
			D.Knock(TD.Release, false, Away);
			A.Move = nullptr;
			A.SetState(EState::Land);
			A.LandFramesNow = TD.Recover;
		}
	}

	// --- KO, finish and round flow -----------------------------------------------------------------------

	static bool InStates(EState S, std::initializer_list<EState> L)
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

	void FFightSim::CheckRound()
	{
		switch (Phase)
		{
		case EPhase::Fight:
		{
			const bool bDead0 = Fighters[0].Health <= 0;
			const bool bDead1 = Fighters[1].Health <= 0;
			if (bDead0 && bDead1)
			{
				for (FFighter& F : Fighters)
				{
					if (!InStates(F.State, {EState::Fall, EState::Juggle, EState::KO, EState::Down}))
					{
						F.Knock(FVector2D(1.2, 3.0), false);
					}
				}
				EndRound(-1, TEXT("double_ko"));
			}
			else if (bDead0 || bDead1)
			{
				const int32 Loser = bDead0 ? 0 : 1;
				const int32 Winner = 1 - Loser;
				FFighter& LF = Fighters[Loser];
				if (Wins[Winner] + 1 >= RoundsToWin && FinishAllowed(Winner))
				{
					FinishWinner = Winner;
					// The beaten fighter's shots in flight die with the round.
					for (FProjectile& Pr : Projectiles)
					{
						if (Pr.Owner == Loser)
						{
							Pr.bDead = true;
						}
					}
					Phase = EPhase::Finish;
					PhaseT = 0;
					FSimEvent E = Ev(EEvent::KO, Loser, Winner);
					E.bFinish = true;
					Emit(E);
				}
				else
				{
					if (InStates(LF.State, {EState::HitStun, EState::BlockStun, EState::Idle, EState::Move, EState::Thrown}) && LF.ThrownBy < 0)
					{
						LF.Knock(FVector2D(1.4, 3.2), false);
					}
					FSimEvent E = Ev(EEvent::KO, Loser, Winner);
					E.bBoss = LF.Def->bBoss;
					Emit(E);
					EndRound(Winner, TEXT("ko"));
				}
			}
			else if (Timer <= 0)
			{
				const double H0 = double(Fighters[0].Health) / Fighters[0].MaxHealth();
				const double H1 = double(Fighters[1].Health) / Fighters[1].MaxHealth();
				const int32 W = FMath::IsNearlyEqual(H0, H1) ? -1 : (H0 > H1 ? 0 : 1);
				FSimEvent E = Ev(EEvent::Timeout);
				E.Winner = W;
				Emit(E);
				EndRound(W, TEXT("timeout"));
			}
			break;
		}
		case EPhase::Finish:
		{
			FFighter& Victim = Fighters[1 - FinishWinner];
			Victim.Health = 0;
			if (!bFinishDizzy)
			{
				// The beaten fighter gets back up, unsteady, before the winner may finish them.
				const bool bStanding = InStates(Victim.State, {EState::Idle, EState::Crouch, EState::WalkF, EState::WalkB, EState::Block, EState::CrouchBlock, EState::Land});
				const bool bRisen = Victim.State == EState::Down && Victim.SF >= FFighter::DownFrames;
				if ((bStanding || bRisen) && Victim.Hitstop <= 0)
				{
					Victim.Move = nullptr;
					Victim.Pos.Y = 0.0;
					Victim.SetState(EState::Dizzy);
					bFinishDizzy = true;
					FinishT0 = PhaseT;
					FSimEvent E = Ev(EEvent::FinishPrompt, FinishWinner);
					E.Victim = Victim.Index;
					E.Kind = FName(*Victim.Def->Pronoun);
					Emit(E);
				}
				else if (PhaseT > 600)
				{
					EndRound(FinishWinner, TEXT("ko"));
				}
			}
			else if (Victim.State == EState::Dizzy)
			{
				if (PhaseT - FinishT0 > FinishTicks)
				{
					Victim.Knock(FVector2D(0.3, 1.0), false);
					Emit(Ev(EEvent::FinishTimeout, FinishWinner));
					EndRound(FinishWinner, TEXT("ko"));
				}
			}
			else
			{
				EndRound(FinishWinner, TEXT("ko"));
			}
			break;
		}
		case EPhase::RoundEnd:
			RoundEndTick();
			break;
		default:
			break;
		}
	}

	bool FFightSim::FinishAllowed(int32 Winner) const
	{
		if (!bFinishersEnabled || bTraining)
		{
			return false;
		}
		return !Fighters[1 - Winner].Def->bBoss && !Fighters[Winner].Def->bBoss;
	}

	void FFightSim::EndRound(int32 Winner, FName How)
	{
		Phase = EPhase::RoundEnd;
		PhaseT = 0;
		RoundWinner = Winner;
		RoundHow = How;
		for (const FFighter& F : Fighters)
		{
			MatchDamage[F.Index] += F.DamageTaken;
		}
		if (Winner >= 0)
		{
			++Wins[Winner];
			if (Fighters[Winner].DamageTaken == 0)
			{
				Flawless[Winner] = true;
			}
		}
		FSimEvent E = Ev(EEvent::RoundEnd);
		E.Winner = Winner;
		E.How = How;
		E.Round = RoundNum;
		E.bFlawless = Winner >= 0 && Flawless[Winner];
		Emit(E);
	}

	void FFightSim::RoundEndTick()
	{
		bool bSettled = true;
		for (const FFighter& F : Fighters)
		{
			if (InStates(F.State, {EState::Move, EState::Air, EState::Juggle, EState::Fall, EState::HitStun, EState::BlockStun, EState::PreJump})
				|| F.Hitstop > 0 || F.ThrowVictim >= 0)
			{
				bSettled = false;
			}
		}
		if (RoundWinner >= 0)
		{
			FFighter& W = Fighters[RoundWinner];
			if (bSettled && W.State != EState::Win && PhaseT > 40)
			{
				W.Move = nullptr;
				W.SetState(EState::Win);
				FSimEvent E = Ev(EEvent::WinPose, RoundWinner);
				E.bFlawless = Flawless[RoundWinner];
				Emit(E);
				FFighter& L = W.Opponent();
				if (RoundHow == TEXT("timeout") && InStates(L.State, {EState::Idle, EState::Crouch, EState::Block, EState::CrouchBlock, EState::WalkF, EState::WalkB}))
				{
					L.SetState(EState::Dizzy);
				}
			}
		}
		else
		{
			for (FFighter& F : Fighters)
			{
				if (bSettled && F.Health > 0 && F.State != EState::Dizzy && F.IsActionable())
				{
					F.SetState(EState::Dizzy);
				}
			}
		}
		if (PhaseT < RoundEndTicks + (bSettled ? 0 : 40))
		{
			return;
		}
		if (!bSettled && PhaseT < RoundEndTicks + 240)
		{
			return;
		}
		const bool bDecided = Wins[0] >= RoundsToWin || Wins[1] >= RoundsToWin;
		if (bDecided || RoundNum >= MaxRounds)
		{
			MatchWinner = -1;
			if (Wins[0] >= RoundsToWin)
			{
				MatchWinner = 0;
			}
			else if (Wins[1] >= RoundsToWin)
			{
				MatchWinner = 1;
			}
			Phase = EPhase::MatchEnd;
			PhaseT = 0;
			FSimEvent E = Ev(EEvent::MatchEnd);
			E.Winner = MatchWinner;
			E.Wins[0] = Wins[0];
			E.Wins[1] = Wins[1];
			E.Move = FinisherMove;
			E.bFlawless = MatchWinner >= 0 && MatchDamage[MatchWinner] == 0;
			Emit(E);
			return;
		}
		++RoundNum;
		StartRound(false);
	}

	bool FFightSim::TryFinisher(FFighter& F)
	{
		if (Phase != EPhase::Finish || F.Index != FinishWinner)
		{
			return false;
		}
		const FFighter& V = F.Opponent();
		if (V.State != EState::Dizzy)
		{
			return false;
		}
		const double Gap = FMath::Abs(V.Pos.X - F.Pos.X);
		struct FCand { FCommand Cmd; FName Range; };
		TArray<FCand, TInlineAllocator<2>> Cands;
		if (F.Def->bHasFinisher)
		{
			Cands.Add({F.Def->FinisherCmd, F.Def->FinisherRange});
		}
		if (!StageFinisher.IsNone())
		{
			if (const FMoveDef* SM = F.Def->Move(StageFinisher))
			{
				FCommand SC = FCommand::Parse(SM->StageInput);
				SC.Move = StageFinisher;
				if (SC.bValid)
				{
					Cands.Add({SC, TEXT("close")});
				}
			}
		}
		for (const FCand& Cand : Cands)
		{
			const int32 Age = F.Input.ChordAge(Cand.Cmd.Mask);
			if (Age < 0 || !Cand.Cmd.Matches(F.Input, Age, F.Facing))
			{
				continue;
			}
			double Lo = 0.0, Hi = 1.15;
			if (Cand.Range == TEXT("sweep"))
			{
				Lo = 1.15;
				Hi = 2.3;
			}
			else if (Cand.Range == TEXT("far"))
			{
				Lo = 2.6;
				Hi = 99.0;
			}
			if (Gap < Lo || Gap > Hi)
			{
				FSimEvent E = Ev(EEvent::FinisherRange, F.Index);
				E.Gap = Gap;
				E.Want = Cand.Range;
				Emit(E);
				continue;
			}
			F.Input.Consume(Cand.Cmd.Mask);
			BeginFinisher(F, Cand.Cmd.Move);
			return true;
		}
		return false;
	}

	void FFightSim::BeginFinisher(FFighter& F, FName MoveId)
	{
		FFighter& V = F.Opponent();
		Phase = EPhase::Finisher;
		PhaseT = 0;
		FinisherWho = F.Index;
		FinisherMove = MoveId;
		bFinisherStage = MoveId == StageFinisher;
		F.Move = F.Def->Move(MoveId);
		F.MF = 0;
		F.Vel = FVector2D::ZeroVector;
		V.Vel = FVector2D::ZeroVector;
		F.SetState(EState::Finisher);
		V.SetState(EState::Victim);
		Projectiles.Reset();
		for (FFighter& X : Fighters)
		{
			X.ProjectilesOut = 0;
		}
		FSimEvent E = Ev(EEvent::Finisher, F.Index);
		E.Victim = V.Index;
		E.Move = MoveId;
		E.bStage = bFinisherStage;
		Emit(E);
	}

	void FFightSim::FinisherTick()
	{
		FFighter& F = Fighters[FinisherWho];
		++F.MF;
		++F.SF;
		++F.Opponent().SF;
		if (!F.Move || F.MF >= F.Move->Total)
		{
			F.SetState(EState::Win);
			F.Opponent().SetState(EState::KO);
			FSimEvent E = Ev(EEvent::FinisherEnd, F.Index);
			E.Move = FinisherMove;
			Emit(E);
			EndRound(F.Index, TEXT("finisher"));
			// The finisher already showed the outcome; the results follow promptly.
			PhaseT = RoundEndTicks - 30;
		}
	}

	void FFightSim::SkipFinisher()
	{
		if (Phase != EPhase::Finisher)
		{
			return;
		}
		FFighter& F = Fighters[FinisherWho];
		if (F.Move)
		{
			F.MF = F.Move->Total - 1;
		}
	}

	// --- Trials ---------------------------------------------------------------------------------------------

	FTrialCheck::EResult FTrialCheck::Feed(const FSimEvent& E)
	{
		if (bDone || Moves.Num() == 0)
		{
			return EResult::None;
		}
		if (E.Type == EEvent::Hit)
		{
			if (E.By != Who)
			{
				return EResult::None;
			}
			const int32 N = E.Combo;
			if (N == Progress + 1 && E.Move == Moves[Progress])
			{
				++Progress;
				if (Progress >= Moves.Num())
				{
					bDone = true;
					return EResult::Done;
				}
				return EResult::Hit;
			}
			// The wrong move, or a hit after the combo was dropped: start over, counting this hit
			// as the first of a new attempt when it fits.
			const bool bStarted = Progress > 0;
			Progress = (N == 1 && E.Move == Moves[0]) ? 1 : 0;
			if (Progress == 1 && Moves.Num() == 1)
			{
				bDone = true;
				return EResult::Done;
			}
			return bStarted ? EResult::Dropped : EResult::None;
		}
		if (E.Type == EEvent::ComboEnd && E.Who == 1 - Who && Progress > 0)
		{
			Progress = 0;
			return EResult::Dropped;
		}
		return EResult::None;
	}
}
