#pragma once

#include "GameFramework/Actor.h"
#include "Combat/SlopSim.h"
#include "View/SlopMatch.h"
#include "SlopShowroom.generated.h"

class ASlopFighterActor;
class UCameraComponent;
class USpotLightComponent;

// The fighters as the menus show them: in the select screen each side's choice stands in its
// stance on a dark floor, breathing and slowly turning, lit with a rim in its own colour, behind
// the screen's panels; a pick plays the win pose. Each figure runs on its own small simulation.
// Also renders the select-screen portraits (head and shoulders), one fighter at a time.
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
	void SetLayout(double LeftX, double RightX, double FeetY, double HeightPx);
	// The haze's colour (the face-off's rust).
	void SetMood(const FLinearColor& Haze);
	// Tints the second figure as the Reflection.
	void SetReflection(int32 Side, bool bOn);
	// Who stands on a side (NAME_None leaves it empty), in which colours.
	void Show(int32 Side, FName Id, bool bCostume);
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
		// This choice could not be shown (no data or no body); not retried until it changes.
		bool bFailed = false;
		double Celebrate = 0.0;
		double Age = 0.0;
		TObjectPtr<USpotLightComponent> Key;
		TObjectPtr<USpotLightComponent> Rim;
	};
	FVector Spot(int32 Side) const;
	void PlaceLights();
	void Place(FFigure& F, int32 Side, double Dt);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY()
	TObjectPtr<class UExponentialHeightFogComponent> Fog;
	FFigure Figures[2];
	double Depth = 715.0;
	double PanelX[2] = {-0.677, 0.677};
	bool bReflection[2] = {false, false};
	Slop::FFixedClock Clock;
	double Time = 0.0;
	bool bPortrait = false;
};
