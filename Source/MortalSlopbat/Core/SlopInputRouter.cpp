#include "Core/SlopInputRouter.h"
#include "HAL/PlatformTime.h"
#include "GenericPlatform/IInputInterface.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "Core/SlopLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Slop
{
	static constexpr float RepeatDelay = 0.38f;
	static constexpr float RepeatEvery = 0.085f;
	// The triggers count as pressed past this much travel (the digital trigger keys fire at a hair).
	static constexpr float TriggerOn = 0.4f;
	static constexpr float TriggerOff = 0.3f;

	int32 FSlopInputRouter::DirBit(EGameAction A) const
	{
		switch (A)
		{
		case EGameAction::Up: return PadUp;
		case EGameAction::Down: return PadDown;
		case EGameAction::Left: return PadLeft;
		case EGameAction::Right: return PadRight;
		default: return 0;
		}
	}

	static int32 ButtonBit(EGameAction A)
	{
		switch (A)
		{
		case EGameAction::HP: return BtnHP;
		case EGameAction::LP: return BtnLP;
		case EGameAction::HK: return BtnHK;
		case EGameAction::LK: return BtnLK;
		case EGameAction::BL: return BtnBL;
		case EGameAction::RN: return BtnRN;
		default: return 0;
		}
	}

	int32 FSlopInputRouter::SideOf(int32 Device) const
	{
		if (Device < 0 || Device >= NumDevices)
		{
			return -1;
		}
		return bSolo ? 0 : Settings.Assign[Device];
	}

	bool FSlopInputRouter::IgnoresPads()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		static const bool bNoPads = FParse::Param(FCommandLine::Get(), TEXT("SlopNoPads"));
		return bNoPads;
#endif
	}

	int32 FSlopInputRouter::PadSlotFor(FInputDeviceId Id)
	{
		if (IgnoresPads())
		{
			return -1;
		}
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			if (Devices[D].Id == Id && (Devices[D].bConnected || Devices[D].Id.IsValid()))
			{
				Devices[D].bConnected = true;
				return D;
			}
		}
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			if (!Devices[D].bConnected && !Devices[D].Id.IsValid())
			{
				Devices[D].Id = Id;
				Devices[D].bConnected = true;
				const TOptional<FInputDeviceDescriptor> Desc = FInputDeviceRegistry::FindDescriptor(Id);
				Devices[D].Name = Desc ? Desc->HardwareDeviceIdentifier.ToString() : FString(TEXT("Controller"));
				++Serial;
				UE_LOG(LogSlop, Log, TEXT("Controller slot %d: '%s' (device %d)"), D - 1, *Devices[D].Name, Id.GetId());
				return D;
			}
		}
		return -1;
	}

	void FSlopInputRouter::OnConnectionChanged(FInputDeviceId Id, bool bConnected)
	{
		int32 Slot = -1;
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			if (Devices[D].Id == Id)
			{
				Slot = D;
			}
		}
		if (bConnected)
		{
			if (Slot < 0)
			{
				PadSlotFor(Id);
			}
			else
			{
				Devices[Slot].bConnected = true;
				const TOptional<FInputDeviceDescriptor> Desc = FInputDeviceRegistry::FindDescriptor(Id);
				if (Desc)
				{
					Devices[Slot].Name = Desc->HardwareDeviceIdentifier.ToString();
				}
			}
		}
		else if (Slot >= 0)
		{
			// A pad that vanishes leaves nothing held.
			FDeviceState& S = Devices[Slot];
			S.Held = S.Pressed = S.DirKeys = S.DirStick = S.MenuStick = S.NavKeys = S.TrigHeld = 0;
			S.StickX = S.StickY = S.TrigL = S.TrigR = 0;
			S.bConnected = false;
		}
		++Serial;
		UE_LOG(LogSlop, Log, TEXT("Controller %s: device %d"), bConnected ? TEXT("connected") : TEXT("disconnected"), Id.GetId());
	}

	bool FSlopInputRouter::IsPadConnected(int32 Device) const { return Device >= (int32)EDevice::FirstPad && Device < NumDevices && Devices[Device].bConnected; }
	FString FSlopInputRouter::PadName(int32 Device) const { return IsPadConnected(Device) ? Devices[Device].Name : FString(); }

	int32 FSlopInputRouter::ConnectedPadCount() const
	{
		int32 N = 0;
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			N += Devices[D].bConnected ? 1 : 0;
		}
		return N;
	}

	EGlyphs FSlopInputRouter::Glyphs() const
	{
		if (LastUsed < (int32)EDevice::FirstPad)
		{
			return EGlyphs::Keyboard;
		}
		if (Settings.Glyphs == TEXT("playstation"))
		{
			return EGlyphs::PlayStation;
		}
		if (Settings.Glyphs == TEXT("xbox"))
		{
			return EGlyphs::Xbox;
		}
		const FString N = Devices[LastUsed].Name;
		for (const TCHAR* S : {TEXT("Sony"), TEXT("PlayStation"), TEXT("PS3"), TEXT("PS4"), TEXT("PS5"), TEXT("DualSense"), TEXT("DualShock")})
		{
			if (N.Contains(S))
			{
				return EGlyphs::PlayStation;
			}
		}
		return EGlyphs::Xbox;
	}

	int32 FSlopInputRouter::DeviceForKey(const FKeyEvent& E, const FKey& K) const
	{
		if (K.IsGamepadKey())
		{
			return const_cast<FSlopInputRouter*>(this)->PadSlotFor(E.GetInputDeviceId());
		}
		for (int32 H = 0; H < 2; ++H)
		{
			for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
			{
				if (Settings.Keys[H][A] == K)
				{
					return H;
				}
			}
		}
		return (int32)EDevice::KeyboardLeft;
	}

	void FSlopInputRouter::PushUi(EUi A, int32 Device, bool bRepeat)
	{
		FUiEvent E;
		E.Action = A;
		E.Device = Device;
		E.Side = SideOf(Device);
		E.bRepeat = bRepeat;
		Ui.Add(E);
		if (!bRepeat && (A == EUi::Up || A == EUi::Down || A == EUi::Left || A == EUi::Right))
		{
			RepeatDevice = Device;
			RepeatAction = A;
			RepeatT = RepeatDelay;
		}
	}

	bool FSlopInputRouter::HandleButton(int32 Device, const FKey& K, bool bDown, bool bRepeat)
	{
		if (Device < 0)
		{
			return false;
		}
		FDeviceState& S = Devices[Device];
		const bool bPad = Device >= (int32)EDevice::FirstPad;
		bool bUsed = false;
		// Fighting actions.
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			const bool bMatch = bPad ? Settings.PadButtons[A] == K : Settings.Keys[Device][A] == K;
			if (!bMatch)
			{
				continue;
			}
			bUsed = true;
			const int32 Dir = DirBit((EGameAction)A);
			const int32 Btn = ButtonBit((EGameAction)A);
			if (bDown)
			{
				if (!bRepeat)
				{
					S.DirKeys |= Dir;
					S.Held |= Btn;
					S.Pressed |= Btn;
				}
			}
			else
			{
				S.DirKeys &= ~Dir;
				S.Held &= ~Btn;
			}
		}
		// Pads: the D-pad always gives directions, whatever the buttons are bound to.
		if (bPad)
		{
			int32 Dir = 0;
			if (K == EKeys::Gamepad_DPad_Up) Dir = PadUp;
			else if (K == EKeys::Gamepad_DPad_Down) Dir = PadDown;
			else if (K == EKeys::Gamepad_DPad_Left) Dir = PadLeft;
			else if (K == EKeys::Gamepad_DPad_Right) Dir = PadRight;
			if (Dir)
			{
				bUsed = true;
				S.DirKeys = bDown ? (S.DirKeys | Dir) : (S.DirKeys & ~Dir);
			}
		}
		if (!bDown)
		{
			// Any release of a navigation key ends that direction's menu repeat.
			int32 Nav = 0;
			if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up || (!bPad && K == Settings.Keys[Device][(int32)EGameAction::Up])) Nav = PadUp;
			else if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down || (!bPad && K == Settings.Keys[Device][(int32)EGameAction::Down])) Nav = PadDown;
			else if (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || (!bPad && K == Settings.Keys[Device][(int32)EGameAction::Left])) Nav = PadLeft;
			else if (K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || (!bPad && K == Settings.Keys[Device][(int32)EGameAction::Right])) Nav = PadRight;
			if (Nav)
			{
				S.NavKeys &= ~Nav;
				bUsed = true;
			}
			return bUsed;
		}
		if (bRepeat)
		{
			return bUsed;
		}
		// Menu actions.
		auto Menu = [&](EUi A)
		{
			PushUi(A, Device, false);
			bUsed = true;
			S.NavKeys |= A == EUi::Up ? PadUp : A == EUi::Down ? PadDown : A == EUi::Left ? PadLeft : A == EUi::Right ? PadRight : 0;
		};
		if (bPad)
		{
			if (K == EKeys::Gamepad_DPad_Up) Menu(EUi::Up);
			else if (K == EKeys::Gamepad_DPad_Down) Menu(EUi::Down);
			else if (K == EKeys::Gamepad_DPad_Left) Menu(EUi::Left);
			else if (K == EKeys::Gamepad_DPad_Right) Menu(EUi::Right);
			else if (K == EKeys::Gamepad_FaceButton_Bottom) Menu(EUi::Confirm);
			else if (K == EKeys::Gamepad_FaceButton_Right) Menu(EUi::Back);
			else if (K == EKeys::Gamepad_Special_Right) Menu(EUi::Pause);
			else if (K == EKeys::Gamepad_Special_Left) Menu(EUi::Reset);
			else if (K == EKeys::Gamepad_FaceButton_Top) Menu(EUi::Alt);
			else if (!bFightActive && K == EKeys::Gamepad_RightShoulder) Menu(EUi::Next);
			else if (!bFightActive && K == EKeys::Gamepad_LeftShoulder) Menu(EUi::Prev);
			// In a fight the shoulders fight; the right stick's click (never a fighting button)
			// changes the dummy, skips a tutorial step or moves to the next trial.
			else if (K == EKeys::Gamepad_RightThumbstick) Menu(EUi::Next);
		}
		else
		{
			const FKey* Keys = Settings.Keys[Device];
			if (K == EKeys::Up || K == Keys[(int32)EGameAction::Up]) Menu(EUi::Up);
			else if (K == EKeys::Down || K == Keys[(int32)EGameAction::Down]) Menu(EUi::Down);
			else if (K == EKeys::Left || K == Keys[(int32)EGameAction::Left]) Menu(EUi::Left);
			else if (K == EKeys::Right || K == Keys[(int32)EGameAction::Right]) Menu(EUi::Right);
			else if (K == EKeys::Enter || K == EKeys::SpaceBar || (!bFightActive && (K == Keys[(int32)EGameAction::LP] || K == Keys[(int32)EGameAction::HP])))
				Menu(EUi::Confirm);
			// Each half's Block key goes back for that half, as Backspace does (the original's
			// "Block or Backspace"), so the right-hand player can undo a pick too.
			else if (K == EKeys::BackSpace || (!bFightActive && K == Keys[(int32)EGameAction::BL])) Menu(EUi::Back);
			else if (K == EKeys::Escape) Menu(bFightActive ? EUi::Pause : EUi::Back);
			else if (K == EKeys::Tab) Menu(EUi::Next);
			else if (!bFightActive && K == Keys[(int32)EGameAction::HK]) Menu(EUi::Alt);
		}
		return bUsed;
	}

	bool FSlopInputRouter::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		const FKey K = InKeyEvent.GetKey();
		// The engine's own keys pass through: the console, and Alt+Enter / F11 full screen.
		if (K == EKeys::Tilde || K == EKeys::F11 || (K == EKeys::Enter && InKeyEvent.IsAltDown()))
		{
			return false;
		}
		// The digital trigger keys fire at a hair of travel; the trigger axes are read instead.
		if (K == EKeys::Gamepad_LeftTrigger || K == EKeys::Gamepad_RightTrigger)
		{
			return true;
		}
		if (bCapturing)
		{
			if (!InKeyEvent.IsRepeat())
			{
				FCapture C;
				C.Key = K;
				C.Device = K.IsGamepadKey() ? PadSlotFor(InKeyEvent.GetInputDeviceId()) : (int32)EDevice::KeyboardLeft;
				Captured = C;
			}
			return true;
		}
		static const FKey DebugList[] = {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8};
		for (const FKey& D : DebugList)
		{
			if (K == D)
			{
				if (!InKeyEvent.IsRepeat())
				{
					DebugKeys.Add(K);
				}
				return true;
			}
		}
		const int32 Device = DeviceForKey(InKeyEvent, K);
		if (Device >= 0)
		{
			LastUsed = Device;
		}
		return HandleButton(Device, K, true, InKeyEvent.IsRepeat());
	}

	bool FSlopInputRouter::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		const FKey K = InKeyEvent.GetKey();
		if (K == EKeys::Tilde || K == EKeys::F11)
		{
			return false;
		}
		if (bCapturing)
		{
			return true;
		}
		// Release on every device that has this key (a key bound in both halves cannot happen,
		// but a release must never be lost).
		bool bUsed = false;
		if (K.IsGamepadKey())
		{
			bUsed = HandleButton(PadSlotFor(InKeyEvent.GetInputDeviceId()), K, false, false);
		}
		else
		{
			bUsed |= HandleButton(0, K, false, false);
			bUsed |= HandleButton(1, K, false, false);
		}
		return bUsed;
	}

	void FSlopInputRouter::UpdateStickDirs(FDeviceState& D)
	{
		const float X = D.StickX;
		const float Y = D.StickY;
		const float Mag = FMath::Sqrt(X * X + Y * Y);
		int32 Dirs = 0;
		if (Mag >= Settings.DeadZone)
		{
			// Eight equal sectors: a direction counts once the stick is within 67.5 degrees of it.
			const float NX = X / Mag;
			const float NY = Y / Mag;
			if (NX > 0.383f) Dirs |= PadRight;
			if (NX < -0.383f) Dirs |= PadLeft;
			if (NY > 0.383f) Dirs |= PadUp;
			if (NY < -0.383f) Dirs |= PadDown;
		}
		D.DirStick = Dirs;
	}

	bool FSlopInputRouter::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& E)
	{
		const FKey K = E.GetKey();
		if (!K.IsGamepadKey())
		{
			return false;
		}
		const int32 Device = PadSlotFor(E.GetInputDeviceId());
		if (Device < 0)
		{
			return true;
		}
		FDeviceState& S = Devices[Device];
		const float V = E.GetAnalogValue();
		if (K == EKeys::Gamepad_LeftX || K == EKeys::Gamepad_LeftY)
		{
			(K == EKeys::Gamepad_LeftX ? S.StickX : S.StickY) = V;
			UpdateStickDirs(S);
			// Menus take only the stick's dominant direction, so an angled push down a list never
			// also changes a value; fights keep all eight (DirStick).
			int32 Menu = 0;
			if (FMath::Sqrt(S.StickX * S.StickX + S.StickY * S.StickY) >= Settings.DeadZone)
			{
				Menu = FMath::Abs(S.StickX) > FMath::Abs(S.StickY) ? (S.StickX > 0 ? PadRight : PadLeft) : (S.StickY > 0 ? PadUp : PadDown);
			}
			if (Menu != S.MenuStick)
			{
				S.MenuStick = Menu;
				if (Menu)
				{
					LastUsed = Device;
					PushUi(Menu == PadUp ? EUi::Up : Menu == PadDown ? EUi::Down : Menu == PadLeft ? EUi::Left : EUi::Right, Device, false);
				}
			}
			if (S.DirStick == 0 && Device == RepeatDevice && (S.DirKeys == 0))
			{
				RepeatDevice = -1;
			}
			return true;
		}
		if (K == EKeys::Gamepad_LeftTriggerAxis || K == EKeys::Gamepad_RightTriggerAxis)
		{
			const bool bLeft = K == EKeys::Gamepad_LeftTriggerAxis;
			float& T = bLeft ? S.TrigL : S.TrigR;
			const int32 Bit = bLeft ? BtnRN : BtnBL;
			const bool bWas = T >= TriggerOn;
			const bool bNow = bWas ? V > TriggerOff : V >= TriggerOn;
			T = bNow ? FMath::Max(V, TriggerOn) : FMath::Min(V, TriggerOff - 0.01f);
			if (bNow && !bWas)
			{
				LastUsed = Device;
				S.TrigHeld |= Bit;
				S.Pressed |= Bit;
			}
			else if (!bNow && bWas)
			{
				// Only what the trigger holds is released; a shoulder may hold the same action.
				S.TrigHeld &= ~Bit;
			}
			return true;
		}
		return true;
	}

	bool FSlopInputRouter::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		LastUsed = (int32)EDevice::KeyboardLeft;
		return false;
	}

	void FSlopInputRouter::SetPadRumble(int32 Device, float Strength)
	{
		if (!FSlateApplication::IsInitialized() || !Devices[Device].Id.IsValid())
		{
			return;
		}
		if (IInputInterface* In = FSlateApplication::Get().GetInputInterface())
		{
			FForceFeedbackValues V;
			V.LeftLarge = V.RightLarge = Strength;
			V.LeftSmall = V.RightSmall = Strength * 0.6f;
			// The platform addresses force feedback by controller id, mapped from the device as the
			// Linux application itself maps it.
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			int32 ControllerId = INDEX_NONE;
			Mapper.RemapUserAndDeviceToControllerId(Mapper.GetUserForInputDevice(Devices[Device].Id), ControllerId, Devices[Device].Id);
			if (ControllerId != INDEX_NONE)
			{
				In->SetForceFeedbackChannelValues(ControllerId, V);
			}
		}
	}

	void FSlopInputRouter::Rumble(int32 Side, float Strength, float Seconds)
	{
		if (!Settings.bVibration)
		{
			return;
		}
		const double Until = FPlatformTime::Seconds() + Seconds;
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			const bool bTheirs = bSolo ? D == LastUsed : Settings.Assign[D] == Side;
			if (Devices[D].bConnected && bTheirs)
			{
				SetPadRumble(D, FMath::Clamp(Strength, 0.f, 1.f));
				RumbleUntil[D] = FMath::Max(RumbleUntil[D], Until);
			}
		}
	}

	void FSlopInputRouter::StopRumble()
	{
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			if (RumbleUntil[D] > 0.0)
			{
				RumbleUntil[D] = 0.0;
				SetPadRumble(D, 0.f);
			}
		}
	}

	void FSlopInputRouter::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
	{
		const double Now = FPlatformTime::Seconds();
		for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
		{
			if (RumbleUntil[D] > 0.0 && Now >= RumbleUntil[D])
			{
				RumbleUntil[D] = 0.0;
				SetPadRumble(D, 0.f);
			}
		}
		if (RepeatDevice >= 0)
		{
			const FDeviceState& S = Devices[RepeatDevice];
			const int32 Dirs = S.DirKeys | S.DirStick;
			int32 Need = 0;
			switch (RepeatAction)
			{
			case EUi::Up: Need = PadUp; break;
			case EUi::Down: Need = PadDown; break;
			case EUi::Left: Need = PadLeft; break;
			case EUi::Right: Need = PadRight; break;
			default: break;
			}
			const bool bHeld = ((S.DirKeys | S.MenuStick | S.NavKeys) & Need) != 0;
			if (!bHeld)
			{
				RepeatDevice = -1;
			}
			else if ((RepeatT -= DeltaTime) <= 0.0f)
			{
				RepeatT = RepeatEvery;
				FUiEvent E;
				E.Action = RepeatAction;
				E.Device = RepeatDevice;
				E.Side = SideOf(RepeatDevice);
				E.bRepeat = true;
				Ui.Add(E);
			}
		}
	}

	FPadState FSlopInputRouter::Poll(int32 Side)
	{
		FPadState P;
		for (int32 D = 0; D < NumDevices; ++D)
		{
			if (SideOf(D) != Side)
			{
				continue;
			}
			FDeviceState& S = Devices[D];
			P.Dirs |= S.DirKeys | S.DirStick;
			P.Held |= S.Held | S.TrigHeld;
			P.Pressed |= S.Pressed;
			S.Pressed = 0;
		}
		return P;
	}

	FPadState FSlopInputRouter::Peek(int32 Side) const
	{
		FPadState P;
		for (int32 D = 0; D < NumDevices; ++D)
		{
			if (SideOf(D) == Side)
			{
				const FDeviceState& S = Devices[D];
				P.Dirs |= S.DirKeys | S.DirStick;
				P.Held |= S.Held | S.TrigHeld;
				P.Pressed |= S.Pressed;
			}
		}
		return P;
	}

	void FSlopInputRouter::ResetAll()
	{
		for (FDeviceState& S : Devices)
		{
			S.Held = S.Pressed = S.DirKeys = S.DirStick = S.MenuStick = S.NavKeys = S.TrigHeld = 0;
			S.StickX = S.StickY = 0;
			S.TrigL = S.TrigR = 0;
		}
		RepeatDevice = -1;
	}

	TArray<FUiEvent> FSlopInputRouter::TakeUi()
	{
		TArray<FUiEvent> Out = MoveTemp(Ui);
		Ui.Reset();
		return Out;
	}

	TArray<FKey> FSlopInputRouter::TakeDebugKeys()
	{
		TArray<FKey> Out = MoveTemp(DebugKeys);
		DebugKeys.Reset();
		return Out;
	}
}
