#include "View/SlopDevTour.h"

namespace Slop
{
	void FMoveTour::Place()
	{
		// Touching distance on the left (pass 0) or the right (pass 1) of the stage centre.
		FFighter& A = Sim.Fighters[Index];
		FFighter& B = Sim.Fighters[1 - Index];
		const double Side = Pass == 0 ? -1.0 : 1.0;
		A.Pos = FVector2D(Side * 0.55, 0);
		B.Pos = FVector2D(-Side * 0.55, 0);
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
}
