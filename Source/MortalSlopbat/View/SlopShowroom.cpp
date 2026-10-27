#include "View/SlopShowroom.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopFighterActor.h"

namespace SlopShowroomImpl
{
	// The camera: 30 degrees of vertical view at 16:9, level. The figures' depth and the camera's
	// height place them on screen (SetLayout); the select screen's panels are the default.
	constexpr double VFov = 30.0;
	constexpr double EyeHeight = 89.0;

	double HFov(double V) { return 2.0 * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(V * 0.5)) * 16.0 / 9.0)); }

	FLinearColor Glow(const Slop::FFighterDef* D)
	{
		return FLinearColor(FColor::FromHex(Slop::Json::Str(D->Look, TEXT("glow"), TEXT("#a8db38"))));
	}
}

using namespace SlopShowroomImpl;

ASlopShowroom::ASlopShowroom()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetRelativeLocation(FVector(0, 0, EyeHeight));
	Camera->SetFieldOfView(HFov(VFov));
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = -1.f;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.6f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.4f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
}

void ASlopShowroom::Open(APlayerController* PC)
{
	// A wet dark floor, a low teal haze, and a faint night sky for the ambient.
	UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>(this);
	Floor->SetupAttachment(Root);
	Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Floor->SetRelativeLocation(FVector(Depth, 0, 0));
	Floor->SetRelativeScale3D(FVector(60, 60, 1));
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Surface.M_Surface")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		auto Tex = [](const TCHAR* Suffix) { return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Slop/Surfaces/monastery_stone_floor/T_monastery_stone_floor_%s.T_monastery_stone_floor_%s"), Suffix, Suffix), nullptr, LOAD_Quiet | LOAD_NoWarn); };
		if (UTexture2D* T = Tex(TEXT("BaseColor"))) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture2D* T = Tex(TEXT("Normal"))) M->SetTextureParameterValue(TEXT("NormalTex"), T);
		if (UTexture2D* T = Tex(TEXT("ORM"))) M->SetTextureParameterValue(TEXT("ORMTex"), T);
		M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.22f, 0.24f, 0.23f));
		M->SetScalarParameterValue(TEXT("Tiling"), 0.5f);
		M->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
		Floor->SetMaterial(0, M);
	}
	Floor->RegisterComponent();
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.12f);
	Fog->SetFogHeightFalloff(0.6f);
	Fog->SetFogInscatteringColor(FLinearColor(0.02f, 0.035f, 0.03f));
	Fog->SetStartDistance(300.f);
	Fog->RegisterComponent();
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	Sky->SetCubemap(LoadObject<UTextureCube>(nullptr, TEXT("/Game/Slop/Skies/HDR_lakeside_night.HDR_lakeside_night")));
	Sky->SetLightColor(FLinearColor(0.32f, 0.36f, 0.34f));
	Sky->SetIntensity(0.45f);
	Sky->RegisterComponent();
	for (int32 S = 0; S < 2; ++S)
	{
		FFigure& F = Figures[S];
		// A warm key from the front and the camera's side, a coloured rim from behind.
		F.Key = NewObject<USpotLightComponent>(this);
		F.Key->SetupAttachment(Root);
		F.Key->SetIntensityUnits(ELightUnits::Candelas);
		F.Key->SetIntensity(70.f);
		F.Key->SetLightColor(FLinearColor(1.f, 0.86f, 0.7f));
		F.Key->SetOuterConeAngle(32.f);
		F.Key->SetInnerConeAngle(16.f);
		F.Key->SetAttenuationRadius(1200.f);
		F.Key->RegisterComponent();
		F.Rim = NewObject<USpotLightComponent>(this);
		F.Rim->SetupAttachment(Root);
		F.Rim->SetIntensityUnits(ELightUnits::Candelas);
		F.Rim->SetIntensity(90.f);
		F.Rim->SetOuterConeAngle(30.f);
		F.Rim->SetAttenuationRadius(1200.f);
		F.Rim->SetCastShadows(false);
		F.Rim->RegisterComponent();
		// Lit only while someone stands there (Show).
		F.Key->SetVisibility(false);
		F.Rim->SetVisibility(false);
	}
	PlaceLights();
	if (PC)
	{
		PC->SetViewTarget(this);
	}
}

