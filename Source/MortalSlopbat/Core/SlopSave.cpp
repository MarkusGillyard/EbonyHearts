#include "Core/SlopSave.h"
#include "HAL/PlatformFileManager.h"
#include "Core/SlopLog.h"
#include "Combat/SlopData.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "SlopSave"

namespace Slop
{
	const TCHAR* ActionId(EGameAction A)
	{
		static const TCHAR* Ids[] = {TEXT("up"), TEXT("down"), TEXT("left"), TEXT("right"), TEXT("hp"), TEXT("lp"), TEXT("hk"), TEXT("lk"), TEXT("bl"), TEXT("rn")};
		return Ids[(int32)A];
	}

	FText ActionLabel(EGameAction A)
	{
		switch (A)
		{
		case EGameAction::Up: return LOCTEXT("Up", "Up (jump)");
		case EGameAction::Down: return LOCTEXT("Down", "Down (crouch)");
		case EGameAction::Left: return LOCTEXT("Left", "Left");
		case EGameAction::Right: return LOCTEXT("Right", "Right");
		case EGameAction::HP: return LOCTEXT("HP", "High Punch");
		case EGameAction::LP: return LOCTEXT("LP", "Low Punch");
		case EGameAction::HK: return LOCTEXT("HK", "High Kick");
		case EGameAction::LK: return LOCTEXT("LK", "Low Kick");
		case EGameAction::BL: return LOCTEXT("BL", "Block");
		case EGameAction::RN: return LOCTEXT("RN", "Run");
		default: return FText::GetEmpty();
		}
	}

	FString ProfileSuffix()
	{
		FString Profile;
		if (FParse::Value(FCommandLine::Get(), TEXT("profile="), Profile) && !Profile.IsEmpty())
		{
			return TEXT("_") + FPaths::MakeValidFileName(Profile);
		}
		return FString();
	}

	FString SaveDir()
	{
		FString Base = FPlatformMisc::GetEnvironmentVariable(TEXT("XDG_DATA_HOME"));
		if (Base.IsEmpty())
		{
			Base = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME")) / TEXT(".local/share");
		}
		return Base / TEXT("MortalSlopbat/UE5");
	}

	bool WriteFileAtomic(const FString& Path, const FString& Text)
	{
		// Written beside the old file, flushed to the disk, then renamed over it in one step
		// (rename(2)), so a crash at any moment leaves either the old file or the new one.
		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		PF.CreateDirectoryTree(*FPaths::GetPath(Path));
		const FString Tmp = Path + TEXT(".tmp");
		const FTCHARToUTF8 Utf8(*Text);
		{
			TUniquePtr<IFileHandle> H(PF.OpenWrite(*Tmp));
			if (!H || !H->Write(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()) || !H->Flush(true))
			{
				UE_LOG(LogSlop, Error, TEXT("Cannot write %s"), *Tmp);
				return false;
			}
		}
		if (!PF.MoveFile(*Path, *Tmp))
		{
			UE_LOG(LogSlop, Error, TEXT("Cannot replace %s"), *Path);
			return false;
		}
		return true;
	}

	// A write interrupted between the two steps leaves only the flushed temporary file.
	static void RecoverInterruptedWrite(const FString& Path)
	{
		const FString Tmp = Path + TEXT(".tmp");
		if (!FPaths::FileExists(Path) && FPaths::FileExists(Tmp))
		{
			FPlatformFileManager::Get().GetPlatformFile().MoveFile(*Path, *Tmp);
		}
	}

