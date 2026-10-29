#pragma once

#include "AI/SlopAI.h"

namespace Slop
{
	// Development: performs every ground command of a fighter against a standing dummy, one at a
	// time from a set distance, first from the left side and then from the right, so each move
	// can be inspected (and screenshotted) from the gameplay camera with its boxes drawn.
	class FMoveTour : public IBrain
	{
	public:
		FMoveTour(FFightSim& InSim, int32 InIndex) : Sim(InSim), Index(InIndex) {}
		virtual FPadState Think() override;
		bool Done() const { return Pass >= 2; }
		int32 Pass = 0;
		int32 Step = 0;
		FString Current;
		// Distance between the two at each move (-SlopTourGap=; projectiles are seen in flight
		// from further off).
		double Gap = 1.1;

	private:
		void Place();
		FFightSim& Sim;
		int32 Index;
		FPadIntent Intent;
		int32 Wait = 30;
	};

	// Development: plays a script against a standing opponent (-SlopScript=, steps separated by
	// ";"), to look at moves on screen. A command ("D+RN+LP") is entered when the fighter can act,
	// or at once with a leading "~" (a link, or a change of shape inside a combo); "hit" waits
	// until the last command's blow has landed and its hitstop is over; "fight" until a round is
	// under way and the fighter free; "w<n>" waits n ticks;
	// "shot:<name>" takes a screenshot; "qhi:<n>" sets the meter; "place:<m>" stands the two m
	// apart, facing each other, free.
	class FScriptBrain : public IBrain
	{
	public:
		FScriptBrain(FFightSim& InSim, int32 InIndex, const FString& Script, TFunction<void(const FString&)> InShot);
		virtual FPadState Think() override;
		bool Done() const { return At >= Steps.Num(); }

	private:
		FFightSim& Sim;
		int32 Index;
		TArray<FString> Steps;
		int32 At = 0;
		int32 Wait = 0;
		int32 Waited = 0;
		int32 HitsBefore = 0;
		FPadIntent Intent;
		TFunction<void(const FString&)> Shot;
	};
}
