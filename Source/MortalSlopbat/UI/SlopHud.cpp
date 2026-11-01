#include "UI/SlopHud.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Anim/SlopMeshRig.h"
#include "Core/SlopGameInstance.h"
#include "UI/SlopStyle.h"
#include "View/SlopMatch.h"

#define LOCTEXT_NAMESPACE "SlopHud"

using namespace Slop;

namespace
{
	// In design units from the top centre of the canvas. The frame texture is 880 x 74 at full size,
	// its slot 34 to 862 across and 22 to 46 down.
	constexpr float FrameScale = 0.93f;
	constexpr float FrameW = 880.f * FrameScale;
	constexpr float FrameH = 74.f * FrameScale;
	constexpr float FrameTop = 24.f;
	constexpr float FrameInner = 80.f;
	constexpr float SlotIn = 34.f * FrameScale;
	constexpr float SlotOut = 18.f * FrameScale;
	constexpr float SlotW = (862.f - 34.f) * FrameScale;
	constexpr float SlotTop = FrameTop + 22.f * FrameScale;
	constexpr float SlotH = 24.f * FrameScale;
	constexpr float MeterW = 400.f;
	constexpr float MeterTop = SlotTop + SlotH + 9.f;
	// Health at or under this share reads as in danger.
	constexpr float Low = 0.25f;
}

namespace Slop
{
	FHudReadout HudReadout(const FFighter& F)
	{
		FHudReadout R;
		R.Health = FMath::Clamp((float)F.Health / FMath::Max(1, F.MaxHealth()), 0.f, 1.f);
		R.bLow = R.Health <= Low;
		if (F.bGuardian)
		{
			R.Meter = F.bCharging ? EHudMeter::Vessel : (F.VentT > 0 ? EHudMeter::VesselBroken : EHudMeter::None);
			R.Vessel = (float)F.Charge;
			return R;
		}
		const bool bRun = F.HasRunMeter();
		const bool bQhi = F.UsesQhi();
		R.Meter = bRun ? (bQhi ? EHudMeter::RunAndQhi : EHudMeter::Run) : (bQhi ? EHudMeter::Qhi : EHudMeter::None);
		R.bBorrowed = F.Form == EForm::Borrowed;
		R.bSpent = F.Form == EForm::Spent;
		if (bRun)
		{
			R.Run = (float)(F.RunMeter / FFighter::RunMax);
		}
		if (bQhi)
		{
			R.Qhi = (float)(F.Qhi / F.QhiRules().Max);
			if (!bRun)
			{
				R.Mark = (float)(R.bSpent ? F.QhiRules().RecoverAt / F.QhiRules().Max : FQhiRules::ShapeshiftShare);
			}
		}
		return R;
	}
}

void SSlopHud::Construct(const FArguments& InArgs)
{
	GI = InArgs._GI;
	Match = InArgs._Match;
	bInputDisplay = GI && (GI->Settings.bInputDisplay || GI->Flow.Config.Mode == EMode::Training);
	SetCanTick(true);
	if (Match.IsValid())
	{
		Match->OnEvent = [this](const FSimEvent& E) { OnEvent(E); };
		Match->OnAnnounce = [this](const FText& T, const FText& S) { Announce(T, S, 2.4f, SlopUI::Spark, 92.f); };
		Rebuild();
	}
}

void SSlopHud::Rebuild()
{
	const ASlopMatch* M = Match.Get();
	for (int32 I = 0; I < 2; ++I)
	{
		// Whoever's shape they wear, the bar names who they are.
		NameText[I] = I == 1 && M->Config.bMirrorDark ? LOCTEXT("Reflection", "THE REFLECTION") : FText::FromString(M->Sim.Fighters[I].BaseDef->Name.ToUpper());
		const bool bCpu = GI && GI->Flow.IsCpu(I);
		const bool bDummy = I == 1 && M->Dummy;
		TagText[I] = bDummy ? LOCTEXT("TagDummy", "DUMMY") : (bCpu ? LOCTEXT("TagCpu", "CPU") : (I == 0 ? LOCTEXT("TagP1", "P1") : LOCTEXT("TagP2", "P2")));
	}
}

void SSlopHud::Announce(const FText& Text, const FText& Sub, float Life, const FLinearColor& Color, float Size)
{
	AnnounceText = Text;
	AnnounceSub = Sub;
	bFinisherCall = false;
	AnnounceLife = Life;
	AnnounceT = 0.f;
	AnnounceColor = Color;
	AnnounceSize = Size;
}

void SSlopHud::Subtitle(const FText& Text, float Life)
{
	SubtitleText = Text;
	SubtitleT = Life;
}

