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
		// Alternate colours dye the costume, never skin, faces or hair.
		IsSkin.Add(Slot.Contains(TEXT("skin")) || Slot.Contains(TEXT("body")) || Slot.Contains(TEXT("eye")) || Slot.Contains(TEXT("brow")) || Slot.Contains(TEXT("lash"))
			|| Slot.Contains(TEXT("face")) || Slot.Contains(TEXT("hair")));
	}
	const FString Glow = Json::Str(Fighter.Def->Look, TEXT("glow"), TEXT("#ffffff"));
	// The original's rim strengths were set for its brighter exposure; here they read at half.
	SetAccent(FLinearColor(FColor::FromHex(Glow)), 0.5f * (float)Json::Num(Fighter.Def->Look, TEXT("accent"), 0.24));
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
		// A blow brightens the body for a frame or two; strong enough to whiten it reads as a glitch.
		SetScalar(TEXT("Flash"), Flash * 1.8f);
		LastFlash = Flash;
	}
	SetScalar(TEXT("Glow"), (float)Anim.Glow * 3.f);
	if (bSweats && Anim.Fighter)
	{
		// About two minutes of fighting, or the health taken, brings it on fully.
		Exertion += Delta / 120.0;
		const double Hurt = double(Anim.Fighter->DamageTaken) / FMath::Max(1, Anim.Fighter->Def->Health);
		const float Sweat = (float)FMath::Clamp(Exertion * 0.6 + Hurt * 0.6, 0.0, 1.0);
		if (FMath::Abs(Sweat - LastSweat) > 0.01f)
		{
			SetScalar(TEXT("Sweat"), Sweat);
			LastSweat = Sweat;
		}
	}
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
