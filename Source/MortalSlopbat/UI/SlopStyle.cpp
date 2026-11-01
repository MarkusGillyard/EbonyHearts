#include "UI/SlopStyle.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Core/SlopGameInstance.h"

#define LOCTEXT_NAMESPACE "SlopUI"

namespace SlopUI
{
	const FLinearColor Ink(FColor(10, 7, 16));
	const FLinearColor Panel = FLinearColor(FColor(14, 9, 21)).CopyWithNewOpacity(0.84f);
	const FLinearColor Raised = FLinearColor(FColor(44, 26, 66)).CopyWithNewOpacity(0.9f);
	const FLinearColor Line = FLinearColor(FColor(133, 122, 138)).CopyWithNewOpacity(0.55f);
	const FLinearColor Text(FColor(232, 225, 236));
	const FLinearColor Dim(FColor(174, 162, 184));
	const FLinearColor Faint(FColor(108, 98, 116));
	const FLinearColor Accent(FColor(136, 77, 186));
	const FLinearColor Spark(FColor(210, 125, 242));
	const FLinearColor Second(FColor(124, 196, 214));
	const FLinearColor Warn(FColor(232, 168, 76));
	const FLinearColor Danger(FColor(218, 62, 72));
	const FLinearColor Good(FColor(126, 200, 160));
	const FLinearColor Qhi(1.f, 0.78f, 0.3f);
	const FLinearColor Run(FColor(96, 150, 232));

	static TSharedPtr<const FCompositeFont> LoadFont(const TCHAR* Name, const TCHAR* File)
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Fonts") / File;
		TSharedPtr<FStandaloneCompositeFont> F = MakeShared<FStandaloneCompositeFont>(FName(Name), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		// Arrows, button symbols and marks come from the engine's own fallback face; the game's faces
		// have letters, digits and common punctuation only.
		F->FallbackTypeface.Typeface.AppendFont(TEXT("Fallback"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return F;
	}

	static float TextScale = 1.f;
	static bool bReducedMotion = false;

	void SetTextScale(float Scale) { TextScale = FMath::Clamp(Scale, 0.85f, 1.25f); }

	// Sizes are written in pixels at 1080p; Slate's are points.
	static FSlateFontInfo Make(const TSharedPtr<const FCompositeFont>& F, float Size, int32 Tracking = 0)
	{
		FSlateFontInfo I(F, Size * 0.75f * TextScale);
		I.LetterSpacing = Tracking;
		return I;
	}

	// The faces are loaded once and kept for the life of the process (Slate holds them until exit).
	FSlateFontInfo Display(float Size)
	{
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("CinzelSemiBold"), TEXT("Cinzel-SemiBold.ttf")));
		return Make(*F, Size, 40);
	}

