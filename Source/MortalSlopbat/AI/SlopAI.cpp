#include "AI/SlopAI.h"
#include "Core/SlopFlow.h"

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
			// Beyond Master, for one fight only (the Grandmaster's challenge): the same rules and
			// meters, a human expert's reactions (150 ms) and sharper reads.
			{9, 0.7, 0.85, 0.92, 0.95, 0.72, 0.7, 6, 0.02, 1.0},
		};
		return Levels[FMath::Clamp(Difficulty, 0, 3)];
	}

	FCpuBrain::FCpuBrain(FFightSim& InSim, int32 InIndex, int32 Difficulty, int32 Seed)
		: Sim(InSim), Index(InIndex), Diff(FMath::Clamp(Difficulty, 0, 3)), L(Level(Difficulty))
	{
		Rng.Initialize(Seed != 0 ? Seed : 9001 + InIndex * 31 + Difficulty * 7);
		Ai = Me().Def->AI;
		AiDef = Me().Def;
	}

	void FCpuBrain::SyncForm()
	{
		if (Me().Def == AiDef)
		{
			return;
		}
		AiDef = Me().Def;
		Ai = AiDef->AI;
		ReachCache.Reset();
		Recent.Reset();
		Intent.Clear();
		AaAt = -1;
		// Changed inside a combo: the new shape's blows follow at once.
		bFollowUp = Opp().State == EState::HitStun || Opp().State == EState::Juggle;
	}

	double FCpuBrain::QhiSetting(const TCHAR* Key, double Default) const
	{
		const TSharedPtr<FJsonObject> Q = Json::Obj(Me().BaseDef->AI, TEXT("qhi"));
		if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(Q, Key))
		{
			return A->Num() > 0 ? (*A)[FMath::Min(Diff, A->Num() - 1)]->AsNumber() : Default;
		}
		return Json::Num(Q, Key, Default);
	}

	bool FCpuBrain::Affordable(const FMoveDef& Mv) const
	{
		const FFighter& M = Me();
		return Mv.QhiCost <= 0.0 || (M.UsesQhi() && M.Qhi - Mv.QhiCost >= Reserve());
	}

	double FCpuBrain::Reserve() const
	{
		const FFighter& M = Me();
		const FFighter& O = Opp();
		double Keep = QhiSetting(TEXT("keep"), 0.0);
		if (double(M.Health) / M.MaxHealth() < 0.35)
		{
			// In danger: something kept back for a ward or a change of shape out of trouble.
			Keep += 10.0;
		}
		if (double(O.Health) / O.MaxHealth() < 0.25)
		{
			// Closing the round out: spend.
			Keep = FMath::Max(0.0, Keep - 10.0);
		}
		return Keep;
	}

	FName FCpuBrain::PickForm(const FFighter& M, bool bInCombo) const
	{
		if (!M.UsesQhi() || M.Form == EForm::Spent)
		{
			return NAME_None;
		}
		const TSharedPtr<FJsonObject> Q = Json::Obj(M.BaseDef->AI, TEXT("qhi"));
		const double Spare = M.Qhi - Reserve();
		auto Usable = [&M, Spare](FName Id)
		{
			const FMoveDef* Mv = M.Def->Move(Id);
			return Mv && M.CanUse(*Mv) && Spare >= M.ShapeCost(*Mv);
		};
		const FFighter& O = Opp();
		const FFighterDef* Facing = O.Def->CopyOf ? O.Def->CopyOf : O.BaseDef;
		// The Fragment's shape with a full meter, at the best levels (facing the Fragment itself,
		// its copy below is the cheaper way in).
		const FName Fragment = TEXT("shape_slopbat");
		if (!bInCombo && Facing->Id != TEXT("slopbat") && M.Qhi >= QhiSetting(TEXT("fragment_from"), 101.0) && Usable(Fragment)
			&& Rng.GetFraction() < QhiSetting(TEXT("fragment_chance"), 0.0))
		{
			return Fragment;
		}
		// With the meter high, the opponent's own shape, enhanced (from his own body only).
		const FName Copy = TEXT("borrowed_face");
		if (M.Form == EForm::Own && M.Qhi >= QhiSetting(TEXT("shapeshift_from"), 90.0) && Usable(Copy) && (!bInCombo || Diff >= 3))
		{
			return Copy;
		}
		// A remembered shape (30, not enhanced) is the quick way to a second shape inside a combo;
		// from a standstill it is worth less than the copy, so only taken where the copy is refused
		// (a mirror match), with the meter as high as the copy would need.
		const bool bCopyRefused = M.Form == EForm::Own && !M.BorrowRefusal().IsNone();
		if (bInCombo ? M.Qhi < QhiSetting(TEXT("combo_form_from"), 101.0) : !(bCopyRefused && M.Qhi >= QhiSetting(TEXT("shapeshift_from"), 90.0)))
		{
			return NAME_None;
		}
		// A remembered shape for the matchup: against a fighter who keeps away, one that rushes in;
		// against one who rushes in, one that keeps away (each fighter's own AI notes say which it is).
		// A data override ("forms_vs": opponent -> form) wins.
		const FName Named(*Json::Str(Json::Obj(Q, TEXT("forms_vs")), *Facing->Id.ToString()));
		if (!Named.IsNone() && Usable(FName(*(TEXT("shape_") + Named.ToString()))))
		{
			return FName(*(TEXT("shape_") + Named.ToString()));
		}
		const double Zo = Json::Num(Facing->AI, TEXT("zoning"), 0.2);
		FName Best;
		double BestScore = -1.0;
		for (const TPair<FName, const FFighterDef*>& KV : M.BaseDef->FormSources)
		{
			const FName Id(*(TEXT("shape_") + KV.Key.ToString()));
			if (KV.Key == TEXT("slopbat") || !Usable(Id))
			{
				continue;
			}
			// Inside a combo a shape with a long string of its own carries it further.
			int32 Longest = 0;
			if (const TArray<TSharedPtr<FJsonValue>>* C = Json::Arr(KV.Value->AI, TEXT("combos")))
			{
				for (const TSharedPtr<FJsonValue>& V : *C)
				{
					Longest = FMath::Max(Longest, V->AsArray().Num());
				}
			}
			const double Score = Zo * Json::Num(KV.Value->AI, TEXT("aggression"), 0.5) + (1.0 - Zo) * Json::Num(KV.Value->AI, TEXT("zoning"), 0.2)
				+ (bInCombo ? 0.1 * FMath::Max(0, Longest - 2) : 0.0) + 0.15 * Rng.GetFraction();
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Id;
			}
		}
		return Best;
	}

	bool FCpuBrain::ComboShift(FFighter& M)
	{
		if (!M.UsesQhi() || M.Form == EForm::Spent || M.State != EState::Move || !M.Move || M.MoveContact != TEXT("hit")
			|| M.MF <= M.Move->Startup || M.FormShifts >= M.QhiRules().MaxShifts)
		{
			return false;
		}
		const FFighter& O = Opp();
		if (O.State != EState::HitStun && O.State != EState::Juggle)
		{
			return false;
		}
		// The better levels open with two blows of his own first, so the combo can run long.
		if (M.Form == EForm::Own && Diff >= 2 && O.ComboHits < 2)
		{
			return false;
		}
		// One try a blow, pressed as its hitstop ends (a motion begun inside it).
		const int32 Started = Sim.TickCount - M.MF;
		if (Started == ShiftTriedAt || M.Hitstop > 3)
		{
			return false;
		}
		ShiftTriedAt = Started;
		if (Rng.FRand() >= QhiSetting(TEXT("combo_shift"), 0.0))
		{
			return false;
		}
		FName To;
		if (M.Form == EForm::Borrowed)
		{
			// A shape is worth holding: it is given back inside a combo to save the Qhi it costs, or,
			// at the better levels, to carry a long combo on in his own body toward Heart Stop (free,
			// and his own body is the combo's next shape).
			const FMoveDef* Hs = M.Def->Move(FName(*Json::Str(Json::Obj(Json::Obj(M.BaseDef->AI, TEXT("qhi")), TEXT("kills")), TEXT("combo"))));
			const bool bToward = Diff >= 2 && Hs && O.ComboHits >= 3 && M.Qhi >= Hs->QhiCost;
			if (M.Qhi - Reserve() >= 20.0 && !bToward)
			{
				return false;
			}
			To = TEXT("own_face");
		}
		else
		{
			To = PickForm(M, true);
		}
		const FMoveDef* Mv = To.IsNone() ? nullptr : M.Def->Move(To);
		if (!Mv || !M.CanUse(*Mv))
		{
			return false;
		}
		Intent.Clear();
		Intent.Command(Notation(To));
		return true;
	}

	bool FCpuBrain::KillPlan(FFighter& M, const FSeen& Seen, double Dist)
	{
		if (!M.UsesQhi() || M.Form == EForm::Spent || Diff < 1 || Intent.Busy())
		{
			return false;
		}
		const TSharedPtr<FJsonObject> K = Json::Obj(Json::Obj(M.BaseDef->AI, TEXT("qhi")), TEXT("kills"));
		// Heart Stop the moment a combo allows it (the simulation's own gate: hits, shapes, caught),
		// from neutral or out of the last blow's cancel window, as a player would.
		const FName Combo(*Json::Str(K, TEXT("combo")));
		if (const FMoveDef* Hs = M.Def->Move(Combo); Hs && M.CanUse(*Hs))
		{
			const int32 Lead = FCommand::Parse(Notation(Combo)).Steps.Num() + 1;
			const bool bCancel = M.State == EState::Move && M.Move && M.MoveContact == TEXT("hit") && M.Move->Cancel != FIntPoint::ZeroValue
				&& M.MF + Lead >= M.Move->Cancel.X && M.MF + Lead <= M.Move->Cancel.Y;
			if ((M.IsActionable() || bCancel) && Rng.FRand() < 0.4 + 0.2 * Diff)
			{
				Intent.Command(Notation(Combo));
				return true;
			}
		}
		if (!M.IsActionable())
		{
			return false;
		}
		// Find the Crack: an opponent standing guard (or idling) within its reach, the meter near
		// full. A guard is no answer to it, a blow in its long startup is, so it is a read on a turtle.
		const FName Crack(*Json::Str(K, TEXT("standing")));
		const FMoveDef* Fc = M.Def->Move(Crack);
		if (!Fc || !M.CanUse(*Fc) || M.Qhi < QhiSetting(TEXT("crack_from"), 95.0) || Seen.bAir || !Seen.bActionable || Dist > Reach(Crack) + 0.05)
		{
			return false;
		}
		const bool bGuard = Seen.State == EState::Block || Seen.State == EState::CrouchBlock || Seen.State == EState::BlockStun;
		const bool bStill = bGuard || Seen.State == EState::Idle || Seen.State == EState::Crouch;
		if (!bStill)
		{
			return false;
		}
		double Chance = QhiSetting(TEXT("crack_chance"), 0.0) * (bGuard ? 3.0 : 1.0);
		const FFighter& O = Opp();
		if (double(M.Health) / M.MaxHealth() < 0.3 && double(O.Health) / O.MaxHealth() > 0.5)
		{
			// Far behind: the one blow that can turn the round.
			Chance *= 2.0;
		}
		if (Rng.FRand() < Chance)
		{
			Intent.Command(Notation(Crack));
			return true;
		}
		return false;
	}

	void FCpuBrain::Observe(const FFighter& M)
	{
#if !UE_BUILD_SHIPPING
		if (!M.UsesQhi())
		{
			return;
		}
		const bool bFight = Sim.Phase == EPhase::Fight;
		if (bFight)
		{
			++Tele.Ticks;
			if (LastQhi >= 0.0)
			{
				const double Delta = M.Qhi - LastQhi;
				(Delta < 0.0 ? Tele.Spent : Tele.Gained) += FMath::Abs(Delta);
			}
			const FName In = FormName(M.Form);
			++Tele.FormTicks.FindOrAdd(In);
			Tele.Dealt.FindOrAdd(In) += FMath::Max(0, Opp().DamageTaken - LastDealt);
			Tele.Taken.FindOrAdd(In) += FMath::Max(0, M.DamageTaken - LastTaken);
			for (const TPair<FName, TSharedRef<FMoveDef>>& KV : M.Def->Moves)
			{
				const FMoveDef& Mv = *KV.Value;
				if (!Mv.bKill)
				{
					continue;
				}
				// A counter's chance is its Qhi in hand while the catch that leads to it can begin.
				bool bChance = M.IsActionable() && M.CanUse(Mv);
				for (const TPair<FName, TSharedRef<FMoveDef>>& P : M.Def->Moves)
				{
					if (P.Value->ParryCounter == Mv.Id)
					{
						bChance = M.IsActionable() && M.CanUse(*P.Value) && M.Qhi >= Mv.QhiCost;
					}
				}
				if (bChance)
				{
					++Tele.KillChances.FindOrAdd(Mv.Id);
				}
			}
		}
		LastQhi = M.Qhi;
		LastDealt = Opp().DamageTaken;
		LastTaken = M.DamageTaken;
		for (const FSimEvent& E : Sim.Events)
		{
			if (E.Type == EEvent::Form && E.Who == Index)
			{
				if (E.Kind == TEXT("borrowed") || (E.Kind == TEXT("own") && bFight))
				{
					++Tele.Forms.FindOrAdd(E.Kind == TEXT("own") ? FName(TEXT("own")) : E.Move);
					Tele.ComboShifts += Opp().ComboHits > 0 ? 1 : 0;
				}
				Tele.Spents += E.Kind == TEXT("spent") ? 1 : 0;
				Tele.Breaks += E.Kind == TEXT("broken") ? 1 : 0;
			}
			else if (E.Type == EEvent::Move && E.Who == Index)
			{
				if (const FMoveDef* Mv = M.Def->Move(E.Move); Mv && Mv->bKill)
				{
					++Tele.KillTries.FindOrAdd(E.Move);
				}
			}
			else if (E.Type == EEvent::Hit && E.By == Index && E.bKill)
			{
				++Tele.Kills.FindOrAdd(E.Move);
			}
			else if (E.Type == EEvent::ComboEnd && E.Who != Index && E.Hits >= 2)
			{
				++Tele.Combos.FindOrAdd(E.Hits >= 6 ? FName(TEXT("6+")) : FName(*FString::FromInt(E.Hits)));
			}
			else if (E.Type == EEvent::RoundEnd)
			{
				++Tele.Rounds;
				Tele.Unused += FightQhi;
				if (E.Winner >= 0 && E.Winner != Index)
				{
					++Tele.Losses.FindOrAdd(FormName(LastForm));
				}
			}
		}
		if (bFight)
		{
			FightQhi = M.Qhi;
			LastForm = M.Form;
		}
#endif
	}

	void FCpuBrain::FQhiTelemetry::Add(const FQhiTelemetry& O)
	{
		auto Merge = [](TMap<FName, int32>& Into, const TMap<FName, int32>& From)
		{
			for (const TPair<FName, int32>& KV : From)
			{
				Into.FindOrAdd(KV.Key) += KV.Value;
			}
		};
		Ticks += O.Ticks;
		Spent += O.Spent;
		Gained += O.Gained;
		ComboShifts += O.ComboShifts;
		Spents += O.Spents;
		Breaks += O.Breaks;
		Unused += O.Unused;
		Rounds += O.Rounds;
		Merge(Forms, O.Forms);
		Merge(KillChances, O.KillChances);
		Merge(KillTries, O.KillTries);
		Merge(Kills, O.Kills);
		Merge(Losses, O.Losses);
		Merge(FormTicks, O.FormTicks);
		Merge(Combos, O.Combos);
		Merge(Dealt, O.Dealt);
		Merge(Taken, O.Taken);
	}

	FString FCpuBrain::FQhiTelemetry::Summary() const
	{
		auto List = [](const TMap<FName, int32>& Map, double Per = 1.0)
		{
			FString Out;
			for (const TPair<FName, int32>& KV : Map)
			{
				Out += FString::Printf(TEXT("%s%s %.0f"), Out.IsEmpty() ? TEXT("") : TEXT(", "), *KV.Key.ToString(), KV.Value / Per);
			}
			return Out.IsEmpty() ? FString(TEXT("none")) : Out;
		};
		const double Secs = FMath::Max(1, Ticks) / 60.0;
		// Per form: share of the time, and damage dealt and taken a second in it.
		FString PerForm;
		for (const TPair<FName, int32>& KV : FormTicks)
		{
			const double S = FMath::Max(1, KV.Value) / 60.0;
			PerForm += FString::Printf(TEXT("%s%s %.0f%% (deals %.0f/s, takes %.0f/s)"), PerForm.IsEmpty() ? TEXT("") : TEXT(", "), *KV.Key.ToString(),
				100.0 * KV.Value / FMath::Max(1, Ticks), Dealt.FindRef(KV.Key) / S, Taken.FindRef(KV.Key) / S);
		}
		return FString::Printf(TEXT("forms [%s]; combos by hits [%s]; Qhi spent %.0f (%.1f/s), gained %.0f; shapes [%s], %d inside combos; spent %d, broken %d; ")
			TEXT("kill chances (s) [%s], tried [%s], kills [%s]; unused at round end %.0f avg over %d rounds; losses by form [%s]"),
			*PerForm, *List(Combos), Spent, Spent / Secs, Gained, *List(Forms), ComboShifts, Spents, Breaks, *List(KillChances, 60.0), *List(KillTries), *List(Kills),
			Rounds > 0 ? Unused / Rounds : 0.0, Rounds, *List(Losses));
	}

	bool FCpuBrain::QhiPlan(const FFighter& M, const FSeen& Seen, double Dist)
	{
		if (!M.UsesQhi() || !M.IsActionable())
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Q = Json::Obj(M.BaseDef->AI, TEXT("qhi"));
		const bool bOppBusy = Seen.State == EState::Move || Seen.bAir;
		if (M.Form == EForm::Own)
		{
			// Borrow the shape with the meter well above the price, and only where its long
			// startup is safe: while the opponent (as seen) is down or getting up, or from far off
			// and not into an attack already coming. A better CPU waits for the knockdown.
			const bool bDown = Seen.State == EState::Down || Seen.State == EState::WakeUp || Seen.State == EState::Fall;
			const bool bFar = Dist > QhiSetting(TEXT("far_from"), 2.8) && !bOppBusy;
			if ((bDown && Dist > 0.9 && Rng.FRand() < 0.3 + 0.3 * Diff) || (bFar && Rng.FRand() < QhiSetting(TEXT("far_shift"), 0.03)))
			{
				// Which shape: the opponent's, a remembered one for the matchup, or the Fragment's.
				const FName Form = PickForm(M, false);
				if (!Form.IsNone())
				{
					Intent.Command(Notation(Form));
					return true;
				}
			}
		}
		else if (M.Form == EForm::Borrowed)
		{
			// Give the shape back before the Qhi runs out (a novice holds on until it breaks).
			// (A shape that costs more to hold, the Fragment's, is given back sooner.)
			const double Below = QhiSetting(TEXT("revert_below"), 10.0) * M.UpkeepScale;
			if (Below > 0.0 && M.Qhi < Below && !bOppBusy && Dist > 1.3 && Rng.FRand() < 0.5)
			{
				Intent.Command(Json::Str(Q, TEXT("revert")));
				return true;
			}
		}
		else if (M.Form == EForm::Spent)
		{
			// Spent, the old man boxes to survive: he gives ground when pressed (his retreat when the
			// shape has one, a step back otherwise) and guards (the spent AI's "guard").
			if (Dist < 1.5 && Seen.bActionable && Rng.FRand() < 0.05 + 0.05 * Diff)
			{
				const TArray<TSharedPtr<FJsonValue>>* R = AiArr(TEXT("retreat"));
				if (R && R->Num() > 0)
				{
					Intent.Command((*R)[Rng.RandRange(0, R->Num() - 1)]->AsString());
				}
				else
				{
					Intent.Hold(4, Rng.RandRange(10, 20));
				}
				return true;
			}
		}
		return false;
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
		SyncForm();
		const FSeen Seen = Perceive();
		Observe(M);
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
		NoteBlocked(M);
		// The Grandmaster feels his blow land and changes shape inside the combo.
		if (ComboShift(M))
		{
			return Intent.Next(M.Facing);
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
		if (PunishRead(M))
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
			// (Heart Stop may still be cancelled into from the blow that set it up.)
			if (KillPlan(M, Seen, FMath::Abs(Seen.Pos.X - M.Pos.X)))
			{
				return Intent.Next(M.Facing);
			}
			return FPadIntent::Pad(5, M.Facing);
		}
		FFighter& O = Opp();
		// Distances too are as seen: the CPU judges spacing from what was on screen, not from now.
		const double Dist = FMath::Abs(Seen.Pos.X - M.Pos.X);
		// A change of shape inside a combo: the new shape carries the combo on (or ends it).
		if (bFollowUp)
		{
			bFollowUp = false;
			if (KillPlan(M, Seen, Dist))
			{
				return Intent.Next(M.Facing);
			}
			if (O.State == EState::HitStun || O.State == EState::Juggle)
			{
				StartCombo(false, Diff >= 2);
				return Intent.Next(M.Facing);
			}
		}
		if (KillPlan(M, Seen, Dist))
		{
			return Intent.Next(M.Facing);
		}
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
		const FMoveDef* SM = Seen.Move;
		// --- Reactions to what has been seen ---
		if (Seen.State == EState::Move && SM && SM->IsAttack() && SM->Level == ELevel::Unblockable && Seen.MF < SM->Startup && Dist < 2.4
			&& Seen.Tick - Seen.MF != UnblockJudged)
		{
			// No guard stops it (the Grandmaster's Find the Crack): strike it in its startup if there
			// is time and reach, otherwise back out of reach. Judged once a blow.
			UnblockJudged = Seen.Tick - Seen.MF;
			if (M.IsActionable() && Rng.FRand() < L.Block)
			{
				const FString Hit = AttackFor(Dist);
				if (!Hit.IsEmpty() && SM->Startup - Seen.MF - L.React > 12)
				{
					Intent.Command(Hit);
				}
				else
				{
					Intent.Hold(4, 24);
				}
				return Intent.Next(M.Facing);
			}
		}
		if (Seen.State == EState::Move && SM && SM->IsAttack() && Seen.MF <= SM->LastActive() && Dist < 2.4 && SM->Level != ELevel::Unblockable)
		{
			// (A shape's own AI notes may guard more: the Grandmaster's spent, boxing body does.)
			if (M.State != EState::Air && Rng.FRand() < L.Block + Json::Num(Ai, TEXT("guard"), 0.0))
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
				if (const FMoveDef* AaDef = M.Def->Move(Aa); AaDef && !Affordable(*AaDef))
				{
					Aa = TEXT("uppercut");
				}
				Intent.Command(Notation(Aa));
				return Intent.Next(M.Facing);
			}
		}
		if (AnswerThrow(M, Seen, Dist))
		{
			return Intent.Next(M.Facing);
		}
		for (const FProjectile& P : Sim.Projectiles)
		{
			if (P.Owner != Index && P.Age > L.React && FMath::Abs(P.Pos.X - M.Pos.X) < 3.0 && FMath::Sign(M.Pos.X - P.Pos.X) == FMath::Sign(P.Vel.X))
			{
				// A fighter with a ward (the Grandmaster's) breaks the shot on it when it can spare the Qhi.
				if (const TArray<TSharedPtr<FJsonValue>>* W = AiArr(TEXT("wards")))
				{
					const FName WardMove = W->Num() > 0 ? FName(*(*W)[0]->AsString()) : NAME_None;
					const FMoveDef* WM = M.Def->Move(WardMove);
					if (WM && Affordable(*WM) && M.CanUse(*WM) && Rng.FRand() < 0.3 + 0.2 * Diff)
					{
						Intent.Command(Notation(WardMove));
						return Intent.Next(M.Facing);
					}
				}
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
		// Punish a whiffed attack still recovering, judged once a move, with a blow that reaches it
		// in the time left after the delay. (The old check left a tick or two to act in, and then
		// threw the launcher whatever the distance.)
		if (Seen.State == EState::Move && SM && SM->IsAttack() && Seen.MF > SM->LastActive() && Seen.Tick - Seen.MF != WhiffJudged && M.IsActionable() && Sim.TickCount >= NoWhiffBefore)
		{
			WhiffJudged = Seen.Tick - Seen.MF;
			const FName Best = PunisherFor(SM->Total - Seen.MF - L.React, Dist);
			if (!Best.IsNone() && Rng.FRand() < L.Punish)
			{
				Intent.Command(Notation(Best));
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
		if (QhiPlan(M, Seen, Dist))
		{
			return Intent.Next(M.Facing);
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
		// A zoner fires from its own range (fighter AI "zone_from"); the others only from afar.
		if (Dist > Json::Num(Ai, TEXT("zone_from"), 3.4) && Dist <= 3.4)
		{
			const TArray<FName> Zoner = MovesTagged(TEXT("projectile"));
			if (Zoner.Num() > 0 && Rng.FRand() < Json::Num(Ai, TEXT("zoning"), 0.2))
			{
				Intent.Command(Notation(Zoner[Rng.RandRange(0, Zoner.Num() - 1)]));
				return;
			}
		}
		if (Dist > 3.4)
		{
			// A fighter with a way in (Wren's Flicker Step, through projectiles) takes it now and then.
			if (const TArray<TSharedPtr<FJsonValue>>* In = AiArr(TEXT("approach")))
			{
				if (In->Num() > 0 && Rng.FRand() < 0.25 * Aggr)
				{
					Intent.Command((*In)[Rng.RandRange(0, In->Num() - 1)]->AsString());
					return;
				}
			}
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
		// Under fire at range, guarding on the spot only loses: use the gaps between shots to close.
		if (BeingZoned() && Dist > 1.4)
		{
			Walk = 6;
			WalkT = Rng.RandRange(14, 26);
			RunT = M.Def->Resource == EResource::Run && M.RunMeter > 30.0 ? WalkT : 0;
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

	bool FCpuBrain::AnswerThrow(FFighter& M, const FSeen& Seen, double Dist)
	{
		const FMoveDef* SM = Seen.Move;
		if (!SM || !SM->Projectile.bHas || Seen.State != EState::Move || SM->bAir || Seen.Tick - Seen.MF == ThrowJudged)
		{
			return false;
		}
		ThrowJudged = Seen.Tick - Seen.MF;
		Zoned = Sim.TickCount - ZonedAt < 240 ? Zoned + 1 : 1;
		ZonedAt = Sim.TickCount;
		if (!M.IsActionable() || Dist < 1.2 || Dist > 3.6)
		{
			return false;
		}
		// The more shots have come, the surer the read: guarding each one only loses the round.
		if (Rng.FRand() >= L.AntiAir * 0.55 + FMath::Min(0.3, 0.1 * Zoned))
		{
			return false;
		}
		// A low shot rolls under nobody's jump; a high one can also be passed by a way in of one's own.
		if (const TArray<TSharedPtr<FJsonValue>>* In = AiArr(TEXT("approach")))
		{
			if (In->Num() > 0 && Rng.FRand() < 0.4)
			{
				Intent.Command((*In)[Rng.RandRange(0, In->Num() - 1)]->AsString());
				return true;
			}
		}
		Intent.Hold(9, 3);
		Intent.Hold(5, 18);
		Intent.Press(5, BtnHK);
		return true;
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
		if (Mv && !Affordable(*Mv))
		{
			// A Qhi anti-air the meter cannot spare: the plain uppercut instead.
			AaMove = TEXT("uppercut");
			Mv = M.Def->Move(AaMove);
		}
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
			// A catch whose answer costs Qhi (Two Fingers into One Strike) is only a read worth
			// making with the Qhi in hand.
			if (const FMoveDef* Answer = M.Def->Move(PM->ParryCounter); Answer && Answer->QhiCost > 0.0 && (!M.UsesQhi() || M.Qhi < Answer->QhiCost))
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

	// A blocked blow is felt the moment it lands, not a reaction later, and a fighter who knows
	// the move knows how long it leaves its attacker stuck. (The punish answered a delayed sight
	// of the recovery before, which mostly came too late, so the CPU almost never took one.) A
	// cancel into something else after the block makes the read wrong, as it would a player's.
	void FCpuBrain::NoteBlocked(const FFighter& M)
	{
		const FFighter& O = Opp();
		// Hit, thrown or knocked down since: the read no longer stands.
		if (M.State != EState::BlockStun && !M.IsActionable())
		{
			Blocked = nullptr;
		}
		for (const FSimEvent& E : Sim.Events)
		{
			if (E.Type == EEvent::Block && E.Who == Index && E.By == 1 - Index && !E.bProj && O.State == EState::Move && O.Move && O.Move->Id == E.Move)
			{
				// A jump-in is over when its attacker lands, and a few frames after: when that is
				// follows from the arc, stepped as the simulation steps it (a deep one lands at
				// once, while its frames would say it had far longer to run).
				int32 Rest = O.Move->Total - O.MF;
				if (O.Move->bAir)
				{
					double Y = O.Pos.Y, Vy = O.Vel.Y;
					int32 Fall = 0;
					while (Y > 0.0 && Fall < 240)
					{
						Vy -= O.Def->Gravity * DT;
						Y += Vy * DT;
						++Fall;
					}
					Rest = Fall + FFighter::LandFrames;
					// Down and free before the guard is: an anti-air timed from the sight of the
					// jump would only swing at a fighter already guarding again.
					if (Rest <= M.Stun)
					{
						AaAt = -1;
					}
				}
				// The attacker is free once the hit-stop and the rest of the move are over. The push
				// felt on the guard opens the distance by its whole travel (in a corner the
				// attacker is the one pushed back).
				Blocked = O.Move;
				BlockedFreeAt = Sim.TickCount + O.Hitstop + Rest;
				BlockedDist = FMath::Abs(O.Pos.X - M.Pos.X);
				for (double V = FMath::Abs(M.PushV); V > 0.0; V = FMath::Max(0.0, V - M.PushDec))
				{
					BlockedDist += V * DT;
				}
			}
		}
	}

	bool FCpuBrain::PunishRead(const FFighter& M)
	{
		if (!Blocked || !M.IsActionable())
		{
			return false;
		}
		Blocked = nullptr;
		NoWhiffBefore = BlockedFreeAt;
		if (Rng.FRand() >= L.Punish)
		{
			return false;
		}
		// A gap too short for any blow, or pushed out of reach of all of them, is left alone.
		const FName Best = PunisherFor(BlockedFreeAt - Sim.TickCount, BlockedDist);
		if (Best.IsNone())
		{
			return false;
		}
		Intent.Command(Notation(Best));
		return true;
	}

	void FCpuBrain::Forget()
	{
		Intent = FPadIntent();
		BlockT = 0;
		AaAt = -1;
		Blocked = nullptr;
		WhiffJudged = -1;
		NoWhiffBefore = 0;
		SeenHistory.Reset();
	}

	FName FCpuBrain::PunisherFor(int32 Gap, double Dist)
	{
		const FFighter& M = Me();
		FName Best;
		int32 BestDamage = 0;
		for (const TPair<FName, TSharedRef<FMoveDef>>& P : M.Def->Moves)
		{
			const FMoveDef& Mv = *P.Value;
			if (!Mv.IsAttack() || Mv.bAir || Mv.IsThrowKind() || Mv.bParry || Mv.Projectile.bHas || Mv.Kind == EMoveKind::Chain || Mv.Kind == EMoveKind::Finisher
				|| M.Def->NotationOf(P.Key).IsEmpty() || !M.CanUse(Mv) || !Affordable(Mv))
			{
				continue;
			}
			if (Mv.Damage > BestDamage && TicksToHit(P.Key) <= Gap && Reach(P.Key) > Dist)
			{
				Best = P.Key;
				BestDamage = Mv.Damage;
			}
		}
		return Best;
	}

	int32 FCpuBrain::TicksToHit(FName MoveId) const
	{
		const FMoveDef* Mv = Me().Def->Move(MoveId);
		return Mv ? FCommand::Parse(Notation(MoveId)).Steps.Num() + 1 + Mv->Startup : 999;
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

	void FCpuBrain::StartCombo(bool bPunish, bool bLongest)
	{
		const TArray<TSharedPtr<FJsonValue>>* Combos = AiArr(TEXT("combos"));
		if (!Combos || Combos->Num() == 0 || Rng.FRand() > L.Combo)
		{
			Intent.Command(bPunish ? TEXT("D+HP") : TEXT("HP"));
			return;
		}
		int32 Pick = Rng.RandRange(0, Combos->Num() - 1);
		for (int32 I = 0; bLongest && I < Combos->Num(); ++I)
		{
			Pick = (*Combos)[I]->AsArray().Num() > (*Combos)[Pick]->AsArray().Num() ? I : Pick;
		}
		const TArray<TSharedPtr<FJsonValue>>& C = (*Combos)[Pick]->AsArray();
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
			// (The instant kills are chosen by KillPlan, never as an ordinary blow.)
			if (!Mv || Mv->IsThrowKind() || Mv->bFinisher || Mv->bKill || !Mv->IsAttack() || Mv->Hitboxes.Num() == 0)
			{
				continue;
			}
			if ((Mv->Kind == EMoveKind::Special && !bSpecialsOk) || !Affordable(*Mv))
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
			if (Mv && Mv->HasTag(Tag) && Affordable(*Mv))
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
		// An unblockable blow winding up (the Grandmaster's Find the Crack): claw it in its long
		// startup when close, gust it from further off.
		if (O.State == EState::Move && O.Move && O.Move->Level == ELevel::Unblockable && O.MF + L.React < O.Move->Startup && Dist < 2.4
			&& Rng.FRand() < 0.1)
		{
			Do(Dist < 1.6 ? TEXT("HP") : TEXT("B,F+HK"));
			return Intent.Next(M.Facing);
		}
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
		if (Name == TEXT("CPU"))
		{
			// At the difficulty set in Settings.
			return FString::Printf(TEXT("CPU (%s)"), *FGameFlow::DifficultyName(Level).ToString());
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

	FTrainingDummy::FTrainingDummy(FFightSim& InSim, int32 InIndex, int32 InLevel) : Sim(InSim), Index(InIndex), Level(FMath::Clamp(InLevel, 0, 2))
	{
		Rng.Initialize(77);
	}

	void FTrainingDummy::SetLevel(int32 InLevel)
	{
		const int32 L = FMath::Clamp(InLevel, 0, 2);
		if (L != Level)
		{
			Level = L;
			if (Cpu)
			{
				Cpu = MakeUnique<FCpuBrain>(Sim, Index, Level);
			}
		}
	}

	void FTrainingDummy::Forget()
	{
		if (Cpu)
		{
			Cpu->Forget();
		}
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
			Cpu = MakeUnique<FCpuBrain>(Sim, Index, Level);
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
}