	static FString ToText(const TSharedRef<FJsonObject>& J)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(J, W);
		return Out;
	}

	// --- Settings ----------------------------------------------------------------------------------

	FSlopSettings::FSlopSettings() { ResetBindings(); }

	void FSlopSettings::ResetBindings()
	{
		const FKey L[] = {EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::U, EKeys::J, EKeys::O, EKeys::L, EKeys::I, EKeys::K};
		const FKey R[] = {EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::NumPadSeven, EKeys::NumPadFour, EKeys::NumPadNine,
			EKeys::NumPadSix, EKeys::NumPadEight, EKeys::NumPadFive};
		// Standard layout: top face button high punch, left low punch, right high kick, bottom low
		// kick, right shoulder block, left shoulder run (the triggers also block and run).
		const FKey P[] = {EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right,
			EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Bottom,
			EKeys::Gamepad_RightShoulder, EKeys::Gamepad_LeftShoulder};
		for (int32 I = 0; I < (int32)EGameAction::Count; ++I)
		{
			Keys[0][I] = L[I];
			Keys[1][I] = R[I];
			PadButtons[I] = P[I];
		}
	}

	void FSlopSettings::ResetAll()
	{
		*this = FSlopSettings();
	}

	void FSlopSettings::RebindKey(int32 Half, EGameAction Action, const FKey& Key)
	{
		const FKey Old = Keys[Half][(int32)Action];
		for (int32 H = 0; H < 2; ++H)
		{
			for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
			{
				if (Keys[H][A] == Key && !(H == Half && A == (int32)Action))
				{
					Keys[H][A] = Old;
				}
			}
		}
		Keys[Half][(int32)Action] = Key;
	}

	bool FSlopSettings::IsReservedPadButton(const FKey& K)
	{
		// Pause, reset and "next" (the right stick's click), the D-pad and the sticks' directions
		// (which the platform also reports as buttons) are never fighting buttons.
		return K == EKeys::Gamepad_Special_Right || K == EKeys::Gamepad_Special_Left || K == EKeys::Gamepad_RightThumbstick
			|| K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_DPad_Right
			|| K == EKeys::Gamepad_LeftStick_Up || K == EKeys::Gamepad_LeftStick_Down || K == EKeys::Gamepad_LeftStick_Left || K == EKeys::Gamepad_LeftStick_Right
			|| K == EKeys::Gamepad_RightStick_Up || K == EKeys::Gamepad_RightStick_Down || K == EKeys::Gamepad_RightStick_Left || K == EKeys::Gamepad_RightStick_Right
			|| !K.IsGamepadKey() || K.IsAnalog();
	}

	bool FSlopSettings::IsReservedKey(const FKey& K)
	{
		// The menus' and the window's keys, and the training and debug function keys.
		return K == EKeys::Enter || K == EKeys::BackSpace || K == EKeys::Escape || K == EKeys::Tab || K == EKeys::SpaceBar
			|| K == EKeys::Tilde || K == EKeys::F11 || K == EKeys::F1 || K == EKeys::F2 || K == EKeys::F3 || K == EKeys::F4
			|| K == EKeys::F5 || K == EKeys::F6 || K == EKeys::F7 || K == EKeys::F8;
	}

	bool FSlopSettings::RebindPad(EGameAction Action, const FKey& Button)
	{
		// Pad directions are the D-pad and the stick, always; only buttons are rebound.
		if (IsReservedPadButton(Button) || DirBitOf(Action))
		{
			return false;
		}
		const FKey Old = PadButtons[(int32)Action];
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			if (PadButtons[A] == Button && A != (int32)Action)
			{
				PadButtons[A] = Old;
			}
		}
		PadButtons[(int32)Action] = Button;
		return true;
	}

	FString FSlopSettings::Path() { return SaveDir() / (TEXT("settings") + ProfileSuffix() + TEXT(".json")); }

	TSharedRef<FJsonObject> FSlopSettings::ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("version"), Version);
		J->SetNumberField(TEXT("window_mode"), WindowMode);
		J->SetNumberField(TEXT("res_x"), Resolution.X);
		J->SetNumberField(TEXT("res_y"), Resolution.Y);
		J->SetBoolField(TEXT("vsync"), bVSync);
		J->SetNumberField(TEXT("frame_cap"), FrameCap);
		J->SetBoolField(TEXT("dynamic_res"), bDynamicRes);
		J->SetStringField(TEXT("graphics"), Graphics);
		J->SetNumberField(TEXT("ui_scale"), UIScale);
		J->SetNumberField(TEXT("master_volume"), MasterVolume);
		J->SetNumberField(TEXT("music_volume"), MusicVolume);
		J->SetNumberField(TEXT("effects_volume"), EffectsVolume);
		J->SetNumberField(TEXT("voice_volume"), VoiceVolume);
		J->SetBoolField(TEXT("screen_shake"), bScreenShake);
		J->SetBoolField(TEXT("flashes"), bFlashes);
		J->SetStringField(TEXT("effects"), Effects);
		J->SetStringField(TEXT("gore"), Gore);
		J->SetBoolField(TEXT("post_fx"), bPostFx);
		J->SetBoolField(TEXT("motion_smoothing"), bMotionSmoothing);
		J->SetBoolField(TEXT("input_display"), bInputDisplay);
		J->SetNumberField(TEXT("difficulty"), Difficulty);
		J->SetNumberField(TEXT("versus_rounds"), VersusRounds);
		J->SetNumberField(TEXT("round_seconds"), RoundSeconds);
		J->SetNumberField(TEXT("dead_zone"), DeadZone);
		J->SetBoolField(TEXT("vibration"), bVibration);
		J->SetStringField(TEXT("glyphs"), Glyphs);
		for (int32 H = 0; H < 2; ++H)
		{
			TSharedRef<FJsonObject> K = MakeShared<FJsonObject>();
			for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
			{
				K->SetStringField(ActionId((EGameAction)A), Keys[H][A].GetFName().ToString());
			}
			J->SetObjectField(H == 0 ? TEXT("keys_left") : TEXT("keys_right"), K);
		}
		TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			P->SetStringField(ActionId((EGameAction)A), PadButtons[A].GetFName().ToString());
		}
		J->SetObjectField(TEXT("pad"), P);
		TArray<TSharedPtr<FJsonValue>> As;
		for (int32 D = 0; D < NumDevices; ++D)
		{
			As.Add(MakeShared<FJsonValueNumber>(Assign[D]));
		}
		J->SetArrayField(TEXT("assign"), As);
		return J;
	}

	void FSlopSettings::FromJson(const TSharedPtr<FJsonObject>& J)
	{
		if (!J.IsValid())
		{
			return;
		}
		const FSlopSettings D;
		WindowMode = FMath::Clamp(Json::Int(J, TEXT("window_mode"), D.WindowMode), 0, 2);
		const int32 RX = Json::Int(J, TEXT("res_x"), D.Resolution.X);
		const int32 RY = Json::Int(J, TEXT("res_y"), D.Resolution.Y);
		Resolution = (RX >= 640 && RY >= 360 && RX <= 7680 && RY <= 4320) ? FIntPoint(RX, RY) : D.Resolution;
		bVSync = Json::Bool(J, TEXT("vsync"), D.bVSync);
		FrameCap = Json::Int(J, TEXT("frame_cap"), D.FrameCap);
		// Version 1 wrote its default cap, 60, into every file; follow the display instead.
		if (Json::Int(J, TEXT("version"), 1) < 2 && FrameCap == 60)
		{
			FrameCap = D.FrameCap;
		}
		if (FrameCap != -1 && FrameCap != 0 && FrameCap != 30 && FrameCap != 60 && FrameCap != 120 && FrameCap != 144)
		{
			FrameCap = D.FrameCap;
		}
		bDynamicRes = Json::Bool(J, TEXT("dynamic_res"), D.bDynamicRes);
		const FString G = Json::Str(J, TEXT("graphics"), D.Graphics);
		Graphics = (G == TEXT("low") || G == TEXT("high") || G == TEXT("epic")) ? G : D.Graphics;
		UIScale = FMath::Clamp(Json::Num(J, TEXT("ui_scale"), D.UIScale), 0.85, 1.25);
		MasterVolume = FMath::Clamp(Json::Num(J, TEXT("master_volume"), D.MasterVolume), 0.0, 1.0);
		MusicVolume = FMath::Clamp(Json::Num(J, TEXT("music_volume"), D.MusicVolume), 0.0, 1.0);
		EffectsVolume = FMath::Clamp(Json::Num(J, TEXT("effects_volume"), D.EffectsVolume), 0.0, 1.0);
		VoiceVolume = FMath::Clamp(Json::Num(J, TEXT("voice_volume"), D.VoiceVolume), 0.0, 1.0);
		bScreenShake = Json::Bool(J, TEXT("screen_shake"), D.bScreenShake);
		bFlashes = Json::Bool(J, TEXT("flashes"), D.bFlashes);
		const FString E = Json::Str(J, TEXT("effects"), D.Effects);
		Effects = (E == TEXT("full") || E == TEXT("reduced")) ? E : D.Effects;
		const FString Go = Json::Str(J, TEXT("gore"), D.Gore);
		Gore = (Go == TEXT("full") || Go == TEXT("low") || Go == TEXT("off")) ? Go : D.Gore;
		bPostFx = Json::Bool(J, TEXT("post_fx"), D.bPostFx);
		bMotionSmoothing = Json::Bool(J, TEXT("motion_smoothing"), D.bMotionSmoothing);
		bInputDisplay = Json::Bool(J, TEXT("input_display"), D.bInputDisplay);
		Difficulty = FMath::Clamp(Json::Int(J, TEXT("difficulty"), D.Difficulty), 0, 2);
		VersusRounds = FMath::Clamp(Json::Int(J, TEXT("versus_rounds"), D.VersusRounds), 1, 5);
		const int32 Secs = Json::Int(J, TEXT("round_seconds"), D.RoundSeconds);
		RoundSeconds = (Secs == 0 || Secs == 30 || Secs == 60 || Secs == 90) ? Secs : D.RoundSeconds;
		DeadZone = FMath::Clamp(Json::Num(J, TEXT("dead_zone"), D.DeadZone), 0.05, 0.6);
		bVibration = Json::Bool(J, TEXT("vibration"), D.bVibration);
		const FString Gl = Json::Str(J, TEXT("glyphs"), D.Glyphs);
		Glyphs = (Gl == TEXT("auto") || Gl == TEXT("playstation") || Gl == TEXT("xbox")) ? Gl : D.Glyphs;
		for (int32 H = 0; H < 2; ++H)
		{
			const TSharedPtr<FJsonObject> K = Json::Obj(J, H == 0 ? TEXT("keys_left") : TEXT("keys_right"));
			for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
			{
				const FKey Key(*Json::Str(K, ActionId((EGameAction)A), FString()));
				if (Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton() && !IsReservedKey(Key))
				{
					Keys[H][A] = Key;
				}
			}
		}
		const TSharedPtr<FJsonObject> P = Json::Obj(J, TEXT("pad"));
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			const FKey Key(*Json::Str(P, ActionId((EGameAction)A), FString()));
			if (Key.IsValid() && !IsReservedPadButton(Key) && !DirBitOf((EGameAction)A))
			{
				PadButtons[A] = Key;
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* As = Json::Arr(J, TEXT("assign")))
		{
			for (int32 D2 = 0; D2 < FMath::Min(As->Num(), NumDevices); ++D2)
			{
				const int32 Side = (int32)(*As)[D2]->AsNumber();
				if (Side >= -1 && Side <= 1)
				{
					Assign[D2] = Side;
				}
			}
		}
		// A file with duplicate keys (edited by hand) falls back to the defaults for the bindings.
		TSet<FKey> Seen;
		bool bDup = false;
		for (int32 H = 0; H < 2; ++H)
		{
			for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
			{
				bool bAlready = false;
				Seen.Add(Keys[H][A], &bAlready);
				bDup |= bAlready;
			}
		}
		TSet<FKey> SeenPad;
		for (int32 A = 0; A < (int32)EGameAction::Count; ++A)
		{
			bool bAlready = false;
			SeenPad.Add(PadButtons[A], &bAlready);
			bDup |= bAlready;
		}
		if (bDup)
		{
			UE_LOG(LogSlop, Warning, TEXT("Settings had one key bound to two actions; bindings reset to the defaults"));
			ResetBindings();
		}
	}

	bool FSlopSettings::Load()
	{
		const FString P = Path();
		RecoverInterruptedWrite(P);
		if (!FPaths::FileExists(P))
		{
			return false;
		}
		const TSharedPtr<FJsonObject> J = FContent::LoadJson(P);
		if (!J.IsValid() || Json::Int(J, TEXT("version"), 0) < 1)
		{
			// Unreadable: kept aside, and the defaults are used (and saved on the next change).
			UE_LOG(LogSlop, Warning, TEXT("Settings file unreadable; using defaults and keeping the old one as .bad"));
			FPlatformFileManager::Get().GetPlatformFile().MoveFile(*(P + TEXT(".bad")), *P);
			LoadNote = TEXT("The settings file could not be read; it was kept aside and the defaults are in use.");
			return false;
		}
		FromJson(J);
		return true;
	}

	bool FSlopSettings::Save() const { return WriteFileAtomic(Path(), ToText(ToJson())); }

	// --- Progress ------------------------------------------------------------------------------------

	FString FSlopProgress::Path() { return SaveDir() / (TEXT("progress") + ProfileSuffix() + TEXT(".json")); }

	void FSlopProgress::Reset()
	{
		Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("version"), Version);
		Data->SetObjectField(TEXT("arcade"), MakeShared<FJsonObject>());
		Data->SetObjectField(TEXT("survival"), MakeShared<FJsonObject>());
		Data->SetObjectField(TEXT("trials"), MakeShared<FJsonObject>());
		Data->SetArrayField(TEXT("endings"), {});
		Data->SetArrayField(TEXT("unlocks"), {});
		Data->SetNumberField(TEXT("stranger"), 0);
		TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
		for (const TCHAR* S : {TEXT("matches"), TEXT("wins"), TEXT("finishers"), TEXT("flawless")})
		{
			Stats->SetNumberField(S, 0);
		}
		Data->SetObjectField(TEXT("stats"), Stats);
		Data->SetBoolField(TEXT("tutorial_done"), false);
	}

	void FSlopProgress::Load()
	{
		Reset();
		LoadNote.Reset();
		const FString P = Path();
		RecoverInterruptedWrite(P);
		if (!FPaths::FileExists(P))
		{
			// First run of this version: bring over what the original game recorded, read-only.
			const FString Old = FPaths::GetPath(SaveDir()) / (TEXT("progress") + ProfileSuffix() + TEXT(".json"));
			if (FPaths::FileExists(Old))
			{
				if (TSharedPtr<FJsonObject> OldJ = FContent::LoadJson(Old))
				{
					MergeOriginal(OldJ);
					LoadNote = TEXT("Progress from the earlier version was brought over.");
					Save();
				}
			}
			return;
		}
		const TSharedPtr<FJsonObject> J = FContent::LoadJson(P);
		if (!J.IsValid() || Json::Int(J, TEXT("version"), 0) < 1)
		{
			UE_LOG(LogSlop, Warning, TEXT("Progress file unreadable; starting fresh and keeping the old one as .bad"));
			IFileManager::Get().Move(*(P + TEXT(".bad")), *P, true, true);
			LoadNote = TEXT("The progress file could not be read; it was kept aside and a fresh one started.");
			return;
		}
		const int32 FileVersion = Json::Int(J, TEXT("version"), 0);
		if (FileVersion > Version)
		{
			// Written by a later version: what this one understands is read, and the file itself
			// is copied aside once before this version first saves over it.
			const FString Keep = FString::Printf(TEXT("%s.v%d"), *P, FileVersion);
			if (!FPaths::FileExists(Keep))
			{
				FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Keep, *P);
			}
			LoadNote = TEXT("The progress file is from a later version; a copy was kept beside it.");
		}
		// Take only fields of the expected types, so a damaged entry cannot break the game.
		for (const TCHAR* K : {TEXT("arcade"), TEXT("survival"), TEXT("trials"), TEXT("stats")})
		{
			if (const TSharedPtr<FJsonObject> O = Json::Obj(J, K))
			{
				Data->SetObjectField(K, O);
			}
		}
		for (const TCHAR* K : {TEXT("endings"), TEXT("unlocks")})
		{
			if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(J, K))
			{
				TArray<TSharedPtr<FJsonValue>> Clean;
				for (const TSharedPtr<FJsonValue>& V : *A)
				{
					if (V->Type == EJson::String)
					{
						Clean.Add(V);
					}
				}
				Data->SetArrayField(K, Clean);
			}
		}
		Data->SetNumberField(TEXT("stranger"), FMath::Clamp(Json::Int(J, TEXT("stranger"), 0), 0, 6));
		Data->SetBoolField(TEXT("tutorial_done"), Json::Bool(J, TEXT("tutorial_done"), false));
	}

	void FSlopProgress::MergeOriginal(const TSharedPtr<FJsonObject>& Old)
	{
		// The original's format is this one's version 1, so the same fields carry over as they are.
		for (const TCHAR* K : {TEXT("arcade"), TEXT("survival"), TEXT("trials"), TEXT("stats")})
		{
			if (const TSharedPtr<FJsonObject> O = Json::Obj(Old, K))
			{
				Data->SetObjectField(K, O);
			}
		}
		for (const TCHAR* K : {TEXT("endings"), TEXT("unlocks")})
		{
			if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(Old, K))
			{
				Data->SetArrayField(K, *A);
			}
		}
		Data->SetNumberField(TEXT("stranger"), FMath::Clamp(Json::Int(Old, TEXT("stranger"), 0), 0, 6));
		Data->SetBoolField(TEXT("tutorial_done"), Json::Bool(Old, TEXT("tutorial_done"), false));
		Data->SetBoolField(TEXT("imported"), true);
	}

	bool FSlopProgress::Save() const { return WriteFileAtomic(Path(), ToText(Data.ToSharedRef())); }

	static TSharedPtr<FJsonObject> Sub(const TSharedPtr<FJsonObject>& D, const TCHAR* K)
	{
		TSharedPtr<FJsonObject> O = Json::Obj(D, K);
		if (!O.IsValid())
		{
			O = MakeShared<FJsonObject>();
			D->SetObjectField(K, O);
		}
		return O;
	}

	static bool ArrHas(const TSharedPtr<FJsonObject>& D, const TCHAR* K, const FString& V)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(D, K))
		{
			for (const TSharedPtr<FJsonValue>& X : *A)
			{
				if (X->AsString() == V)
				{
					return true;
				}
			}
		}
		return false;
	}

	static void ArrAdd(const TSharedPtr<FJsonObject>& D, const TCHAR* K, const FString& V)
	{
		if (ArrHas(D, K, V))
		{
			return;
		}
		TArray<TSharedPtr<FJsonValue>> A;
		if (const TArray<TSharedPtr<FJsonValue>>* Old = Json::Arr(D, K))
		{
			A = *Old;
		}
		A.Add(MakeShared<FJsonValueString>(V));
		D->SetArrayField(K, A);
	}

	void FSlopProgress::RecordArcade(FName Fighter, int32 Difficulty, double Seconds, int32 Score)
	{
		const TSharedPtr<FJsonObject> A = Sub(Data, TEXT("arcade"));
		const FString Key = FString::Printf(TEXT("%s:%d"), *Fighter.ToString(), Difficulty);
		const TSharedPtr<FJsonObject> Old = Json::Obj(A, *Key);
		const double Best = Json::Num(Old, TEXT("best"), 0.0);
		TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
		N->SetNumberField(TEXT("clears"), Json::Int(Old, TEXT("clears"), 0) + 1);
		N->SetNumberField(TEXT("best"), Best <= 0.0 ? Seconds : FMath::Min(Best, Seconds));
		N->SetNumberField(TEXT("score"), FMath::Max(Json::Int(Old, TEXT("score"), 0), Score));
		A->SetObjectField(Key, N);
		ArrAdd(Data, TEXT("endings"), TEXT("ending:") + Fighter.ToString());
		Unlock(TEXT("costume:") + Fighter.ToString());
		Unlock(TEXT("arena:roost"));
		Save();
	}

	void FSlopProgress::RaiseArcadeScore(FName Fighter, int32 Difficulty, int32 Score)
	{
		const TSharedPtr<FJsonObject> A = Sub(Data, TEXT("arcade"));
		const TSharedPtr<FJsonObject> Rec = Json::Obj(A, *FString::Printf(TEXT("%s:%d"), *Fighter.ToString(), Difficulty));
		if (Rec.IsValid() && Score > Json::Int(Rec, TEXT("score"), 0))
		{
			Rec->SetNumberField(TEXT("score"), Score);
		}
		Save();
	}

	bool FSlopProgress::RecordSurvival(FName Fighter, int32 Waves)
	{
		const TSharedPtr<FJsonObject> S = Sub(Data, TEXT("survival"));
		const bool bBest = Waves > Json::Int(S, *Fighter.ToString(), 0);
		if (bBest)
		{
			S->SetNumberField(Fighter.ToString(), Waves);
		}
		Save();
		return bBest;
	}

	bool FSlopProgress::TrialDone(FName Fighter, const FString& TrialName)
	{
		const TSharedPtr<FJsonObject> T = Sub(Data, TEXT("trials"));
		if (ArrHas(T, *Fighter.ToString(), TrialName))
		{
			return false;
		}
		ArrAdd(T, *Fighter.ToString(), TrialName);
		Save();
		return true;
	}

	TArray<FString> FSlopProgress::TrialsDone(FName Fighter) const
	{
		TArray<FString> Out;
		if (const TArray<TSharedPtr<FJsonValue>>* A = Json::Arr(Json::Obj(Data, TEXT("trials")), *Fighter.ToString()))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				Out.Add(V->AsString());
			}
		}
		return Out;
	}

	int32 FSlopProgress::BestSurvival(FName Fighter) const { return Json::Int(Json::Obj(Data, TEXT("survival")), *Fighter.ToString(), 0); }

	int32 FSlopProgress::BestScore(FName Fighter, int32 Difficulty) const
	{
		return Json::Int(Json::Obj(Json::Obj(Data, TEXT("arcade")), *FString::Printf(TEXT("%s:%d"), *Fighter.ToString(), Difficulty)), TEXT("score"), 0);
	}

	double FSlopProgress::BestTime(FName Fighter, int32 Difficulty) const
	{
		return Json::Num(Json::Obj(Json::Obj(Data, TEXT("arcade")), *FString::Printf(TEXT("%s:%d"), *Fighter.ToString(), Difficulty)), TEXT("best"), 0.0);
	}

	int32 FSlopProgress::Clears(FName Fighter, int32 Difficulty) const
	{
		return Json::Int(Json::Obj(Json::Obj(Data, TEXT("arcade")), *FString::Printf(TEXT("%s:%d"), *Fighter.ToString(), Difficulty)), TEXT("clears"), 0);
	}

	bool FSlopProgress::Cleared(FName Fighter, int32 Difficulty) const
	{
		const TSharedPtr<FJsonObject> A = Json::Obj(Data, TEXT("arcade"));
		if (!A.IsValid())
		{
			return false;
		}
		for (const auto& KV : A->Values)
		{
			FString Who, Level;
			if (FString(*KV.Key).Split(TEXT(":"), &Who, &Level) && Who == Fighter.ToString() && (Difficulty < 0 || FCString::Atoi(*Level) == Difficulty))
			{
				return true;
			}
		}
		return false;
	}

	bool FSlopProgress::AnyCleared() const
	{
		const TSharedPtr<FJsonObject> A = Json::Obj(Data, TEXT("arcade"));
		return A.IsValid() && A->Values.Num() > 0;
	}

	void FSlopProgress::Unlock(const FString& What) { ArrAdd(Data, TEXT("unlocks"), What); }
	bool FSlopProgress::IsUnlocked(const FString& What) const { return ArrHas(Data, TEXT("unlocks"), What); }
	bool FSlopProgress::EndingSeen(FName Fighter) const { return ArrHas(Data, TEXT("endings"), TEXT("ending:") + Fighter.ToString()); }

	void FSlopProgress::Count(const FString& S, int32 N)
	{
		const TSharedPtr<FJsonObject> St = Sub(Data, TEXT("stats"));
		St->SetNumberField(S, Json::Int(St, *S, 0) + N);
	}

	int32 FSlopProgress::Stat(const FString& S) const { return Json::Int(Json::Obj(Data, TEXT("stats")), *S, 0); }

	void FSlopProgress::SetStranger(int32 Stage)
	{
		if (Stage > Stranger())
		{
			Data->SetNumberField(TEXT("stranger"), FMath::Min(Stage, 6));
			Save();
		}
	}

	int32 FSlopProgress::Stranger() const { return Json::Int(Data, TEXT("stranger"), 0); }
	bool FSlopProgress::TutorialDone() const { return Json::Bool(Data, TEXT("tutorial_done"), false); }

	void FSlopProgress::SetTutorialDone()
	{
		Data->SetBoolField(TEXT("tutorial_done"), true);
		Save();
	}
}

#undef LOCTEXT_NAMESPACE
