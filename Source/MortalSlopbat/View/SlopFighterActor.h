#pragma once

#include "GameFramework/Actor.h"
#include "Anim/SlopAnimator.h"
#include "Anim/SlopMeshRig.h"
#include "SlopFighterActor.generated.h"

class UPoseableMeshComponent;
class UMaterialInstanceDynamic;

// One fighter on screen: a skinned body posed every frame from the animator's solved rig. It
// only reads the simulation.
UCLASS()
class ASlopFighterActor : public AActor
{
	GENERATED_BODY()
public:
	ASlopFighterActor();

	// Binds to a simulated fighter; `MeshPath` is the skeletal mesh to wear. Returns false when the
	// mesh is missing or does not have the game skeleton.
	bool Setup(const Slop::FFighter& Fighter, const FString& MeshPath);
	void SyncFromSim(bool bTeleport = false) { Anim.SyncFromSim(bTeleport); }
	// Poses the body for this rendered frame.
	void UpdateVisual(double Frac, double Delta);
	// Material effects.
	void SetTint(const FLinearColor& Color, float Amount, float Gloss = 0.f);
	void SetDissolve(float Amount, const FLinearColor& Color = FLinearColor(1.f, 0.45f, 0.1f));
	void SetCostume(const FLinearColor& ClothTint);
	void SetAccent(const FLinearColor& Color, float Strength);
	void SetHidden(bool bHide);
	bool bFlashes = true;

	Slop::Anim::FAnimator Anim;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPoseableMeshComponent> Body;

private:
	void SetScalar(FName Param, float Value);
	void SetVector(FName Param, const FLinearColor& Value);
	Slop::Anim::FMeshRig MeshRig;
	TArray<FTransform> Locals;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	TArray<bool> IsSkin;
	float LastFlash = -1.f;
};
