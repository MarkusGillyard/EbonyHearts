#include "AI/SlopAI.h"

namespace Slop
{
	// --- Pad intent ------------------------------------------------------------------------------------

	int32 FPadIntent::ToScreen(int32 RelDir, int32 Facing)
	{
		int32 Bits = 0;
		int32 X = (RelDir - 1) % 3 - 1;
		const int32 Y = (RelDir - 1) / 3 - 1;
		if (Facing < 0)
		{
			X = -X;
		}
		if (X > 0) Bits |= PadRight;
		else if (X < 0) Bits |= PadLeft;
		if (Y > 0) Bits |= PadUp;
		else if (Y < 0) Bits |= PadDown;
		return Bits;
	}

	FPadState FPadIntent::Pad(int32 RelDir, int32 Facing, int32 Press, int32 Held)
	{
		FPadState P;
		P.Dirs = ToScreen(RelDir, Facing);
		P.Held = Press | Held;
		P.Pressed = Press;
		return P;
	}

	void FPadIntent::Command(const FString& Notation, int32 HoldTicks)
	{
		const FCommand C = FCommand::Parse(Notation);
		if (C.Steps.Num() >= 2)
		{
			for (int32 I = 0; I < C.Steps.Num(); ++I)
			{
				const int32 D = C.Steps[I];
				Queue.Add({D, 0, 0});
				if (I + 1 < C.Steps.Num())
				{
					const int32 Nxt = C.Steps[I + 1];
					if (D == 2 && Nxt == 6) Queue.Add({3, 0, 0});
					else if (D == 2 && Nxt == 4) Queue.Add({1, 0, 0});
					else if (D == Nxt) Queue.Add({5, 0, 0});
				}
			}
			Queue.Add({C.Steps.Last(), C.Mask, 0});
		}
		else if (C.Steps.Num() == 1)
		{
			Queue.Add({C.Steps[0], C.Mask, 0});
		}
		else
		{
			Queue.Add({5, C.Mask, 0});
		}
		for (int32 I = 0; I < HoldTicks; ++I)
		{
			Queue.Add({5, 0, 0});
		}
	}

	void FPadIntent::Hold(int32 RelDir, int32 Ticks, int32 Buttons)
	{
		for (int32 I = 0; I < Ticks; ++I)
		{
			Queue.Add({RelDir, 0, Buttons});
		}
	}

	FPadState FPadIntent::Next(int32 Facing, int32 IdleDir, int32 IdleHeld)
	{
		if (Queue.Num() == 0)
		{
			return Pad(IdleDir, Facing, 0, IdleHeld | HeldButtons);
		}
		const FStep Q = Queue[0];
		Queue.RemoveAt(0);
		return Pad(Q.Dir, Facing, Q.Press, Q.Held | HeldButtons);
	}

	// --- CPU opponent ----------------------------------------------------------------------------------

	const FCpuLevel& FCpuBrain::Level(int32 Difficulty)
	{
		static const FCpuLevel Levels[] = {
			{24, 0.25, 0.2, 0.25, 0.35, 0.45, 0.1, 16, 0.25, 0.35},
			{16, 0.45, 0.5, 0.55, 0.7, 0.6, 0.35, 10, 0.1, 0.7},
			{11, 0.62, 0.75, 0.85, 0.92, 0.7, 0.6, 7, 0.03, 0.95},
		};
		return Levels[FMath::Clamp(Difficulty, 0, 2)];
	}

	FCpuBrain::FCpuBrain(FFightSim& InSim, int32 InIndex, int32 Difficulty, int32 Seed)
		: Sim(InSim), Index(InIndex), L(Level(Difficulty))
	{
		Rng.Initialize(Seed != 0 ? Seed : 9001 + InIndex * 31 + Difficulty * 7);
		Ai = Me().Def->AI;
	}

	const TArray<TSharedPtr<FJsonValue>>* FCpuBrain::AiArr(const TCHAR* Key) const { return Json::Arr(Ai, Key); }

	const FCpuBrain::FSeen& FCpuBrain::Perceive()
	{
		const FFighter& O = Opp();
		FSeen S;
		S.State = O.State;
		S.Move = O.Move;
		S.MF = O.MF;
		S.Pos = O.Pos;
		S.Vel = O.Vel;
		S.bAir = O.IsAirborne();
		S.bCrouch = O.IsCrouching();
		S.bActionable = O.IsActionable();
		S.StunKind = O.StunKind;
		S.Stun = O.Stun;
		S.JugglePoints = O.JugglePoints;
		S.Tick = Sim.TickCount;
		SeenHistory.Insert(S, 0);
		if (SeenHistory.Num() > 48)
		{
			SeenHistory.Pop();
		}
		return SeenHistory[FMath::Min(L.React, SeenHistory.Num() - 1)];
	}

	int32 FCpuBrain::CountHitBy(FName Kind) const
	{
		int32 N = 0;
		for (const FName& K : HitBy)
		{
			N += K == Kind ? 1 : 0;
		}
		return N;
	}

