#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"
#include "Core/SlopInputRouter.h"

class USlopGameInstance;

// The shared look: bone-white type on near-black teal, slop-green for focus and selection, rust
// and amber for warnings and titles. New Rocker for display text, Oswald for everything else.
// Layout is in 1920x1080 units; Unreal's DPI scale maps that to the window.
namespace SlopUI
{
	extern const FLinearColor Accent;
	extern const FLinearColor Amber;
	extern const FLinearColor Rust;
	extern const FLinearColor Text;
	extern const FLinearColor Dim;
	extern const FLinearColor Bg;
	extern const FLinearColor Panel;
	extern const FLinearColor PanelEdge;

	// Every game font's size is multiplied by this (Settings > Comfort > Interface size).
	void SetTextScale(float Scale);
	FSlateFontInfo Display(float Size);
	FSlateFontInfo Body(float Size);
	const FSlateBrush* White();
	const FSlateBrush* Rounded();
	// A fighter's select-screen portrait (head and shoulders, 420 x 500), or null when it has none.
	const FSlateBrush* Portrait(FName Fighter);
	// The stage screen's picture of a ground (T_stage_<id>), or null when there is none.
	const FSlateBrush* StagePicture(FName Stage);
	// A plain coloured box brush.
	const FSlateBrush* Box();

	TSharedRef<SWidget> Label(const TAttribute<FText>& InText, float Size, const TAttribute<FSlateColor>& Color = FSlateColor(Text), bool bDisplay = false);
	TSharedRef<SWidget> Title(const FText& InText, float Size = 84.f, const FLinearColor& Color = Accent);
	// A list of menu items; `Selected` gives the focused index, `Enabled` whether an item may be
	// chosen. Clicking an item calls OnClick; the mouse moving over one calls OnHover (to select it).
	TSharedRef<SWidget> Menu(const TArray<FText>& Items, TFunction<int32()> Selected, TFunction<bool(int32)> Enabled,
		TFunction<void(int32)> OnClick, float Width = 460.f, float Size = 34.f, TFunction<void(int32)> OnHover = nullptr);
	TSharedRef<SWidget> PanelBox(const TSharedRef<SWidget>& Content, const FMargin& Padding = FMargin(28.f, 20.f));
	// The prompt line at the foot of a screen, worded for the device in hand.
	TSharedRef<SWidget> Hint(TFunction<FText()> Text);
	// Button names for the device in hand.
	FText Confirm(Slop::EGlyphs G);
	FText Back(Slop::EGlyphs G);
	FText PauseName(Slop::EGlyphs G);
	// Practice fights: change the dummy, skip a tutorial step, next trial; and reset.
	FText NextName(Slop::EGlyphs G);
	FText ResetName(Slop::EGlyphs G);
	FText KeyName(const FKey& K, Slop::EGlyphs G);
	FText ActionKey(USlopGameInstance* GI, Slop::EGameAction A, int32 Side = 0);
	// A dark gradient-free backdrop for menus with a faint vignette.
	TSharedRef<SWidget> Backdrop();
}
