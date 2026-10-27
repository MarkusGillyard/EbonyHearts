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

// A blow should be seen to land: on its first active frame some striking part of the body (a hand,
// foot, elbow, knee or the head) is inside, or close to, the move's hitbox, on the ground or in
// the air. (Moves tagged "sound"
// reach further than any limb and are shown by rings instead.)
SLOP_TEST(FSlopStrikesReachBoxes, "Anim.StrikesReachTheirHitboxes")
bool FSlopStrikesReachBoxes::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Special());
	int32 Checked = 0;
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		for (const TPair<FName, TSharedRef<FMoveDef>>& Pair : Def->Moves)
		{
			const FMoveDef& M = *Pair.Value;
			if (M.Projectile.bHas || M.IsThrowKind() || M.Hitboxes.Num() == 0 || M.Active <= 0 || M.bFinisher)
			{
				continue;
			}
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(Def, GetContent().Fighter(TEXT("gorm")));
			FAnimator A;
			A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
			FPadState Idle[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
			{
				Sim.Step(Idle);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			FFighter& F = Sim.Fighters[0];
			// Air attacks start high enough to reach their first active frame before landing.
			F.Pos = FVector2D(-2.0, M.bAir ? 1.4 : 0.0);
			F.Vel = FVector2D::ZeroVector;
			Sim.Fighters[1].Pos = FVector2D(6.0, 0.0);
			F.StartMove(M);
			F.SetState(EState::Move);
			for (int32 T = 0; T < 200 && F.Move == &M && F.MF < M.Startup; ++T)
			{
				Sim.Step(Idle);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			if (F.Move != &M)
			{
				continue;
			}
			TArray<TPair<int32, FRect>> Boxes;
			F.ActiveHitboxes(Boxes);
			if (Boxes.Num() == 0)
			{
				continue;
			}
			const FRig& R = A.Rig;
			const FVector Tips[] = {R.HandTip(0), R.HandTip(1), R.FootTip(0), R.FootTip(1), R.J(EJoint::ForearmL).P, R.J(EJoint::ForearmR).P,
				R.J(EJoint::ShinL).P, R.J(EJoint::ShinR).P, R.J(EJoint::Head).P};
			double Best = 1e9;
			for (const FVector& Tip : Tips)
			{
				const FVector W = A.WorldPoint(Tip);
				for (const TPair<int32, FRect>& B : Boxes)
				{
					const double DX = FMath::Max3(B.Value.X - W.X, 0.0, W.X - B.Value.EndX());
					const double DY = FMath::Max3(B.Value.Y - W.Y, 0.0, W.Y - B.Value.EndY());
					Best = FMath::Min(Best, FMath::Sqrt(DX * DX + DY * DY));
				}
			}
			++Checked;
			// A wail reaches by sound, not by a limb.
			if (!M.HasTag(TEXT("sound")))
			{
				TestTrue(FString::Printf(TEXT("%s %s: a striking part is within 12 cm of the hitbox (%.2f m)"), *Id.ToString(), *M.Id.ToString(), Best), Best <= 0.12);
			}
		}
	}
	UE_LOG(LogSlop, Display, TEXT("Reach: %d strikes checked"), Checked);
	TestTrue(FString::Printf(TEXT("strikes checked (%d)"), Checked), Checked > 80);
	return true;
}

// The body should be where it can be hit. High attacks are meant to pass over a crouching fighter,
// so the crouching head has to sit about as low as the crouch hurtbox (within 15 cm), or a fist
// would be seen to go through it. Standing, the low stances (sumo, ninja, the Slopbat's hunch) sit
// under a hurtbox drawn for the original's upright fighters; those are logged, not changed, as the
// hurtboxes are rules.
SLOP_TEST(FSlopHurtboxesFitBody, "Anim.CrouchingHeadFitsTheHurtbox")
bool FSlopHurtboxesFitBody::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		for (int32 bCrouch = 0; bCrouch < 2; ++bCrouch)
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(Def, GetContent().Fighter(TEXT("gorm")));
			FAnimator A;
			A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
			FPadState Pads[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
			{
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			Pads[0].Dirs = bCrouch ? PadDown : 0;
			for (int32 T = 0; T < 40; ++T)
			{
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			const FFighter& F = Sim.Fighters[0];
			TArray<FRect> Hurt;
			F.Hurtboxes(Hurt);
			double Top = -1e9;
			for (const FRect& R : Hurt)
			{
				Top = FMath::Max(Top, R.EndY());
			}
			// The head joint sits at 0.885 of the height in the rig, the crown at the full height.
			const double Head = A.WorldPoint(A.Rig.J(EJoint::Head).P).Y + 0.115 * A.Rig.H;
			if (bCrouch)
			{
				TestTrue(FString::Printf(TEXT("%s crouching: head top %.2f m, hurtbox top %.2f m"), *Id.ToString(), Head, Top), FMath::Abs(Head - Top) <= 0.15);
			}
			else if (FMath::Abs(Head - Top) > 0.15)
			{
				UE_LOG(LogSlop, Display, TEXT("Hurt: %s standing: head top %.2f m, hurtbox top %.2f m"), *Id.ToString(), Head, Top);
			}
		}
	}
	return true;
}

// Two fighters walked into each other stop at their pushboxes; their bodies should then be close
// but not inside one another (measured between the chests and the hips, less a torso's depth).
SLOP_TEST(FSlopBodiesApart, "Anim.BodiesPushedTogetherDoNotOverlap")
bool FSlopBodiesApart::RunTest(const FString&)
{
	const TArray<FName>& Roster = FContent::Roster();
	double Tightest = 1e9;
	FString TightestPair;
	for (const FName& IdA : Roster)
	{
		for (const FName& IdB : Roster)
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(GetContent().Fighter(IdA), GetContent().Fighter(IdB));
			FAnimator A[2];
			A[0].Setup(Sim.Fighters[0], GetContent().CommonPoses);
			A[1].Setup(Sim.Fighters[1], GetContent().CommonPoses);
			FPadState Pads[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
			{
				Sim.Step(Pads);
				for (FAnimator& X : A) { X.SyncFromSim(); X.Update(1.0, 1.0 / 60.0); }
			}
			Pads[0].Dirs = PadRight;
			Pads[1].Dirs = PadLeft;
			for (int32 T = 0; T < 150; ++T)
			{
				Sim.Step(Pads);
				for (FAnimator& X : A) { X.SyncFromSim(); X.Update(1.0, 1.0 / 60.0); }
			}
			Pads[0].Dirs = Pads[1].Dirs = 0;
			for (int32 T = 0; T < 40; ++T)
			{
				Sim.Step(Pads);
				for (FAnimator& X : A) { X.SyncFromSim(); X.Update(1.0, 1.0 / 60.0); }
			}
			const auto X = [&A](int32 I, EJoint J) { return A[I].WorldPoint(A[I].Rig.J(J).P).X; };
			// Half the depth of a torso, front to back, scaled with height.
			const double Depth = 0.12 * (A[0].Rig.H + A[1].Rig.H) / 1.8;
			const double Chest = X(1, EJoint::Chest) - X(0, EJoint::Chest) - Depth;
			const double Hips = X(1, EJoint::Hips) - X(0, EJoint::Hips) - Depth;
			const double Gap = FMath::Abs(Sim.Fighters[1].Pos.X - Sim.Fighters[0].Pos.X);
			TestTrue(FString::Printf(TEXT("%s v %s: bodies apart (chests %+.2f m, hips %+.2f m)"), *IdA.ToString(), *IdB.ToString(), Chest, Hips), Chest > 0.0 && Hips > 0.0);
			if (Chest < Tightest)
			{
				Tightest = Chest;
				TightestPair = FString::Printf(TEXT("%s v %s, %.2f m apart"), *IdA.ToString(), *IdB.ToString(), Gap);
			}
		}
	}
	UE_LOG(LogSlop, Display, TEXT("Bodies: tightest %s, chests %+.2f m clear"), *TightestPair, Tightest);
	return true;
}

// Knocked down, a body lies on the floor and gets up off it: through the fall, the time down and
// the rise, the spine and head neither sink below the floor nor float above it.
SLOP_TEST(FSlopFloorHolds, "Anim.KnockdownsStayAboveTheFloor")
bool FSlopFloorHolds::RunTest(const FString&)
{
	const FFighterDef* Attacker = GetContent().Fighter(TEXT("bram"));
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		for (const TCHAR* Blow : {TEXT("sweep"), TEXT("uppercut")})
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(Attacker, GetContent().Fighter(Id));
			FAnimator A;
			A.Setup(Sim.Fighters[1], GetContent().CommonPoses);
			FPadState Pads[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
			{
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			Sim.Fighters[0].Pos = FVector2D(-0.5, 0.0);
			Sim.Fighters[1].Pos = FVector2D(0.5, 0.0);
			Sim.Fighters[0].StartMove(*Attacker->Move(Blow));
			Sim.Fighters[0].SetState(EState::Move);
			double Lowest = 1e9;
			FString Where;
			bool bDowned = false;
			for (int32 T = 0; T < 300; ++T)
			{
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
				const FFighter& F = Sim.Fighters[1];
				bDowned |= F.State == EState::Down;
				if (!bDowned && F.State != EState::Fall && F.State != EState::Juggle)
				{
					continue;
				}
				for (int32 J = 0; J <= (int32)EJoint::Head; ++J)
				{
					const double Y = A.WorldPoint(A.Rig.Xf[J].P).Y;
					if (Y < Lowest)
					{
						Lowest = Y;
						Where = FString::Printf(TEXT("%s in %s"), JointName((EJoint)J), StateName(F.State));
					}
				}
				if (bDowned && F.IsActionable())
				{
					break;
				}
			}
			TestTrue(FString::Printf(TEXT("%s: knocked down by a %s"), *Id.ToString(), Blow), bDowned);
			// Lying on the back, the spine and head rest a few centimetres up (about 9 cm for a
			// 1.8 m body); the original's pose left them floating 15 cm up.
			TestTrue(FString::Printf(TEXT("%s after a %s: lowest %s at %+.3f m, on the floor"), *Id.ToString(), Blow, *Where, Lowest),
				Lowest > -0.02 && Lowest < 0.075 * A.Rig.H);
		}
	}
	return true;
}

// Thrown, a body goes where the throw takes it: never through the floor, and not through the
// thrower (measured between the spine and head joints of the two bodies, in 3D).
SLOP_TEST(FSlopThrowPaths, "Anim.ThrownBodiesStayOutOfFloorAndThrower")
bool FSlopThrowPaths::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		for (const TPair<FName, TSharedRef<FMoveDef>>& Pair : Def->Moves)
		{
			const FMoveDef& M = *Pair.Value;
			if (!M.IsThrowKind() || !M.Throw.bHas || M.bAir)
			{
				continue;
			}
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.Setup(Def, GetContent().Fighter(TEXT("bram")));
			FAnimator A[2];
			A[0].Setup(Sim.Fighters[0], GetContent().CommonPoses);
			A[1].Setup(Sim.Fighters[1], GetContent().CommonPoses);
			FPadState Pads[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
			{
				Sim.Step(Pads);
				for (FAnimator& X : A) { X.SyncFromSim(); X.Update(1.0, 1.0 / 60.0); }
			}
			Sim.Fighters[0].Pos = FVector2D(-0.4, 0.0);
			Sim.Fighters[1].Pos = FVector2D(0.4, 0.0);
			Sim.Fighters[0].StartMove(M);
			Sim.Fighters[0].SetState(EState::Move);
			double Lowest = 1e9, Closest = 1e9;
			FString Nearest;
			bool bThrown = false;
			for (int32 T = 0; T < 200; ++T)
			{
				Sim.Step(Pads);
				for (FAnimator& X : A) { X.SyncFromSim(); X.Update(1.0, 1.0 / 60.0); }
				if (Sim.Fighters[1].State != EState::Thrown)
				{
					if (bThrown)
					{
						break;
					}
					continue;
				}
				bThrown = true;
				for (int32 J = 0; J <= (int32)EJoint::Head; ++J)
				{
					const FVector V = A[1].WorldPoint(A[1].Rig.Xf[J].P);
					Lowest = FMath::Min(Lowest, V.Y);
					for (int32 K = 0; K <= (int32)EJoint::Head; ++K)
					{
						const FVector W = A[0].WorldPoint(A[0].Rig.Xf[K].P);
						if ((V - W).Size() < Closest)
						{
							Closest = (V - W).Size();
							Nearest = FString::Printf(TEXT("victim %s to thrower %s at tick %d"), JointName((EJoint)J), JointName((EJoint)K), T);
						}
					}
				}
			}
			TestTrue(FString::Printf(TEXT("%s %s connects"), *Id.ToString(), *M.Id.ToString()), bThrown);
			TestTrue(FString::Printf(TEXT("%s %s: the victim's spine stays above the floor (%+.2f m)"), *Id.ToString(), *M.Id.ToString(), Lowest), Lowest > 0.0);
			// Throws are close work (a headlock, a hip throw), but two spines or heads closer than
			// 10 cm are inside each other: the chancery and the back throws were at 1-17 cm.
			TestTrue(FString::Printf(TEXT("%s %s: bodies stay apart (%.2f m, %s)"), *Id.ToString(), *M.Id.ToString(), Closest, *Nearest), Closest >= 0.1);
		}
	}
	return true;
}

