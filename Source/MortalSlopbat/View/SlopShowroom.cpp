#include "View/SlopShowroom.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/SlopGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UI/SlopStyle.h"
#include "View/SlopFighterActor.h"

namespace SlopShowroomImpl
{
	// The camera: 30 degrees of vertical view at 16:9, level. The figures' depth and the camera's
	// height place them on screen (SetLayout).
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
	// A wet dark floor, a far sky of the Heart's violet, a low haze, and a faint night for the ambient.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>(this);
	Floor->SetupAttachment(Root);
	Floor->SetStaticMesh(Plane);
	Floor->SetRelativeLocation(FVector(1800, 0, 0));
	Floor->SetRelativeScale3D(FVector(80, 90, 1));
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Surface.M_Surface")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		auto Tex = [](const TCHAR* Suffix) { return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Slop/Surfaces/monastery_stone_floor/T_monastery_stone_floor_%s.T_monastery_stone_floor_%s"), Suffix, Suffix), nullptr, LOAD_Quiet | LOAD_NoWarn); };
		if (UTexture2D* T = Tex(TEXT("BaseColor"))) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture2D* T = Tex(TEXT("Normal"))) M->SetTextureParameterValue(TEXT("NormalTex"), T);
		if (UTexture2D* T = Tex(TEXT("ORM"))) M->SetTextureParameterValue(TEXT("ORMTex"), T);
		M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.15f, 0.14f, 0.18f));
		M->SetScalarParameterValue(TEXT("Tiling"), 0.5f);
		M->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
		Floor->SetMaterial(0, M);
	}
	Floor->RegisterComponent();
	if (UMaterialInterface* HallSky = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Hall.M_Hall"), nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		Wall = NewObject<UStaticMeshComponent>(this);
		Wall->SetupAttachment(Root);
		Wall->SetStaticMesh(Plane);
		Wall->SetRelativeLocation(FVector(5400, 0, 1300));
		Wall->SetRelativeRotation(FRotator(90, 0, 0));
		Wall->SetRelativeScale3D(FVector(44, 88, 1));
		Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wall->SetCastShadow(false);
		UMaterialInstanceDynamic* Night = UMaterialInstanceDynamic::Create(HallSky, this);
		Night->SetScalarParameterValue(TEXT("Intensity"), 0.8f);
		Wall->SetMaterial(0, Night);
		Wall->RegisterComponent();
	}
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.05f);
	Fog->SetFogHeightFalloff(0.9f);
	Fog->SetFogInscatteringColor(FLinearColor(0.016f, 0.008f, 0.03f));
	Fog->SetStartDistance(500.f);
	Fog->RegisterComponent();
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	Sky->SetCubemap(LoadObject<UTextureCube>(nullptr, TEXT("/Game/Slop/Skies/HDR_lakeside_night.HDR_lakeside_night")));
	Sky->SetLightColor(FLinearColor(0.3f, 0.3f, 0.38f));
	Sky->SetIntensity(0.4f);
	Sky->RegisterComponent();
	// A low violet glow out on the floor, for the sky's light to lie along the wet stone.
	Pool = NewObject<UPointLightComponent>(this);
	Pool->SetupAttachment(Root);
	Pool->SetIntensityUnits(ELightUnits::Candelas);
	Pool->SetIntensity(800.f);
	Pool->SetLightColor(FLinearColor(0.42f, 0.16f, 0.9f));
	Pool->SetAttenuationRadius(3200.f);
	Pool->SetCastShadows(false);
	Pool->SetRelativeLocation(FVector(1900, 0, 300));
	Pool->RegisterComponent();
	for (int32 S = 0; S < 2; ++S)
	{
		FFigure& F = Figures[S];
		// A cool key from the front and the camera's side, a coloured rim from behind.
		F.Key = NewObject<USpotLightComponent>(this);
		F.Key->SetupAttachment(Root);
		F.Key->SetIntensityUnits(ELightUnits::Candelas);
		F.Key->SetIntensity(120.f);
		F.Key->SetLightColor(FLinearColor(0.84f, 0.82f, 1.f));
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
	return FVector(Depth, PanelX[Side] * Depth * TanH, 0);
}

