#pragma once

#include "GameFramework/Actor.h"
#include "SlopArena.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;

// A stage built in code from a few meshes, materials and lights: the walkable floor, the scenery
// behind and in front of the fighting plane, its lighting and its moving details.
UCLASS()
class ASlopArena : public AActor
{
	GENERATED_BODY()
public:
	ASlopArena();
	void Build(FName Stage, bool bHighGraphics);
	void Step(double Delta);
	FString Music = TEXT("music_yards");
	FString Footstep = TEXT("step");
	FName StageId;

	// Building blocks (positions in the original's metres: x along the stage, y up, z toward the camera).
	UStaticMeshComponent* Mesh(UStaticMesh* M, const FVector& Pos, const FVector& SizeM, const FRotator& Rot, UMaterialInterface* Mat);
	UStaticMeshComponent* Cube(const FVector& Pos, const FVector& SizeM, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* Cylinder(const FVector& Pos, double Radius, double Height, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* Sphere(const FVector& Pos, double Radius, UMaterialInterface* Mat);
	UPointLightComponent* Light(const FVector& Pos, const FLinearColor& Color, float Intensity, float Radius, bool bShadows = false);
	UMaterialInstanceDynamic* Surface(const TCHAR* TextureSet, const FLinearColor& Tint, float Tiling, float Roughness = 1.f, float Metallic = 0.f);
	UMaterialInstanceDynamic* Glow(const FLinearColor& Color, float Intensity, bool bAdditive = false);

private:
	void BuildYards();
	void BuildChapel();
	void BuildMarket();
	void BuildRoost();
	void Common(const FLinearColor& FogColor, float FogDensity, const FLinearColor& Ambient, float AmbientIntensity);
	USceneComponent* Root;
	bool bHigh = true;
	double Time = 0;
	struct FFlicker { TObjectPtr<UPointLightComponent> Light; float Base; float Speed; float Depth; float Phase; };
	TArray<FFlicker> Flickers;
	struct FBob { TObjectPtr<USceneComponent> C; FVector Base; float Amp; float Speed; float Phase; };
	TArray<FBob> Bobs;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
};
