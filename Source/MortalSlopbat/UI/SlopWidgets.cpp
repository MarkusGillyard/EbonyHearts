#include "UI/SlopWidgets.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Core/SlopGameInstance.h"

#define LOCTEXT_NAMESPACE "SlopWidgets"

namespace SlopUI
{
	void SStage::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
	{
		const TSharedRef<SWidget>& Child = ChildSlot.GetWidget();
		if (ArrangedChildren.Accepts(Child->GetVisibility()))
		{
			const FStageFrame F = Frame(AllottedGeometry.GetLocalSize());
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Child, F.Size, FSlateLayoutTransform(F.Scale, F.Origin)));
		}
	}

	FStageFrame SStage::Frame(const FVector2D& Local) const
	{
		if (bFullBleed)
		{
			FStageFrame F;
			F.Scale = StageScale(Local);
			F.Size = Local / F.Scale;
			return F;
		}
		return StageFrame(Local);
	}

	float SStage::GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const
	{
		const FVector2D Local = GetTickSpaceGeometry().GetLocalSize();
		return Local.X > 0.0 && Local.Y > 0.0 ? Frame(Local).Scale : 1.f;
	}

	TSharedRef<SConstraintCanvas> Canvas() { return SNew(SConstraintCanvas); }

	void Place(const TSharedRef<SConstraintCanvas>& C, const FVector2D& Anchor, const FVector2D& Offset, const TSharedRef<SWidget>& W, const FVector2D& Align, const FVector2D& Size)
	{
		// Slots default to centred alignment: every one here says its own.
		const bool bAuto = Size.IsNearlyZero();
		C->AddSlot().Anchors(FAnchors(Anchor.X, Anchor.Y)).Alignment(Align).AutoSize(bAuto).Offset(FMargin(Offset.X, Offset.Y, Size.X, Size.Y))[W];
	}

	void Fill(const TSharedRef<SConstraintCanvas>& C, const FAnchors& Anchors, const FMargin& Offset, const TSharedRef<SWidget>& W)
	{
		C->AddSlot().Anchors(Anchors).Alignment(FVector2D::ZeroVector).Offset(Offset)[W];
	}

	TSharedRef<SWidget> Label(const TAttribute<FText>& InText, float Size, const TAttribute<FSlateColor>& Color, bool bDisplay)
	{
		return SNew(STextBlock).Text(InText).Font(bDisplay ? Display(Size) : Body(Size)).ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f));
	}

	TSharedRef<SWidget> CapsLabel(const TAttribute<FText>& InText, float Size, const TAttribute<FSlateColor>& Color)
	{
		return SNew(STextBlock).Text(InText).Font(Caps(Size)).ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f));
	}

	TSharedRef<SWidget> Title(const TAttribute<FText>& InText, float Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		return SNew(STextBlock).Text(InText).Font(Size >= 70.f ? Call(Size) : Display(Size)).ColorAndOpacity(Color).Justification(Justify).AutoWrapText(true)
			.ShadowOffset(FVector2D(2.f, 3.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75f));
	}

	TSharedRef<SWidget> Rule(float Width, const FLinearColor& Color)
	{
		return SNew(SBox).WidthOverride(Width).HeightOverride(4.f)[SNew(SImage).Image(Brush(ETex::Rule)).ColorAndOpacity(Color)];
	}

	TSharedRef<SWidget> Heading(const TAttribute<FText>& InText, const TAttribute<FText>& Small, float Size)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock).Text(Small).Font(Caps(20.f)).ColorAndOpacity(Spark)
				.Visibility_Lambda([Small]() { return Small.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			]
			+ SVerticalBox::Slot().AutoHeight()[Title(InText, Size, Text)]
			+ SVerticalBox::Slot().AutoHeight().Padding(-40, 12, 0, 0)[Rule(760.f)];
	}

	TSharedRef<SWidget> PanelBox(const TSharedRef<SWidget>& Content, const FMargin& Padding)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(Box()).ColorAndOpacity(Panel)]
			+ SOverlay::Slot()[SNew(SImage).Image(Brush(ETex::Frame)).ColorAndOpacity(Line)]
			+ SOverlay::Slot().Padding(Padding)[Content];
	}

	// One row of a menu. Focus eases in on the row's own clock, so moving through a list reads as one
	// mark travelling down it; the list itself never rebuilds.
	class SMenuRow : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMenuRow) {}
			SLATE_ARGUMENT(FText, Label)
			SLATE_ARGUMENT(FSlateFontInfo, Font)
			SLATE_ARGUMENT(float, Height)
			SLATE_ARGUMENT(TFunction<bool()>, IsSel)
			SLATE_ARGUMENT(TFunction<bool()>, IsOn)
			SLATE_ARGUMENT(TFunction<void()>, OnPick)
			SLATE_ARGUMENT(TFunction<void()>, OnHover)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			IsSel = A._IsSel;
			IsOn = A._IsOn;
			OnPick = A._OnPick;
			OnHover = A._OnHover;
			Focus = IsSel() ? 1.f : 0.f;
			SetCanTick(true);
			const float Mark = 22.f;
			ChildSlot
			[
				SNew(SBox).HeightOverride(A._Height)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().Padding(FMargin(0, 4))
					[
						SNew(SImage).Image(Brush(ETex::Sweep)).ColorAndOpacity_Lambda([this]()
						{
							FLinearColor C = Accent;
							C.A = 0.55f * Focus;
							return C;
						})
					]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(-Mark * 0.5f, 0, 0, 0))
					[
						SNew(SBox).WidthOverride(Mark).HeightOverride(Mark)
						[
							SNew(SImage).Image(Brush(ETex::Spike))
							.RenderTransformPivot(FVector2D(0.5f, 0.5f))
							.RenderTransform_Lambda([this]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2f(0.3f + 0.7f * Focus))); })
							.ColorAndOpacity_Lambda([this]()
							{
								FLinearColor C = Spark;
								C.A = Focus;
								return C;
							})
						]
					]
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(28, 0, 0, 0))
					[
						SNew(STextBlock).Text(A._Label).Font(A._Font)
						.RenderTransform_Lambda([this]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(8.f * Focus, 0.f))); })
						.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
						.ColorAndOpacity_Lambda([this]() { return !IsOn() ? FSlateColor(Faint) : FSlateColor(FMath::Lerp(Dim, Text, Focus)); })
					]
				]
			];
		}

		virtual void Tick(const FGeometry&, const double, const float InDeltaTime) override
		{
			const float Target = IsSel() ? 1.f : 0.f;
			Focus = ReducedMotion() ? Target : FMath::FInterpConstantTo(Focus, Target, InDeltaTime, 9.f);
		}

		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override
		{
			if (OnPick && IsOn())
			{
				OnPick();
			}
			return FReply::Handled();
		}

		virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override
		{
			// The item under the mouse becomes the selection.
			if (OnHover && IsOn() && !IsSel())
			{
				OnHover();
			}
			return FReply::Unhandled();
		}

	private:
		TFunction<bool()> IsSel;
		TFunction<bool()> IsOn;
		TFunction<void()> OnPick;
		TFunction<void()> OnHover;
		float Focus = 0.f;
	};

	TSharedRef<SWidget> Menu(const TArray<FText>& Items, TFunction<int32()> Selected, TFunction<bool(int32)> Enabled,
		TFunction<void(int32)> OnClick, TFunction<void(int32)> OnHover, const FMenuStyle& Style)
	{
		TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
		for (int32 I = 0; I < Items.Num(); ++I)
		{
			const bool bMajor = I < Style.Major;
			const float Size = bMajor ? Style.Size : Style.MinorSize;
			V->AddSlot().AutoHeight()
			[
				SNew(SMenuRow).Label(Items[I]).Font(bMajor ? Display(Size) : Body(Size)).Height(Size * 1.62f)
				.IsSel([Selected, I]() { return Selected() == I; })
				.IsOn([Enabled, I]() { return !Enabled || Enabled(I); })
				.OnPick([OnClick, I]() { if (OnClick) OnClick(I); })
				.OnHover([OnHover, I]() { if (OnHover) OnHover(I); })
			];
			if (Style.GapsAfter.Contains(I))
			{
				V->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(18.f)];
			}
		}
		// The rail the focus mark rides, as a single quiet line.
		return SNew(SBox).WidthOverride(Style.Width)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(1.5f)[SNew(SImage).Image(Box()).ColorAndOpacity(Line)]]
			+ SOverlay::Slot()[V]
		];
	}

	static FText GlyphOf(USlopGameInstance* GI, EPrompt Key)
	{
		const Slop::EGlyphs G = GI->Router->Glyphs();
		switch (Key)
		{
		case EPrompt::Confirm: return Confirm(G);
		case EPrompt::Back: return Back(G);
		case EPrompt::Pause: return PauseName(G);
		case EPrompt::Next: return NextName(G);
		case EPrompt::Reset: return ResetName(GI);
		case EPrompt::Page: return G == Slop::EGlyphs::Keyboard ? LOCTEXT("PageKb", "Tab") : (G == Slop::EGlyphs::Xbox ? LOCTEXT("PageXb", "RB") : LOCTEXT("PagePs", "R1"));
		case EPrompt::Move: return LOCTEXT("MoveG", "↑↓←→");
		case EPrompt::UpDown: return LOCTEXT("UpDownG", "↑↓");
		case EPrompt::Sideways: return LOCTEXT("SideG", "←→");
		default: return FText::GetEmpty();
		}
	}

	TSharedRef<SWidget> Prompts(USlopGameInstance* GI, const TArray<FPrompt>& List)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (const FPrompt& P : List)
		{
			const EPrompt Key = P.Key;
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(26, 0, 0, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 9, 0)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(Box()).ColorAndOpacity(Panel)]
					+ SOverlay::Slot()[SNew(SImage).Image(Brush(ETex::Frame)).ColorAndOpacity(Line)]
					+ SOverlay::Slot().Padding(FMargin(11, 2)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(Strong(21.f)).ColorAndOpacity(Text).Text_Lambda([GI, Key]() { return GlyphOf(GI, Key); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Body(23.f)).ColorAndOpacity(Dim).Text(P.Text)
					.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))
				]
			];
		}
		return Row;
	}

	class SMotes : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMotes) {}
			SLATE_ARGUMENT(int32, Count)
		SLATE_END_ARGS()

		void Construct(const FArguments& A)
		{
			Count = A._Count;
			ForceVolatile(true);
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle&, bool) const override
		{
			if (ReducedMotion())
			{
				return LayerId;
			}
			const FVector2D Size = G.GetLocalSize();
			const float T = (float)FSlateApplication::Get().GetCurrentTime();
			for (int32 I = 0; I < Count; ++I)
			{
				// Each mote is fixed by its index: nothing is stored and nothing allocated per frame.
				const float A = FMath::Frac(I * 0.61803f);
				const float B = FMath::Frac(I * 0.75487f);
				const float Rate = 0.012f + 0.022f * B;
				const float Rise = FMath::Frac(T * Rate + A);
				const float X = (A + 0.018f * FMath::Sin(T * 0.3f + I)) * Size.X;
				const float Y = (1.f - Rise) * Size.Y;
				const float R = 3.f + 9.f * B * B;
				FLinearColor C = FMath::Lerp(Spark, Text, B * 0.4f);
				C.A = 0.22f * FMath::Sin(Rise * PI) * (0.6f + 0.4f * FMath::Sin(T * (0.7f + A) + I));
				FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(FVector2D(R, R), FSlateLayoutTransform(FVector2D(X, Y))), Brush(ETex::Glow), ESlateDrawEffect::None, C);
			}
			return LayerId;
		}

	private:
		int32 Count = 0;
	};

	TSharedRef<SWidget> Motes(int32 Count) { return SNew(SMotes).Count(Count); }

	TSharedRef<SWidget> Shell(USlopGameInstance* GI, const TSharedRef<SConstraintCanvas>& C, const TArray<FPrompt>& List, EVeil Veil)
	{
		TSharedRef<SConstraintCanvas> Under = Canvas();
		switch (Veil)
		{
		case EVeil::Side:
			Fill(Under, FAnchors(0.f, 0.f, 0.8f, 1.f), FMargin(0.f), SNew(SImage).Image(Brush(ETex::FadeSide)));
			break;
		case EVeil::Full:
			Fill(Under, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f), SNew(SImage).Image(Box()).ColorAndOpacity(Ink.CopyWithNewOpacity(0.74f)));
			break;
		default:
			break;
		}
		Fill(Under, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(0.f, -300.f, 0.f, 300.f), SNew(SImage).Image(Brush(ETex::FadeFoot)));
		if (List.Num() > 0)
		{
			Place(C, FootRight, FVector2D(-72.f, -40.f), Prompts(GI, List), FVector2D(1.f, 1.f));
		}
		return SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SStage).FullBleed(true)[Under]]
			+ SOverlay::Slot()[SNew(SStage)[C]];
	}
}

#undef LOCTEXT_NAMESPACE
