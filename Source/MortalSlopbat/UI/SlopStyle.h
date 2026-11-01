#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Core/SlopInputRouter.h"

class USlopGameInstance;

// The design system's tokens: colours sampled from the owner's title card, the two type families,
// the interface textures (SourceArt/UI, rebuilt by Tools/ui_art.py) and the button names of the
// device in hand. Docs/UI.md gives the reasoning; the shared widgets are in SlopWidgets.h. Layout
// is in 1920x1080 units on a canvas that grows on the long side of any other shape of screen.
namespace SlopUI
{
	extern const FLinearColor Ink;
	extern const FLinearColor Panel;
	extern const FLinearColor Raised;
	extern const FLinearColor Line;
	extern const FLinearColor Text;
	extern const FLinearColor Dim;
	extern const FLinearColor Faint;
	// The signature violet, for focus and selection; Spark is its brightest vein, for small marks.
	extern const FLinearColor Accent;
	extern const FLinearColor Spark;
	// The second player's cursor and name, so two players never share a colour.
	extern const FLinearColor Second;
	extern const FLinearColor Warn;
	extern const FLinearColor Danger;
	extern const FLinearColor Good;
	// Resources keep their meaning: the Grandmaster's Qhi is gold, the run meter blue.
	extern const FLinearColor Qhi;
	extern const FLinearColor Run;

	// Every game font's size is multiplied by this (Settings > Comfort > Text size).
	void SetTextScale(float Scale);
	// Cinzel: headings and names.
	FSlateFontInfo Display(float Size);
	// Cinzel Bold: round calls, the clock, big numbers.
	FSlateFontInfo Call(float Size);
	// Source Sans 3: anything that is read rather than glanced at.
	FSlateFontInfo Body(float Size);
	FSlateFontInfo Strong(float Size);
	// Source Sans 3 SemiBold, tracked out: small labels in capitals.
	FSlateFontInfo Caps(float Size);

	enum class ETex : uint8
	{
		Logo, LogoEnergy, FadeSide, FadeFoot, Rule, Spike, SpikeHollow, Frame, BarFrame, BarFrameFlipped,
		TimerPlate, BarFill, Band, Vignette, Glow, Sweep, Count
	};
	// An interface texture as a brush; a missing one draws nothing.
	const FSlateBrush* Brush(ETex T);
	// A plain coloured box brush.
	const FSlateBrush* Box();
	// A fighter's portrait (head and shoulders, 420 x 500), or null when it has none.
	const FSlateBrush* Portrait(FName Fighter);
	// The stage screen's picture of a ground (T_stage_<id>), or null when there is none.
	const FSlateBrush* StagePicture(FName Stage);

	// Effects: Reduced in Settings also stills the interface's ambient motion.
	void SetReducedMotion(bool bReduced);
	bool ReducedMotion();
	float EaseOut(float T);
	float EaseInOut(float T);
	// T's progress through From..To, clamped to 0..1.
	float Span(float T, float From, float To);

	// Scale and size of the design canvas on a surface of this size: at least 1920 x 1080 units.
	float StageScale(const FVector2D& Local);
	FVector2D StageSize(const FVector2D& Local);
	// The share of each edge of the screen left clear for a TV that crops its picture (Settings).
	void SetScreenMargin(float Share);
	// The canvas inside that margin: its origin on the surface, its scale and its size in units.
	struct FStageFrame
	{
		FVector2D Origin = FVector2D::ZeroVector;
		float Scale = 1.f;
		FVector2D Size = FVector2D(1920.f, 1080.f);
	};
	FStageFrame StageFrame(const FVector2D& Local);

	// Button names for the device in hand.
	FText Confirm(Slop::EGlyphs G);
	FText Back(Slop::EGlyphs G);
	FText PauseName(Slop::EGlyphs G);
	// Practice fights: change the dummy, skip a tutorial step, next trial; and reset.
	FText NextName(Slop::EGlyphs G);
	FText ResetName(USlopGameInstance* GI);
	FText KeyName(const FKey& K, Slop::EGlyphs G);
	FText ActionKey(USlopGameInstance* GI, Slop::EGameAction A, int32 Side = 0);
	// Move notation with its button codes (HP, LP, ...) as the buttons being shown: "↓ → + HP" reads
	// "↓ → + △" for a PlayStation pad.
	FString ButtonsIn(USlopGameInstance* GI, const FString& Notation, int32 Side = 0);
	FText StageName(FName Id);
	FText StageBlurb(FName Id);
}
