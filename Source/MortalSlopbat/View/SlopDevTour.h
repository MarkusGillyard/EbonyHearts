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

	private:
		void Place();
		FFightSim& Sim;
		int32 Index;
		FPadIntent Intent;
		int32 Wait = 30;
	};
}
