#pragma once

#include "GameFramework/Actor.h"
#include "Combat/SlopSim.h"
#include "SlopFightCamera.generated.h"

class UCameraComponent;

// The side-on fighting camera. It frames both fighters whole, pulls back as they separate,
// stays inside the stage and is smoothed with critically damped springs, so distances stay easy
// to judge. Shake is small and brief. Knockouts and finishers may take over the framing for a
// moment. Works in the original's space (metres) and maps to Unreal's at the end.
UCLASS()
class ASlopFightCamera : public AActor
{
	GENERATED_BODY()
public:
	static constexpr double VFov = 30.0;
	static constexpr double Margin = 1.9;
	static constexpr double DistMin = 4.8;
	static constexpr double DistMax = 8.4;
	static constexpr double DistMaxHeight = 7.2;
	static constexpr double FloorRoom = 0.3;
	static constexpr double HeadRoom = 0.35;

	ASlopFightCamera();
	void Bind(const Slop::FFightSim* InSim) { Sim = InSim; }
	void Snap();
	void Step(double Delta);
	void PunchIn(double Amount, int32 Focus = -1) { ZoomTarget = Amount; ZoomFocus = Focus; }
	void ReleaseZoom() { ZoomTarget = 0.0; ZoomFocus = -1; }
	void AddShake(double Amount) { if (bShakeEnabled) Shake = FMath::Max(Shake, Amount); }
	// Nudges the camera along a world direction; it springs back within a few frames.
	void Kick(const FVector& Dir, double Amount);
	void SetOverride(const FVector& InPos, const FVector& InLook, double Weight = 1.0) { OverridePos = InPos; OverrideLook = InLook; OverrideTarget = Weight; }
	void ClearOverride() { OverrideTarget = 0.0; }
	// Depth of field behind the fighting plane, vignette and grain.
	void SetLook(bool bHighGraphics, bool bPostFx);
	bool bShakeEnabled = true;
	// Where the camera is and looks, in the original's space (for effects that face it).
	FVector Pos = FVector::ZeroVector;
	FVector LookAt = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

private:
	void Targets(FVector& OutPos, FVector& OutLook) const;
	void Apply();
	double Aspect() const;
	const Slop::FFightSim* Sim = nullptr;
	FVector PosV = FVector::ZeroVector;
	FVector LookV = FVector::ZeroVector;
	double Shake = 0.0;
	double ShakeT = 0.0;
	FVector KickOff = FVector::ZeroVector;
	FVector KickV = FVector::ZeroVector;
	double Zoom = 0.0;
	double ZoomTarget = 0.0;
	int32 ZoomFocus = -1;
	FVector OverridePos = FVector::ZeroVector;
	FVector OverrideLook = FVector::ZeroVector;
	double OverrideW = 0.0;
	double OverrideTarget = 0.0;
	bool bDof = true;
};
