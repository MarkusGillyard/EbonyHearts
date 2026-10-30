#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Anim/SlopMeshRig.h"
#include "Combat/SlopData.h"
#include "Core/SlopGameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace SlopContentTestSuite
{
using namespace Slop;

#define SLOP_TEST(Name, Path) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Name, "Slop." Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Every fighter the data names has a body with the game skeleton, a character material on every
// slot (not the importer's own), and a select-screen portrait.
SLOP_TEST(FSlopEveryFighterHasABody, "Content.EveryFighterHasABodyAndPortrait")
bool FSlopEveryFighterHasABody::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Secret());
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		if (!TestNotNull(*FString::Printf(TEXT("%s data"), *Id.ToString()), Def))
		{
			continue;
		}
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *USlopGameInstance::MeshFor(Id));
		if (!TestNotNull(*FString::Printf(TEXT("%s mesh"), *Id.ToString()), Mesh))
		{
			continue;
		}
		Anim::FMeshRig Rig;
		FString Error;
		TestTrue(*FString::Printf(TEXT("%s skeleton maps onto the rig: %s"), *Id.ToString(), *Error), Rig.Init(Mesh->GetRefSkeleton(), Def->Height, Error));
		for (const FSkeletalMaterial& M : Mesh->GetMaterials())
		{
			UMaterialInterface* Mi = M.MaterialInterface;
			const bool bOurs = Mi && Mi->GetName().StartsWith(TEXT("MI_")) && Mi->GetBaseMaterial() && Mi->GetBaseMaterial()->GetName().StartsWith(TEXT("M_Character"));
			TestTrue(*FString::Printf(TEXT("%s slot %s has the character material"), *Id.ToString(), *M.MaterialSlotName.ToString()), bOurs);
		}
		const FString P = FString::Printf(TEXT("/Game/Slop/Portraits/T_portrait_%s.T_portrait_%s"), *Id.ToString(), *Id.ToString());
		TestNotNull(*FString::Printf(TEXT("%s portrait"), *Id.ToString()), LoadObject<UTexture2D>(nullptr, *P));
	}
	return true;
}

// A foot off the floor follows the shin. It may point, a turn about the ankle, but it must not turn
// about the shin itself: on a skinned body that wrings the ankle to a thin point (every raised foot
// did, on every fighter, until the shin's bind frame was taken out of it).
SLOP_TEST(FSlopRaisedFeetDoNotTwist, "Content.RaisedFeetDoNotTwistAboutTheShin")
bool FSlopRaisedFeetDoNotTwist::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Secret());
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		USkeletalMesh* Mesh = Def ? LoadObject<USkeletalMesh>(nullptr, *USlopGameInstance::MeshFor(Id)) : nullptr;
		Anim::FMeshRig MeshRig;
		FString Error;
		if (!TestTrue(Id.ToString() + TEXT(" rig"), Mesh && MeshRig.Init(Mesh->GetRefSkeleton(), Def->Height, Error)))
		{
			continue;
		}
		Anim::FRig Rig;
		MeshRig.ConfigureRig(Rig);
		Anim::FPoseLibrary Lib;
		Lib.Build(*Def, GetContent().CommonPoses);
		FQuat Extra[(int32)Anim::EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		for (const TCHAR* Name : {TEXT("mc_hk_09"), TEXT("mc_lk_07"), TEXT("mc_roundhouse_13"), TEXT("rh_hit"), TEXT("c_hk_hit")})
		{
			const Anim::FPose& P = Lib.Get(FName(Name));
			const FVector Feet[2] = {P.P(Anim::EPos::FootL) + FVector(0, Rig.AnkleH, 0), P.P(Anim::EPos::FootR) + FVector(0, Rig.AnkleH, 0)};
			const double Yaws[2] = {FMath::DegreesToRadians(P.V(Anim::EVal::YawL)), FMath::DegreesToRadians(P.V(Anim::EVal::YawR))};
			Rig.Apply(P, Feet, Yaws, Extra);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (P.V(Side == 0 ? Anim::EVal::FreeL : Anim::EVal::FreeR) < 1.0)
				{
					continue;
				}
				const FQuat Shin = Rig.J(Side == 0 ? Anim::EJoint::ShinL : Anim::EJoint::ShinR).Q * Rig.ShinBind[Side].Inverse();
				const FQuat Foot = Rig.J(Side == 0 ? Anim::EJoint::FootL : Anim::EJoint::FootR).Q;
				const FQuat Rel = (Shin.Inverse() * Foot).GetNormalized();
				const FVector Along = Rig.ShinBind[Side].RotateVector(FVector(0, 1, 0));
				const double Twist = 2.0 * FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(FVector::DotProduct(FVector(Rel.X, Rel.Y, Rel.Z), Along)), FMath::Abs(Rel.W)));
				TestTrue(FString::Printf(TEXT("%s %s side %d: turned %.1f degrees about the shin"), *Id.ToString(), Name, Side, Twist), Twist < 3.0);
			}
		}
	}
	return true;
}