// A guard holds the hands in front of the body, not inside it: in the held poses, each hand stays
// clear of the line from the hips to the neck.
SLOP_TEST(FSlopHandsOutsideBody, "Anim.HandsStayOutsideTheBody")
bool FSlopHandsOutsideBody::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Special());
	const TCHAR* Poses[] = {TEXT("stance"), TEXT("block"), TEXT("crouch"), TEXT("block_low"), TEXT("hit_head"), TEXT("hit_body"), TEXT("hit_leg"), TEXT("block_hit")};
	for (const FName& Id : Ids)
	{
		FFightSim Sim;
		Sim.bIntro = false;
		Sim.bTraining = true;
		Sim.Setup(GetContent().Fighter(Id), GetContent().Fighter(TEXT("gorm")));
		FAnimator A;
		A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
		FPadState Pads[2];
		for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 30); ++T)
		{
			Sim.Step(Pads);
			A.SyncFromSim();
			A.Update(1.0, 1.0 / 60.0);
		}
		for (const TCHAR* Pose : Poses)
		{
			if (!A.Lib.Has(Pose))
			{
				continue;
			}
			A.PoseOverride = Pose;
			for (int32 T = 0; T < 30; ++T)
			{
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			const FVector Hips = A.Rig.J(EJoint::Hips).P, Neck = A.Rig.J(EJoint::Neck).P;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector H = A.Rig.HandTip(Side);
				const FVector C = FMath::ClosestPointOnSegment(H, Hips, Neck);
				const double D = (H - C).Size();
				// About half a torso's depth (11 cm on a 1.8 m body): the 2.5.2 crouch had them at 5-8 cm.
				TestTrue(FString::Printf(TEXT("%s %s: %s hand %.3f m from the spine"), *Id.ToString(), Pose, Side == 0 ? TEXT("left") : TEXT("right"), D),
					D >= 0.11 * A.Rig.H / 1.8);
			}
		}
		A.PoseOverride = NAME_None;
	}
	return true;
}