FVector ASlopShowroom::Spot(int32 Side) const
{
	if (bPortrait)
	{
		return FVector(Depth, 0, 0);
	}
	const double Half = FMath::Tan(FMath::DegreesToRadians(HFov(VFov) * 0.5));
	return FVector(Depth, PanelX[Side] * Depth * Half, 0);
}

void ASlopShowroom::PlaceLights()
{
	for (int32 S = 0; S < 2; ++S)
	{
		FFigure& F = Figures[S];
		if (!F.Key)
		{
			continue;
		}
		const FVector At = Spot(S);
		const double Toward = S == 0 ? 1.0 : -1.0;
		F.Key->SetRelativeLocation(At + FVector(-380, 160 * Toward, 360));
		F.Key->SetRelativeRotation((At + FVector(0, 0, 110) - F.Key->GetRelativeLocation()).Rotation());
		F.Rim->SetRelativeLocation(At + FVector(340, -140 * Toward, 300));
		F.Rim->SetRelativeRotation((At + FVector(0, 0, 120) - F.Rim->GetRelativeLocation()).Rotation());
	}
}

void ASlopShowroom::SetLayout(double LeftX, double RightX, double FeetY, double HeightPx)
{
	// A 1.8 m figure HeightPx tall: the depth that gives; then the camera height that puts
	// the floor line at FeetY; then each side's offset to its panel centre.
	const double TanV = FMath::Tan(FMath::DegreesToRadians(VFov * 0.5));
	Depth = 180.0 * 1080.0 / (HeightPx * 2.0 * TanV);
	const double FeetNdc = 1.0 - 2.0 * FeetY / 1080.0;
	Camera->SetRelativeLocation(FVector(0, 0, -FeetNdc * Depth * TanV));
	PanelX[0] = LeftX / 960.0 - 1.0;
	PanelX[1] = RightX / 960.0 - 1.0;
	PlaceLights();
}

void ASlopShowroom::SetMood(const FLinearColor& Haze)
{
	if (Fog)
	{
		Fog->SetFogInscatteringColor(Haze);
	}
}

void ASlopShowroom::SetReflection(int32 Side, bool bOn)
{
	bReflection[Side] = bOn;
	if (Figures[Side].View)
	{
		Figures[Side].View->SetTint(FLinearColor(0.06f, 0.1f, 0.05f), bOn ? 0.72f : 0.f, bOn ? 0.6f : 0.f);
	}
}

void ASlopShowroom::Show(int32 Side, FName Id, bool bCostume)
{
	FFigure& F = Figures[Side];
	if (F.Id == Id && F.bCostume == bCostume && (F.View || Id.IsNone() || F.bFailed))
	{
		return;
	}
	if (F.View)
	{
		F.View->Destroy();
		F.View = nullptr;
	}
	F.Sim.Reset();
	F.Id = Id;
	F.bCostume = bCostume;
	F.Celebrate = 0.0;
	F.Age = 0.0;
	F.bFailed = false;
	const Slop::FFighterDef* D = Id.IsNone() ? nullptr : Slop::GetContent().Fighter(Id);
	if (F.Key) F.Key->SetVisibility(D != nullptr);
	if (F.Rim) F.Rim->SetVisibility(D != nullptr);
	if (!D)
	{
		F.bFailed = !Id.IsNone();
		return;
	}
	F.Sim = MakeUnique<Slop::FFightSim>();
	F.Sim->bTraining = true;
	F.Sim->bIntro = false;
	F.Sim->Setup(D, D);
	F.Sim->Phase = Slop::EPhase::Fight;
	Slop::FFighter& Me = F.Sim->Fighters[0];
	Me.Pos = FVector2D::ZeroVector;
	Me.Facing = Side == 0 ? 1 : -1;
	Me.SetState(Slop::EState::Idle);
	F.Sim->Fighters[1].Pos = FVector2D(30.0 * Me.Facing, 0.0);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	F.View = GetWorld()->SpawnActor<ASlopFighterActor>(P);
	if (!F.View->Setup(Me, USlopGameInstance::MeshFor(Id)))
	{
		F.View->Destroy();
		F.View = nullptr;
		F.bFailed = true;
		return;
	}
	if (bCostume)
	{
		F.View->SetCostume(FLinearColor(0.55f, 0.75f, 1.1f));
	}
	if (bReflection[Side])
	{
		F.View->SetTint(FLinearColor(0.06f, 0.1f, 0.05f), 0.72f, 0.6f);
	}
	F.View->SyncFromSim(true);
	if (F.Rim)
	{
		F.Rim->SetLightColor(FMath::Lerp(Glow(D), FLinearColor::White, 0.3f));
	}
}

