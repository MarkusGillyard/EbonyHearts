#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/Anchors.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "UI/SlopStyle.h"

class USlopGameInstance;

// The shared widgets every screen is built from, so each focus state, prompt, heading and panel
// looks and moves the same wherever it appears.
namespace SlopUI
{
	// Gives its child the design canvas: 1920 x 1080 units or more, scaled to fill the surface.
	class SStage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SStage) : _FullBleed(false) {}
			// Ignores the screen margin: for what must reach the very edges, like veils.
			SLATE_ARGUMENT(bool, FullBleed)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { bFullBleed = InArgs._FullBleed; ChildSlot[InArgs._Content.Widget]; }
		virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
		// Slate measures text at the scale it will be drawn: without this, wrapped text is laid out
		// for one size and painted at another, and lines overlap on any screen but 1080 high.
		virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const override;

	private:
		FStageFrame Frame(const FVector2D& Local) const;
		bool bFullBleed = false;
	};

	// Anchors on the canvas; Place's offset runs from one, and the widget's own alignment says which
	// part of it sits there.
	inline const FVector2D TopLeft(0.f, 0.f), TopCentre(0.5f, 0.f), TopRight(1.f, 0.f), MidLeft(0.f, 0.5f), Middle(0.5f, 0.5f), MidRight(1.f, 0.5f),
		FootLeft(0.f, 1.f), FootCentre(0.5f, 1.f), FootRight(1.f, 1.f);
	TSharedRef<SConstraintCanvas> Canvas();
	void Place(const TSharedRef<SConstraintCanvas>& C, const FVector2D& Anchor, const FVector2D& Offset, const TSharedRef<SWidget>& W,
		const FVector2D& Align = FVector2D::ZeroVector, const FVector2D& Size = FVector2D::ZeroVector);
	// Between anchors. On an axis whose two anchors are equal, Slate reads the far offset as a size.
	void Fill(const TSharedRef<SConstraintCanvas>& C, const FAnchors& Anchors, const FMargin& Offset, const TSharedRef<SWidget>& W);

	TSharedRef<SWidget> Label(const TAttribute<FText>& InText, float Size, const TAttribute<FSlateColor>& Color = FSlateColor(Text), bool bDisplay = false);
	// A label in tracked capitals.
	TSharedRef<SWidget> CapsLabel(const TAttribute<FText>& InText, float Size = 20.f, const TAttribute<FSlateColor>& Color = FSlateColor(Dim));
	// A display line with a soft shadow, for headings and calls.
	TSharedRef<SWidget> Title(const TAttribute<FText>& InText, float Size = 84.f, const FLinearColor& Color = Text, ETextJustify::Type Justify = ETextJustify::Left);
	// A line of fractured metal.
	TSharedRef<SWidget> Rule(float Width, const FLinearColor& Color = Line);
	// A screen's heading: an optional small line, the title, and a rule beneath.
	TSharedRef<SWidget> Heading(const TAttribute<FText>& InText, const TAttribute<FText>& Small = FText::GetEmpty(), float Size = 54.f);
	// Dark fill behind a framed block.
	TSharedRef<SWidget> PanelBox(const TSharedRef<SWidget>& Content, const FMargin& Padding = FMargin(30.f, 22.f));

	struct FMenuStyle
	{
		float Width = 460.f;
		float Size = 32.f;
		// The first Major rows at Size in Cinzel; the rest smaller.
		int32 Major = 100;
		float MinorSize = 26.f;
		// Rows after which a small gap falls, grouping a long list.
		TArray<int32> GapsAfter;
	};
	// A list of menu items; `Selected` gives the focused index, `Enabled` whether an item may be
	// chosen. Clicking an item calls OnClick; the mouse moving over one calls OnHover (to select it).
	TSharedRef<SWidget> Menu(const TArray<FText>& Items, TFunction<int32()> Selected, TFunction<bool(int32)> Enabled,
		TFunction<void(int32)> OnClick, TFunction<void(int32)> OnHover, const FMenuStyle& Style = FMenuStyle());

	// A prompt: the button for the device in hand, and what it does here.
	enum class EPrompt : uint8 { Confirm, Back, Pause, Next, Reset, Page, Move, UpDown, Sideways };
	struct FPrompt
	{
		EPrompt Key;
		FText Text;
	};
	TSharedRef<SWidget> Prompts(USlopGameInstance* GI, const TArray<FPrompt>& List);

	// Slow motes of violet drifting up the screen; nothing with reduced effects.
	TSharedRef<SWidget> Motes(int32 Count = 26);

	// What lies between the world and a screen's content.
	enum class EVeil : uint8
	{
		// Nothing: a composed scene, its text on the foot's gradient.
		Foot,
		// Ink at the left falling away to the right, the world in sight.
		Side,
		// Over the whole screen.
		Full,
	};
	// A whole screen: its canvas on the stage, the veil behind it and the prompts at its foot.
	TSharedRef<SWidget> Shell(USlopGameInstance* GI, const TSharedRef<SConstraintCanvas>& C, const TArray<FPrompt>& List, EVeil Veil = EVeil::Side);
}