void SSlopHud::OnEvent(const FSimEvent& E)
{
	const ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	const FFightSim& Sim = M->Sim;
	switch (E.Type)
	{
	case EEvent::RoundStart:
	{
		const bool bLast = Sim.Wins[0] == Sim.RoundsToWin - 1 && Sim.Wins[1] == Sim.RoundsToWin - 1 && E.Round > 1;
		Announce(bLast ? LOCTEXT("Final", "FINAL ROUND") : FText::Format(LOCTEXT("Round", "ROUND {0}"), E.Round), FText::GetEmpty(), 1.5f, SlopUI::Text, 96.f);
		break;
	}
	case EEvent::Fight:
		Announce(LOCTEXT("Fight", "FIGHT"), FText::GetEmpty(), 0.9f, SlopUI::Spark, 132.f);
		break;
	case EEvent::FinishPrompt:
		Announce(FText::Format(LOCTEXT("Finish", "FINISH {0}"), FText::FromString(E.Kind.ToString().ToUpper())), FText::GetEmpty(), 2.2f, SlopUI::Danger, 104.f);
		break;
	case EEvent::Finisher:
		Announce(FText::GetEmpty(), FText::GetEmpty(), 0.f, SlopUI::Text, 120.f);
		NoticeT = 0.f;
		break;
	case EEvent::FinisherEnd:
		Announce(E.Move.ToString().StartsWith(TEXT("stage_")) ? LOCTEXT("StageHearted", "STAGE HEARTED") : LOCTEXT("Hearted", "HEARTED"), FText::GetEmpty(), 3.0f, SlopUI::Spark, 108.f);
		bFinisherCall = true;
		break;
	case EEvent::Timeout:
		Announce(LOCTEXT("Time", "TIME"), FText::GetEmpty(), 1.2f, SlopUI::Warn, 96.f);
		break;
	case EEvent::RoundEnd:
		if (E.How == TEXT("double_ko"))
		{
			Announce(LOCTEXT("DoubleKO", "DOUBLE KO"), FText::GetEmpty(), 2.0f, SlopUI::Warn, 96.f);
		}
		else if (E.Winner < 0)
		{
			Announce(LOCTEXT("Draw", "DRAW"), FText::GetEmpty(), 2.0f, SlopUI::Warn, 96.f);
		}
		break;
	case EEvent::WinPose:
	{
		const FText Wins = FText::Format(LOCTEXT("Wins", "{0} WINS"), NameText[FMath::Clamp(E.Who, 0, 1)]);
		if (AnnounceLife > AnnounceT && bFinisherCall)
		{
			AnnounceSub = Wins;
		}
		else
		{
			Announce(Wins, E.bFlawless ? LOCTEXT("Flawless", "FLAWLESS VICTORY") : FText::GetEmpty(), 2.6f, SlopUI::Text, 84.f);
		}
		break;
	}
	case EEvent::KO:
		if (!E.bFinish)
		{
			Announce(LOCTEXT("KO", "K.O."), FText::GetEmpty(), 1.6f, SlopUI::Danger, 132.f);
		}
		break;
	case EEvent::Hit:
	{
		const int32 By = FMath::Clamp(E.By, 0, 1);
		if (E.bKill)
		{
			const FMoveDef* Mv = Sim.Fighters[By].Def->Move(E.Move);
			Announce(FText::FromString(Mv ? Mv->Name.ToUpper() : FString()), LOCTEXT("InstantKill", "INSTANT KILL"), 2.2f, SlopUI::Qhi, 92.f);
		}
		if (E.Combo >= 2 || E.bCounter)
		{
			const FFighter& V = Sim.Fighters[FMath::Clamp(E.Who, 0, 1)];
			bComboCounter[By] = E.bCounter;
			ComboCount[By] = E.Combo >= 2 ? FText::AsNumber(E.Combo) : FText::GetEmpty();
			ComboLine[By] = E.Combo >= 2 ? FText::Format(LOCTEXT("Combo", "HITS   {0}%"), FMath::RoundToInt(100.0 * V.ComboDamage / FMath::Max(1, V.MaxHealth()))) : FText::GetEmpty();
			// The counter warms as the combo grows, and jumps with every hit.
			ComboColor[By] = E.Combo < 4 ? SlopUI::Text : (E.Combo < 7 ? SlopUI::Warn : SlopUI::Spark);
			ComboT[By] = 1.6f;
			ComboPop[By] = 1.f;
		}
		break;
	}
	case EEvent::Move:
		if (E.Who >= 0 && E.Who <= 1)
		{
			// An instant kill begun is said aloud under his bar: its startup is the opponent's chance.
			const FMoveDef* Mv = Sim.Fighters[E.Who].Def->Move(E.Move);
			if (Mv && Mv->bKill)
			{
				FormText[E.Who] = FText::FromString(Mv->Name.ToUpper());
				FormColor[E.Who] = SlopUI::Danger;
				FormT[E.Who] = 1.6f;
			}
		}
		break;
	case EEvent::FinisherRange:
		NoticeLine = FText::Format(LOCTEXT("Range", "Wrong distance: {0} range"), FText::FromName(E.Want));
		NoticeT = 2.0f;
		break;
	case EEvent::Form:
	{
		const int32 W = FMath::Clamp(E.Who, 0, 1);
		const FFighterDef* Copied = GetContent().Fighter(E.Move);
		const FName K = E.Kind;
		if (K == TEXT("borrowed"))
		{
			// The opponent's face (Qhi-enhanced) or a shape from his memory.
			const bool bFace = E.How == TEXT("copied");
			FormText[W] = FText::Format(bFace ? LOCTEXT("Borrowed", "BORROWED FACE: {0}") : LOCTEXT("Remembered", "SHAPE: {0}"), FText::FromString(Copied ? Copied->Name.ToUpper() : FString()));
			FormColor[W] = SlopUI::Qhi;
		}
		else if (K == TEXT("own"))
		{
			FormText[W] = LOCTEXT("OwnFace", "OWN FACE");
			FormColor[W] = SlopUI::Qhi;
		}
		else if (K == TEXT("spent") || K == TEXT("broken"))
		{
			FormText[W] = K == TEXT("broken") ? LOCTEXT("ShapeBreaks", "THE SHAPE BREAKS: QHI SPENT") : LOCTEXT("QhiSpent", "QHI SPENT");
			FormColor[W] = SlopUI::Danger;
		}
		else if (K == TEXT("recentred"))
		{
			FormText[W] = LOCTEXT("Recentred", "QHI RECENTRED");
			FormColor[W] = SlopUI::Qhi;
		}
		else if (K == TEXT("failed"))
		{
			FormText[W] = LOCTEXT("Failed", "SHAPESHIFT BROKEN");
			FormColor[W] = SlopUI::Danger;
		}
		else if (K == TEXT("refused"))
		{
			FormText[W] = E.How == TEXT("qhi") ? LOCTEXT("NeedQhi", "NOT ENOUGH QHI")
				: E.How == TEXT("mirror") ? LOCTEXT("NoMirror", "A MIRROR SHOWS ONLY ITSELF")
				: E.How == TEXT("same") ? LOCTEXT("SameShape", "ALREADY IN THAT SHAPE")
				: LOCTEXT("NoShape", "NO SHAPE TO BORROW");
			FormColor[W] = SlopUI::Dim;
		}
		FormT[W] = 1.8f;
		break;
	}
	default:
		break;
	}
}

void SSlopHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	const float Dt = M->IsPaused() ? 0.f : InDeltaTime;
	Arrive = M->InPrelude() ? 0.f : (SlopUI::ReducedMotion() ? 1.f : FMath::FInterpConstantTo(Arrive, 1.f, InDeltaTime, 2.4f));
	const int32 Seconds = M->Sim.bTraining || M->Sim.RoundLength <= 0 ? -1 : M->Sim.TimerSeconds();
	if (Seconds != ShownSeconds || TimerText.IsEmpty())
	{
		ShownSeconds = Seconds;
		TimerText = Seconds >= 0 ? FText::AsNumber(Seconds) : FText::FromString(TEXT("∞"));
	}
	AnnounceT += Dt;
	SubtitleT = FMath::Max(0.f, SubtitleT - Dt);
	NoticeT = FMath::Max(0.f, NoticeT - Dt);
	for (int32 I = 0; I < 2; ++I)
	{
		ComboT[I] = FMath::Max(0.f, ComboT[I] - Dt);
		FormT[I] = FMath::Max(0.f, FormT[I] - Dt);
		if (M->Sim.Fighters[I].UsesQhi())
		{
			const float Q = (float)(M->Sim.Fighters[I].Qhi / M->Sim.Fighters[I].QhiRules().Max);
			QhiTrail[I] = Q > QhiTrail[I] ? Q : FMath::Max(Q, QhiTrail[I] - Dt * 0.6f);
		}
		ComboPop[I] = FMath::Max(0.f, ComboPop[I] - Dt * 6.f);
		const FFighter& F = M->Sim.Fighters[I];
		if (ShapeOf[I] != F.Def)
		{
			ShapeOf[I] = F.Def;
			ShapeText[I] = FText::Format(LOCTEXT("QhiHolds", "QHI   AS {0}"), FText::FromString(F.Def->Name.ToUpper()));
		}
		const float H = (float)F.Health / FMath::Max(1, F.MaxHealth());
		Trail[I] = H > Trail[I] ? H : FMath::Max(H, Trail[I] - Dt * 0.45f);
	}
	if (M->Sim.TickCount != LastTick)
	{
		LastTick = M->Sim.TickCount;
		for (int32 I = 0; I < 2; ++I)
		{
			const FInputBuffer& B = M->Sim.Fighters[I].Input;
			const int32 D = B.Dir(0);
			const int32 Btn = B.Pressed(0) | (B.Held(0) & (BtnBL | BtnRN));
			TArray<FHist>& H = History[I];
			if (H.Num() > 0 && H[0].Dir == D && H[0].Btn == Btn)
			{
				H[0].Ticks = FMath::Min(99, H[0].Ticks + 1);
			}
			else
			{
				H.Insert({D, Btn, 1}, 0);
				if (H.Num() > 18)
				{
					H.Pop();
				}
			}
		}
	}
}