	FPadState FCpuBrain::Think()
	{
		FFighter& M = Me();
		const FSeen Seen = Perceive();
		if (!Sim.CanAct())
		{
			Intent.Clear();
			BlockT = 0;
			return FPadState();
		}
		// Breaking a throw needs anticipation, not reaction: the window is shorter than any
		// reaction time, so the chance of a break stands in for a player mashing when grabbed.
		if (M.State == EState::Thrown)
		{
			if (M.ThrownBy >= 0 && Sim.Fighters[M.ThrownBy].ThrowT == FMath::Min(L.React / 3, 6) && Rng.FRand() < L.Tech)
			{
				return FPadIntent::Pad(5, M.Facing, BtnLP);
			}
			return FPadState();
		}
		if (Sim.Phase == EPhase::Finish)
		{
			return Finish();
		}
		if (Intent.Busy())
		{
			return Intent.Next(M.Facing);
		}
		// A blow ending as seen can be read before a held guard runs out.
		if (ParryRead(M, Seen))
		{
			BlockT = 0;
			return Intent.Next(M.Facing);
		}
		if (BlockT > 0)
		{
			--BlockT;
			return FPadIntent::Pad(bBlockLow ? 2 : 5, M.Facing, 0, BtnBL);
		}
		NoteHit(M);
		if (!M.IsActionable() && M.State != EState::Air)
		{
			bWasBlocking = M.State == EState::BlockStun;
			return FPadIntent::Pad(5, M.Facing);
		}
		FFighter& O = Opp();
		// Distances too are as seen: the CPU judges spacing from what was on screen, not from now.
		const double Dist = FMath::Abs(Seen.Pos.X - M.Pos.X);
		// A jump is read from its start: the arc is predictable, so the anti-air is timed to meet
		// the opponent on the way down (from what was seen, not from inputs).
		PlanAntiAir(M, Seen);
		if (AaAt >= 0 && Sim.TickCount >= AaAt)
		{
			AaAt = -1;
			if (Seen.bAir && Dist < 2.6)
			{
				Intent.Command(Notation(AaMove));
				return Intent.Next(M.Facing);
			}
		}
		// A stunned, dazed or reeled-in opponent (as seen) is an opening: take it.
		if (Seen.State == EState::HitStun && (Seen.StunKind == EStun::Dazed || Seen.StunKind == EStun::Pulled || Seen.StunKind == EStun::Break)
			&& Seen.Stun - L.React > 8 && Dist < 1.6)
		{
			if (Rng.FRand() < L.Combo)
			{
				StartCombo(true);
				return Intent.Next(M.Facing);
			}
		}
		// Just blocked something that is still recovering: punish it.
		const FMoveDef* SM = Seen.Move;
		if (bWasBlocking && Seen.State == EState::Move && SM && Seen.MF > SM->LastActive() && SM->Total - Seen.MF - L.React > 8 && Dist < 1.5)
		{
			bWasBlocking = false;
			if (Rng.FRand() < L.Punish)
			{
				StartCombo(true);
				return Intent.Next(M.Facing);
			}
		}
		bWasBlocking = false;
		// --- Reactions to what has been seen ---
		if (Seen.State == EState::Move && SM && SM->IsAttack() && Seen.MF <= SM->LastActive() && Dist < 2.4)
		{
			if (M.State != EState::Air && Rng.FRand() < L.Block)
			{
				BlockT = FMath::Max(8, SM->LastActive() - Seen.MF + 10);
				bBlockLow = SM->Level == ELevel::Low || (SM->Level != ELevel::Overhead && Rng.FRand() < 0.2);
				return FPadIntent::Pad(bBlockLow ? 2 : 5, M.Facing, 0, BtnBL);
			}
		}
		if (Seen.bAir && (Seen.State == EState::Air || Seen.State == EState::Move) && Dist < 2.2 && Seen.Vel.Y < 1.0)
		{
			if (Rng.FRand() < L.AntiAir * 0.25)
			{
				FName Aa = TEXT("uppercut");
				if (const TArray<TSharedPtr<FJsonValue>>* A = AiArr(TEXT("anti_air")))
				{
					if (A->Num() > 0)
					{
						Aa = FName(*(*A)[Rng.RandRange(0, A->Num() - 1)]->AsString());
					}
				}
				Intent.Command(Notation(Aa));
				return Intent.Next(M.Facing);
			}
		}
		for (const FProjectile& P : Sim.Projectiles)
		{
			if (P.Owner != Index && P.Age > L.React && FMath::Abs(P.Pos.X - M.Pos.X) < 3.0 && FMath::Sign(M.Pos.X - P.Pos.X) == FMath::Sign(P.Vel.X))
			{
				const double Roll = Rng.FRand();
				if (P.Def().Level == ELevel::High && Roll < 0.4)
				{
					return FPadIntent::Pad(2, M.Facing);
				}
				if (Roll < 0.4 + L.Block * 0.5)
				{
					BlockT = 14;
					bBlockLow = P.Def().Level == ELevel::Low;
					return FPadIntent::Pad(bBlockLow ? 2 : 5, M.Facing, 0, BtnBL);
				}
				Intent.Hold(9, 2);
				return Intent.Next(M.Facing);
			}
		}
		// Punish a whiffed attack still recovering.
		if (Seen.State == EState::Move && SM && Seen.MF > SM->LastActive() && SM->Total - Seen.MF - L.React > 14 && Dist < 1.5)
		{
			if (Rng.FRand() < L.Punish * 0.4)
			{
				StartCombo(true);
				return Intent.Next(M.Facing);
			}
		}
		if (Seen.State == EState::Juggle && Dist < 1.8 && Seen.Pos.Y < 1.6 && Seen.Vel.Y < 0.0 && Seen.JugglePoints < FFighter::JuggleLimit - 2
			&& O.State == EState::Juggle)
		{
			if (Rng.FRand() < L.Combo)
			{
				Intent.Command(TEXT("D+HP"));
				return Intent.Next(M.Facing);
			}
		}
		// Swept again and again: crouch and guard while the sweeper is in range, which in turn
		// leaves the CPU open to overheads and jump-ins (the habit fades as other hits replace it).
		const int32 Lows = CountHitBy(TEXT("low"));
		if (Lows >= 4 && Seen.bActionable && Dist < 2.3 && Rng.FRand() < L.Block * 0.015 * Lows)
		{
			BlockT = Rng.RandRange(14, 26);
			bBlockLow = true;
			return FPadIntent::Pad(2, M.Facing, 0, BtnBL);
		}
		// --- Plans ---
		if (--ThinkT <= 0)
		{
			ThinkT = L.Think + Rng.RandRange(0, L.Think);
			Plan(M, Seen, Dist);
			if (Intent.Busy())
			{
				return Intent.Next(M.Facing);
			}
		}
		if (WalkT > 0)
		{
			--WalkT;
			if (RunT > 0)
			{
				--RunT;
				return FPadIntent::Pad(6, M.Facing, 0, BtnRN);
			}
			return FPadIntent::Pad(Walk, M.Facing);
		}
		return FPadIntent::Pad(5, M.Facing);
	}

