#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Combat/SlopSim.h"

class USlopGameInstance;
class ASlopMatch;

namespace Slop
{
	// What a fighter's bars show, read straight from the simulation: the one place that decides it,
	// and testable without a screen. The HUD never holds a value of its own beyond its trailing bars.
	enum class EHudMeter : uint8
	{
		None,
		// The run meter.
		Run,
		// The Grandmaster's Qhi: in his own body, spent, or in a shape with no run meter.
		Qhi,
		// A borrowed shape's run meter, with the Qhi that holds the shape beneath it.
		RunAndQhi,
		// Hollis's Fragmented Vessel filling, and just broken.
		Vessel,
		VesselBroken,
	};
	struct FHudReadout
	{
		float Health = 0.f;
		bool bLow = false;
		EHudMeter Meter = EHudMeter::None;
		float Run = 0.f;
		float Qhi = 0.f;
		float Vessel = 0.f;
		// Where on the Qhi bar a mark stands: half for a shapeshift's price, or the recentring point.
		float Mark = -1.f;
		bool bSpent = false;
		bool bBorrowed = false;
	};
	FHudReadout HudReadout(const FFighter& F);
}

// The fight's heads-up display, painted directly from the simulation each frame: health with names,
// meters beneath, round wins, the timer, round calls, the combo counter, the Grandmaster's changes of
// shape, and the training readouts (dummy mode, input history, frame data, states). Everything sits on
// the design canvas, pinned to the top and centre so the bars stay near the eye on a wide screen. It
// holds no fight state of its own beyond the trailing bars; it only ever reads the rules.
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
	void PaintBars(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 Layer) const;
	void PaintMeters(FSlateWindowElementList& Out, const FGeometry& G, int32 Side, float SlotX, float SlotW, float Y, int32 Layer) const;
	void PaintCalls(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 Layer) const;
	void PaintTraining(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 Layer) const;
	// A block of text on a framed panel, centred on X or hung from either side of it.
	void PaintCard(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FText& T, const FSlateFontInfo& F, const FVector2D& Pos,
		const FLinearColor& C, int32 Align) const;
	// Training's hit, hurt and push boxes, projected through the fight camera.
	void PaintBoxes(FSlateWindowElementList& Out, const FGeometry& AllottedGeometry, int32 Layer) const;
	void PaintText(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FText& T, const FSlateFontInfo& F,
		const FVector2D& Pos, const FLinearColor& C, int32 Align, float Scale = 1.f) const;
	void Rebuild();
	USlopGameInstance* GI = nullptr;
	TWeakObjectPtr<ASlopMatch> Match;
	FText AnnounceText, AnnounceSub;
	FLinearColor AnnounceColor = FLinearColor::White;
	float AnnounceSize = 120.f;
	float AnnounceT = 0.f, AnnounceLife = 0.f;
	// The call on screen is the finisher's: the winner's line goes under it rather than replacing it.
	bool bFinisherCall = false;
	FText SubtitleText;
	float SubtitleT = 0.f;
	FText NoticeLine;
	float NoticeT = 0.f;
	// Text kept between frames, so a steady bar costs no strings: who each side is, the clock.
	FText NameText[2];
	FText TagText[2];
	FText ShapeText[2];
	const Slop::FFighterDef* ShapeOf[2] = {nullptr, nullptr};
	FText TimerText;
	int32 ShownSeconds = -1;
	// 0 to 1 as the bars come in after the staging.
	float Arrive = 0.f;
	FText ComboCount[2];
	FText ComboLine[2];
	bool bComboCounter[2] = {false, false};
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