void SSlopHud::PaintText(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FText& T, const FSlateFontInfo& F,
	const FVector2D& Pos, const FLinearColor& C, int32 Align, float Scale) const
{
	if (T.IsEmpty())
	{
		return;
	}
	// Text is rasterised at its final scale, so an animated scale (the announcements' shrink, the
	// combo count's pop) would make new glyphs every frame and overflow the font cache: keep to
	// sixteenths, a handful of sizes the eye still reads as smooth.
	Scale = FMath::RoundToFloat(Scale * 16.f) / 16.f;
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Size = Measure->Measure(T, F) * Scale;
	FVector2D P = Pos;
	if (Align == 1) P.X -= Size.X * 0.5;
	if (Align == 2) P.X -= Size.X;
	FSlateFontInfo Shadowed = F;
	Shadowed.OutlineSettings.OutlineSize = FMath::Max(2, (int32)(F.Size / 30));
	Shadowed.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.85f * C.A);
	FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(Size, FSlateLayoutTransform(Scale, P)), T, Shadowed, ESlateDrawEffect::None, C);
}

void SSlopHud::PaintCard(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FText& T, const FSlateFontInfo& F, const FVector2D& Pos, const FLinearColor& C, int32 Align) const
{
	if (T.IsEmpty())
	{
		return;
	}
	const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, F);
	const FVector2D Pad(22.f, 9.f);
	FVector2D At = Pos - Pad;
	if (Align == 1) At.X -= Size.X * 0.5;
	if (Align == 2) At.X -= Size.X;
	const FPaintGeometry Box = G.ToPaintGeometry(Size + Pad * 2.0, FSlateLayoutTransform(At));
	FSlateDrawElement::MakeBox(Out, Layer, Box, SlopUI::Box(), ESlateDrawEffect::None, SlopUI::Panel);
	FSlateDrawElement::MakeBox(Out, Layer + 1, Box, SlopUI::Brush(SlopUI::ETex::Frame), ESlateDrawEffect::None, SlopUI::Line);
	PaintText(Out, G, Layer + 2, T, F, Pos, C, Align);
}

void SSlopHud::PaintMeters(FSlateWindowElementList& Out, const FGeometry& G, int32 I, float X, float W, float Y, int32 Layer) const
{
	const FFighter& F = Match->Sim.Fighters[I];
	const FSlateBrush* Fill = SlopUI::Brush(SlopUI::ETex::BarFill);
	const float MeterX = I == 0 ? X : X + W - MeterW;
	auto Rect = [&](float X0, float Y0, float Wd, float Ht, const FLinearColor& C, int32 L, const FSlateBrush* B)
	{
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X0, Y0))), B, ESlateDrawEffect::None, C);
	};
	auto Meter = [&](float MY, float H, float Value, float TrailValue, const FLinearColor& C)
	{
		Rect(MeterX, MY, MeterW, H, SlopUI::Ink.CopyWithNewOpacity(0.8f), Layer, SlopUI::Box());
		if (TrailValue > Value)
		{
			const float TW = MeterW * TrailValue;
			Rect(I == 0 ? X : X + W - TW, MY, TW, H, FLinearColor(1.f, 1.f, 1.f, 0.35f), Layer + 1, SlopUI::Box());
		}
		const float FW = MeterW * Value;
		Rect(I == 0 ? X : X + W - FW, MY, FW, H, C, Layer + 2, Fill);
	};
	// Labels sit on the inner side of a meter, so a filling bar never runs through them.
	const FVector2D LabelAt(I == 0 ? MeterX + MeterW + 12.f : MeterX - 12.f, Y - 7.f);
	const int32 LabelAlign = I == 0 ? 0 : 2;
	const FSlateFontInfo Small = SlopUI::Caps(15.f);
	const FHudReadout R = HudReadout(F);
	switch (R.Meter)
	{
	case EHudMeter::Vessel:
	{
		// The Fragmented Vessel filling: stop him before this reaches the end.
		const float Pulse = 0.7f + 0.3f * FMath::Sin((float)(FPlatformTime::Seconds() * 14.0));
		Meter(Y, 11.f, R.Vessel, 0.f, SlopUI::Spark.CopyWithNewOpacity(Pulse));
		PaintText(Out, G, Layer + 3, LOCTEXT("Vessel", "FRAGMENTED VESSEL"), Small, LabelAt, SlopUI::Spark, LabelAlign);
		break;
	}
	case EHudMeter::VesselBroken:
		PaintText(Out, G, Layer + 3, LOCTEXT("VesselBroken", "BROKEN"), Small, LabelAt, SlopUI::Good, LabelAlign);
		break;
	case EHudMeter::Run:
	case EHudMeter::RunAndQhi:
		// The Grandmaster has a run meter only in a borrowed shape, with the Qhi that holds it beneath.
		Meter(Y, 8.f, R.Run, 0.f, SlopUI::Run);
		PaintText(Out, G, Layer + 3, LOCTEXT("Run", "RUN"), Small, LabelAt, SlopUI::Dim, LabelAlign);
		if (R.Meter == EHudMeter::RunAndQhi)
		{
			Meter(Y + 12.f, 5.f, R.Qhi, QhiTrail[I], SlopUI::Qhi);
			PaintText(Out, G, Layer + 3, ShapeText[I], Small, FVector2D(LabelAt.X, Y + 10.f), SlopUI::Qhi, LabelAlign);
		}
		break;
	case EHudMeter::Qhi:
	{
		// Spent: a dull bar refilling toward the mark where the Qhi recentres, pulsing.
		const float Pulse = R.bSpent ? 0.65f + 0.35f * FMath::Sin((float)Match->Sim.TickCount * 0.25f) : 1.f;
		Meter(Y, 9.f, R.Qhi, QhiTrail[I], R.bSpent ? SlopUI::Danger * Pulse : SlopUI::Qhi);
		// A notch and a tooth, so the mark reads without leaning on its colour.
		const float MX = I == 0 ? X + MeterW * R.Mark : X + W - MeterW * R.Mark;
		Rect(MX - 1.f, Y - 4.f, 2.f, 17.f, FLinearColor::White.CopyWithNewOpacity(0.9f), Layer + 3, SlopUI::Box());
		Rect(MX - 5.f, Y - 12.f, 10.f, 10.f, FLinearColor::White.CopyWithNewOpacity(0.9f), Layer + 3, SlopUI::Brush(SlopUI::ETex::Spike));
		PaintText(Out, G, Layer + 3, R.bSpent ? LOCTEXT("QhiSpentLabel", "QHI SPENT") : (R.bBorrowed ? ShapeText[I] : LOCTEXT("Qhi", "QHI")), Small, LabelAt,
			R.bSpent ? SlopUI::Danger : SlopUI::Qhi, LabelAlign);
		break;
	}
	default:
		break;
	}
}