void ASlopShowroom::Celebrate(int32 Side)
{
	FFigure& F = Figures[Side];
	if (F.Sim)
	{
		F.Celebrate = 1.6;
		F.Sim->Fighters[0].SetState(Slop::EState::Win);
	}
}

void ASlopShowroom::Place(FFigure& F, int32 Side, double Dt)
{
	if (!F.View)
	{
		return;
	}
	F.View->UpdateVisual(Clock.Frac(), Dt);
	// The original turned its camera slowly about each figure; here the figure turns instead,
	// a quarter toward the camera and swaying about that.
	const double Facing = Side == 0 ? 1.0 : -1.0;
	const double A = bPortrait ? 0.0 : FMath::Sin(Time * 0.35 + Side * 1.3) * 0.18 + 0.25 * Facing;
	UPoseableMeshComponent* Body = F.View->Body;
	FTransform T = Body->GetComponentTransform();
	T = T * FTransform(FRotator(0.0, FMath::RadiansToDegrees(A), 0.0), Spot(Side));
	Body->SetWorldTransform(T);
}

void ASlopShowroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	const int32 Steps = Clock.Advance(DeltaSeconds);
	static const Slop::FPadState None[2];
	for (int32 S = 0; S < 2; ++S)
	{
		FFigure& F = Figures[S];
		if (!F.Sim)
		{
			continue;
		}
		F.Age += DeltaSeconds;
		for (int32 I = 0; I < Steps; ++I)
		{
			F.Sim->Step(None);
			if (F.View)
			{
				F.View->SyncFromSim();
			}
		}
		if (F.Celebrate > 0.0 && (F.Celebrate -= DeltaSeconds) <= 0.0)
		{
			F.Sim->Fighters[0].SetState(Slop::EState::Idle);
		}
		Place(F, S, DeltaSeconds);
	}
}

bool ASlopShowroom::Portrait(FName Id)
{
	bPortrait = true;
	FFigure& F = Figures[0];
	if (F.Id != Id)
	{
		Show(1, NAME_None, false);
		Show(0, Id, false);
		F.Sim->Fighters[0].Facing = 1;
		return false;
	}
	if (!F.View || F.Age < 1.0)
	{
		return false;
	}
	// Head and shoulders from the front and to the right, a little above: about 0.75 m of a
	// 1.8 m fighter fills the frame's height (the original's portrait camera, 24 degrees tall).
	const Slop::FFighterDef* D = Slop::GetContent().Fighter(Id);
	const double K = D ? D->Height / 1.8 : 1.0;
	const FVector Head = F.View->Body->GetBoneLocation(TEXT("head")) - FVector(0, 0, 6.0 * K);
	const FVector Dir = FVector(-1.0, 0.55, 0.05).GetSafeNormal();
	Camera->SetFieldOfView(HFov(24.0));
	Camera->SetWorldLocationAndRotation(Head + Dir * 175.0 * K, (-Dir).Rotation());
	F.Key->SetWorldLocation(Head + FVector(-300, 220, 180));
	F.Key->SetWorldRotation((Head - F.Key->GetComponentLocation()).Rotation());
	F.Rim->SetWorldLocation(Head + FVector(260, -200, 60));
	F.Rim->SetWorldRotation((Head - F.Rim->GetComponentLocation()).Rotation());
	return F.Age > 1.6;
}
