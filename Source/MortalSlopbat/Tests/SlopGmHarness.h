#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SlopSimHarness.h"

// Shared by the Grandmaster's tests (SlopRosterTests.cpp, SlopFormTests.cpp).
namespace Slop::Test
{
inline const FFighterDef* Fighter(const TCHAR* Id) { return GetContent().Fighter(FName(Id)); }

// Damage the right-hand fighter has taken.
inline int32 Taken(FSimHarness& H) { return H.F(1).MaxHealth() - H.F(1).Health; }

// A match against `Opponent` with real rules (no infinite meters), the Grandmaster on the left.
struct FGmMatch : FSimHarness
{
	explicit FGmMatch(const TCHAR* Opponent, const TCHAR* Self = TEXT("grandmaster")) : FSimHarness(Fighter(Self), Fighter(Opponent), false)
	{
		Sim.RoundLength = 0;
		Place(3.0);
	}
	FFighter& Gm() { return F(0); }
	// D,B,F + Block: Borrowed Face (and, in a borrowed shape, Own Face).
	void Shapeshift() { Motion(0, {D, D | L, L, R}, BL); }
	const FSimEvent* FormEvent(FName Kind, int32 Since = 0) const
	{
		for (int32 I = Since; I < Log.Num(); ++I)
		{
			if (Log[I].Type == EEvent::Form && Log[I].Kind == Kind)
			{
				return &Log[I];
			}
		}
		return nullptr;
	}
	bool Borrow()
	{
		Shapeshift();
		return Until([this]() { return Gm().Form == EForm::Borrowed; }, 60);
	}
	// The opponent's high punch from where it lands.
	int32 JabDamageOnGm()
	{
		Place(0.7);
		const int32 Was = Gm().Health;
		Step(0, 0, 0, HP);
		Idle(20);
		return Was - Gm().Health;
	}
};
}

#endif
