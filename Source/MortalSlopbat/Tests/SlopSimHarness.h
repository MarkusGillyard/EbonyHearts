#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/SlopSim.h"

namespace Slop::Test
{
	constexpr int32 R = PadRight;
	constexpr int32 L = PadLeft;
	constexpr int32 U = PadUp;
	constexpr int32 D = PadDown;
	constexpr int32 HP = BtnHP;
	constexpr int32 LP = BtnLP;
	constexpr int32 HK = BtnHK;
	constexpr int32 LK = BtnLK;
	constexpr int32 BL = BtnBL;
	constexpr int32 RN = BtnRN;

	// Builds deterministic matches for tests and drives them with scripted device states.
	struct FSimHarness
	{
		FFightSim Sim;
		// Every event emitted while stepping, oldest first.
		TArray<FSimEvent> Log;
		// Buttons held down for each side (block, run), on top of each tick's presses.
		int32 Hold[2] = {0, 0};
		TArray<TSharedPtr<FFighterDef>> Owned;

		// A plain fighter made only of the shared basics plus a projectile, a low projectile, a
		// grab and a finisher. `ExtraJson` replaces whole top-level keys of the definition.
		static TSharedPtr<FFighterDef> DummyDef(const FString& Id, const FString& ExtraJson = FString())
		{
			FString Json = FString::Printf(TEXT(R"({
				"id": "%s", "name": "%s", "stats": {"height": 1.8, "health": 1000},
				"moves": {
					"orb": {"kind": "special", "startup": 10, "active": 0, "recovery": 20, "level": "none",
						"projectile": {"frame": 10, "kind": "orb", "offset": [0.6, 1.3], "speed": 8.0, "size": [0.3, 0.3], "life": 100,
							"level": "high", "damage": 60, "chip": 10}},
					"low_orb": {"kind": "special", "startup": 10, "active": 0, "recovery": 20, "level": "none",
						"projectile": {"frame": 10, "kind": "orb", "offset": [0.6, 0.25], "speed": 8.0, "size": [0.3, 0.3], "life": 100,
							"level": "low", "damage": 50, "chip": 10}},
					"grab": {"kind": "grab", "startup": 6, "active": 3, "recovery": 30, "level": "throw",
						"hitboxes": [{"rect": [0.2, 0.8, 0.6, 0.6]}],
						"throw": {"frames": 30, "damage_frame": 20, "damage": 150, "release": [2.0, 3.0]}},
					"fin": {"kind": "finisher", "total": 120}
				},
				"commands": [["D,F+HP", "orb"], ["D,B+LP", "low_orb"], ["B,F+LP", "grab"]],
				"finisher": {"input": "F,F,D+HP", "range": "close", "move": "fin"}
			})"), *Id, *Id);
			TSharedPtr<FJsonObject> Obj = FContent::ParseJson(Json);
			if (!ExtraJson.IsEmpty())
			{
				if (TSharedPtr<FJsonObject> Extra = FContent::ParseJson(ExtraJson))
				{
					for (const auto& KV : Extra->Values)
					{
						Obj->SetField(FString(*KV.Key), KV.Value);
					}
				}
			}
			return GetContent().ParseFighter(Obj);
		}

		explicit FSimHarness(const FFighterDef* D1 = nullptr, const FFighterDef* D2 = nullptr, bool bTraining = true)
		{
			if (!D1)
			{
				Owned.Add(DummyDef(TEXT("p1")));
				D1 = Owned.Last().Get();
			}
			if (!D2)
			{
				Owned.Add(DummyDef(TEXT("p2")));
				D2 = Owned.Last().Get();
			}
			Sim.bTraining = bTraining;
			Sim.bIntro = false;
			Sim.Setup(D1, D2);
			// Skip the round announcement.
			const FPadState Zero[2];
			while (Sim.Phase != EPhase::Fight)
			{
				Sim.Step(Zero);
			}
		}

		FFighter& F(int32 I) { return Sim.Fighters[I]; }

		// Places both fighters `Gap` metres apart around `Centre`, facing each other, at rest.
		void Place(double Gap, double Centre = 0.0)
		{
			F(0).Pos = FVector2D(Centre - Gap * 0.5, 0);
			F(1).Pos = FVector2D(Centre + Gap * 0.5, 0);
			F(0).Facing = 1;
			F(1).Facing = -1;
			for (FFighter& X : Sim.Fighters)
			{
				X.Move = nullptr;
				X.SetState(EState::Idle);
				X.Vel = FVector2D::ZeroVector;
				X.PushV = 0.0;
				X.Hitstop = 0;
				X.Stun = 0;
				X.Input.Clear();
				X.ProjectilesOut = 0;
			}
			Sim.Projectiles.Reset();
		}

		// One tick: directions held as bits and buttons pressed this tick (plus Hold).
		void Step(int32 P1Dirs = 0, int32 P1Press = 0, int32 P2Dirs = 0, int32 P2Press = 0)
		{
			FPadState Pads[2];
			Pads[0] = {P1Dirs, P1Press | Hold[0], P1Press};
			Pads[1] = {P2Dirs, P2Press | Hold[1], P2Press};
			Sim.Step(Pads);
			Log.Append(Sim.Events);
		}

		void Idle(int32 N, int32 P1Dirs = 0, int32 P2Dirs = 0)
		{
			for (int32 I = 0; I < N; ++I)
			{
				Step(P1Dirs, 0, P2Dirs, 0);
			}
		}

		// Taps screen directions for fighter `Who`, one tick each, then presses `Btn` holding the last.
		void Motion(int32 Who, std::initializer_list<int32> Dirs, int32 Btn)
		{
			int32 Last = 0;
			for (int32 DD : Dirs)
			{
				Who == 0 ? Step(DD, 0, 0, 0) : Step(0, 0, DD, 0);
				Last = DD;
			}
			Who == 0 ? Step(Last, Btn, 0, 0) : Step(0, 0, Last, Btn);
		}

		const FSimEvent* Find(EEvent Type, int32 Since = 0) const
		{
			for (int32 I = Since; I < Log.Num(); ++I)
			{
				if (Log[I].Type == Type)
				{
					return &Log[I];
				}
			}
			return nullptr;
		}

		int32 Count(EEvent Type, int32 Since = 0) const
		{
			int32 N = 0;
			for (int32 I = Since; I < Log.Num(); ++I)
			{
				N += Log[I].Type == Type ? 1 : 0;
			}
			return N;
		}

		bool Started(int32 Who, FName Move, int32 Since = 0) const
		{
			for (int32 I = Since; I < Log.Num(); ++I)
			{
				if (Log[I].Type == EEvent::Move && Log[I].Who == Who && Log[I].Move == Move)
				{
					return true;
				}
			}
			return false;
		}

		// Steps until `Cond` holds or `Limit` ticks pass; returns whether it held.
		template <typename FCond>
		bool Until(FCond Cond, int32 Limit = 400, int32 P1Dirs = 0, int32 P2Dirs = 0)
		{
			for (int32 I = 0; I < Limit; ++I)
			{
				if (Cond())
				{
					return true;
				}
				Step(P1Dirs, 0, P2Dirs, 0);
			}
			return Cond();
		}

		FName MoveId(int32 I) { return F(I).Move ? F(I).Move->Id : NAME_None; }
	};
}

#endif