void ASlopShowroom::FitView()
{
	if (bPortrait)
	{
		return;
	}
	double Aspect = 16.0 / 9.0;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		if (Size.Y > 0)
		{
			Aspect = Size.X / Size.Y;
		}
	}
	const double Base = FMath::Tan(FMath::DegreesToRadians(VFov * 0.5));
	const double TanV = Aspect >= 16.0 / 9.0 ? Base : Base * (16.0 / 9.0) / Aspect;
	TanH = TanV * Aspect;
	Camera->SetAspectRatio((float)Aspect);
	Camera->SetFieldOfView((float)(2.0 * FMath::RadiansToDegrees(FMath::Atan(TanH))));
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
	TargetDepth = 180.0 * 1080.0 / (HeightPx * 2.0 * TanV);
	const double FeetNdc = 1.0 - 2.0 * FeetY / 1080.0;
	TargetCameraZ = -FeetNdc * TargetDepth * TanV;
	TargetPanelX[0] = LeftX / 960.0 - 1.0;
	TargetPanelX[1] = RightX / 960.0 - 1.0;
	if (!bLaidOut)
	{
		bLaidOut = true;
		Depth = TargetDepth;
		CameraZ = TargetCameraZ;
		PanelX[0] = TargetPanelX[0];
		PanelX[1] = TargetPanelX[1];
	}
}

void ASlopShowroom::Clear()
{
	Show(0, NAME_None, false);
	Show(1, NAME_None, false);
	SetLayout(-2600.0, 2600.0, 1000.0, 620.0);
}

void ASlopShowroom::Show(int32 Side, FName Id, bool bCostume, FName Outfit)
{
	FFigure& F = Figures[Side];
	if (F.Id == Id && F.bCostume == bCostume && F.Outfit == Outfit && (F.View || Id.IsNone() || F.bFailed))
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
	F.Outfit = Outfit;
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
	if (!F.View->Setup(Me, USlopGameInstance::MeshFor(Outfit.IsNone() ? Id : Outfit)))
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
	// A new figure steps in from its own side of the screen.
	const double In = bPortrait ? 0.0 : 1.0 - SlopUI::EaseOut((float)(F.Age / 0.3));
	const FVector At = Spot(Side) + FVector(0.0, Facing * -130.0 * In, 0.0);
	T = T * FTransform(FRotator(0.0, FMath::RadiansToDegrees(A), 0.0), At);
	Body->SetWorldTransform(T);
	if (!bPortrait)
	{
		// A fighting stance carries the hips half a metre off the mark, more on a big fighter. Screens
		// compose around the body, so it is centred by its hips; smoothed, to leave its sway in.
		const double Lean = Body->GetBoneLocation(TEXT("pelvis")).Y - At.Y;
		F.Lean = F.Age < 0.1 ? Lean : FMath::Lerp(F.Lean, Lean, FMath::Min(Dt * 2.0, 1.0));
		Body->AddWorldOffset(FVector(0.0, -F.Lean, 0.0));
	}
}

void ASlopShowroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (!bPortrait)
	{
		FitView();
		const bool bStill = SlopUI::ReducedMotion();
		const double K = bStill ? 1.0 : 1.0 - FMath::Exp(-DeltaSeconds * 4.0);
		Depth = FMath::Lerp(Depth, TargetDepth, K);
		CameraZ = FMath::Lerp(CameraZ, TargetCameraZ, K);
		PanelX[0] = FMath::Lerp(PanelX[0], TargetPanelX[0], K);
		PanelX[1] = FMath::Lerp(PanelX[1], TargetPanelX[1], K);
		Camera->SetRelativeLocation(FVector(0.0, bStill ? 0.0 : FMath::Sin(Time * 0.11) * 14.0, CameraZ));
		if (Pool)
		{
			Pool->SetIntensity(bStill ? 800.f : 800.f + 140.f * (float)FMath::Sin(Time * 0.5));
		}
		PlaceLights();
	}
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
	// Portraits are of the fighter alone, on black.
	bPortrait = true;
	if (Wall)
	{
		Wall->SetVisibility(false);
	}
	if (Pool)
	{
		Pool->SetVisibility(false);
	}
	FFigure& F = Figures[0];
	if (F.Id != Id)
	{
		Show(1, NAME_None, false);
		Show(0, Id, false);
		F.Sim->Fighters[0].Facing = 1;
		// A fighter whose guard hides their face stands easy for it.
		const Slop::FFighterDef* Def = Slop::GetContent().Fighter(Id);
		if (F.View && Def && Def->Look.IsValid() && Def->Look->HasField(TEXT("portrait_pose")))
		{
			F.View->Anim.PoseOverride = FName(*Def->Look->GetStringField(TEXT("portrait_pose")));
		}
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
