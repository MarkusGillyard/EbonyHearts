#pragma once

#include "Combat/SlopCombatTypes.h"

namespace Slop
{
	// One fighter's input history, one entry per simulation tick. Directions are kept as seen on
	// screen (numpad notation: 6 right, 4 left, 8 up, 2 down) with the side the fighter faced on
	// that tick, so commands read relative to the fighter even across a crossover. Ticks frozen in
	// hit-stop are recorded but take no time: buffer and motion windows count only ticks the
	// fighter could move in, so a press made during hit-stop is never lost.
	class FInputBuffer
	{
	public:
		static constexpr int32 Size = 120;
		// Buttons pressed this many ticks apart still count as one chord.
		static constexpr int32 ChordTicks = 3;
		// A press is remembered this long while the fighter cannot act yet.
		static constexpr int32 BufferTicks = 6;

		FInputBuffer() { Clear(); }
		void Clear();
		void Push(int32 Screen, int32 Facing, int32 Held, int32 Pressed, bool bFrozen);

		int32 Num() const { return Count; }
		// Effective ages for raw ages 0..N-1: how many unfrozen ticks lie between now and each entry.
		void EffAges(int32 N, TArray<int32>& Out) const;

		int32 ScreenDir(int32 Age = 0) const;
		// Direction relative to the fighter as it faced on that tick (6 toward the opponent).
		int32 Dir(int32 Age = 0) const;
		// Direction relative to the fighter as if it had faced `Facing` on that tick.
		int32 DirAs(int32 Age, int32 Facing) const;
		int32 FacingAt(int32 Age) const;
		int32 Held(int32 Age = 0) const;
		int32 Pressed(int32 Age = 0) const;

		// Age of the most recent press of any button in `Button` within `Window` ticks, or -1.
		int32 PressAge(int32 Button, int32 Window = BufferTicks) const;
		// Age of the latest press when every button in `Mask` was pressed within ChordTicks of
		// the others inside `Window`; -1 when the chord is incomplete.
		int32 ChordAge(int32 Mask, int32 Window = BufferTicks) const;
		// Marks presses of `Mask` inside the window as used, so one press starts one move.
		void Consume(int32 Mask, int32 Window = BufferTicks);

		static int32 Rel(int32 Screen, int32 Facing);
		static int32 ScreenFrom(bool bRight, bool bLeft, bool bUp, bool bDown);
		static bool IsDown(int32 D) { return D <= 3; }
		static bool IsUp(int32 D) { return D >= 7; }
		static bool IsFwd(int32 D) { return D == 3 || D == 6 || D == 9; }
		static bool IsBack(int32 D) { return D == 1 || D == 4 || D == 7; }

	private:
		int32 Idx(int32 Age) const { return (Head - Age + Size * 2) % Size; }
		int32 DirS[Size];
		int32 FaceS[Size];
		int32 HeldS[Size];
		int32 PressedS[Size];
		uint8 FrozenS[Size];
		int32 Head = -1;
		int32 Count = 0;
	};

	// A move command in arcade notation, relative to the fighter:
	//   "HP"  "D+HP"  "B+LK"  "D,F+HP"  "B,B+LP"  "F,D,F+HK"  "B+LP+BL+LK"  "D,U+HK"
	// A single direction must be held when the buttons are pressed. Two or more directions are a
	// motion, tapped in order shortly before the buttons.
	struct FCommand
	{
		// The last direction of a motion may come this many ticks before the buttons (or one after).
		static constexpr int32 LastGap = 10;
		// Consecutive motion steps may be this many ticks apart.
		static constexpr int32 StepGap = 14;
		// The whole motion must fit in this many ticks.
		static constexpr int32 MotionWindow = 32;

		FString Text;
		TArray<int32> Steps;
		int32 Mask = 0;
		FName Move;
		bool bValid = false;

		// Parses notation; on failure bValid is false and Error says why.
		static FCommand Parse(const FString& T, FString* Error = nullptr);
		bool IsMotion() const { return Steps.Num() >= 2; }
		// Sort key: longer motions first, then chords, then held directions, then plain buttons.
		int32 Priority() const;
		// True when this command was entered with its buttons pressed `Age` ticks ago. Motions are
		// read both with the facing stored on each tick and with the current facing.
		bool Matches(const FInputBuffer& Buf, int32 Age, int32 FacingNow) const;

		// Held directions that satisfy a held-direction step; directions that satisfy a motion step.
		static bool HeldOk(int32 Step, int32 D);
		static bool StepOk(int32 Step, int32 D);
		static int32 BitCount(int32 M);
		// Readable notation for menus: "D,F+HP" becomes "↓ → + HP".
		static FString Pretty(const FString& T);

	private:
		bool Motion(const FInputBuffer& Buf, int32 Press, int32 Facing) const;
	};
}
