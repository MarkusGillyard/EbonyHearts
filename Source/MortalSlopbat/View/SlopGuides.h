#pragma once

#include "Combat/SlopSim.h"
#include "Core/SlopSave.h"

class ASlopMatch;
class USlopGameInstance;

namespace Slop
{
	// A guide running inside a practice match: it may shape the second side's pads, it reads the
	// simulation's events, and it writes prompts on the HUD.
	class IGuide
	{
	public:
		virtual ~IGuide() = default;
		virtual void FilterPads(FPadState Pads[2]) {}
		virtual void OnEvent(const FSimEvent& E) {}
		virtual void OnTick() {}
		// Tab / R1 in the match.
		virtual void Skip() {}
		virtual FText Prompt() const = 0;
	};

	// The tutorial (Ashka against Gorm): one prompt at a time, each checked against what actually
	// happens. Gorm attacks on cue for the blocking steps; the last step asks for a knockout and a
	// Sloptality.
	class FTutorial : public IGuide
	{
	public:
		FTutorial(ASlopMatch* InMatch, USlopGameInstance* InGI);
		virtual void FilterPads(FPadState Pads[2]) override;
		virtual void OnEvent(const FSimEvent& E) override;
		virtual void OnTick() override;
		virtual void Skip() override { Next(); }
		virtual FText Prompt() const override { return PromptText; }
		bool IsDone() const { return bDone; }

	private:
		void Next();
		void ArmFinish();
		FText Keys(EGameAction A) const;
		double Reach(const FFighter& F, FName Move) const;
		ASlopMatch* M;
		USlopGameInstance* GI;
		int32 StepIndex = -1;
		int32 Count = 0;
		TSet<FName> Seen;
		int32 T = 0;
		bool bDone = false;
		int32 RetryAt = -1;
		FText PromptText;
	};

	// Combo trials against the fighter's rival: one at a time, its inputs on screen, each hit
	// ticked off as it lands; a dropped combo starts over; a completed one is saved.
	class FTrialsRunner : public IGuide
	{
	public:
		FTrialsRunner(ASlopMatch* InMatch, USlopGameInstance* InGI);
		virtual void OnEvent(const FSimEvent& E) override;
		virtual void OnTick() override;
		virtual void Skip() override;
		virtual FText Prompt() const override { return PromptText; }

	private:
		void Begin();
		void Show(const FString& Note = FString());
		void Advance();
		ASlopMatch* M;
		USlopGameInstance* GI;
		FName Fighter;
		struct FTrial { FString Name; TArray<FString> Steps; TArray<FName> Moves; };
		TArray<FTrial> Trials;
		int32 Index = 0;
		FTrialCheck Check;
		int32 BusyUntil = -1;
		FText PromptText;
	};
}
