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
			const bool bOurs = Mi && Mi->GetName().StartsWith(TEXT("MI_")) && Mi->GetBaseMaterial() && Mi->GetBaseMaterial()->GetName() == TEXT("M_Character");
			TestTrue(*FString::Printf(TEXT("%s slot %s has the character material"), *Id.ToString(), *M.MaterialSlotName.ToString()), bOurs);
		}
		const FString P = FString::Printf(TEXT("/Game/Slop/Portraits/T_portrait_%s.T_portrait_%s"), *Id.ToString(), *Id.ToString());
		TestNotNull(*FString::Printf(TEXT("%s portrait"), *Id.ToString()), LoadObject<UTexture2D>(nullptr, *P));
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
