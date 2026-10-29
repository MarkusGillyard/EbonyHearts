#pragma once

#include "GameFramework/Actor.h"
#include "AI/SlopAI.h"
#include "Core/SlopFlow.h"
#include "View/SlopFinisher.h"
#include "View/SlopGuides.h"
#include "SlopMatch.generated.h"

class ASlopFighterActor;
class ASlopFightCamera;
class ASlopArena;
class ASlopVfx;
class USlopGameInstance;

namespace Slop
{
	// A fixed 60 Hz simulation clock driven by rendered frames of any length. Each frame adds its
	// time; whole ticks are taken out. At most MaxSteps ticks run per frame: a frame longer than
	// that (a hitch, a debugger) drops the excess, so the game slows rather than spiralling.
	struct FFixedClock
	{
		static constexpr int32 MaxSteps = 4;
		double Accum = 0.0;
		int32 Advance(double FrameSeconds)
		{
			Accum += FMath::Clamp(FrameSeconds, 0.0, 0.25);
			int32 N = 0;
			while (Accum >= DT && N < MaxSteps)
			{
				Accum -= DT;
				++N;
			}
			if (N == MaxSteps && Accum >= DT)
			{
				Accum = FMath::Fmod(Accum, DT);
			}
			return N;
		}
		// How far rendering is between the last tick and the next one.
		double Frac() const { return FMath::Clamp(Accum / DT, 0.0, 1.0); }
		void Reset() { Accum = 0.0; }
	};
}

// A fight on screen. Steps the simulation at 60 Hz from the players' devices or the CPU, then
// draws the fighters between ticks, the camera, effects and HUD. Losing focus or a controller
// pauses the match and clears every held input.
UCLASS()
class ASlopMatch : public AActor
{
	GENERATED_BODY()
public:
	ASlopMatch();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	bool Start(const Slop::FMatchConfig& Config);
	void SetPaused(bool bPause);
	bool IsPaused() const { return bPaused; }
	bool HasEnded() const { return bEnded; }
	// Training: back to the start positions.
	void ResetPositions();
	void SkipFinisher() { Sim.SkipFinisher(); }
	// Applies the comfort and graphics settings (shake, flashes, effects, depth of field).
	void ApplySettings();
	// Slow motion asked for by a set piece (an instant kill's blow).
	void KoSlowExternal(int32 Ticks) { KoSlow = FMath::Max(KoSlow, Ticks); }
	USlopGameInstance* GetGI() const { return GI(); }
	Slop::FFinisherDirector Director;
	// The tutorial or the trials, when running.
	TUniquePtr<Slop::IGuide> Guide;
	// A guide ends the match itself (the tutorial completed without a knockout).
	void FinishFromGuide(int32 Winner);
	void Announce(const FText& Text, const FText& Sub) { if (OnAnnounce) OnAnnounce(Text, Sub); }
	TFunction<void(const FText&, const FText&)> OnAnnounce;

	Slop::FFightSim Sim;
	Slop::FMatchConfig Config;
	Slop::FMatchResult Result;
	TUniquePtr<Slop::IBrain> Brains[2];
	// The CPU brains among them, for their development telemetry (-SlopAiLog).
	Slop::FCpuBrain* CpuBrains[2] = {nullptr, nullptr};
	Slop::FTrainingDummy* Dummy = nullptr;
	// The events of the ticks run this frame, for the HUD and effects.
	TArray<Slop::FSimEvent> FrameEvents;
	// The pads fed to the simulation on the last tick (input display).
	Slop::FPadState LastPads[2];
	// Frame advantage of the last exchange (training's frame view).
	Slop::FAdvantageMeter Advantage;
	// Development: freeze, single-step and slow motion; hit/hurtbox and frame-data views.
	bool bDebugPause = false;
	bool bDebugStep = false;
	bool bDebugSlow = false;
	bool bShowBoxes = false;
	// Development: -SlopHideFighters keeps both bodies hidden (stage pictures).
	bool bDevHideFighters = false;
	int32 TicksRun = 0;
	double SimMicros = 0.0;
	// Seconds left on the clock at the end of each round won by player 1, summed (time bonus).
	int32 TimeLeft = 0;

	UPROPERTY()
	TObjectPtr<ASlopFighterActor> Views[2];
	UPROPERTY()
	TObjectPtr<ASlopFightCamera> Camera;
	UPROPERTY()
	TObjectPtr<ASlopArena> Arena;
	UPROPERTY()
	TObjectPtr<ASlopVfx> Vfx;

	// Called once when the match ends.
	TFunction<void(const Slop::FMatchResult&)> OnFinished;
	// Called for every simulation event after the match has reacted to it.
	TFunction<void(const Slop::FSimEvent&)> OnEvent;

private:
	void FixedTick();
	// A fighter that changed shape gets its new body, and the looks this match gives it.
	void RebindView(int32 I);
	void DressView(int32 I);
	// A body that has just changed comes together out of gold over a quarter of a second (1 to 0).
	float FormFade[2] = {0.f, 0.f};
	void HandleEvent(const Slop::FSimEvent& E);
	void PlayEventSound(const Slop::FSimEvent& E);
public:
	// Every box in play, in simulation space, with the colour it is drawn in (training's box
	// view, drawn by the HUD so it works in any build).
	void CollectBoxes(TArray<TPair<Slop::FRect, FLinearColor>>& Out) const;
private:
	USlopGameInstance* GI() const;

	Slop::FFixedClock Clock;
	bool bPaused = false;
	bool bEnded = false;
	int32 KoFreeze = 0;
	int32 KoSlow = 0;
	int32 SlowCount = 0;
	int32 SteppedThisFrame = 0;
};
