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
