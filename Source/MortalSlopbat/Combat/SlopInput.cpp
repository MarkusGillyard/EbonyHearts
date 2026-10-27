#include "Combat/SlopInput.h"

namespace Slop
{
	void FInputBuffer::Clear()
	{
		for (int32 I = 0; I < Size; ++I)
		{
			DirS[I] = 5;
			FaceS[I] = 1;
			HeldS[I] = 0;
			PressedS[I] = 0;
			FrozenS[I] = 0;
		}
		Head = -1;
		Count = 0;
	}

	void FInputBuffer::Push(int32 Screen, int32 Facing, int32 InHeld, int32 InPressed, bool bFrozen)
	{
		Head = (Head + 1) % Size;
		DirS[Head] = Screen;
		FaceS[Head] = Facing;
		HeldS[Head] = InHeld;
		PressedS[Head] = InPressed;
		FrozenS[Head] = bFrozen ? 1 : 0;
		Count = FMath::Min(Count + 1, Size);
	}

	void FInputBuffer::EffAges(int32 N, TArray<int32>& Out) const
	{
		N = FMath::Min(N, Count);
		Out.SetNumUninitialized(N);
		int32 E = 0;
		for (int32 A = 0; A < N; ++A)
		{
			Out[A] = E;
			if (FrozenS[Idx(A)] == 0)
			{
				++E;
			}
		}
	}

	int32 FInputBuffer::ScreenDir(int32 Age) const { return Age >= Count ? 5 : DirS[Idx(Age)]; }

	int32 FInputBuffer::Dir(int32 Age) const
	{
		if (Age >= Count)
		{
			return 5;
		}
		const int32 I = Idx(Age);
		return Rel(DirS[I], FaceS[I]);
	}

	int32 FInputBuffer::DirAs(int32 Age, int32 Facing) const { return Age >= Count ? 5 : Rel(DirS[Idx(Age)], Facing); }
	int32 FInputBuffer::FacingAt(int32 Age) const { return Age >= Count ? 1 : FaceS[Idx(Age)]; }
	int32 FInputBuffer::Held(int32 Age) const { return Age >= Count ? 0 : HeldS[Idx(Age)]; }
	int32 FInputBuffer::Pressed(int32 Age) const { return Age >= Count ? 0 : PressedS[Idx(Age)]; }

	int32 FInputBuffer::PressAge(int32 Button, int32 Window) const
	{
		int32 E = 0;
		for (int32 A = 0; A < Count; ++A)
		{
			const int32 I = Idx(A);
			if (PressedS[I] & Button)
			{
				return A;
			}
			if (FrozenS[I] == 0)
			{
				++E;
				if (E >= Window)
				{
					break;
				}
			}
		}
		return -1;
	}

	int32 FInputBuffer::ChordAge(int32 Mask, int32 Window) const
	{
		int32 Latest = -1;
		int32 Earliest = -1;
		for (int32 B = 1; B <= BtnRN; B <<= 1)
		{
			if (Mask & B)
			{
				const int32 A = PressAge(B, Window);
				if (A < 0)
				{
					return -1;
				}
				Latest = Latest < 0 ? A : FMath::Min(Latest, A);
				Earliest = FMath::Max(Earliest, A);
			}
		}
		if (Earliest - Latest > ChordTicks)
		{
			return -1;
		}
		return Latest;
	}

	void FInputBuffer::Consume(int32 Mask, int32 Window)
	{
		int32 E = 0;
		for (int32 A = 0; A < Count; ++A)
		{
			const int32 I = Idx(A);
			PressedS[I] &= ~Mask;
			if (FrozenS[I] == 0)
			{
				++E;
				if (E >= Window)
				{
					break;
				}
			}
		}
	}

	int32 FInputBuffer::Rel(int32 Screen, int32 Facing)
	{
		if (Facing >= 0)
		{
			return Screen;
		}
		switch (Screen)
		{
		case 6: return 4;
		case 4: return 6;
		case 3: return 1;
		case 1: return 3;
		case 9: return 7;
		case 7: return 9;
		default: return Screen;
		}
	}

