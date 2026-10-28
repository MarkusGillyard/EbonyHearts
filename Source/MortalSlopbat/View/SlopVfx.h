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
	// Set pieces (finishers): positions in the original's metres.
	void BurstColor(const FVector& P, const FLinearColor& C, int32 N, double Speed, double Life);
	void FlashAt(const FVector& P, const FLinearColor& C, float Intensity) { Flash(P, C, Intensity); }
	void DustAt(const FVector& P);
	void BloodAt(const FVector& P, double Dir, double Amount);
	bool bReduced = false;
	bool bFlashes = true;
	FString Gore = TEXT("full");

private:
	enum class EKind : uint8 { Spark, Ember, Blood, Dust, Slop, Ring, Disc, Pale, Count };
	struct FParticle { FVector P; FVector V; double Life; double Age; double Size; double Grow; double Drag; double Gravity; FRotator R; };
	void Spawn(EKind K, const FVector& P, const FVector& V, double Life, double Size, double Grow = 0, double Drag = 2.0, double Gravity = 9.8);
	void Burst(EKind K, const FVector& P, int32 N, double Speed, double Life, double Size, const FVector& Bias = FVector::ZeroVector);
	void Flash(const FVector& P, const FLinearColor& C, float Intensity);
	void SyncProjectiles(double Delta);
	UStaticMeshComponent* ProjPart(const TCHAR* Shape, class UMaterialInterface* Material);
	class UMaterialInterface* SolidMaterial(const FLinearColor& Color, double Roughness, double Metallic, bool bTar);
	TWeakObjectPtr<ASlopMatch> Match;
	TArray<FParticle> Parts[(int32)EKind::Count];
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Ism;
	// A projectile as drawn: a glowing core for fire and energy, or solid parts for a thrown
	// weapon (blade, harpoon, anchor) or tar, a chain back to the thrower when it is tethered,
	// and a trail.
	struct FProjView
	{
		int32 Serial = 0;
		FName Kind;
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UStaticMeshComponent> Part;
		TObjectPtr<UStaticMeshComponent> Chain;
		TObjectPtr<UPointLightComponent> Light;
		double TrailT = 0;
		double Spin = 0;
	};
	TArray<FProjView> Projs;
	struct FFlashLight { TObjectPtr<UPointLightComponent> L; double Life; double Age; float Peak; };
	TArray<FFlashLight> Flashes;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	// The move frame each fighter last sent out a sound ring on (moves tagged "sound").
	int32 SoundMF[2] = {-1, -1};
	void SoundRings();
	// The Storm Shroud's telegraph: a green light swelling over the charge with slime bursting
	// off the body, then thunder, a flash and lightning while it is live.
	void StormStep(double Delta);
	// A chain whipped out to a move's reach (moves with a "whip" range: Nix's Undertow), as the
	// original drew it: from the hand to the far end of the hitbox, paid out over three frames.
	void WhipStep();
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WhipChain[2];
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WhipTip[2];
	UPROPERTY()
	TObjectPtr<UPointLightComponent> StormLight;
	int32 StormFired = -1;
	double StormTime = 0;
};