	void FCpuBrain::Plan(FFighter& M, const FSeen& O, double Dist)
	{
		const double Aggr = L.Aggr * (0.6 + Json::Num(Ai, TEXT("aggression"), 0.5) * 0.8);
		const double Want = Json::Num(Ai, TEXT("range"), 1.1);
		if (Rng.FRand() < L.Mistake)
		{
			const int32 Walks[] = {4, 6, 5};
			Walk = Walks[Rng.RandRange(0, 2)];
			WalkT = Rng.RandRange(6, 20);
			return;
		}
		if (O.State == EState::Down || O.State == EState::WakeUp || O.State == EState::KO)
		{
			Walk = Dist > 1.6 ? 6 : 5;
			WalkT = 10;
			return;
		}
		if (Dist > 3.4)
		{
			const TArray<FName> Zoner = MovesTagged(TEXT("projectile"));
			if (Zoner.Num() > 0 && Rng.FRand() < 0.3 + Json::Num(Ai, TEXT("zoning"), 0.2))
			{
				Intent.Command(Notation(Zoner[Rng.RandRange(0, Zoner.Num() - 1)]));
				return;
			}
			if (Rng.FRand() < Aggr * 0.5)
			{
				Walk = 6;
				WalkT = 30;
				RunT = (M.RunMeter > 50.0 && Rng.FRand() < Aggr) ? 30 : 0;
				return;
			}
			if (Rng.FRand() < 0.15)
			{
				Intent.Hold(9, 3);
				Intent.Hold(5, 20);
				Intent.Press(5, BtnHK);
				return;
			}
			Walk = 6;
			WalkT = 20;
			return;
		}
		if (Dist > Want + 0.9)
		{
			const double Roll = Rng.FRand();
			const int32 Lows = CountHitBy(TEXT("low"));
			if (Roll < L.Block * (0.35 + 0.08 * Lows) && O.bActionable)
			{
				// Expecting the opponent to come in: guard for a moment, low if they keep sweeping.
				BlockT = Rng.RandRange(12, 24);
				bBlockLow = Rng.FRand() < 0.3 + 0.12 * Lows;
				return;
			}
			if (Roll < 0.18 * Aggr + 0.05)
			{
				Intent.Hold(9, 3);
				Intent.Hold(5, 18);
				Intent.Press(5, BtnHK);
				return;
			}
			const TArray<FName> Rush = MovesTagged(TEXT("rush"));
			if (Rush.Num() > 0 && Roll < 0.3 * Aggr + 0.1)
			{
				Intent.Command(Notation(Rush[Rng.RandRange(0, Rush.Num() - 1)]));
				return;
			}
			if (Roll < 0.45)
			{
				const FString Pick = AttackFor(Dist);
				if (!Pick.IsEmpty())
				{
					Intent.Command(Pick);
					return;
				}
			}
			Walk = 6;
			WalkT = Rng.RandRange(10, 24);
			return;
		}
		// Close.
		if ((O.State == EState::CrouchBlock || O.State == EState::Block) && Dist < M.Def->ThrowRange && Rng.FRand() < 0.35)
		{
			// Someone sitting in their guard gets thrown.
			Intent.Command(TEXT("F+LP"));
			return;
		}
		const int32 LowsClose = CountHitBy(TEXT("low"));
		if (LowsClose >= 2 && O.bActionable && Rng.FRand() < L.Block * 0.12 * LowsClose)
		{
			BlockT = Rng.RandRange(12, 22);
			bBlockLow = true;
			return;
		}
		const double R2 = Rng.FRand();
		if (R2 < 0.12 && Dist < M.Def->ThrowRange)
		{
			Intent.Command(TEXT("F+LP"));
			return;
		}
		if (R2 < 0.12 + 0.45 * Aggr && Dist < Reach(TEXT("hp")) + 0.05)
		{
			StartCombo(false);
			return;
		}
		if (R2 < 0.8)
		{
			const FString Pick = AttackFor(Dist);
			if (!Pick.IsEmpty())
			{
				Intent.Command(Pick);
				return;
			}
			Walk = 6;
			WalkT = 6;
			return;
		}
		if (Rng.FRand() < 0.5)
		{
			BlockT = Rng.RandRange(10, 26);
			bBlockLow = Rng.FRand() < 0.4 + 0.1 * CountHitBy(TEXT("low"));
		}
		else
		{
			Walk = 4;
			WalkT = Rng.RandRange(8, 18);
		}
	}

