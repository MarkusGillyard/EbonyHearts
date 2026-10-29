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
	// The fighter changed shape (the Grandmaster): its own body takes the form's poses; a borrowed
	// shape puts on the other fighter's body. False when the mesh is missing.
	bool Rebind(const Slop::FFighter& Fighter);
	// The definition and body the actor is set up for.
	const Slop::FFighterDef* BoundDef = nullptr;
	FName BoundBody;
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
	// A fighter in a match works up a sweat over the round and as it takes damage (the skin
	// material's Sweat); the menu figures stay dry.
	bool bSweats = false;
	// The marks a fight leaves on the skin (the skin material's Damage): a blow bruises the region
	// it lands on (0 head, 1 torso, 2 legs; see FAnimator::RegionAt), fully once a third of the
	// fighter's health has been taken there. They last the match, except where health refills
	// (Training, the tutorial, trials): there they heal with it. `Blood` (0 to 1, from the gore
	// setting) is how much of it bleeds.
	void TakeBlow(int32 Region, int32 Damage);
	void SetBlood(float Amount);

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
	double Exertion = 0.0;
	float LastSweat = -1.f;
	double Marks[3] = {0.0, 0.0, 0.0};
	int32 LastHealth = -1;
	float Blood = 1.f;
	void ShowMarks();
	// Loose fabric's swing (the cloth slots' Sway): a spring on the hips' motion, in metres.
	void StepSway(double Delta);
	FVector SwayX = FVector::ZeroVector;
	FVector SwayV = FVector::ZeroVector;
	FVector SwayLastP = FVector::ZeroVector;
	FVector SwayLastV = FVector::ZeroVector;
	bool bSwayValid = false;
};