void SSlopHud::PaintBars(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 Layer) const
{
	const ASlopMatch* M = Match.Get();
	const FFightSim& Sim = M->Sim;
	const float CX = (float)Size.X * 0.5f;
	const float Slide = -44.f * (1.f - SlopUI::EaseOut(Arrive));
	const float Alpha = SlopUI::EaseOut(Arrive);
	auto Rect = [&](float X0, float Y0, float Wd, float Ht, const FLinearColor& C, int32 L, const FSlateBrush* B)
	{
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X0, Y0 + Slide))), B, ESlateDrawEffect::None, C.CopyWithNewOpacity(C.A * Alpha));
	};
	const FSlateBrush* Fill = SlopUI::Brush(SlopUI::ETex::BarFill);
	const FSlateBrush* Plain = SlopUI::Box();
	const float Beat = (float)FPlatformTime::Seconds();
	for (int32 I = 0; I < 2; ++I)
	{
		const FFighter& F = Sim.Fighters[I];
		const bool bBoss = F.BaseDef->bBoss || F.bGuardian;
		const float X = I == 0 ? CX - FrameInner - FrameW + SlotIn : CX + FrameInner + SlotOut;
		const FHudReadout Read = HudReadout(F);
		const float H = Read.Health;
		Rect(X - 1.f, SlotTop - 1.f, SlotW + 2.f, SlotH + 2.f, SlopUI::Ink.CopyWithNewOpacity(0.92f), Layer, Plain);
		// Bars drain toward their outer ends, as in the arcade.
		const float TrailW = SlotW * Trail[I];
		const float HW = SlotW * H;
		Rect(I == 0 ? X : X + SlotW - TrailW, SlotTop, TrailW, SlotH, SlopUI::Spark, Layer + 1, Fill);
		const bool bLow = Read.bLow;
		const float Pulse = bLow && !SlopUI::ReducedMotion() ? 0.82f + 0.18f * FMath::Sin(Beat * (H <= 0.1f ? 11.f : 6.f)) : 1.f;
		Rect(I == 0 ? X : X + SlotW - HW, SlotTop, HW, SlotH, (bLow ? SlopUI::Danger : SlopUI::Text * 0.8f) * FLinearColor(Pulse, Pulse, Pulse, 1.f), Layer + 2, Fill);
		// Quarters scored across the bar, so a health is a measure as well as a colour.
		for (int32 Q = 1; Q < 4; ++Q)
		{
			Rect(X + SlotW * Q * 0.25f - 0.75f, SlotTop, 1.5f, SlotH, SlopUI::Ink.CopyWithNewOpacity(0.6f), Layer + 3, Plain);
		}
		// The frame, with a boss's in the card's violet.
		Rect(I == 0 ? CX - FrameInner - FrameW : CX + FrameInner, FrameTop, FrameW, FrameH, bBoss ? FLinearColor(0.95f, 0.72f, 1.25f) : FLinearColor::White, Layer + 4,
			SlopUI::Brush(I == 0 ? SlopUI::ETex::BarFrame : SlopUI::ETex::BarFrameFlipped));
		const float NameX = I == 0 ? X + 4.f : X + SlotW - 4.f;
		const FLinearColor NameC = SlopUI::Text.CopyWithNewOpacity(Alpha);
		PaintText(Out, G, Layer + 5, NameText[I], SlopUI::Display(bBoss ? 29.f : 25.f), FVector2D(NameX, SlotTop - (bBoss ? 41.f : 35.f) + Slide), NameC, I == 0 ? 0 : 2);
		// Who plays them, on the bar's inner side: enough to tell a mirror match apart at a glance.
		PaintText(Out, G, Layer + 5, TagText[I], SlopUI::Caps(15.f), FVector2D(I == 0 ? X + SlotW - 4.f : X + 4.f, SlotTop - 23.f + Slide), SlopUI::Dim.CopyWithNewOpacity(Alpha), I == 0 ? 2 : 0);
		if (Alpha > 0.5f)
		{
			PaintMeters(Out, G, I, X, SlotW, MeterTop + Slide, Layer + 5);
		}
		// Round wins by the clock, hollow until won.
		for (int32 R = 0; R < Sim.RoundsToWin; ++R)
		{
			const bool bWon = Sim.Wins[I] > R;
			const float CXR = I == 0 ? X + SlotW - 13.f - R * 27.f : X + 13.f + R * 27.f;
			Rect(CXR - 11.f, MeterTop - 3.f, 22.f, 22.f, bWon ? SlopUI::Spark : SlopUI::Dim, Layer + 5, SlopUI::Brush(bWon ? SlopUI::ETex::Spike : SlopUI::ETex::SpikeHollow));
		}
	}
	// The timer on its plaque.
	Rect(CX - 75.f, 4.f, 150.f, 116.f, FLinearColor::White, Layer + 5, SlopUI::Brush(SlopUI::ETex::TimerPlate));
	const bool bClock = ShownSeconds >= 0;
	const bool bRunningOut = bClock && ShownSeconds <= 10;
	const float TimeAlpha = bRunningOut && !SlopUI::ReducedMotion() ? 0.78f + 0.22f * FMath::Sin(Beat * 7.f) : 1.f;
	PaintText(Out, G, Layer + 7, TimerText, SlopUI::Call(60.f), FVector2D(CX, 16.f + Slide), (bRunningOut ? SlopUI::Danger : SlopUI::Text).CopyWithNewOpacity(Alpha * TimeAlpha), 1);
}