	void FCpuBrain::PlanAntiAir(FFighter& M, const FSeen& Seen)
	{
		if (!Seen.bAir)
		{
			bAaReady = true;
			return;
		}
		if (!bAaReady || AaAt >= 0)
		{
			return;
		}
		const FFighter& O = Opp();
		const FVector2D P = Seen.Pos;
		const FVector2D V = Seen.Vel;
		const double Toward = FMath::Sign(M.Pos.X - P.X);
		if (V.Y <= 0.0 || FMath::Sign(V.X) != Toward || FMath::Abs(M.Pos.X - P.X) > 3.6)
		{
			return;
		}
		bAaReady = false;
		const double Chance = FMath::Min(0.95, L.AntiAir + 0.12 * CountHitBy(TEXT("jump")));
		if (Rng.FRand() > Chance)
		{
			return;
		}
		const double G = O.Def->Gravity;
		const double H = 1.4 * O.Scale();
		const double Disc = V.Y * V.Y + 2.0 * G * (P.Y - H);
		if (Disc < 0.0)
		{
			return;
		}
		const double T = (V.Y + FMath::Sqrt(Disc)) / G;
		AaMove = TEXT("uppercut");
		if (const TArray<TSharedPtr<FJsonValue>>* A = AiArr(TEXT("anti_air")))
		{
			if (A->Num() > 0)
			{
				AaMove = FName(*(*A)[Rng.RandRange(0, A->Num() - 1)]->AsString());
			}
		}
		const FMoveDef* Mv = M.Def->Move(AaMove);
		const int32 Startup = Mv ? Mv->Startup : 8;
		const int32 TicksLeft = (int32)(T * 60.0) - (Sim.TickCount - Seen.Tick);
		AaAt = Sim.TickCount + FMath::Max(0, TicksLeft - Startup + 1);
	}

	// A parry (the fighter's "parries": Ashka's shin check for lows, Bram's Stand Down for highs
	// and mids) is a read, not a reaction: through the CPU's delay a blow is already out when it is
	// seen. Hit that way before, the CPU bets the attacker throws the same blow again the moment
	// it can, and times the parry's window (after the inputs and the parry's own startup) to be
	// open when that blow would land.
	bool FCpuBrain::ParryRead(const FFighter& M, const FSeen& Seen)
	{
		const TArray<TSharedPtr<FJsonValue>>* Parries = AiArr(TEXT("parries"));
		const FMoveDef* SL = Seen.Move;
		if (!Parries || !SL || Seen.State != EState::Move || !SL->IsAttack() || SL->bAir || M.State == EState::Air || FMath::Abs(Seen.Pos.X - M.Pos.X) >= 1.6)
		{
			return false;
		}
		if (CountHitBy(SL->Level == ELevel::Low ? TEXT("low") : TEXT("other")) < 2 || SL->Projectile.bHas)
		{
			return false;
		}
		// The bet needs the attacker still in reach of the same blow (a blocked one often pushes
		// them out, and then they walk in instead).
		double Reach = 0.0;
		for (const FFrameBox& B : SL->Hitboxes)
		{
			Reach = FMath::Max(Reach, B.Rect.EndX());
		}
		if (SL->Motion.Num() > 0)
		{
			Reach += SL->Motion.Last().Y;
		}
		if (FMath::Abs(Seen.Pos.X - M.Pos.X) > Reach + 0.22 * M.Def->Scale() + 0.1)
		{
			return false;
		}
		// Where the attacker's move is now, allowing for the delay.
		const int32 Now = Seen.MF + L.React;
		for (const TSharedPtr<FJsonValue>& V : *Parries)
		{
			const FMoveDef* PM = M.Def->Move(FName(*V->AsString()));
			if (!PM || !PM->bParry || !PM->Parries(SL->Level))
			{
				continue;
			}
			const FString Cmd = Notation(PM->Id);
			const int32 Lead = FCommand::Parse(Cmd).Steps.Num() + 1;
			// The repeated blow lands at Total + Startup; the window must open a tick before it and
			// still be open when its active frames end.
			const int32 Target = SL->Total + SL->Startup;
			const int32 Latest = Target - 1 - Lead - PM->ParryFrames.X;
			const int32 Earliest = FMath::Max(Latest - 2, Target + SL->Active - Lead - PM->ParryFrames.Y);
			const bool bFree = M.IsActionable() || (M.State == EState::BlockStun && M.Stun <= Lead);
			if (!bFree || Now < Earliest || Now > Latest)
			{
				return false;
			}
			if (Rng.FRand() < L.Block * 0.5)
			{
				Intent.Command(Cmd);
				return true;
			}
			return false;
		}
		return false;
	}

