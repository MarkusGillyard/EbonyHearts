#pragma once

#include "GameFramework/Actor.h"
#include "Combat/SlopSim.h"
#include "View/SlopMatch.h"
#include "SlopShowroom.generated.h"

class ASlopFighterActor;
class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;

// The hall: one dark, wet floor under a far fractured sky, behind every menu, with whoever a screen
// asks for standing in it, breathing and slowly turning, lit with a rim in their own colour. Moving
// from screen to screen moves its camera rather than changing the scene. A pick plays the win pose.
// Each figure runs on its own small simulation. Also renders the select-screen portraits (head and
// shoulders), one fighter at a time.
UCLASS()
class ASlopShowroom : public AActor
{
	GENERATED_BODY()
public:
	ASlopShowroom();
	virtual void Tick(float DeltaSeconds) override;
	// Takes the view.
	void Open(APlayerController* PC);
	// Where the figures stand on screen, in 1920 x 1080 pixels: the centre of each side's panel,
	// the line their feet stand on, and how tall a 1.8 m fighter appears.
	// A new layout is eased into, the first one after opening taken at once.
	void SetLayout(double LeftX, double RightX, double FeetY, double HeightPx);
	// Empties the hall and returns its camera to the wide view.
	void Clear();
	// Who stands on a side (NAME_None leaves it empty), in which colours.
	void Show(int32 Side, FName Id, bool bCostume, FName Outfit = NAME_None);
	void Celebrate(int32 Side);
	// Portrait framing: one fighter in the middle, the camera on its head and shoulders. Returns
	// false until the figure has loaded and settled.
	bool Portrait(FName Id);

private:
	struct FFigure
	{
		TUniquePtr<Slop::FFightSim> Sim;
		TObjectPtr<ASlopFighterActor> View;
		FName Id;
		bool bCostume = false;
		FName Outfit;
		// This choice could not be shown (no data or no body); not retried until it changes.
		bool bFailed = false;
		double Celebrate = 0.0;
		double Age = 0.0;
		// How far a stance carries the hips off the figure's mark, smoothed.
		double Lean = 0.0;
		TObjectPtr<USpotLightComponent> Key;
		TObjectPtr<USpotLightComponent> Rim;
	};
	FVector Spot(int32 Side) const;
	void PlaceLights();
	// Fits the view to the screen: never narrower than 16:9 shows, wider on a wide one.
	void FitView();
	void Place(FFigure& F, int32 Side, double Dt);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY()
	TObjectPtr<class UExponentialHeightFogComponent> Fog;
	UPROPERTY()
	TObjectPtr<class UPointLightComponent> Pool;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Wall;
	FFigure Figures[2];
	double Depth = 715.0;
	double PanelX[2] = {-0.677, 0.677};
	double CameraZ = 89.0;
	double TargetDepth = 715.0;
	double TargetPanelX[2] = {-0.677, 0.677};
	double TargetCameraZ = 89.0;
	bool bLaidOut = false;
	double TanH = 0.47;
	Slop::FFixedClock Clock;
	double Time = 0.0;
	bool bPortrait = false;
};
