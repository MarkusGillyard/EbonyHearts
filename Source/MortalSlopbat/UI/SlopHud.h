#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Combat/SlopSim.h"

class USlopGameInstance;
class ASlopMatch;

// The fight's heads-up display: health with names, run meters beneath, round wins, the timer,
// announcements, the combo counter and counter-hit callout, the stranger's lines, and the
// training readouts (dummy mode, input history, frame data, states).
class SSlopHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlopHud) {}
		SLATE_ARGUMENT(USlopGameInstance*, GI)
		SLATE_ARGUMENT(ASlopMatch*, Match)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void OnEvent(const Slop::FSimEvent& E);
	void Announce(const FText& Text, const FText& Sub, float Life, const FLinearColor& Color, float Size);
	void Subtitle(const FText& Text, float Life);
	void Prompt(const FText& Text) { PromptText = Text; }
	bool bShowFrames = false;
	bool bShowStates = false;
	bool bInputDisplay = false;
	FText PromptText;

private:
	void PaintBars(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer) const;
	// Training's hit, hurt and push boxes, projected through the fight camera.
	void PaintBoxes(FSlateWindowElementList& Out, const FGeometry& AllottedGeometry, int32 Layer) const;
	void PaintText(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FText& T, const FSlateFontInfo& F,
		const FVector2D& Pos, const FLinearColor& C, int32 Align, float Scale = 1.f) const;
	FText NameOf(int32 Side) const;
	USlopGameInstance* GI = nullptr;
	TWeakObjectPtr<ASlopMatch> Match;
	FText AnnounceText, AnnounceSub;
	FLinearColor AnnounceColor = FLinearColor::White;
	float AnnounceSize = 120.f;
	float AnnounceT = 0.f, AnnounceLife = 0.f;
	FText SubtitleText;
	float SubtitleT = 0.f;
	FText NoticeLine;
	float NoticeT = 0.f;
	FText ComboText[2];
	FLinearColor ComboColor[2];
	float ComboT[2] = {0, 0};
	float ComboPop[2] = {0, 0};
	// The Grandmaster's changes of shape, called out under his bar.
	FText FormText[2];
	FLinearColor FormColor[2];
	float FormT[2] = {0, 0};
	// The Qhi bar shown, trailing a spend so the cost reads.
	float QhiTrail[2] = {1, 1};
	// Health shown, trailing the real value so a blow's damage reads as a draining bar.
	float Trail[2] = {1, 1};
	// Input history per side for the input display: [dir arrow, buttons, ticks held].
	struct FHist { int32 Dir; int32 Btn; int32 Ticks; };
	TArray<FHist> History[2];
	int32 LastTick = -1;
};