// The half joints keep a bent limb's width (Tools/blender/fighter_joints.py): half way through a bend,
// half way through a twist, with no jump as a twist passes 180 degrees.
SLOP_TEST(FSlopHalfTurn, "Anim.HalfTurnStaysContinuous")
bool FSlopHalfTurn::RunTest(const FString&)
{
	const FVector Along(0, 0, 1);
	auto Degrees = [](const FQuat& A, const FQuat& B) { return FMath::RadiansToDegrees(A.AngularDistance(B)); };
	const FQuat Bend(FVector(1, 0, 0), FMath::DegreesToRadians(120.0));
	TestTrue("half of a bend", Degrees(Anim::FMeshRig::HalfTurn(Bend, Along), FQuat(FVector(1, 0, 0), FMath::DegreesToRadians(60.0))) < 0.01);
	const FQuat Twist(Along, FMath::DegreesToRadians(60.0));
	TestTrue("half of a twist", Degrees(Anim::FMeshRig::HalfTurn(Twist, Along), FQuat(Along, FMath::DegreesToRadians(30.0))) < 0.01);
	FQuat Last = Anim::FMeshRig::HalfTurn(FQuat(Along, FMath::DegreesToRadians(-200.0)) * FQuat::Identity, Along);
	for (double A = -199.0; A <= 200.0; A += 1.0)
	{
		const FQuat Now = Anim::FMeshRig::HalfTurn(FQuat(Along, FMath::DegreesToRadians(A)), Along);
		TestTrue(FString::Printf(TEXT("a twist of %.0f degrees moves its half by %.1f"), A, Degrees(Now, Last)), Degrees(Now, Last) < 3.0);
		Last = Now;
	}
	return true;
}