	FSlateFontInfo Call(float Size)
	{
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("CinzelBold"), TEXT("Cinzel-Bold.ttf")));
		return Make(*F, Size, 60);
	}

	FSlateFontInfo Body(float Size)
	{
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("SourceSans"), TEXT("SourceSans3-Regular.ttf")));
		return Make(*F, Size);
	}

	FSlateFontInfo Strong(float Size)
	{
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("SourceSansSemi"), TEXT("SourceSans3-SemiBold.ttf")));
		return Make(*F, Size);
	}

	FSlateFontInfo Caps(float Size)
	{
		FSlateFontInfo I = Strong(Size);
		I.LetterSpacing = 140;
		return I;
	}

	static UTexture2D* LoadUiTexture(const FString& Folder, const FString& Name)
	{
		UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (T)
		{
			T->AddToRoot();
		}
		return T;
	}

	const FSlateBrush* Brush(ETex T)
	{
		struct FDef { const TCHAR* Name; FVector2f Size; bool bSliced; bool bFlipped; };
		static const FDef Defs[(int32)ETex::Count] = {
			{TEXT("T_ui_logo"), {2172.f, 724.f}, false, false},
			{TEXT("T_ui_logo_energy"), {1086.f, 362.f}, false, false},
			{TEXT("T_ui_fade_side"), {1024.f, 64.f}, false, false},
			{TEXT("T_ui_fade_foot"), {64.f, 512.f}, false, false},
			{TEXT("T_ui_rule"), {1024.f, 8.f}, false, false},
			{TEXT("T_ui_spike"), {64.f, 64.f}, false, false},
			{TEXT("T_ui_spike_hollow"), {64.f, 64.f}, false, false},
			{TEXT("T_ui_frame"), {48.f, 48.f}, true, false},
			{TEXT("T_ui_bar_frame"), {880.f, 74.f}, false, false},
			{TEXT("T_ui_bar_frame"), {880.f, 74.f}, false, true},
			{TEXT("T_ui_timer_plate"), {150.f, 116.f}, false, false},
			{TEXT("T_ui_bar_fill"), {512.f, 24.f}, false, false},
			{TEXT("T_ui_band"), {1024.f, 256.f}, false, false},
			{TEXT("T_ui_vignette"), {512.f, 288.f}, false, false},
			{TEXT("T_ui_glow"), {256.f, 256.f}, false, false},
			{TEXT("T_ui_sweep"), {512.f, 64.f}, false, false},
		};
		// Kept for the life of the process, like the fonts: a handful of small textures.
		static TArray<TSharedPtr<FSlateBrush>>* Cache = new TArray<TSharedPtr<FSlateBrush>>();
		if (Cache->Num() == 0)
		{
			Cache->SetNum((int32)ETex::Count);
		}
		TSharedPtr<FSlateBrush>& B = (*Cache)[(int32)T];
		if (!B.IsValid())
		{
			const FDef& D = Defs[(int32)T];
			B = MakeShared<FSlateBrush>();
			if (UTexture2D* Tex = LoadUiTexture(TEXT("/Game/Slop/UI"), D.Name))
			{
				B->SetResourceObject(Tex);
				B->ImageSize = FVector2D(D.Size);
				B->DrawAs = D.bSliced ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
				B->Margin = D.bSliced ? FMargin(0.34f) : FMargin(0.f);
				B->Mirroring = D.bFlipped ? ESlateBrushMirrorType::Horizontal : ESlateBrushMirrorType::NoMirror;
			}
			else
			{
				B->DrawAs = ESlateBrushDrawType::NoDrawType;
			}
		}
		return B.Get();
	}

	const FSlateBrush* Box()
	{
		static FSlateColorBrush B(FLinearColor::White);
		return &B;
	}

	void SetReducedMotion(bool bReduced) { bReducedMotion = bReduced; }
	bool ReducedMotion() { return bReducedMotion; }
	float EaseOut(float T) { const float I = 1.f - FMath::Clamp(T, 0.f, 1.f); return 1.f - I * I * I; }
	float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float Span(float T, float From, float To) { return FMath::Clamp((T - From) / FMath::Max(To - From, 1e-4f), 0.f, 1.f); }

	float StageScale(const FVector2D& Local) { return FMath::Max(1e-3f, FMath::Min((float)Local.X / 1920.f, (float)Local.Y / 1080.f)); }
	FVector2D StageSize(const FVector2D& Local) { return Local / StageScale(Local); }

	static float ScreenMargin = 0.f;

	void SetScreenMargin(float Share) { ScreenMargin = FMath::Clamp(Share, 0.f, 0.05f); }

	FStageFrame StageFrame(const FVector2D& Local)
	{
		FStageFrame F;
		F.Origin = Local * ScreenMargin;
		const FVector2D Inner = Local - F.Origin * 2.0;
		F.Scale = StageScale(Inner);
		F.Size = Inner / F.Scale;
		return F;
	}

	FText StageName(FName Id)
	{
		if (Id == TEXT("rendering_yards")) return LOCTEXT("Yards", "The Rendering Yards");
		if (Id == TEXT("drowned_chapel")) return LOCTEXT("Chapel", "The Drowned Chapel");
		if (Id == TEXT("stilt_market")) return LOCTEXT("Market", "Lowmere Stilt Market");
		if (Id == TEXT("roost")) return LOCTEXT("Roost", "The Roost");
		if (Id == TEXT("palace_gate")) return LOCTEXT("Palace", "The Fragmented Palace");
		if (Id == TEXT("heartwater")) return LOCTEXT("Heartwater", "The Heartwater");
		return FText::FromName(Id);
	}

	FText StageBlurb(FName Id)
	{
		if (Id == TEXT("rendering_yards")) return LOCTEXT("YardsB", "Tallowmarket's iron floor between great glowing vats, catwalks and furnace mouths. Gorm's home.");
		if (Id == TEXT("drowned_chapel")) return LOCTEXT("ChapelB", "Lowmere's sunken church: ankle-deep water on stone flags, broken pews, candles and a rose window full of moonlight. Mirela's home.");
		if (Id == TEXT("stilt_market")) return LOCTEXT("MarketB", "The market on stilts over the bog at night: planks and rope rails, strings of paper lanterns, mist. Bram's beat.");
		if (Id == TEXT("roost")) return LOCTEXT("RoostB", "Beneath the Sump: a ledge over a glowing lake, tar falls, bone and the drowned hoard, lit by green storm light.");
		if (Id == TEXT("heartwater")) return LOCTEXT("HeartwaterB", "Water without a shore: the Fragment's own arena.");
		return FText::GetEmpty();
	}

	FText Confirm(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsConfirm", "✕");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbConfirm", "A");
		default: return LOCTEXT("KbConfirm", "Enter");
		}
	}

	FText Back(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsBack", "○");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbBack", "B");
		default: return LOCTEXT("KbBack", "Esc");
		}
	}

	FText NextName(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsNext", "R3");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbNext", "RS");
		default: return LOCTEXT("KbNext", "Tab");
		}
	}

	FText ResetName(USlopGameInstance* GI)
	{
		switch (GI->Router->Glyphs())
		{
		case Slop::EGlyphs::PlayStation: return GI->Router->PadSaysShare() ? LOCTEXT("PsResetOld", "Share") : LOCTEXT("PsReset", "Create");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbReset", "View");
		default: return LOCTEXT("KbReset", "F4");
		}
	}

	FText PauseName(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsPause", "Options");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbPause", "Menu");
		default: return LOCTEXT("KbPause", "Esc");
		}
	}

	FText KeyName(const FKey& K, Slop::EGlyphs G)
	{
		const bool bPs = G == Slop::EGlyphs::PlayStation;
		if (K == EKeys::Gamepad_FaceButton_Bottom) return bPs ? LOCTEXT("Cross", "✕") : LOCTEXT("A", "A");
		if (K == EKeys::Gamepad_FaceButton_Right) return bPs ? LOCTEXT("Circle", "○") : LOCTEXT("B", "B");
		if (K == EKeys::Gamepad_FaceButton_Left) return bPs ? LOCTEXT("Square", "□") : LOCTEXT("X", "X");
		if (K == EKeys::Gamepad_FaceButton_Top) return bPs ? LOCTEXT("Triangle", "△") : LOCTEXT("Y", "Y");
		if (K == EKeys::Gamepad_LeftShoulder) return bPs ? LOCTEXT("L1", "L1") : LOCTEXT("LB", "LB");
		if (K == EKeys::Gamepad_RightShoulder) return bPs ? LOCTEXT("R1", "R1") : LOCTEXT("RB", "RB");
		if (K == EKeys::Gamepad_LeftTrigger || K == EKeys::Gamepad_LeftTriggerAxis) return bPs ? LOCTEXT("L2", "L2") : LOCTEXT("LT", "LT");
		if (K == EKeys::Gamepad_RightTrigger || K == EKeys::Gamepad_RightTriggerAxis) return bPs ? LOCTEXT("R2", "R2") : LOCTEXT("RT", "RT");
		if (K == EKeys::Gamepad_LeftThumbstick) return bPs ? LOCTEXT("L3", "L3") : LOCTEXT("LS", "LS");
		if (K == EKeys::Gamepad_RightThumbstick) return bPs ? LOCTEXT("R3", "R3") : LOCTEXT("RS", "RS");
		if (K == EKeys::Gamepad_DPad_Up) return LOCTEXT("DUp", "D-pad up");
		if (K == EKeys::Gamepad_DPad_Down) return LOCTEXT("DDown", "D-pad down");
		if (K == EKeys::Gamepad_DPad_Left) return LOCTEXT("DLeft", "D-pad left");
		if (K == EKeys::Gamepad_DPad_Right) return LOCTEXT("DRight", "D-pad right");
		return K.GetDisplayName();
	}

	FText ActionKey(USlopGameInstance* GI, Slop::EGameAction A, int32 Side)
	{
		const Slop::EGlyphs G = GI->Router->Glyphs();
		if (G == Slop::EGlyphs::Keyboard)
		{
			return KeyName(GI->Settings.Keys[FMath::Clamp(Side, 0, 1)][(int32)A], G);
		}
		return KeyName(GI->Settings.PadButtons[(int32)A], G);
	}

	FString ButtonsIn(USlopGameInstance* GI, const FString& Notation, int32 Side)
	{
		using Slop::EGameAction;
		static const TPair<const TCHAR*, EGameAction> Codes[] = {{TEXT("HP"), EGameAction::HP}, {TEXT("LP"), EGameAction::LP}, {TEXT("HK"), EGameAction::HK},
			{TEXT("LK"), EGameAction::LK}, {TEXT("BL"), EGameAction::BL}, {TEXT("RN"), EGameAction::RN}};
		FString Out;
		for (int32 I = 0; I < Notation.Len();)
		{
			int32 End = I;
			while (End < Notation.Len() && FChar::IsAlnum(Notation[End]))
			{
				++End;
			}
			if (End == I)
			{
				Out.AppendChar(Notation[I++]);
				continue;
			}
			const FString Word = Notation.Mid(I, End - I);
			FString Name = Word;
			for (const TPair<const TCHAR*, EGameAction>& C : Codes)
			{
				if (Word == C.Key)
				{
					Name = ActionKey(GI, C.Value, Side).ToString();
				}
			}
			Out += Name;
			I = End;
		}
		return Out;
	}

	const FSlateBrush* Portrait(FName Fighter)
	{
		// Kept for the life of the process, like the fonts: a handful of small textures.
		static TMap<FName, TSharedPtr<FSlateBrush>>* Cache = new TMap<FName, TSharedPtr<FSlateBrush>>();
		if (const TSharedPtr<FSlateBrush>* Found = Cache->Find(Fighter))
		{
			return Found->Get();
		}
		const FString Name = FString::Printf(TEXT("T_portrait_%s"), *Fighter.ToString());
		UTexture2D* T = LoadUiTexture(TEXT("/Game/Slop/Portraits"), Name);
		TSharedPtr<FSlateBrush> B;
		if (T)
		{
			B = MakeShared<FSlateBrush>();
			B->SetResourceObject(T);
			B->ImageSize = FVector2D(420, 500);
			B->DrawAs = ESlateBrushDrawType::Image;
		}
		Cache->Add(Fighter, B);
		return B.Get();
	}

	const FSlateBrush* StagePicture(FName Stage)
	{
		// Kept for the life of the process, like the portraits.
		static TMap<FName, TSharedPtr<FSlateBrush>>* Cache = new TMap<FName, TSharedPtr<FSlateBrush>>();
		if (const TSharedPtr<FSlateBrush>* Found = Cache->Find(Stage))
		{
			return Found->Get();
		}
		const FString Name = FString::Printf(TEXT("T_stage_%s"), *Stage.ToString());
		UTexture2D* T = LoadUiTexture(TEXT("/Game/Slop/UI"), Name);
		TSharedPtr<FSlateBrush> B;
		if (T)
		{
			B = MakeShared<FSlateBrush>();
			B->SetResourceObject(T);
			B->ImageSize = FVector2D(960, 540);
			B->DrawAs = ESlateBrushDrawType::Image;
		}
		Cache->Add(Stage, B);
		return B.Get();
	}
}

#undef LOCTEXT_NAMESPACE