void SSlopHud::PaintBoxes(FSlateWindowElementList& Out, const FGeometry& AllottedGeometry, int32 Layer) const
{
	const ASlopMatch* M = Match.Get();
	APlayerController* PC = UGameplayStatics::GetPlayerController(const_cast<ASlopMatch*>(M), 0);
	FVector2D Viewport(0, 0);
	if (!PC || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	GEngine->GameViewport->GetViewportSize(Viewport);
	if (Viewport.X <= 0 || Viewport.Y <= 0)
	{
		return;
	}
	const FVector2D Local = AllottedGeometry.GetLocalSize();
	TArray<TPair<FRect, FLinearColor>> Boxes;
	M->CollectBoxes(Boxes);
	TArray<FVector2D> Pts;
	for (const TPair<FRect, FLinearColor>& B : Boxes)
	{
		const FRect& R = B.Key;
		const FVector2D Corners[] = {{R.X, R.Y}, {R.X + R.W, R.Y}, {R.X + R.W, R.Y + R.H}, {R.X, R.Y + R.H}};
		Pts.Reset();
		bool bOk = true;
		for (const FVector2D& C : Corners)
		{
			FVector2D S;
			bOk &= PC->ProjectWorldLocationToScreen(Slop::Anim::WorldToUE(FVector(C.X, C.Y, 0.0)), S, true);
			Pts.Add(S / Viewport * Local);
		}
		if (bOk)
		{
			const FVector2D First = Pts[0];
			Pts.Add(First);
			FSlateDrawElement::MakeLines(Out, Layer, AllottedGeometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, B.Value, true, 2.f);
		}
	}
}

void SSlopHud::PaintCalls(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 L) const
{
	const float CX = (float)Size.X * 0.5f;
	const float VH = (float)Size.Y;
	const ASlopMatch* M = Match.Get();
	if (AnnounceLife > 0.f && AnnounceT < AnnounceLife)
	{
		const float K = AnnounceT / AnnounceLife;
		const float In = FMath::Clamp(AnnounceT / 0.14f, 0.f, 1.f);
		const float Fade = FMath::Min(In, K > 0.8f ? (1.f - K) / 0.2f : 1.f);
		const float Grow = SlopUI::ReducedMotion() ? 1.f : 1.14f - 0.14f * SlopUI::EaseOut(In);
		// A dark band across the stage, then the call on it: read against any ground.
		const float Y = VH * 0.36f;
		const float BandH = 300.f;
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(Size.X, BandH), FSlateLayoutTransform(FVector2D(0.f, Y - BandH * 0.5f))), SlopUI::Brush(SlopUI::ETex::Band), ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, Fade));
		const float RuleW = 1120.f;
		for (float Side : {-1.f, 1.f})
		{
			FSlateDrawElement::MakeBox(Out, L + 1, G.ToPaintGeometry(FVector2D(RuleW, 4.f), FSlateLayoutTransform(FVector2D(CX - RuleW * 0.5f, Y + Side * AnnounceSize * 0.62f))),
				SlopUI::Brush(SlopUI::ETex::Rule), ESlateDrawEffect::None, SlopUI::Line.CopyWithNewOpacity(0.8f * Fade));
		}
		PaintText(Out, G, L + 2, AnnounceText, SlopUI::Call(AnnounceSize), FVector2D(CX, Y - AnnounceSize * 0.68f * Grow), AnnounceColor.CopyWithNewOpacity(Fade), 1, Grow);
		PaintText(Out, G, L + 2, AnnounceSub, SlopUI::Display(42.f), FVector2D(CX, Y + AnnounceSize * 0.7f), SlopUI::Spark.CopyWithNewOpacity(Fade), 1);
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const float Edge = I == 0 ? CX - FrameInner - FrameW + SlotIn + 4.f : CX + FrameInner + SlotOut + SlotW - 4.f;
		const int32 Align = I == 0 ? 0 : 2;
		if (FormT[I] > 0.f)
		{
			PaintText(Out, G, L, FormText[I], SlopUI::Display(29.f), FVector2D(Edge, MeterTop + 26.f), FormColor[I].CopyWithNewOpacity(FMath::Clamp(FormT[I] / 0.4f, 0.f, 1.f)), Align);
		}
		if (ComboT[I] > 0.f)
		{
			const float A = FMath::Clamp(ComboT[I] / 0.4f, 0.f, 1.f);
			const FLinearColor C = ComboColor[I].CopyWithNewOpacity(A);
			float Y = 190.f;
			if (bComboCounter[I])
			{
				PaintText(Out, G, L, LOCTEXT("Counter", "COUNTER"), SlopUI::Caps(25.f), FVector2D(Edge, Y), SlopUI::Danger.CopyWithNewOpacity(A), Align);
				Y += 30.f;
			}
			PaintText(Out, G, L, ComboCount[I], SlopUI::Call(74.f), FVector2D(Edge, Y), C, Align, 1.f + ComboPop[I] * 0.22f);
			PaintText(Out, G, L, ComboLine[I], SlopUI::Caps(23.f), FVector2D(Edge, Y + 90.f), SlopUI::Text.CopyWithNewOpacity(A), Align);
		}
	}
	if (SubtitleT > 0.f)
	{
		PaintCard(Out, G, L, SubtitleText, SlopUI::Body(34.f), FVector2D(CX, VH - 196.f), SlopUI::Text.CopyWithNewOpacity(FMath::Clamp(SubtitleT / 0.5f, 0.f, 1.f)), 1);
	}
	const FText Prompt = M->Guide ? M->Guide->Prompt() : PromptText;
	PaintCard(Out, G, L + 3, Prompt, SlopUI::Strong(M->Guide ? 28.f : 25.f), FVector2D(CX, VH - 120.f), M->Guide ? SlopUI::Text : SlopUI::Dim, 1);
	if (NoticeT > 0.f)
	{
		PaintCard(Out, G, L + 3, NoticeLine, SlopUI::Strong(27.f), FVector2D(CX, VH - 256.f), SlopUI::Warn, 1);
	}
}

