#pragma once

#include "GameFramework/Actor.h"
#include "Combat/SlopCombatTypes.h"
#include "SlopVfx.generated.h"

class ASlopMatch;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

// Short, punchy effects: impact sparks and rings and a brief light at the point of contact,
// blood that follows the gore setting, dust where bodies land, and the projectiles themselves.
// Particles are instances of a few meshes, simulated here; nothing here touches the simulation.
UCLASS()
class ASlopVfx : public AActor
{
	GENERATED_BODY()
public:
	ASlopVfx();
	void Setup(ASlopMatch* InMatch);
	void OnEvent(const Slop::FSimEvent& E);
	void Step(double Delta);
	bool bReduced = false;
	bool bFlashes = true;
	FString Gore = TEXT("full");

private:
	enum class EKind : uint8 { Spark, Ember, Blood, Dust, Slop, Ring, Count };
	struct FParticle { FVector P; FVector V; double Life; double Age; double Size; double Grow; double Drag; double Gravity; FRotator R; };
	void Spawn(EKind K, const FVector& P, const FVector& V, double Life, double Size, double Grow = 0, double Drag = 2.0, double Gravity = 9.8);
	void Burst(EKind K, const FVector& P, int32 N, double Speed, double Life, double Size, const FVector& Bias = FVector::ZeroVector);
	void Flash(const FVector& P, const FLinearColor& C, float Intensity);
	void SyncProjectiles();
	TWeakObjectPtr<ASlopMatch> Match;
	TArray<FParticle> Parts[(int32)EKind::Count];
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	struct FProjView { int32 Serial; TObjectPtr<UStaticMeshComponent> Mesh; TObjectPtr<UPointLightComponent> Light; FName Kind; };
	TArray<FProjView> Projs;
	struct FFlashLight { TObjectPtr<UPointLightComponent> L; double Life; double Age; float Peak; };
	TArray<FFlashLight> Flashes;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
};