	int32 FInputBuffer::ScreenFrom(bool bRight, bool bLeft, bool bUp, bool bDown)
	{
		// Opposite directions held together cancel out (no "last pressed wins").
		const int32 X = (bRight && !bLeft) ? 1 : ((bLeft && !bRight) ? -1 : 0);
		const int32 Y = (bUp && !bDown) ? 1 : ((bDown && !bUp) ? -1 : 0);
		return 5 + X + 3 * Y;
	}

	// --- Commands ---------------------------------------------------------------------------------

	static int32 DirCode(const FString& D)
	{
		if (D == TEXT("F")) return 6;
		if (D == TEXT("B")) return 4;
		if (D == TEXT("U")) return 8;
		if (D == TEXT("D")) return 2;
		if (D == TEXT("DF")) return 3;
		if (D == TEXT("DB")) return 1;
		if (D == TEXT("UF")) return 9;
		if (D == TEXT("UB")) return 7;
		return 0;
	}

	static int32 ButtonCode(const FString& B)
	{
		if (B == TEXT("HP")) return BtnHP;
		if (B == TEXT("LP")) return BtnLP;
		if (B == TEXT("HK")) return BtnHK;
		if (B == TEXT("LK")) return BtnLK;
		if (B == TEXT("BL")) return BtnBL;
		if (B == TEXT("RN")) return BtnRN;
		return 0;
	}

	FCommand FCommand::Parse(const FString& T, FString* Error)
	{
		FCommand C;
		C.Text = T;
		FString S = T.TrimStartAndEnd().Replace(TEXT(" "), TEXT(""));
		int32 Plus = INDEX_NONE;
		S.FindChar(TEXT('+'), Plus);
		FString DirPart = S;
		FString BtnPart;
		if (Plus != INDEX_NONE)
		{
			DirPart = S.Left(Plus);
			BtnPart = S.Mid(Plus + 1);
		}
		if (ButtonCode(DirPart) != 0)
		{
			// No direction at all: "HP" or "HP+LP".
			BtnPart = S;
			DirPart.Reset();
		}
		auto Fail = [&](const FString& Why)
		{
			if (Error)
			{
				*Error = FString::Printf(TEXT("command '%s': %s"), *T, *Why);
			}
			C.bValid = false;
			return C;
		};
		if (!DirPart.IsEmpty())
		{
			TArray<FString> Parts;
			DirPart.ParseIntoArray(Parts, TEXT(","), false);
			for (const FString& D : Parts)
			{
				const int32 Code = DirCode(D);
				if (Code == 0)
				{
					return Fail(FString::Printf(TEXT("unknown direction '%s'"), *D));
				}
				C.Steps.Add(Code);
			}
		}
		if (!BtnPart.IsEmpty())
		{
			TArray<FString> Parts;
			BtnPart.ParseIntoArray(Parts, TEXT("+"), false);
			for (const FString& B : Parts)
			{
				const int32 Code = ButtonCode(B);
				if (Code == 0)
				{
					return Fail(FString::Printf(TEXT("unknown button '%s'"), *B));
				}
				C.Mask |= Code;
			}
		}
		if (C.Mask == 0)
		{
			return Fail(TEXT("no buttons"));
		}
		C.bValid = true;
		return C;
	}

	int32 FCommand::Priority() const
	{
		return Steps.Num() * 100 + BitCount(Mask) * 10 + (Steps.Num() == 1 ? 1 : 0);
	}

	int32 FCommand::BitCount(int32 M)
	{
		int32 N = 0;
		while (M)
		{
			N += M & 1;
			M >>= 1;
		}
		return N;
	}

	// Forward and back exclude the diagonals: down-forward and down-back are crouching,
	// up-forward and up-back are jumps.
	bool FCommand::HeldOk(int32 Step, int32 D)
	{
		switch (Step)
		{
		case 2: return D == 1 || D == 2 || D == 3;
		case 8: return D == 7 || D == 8 || D == 9;
		default: return D == Step;
		}
	}

	// Lenient: a rolled diagonal counts as the cardinal next to it.
	bool FCommand::StepOk(int32 Step, int32 D)
	{
		switch (Step)
		{
		case 6: return D == 3 || D == 6 || D == 9;
		case 4: return D == 1 || D == 4 || D == 7;
		case 2: return D == 1 || D == 2 || D == 3;
		case 8: return D == 7 || D == 8 || D == 9;
		default: return D == Step;
		}
	}

