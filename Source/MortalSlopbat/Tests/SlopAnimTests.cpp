#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Anim/SlopAnimator.h"
#include "Core/SlopLog.h"
#include "Misc/Paths.h"

namespace SlopAnimTestSuite
{
using namespace Slop;
using namespace Slop::Anim;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

SLOP_TEST(FSlopRigMatchesOriginal, "Anim.RigMatchesTheOriginal")
bool FSlopRigMatchesOriginal::RunTest(const FString&)
{
	// Tools/reference/rig_reference.json is written by Tools/reference/dump_rig.gd running the
	// original game's own pose library and rig on every pose of every fighter.
	const TSharedPtr<FJsonObject> Ref = FContent::LoadJson(FPaths::ProjectDir() / TEXT("Tools/reference/rig_reference.json"));
	if (!TestTrue("reference present", Ref.IsValid()))
	{
		return false;
	}
	int32 Checked = 0, Bad = 0;
	double WorstPos = 0, WorstDot = 1;
	for (const auto& FV : Ref->Values)
	{
		const FFighterDef* Def = GetContent().Fighter(FName(*FV.Key));
		if (!TestNotNull(FString(*FV.Key), Def))
		{
			continue;
		}
		// The original's poses, without the style refinements made since the port.
		FPoseLibrary Lib;
		Lib.Build(*Def, GetContent().CommonPosesOriginal, false);
		FRig Rig;
		Rig.Build(*Def);
		FQuat Extra[(int32)EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		for (const auto& PV : FV.Value->AsObject()->Values)
		{
			const FPose& P = Lib.Get(FName(*PV.Key));
			const FVector Feet[2] = {P.P(EPos::FootL) + FVector(0, Rig.AnkleH, 0), P.P(EPos::FootR) + FVector(0, Rig.AnkleH, 0)};
			const double Yaws[2] = {FMath::DegreesToRadians(P.V(EVal::YawL)), FMath::DegreesToRadians(P.V(EVal::YawR))};
			Rig.Apply(P, Feet, Yaws, Extra);
			const TSharedPtr<FJsonObject> Joints = PV.Value->AsObject();
			for (int32 J = 0; J <= (int32)EJoint::Count; ++J)
			{
				const bool bTip = J == (int32)EJoint::Count;
				const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(Joints, bTip ? TEXT("hand_tip_r") : JointName((EJoint)J));
				if (!A || A->Num() != 7)
				{
					continue;
				}
				const FVector RP((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
				const FVector MP = bTip ? Rig.HandTip(1) : Rig.Xf[J].P;
				const double DP = FVector::Dist(RP, MP);
				double Dot = 1.0;
				if (!bTip)
				{
					const FQuat RQ((*A)[3]->AsNumber(), (*A)[4]->AsNumber(), (*A)[5]->AsNumber(), (*A)[6]->AsNumber());
					Dot = FMath::Abs(RQ | Rig.Xf[J].Q.GetNormalized());
				}
				WorstPos = FMath::Max(WorstPos, DP);
				WorstDot = FMath::Min(WorstDot, Dot);
				++Checked;
				if (DP > 1e-4 || Dot < 0.99995)
				{
					if (++Bad <= 12)
					{
						AddError(FString::Printf(TEXT("%s %s %s: position off by %.5f m, rotation dot %.6f"), *FString(*FV.Key), *FString(*PV.Key),
							bTip ? TEXT("hand_tip_r") : JointName((EJoint)J), DP, Dot));
					}
				}
			}
		}
	}
	UE_LOG(LogSlop, Display, TEXT("Rig reference: %d joints checked; worst position error %.6f m, worst rotation dot %.7f"), Checked, WorstPos, WorstDot);
	TestTrue("thousands of joints compared", Checked > 10000);
	TestEqual("every joint matches the original", Bad, 0);
	return true;
}

SLOP_TEST(FSlopAnimatorRuns, "Anim.AnimatorFollowsEveryStateOfAMatch")
bool FSlopAnimatorRuns::RunTest(const FString&)
{
	// A scripted match drives both animators through intro, walks, jumps, attacks, stuns, throws,
	// knockdowns and the round end at a mix of frame rates; poses stay finite and feet stay near
	// the floor while standing.
	FFightSim Sim;
	Sim.bIntro = true;
	Sim.Setup(GetContent().Fighter(TEXT("ashka")), GetContent().Fighter(TEXT("gorm")));
	FAnimator A[2];
	for (int32 I = 0; I < 2; ++I)
	{
		A[I].Setup(Sim.Fighters[I], GetContent().CommonPoses);
	}
	FRandomStream Rng(5);
	int32 BadFrames = 0;
	TSet<int32> StatesSeen;
	for (int32 T = 0; T < 60 * 40 && Sim.Phase != EPhase::MatchEnd; ++T)
	{
		FPadState Pads[2];
		for (FPadState& P : Pads)
		{
			const int32 Dirs[] = {0, PadRight, PadLeft, PadDown, PadUp | PadRight, PadRight, PadLeft};
			const int32 Btns[] = {0, 0, 0, BtnHP, BtnLP, BtnHK, BtnLK, BtnBL};
			P.Dirs = Dirs[Rng.RandRange(0, 6)];
			P.Pressed = Btns[Rng.RandRange(0, 7)];
			P.Held = P.Pressed;
		}
		Sim.Step(Pads);
		for (int32 I = 0; I < 2; ++I)
		{
			A[I].SyncFromSim();
			StatesSeen.Add((int32)Sim.Fighters[I].State);
			// Two or three render frames per tick at uneven rates.
			const int32 Frames = 1 + (T % 3);
			for (int32 K = 1; K <= Frames; ++K)
			{
				A[I].Update(double(K) / Frames, 1.0 / (60.0 * Frames));
				for (int32 J = 0; J < (int32)EJoint::Count; ++J)
				{
					const FVector W = A[I].WorldPoint(A[I].Rig.Xf[J].P);
					if (W.ContainsNaN() || W.Size() > 50.0)
					{
						++BadFrames;
					}
				}
			}
		}
	}
	TestEqual("no frame produced a broken pose", BadFrames, 0);
	TestTrue("the match went through many states", StatesSeen.Num() >= 10);
	return true;
}

SLOP_TEST(FSlopAttackingArmOnPose, "Anim.AttackingArmStaysOnItsPose")
bool FSlopAttackingArmOnPose::RunTest(const FString&)
{
	// The body follows through on springs, but the arm that carries a blow must be exactly where
	// the authored pose puts it until the blow is over: the hitbox is there.
	int32 Checked = 0;
	int32 Off = 0;
	for (const FName P1 : {FName(TEXT("ashka")), FName(TEXT("nix")), FName(TEXT("hollis"))})
	{
		FFightSim Sim;
		Sim.Setup(GetContent().Fighter(P1), GetContent().Fighter(TEXT("gorm")));
		FAnimator A;
		A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
		FRandomStream Rng(11);
		for (int32 T = 0; T < 60 * 30 && Sim.Phase != EPhase::MatchEnd; ++T)
		{
			FPadState Pads[2];
			const int32 Dirs[] = {0, PadRight, PadRight, PadDown, PadLeft};
			const int32 Btns[] = {BtnLP, BtnHP, BtnLP, BtnHP, 0};
			Pads[0].Dirs = Dirs[Rng.RandRange(0, 4)];
			Pads[0].Pressed = Pads[0].Held = Btns[Rng.RandRange(0, 4)];
			Sim.Step(Pads);
			A.SyncFromSim();
			A.Update(1.0, 1.0 / 60.0);
			const FFighter& F = Sim.Fighters[0];
			if (F.State != EState::Move || !F.Move || F.ThrowVictim >= 0 || F.Move->Active <= 0 || F.MF > F.Move->LastActive()
				|| (F.Move->Tip != FName(TEXT("hand_l")) && F.Move->Tip != FName(TEXT("hand_r"))))
			{
				continue;
			}
			const bool bRight = F.Move->Tip == FName(TEXT("hand_r"));
			FVector Want = A.Authored().P(bRight ? EPos::HandR : EPos::HandL);
			EPos Shown = bRight ? EPos::HandR : EPos::HandL;
			if (F.Facing < 0)
			{
				Want.X = -Want.X;
				Shown = bRight ? EPos::HandL : EPos::HandR;
			}
			++Checked;
			if (!A.Shown().P(Shown).Equals(Want, 1e-4))
			{
				++Off;
			}
		}
	}
	TestTrue("attacks were checked", Checked > 30);
	TestEqual("the attacking hand left its pose", Off, 0);
	return true;
}

SLOP_TEST(FSlopBlowDirections, "Anim.BlowsMoveTheHeadTheWayTheyTravel")
bool FSlopBlowDirections::RunTest(const FString&)
{
	// A straight drives the struck head away from the attacker, a hook turns the face the way the
	// fist travels, an uppercut lifts the chin and a body shot folds the torso; on either side.
	FFightSim Sim;
	Sim.bIntro = false;
	Sim.Setup(GetContent().Fighter(TEXT("ashka")), GetContent().Fighter(TEXT("gorm")));
	FPadState Pads[2];
	for (int32 T = 0; T < 600 && Sim.Phase != EPhase::Fight; ++T)
	{
		Sim.Step(Pads);
	}
	struct FCase { const TCHAR* Name; ELevel Level; FVector Strike; };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// Fighter 1 stands on the right, so a blow pushes it toward +x.
		const double Dir = Side == 1 ? 1.0 : -1.0;
		const FCase Cases[] = {{TEXT("straight"), ELevel::High, FVector(Dir, 0, 0)}, {TEXT("hook toward the camera"), ELevel::High, FVector(0, 0, 1)},
			{TEXT("hook away from the camera"), ELevel::High, FVector(0, 0, -1)}, {TEXT("uppercut"), ELevel::High, FVector(Dir * 0.3, 1, 0)},
			{TEXT("body shot"), ELevel::Mid, FVector(Dir, 0, 0)}};
		for (const FCase& C : Cases)
		{
			FAnimator A;
			A.Setup(Sim.Fighters[Side], GetContent().CommonPoses);
			for (int32 K = 0; K < 30; ++K)
			{
				A.Update(1.0, 1.0 / 60.0);
			}
			const auto Head = [&A]() { return A.WorldPoint(A.Rig.Xf[(int32)EJoint::Head].P); };
			const auto Face = [&A]() { return (A.Root.Q * A.Rig.Xf[(int32)EJoint::Head].Q).RotateVector(FVector(0, 0, -1)); };
			const FVector H0 = Head(), F0 = Face();
			A.Impact(Dir, 1.0, C.Level, false, C.Strike);
			for (int32 K = 0; K < 5; ++K)
			{
				A.Update(1.0, 1.0 / 60.0);
			}
			const FVector H1 = Head(), F1 = Face();
			const FString What = FString::Printf(TEXT("side %d, %s (head moved %.3f, %.3f, %.3f m; face moved %.3f, %.3f, %.3f)"), Side, C.Name,
				H1.X - H0.X, H1.Y - H0.Y, H1.Z - H0.Z, F1.X - F0.X, F1.Y - F0.Y, F1.Z - F0.Z);
			if (FCString::Strcmp(C.Name, TEXT("straight")) == 0)
			{
				TestTrue(What + TEXT(": head driven away from the attacker"), (H1.X - H0.X) * Dir > 0.01);
			}
			else if (FCString::Strcmp(C.Name, TEXT("hook toward the camera")) == 0)
			{
				TestTrue(What + TEXT(": face turned toward the camera"), F1.Z - F0.Z > 0.05);
			}
			else if (FCString::Strcmp(C.Name, TEXT("hook away from the camera")) == 0)
			{
				TestTrue(What + TEXT(": face turned away from the camera"), F1.Z - F0.Z < -0.05);
			}
			else if (FCString::Strcmp(C.Name, TEXT("uppercut")) == 0)
			{
				TestTrue(What + TEXT(": chin lifted"), F1.Y - F0.Y > 0.05);
			}
			else
			{
				TestTrue(What + TEXT(": torso folded toward the blow, face down"), (H1.X - H0.X) * Dir < -0.005 && F1.Y - F0.Y < -0.03);
			}
		}
	}
	return true;
}

SLOP_TEST(FSlopTurnIsContinuous, "Anim.TurningRoundIsContinuous")
bool FSlopTurnIsContinuous::RunTest(const FString&)
{
	// Crossed over while standing, a fighter turns round over a few frames instead of flipping
	// in one: no joint jumps between frames, and the turn ends facing the new way.
	FFightSim Sim;
	Sim.bIntro = false;
	Sim.Setup(GetContent().Fighter(TEXT("ashka")), GetContent().Fighter(TEXT("gorm")));
	FAnimator A;
	A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
	FPadState Pads[2];
	for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 40); ++T)
	{
		Sim.Step(Pads);
		A.SyncFromSim();
		A.Update(1.0, 1.0 / 60.0);
	}
	FFighter& F = Sim.Fighters[0];
	if (!TestEqual("standing", (int32)F.State, (int32)EState::Idle))
	{
		return false;
	}
	FVector Last[(int32)EJoint::Count];
	for (int32 J = 0; J < (int32)EJoint::Count; ++J)
	{
		Last[J] = A.WorldPoint(A.Rig.Xf[J].P);
	}
	F.Facing = -F.Facing;
	double Worst = 0.0;
	for (int32 K = 0; K < 14; ++K)
	{
		A.Update(1.0, 1.0 / 60.0);
		for (int32 J = 0; J < (int32)EJoint::Count; ++J)
		{
			const FVector W = A.WorldPoint(A.Rig.Xf[J].P);
			Worst = FMath::Max(Worst, (W - Last[J]).Size());
			Last[J] = W;
		}
	}
	TestTrue(FString::Printf(TEXT("largest move of a joint in one frame (%.3f m) is small"), Worst), Worst < 0.3);
	const FVector Front = A.Root.Q.RotateVector(FVector(0, 0, -1));
	TestTrue("facing the new way", Front.X * F.Facing > 0.99);
	return true;
}

}

#undef SLOP_TEST

#endif