// Walking and running play their step cycles by the distance covered, so a foot on the ground stays
// where it was put: while a foot is planted it should hardly move across the floor (the captured
// cycles; the sumo shuffle, the circle walk and the diver's plod are the fighters' own and logged).
// Held on against the distance limit, the walk stops with both feet down, not mid-stride.
SLOP_TEST(FSlopFeetPlanted, "Anim.PlantedFeetDoNotSlide")
bool FSlopFeetPlanted::RunTest(const FString&)
{
	struct FGait { const TCHAR* Name; int32 Dirs; int32 Held; bool bRun; };
	const FGait Gaits[] = {{TEXT("walk forward"), PadRight, 0, false}, {TEXT("walk back"), PadLeft, 0, false}, {TEXT("run"), PadRight, BtnRN, true}};
	for (const FName& Id : FContent::Roster())
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		for (const FGait& G : Gaits)
		{
			FFightSim Sim;
			Sim.bIntro = false;
			Sim.bTraining = true;
			Sim.bInfiniteRun = true;
			Sim.Setup(Def, GetContent().Fighter(TEXT("gorm")));
			FAnimator A;
			A.Setup(Sim.Fighters[0], GetContent().CommonPoses);
			FPadState Idle[2];
			for (int32 T = 0; T < 600 && (Sim.Phase != EPhase::Fight || T < 40); ++T)
			{
				Sim.Step(Idle);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
			}
			// Back: from close to the opponent out to the distance limit, then held there.
			Sim.Fighters[0].Pos.X = G.Dirs == PadLeft ? 5.4 : -5.0;
			Sim.Fighters[1].Pos.X = 6.5;
			const EJoint Feet[2] = {EJoint::FootL, EJoint::FootR};
			TArray<FVector> Track[2];
			TArray<double> BodyX;
			for (int32 T = 0; T < 260; ++T)
			{
				FPadState Pads[2];
				Pads[0].Dirs = G.Dirs;
				Pads[0].Held = G.Held;
				Sim.Step(Pads);
				A.SyncFromSim();
				A.Update(1.0, 1.0 / 60.0);
				for (int32 K = 0; K < 2; ++K)
				{
					Track[K].Add(A.WorldPoint(A.Rig.Xf[(int32)Feet[K]].P));
				}
				BodyX.Add(Sim.Fighters[0].Pos.X);
			}
			// The moving part: from frame 10 while the body still moves.
			int32 End = 10;
			while (End + 1 < BodyX.Num() && FMath::Abs(BodyX[End + 1] - BodyX[End]) > 1e-6)
			{
				++End;
			}
			const double Travel = FMath::Abs(BodyX[End] - BodyX[10]);
			double Slide = 0.0;
			for (int32 K = 0; K < 2; ++K)
			{
				double Low = 1e9;
				for (int32 T = 10; T <= End; ++T)
				{
					Low = FMath::Min(Low, Track[K][T].Y);
				}
				for (int32 T = 11; T <= End; ++T)
				{
					// Planted: at the bottom of the cycle on this frame and the last.
					if (Track[K][T].Y < Low + 0.012 && Track[K][T - 1].Y < Low + 0.012)
					{
						Slide += FMath::Abs(Track[K][T].X - Track[K][T - 1].X);
					}
				}
			}
			const double Share = Slide / FMath::Max(Travel, 0.01);
			UE_LOG(LogSlop, Display, TEXT("Feet: %s %s: travelled %.2f m, planted feet slid %.3f m (%.0f%%)"), *Id.ToString(), G.Name, Travel, Slide, 100.0 * Share);
			const bool bCaptured = G.bRun ? Def->Motion.bRunCycle && Def->Cycles.Contains(TEXT("run")) : Def->Motion.bWalkCycle && Def->Cycles.Contains(TEXT("walk_f"));
			if (bCaptured)
			{
				TestTrue(FString::Printf(TEXT("%s %s: planted feet slide %.0f%% of the way"), *Id.ToString(), G.Name, 100.0 * Share), Share < 0.03);
			}
			if (G.Dirs == PadLeft)
			{
				// Held against the limit for the rest of the run: both feet on the floor.
				TestTrue(FString::Printf(TEXT("%s held back at the limit: walking stopped"), *Id.ToString()), End < BodyX.Num() - 40);
				for (int32 K = 0; K < 2; ++K)
				{
					TestTrue(FString::Printf(TEXT("%s held back at the limit: foot %d down (%.3f m)"), *Id.ToString(), K, Track[K].Last().Y), Track[K].Last().Y < Def->Scale() * 0.08 + 0.025);
				}
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
