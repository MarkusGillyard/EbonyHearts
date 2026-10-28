#include "View/SlopFightCamera.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Anim/SlopMeshRig.h"
#include "Anim/SlopPose.h"

using namespace Slop;

ASlopFightCamera::ASlopFightCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(50.f);
}

double ASlopFightCamera::Aspect() const
{
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		if (Size.Y > 0)
		{
			return Size.X / Size.Y;
		}
	}
	return 16.0 / 9.0;
}

void ASlopFightCamera::Kick(const FVector& Dir, double Amount)
{
	if (bShakeEnabled && Dir.Size() > 0.001)
	{
		KickV += Dir.GetSafeNormal() * Amount * 40.0;
	}
}

void ASlopFightCamera::Targets(FVector& OutPos, FVector& OutLook) const
{
	const FFighter& A = Sim->Fighters[0];
	const FFighter& B = Sim->Fighters[1];
	const double Mid = (A.Pos.X + B.Pos.X) * 0.5;
	const double Sep = FMath::Abs(B.Pos.X - A.Pos.X);
	// Framing is computed for at most 16:9: a wider screen shows more of the stage around the
	// same view instead of pulling the fighters in.
	const double Asp = FMath::Clamp(Aspect(), 1.0, 16.0 / 9.0);
	const double HalfHPerD = FMath::Tan(FMath::DegreesToRadians(VFov * 0.5));
	const double HalfWPerD = HalfHPerD * Asp;
	double Top = -1e9, Bottom = 1e9;
	for (const FFighter& F : Sim->Fighters)
	{
		Top = FMath::Max(Top, F.Pos.Y + F.Def->Height * 1.05 + HeadRoom);
		Bottom = FMath::Min(Bottom, F.Pos.Y - FloorRoom);
	}
	double Dist = FMath::Max((Sep + Margin) / (2.0 * HalfWPerD), FMath::Min((Top - Bottom) / (2.0 * HalfHPerD), DistMaxHeight));
	Dist = FMath::Clamp(Dist, DistMin, DistMax);
	Dist *= 1.0 - Zoom * 0.45;
	const double HalfW = Dist * HalfWPerD;
	const double HalfH = Dist * HalfHPerD;
	const double Lim = FMath::Max(0.0, FFightSim::StageHalf + 0.9 - HalfW);
	const double Cx = FMath::Clamp(Mid, -Lim, Lim);
	// Anchored to the lowest fighter: something above the farthest framing (the Slopbat rising
	// for its dive) leaves the top of the screen rather than the player the bottom.
	const double Ly = Bottom + HalfH;
	FVector Focus(Cx, Ly, 0.0);
	if (ZoomFocus >= 0)
	{
		const FVector2D Fp = Sim->Fighters[ZoomFocus].Pos;
		Focus = FMath::Lerp(Focus, FVector(Fp.X, Ly, 0.0), Zoom * 0.6);
	}
	OutPos = Focus + FVector(0.0, 0.18 + Dist * 0.02, Dist);
	OutLook = Focus;
}

void ASlopFightCamera::Snap()
{
	if (!Sim)
	{
		return;
	}
	Targets(Pos, LookAt);
	PosV = LookV = FVector::ZeroVector;
	Apply();
}

void ASlopFightCamera::Step(double Delta)
{
	if (!Sim)
	{
		return;
	}
	Zoom = MoveToward(Zoom, ZoomTarget, Delta * 2.2);
	OverrideW = MoveToward(OverrideW, OverrideTarget, Delta * 2.5);
	FVector PosT, LookT;
	Targets(PosT, LookT);
	if (OverrideW > 0.0)
	{
		const double K = Anim::FPose::Ease(TEXT("inout"), OverrideW);
		PosT = FMath::Lerp(PosT, OverridePos, K);
		LookT = FMath::Lerp(LookT, OverrideLook, K);
	}
	// The springs are stepped in short slices: one long frame (a stall, a screenshot being saved)
	// stepped whole made them diverge, and the camera swung about wildly until it settled.
	const double Total = FMath::Min(Delta, 0.25);
	const int32 N = FMath::Clamp(FMath::CeilToInt(Total * 240.0), 1, 60);
	const double H = Total / N;
	const double W = 7.0, WL = 9.0, WK = 22.0;
	for (int32 I = 0; I < N; ++I)
	{
		PosV += ((PosT - Pos) * W * W - PosV * 2.0 * W) * H;
		Pos += PosV * H;
		LookV += ((LookT - LookAt) * WL * WL - LookV * 2.0 * WL) * H;
		LookAt += LookV * H;
		KickV += (-KickOff * WK * WK - KickV * 2.0 * WK) * H;
		KickOff += KickV * H;
	}
	ShakeT += Delta;
	Shake = FMath::Max(0.0, Shake - Delta * 0.14);
	Apply();
}

void ASlopFightCamera::Apply()
{
	FVector Off = KickOff;
	if (Shake > 0.0)
	{
		Off += FVector(FMath::Sin(ShakeT * 61.0), FMath::Sin(ShakeT * 47.0 + 1.3), 0.0) * Shake;
	}
	const FVector P = Anim::WorldToUE(Pos + Off);
	const FVector L = Anim::WorldToUE(LookAt + Off);
	SetActorLocationAndRotation(P, (L - P).Rotation());
	// The original framed by vertical field of view; Unreal's is horizontal.
	const double HFov = 2.0 * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VFov * 0.5)) * Aspect()));
	Camera->SetFieldOfView((float)HFov);
	if (bDof)
	{
		Camera->PostProcessSettings.DepthOfFieldFocalDistance = (float)(Pos.Z * 100.0);
	}
}

void ASlopFightCamera::SetLook(bool bHighGraphics, bool bPostFx, float ExposureEV100)
{
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	bDof = bHighGraphics;
	PP.bOverride_DepthOfFieldFocalDistance = bDof;
	PP.bOverride_DepthOfFieldFstop = bDof;
	PP.bOverride_DepthOfFieldSensorWidth = bDof;
	PP.bOverride_DepthOfFieldDepthBlurAmount = false;
	// A long lens's shallow focus: the stage softens a few metres behind the fighting plane.
	PP.DepthOfFieldFstop = 2.8f;
	PP.DepthOfFieldSensorWidth = 24.576f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = bPostFx ? 0.55f : 0.0f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = bPostFx ? 0.12f : 0.0f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	// Each stage is lit for a fixed exposure (EV100); with manual metering the bias is its negative.
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = -ExposureEV100;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.45f;
}
