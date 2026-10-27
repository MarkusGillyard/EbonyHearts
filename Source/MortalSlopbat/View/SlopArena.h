#pragma once

#include "GameFramework/Actor.h"
#include "SlopArena.generated.h"

class UInstancedStaticMeshComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// A stage: the set built in Blender (Tools/blender/stages) and imported as static meshes under
// /Game/Slop/Stages/<id>, placed and lit from Content/Data/Stages/<id>.json: its lights and their
// flicker, fog, ambient and sky, swinging pieces, glow pulses and weather.
UCLASS()
class ASlopArena : public AActor
{
	GENERATED_BODY()
public:
	ASlopArena();
	// False when the stage's data or meshes are missing (the match still runs on a bare floor).
	bool Build(FName Stage, bool bHighGraphics);
	void Step(double Delta, const FVector& CameraPos);
	FString Music = TEXT("music_yards");
	FString Footstep = TEXT("kenney:footstep_concrete");
	FName StageId;
	// Off (the comfort setting) turns the Roost's lightning strobe into a slow swell.
	bool bStrobe = true;
	// The exposure the stage's lights were set for.
	float ExposureEV100 = 0.f;

private:
	struct FFlicker
	{
		TObjectPtr<ULightComponent> Light;
		float Base = 0.f;
		float Speed = 0.f;
		float Depth = 0.f;
		float Phase = 0.f;
	};
	struct FSway
	{
		TObjectPtr<USceneComponent> C;
		FVector Pivot;
		FVector Axis;
		float Amp = 0.f;
		float Speed = 0.f;
		float Phase = 0.f;
	};
	struct FGlowPulse
	{
		TObjectPtr<UMaterialInstanceDynamic> M;
		float Base = 1.f;
		float Speed = 1.f;
		float Depth = 0.f;
		float Phase = 0.f;
	};
	// One kind of weather or ambience: a pool of camera-facing (or floor-lying) quads.
	struct FPart
	{
		FVector P;
		FVector V;
		float Age = 0.f;
		float Life = 1.f;
		float Size = 0.1f;
		float Phase = 0.f;
	};
	enum class EFx : uint8 { Rain, Rise, Drift, Twinkle, Mist, Drips };
	struct FFxLayer
	{
		EFx Kind = EFx::Drift;
		TObjectPtr<UInstancedStaticMeshComponent> Ism;
		TArray<FPart> Parts;
		FVector Center;
		FVector Extent;
		FVector Vel;
		float Rate = 0.f;
		float Size = 0.1f;
		float Grow = 1.f;
		float Life = 4.f;
		float Spawn = 0.f;
		float Alpha = 1.f;
	};
	void BareFloor();
	void Lights(const TArray<TSharedPtr<class FJsonValue>>& List);
	void Atmosphere(const TSharedPtr<class FJsonObject>& D);
	void Effects(const TArray<TSharedPtr<class FJsonValue>>& List);
	FFxLayer& AddLayer(EFx Kind, const TCHAR* Material, const FLinearColor& Color, float Intensity, int32 Count);
	void Respawn(FFxLayer& L, FPart& P, bool bAnywhere);
	void StepLightning(double Dt);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	// The imported meshes sit in Unreal's FBX frame (stage along X, depth toward -Y); this turns
	// them a quarter round into the game's (stage along Y, depth along +X).
	UPROPERTY()
	TObjectPtr<USceneComponent> SetRoot;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	TMap<FString, TObjectPtr<UStaticMeshComponent>> Pieces;
	bool bHigh = true;
	double Time = 0;
	FRandomStream Rng;
	TArray<FFlicker> Flickers;
	TArray<FSway> Sways;
	TArray<FGlowPulse> Pulses;
	TArray<FFxLayer> Layers;
	// The Roost's storm: a light and the bolt mesh, flashing every few seconds.
	TObjectPtr<ULightComponent> Storm;
	TObjectPtr<UStaticMeshComponent> Bolt;
	float StormBase = 0.f;
	double NextFlash = 4.0;
	double FlashT = -1.0;
};
