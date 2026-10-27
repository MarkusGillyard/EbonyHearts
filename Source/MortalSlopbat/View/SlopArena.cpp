#include "View/SlopArena.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Anim/SlopMeshRig.h"
#include "Core/SlopLog.h"

using Slop::Anim::WorldToUE;

static UStaticMesh* Shape(const TCHAR* Name)
{
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
}

ASlopArena::ASlopArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UStaticMeshComponent* ASlopArena::Mesh(UStaticMesh* M, const FVector& Pos, const FVector& SizeM, const FRotator& Rot, UMaterialInterface* Mat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(M);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The engine's basic shapes are 1 m across.
	C->SetRelativeLocation(WorldToUE(Pos));
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(FVector(SizeM.Z, SizeM.X, SizeM.Y));
	if (Mat)
	{
		C->SetMaterial(0, Mat);
	}
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* ASlopArena::Cube(const FVector& Pos, const FVector& SizeM, UMaterialInterface* Mat, const FRotator& Rot)
{
	return Mesh(Shape(TEXT("Cube")), Pos, SizeM, Rot, Mat);
}

UStaticMeshComponent* ASlopArena::Cylinder(const FVector& Pos, double Radius, double Height, UMaterialInterface* Mat, const FRotator& Rot)
{
	return Mesh(Shape(TEXT("Cylinder")), Pos, FVector(Radius * 2, Height, Radius * 2), Rot, Mat);
}

UStaticMeshComponent* ASlopArena::Sphere(const FVector& Pos, double Radius, UMaterialInterface* Mat)
{
	return Mesh(Shape(TEXT("Sphere")), Pos, FVector(Radius * 2), FRotator::ZeroRotator, Mat);
}

UPointLightComponent* ASlopArena::Light(const FVector& Pos, const FLinearColor& Color, float Intensity, float Radius, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	L->SetRelativeLocation(WorldToUE(Pos));
	L->SetLightColor(Color);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(Radius * 100.f);
	L->SetCastShadows(bShadows && bHigh);
	L->RegisterComponent();
	return L;
}

UMaterialInstanceDynamic* ASlopArena::Surface(const TCHAR* Set, const FLinearColor& Tint, float Tiling, float Roughness, float Metallic)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Surface.M_Surface"));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	auto Tex = [Set](const TCHAR* Suffix) -> UTexture2D*
	{
		const FString P = FString::Printf(TEXT("/Game/Slop/Surfaces/%s/T_%s_%s.T_%s_%s"), Set, Set, Suffix, Set, Suffix);
		return LoadObject<UTexture2D>(nullptr, *P, nullptr, LOAD_Quiet | LOAD_NoWarn);
	};
	if (Set && *Set)
	{
		if (UTexture2D* T = Tex(TEXT("BaseColor"))) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture2D* T = Tex(TEXT("Normal"))) M->SetTextureParameterValue(TEXT("NormalTex"), T);
		if (UTexture2D* T = Tex(TEXT("ORM"))) M->SetTextureParameterValue(TEXT("ORMTex"), T);
	}
	M->SetVectorParameterValue(TEXT("Tint"), Tint);
	M->SetScalarParameterValue(TEXT("Tiling"), Tiling);
	M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Mats.Add(M);
	return M;
}

UMaterialInstanceDynamic* ASlopArena::Glow(const FLinearColor& Color, float Intensity, bool bAdditive)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bAdditive ? TEXT("/Game/Slop/Materials/M_GlowAdd.M_GlowAdd") : TEXT("/Game/Slop/Materials/M_GlowOpaque.M_GlowOpaque"));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	Mats.Add(M);
	return M;
}

void ASlopArena::Common(const FLinearColor& FogColor, float FogDensity, const FLinearColor& Ambient, float AmbientIntensity)
{
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(FogDensity);
	Fog->SetFogInscatteringColor(FogColor);
	Fog->SetFogHeightFalloff(0.35f);
	Fog->SetStartDistance(300.f);
	Fog->RegisterComponent();
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	Sky->SetLightColor(Ambient);
	Sky->SetIntensity(AmbientIntensity);
	Sky->bLowerHemisphereIsBlack = false;
	Sky->RegisterComponent();
}

void ASlopArena::Build(FName Stage, bool bHighGraphics)
{
	StageId = Stage;
	bHigh = bHighGraphics;
	if (Stage == TEXT("drowned_chapel")) BuildChapel();
	else if (Stage == TEXT("stilt_market")) BuildMarket();
	else if (Stage == TEXT("roost")) BuildRoost();
	else BuildYards();
}