// Each fighter has its twelve half joints, sitting on their joints between the bones either side, and
// no pose bends a joint so far that the short way round could flip from one frame to the next.
SLOP_TEST(FSlopHalfJoints, "Content.HalfJointsSitBetweenTheirBones")
bool FSlopHalfJoints::RunTest(const FString&)
{
	TArray<FName> Ids = FContent::Roster();
	Ids.Append(FContent::Secret());
	Ids.Append(FContent::Special());
	for (const FName& Id : Ids)
	{
		const FFighterDef* Def = GetContent().Fighter(Id);
		USkeletalMesh* Mesh = Def ? LoadObject<USkeletalMesh>(nullptr, *USlopGameInstance::MeshFor(Id)) : nullptr;
		Anim::FMeshRig MeshRig;
		FString Error;
		if (!TestTrue(Id.ToString() + TEXT(" rig"), Mesh && MeshRig.Init(Mesh->GetRefSkeleton(), Def->Height, Error)))
		{
			continue;
		}
		if (!TestEqual(Id.ToString() + TEXT(" half joints"), MeshRig.NumHalfJoints(), 12))
		{
			continue;
		}
		Anim::FRig Rig;
		MeshRig.ConfigureRig(Rig);
		Anim::FPoseLibrary Lib;
		Lib.Build(*Def, GetContent().CommonPoses);
		FQuat Extra[(int32)Anim::EJoint::Count];
		for (FQuat& Q : Extra)
		{
			Q = FQuat::Identity;
		}
		auto Solve = [&](const Anim::FPose& P)
		{
			const FVector Feet[2] = {P.P(Anim::EPos::FootL) + FVector(0, Rig.AnkleH, 0), P.P(Anim::EPos::FootR) + FVector(0, Rig.AnkleH, 0)};
			const double Yaws[2] = {FMath::DegreesToRadians(P.V(Anim::EVal::YawL)), FMath::DegreesToRadians(P.V(Anim::EVal::YawR))};
			Rig.Apply(P, Feet, Yaws, Extra);
		};
		TArray<FName> Names;
		Lib.Names(Names);
		double MaxSwing[12] = {}, MaxTwist[12] = {};
		FName SwingPose[12], TwistPose[12];
		for (const FName& Name : Names)
		{
			Solve(Lib.Get(Name));
			for (int32 I = 0; I < 12; ++I)
			{
				double Swing, Twist;
				MeshRig.HalfJointSpread(Rig, I, Swing, Twist);
				if (Swing > MaxSwing[I])
				{
					MaxSwing[I] = Swing;
					SwingPose[I] = Name;
				}
				if (Twist > MaxTwist[I])
				{
					MaxTwist[I] = Twist;
					TwistPose[I] = Name;
				}
			}
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 N = Ref.GetNum();
		Solve(Lib.Get(TEXT("mc_hk_09")));
		TArray<FTransform> Locals;
		MeshRig.Pose(Rig, Locals);
		TArray<FTransform> Posed;
		Posed.SetNum(N);
		for (int32 B = 0; B < N; ++B)
		{
			const int32 Par = Ref.GetParentIndex(B);
			Posed[B] = Par >= 0 ? Locals[B] * Posed[Par] : Locals[B];
		}
		for (int32 I = 0; I < 12; ++I)
		{
			int32 Bone, Upper, Lower;
			MeshRig.HalfJointBones(I, Bone, Upper, Lower);
			const FString Name = FString::Printf(TEXT("%s %s"), *Id.ToString(), *Ref.GetBoneName(Bone).ToString());
			TestTrue(Name + TEXT(" sits on its joint"), FVector::Dist(Posed[Bone].GetLocation(), Posed[Lower].GetLocation()) < 0.01);
			AddInfo(FString::Printf(TEXT("%s widest: bend %.0f (%s), twist %.0f (%s)"), *Name, MaxSwing[I], *SwingPose[I].ToString(), MaxTwist[I], *TwistPose[I].ToString()));
			TestTrue(FString::Printf(TEXT("%s bends %.0f degrees in %s"), *Name, MaxSwing[I], *SwingPose[I].ToString()), MaxSwing[I] < 170.0);
		}
	}
	return true;
}

// Every piece a stage's data places exists, and none of its slots is empty or left with an
// importer placeholder; every surface texture a stage material uses is present.
SLOP_TEST(FSlopEveryStagePieceIsMade, "Content.EveryStagePieceHasItsMaterials")
bool FSlopEveryStagePieceIsMade::RunTest(const FString&)
{
	TArray<FName> Stages = FGameFlow::Arenas();
	Stages.AddUnique(FGameFlow::BossArena());
	for (const FName& St : Stages)
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Stages") / St.ToString() + TEXT(".json");
		FString Text;
		TSharedPtr<FJsonObject> D;
		if (!TestTrue(*FString::Printf(TEXT("%s data"), *St.ToString()), FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), D) && D.IsValid()))
		{
			continue;
		}
		int32 Pieces = 0;
		for (const TSharedPtr<FJsonValue>& V : D->GetArrayField(TEXT("placements")))
		{
			const FString Name = V->AsObject()->GetStringField(TEXT("object"));
			if (Name.StartsWith(TEXT("ORIGIN_MARKER")))
			{
				continue;
			}
			const FString MeshPath = FString::Printf(TEXT("/Game/Slop/Stages/%s/%s.%s"), *St.ToString(), *Name, *Name);
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *MeshPath);
			if (!TestNotNull(*FString::Printf(TEXT("%s mesh"), *Name), M))
			{
				continue;
			}
			++Pieces;
			for (const FStaticMaterial& Sm : M->GetStaticMaterials())
			{
				const UMaterialInterface* Mi = Sm.MaterialInterface;
				TestTrue(*FString::Printf(TEXT("%s slot %s has a stage material"), *Name, *Sm.MaterialSlotName.ToString()),
					Mi && (Mi->GetName().StartsWith(TEXT("MI_")) || Mi->GetPathName().StartsWith(TEXT("/Game/Slop/Props/"))));
			}
		}
		TestTrue(*FString::Printf(TEXT("%s has pieces"), *St.ToString()), Pieces > 10);
		for (const auto& S : D->GetObjectField(TEXT("surfaces"))->Values)
		{
			const FString Set = S.Value->AsObject()->GetStringField(TEXT("set"));
			const FString Tex = FString::Printf(TEXT("/Game/Slop/Surfaces/%s/T_%s_BaseColor.T_%s_BaseColor"), *Set, *Set, *Set);
			TestNotNull(*FString::Printf(TEXT("%s surface %s"), *St.ToString(), *Set), LoadObject<UTexture2D>(nullptr, *Tex));
		}
	}
	return true;
}

#undef SLOP_TEST
}

#endif
