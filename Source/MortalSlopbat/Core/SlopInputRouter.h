#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Combat/SlopCombatTypes.h"
#include "Core/SlopSave.h"

namespace Slop
{
	// What menus are told. Each carries the device it came from and the side that device drives.
	enum class EUi : uint8 { Up, Down, Left, Right, Confirm, Back, Pause, Next, Prev, Reset, Alt, Count };

	struct FUiEvent
	{
		EUi Action = EUi::Confirm;
		int32 Device = 0;
		int32 Side = 0;
		bool bRepeat = false;
	};

	// A raw press seen while a binding is being captured.
	struct FCapture
	{
		FKey Key;
		int32 Device = -1;
	};

	enum class EGlyphs : uint8 { Keyboard, PlayStation, Xbox };

	// Every key and pad event passes through here before Slate or the game sees it. The keyboard is
	// split into its two arcade-panel halves; each gamepad is its own device. The router keeps each
	// side's pad state for the fighting simulation (held buttons, presses since the last tick) and
	// turns presses into menu actions for whichever screen is up. It consumes what it uses, so no
	// press reaches the game twice.
	class FSlopInputRouter : public IInputProcessor, public TSharedFromThis<FSlopInputRouter>
	{
	public:
		explicit FSlopInputRouter(FSlopSettings& InSettings) : Settings(InSettings) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
		virtual const TCHAR* GetDebugName() const override { return TEXT("SlopInputRouter"); }

		// In one-player modes every device drives player 1.
		void SetSolo(bool bOn) { bSolo = bOn; }
		bool IsSolo() const { return bSolo; }
		int32 SideOf(int32 Device) const;
		// The pad state for one side for this simulation tick; clears the presses it reports.
		FPadState Poll(int32 Side);
		// What the side holds right now (for the input display), without consuming presses.
		FPadState Peek(int32 Side) const;
		// Forgets everything held and pressed (pause, focus loss, a pad going away).
		void ResetAll();
		// Menu actions since the last call, oldest first.
		TArray<FUiEvent> TakeUi();
		// While capturing, the next press of any key or button is reported here and nothing else
		// happens with it (rebinding). Escape cancels a keyboard capture, Start a pad capture.
		void BeginCapture() { bCapturing = true; Captured.Reset(); }
		void EndCapture() { bCapturing = false; }
		bool IsCapturing() const { return bCapturing; }
		TOptional<FCapture> TakeCapture() { TOptional<FCapture> C = Captured; Captured.Reset(); return C; }

		// Devices: 0 and 1 are the keyboard halves, 2.. are pads in the order they were first seen.
		int32 PadSlotFor(FInputDeviceId Id);
		// Development: -SlopNoPads leaves physical controllers out entirely, so scripted menu
		// flows (which expect the two keyboard halves alone) run the same whatever is plugged in.
		static bool IgnoresPads();
		bool IsPadConnected(int32 Device) const;
		FString PadName(int32 Device) const;
		int32 ConnectedPadCount() const;
		// The prompt style for what was used last (or what the setting forces).
		EGlyphs Glyphs() const;
		int32 LastDevice() const { return LastUsed; }
		// Shakes the pads that play a side (in a one-player mode, the pad last used) for a moment,
		// when vibration is on. Ticked by Tick.
		void Rumble(int32 Side, float Strength, float Seconds);
		void StopRumble();
		// Counts connection changes so screens can notice them.
		int32 ConnectionSerial() const { return Serial; }
		void OnConnectionChanged(FInputDeviceId Id, bool bConnected);
		// Set by the match while a fight is running, so menu keys don't double as attacks.
		bool bFightActive = false;
		// Development keys (F1..F8) seen this frame.
		TArray<FKey> TakeDebugKeys();

	private:
		struct FDeviceState
		{
			int32 Held = 0;
			int32 Pressed = 0;
			// Digital directions (keys, D-pad) and the stick's, kept apart so either can release.
			int32 DirKeys = 0;
			int32 DirStick = 0;
			// The stick's one dominant direction, for menus.
			int32 MenuStick = 0;
			// Menu directions held (any source), for auto-repeat; buttons held by the triggers.
			int32 NavKeys = 0;
			int32 TrigHeld = 0;
			float StickX = 0, StickY = 0;
			float TrigL = 0, TrigR = 0;
			FInputDeviceId Id;
			FString Name;
			bool bConnected = false;
		};
		int32 DeviceForKey(const FKeyEvent& E, const FKey& K) const;
		bool HandleButton(int32 Device, const FKey& K, bool bDown, bool bRepeat);
		void UpdateStickDirs(FDeviceState& D);
		void PushUi(EUi A, int32 Device, bool bRepeat);
		int32 DirBit(EGameAction A) const;

		FSlopSettings& Settings;
		FDeviceState Devices[NumDevices];
		bool bSolo = true;
		TArray<FUiEvent> Ui;
		bool bCapturing = false;
		TOptional<FCapture> Captured;
		int32 LastUsed = 0;
		void SetPadRumble(int32 Device, float Strength);
		// Per device: when its rumble stops (0 when still).
		double RumbleUntil[NumDevices] = {};
		int32 Serial = 0;
		TArray<FKey> DebugKeys;
		// Menu auto-repeat for held directions.
		int32 RepeatDevice = -1;
		EUi RepeatAction = EUi::Up;
		float RepeatT = 0;
	};
}