void ASlopArena::Step(double Delta)
{
	Time += Delta;
	for (FFlicker& F : Flickers)
	{
		if (F.Light)
		{
			const float N = 0.5f * FMath::Sin(Time * F.Speed + F.Phase) + 0.3f * FMath::Sin(Time * F.Speed * 2.7f + F.Phase * 1.7f) + 0.2f * FMath::Sin(Time * F.Speed * 5.3f);
			F.Light->SetIntensity(F.Base * (1.f + F.Depth * N));
		}
	}
	for (FBob& B : Bobs)
	{
		if (B.C)
		{
			B.C->SetRelativeLocation(B.Base + FVector(0, 0, B.Amp * FMath::Sin(Time * B.Speed + B.Phase)));
		}
	}
}

// --- Stages (first pass: floor, back wall, key/rim/fill lights; detailed sets follow) -----------------

static UDirectionalLightComponent* Sun(AActor* Owner, USceneComponent* Root, const FRotator& R, const FLinearColor& C, float Lux, bool bShadows)
{
	UDirectionalLightComponent* D = NewObject<UDirectionalLightComponent>(Owner);
	D->SetupAttachment(Root);
	D->SetMobility(EComponentMobility::Movable);
	D->SetRelativeRotation(R);
	D->SetLightColor(C);
	D->SetIntensity(Lux);
	D->SetCastShadows(bShadows);
	D->RegisterComponent();
	return D;
}

void ASlopArena::BuildYards()
{
	Music = TEXT("music_yards");
	Footstep = TEXT("kenney:footstep_concrete");
	Common(FLinearColor(0.05f, 0.07f, 0.05f), 0.02f, FLinearColor(0.35f, 0.45f, 0.4f), 0.35f);
	Cube(FVector(0, -0.25, 0), FVector(30, 0.5, 14), Surface(TEXT("MetalPlates013"), FLinearColor(0.5f, 0.5f, 0.48f), 6.f, 1.f, 1.f));
	Cube(FVector(0, 5, -7), FVector(40, 12, 0.5), Surface(TEXT("factory_brick"), FLinearColor(0.35f, 0.3f, 0.28f), 6.f));
	Sun(this, Root, FRotator(-38, 140, 0), FLinearColor(1.f, 0.78f, 0.55f), 3.5f, true);
	Light(FVector(-3, 3, 4), FLinearColor(0.45f, 1.f, 0.35f), 4000.f, 12.f);
	Light(FVector(4, 2, -3), FLinearColor(1.f, 0.45f, 0.15f), 3000.f, 10.f);
}

void ASlopArena::BuildChapel()
{
	Music = TEXT("music_chapel");
	Footstep = TEXT("kenney:footstep_concrete");
	Common(FLinearColor(0.04f, 0.06f, 0.09f), 0.025f, FLinearColor(0.4f, 0.45f, 0.6f), 0.35f);
	Cube(FVector(0, -0.25, 0), FVector(30, 0.5, 14), Surface(TEXT("monastery_stone_floor"), FLinearColor(0.5f, 0.52f, 0.55f), 6.f));
	Cube(FVector(0, 5, -7), FVector(40, 12, 0.5), Surface(TEXT("medieval_blocks_03"), FLinearColor(0.35f, 0.37f, 0.4f), 6.f));
	Sun(this, Root, FRotator(-45, 160, 0), FLinearColor(0.7f, 0.8f, 1.f), 3.f, true);
	Light(FVector(0, 2.5, 3), FLinearColor(1.f, 0.7f, 0.4f), 3000.f, 10.f);
}

void ASlopArena::BuildMarket()
{
	Music = TEXT("music_market");
	Footstep = TEXT("kenney:footstep_wood");
	Common(FLinearColor(0.05f, 0.06f, 0.07f), 0.03f, FLinearColor(0.35f, 0.4f, 0.5f), 0.35f);
	Cube(FVector(0, -0.25, 0), FVector(30, 0.5, 14), Surface(TEXT("weathered_planks"), FLinearColor(0.5f, 0.45f, 0.4f), 6.f));
	Sun(this, Root, FRotator(-40, 150, 0), FLinearColor(0.6f, 0.7f, 0.9f), 2.5f, true);
	Light(FVector(-2, 3, 2), FLinearColor(1.f, 0.65f, 0.3f), 3500.f, 10.f);
	Light(FVector(3, 3, 2), FLinearColor(1.f, 0.65f, 0.3f), 3500.f, 10.f);
}

void ASlopArena::BuildRoost()
{
	Music = TEXT("music_roost");
	Footstep = TEXT("kenney:footstep_concrete");
	Common(FLinearColor(0.03f, 0.06f, 0.03f), 0.03f, FLinearColor(0.3f, 0.45f, 0.3f), 0.3f);
	Cube(FVector(0, -0.25, 0), FVector(30, 0.5, 14), Surface(TEXT("dark_rock"), FLinearColor(0.35f, 0.38f, 0.33f), 6.f));
	Sun(this, Root, FRotator(-50, 150, 0), FLinearColor(0.55f, 1.f, 0.5f), 2.f, true);
	Light(FVector(0, 4, -2), FLinearColor(0.4f, 1.f, 0.3f), 5000.f, 14.f);
}
