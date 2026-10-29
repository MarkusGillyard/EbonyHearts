#pragma once

#include "CoreMinimal.h"
#include "Combat/SlopCombatTypes.h"

class ASlopMatch;
class UStaticMeshComponent;

namespace Slop
{
	// Stages the finishers and the other set pieces: every Sloptality, the Rendering Yards' stage
	// finisher, the Fragment's defeat and the Grandmaster's instant kills. Each is timed to its move's frames
	// in the simulation, so skipping a finisher simply jumps to its end, and nothing here changes
	// the outcome. Gore "off" swaps blood for ash and dust. Everything is put back at the next round.
	class FFinisherDirector
	{
	public:
		void Setup(ASlopMatch* InMatch);
		void OnEvent(const FSimEvent& E);
		void Step(double Delta);

	private:
		void Reset();
		bool Once(const TCHAR* Key, double At);
		void CloseUp(const FVector& Mid, double Face, double Dist, double From, double To);
		void BossStep(double Delta);
		void KillStep(double Delta);
		static double EaseRange(double A, double B, double T);

		ASlopMatch* M = nullptr;
		bool bActive = false;
		FName MoveId;
		int32 Who = 0;
		double T = 0;
		TSet<FString> Fired;
		TWeakObjectPtr<UStaticMeshComponent> Pool;
		double BossT = -1;
		int32 Boss = -1;
		double KillT = -1;
		int32 KillBy = -1;
	};
}