void SSlopHud::PaintTraining(FSlateWindowElementList& Out, const FGeometry& G, const FVector2D& Size, int32 L) const
{
	const ASlopMatch* M = Match.Get();
	const float CX = (float)Size.X * 0.5f;
	const float Left = CX - 864.f;
	const float Right = CX + 864.f;
	// These overlays are meant to be dense: each sits on its own panel, in plain type, so they read
	// as instruments apart from the fight's own marks.
	if (M->Dummy && !M->Guide)
	{
		PaintCard(Out, G, L, FText::Format(LOCTEXT("Dummy", "Dummy: {0}    {1} to change    {2} to reset"), FText::FromString(M->Dummy->ModeName()), SlopUI::NextName(GI->Router->Glyphs()), SlopUI::ResetName(GI)),
			SlopUI::Body(23.f), FVector2D(Left + 22.f, 146.f), SlopUI::Text, 0);
	}
	if (bInputDisplay)
	{
		static const TCHAR* Arrows[] = {TEXT(""), TEXT("↙"), TEXT("↓"), TEXT("↘"), TEXT("←"), TEXT("·"), TEXT("→"), TEXT("↖"), TEXT("↑"), TEXT("↗")};
		const FSlateFontInfo F = SlopUI::Strong(21.f);
		for (int32 I = 0; I < 2; ++I)
		{
			const TArray<FHist>& H = History[I];
			const float W = 196.f;
			const float X = I == 0 ? Left : Right - W;
			if (H.Num() > 0)
			{
				FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(W, H.Num() * 27.f + 12.f), FSlateLayoutTransform(FVector2D(X, 196.f))), SlopUI::Box(), ESlateDrawEffect::None, SlopUI::Panel.CopyWithNewOpacity(0.6f));
			}
			for (int32 K = 0; K < H.Num(); ++K)
			{
				FString S = Arrows[FMath::Clamp(H[K].Dir, 1, 9)];
				const int32 B = H[K].Btn;
				if (B & BtnHP) S += TEXT(" HP");
				if (B & BtnLP) S += TEXT(" LP");
				if (B & BtnHK) S += TEXT(" HK");
				if (B & BtnLK) S += TEXT(" LK");
				if (B & BtnBL) S += TEXT(" BL");
				if (B & BtnRN) S += TEXT(" RN");
				S = SlopUI::ButtonsIn(GI, S, I);
				S += FString::Printf(TEXT("  %d"), H[K].Ticks);
				PaintText(Out, G, L + 1, FText::FromString(S), F, FVector2D(I == 0 ? X + 12.f : X + W - 12.f, 200.f + K * 27.f), SlopUI::Text.CopyWithNewOpacity(1.f - K / 20.f), I == 0 ? 0 : 2);
			}
		}
	}
	if (bShowFrames || bShowStates)
	{
		for (int32 I = 0; I < 2; ++I)
		{
			const FFighter& F = M->Sim.Fighters[I];
			FString S;
			if (bShowStates)
			{
				S += FString::Printf(TEXT("%s  sf %d  hitstop %d  stun %d\nhp %d  run %d  combo %d"), StateName(F.State), F.SF, F.Hitstop, F.Stun, F.Health, (int32)F.RunMeter, F.ComboHits);
				if (F.UsesQhi())
				{
					S += FString::Printf(TEXT("\nqhi %.1f  %s"), F.Qhi, FormName(F.Form));
				}
			}
			if (bShowFrames && F.Move)
			{
				const FMoveDef& Mv = *F.Move;
				if (!S.IsEmpty())
				{
					S += TEXT("\n");
				}
				S += FString::Printf(TEXT("%s  frame %d / %d\nstartup %d  active %d  recovery %d\nblock %+d  hit %s"), *Mv.Name, F.MF, Mv.Total, Mv.Startup, Mv.Active, Mv.Recovery,
					Mv.BlockAdvantage(), Mv.HitAdvantage() >= 999 ? TEXT("KD") : *FString::Printf(TEXT("%+d"), Mv.HitAdvantage()));
			}
			PaintCard(Out, G, L, FText::FromString(S), SlopUI::Body(21.f), FVector2D(I == 0 ? Left + 226.f : Right - 226.f, 200.f), SlopUI::Good, I == 0 ? 0 : 2);
		}
		// What the last exchange actually left, measured, beside the move's listed numbers.
		const Slop::FAdvantageMeter& A = M->Advantage;
		if (bShowFrames && A.bHas && A.By >= 0)
		{
			const FText Who = FText::FromString(M->Sim.Fighters[A.By].Def->Name);
			const FText Line = A.bKnockdown
				? FText::Format(LOCTEXT("AdvKD", "Last exchange: {0}, knockdown"), Who)
				: FText::Format(A.bParried ? LOCTEXT("AdvParry", "Last exchange: {0} {1} from a parry")
					: (A.bBlocked ? LOCTEXT("AdvBlock", "Last exchange: {0} {1} on block") : LOCTEXT("AdvHit", "Last exchange: {0} {1} on hit")),
					Who, FText::FromString(FString::Printf(TEXT("%+d"), A.Value)));
			PaintCard(Out, G, L, Line, SlopUI::Strong(25.f), FVector2D(CX, 134.f), A.Value >= 0 || A.bKnockdown ? SlopUI::Good : SlopUI::Warn, 1);
		}
	}
}

int32 SSlopHud::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ASlopMatch* M = Match.Get();
	// During the staging the stage has the whole screen; the bars arrive as it hands over.
	if (!M || M->InPrelude())
	{
		return LayerId;
	}
	const SlopUI::FStageFrame Frame = SlopUI::StageFrame(AllottedGeometry.GetLocalSize());
	const FVector2D Size = Frame.Size;
	const FGeometry G = AllottedGeometry.MakeChild(Size, FSlateLayoutTransform(Frame.Scale, Frame.Origin));
	if (M->bShowBoxes)
	{
		PaintBoxes(OutDrawElements, AllottedGeometry, LayerId);
	}
	// Every mode shows the bars (the tutorial's steps talk about the health and the run meter).
	PaintBars(OutDrawElements, G, Size, LayerId + 1);
	const int32 L = LayerId + 12;
	PaintTraining(OutDrawElements, G, Size, L);
	PaintCalls(OutDrawElements, G, Size, L + 4);
	return L + 10;
}

#undef LOCTEXT_NAMESPACE
