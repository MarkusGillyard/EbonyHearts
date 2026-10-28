#include "UI/SlopStyle.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Core/SlopGameInstance.h"

#define LOCTEXT_NAMESPACE "SlopUI"

namespace SlopUI
{
	const FLinearColor Accent(FColor(168, 219, 56));
	const FLinearColor Amber(FColor(245, 168, 61));
	const FLinearColor Rust(FColor(184, 61, 31));
	const FLinearColor Text(FColor(237, 230, 209));
	const FLinearColor Dim(FColor(153, 158, 143));
	const FLinearColor Bg(FColor(5, 9, 8));
	const FLinearColor Panel(0.004f, 0.007f, 0.006f, 0.88f);
	const FLinearColor PanelEdge(FColor(46, 56, 46));

	static TSharedPtr<const FCompositeFont> LoadFont(const TCHAR* Name, const TCHAR* File)
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Fonts") / File;
		TSharedPtr<FStandaloneCompositeFont> F = MakeShared<FStandaloneCompositeFont>(FName(Name), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		// Arrows, ticks, dots and the infinity sign come from the engine's own fallback face; the
		// game's faces have letters, digits and common punctuation only.
		F->FallbackTypeface.Typeface.AppendFont(TEXT("Fallback"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return F;
	}

	static float TextScale = 1.f;

	void SetTextScale(float Scale) { TextScale = FMath::Clamp(Scale, 0.85f, 1.25f); }

	FSlateFontInfo Display(float Size)
	{
		// Loaded once and kept for the life of the process (Slate holds it until exit).
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("NewRocker"), TEXT("NewRocker-Regular.ttf")));
		// Sizes are written in pixels at 1080p, as the original's were; Slate's are points.
		return FSlateFontInfo(*F, Size * 0.75f * TextScale);
	}