	bool FCommand::Matches(const FInputBuffer& Buf, int32 Age, int32 FacingNow) const
	{
		if (Steps.Num() == 0)
		{
			return true;
		}
		if (Steps.Num() == 1)
		{
			return HeldOk(Steps[0], Buf.DirAs(Age, FacingNow));
		}
		return Motion(Buf, Age, 0) || Motion(Buf, Age, FacingNow);
	}

	// `Facing` 0 reads each tick with the facing stored for it. Gaps are measured in effective
	// ticks (hit-stop takes no time).
	bool FCommand::Motion(const FInputBuffer& Buf, int32 Press, int32 Facing) const
	{
		auto Rd = [&Buf, Facing](int32 Age) { return Facing == 0 ? Buf.Dir(Age) : Buf.DirAs(Age, Facing); };
		const int32 N = Steps.Num();
		TArray<int32, TInlineAllocator<128>> Eff;
		{
			TArray<int32> Tmp;
			Buf.EffAges(Press + MotionWindow * 3, Tmp);
			Eff.Append(Tmp);
		}
		const int32 Limit = Eff.Num();
		if (Press >= Limit)
		{
			return false;
		}
		const int32 LastStep = Steps[N - 1];
		int32 At = -1;
		for (int32 A = FMath::Max(Press - 1, 0); A < Limit; ++A)
		{
			if (Eff[A] - Eff[Press] > LastGap)
			{
				break;
			}
			if (StepOk(LastStep, Rd(A)))
			{
				At = A;
				break;
			}
		}
		if (At < 0)
		{
			return false;
		}
		// The last direction must have been entered shortly before the press, not merely held.
		while (At + 1 < Limit && StepOk(LastStep, Rd(At + 1)))
		{
			++At;
		}
		if (Eff[At] - Eff[Press] > LastGap)
		{
			return false;
		}
		for (int32 K = N - 2; K >= 0; --K)
		{
			const int32 Want = Steps[K];
			const int32 Next = Steps[K + 1];
			const bool bSame = Want == Next;
			int32 Found = -1;
			bool bReleased = false;
			// Leave the later step first: its direction may still be held for a while.
			for (int32 A = At + 1; A < Limit && Eff[A] - Eff[At] <= StepGap && Eff[A] - Eff[Press] <= MotionWindow; ++A)
			{
				const int32 D = Rd(A);
				if (bSame)
				{
					if (!StepOk(Want, D))
					{
						bReleased = true;
					}
					else if (bReleased)
					{
						Found = A;
						break;
					}
				}
				else if (StepOk(Want, D) && !StepOk(Next, D))
				{
					Found = A;
					break;
				}
			}
			if (Found < 0)
			{
				return false;
			}
			At = Found;
		}
		return true;
	}

	FString FCommand::Pretty(const FString& T)
	{
		auto Arrow = [](const FString& D) -> FString
		{
			if (D == TEXT("F")) return TEXT("→");
			if (D == TEXT("B")) return TEXT("←");
			if (D == TEXT("U")) return TEXT("↑");
			if (D == TEXT("D")) return TEXT("↓");
			if (D == TEXT("DF")) return TEXT("↘");
			if (D == TEXT("DB")) return TEXT("↙");
			if (D == TEXT("UF")) return TEXT("↗");
			if (D == TEXT("UB")) return TEXT("↖");
			return D;
		};
		const FString S = T.Replace(TEXT(" "), TEXT(""));
		int32 Plus = INDEX_NONE;
		S.FindChar(TEXT('+'), Plus);
		const FString Dirs = Plus != INDEX_NONE ? S.Left(Plus) : S;
		if (ButtonCode(Dirs) != 0)
		{
			return S.Replace(TEXT("+"), TEXT(" + "));
		}
		TArray<FString> Parts;
		Dirs.ParseIntoArray(Parts, TEXT(","), false);
		TArray<FString> Out;
		for (const FString& D : Parts)
		{
			Out.Add(Arrow(D));
		}
		const FString Rest = Plus != INDEX_NONE ? S.Mid(Plus + 1).Replace(TEXT("+"), TEXT(" + ")) : FString();
		return FString::Join(Out, TEXT(" ")) + (Rest.IsEmpty() ? FString() : TEXT(" + ") + Rest);
	}
}
