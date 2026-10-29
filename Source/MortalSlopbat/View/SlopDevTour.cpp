#include "View/SlopDevTour.h"

namespace Slop
{
	void FMoveTour::Place()
	{
		// Touching distance (or Gap) on the left (pass 0) or the right (pass 1) of the stage centre.
		FFighter& A = Sim.Fighters[Index];
		FFighter& B = Sim.Fighters[1 - Index];
		const double Side = Pass == 0 ? -1.0 : 1.0;
		A.Pos = FVector2D(Side * Gap * 0.5, 0);
		B.Pos = FVector2D(-Side * Gap * 0.5, 0);
		A.Facing = Pass == 0 ? 1 : -1;
		B.Facing = -A.Facing;
		for (FFighter* F : {&A, &B})
		{
			F->Move = nullptr;
			F->SetState(EState::Idle);
			F->Vel = FVector2D::ZeroVector;
			F->PushV = 0;
			F->Stun = 0;
			F->Hitstop = 0;
			F->Health = F->MaxHealth();
		}
		// Under half health, so moves kept for a desperate fighter (the Storm Shroud) show too.
		A.Health = A.MaxHealth() * 45 / 100;
		Sim.Projectiles.Reset();
		A.ProjectilesOut = B.ProjectilesOut = 0;
	}

	FPadState FMoveTour::Think()
	{
		FFighter& A = Sim.Fighters[Index];
		const FFighter& B = Sim.Fighters[1 - Index];
		if (Done() || Sim.Phase != EPhase::Fight)
		{
			return FPadState();
		}
		if (Intent.Busy())
		{
			return Intent.Next(A.Facing);
		}
		const bool bSettled = A.IsActionable() && B.IsActionable() && A.Pos.Y <= 0.0 && B.Pos.Y <= 0.0 && Sim.Projectiles.Num() == 0;
		if (!bSettled)
		{
			Wait = 20;
			return FPadState();
		}
		if (--Wait > 0)
		{
			return FPadState();
		}
		const TArray<FCommand>& Cmds = A.Def->Commands;
		if (Step >= Cmds.Num())
		{
			Step = 0;
			++Pass;
			Wait = 30;
			return FPadState();
		}
		Place();
		const FCommand& C = Cmds[Step++];
		Current = C.Move.ToString();
		Intent.Command(C.Text);
		Wait = 30;
		return Intent.Next(A.Facing);
	}

	FScriptBrain::FScriptBrain(FFightSim& InSim, int32 InIndex, const FString& Script, TFunction<void(const FString&)> InShot)
		: Sim(InSim), Index(InIndex), Shot(MoveTemp(InShot))
	{
		Script.ParseIntoArray(Steps, TEXT(";"));
		for (FString& S : Steps)
		{
			S.TrimStartAndEndInline();
		}
	}

	FPadState FScriptBrain::Think()
	{
		FFighter& A = Sim.Fighters[Index];
		FFighter& B = Sim.Fighters[1 - Index];
		if (Intent.Busy())
		{
			return Intent.Next(A.Facing);
		}
		if (Wait > 0)
		{
			--Wait;
			return FPadIntent::Pad(5, A.Facing);
		}
		while (At < Steps.Num())
		{
			const FString& S = Steps[At];
			if (S.StartsWith(TEXT("w")) && S.Len() > 1 && FChar::IsDigit(S[1]))
			{
				Wait = FCString::Atoi(*S.Mid(1));
				++At;
				return FPadIntent::Pad(5, A.Facing);
			}
			if (S.StartsWith(TEXT("shot:")))
			{
				Shot(S.Mid(5));
				++At;
				continue;
			}
			if (S.StartsWith(TEXT("qhi:")))
			{
				A.Qhi = FCString::Atod(*S.Mid(4));
				++At;
				continue;
			}
			if (S.StartsWith(TEXT("place:")))
			{
				const double Gap = FCString::Atod(*S.Mid(6));
				const double Mid = (A.Pos.X + B.Pos.X) * 0.5;
				A.Pos = FVector2D(Mid - Gap * 0.5, 0);
				B.Pos = FVector2D(Mid + Gap * 0.5, 0);
				A.Facing = 1;
				B.Facing = -1;
				for (FFighter* F : {&A, &B})
				{
					F->Move = nullptr;
					F->SetState(EState::Idle);
					F->Vel = FVector2D::ZeroVector;
					F->Stun = 0;
					F->Hitstop = 0;
				}
				++At;
				continue;
			}
			if (S == TEXT("fight"))
			{
				// Until a round is under way and the fighter free (after the intro, or the next round).
				if (Sim.Phase == EPhase::Fight && A.IsActionable())
				{
					++At;
					continue;
				}
				return FPadIntent::Pad(5, A.Facing);
			}
			if (S == TEXT("hit"))
			{
				if ((B.ComboHits > HitsBefore && A.Hitstop == 0) || ++Waited > 60)
				{
					Waited = 0;
					++At;
					continue;
				}
				return FPadIntent::Pad(5, A.Facing);
			}
			const bool bNow = S.StartsWith(TEXT("~"));
			if (!bNow && !A.IsActionable())
			{
				return FPadIntent::Pad(5, A.Facing);
			}
			HitsBefore = B.ComboHits;
			Intent.Command(bNow ? S.Mid(1) : S);
			++At;
			return Intent.Next(A.Facing);
		}
		return FPadIntent::Pad(5, A.Facing);
	}
}