	FSlateFontInfo Body(float Size)
	{
		static TSharedPtr<const FCompositeFont>* F = new TSharedPtr<const FCompositeFont>(LoadFont(TEXT("Oswald"), TEXT("Oswald-Variable.ttf")));
		return FSlateFontInfo(*F, Size * 0.75f * TextScale);
	}

	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("GenericWhiteBox"); }

	const FSlateBrush* Box()
	{
		static FSlateColorBrush B(FLinearColor::White);
		return &B;
	}

	const FSlateBrush* Rounded()
	{
		static FSlateRoundedBoxBrush B(FLinearColor::White, 6.f, FLinearColor::White, 0.f);
		return &B;
	}

	TSharedRef<SWidget> Label(const TAttribute<FText>& InText, float Size, const TAttribute<FSlateColor>& Color, bool bDisplay)
	{
		return SNew(STextBlock).Text(InText).Font(bDisplay ? Display(Size) : Body(Size)).ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f));
	}

	TSharedRef<SWidget> Title(const FText& InText, float Size, const FLinearColor& Color)
	{
		FSlateFontInfo F = Display(Size);
		F.OutlineSettings.OutlineSize = 2;
		F.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.8f);
		return SNew(STextBlock).Text(InText).Font(F).ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(3.f, 4.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f));
	}

	TSharedRef<SWidget> Menu(const TArray<FText>& Items, TFunction<int32()> Selected, TFunction<bool(int32)> Enabled,
		TFunction<void(int32)> OnClick, float Width, float Size)
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		for (int32 I = 0; I < Items.Num(); ++I)
		{
			auto IsSel = [Selected, I]() { return Selected() == I; };
			auto IsOn = [Enabled, I]() { return !Enabled || Enabled(I); };
			V->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SBox).WidthOverride(Width)
				[
					SNew(SBorder)
					.BorderImage(Rounded())
					.BorderBackgroundColor_Lambda([IsSel]() { return IsSel() ? FLinearColor(0.04f, 0.07f, 0.02f, 0.95f) : FLinearColor(0.01f, 0.016f, 0.013f, 0.8f); })
					.Padding(FMargin(18.f, 6.f))
					.OnMouseButtonDown_Lambda([OnClick, IsOn, I](const FGeometry&, const FPointerEvent&)
					{
						if (OnClick && IsOn())
						{
							OnClick(I);
						}
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
						[
							SNew(SBox).WidthOverride(6.f).HeightOverride(Size * 1.1f)
							[
								SNew(SImage).Image(Box()).ColorAndOpacity_Lambda([IsSel]() { return IsSel() ? Accent : FLinearColor::Transparent; })
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Items[I]).Font(Body(Size))
							.ColorAndOpacity_Lambda([IsSel, IsOn]() { return !IsOn() ? FSlateColor(Dim * 0.6f) : (IsSel() ? FSlateColor(Accent) : FSlateColor(Text)); })
						]
					]
				]
			];
		}
		return V;
	}

	TSharedRef<SWidget> PanelBox(const TSharedRef<SWidget>& Content, const FMargin& Padding)
	{
		return SNew(SBorder).BorderImage(Rounded()).BorderBackgroundColor(Panel).Padding(Padding)[Content];
	}

	TSharedRef<SWidget> Hint(TFunction<FText()> TextFn)
	{
		return SNew(SBox).Padding(FMargin(0, 0, 0, 34)).VAlign(VAlign_Bottom).HAlign(HAlign_Center)
		[
			SNew(STextBlock).Font(Body(24.f)).ColorAndOpacity(Dim).Text_Lambda(MoveTemp(TextFn))
		];
	}

	FText Confirm(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsConfirm", "Cross");
		case Slop::EGlyphs::Xbox: return LOCTEXT("XbConfirm", "A");
		default: return LOCTEXT("KbConfirm", "Enter");
		}
	}

	FText Back(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsBack", "Circle");
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

	FText ResetName(Slop::EGlyphs G)
	{
		switch (G)
		{
		case Slop::EGlyphs::PlayStation: return LOCTEXT("PsReset", "Create");
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
		if (K == EKeys::Gamepad_FaceButton_Bottom) return bPs ? LOCTEXT("Cross", "Cross") : LOCTEXT("A", "A");
		if (K == EKeys::Gamepad_FaceButton_Right) return bPs ? LOCTEXT("Circle", "Circle") : LOCTEXT("B", "B");
		if (K == EKeys::Gamepad_FaceButton_Left) return bPs ? LOCTEXT("Square", "Square") : LOCTEXT("X", "X");
		if (K == EKeys::Gamepad_FaceButton_Top) return bPs ? LOCTEXT("Triangle", "Triangle") : LOCTEXT("Y", "Y");
		if (K == EKeys::Gamepad_LeftShoulder) return bPs ? LOCTEXT("L1", "L1") : LOCTEXT("LB", "LB");
		if (K == EKeys::Gamepad_RightShoulder) return bPs ? LOCTEXT("R1", "R1") : LOCTEXT("RB", "RB");
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

	const FSlateBrush* Portrait(FName Fighter)
	{
		// Kept for the life of the process, like the fonts: a handful of small textures.
		static TMap<FName, TSharedPtr<FSlateBrush>>* Cache = new TMap<FName, TSharedPtr<FSlateBrush>>();
		if (const TSharedPtr<FSlateBrush>* Found = Cache->Find(Fighter))
		{
			return Found->Get();
		}
		const FString Name = FString::Printf(TEXT("T_portrait_%s"), *Fighter.ToString());
		UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Slop/Portraits/%s.%s"), *Name, *Name), nullptr, LOAD_Quiet | LOAD_NoWarn);
		TSharedPtr<FSlateBrush> B;
		if (T)
		{
			T->AddToRoot();
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
		UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Slop/UI/%s.%s"), *Name, *Name), nullptr, LOAD_Quiet | LOAD_NoWarn);
		TSharedPtr<FSlateBrush> B;
		if (T)
		{
			T->AddToRoot();
			B = MakeShared<FSlateBrush>();
			B->SetResourceObject(T);
			B->ImageSize = FVector2D(960, 540);
			B->DrawAs = ESlateBrushDrawType::Image;
		}
		Cache->Add(Stage, B);
		return B.Get();
	}

	TSharedRef<SWidget> Backdrop()
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(Box()).ColorAndOpacity(Bg)]
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Box()).ColorAndOpacity(FLinearColor(0.03f, 0.06f, 0.04f, 0.35f))
			];
	}
}

#undef LOCTEXT_NAMESPACE
