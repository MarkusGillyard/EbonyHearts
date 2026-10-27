#include "View/SlopFighterActor.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/SlopLog.h"

using namespace Slop;

ASlopFighterActor::ASlopFighterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCastShadow(true);
	Body->bCastDynamicShadow = true;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Channel 1 too: each stage's camera fill lights only the fighters.
	Body->LightingChannels.bChannel1 = true;
	// Bones move every frame from the rig; bounds must follow them.
	Body->bComponentUseFixedSkelBounds = false;
}

bool ASlopFighterActor::Setup(const FFighter& Fighter, const FString& MeshPath)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogSlop, Error, TEXT("Fighter mesh missing: %s"), *MeshPath);
		return false;
	}
	FString Error;
	if (!MeshRig.Init(Mesh->GetRefSkeleton(), Fighter.Def->Height, Error))
	{
		UE_LOG(LogSlop, Error, TEXT("Fighter mesh %s: %s"), *MeshPath, *Error);
		return false;
	}
	Body->SetSkinnedAssetAndUpdate(Mesh);
	Anim.Setup(Fighter, GetContent().CommonPoses);
	MeshRig.ConfigureRig(Anim.Rig);
	Anim.SyncFromSim(true);
	Anim.Update(1.0, 0.0);
	Mats.Reset();
	IsSkin.Reset();
	for (int32 I = 0; I < Body->GetNumMaterials(); ++I)
	{
		UMaterialInterface* M = Body->GetMaterial(I);
		UMaterialInstanceDynamic* D = Body->CreateDynamicMaterialInstance(I, M);
		Mats.Add(D);
		const FString Slot = Mesh->GetMaterials().IsValidIndex(I) ? Mesh->GetMaterials()[I].MaterialSlotName.ToString().ToLower() : FString();
		IsSkin.Add(Slot.Contains(TEXT("skin")) || Slot.Contains(TEXT("body")) || Slot.Contains(TEXT("eye")) || Slot.Contains(TEXT("brow")) || Slot.Contains(TEXT("lash")));
	}
	const FString Glow = Json::Str(Fighter.Def->Look, TEXT("glow"), TEXT("#ffffff"));
	SetAccent(FLinearColor(FColor::FromHex(Glow)), (float)Json::Num(Fighter.Def->Look, TEXT("accent"), 0.24));
	UpdateVisual(1.0, 0.0);
	return true;
}

void ASlopFighterActor::UpdateVisual(double Frac, double Delta)
{
	if (!MeshRig.IsValid())
	{
		return;
	}
	Anim.Update(Frac, Delta);
	MeshRig.Pose(Anim.Rig, Locals);
	if (Body->BoneSpaceTransforms.Num() == Locals.Num())
	{
		Body->BoneSpaceTransforms = Locals;
		Body->MarkRefreshTransformDirty();
	}
	Body->SetWorldTransform(MeshRig.ComponentTransform(Anim.Root, Anim.WorldScale));
	const float Flash = bFlashes ? (float)Anim.FlashAmount : 0.f;
	if (Flash != LastFlash)
	{
		SetScalar(TEXT("Flash"), Flash * 6.f);
		LastFlash = Flash;
	}
	SetScalar(TEXT("Glow"), (float)Anim.Glow * 3.f);
}

void ASlopFighterActor::SetScalar(FName Param, float Value)
{
	for (UMaterialInstanceDynamic* M : Mats)
	{
		if (M)
		{
			M->SetScalarParameterValue(Param, Value);
		}
	}
}

void ASlopFighterActor::SetVector(FName Param, const FLinearColor& Value)
{
	for (UMaterialInstanceDynamic* M : Mats)
	{
		if (M)
		{
			M->SetVectorParameterValue(Param, Value);
		}
	}
}

void ASlopFighterActor::SetTint(const FLinearColor& Color, float Amount, float Gloss)
{
	SetVector(TEXT("TintColor"), Color);
	SetScalar(TEXT("TintAmount"), Amount);
	SetScalar(TEXT("TintGloss"), Gloss);
}

void ASlopFighterActor::SetDissolve(float Amount, const FLinearColor& Color)
{
	SetVector(TEXT("DissolveColor"), Color);
	SetScalar(TEXT("Dissolve"), Amount);
}

void ASlopFighterActor::SetCostume(const FLinearColor& ClothTint)
{
	for (int32 I = 0; I < Mats.Num(); ++I)
	{
		if (Mats[I] && !IsSkin[I])
		{
			Mats[I]->SetVectorParameterValue(TEXT("Tint"), ClothTint);
		}
	}
}

void ASlopFighterActor::SetAccent(const FLinearColor& Color, float Strength)
{
	SetVector(TEXT("AccentColor"), Color);
	SetScalar(TEXT("AccentStrength"), Strength);
}

void ASlopFighterActor::SetHidden(bool bHide)
{
	Body->SetVisibility(!bHide, true);
}