	void FCpuBrain::NoteHit(FFighter& M)
	{
		const int32 Taken = M.DamageTaken;
		const bool bHit = Taken > LastDamage;
		LastDamage = Taken;
		if (!bHit || !M.LastHitMove)
		{
			return;
		}
		FName Kind = TEXT("other");
		if (M.LastHitMove->bAir)
		{
			Kind = TEXT("jump");
		}
		else if (M.LastHitMove->Level == ELevel::Low)
		{
			Kind = TEXT("low");
		}
		HitBy.Insert(Kind, 0);
		if (HitBy.Num() > 8)
		{
			HitBy.Pop();
		}
	}

	void FCpuBrain::StartCombo(bool bPunish)
	{
		const TArray<TSharedPtr<FJsonValue>>* Combos = AiArr(TEXT("combos"));
		if (!Combos || Combos->Num() == 0 || Rng.FRand() > L.Combo)
		{
			Intent.Command(bPunish ? TEXT("D+HP") : TEXT("HP"));
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>& C = (*Combos)[Rng.RandRange(0, Combos->Num() - 1)]->AsArray();
		for (const TSharedPtr<FJsonValue>& Link : C)
		{
			Intent.Command(Link->AsString());
			// Space the links so each lands inside its chain window.
			Intent.Hold(5, 6);
		}
	}

	FPadState FCpuBrain::Finish()
	{
		FFighter& M = Me();
		if (Sim.FinishWinner != Index || Opp().State != EState::Dizzy)
		{
			return FPadState();
		}
		if (Intent.Busy())
		{
			return Intent.Next(M.Facing);
		}
		if (!M.Def->bHasFinisher || Rng.FRand() > L.Finish)
		{
			if (Sim.PhaseT - Sim.FinishT0 > 90)
			{
				Intent.Command(TEXT("D+HP"));
			}
			return FPadIntent::Pad(5, M.Facing);
		}
		double Lo = 0.0, Hi = 1.15;
		if (M.Def->FinisherRange == TEXT("sweep"))
		{
			Lo = 1.15;
			Hi = 2.3;
		}
		else if (M.Def->FinisherRange == TEXT("far"))
		{
			Lo = 2.6;
			Hi = 99.0;
		}
		const double Dist = Sim.Distance();
		const double Mid = (Lo + FMath::Min(Hi, 3.2)) * 0.5;
		if (Dist > Hi - 0.05 || (Dist > Mid + 0.3 && Hi < 5.0))
		{
			return FPadIntent::Pad(6, M.Facing);
		}
		if (Dist < Lo + 0.05)
		{
			return FPadIntent::Pad(4, M.Facing);
		}
		if (Sim.PhaseT - Sim.FinishT0 > 50)
		{
			Intent.Command(M.Def->FinisherCmd.Text);
		}
		return FPadIntent::Pad(5, M.Facing);
	}

	double FCpuBrain::Reach(FName MoveId)
	{
		if (const double* Cached = ReachCache.Find(MoveId))
		{
			return *Cached;
		}
		const FMoveDef* Mv = Me().Def->Move(MoveId);
		double R = 0.0;
		if (Mv)
		{
			for (const FFrameBox& B : Mv->Hitboxes)
			{
				R = FMath::Max(R, B.Rect.EndX());
			}
			if (Mv->Motion.Num() > 0)
			{
				R += Mv->Motion.Last().Y;
			}
			if (Mv->Projectile.bHas)
			{
				R = 99.0;
			}
		}
		R = R > 0.0 ? R + 0.22 * Opp().Scale() : 0.0;
		ReachCache.Add(MoveId, R);
		return R;
	}

	FString FCpuBrain::AttackFor(double Dist)
	{
		struct FOption { FString Text; FName Move; double W; };
		TArray<FOption> Options;
		double Total = 0.0;
		const TSharedPtr<FJsonObject> Weights = Json::Obj(Ai, TEXT("weights"));
		// Specials are a spice, not the meal: most decisions consider only normals.
		const bool bSpecialsOk = Rng.FRand() < 0.3;
		for (const FCommand& C : Me().Def->Commands)
		{
			const FMoveDef* Mv = Me().Def->Move(C.Move);
			if (!Mv || Mv->IsThrowKind() || Mv->bFinisher || !Mv->IsAttack() || Mv->Hitboxes.Num() == 0)
			{
				continue;
			}
			if (Mv->Kind == EMoveKind::Special && !bSpecialsOk)
			{
				continue;
			}
			const double R = Reach(C.Move);
			if (R < Dist || R > Dist + 1.4)
			{
				continue;
			}
			double W = Json::Num(Weights, *C.Move.ToString(), 1.0) * (Mv->Kind == EMoveKind::Normal ? 1.6 : 0.35);
			// Variety: a move just used is less likely to be picked again at once.
			W *= Recent.Contains(C.Move) ? 0.3 : 1.0;
			Options.Add({C.Text, C.Move, W});
			Total += W;
		}
		if (Options.Num() == 0)
		{
			return FString();
		}
		double Pick = Rng.FRand() * Total;
		const FOption* Chosen = &Options[0];
		for (const FOption& O : Options)
		{
			Pick -= O.W;
			if (Pick <= 0.0)
			{
				Chosen = &O;
				break;
			}
		}
		Recent.Insert(Chosen->Move, 0);
		if (Recent.Num() > 3)
		{
			Recent.Pop();
		}
		return Chosen->Text;
	}

	TArray<FName> FCpuBrain::MovesTagged(FName Tag) const
	{
		TArray<FName> Out;
		for (const FCommand& C : Me().Def->Commands)
		{
			const FMoveDef* Mv = Me().Def->Move(C.Move);
			if (Mv && Mv->HasTag(Tag))
			{
				Out.Add(C.Move);
			}
		}
		return Out;
	}

	FString FCpuBrain::Notation(FName MoveId) const
	{
		const FString N = Me().Def->NotationOf(MoveId);
		return N.IsEmpty() ? FString(TEXT("HP")) : N;
	}

	// --- Boss ----------------------------------------------------------------------------------------------

	FBossBrain::FBossBrain(FFightSim& InSim, int32 InIndex, int32 Difficulty)
		: Sim(InSim), Index(InIndex)
	{
		static const FLevel Levels[] = {
			{22, 720, 660, 0.25, 26, 0.15, 150, 300},
			{16, 560, 520, 0.45, 20, 0.3, 120, 240},
			{12, 440, 420, 0.65, 15, 0.45, 95, 180},
		};
		L = Levels[FMath::Clamp(Difficulty, 0, 2)];
		Rng.Initialize(4242 + Difficulty);
		DiveCd = L.DiveCd / 2;
	}

	static const TCHAR* const BossTar = TEXT("D,F+HP");
	static const TCHAR* const BossGust = TEXT("B,F+HK");
	static const TCHAR* const BossBite = TEXT("F,F+LP");

	void FBossBrain::Do(const TCHAR* Command)
	{
		Intent.Command(Command);
		LastCommand = Command;
		if (LastCommand == BossTar || LastCommand == BossGust)
		{
			RangedCd = L.RangedCd + Rng.RandRange(0, 45);
		}
		else if (LastCommand == BossBite)
		{
			// A grab beats a block, so it must not come round too often.
			BiteCd = L.BiteCd + Rng.RandRange(0, 60);
		}
	}

	FPadState FBossBrain::Think()
	{
		FFighter& M = Sim.Fighters[Index];
		FFighter& O = Sim.Fighters[1 - Index];
		SeenAir.Insert(O.IsAirborne(), 0);
		if (SeenAir.Num() > 40)
		{
			SeenAir.Pop();
		}
		const bool bSeenAir = SeenAir[FMath::Min(L.React, SeenAir.Num() - 1)];
		if (!Sim.CanAct())
		{
			Intent.Clear();
			return FPadState();
		}
		--DiveCd;
		--StormCd;
		--RangedCd;
		--BiteCd;
		if (Intent.Busy())
		{
			return Intent.Next(M.Facing);
		}
		if (!M.IsActionable())
		{
			return FPadState();
		}
		const double Dist = FMath::Abs(O.Pos.X - M.Pos.X);
		const bool bLow = double(M.Health) / M.MaxHealth() < 0.5;
		if (bSeenAir && Dist < 2.4 && Rng.FRand() < L.Aa * 0.2)
		{
			Do(TEXT("D+HP"));
			return Intent.Next(M.Facing);
		}
		if (--ThinkT > 0)
		{
			if (WalkT > 0)
			{
				--WalkT;
				return FPadIntent::Pad(Walk, M.Facing);
			}
			return FPadIntent::Pad(5, M.Facing);
		}
		ThinkT = L.Think + Rng.RandRange(0, L.Think);
		if (bLow && StormCd <= 0 && Dist < 3.0)
		{
			StormCd = L.StormCd + Rng.RandRange(0, 120);
			Do(TEXT("B,B+HP"));
			return Intent.Next(M.Facing);
		}
		if (DiveCd <= 0 && O.State != EState::Down && O.State != EState::WakeUp)
		{
			DiveCd = L.DiveCd + Rng.RandRange(0, 180) - (bLow ? 120 : 0);
			Do(TEXT("D,U+HK"));
			return Intent.Next(M.Facing);
		}
		const double R = Rng.FRand();
		// Tar and wind keep its distance, but each is followed by a wait and neither comes twice
		// running: meanwhile it walks in, so a gust that drove the opponent off is followed up.
		const auto CanFire = [this](const TCHAR* Command) { return RangedCd <= 0 && LastCommand != Command; };
		if (Dist > 1.5)
		{
			const bool bFar = Dist > 3.2;
			const TCHAR* Pick = R < (bFar ? 0.45 : 0.3) ? BossTar : (R < (bFar ? 0.65 : 0.5) ? BossGust : nullptr);
			if (Pick && CanFire(Pick)) Do(Pick);
			else { Walk = 6; WalkT = bFar ? Rng.RandRange(20, 40) : Rng.RandRange(12, 26); }
		}
		else
		{
			if (R < 0.18 && BiteCd <= 0) Do(BossBite);
			else if (R < 0.55) { Do(TEXT("HP")); Intent.Hold(5, 9); Intent.Command(TEXT("HP")); }
			else if (R < 0.75) Do(TEXT("D+LK"));
			else if (R < 0.85 && CanFire(BossGust)) Do(BossGust);
			else { Walk = 4; WalkT = Rng.RandRange(10, 20); }
		}
		return Intent.Busy() ? Intent.Next(M.Facing) : FPadIntent::Pad(5, M.Facing);
	}

	// --- Training dummy --------------------------------------------------------------------------------------

	const TArray<FString>& FTrainingDummy::Modes()
	{
		static const TArray<FString> M = {TEXT("Stand"), TEXT("Crouch"), TEXT("Block All"), TEXT("Block Random"), TEXT("Crouch Block"), TEXT("Jump"), TEXT("CPU"),
			TEXT("Record"), TEXT("Playback"), TEXT("Block After Hit"), TEXT("Break Throws")};
		return M;
	}

	// Reads the attacker's move as it happens, so every block is correct.
	FPadState FTrainingDummy::BlockAll() const
	{
		const FFighter& F = Sim.Fighters[Index];
		const FFighter& O = Sim.Fighters[1 - Index];
		bool bLowNow = O.State == EState::Move && O.Move && O.Move->Level == ELevel::Low;
		for (const FProjectile& P : Sim.Projectiles)
		{
			if (P.Owner != Index)
			{
				bLowNow = bLowNow || P.Def().Level == ELevel::Low;
			}
		}
		return FPadIntent::Pad(bLowNow ? 2 : 5, F.Facing, 0, BtnBL);
	}

	FString FTrainingDummy::ModeName() const
	{
		const FString& Name = Modes()[Mode];
		if (Name == TEXT("Record"))
		{
			return TakesPlayerPad() ? FString::Printf(TEXT("Record (you control it, %.1f s left)"), (MaxTape - Tape.Num()) / 60.0) : FString(TEXT("Record (done)"));
		}
		if (Name == TEXT("Playback") && Tape.Num() == 0)
		{
			return TEXT("Playback (nothing recorded)");
		}
		return Name;
	}

	// Left and right swapped: from screen directions to "as if facing right" and back.
	static int32 SwapSides(int32 Dirs)
	{
		const int32 X = Dirs & (PadLeft | PadRight);
		return (Dirs & ~(PadLeft | PadRight)) | (X == PadLeft ? PadRight : X == PadRight ? PadLeft : X);
	}

	bool FTrainingDummy::TakesPlayerPad() const
	{
		return Modes()[Mode] == TEXT("Record") && Tape.Num() < MaxTape;
	}

	FPadState FTrainingDummy::Record(const FPadState& Player)
	{
		FPadState Kept = Player;
		if (Sim.Fighters[Index].Facing < 0)
		{
			Kept.Dirs = SwapSides(Kept.Dirs);
		}
		Tape.Add(Kept);
		return Player;
	}

	FTrainingDummy::FTrainingDummy(FFightSim& InSim, int32 InIndex) : Sim(InSim), Index(InIndex)
	{
		Rng.Initialize(77);
	}

	void FTrainingDummy::SetMode(int32 M)
	{
		Mode = M;
		Cpu.Reset();
		PlayAt = 0;
		if (Modes()[Mode] == TEXT("Record"))
		{
			Tape.Reset();
		}
		if (Modes()[Mode] == TEXT("CPU"))
		{
			Cpu = MakeUnique<FCpuBrain>(Sim, Index, 1);
		}
	}

	FPadState FTrainingDummy::Think()
	{
		const FFighter& F = Sim.Fighters[Index];
		const FString& Name = Modes()[Mode];
		if (Name == TEXT("Crouch"))
		{
			return FPadIntent::Pad(2, F.Facing);
		}
		if (Name == TEXT("Block All"))
		{
			return BlockAll();
		}
		if (Name == TEXT("Block After Hit"))
		{
			// Once hit, it blocks everything for a second after it can: what still lands was a true
			// combo, what it blocks had a gap.
			if (F.State == EState::HitStun || F.State == EState::Juggle || F.State == EState::Fall)
			{
				GuardT = 60;
			}
			return GuardT-- > 0 ? BlockAll() : FPadState();
		}
		if (Name == TEXT("Break Throws"))
		{
			// A punch on every other tick while held, so the press lands inside the break window.
			return FPadIntent::Pad(5, F.Facing, F.State == EState::Thrown && Sim.TickCount % 2 == 0 ? BtnLP : 0);
		}
		if (Name == TEXT("Block Random"))
		{
			if (--SwitchT <= 0)
			{
				bLow = Rng.FRand() < 0.5;
				SwitchT = Rng.RandRange(20, 60);
			}
			return FPadIntent::Pad(bLow ? 2 : 5, F.Facing, 0, BtnBL);
		}
		if (Name == TEXT("Crouch Block"))
		{
			return FPadIntent::Pad(2, F.Facing, 0, BtnBL);
		}
		if (Name == TEXT("Jump"))
		{
			return FPadIntent::Pad(F.IsActionable() ? 8 : 5, F.Facing);
		}
		if (Name == TEXT("CPU") && Cpu)
		{
			return Cpu->Think();
		}
		if (Name == TEXT("Playback") && Tape.Num() > 0)
		{
			// The recording, then a short pause, round and round.
			const int32 At = PlayAt;
			PlayAt = (PlayAt + 1) % (Tape.Num() + LoopGap);
			if (At >= Tape.Num())
			{
				return FPadState();
			}
			FPadState P = Tape[At];
			if (F.Facing < 0)
			{
				P.Dirs = SwapSides(P.Dirs);
			}
			return P;
		}
		return FPadState();
	}

	// --- The stranger -------------------------------------------------------------------------------------------

	FStrangerRules::FStrangerRules(FFightSim& InSim, int32 InStage, const FString& BowHint, TFunction<void(int32)> InOnStage)
		: Sim(InSim), Stage(InStage), OnStage(MoveTemp(InOnStage))
	{
		switch (Stage)
		{
		case 0:
			Say(60, TEXT("Yes?"));
			SetStage(1);
			break;
		case 3:
			Say(60, TEXT("What have you learned, child?"));
			SetStage(4);
			break;
		case 5:
			Say(50, TEXT("Interesting."));
			Say(150, TEXT("(You remember?)"));
			Say(250, TEXT("So do you."));
			Say(380, TEXT("It seems I'm not the only immortal bastard around here."));
			SetStage(6);
			break;
		default:
			Say(60, TEXT("You again."));
			break;
		}
		Say(Stage != 6 ? 240 : 520, BowHint, true);
	}

	void FStrangerRules::SetStage(int32 S)
	{
		if (S > Stage)
		{
			Stage = FMath::Max(Stage, S);
			if (OnStage)
			{
				OnStage(Stage);
			}
		}
	}

	void FStrangerRules::Line(const FString& Text, bool bHint)
	{
		FSimEvent E;
		E.Type = EEvent::Line;
		E.Who = 1;
		E.Text = Text;
		E.bHint = bHint;
		Sim.Emit(E);
		if (!bHint)
		{
			LastLineText = Text;
		}
	}

	bool FStrangerRules::IgnoreContact(FFightSim& S, const FContact& C)
	{
		if (C.Def != 1)
		{
			return false;
		}
		if (C.Proj >= 0)
		{
			S.Projectiles[C.Proj].bDead = true;
		}
		if (bBusy || bEnded)
		{
			return true;
		}
		IdleT = 0;
		switch (Stage)
		{
		case 0:
		case 1:
			Evade(TEXT("Careful, child."));
			SetStage(2);
			break;
		case 2:
			Strike(FString());
			SetStage(3);
			break;
		case 3:
		case 4:
			Crack();
			SetStage(5);
			break;
		default:
			Strike(TEXT("Apparently, very little."));
			break;
		}
		return true;
	}

	void FStrangerRules::Evade(const FString& Text)
	{
		FFighter& D = Sim.Fighters[1];
		if (const FMoveDef* M = D.Def->Move(TEXT("dax_evade")))
		{
			D.StartMove(*M);
		}
		Line(Text);
		FSimEvent E;
		E.Type = EEvent::StrangerEvade;
		Sim.Emit(E);
	}

	// Steps in beside the attacker and catches them, so nothing they were doing can carry on.
	void FStrangerRules::Catch(int32 StunTicks)
	{
		FFighter& A = Sim.Fighters[0];
		FFighter& D = Sim.Fighters[1];
		double Dir = FMath::Sign(A.Pos.X - D.Pos.X);
		if (Dir == 0.0)
		{
			Dir = 1.0;
		}
		D.Pos.X = FMath::Clamp(A.Pos.X - Dir * 0.78 * D.Scale(), -FFightSim::StageHalf + D.Def->PushW, FFightSim::StageHalf - D.Def->PushW);
		D.Facing = A.Pos.X > D.Pos.X ? 1 : -1;
		A.Move = nullptr;
		A.Vel = FVector2D::ZeroVector;
		A.Pos.Y = 0.0;
		A.TakeStun(StunTicks, EStun::Dazed);
		for (FProjectile& P : Sim.Projectiles)
		{
			P.bDead = true;
		}
	}

	void FStrangerRules::Strike(const FString& Text)
	{
		bBusy = true;
		Catch(40);
		FFighter& D = Sim.Fighters[1];
		if (const FMoveDef* M = D.Def->Move(TEXT("dax_palm")))
		{
			D.StartMove(*M);
		}
		if (!Text.IsEmpty())
		{
			Line(Text);
		}
		FSimEvent E;
		E.Type = EEvent::StrangerStrike;
		Sim.Emit(E);
	}

	void FStrangerRules::Crack()
	{
		bBusy = true;
		Catch(110);
		FFighter& D = Sim.Fighters[1];
		if (const FMoveDef* M = D.Def->Move(TEXT("dax_crack")))
		{
			D.StartMove(*M);
		}
		FSimEvent E;
		E.Type = EEvent::StrangerCrack;
		Sim.Emit(E);
	}

	void FStrangerRules::OnTick(FFightSim& S)
	{
		if (bEnded || S.Phase != EPhase::Fight)
		{
			return;
		}
		++T;
		++IdleT;
		for (const FLine& Ln : Lines)
		{
			if (Ln.At == T && !Ln.Text.IsEmpty())
			{
				Line(Ln.Text, Ln.bHint);
			}
		}
		FFighter& A = S.Fighters[0];
		if (!bBusy && A.IsActionable() && (A.Input.Held() & BtnBL) && S.Fighters[1].IsActionable())
		{
			if (++Bow >= BowTicks)
			{
				End(TEXT("Go."));
			}
		}
		else
		{
			Bow = 0;
		}
		if (!bBusy && IdleT >= IdleTicks)
		{
			End(TEXT("Go on."));
		}
	}

	void FStrangerRules::End(const FString& Text)
	{
		bEnded = true;
		Line(Text);
		Sim.Phase = EPhase::MatchEnd;
		Sim.PhaseT = 0;
		FSimEvent E;
		E.Type = EEvent::MatchEnd;
		E.Winner = -1;
		E.Text = Text;
		Sim.Emit(E);
	}
}
