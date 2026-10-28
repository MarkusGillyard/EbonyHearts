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

void SSlopHud::Construct(const FArguments& InArgs)
{
	GI = InArgs._GI;
	Match = InArgs._Match;
	bInputDisplay = GI && (GI->Settings.bInputDisplay || GI->Flow.Config.Mode == EMode::Training);
	SetCanTick(true);
	if (Match.IsValid())
	{
		Match->OnEvent = [this](const FSimEvent& E) { OnEvent(E); };
		Match->OnAnnounce = [this](const FText& T, const FText& S) { Announce(T, S, 2.4f, SlopUI::Accent, 104.f); };
	}
}

FText SSlopHud::NameOf(int32 Side) const
{
	const ASlopMatch* M = Match.Get();
	if (!M)
	{
		return FText::GetEmpty();
	}
	if (Side == 1 && M->Config.bMirrorDark)
	{
		return LOCTEXT("Reflection", "THE REFLECTION");
	}
	return FText::FromString(M->Sim.Fighters[Side].Def->Name.ToUpper());
}

void SSlopHud::Announce(const FText& Text, const FText& Sub, float Life, const FLinearColor& Color, float Size)
{
	AnnounceText = Text;
	AnnounceSub = Sub;
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
	const bool bStranger = M->Config.Mode == EMode::Stranger;
	if (bStranger && (E.Type == EEvent::RoundStart || E.Type == EEvent::Fight || E.Type == EEvent::WinPose || E.Type == EEvent::RoundEnd || E.Type == EEvent::Timeout))
	{
		return;
	}
	const FFightSim& Sim = M->Sim;
	switch (E.Type)
	{
	case EEvent::Line:
		if (E.bHint)
		{
			PromptText = FText::FromString(E.Text);
		}
		else
		{
			Subtitle(FText::Format(LOCTEXT("Quote", "“{0}”"), FText::FromString(E.Text)), 3.4f);
		}
		break;
	case EEvent::RoundStart:
	{
		const bool bLast = Sim.Wins[0] == Sim.RoundsToWin - 1 && Sim.Wins[1] == Sim.RoundsToWin - 1 && E.Round > 1;
		Announce(bLast ? LOCTEXT("Final", "FINAL ROUND") : FText::Format(LOCTEXT("Round", "ROUND {0}"), E.Round), FText::GetEmpty(), 1.5f, SlopUI::Text, 120.f);
		break;
	}
	case EEvent::Fight:
		Announce(LOCTEXT("Fight", "FIGHT!"), FText::GetEmpty(), 0.9f, SlopUI::Amber, 150.f);
		break;
	case EEvent::FinishPrompt:
		Announce(FText::Format(LOCTEXT("Finish", "FINISH {0}"), FText::FromString(E.Kind.ToString().ToUpper())), FText::GetEmpty(), 2.2f, SlopUI::Rust * 1.3f, 130.f);
		break;
	case EEvent::Finisher:
		Announce(FText::GetEmpty(), FText::GetEmpty(), 0.f, SlopUI::Text, 120.f);
		NoticeT = 0.f;
		break;
	case EEvent::FinisherEnd:
		Announce(E.Move.ToString().StartsWith(TEXT("stage_")) ? LOCTEXT("StageSlop", "STAGE SLOPTALITY") : LOCTEXT("Slop", "SLOPTALITY"), FText::GetEmpty(), 3.0f, SlopUI::Accent, 140.f);
		break;
	case EEvent::Timeout:
		Announce(LOCTEXT("Time", "TIME"), FText::GetEmpty(), 1.2f, SlopUI::Amber, 120.f);
		break;
	case EEvent::RoundEnd:
		if (E.How == TEXT("double_ko"))
		{
			Announce(LOCTEXT("DoubleKO", "DOUBLE KO"), FText::GetEmpty(), 2.0f, SlopUI::Amber, 120.f);
		}
		else if (E.Winner < 0)
		{
			Announce(LOCTEXT("Draw", "DRAW"), FText::GetEmpty(), 2.0f, SlopUI::Amber, 120.f);
		}
		break;
	case EEvent::WinPose:
	{
		const FText Wins = FText::Format(LOCTEXT("Wins", "{0} WINS"), NameOf(E.Who));
		if (AnnounceLife > AnnounceT && (AnnounceText.ToString().Contains(TEXT("SLOPTALITY"))))
		{
			AnnounceSub = Wins;
		}
		else
		{
			Announce(Wins, E.bFlawless ? LOCTEXT("Flawless", "FLAWLESS VICTORY") : FText::GetEmpty(), 2.6f, SlopUI::Text, 104.f);
		}
		break;
	}
	case EEvent::KO:
		if (!E.bFinish)
		{
			Announce(LOCTEXT("KO", "K.O."), FText::GetEmpty(), 1.6f, SlopUI::Rust * 1.3f, 150.f);
		}
		break;
	case EEvent::Hit:
	{
		const int32 By = FMath::Clamp(E.By, 0, 1);
		if (E.Combo >= 2 || E.bCounter)
		{
			const FFighter& V = Sim.Fighters[FMath::Clamp(E.Who, 0, 1)];
			FString S;
			if (E.bCounter)
			{
				S += LOCTEXT("Counter", "COUNTER!").ToString();
			}
			if (E.Combo >= 2)
			{
				if (!S.IsEmpty())
				{
					S += TEXT("\n");
				}
				S += FText::Format(LOCTEXT("Combo", "{0} HIT COMBO\n{1}%"), E.Combo, FMath::RoundToInt(100.0 * V.ComboDamage / FMath::Max(1, V.MaxHealth()))).ToString();
			}
			ComboText[By] = FText::FromString(S);
			// The counter grows warmer as the combo grows, and jumps with every hit.
			ComboColor[By] = E.Combo < 4 ? SlopUI::Amber : (E.Combo < 7 ? SlopUI::Rust * 1.4f : SlopUI::Accent);
			if (E.bCounter && E.Combo < 2)
			{
				ComboColor[By] = SlopUI::Rust * 1.4f;
			}
			ComboT[By] = 1.6f;
			ComboPop[By] = 1.f;
		}
		break;
	}
	case EEvent::FinisherRange:
		NoticeLine = FText::Format(LOCTEXT("Range", "Wrong distance: {0} range"), FText::FromName(E.Want));
		NoticeT = 2.0f;
		break;
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
	AnnounceT += Dt;
	SubtitleT = FMath::Max(0.f, SubtitleT - Dt);
	NoticeT = FMath::Max(0.f, NoticeT - Dt);
	for (int32 I = 0; I < 2; ++I)
	{
		ComboT[I] = FMath::Max(0.f, ComboT[I] - Dt);
		ComboPop[I] = FMath::Max(0.f, ComboPop[I] - Dt * 6.f);
		const FFighter& F = M->Sim.Fighters[I];
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

void SSlopHud::PaintBars(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer) const
{
	const ASlopMatch* M = Match.Get();
	const FFightSim& Sim = M->Sim;
	const FSlateBrush* W = SlopUI::Box();
	const float BarW = 640.f, BarH = 34.f, Top = 30.f;
	const bool bStranger = M->Config.Mode == EMode::Stranger;
	for (int32 I = 0; I < 2; ++I)
	{
		if (bStranger && I == 1)
		{
			continue;
		}
		const FFighter& F = Sim.Fighters[I];
		const float X = I == 0 ? 48.f : 1920.f - 48.f - BarW;
		const float H = (float)F.Health / FMath::Max(1, F.MaxHealth());
		auto Rect = [&](float X0, float Y0, float Wd, float Ht, const FLinearColor& C, int32 L)
		{
			FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(Wd, Ht), FSlateLayoutTransform(FVector2D(X0, Y0))), W, ESlateDrawEffect::None, C);
		};
		Rect(X - 4, Top - 4, BarW + 8, BarH + 8, FLinearColor(0, 0, 0, 0.75f), Layer);
		Rect(X, Top, BarW, BarH, FLinearColor(0.12f, 0.02f, 0.01f, 0.9f), Layer + 1);
		// Bars drain toward the centre of the screen, as in the arcade.
		const float TrailW = BarW * Trail[I];
		const float HW = BarW * H;
		Rect(I == 0 ? X : X + BarW - TrailW, Top, TrailW, BarH, SlopUI::Amber * 0.9f, Layer + 2);
		const FLinearColor HC = H > 0.25f ? SlopUI::Accent * 0.85f : SlopUI::Rust * 1.3f;
		Rect(I == 0 ? X : X + BarW - HW, Top, HW, BarH, HC, Layer + 3);
		PaintText(Out, G, Layer + 4, NameOf(I), SlopUI::Body(26.f), FVector2D(I == 0 ? X + 12 : X + BarW - 12, Top - 1), FLinearColor(0.02f, 0.03f, 0.02f, 1.f), I == 0 ? 0 : 2);
		// The run meter.
		const float RunW = 360.f * (float)(F.RunMeter / FFighter::RunMax);
		const float RY = Top + BarH + 10.f;
		Rect(I == 0 ? X : X + BarW - 360.f, RY, 360.f, 9.f, FLinearColor(0, 0, 0, 0.7f), Layer + 1);
		Rect(I == 0 ? X : X + BarW - RunW, RY, RunW, 9.f, FLinearColor(0.25f, 0.5f, 1.f, 0.95f), Layer + 2);
		PaintText(Out, G, Layer + 3, LOCTEXT("Run", "RUN"), SlopUI::Body(18.f), FVector2D(I == 0 ? X + 370.f : X + BarW - 370.f, RY - 7.f), SlopUI::Dim, I == 0 ? 0 : 2);
		// Round wins.
		for (int32 R = 0; R < Sim.RoundsToWin; ++R)
		{
			const bool bWon = Sim.Wins[I] > R;
			const float CX = I == 0 ? X + BarW - 26.f - R * 30.f : X + 26.f + R * 30.f;
			Rect(CX - 10, RY - 2, 20, 20, FLinearColor(0, 0, 0, 0.8f), Layer + 1);
			Rect(CX - 7, RY + 1, 14, 14, bWon ? SlopUI::Amber : FLinearColor(0.15f, 0.17f, 0.15f, 1.f), Layer + 2);
		}
	}
	// The timer (none against the stranger).
	if (bStranger)
	{
		return;
	}
	const bool bClock = !Sim.bTraining && Sim.RoundLength > 0;
	const FText Timer = bClock ? FText::AsNumber(Sim.TimerSeconds()) : FText::FromString(TEXT("∞"));
	PaintText(Out, G, Layer + 4, Timer, SlopUI::Display(64.f), FVector2D(960.f, 14.f), bClock && Sim.TimerSeconds() <= 10 ? SlopUI::Rust * 1.4f : SlopUI::Text, 1);
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

int32 SSlopHud::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ASlopMatch* M = Match.Get();
	if (!M)
	{
		return LayerId;
	}
	// Everything is laid out on a 1920x1080 canvas scaled to fit the widget and centred, so a
	// 16:10 or 4:3 screen shows all of it (with a margin above and below) and a wider one too.
	const FVector2D Local = AllottedGeometry.GetLocalSize();
	const float Scale = FMath::Min(Local.Y / 1080.f, Local.X / 1920.f);
	const FVector2D Off((Local.X - 1920.f * Scale) * 0.5f, (Local.Y - 1080.f * Scale) * 0.5f);
	const FGeometry G = AllottedGeometry.MakeChild(FVector2D(1920.f, 1080.f), FSlateLayoutTransform(Scale, Off));
	const bool bStranger = M->Config.Mode == EMode::Stranger;
	if (M->bShowBoxes)
	{
		PaintBoxes(OutDrawElements, AllottedGeometry, LayerId);
	}
	// Every mode shows the bars (the tutorial's steps talk about the health and the run meter);
	// against the stranger only his are hidden, as in the original.
	PaintBars(OutDrawElements, G, LayerId);
	const int32 L = LayerId + 10;
	if (AnnounceLife > 0.f && AnnounceT < AnnounceLife)
	{
		const float K = AnnounceT / AnnounceLife;
		const float In = FMath::Clamp(AnnounceT / 0.12f, 0.f, 1.f);
		const float Fade = K > 0.8f ? (1.f - K) / 0.2f : 1.f;
		const float Grow = 1.25f - 0.25f * In;
		FLinearColor C = AnnounceColor;
		C.A = Fade;
		PaintText(OutDrawElements, G, L, AnnounceText, SlopUI::Display(AnnounceSize), FVector2D(960.f, 380.f - (Grow - 1.f) * AnnounceSize * 0.5f), C, 1, Grow);
		FLinearColor S = SlopUI::Amber;
		S.A = Fade;
		PaintText(OutDrawElements, G, L, AnnounceSub, SlopUI::Display(52.f), FVector2D(960.f, 540.f), S, 1);
	}
	for (int32 I = 0; I < 2; ++I)
	{
		if (ComboT[I] > 0.f)
		{
			FLinearColor C = ComboColor[I];
			C.A = FMath::Clamp(ComboT[I] / 0.4f, 0.f, 1.f);
			PaintText(OutDrawElements, G, L, ComboText[I], SlopUI::Display(44.f), FVector2D(I == 0 ? 70.f : 1850.f, 250.f), C, I == 0 ? 0 : 2, 1.f + ComboPop[I] * 0.25f);
		}
	}
	if (SubtitleT > 0.f)
	{
		FLinearColor C = SlopUI::Text;
		C.A = FMath::Clamp(SubtitleT / 0.5f, 0.f, 1.f);
		PaintText(OutDrawElements, G, L, SubtitleText, SlopUI::Body(38.f), FVector2D(960.f, 880.f), C, 1);
	}
	const FText Prompt = M->Guide ? M->Guide->Prompt() : PromptText;
	if (!Prompt.IsEmpty())
	{
		PaintText(OutDrawElements, G, L, Prompt, SlopUI::Body(28.f), FVector2D(960.f, M->Guide ? 930.f : 960.f), M->Guide ? SlopUI::Text : SlopUI::Dim, 1);
	}
	if (NoticeT > 0.f)
	{
		PaintText(OutDrawElements, G, L, NoticeLine, SlopUI::Body(30.f), FVector2D(960.f, 820.f), SlopUI::Amber, 1);
	}
	// Training readouts (in Trials the same buttons move between trials; their prompt says so).
	if (M->Dummy && !M->Guide)
	{
		PaintText(OutDrawElements, G, L, FText::Format(LOCTEXT("Dummy", "Dummy: {0}    ({1} to change, {2} to reset)"),
			FText::FromString(M->Dummy->ModeName()), SlopUI::NextName(GI->Router->Glyphs()), SlopUI::ResetName(GI->Router->Glyphs())),
			SlopUI::Body(24.f), FVector2D(48.f, 112.f), SlopUI::Text, 0);
	}
	if (bInputDisplay)
	{
		static const TCHAR* Arrows[] = {TEXT(""), TEXT("↙"), TEXT("↓"), TEXT("↘"), TEXT("←"), TEXT("·"), TEXT("→"), TEXT("↖"), TEXT("↑"), TEXT("↗")};
		for (int32 I = 0; I < 2; ++I)
		{
			const TArray<FHist>& H = History[I];
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
				S += FString::Printf(TEXT("  %d"), H[K].Ticks);
				FLinearColor C = SlopUI::Text;
				C.A = 1.f - K / 20.f;
				PaintText(OutDrawElements, G, L, FText::FromString(S), SlopUI::Body(22.f), FVector2D(I == 0 ? 48.f : 1872.f, 170.f + K * 28.f), C, I == 0 ? 0 : 2);
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
				S += FString::Printf(TEXT("%s  sf %d  hitstop %d  stun %d\nhp %d  run %d  combo %d\n"), StateName(F.State), F.SF, F.Hitstop, F.Stun, F.Health, (int32)F.RunMeter, F.ComboHits);
			}
			if (bShowFrames && F.Move)
			{
				const FMoveDef& Mv = *F.Move;
				S += FString::Printf(TEXT("%s  frame %d / %d\nstartup %d  active %d  recovery %d\nblock %+d  hit %s"), *Mv.Name, F.MF, Mv.Total, Mv.Startup, Mv.Active, Mv.Recovery,
					Mv.BlockAdvantage(), Mv.HitAdvantage() >= 999 ? TEXT("KD") : *FString::Printf(TEXT("%+d"), Mv.HitAdvantage()));
			}
			PaintText(OutDrawElements, G, L, FText::FromString(S), SlopUI::Body(22.f), FVector2D(I == 0 ? 460.f : 1460.f, 112.f), SlopUI::Accent, I == 0 ? 0 : 2);
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
			PaintText(OutDrawElements, G, L, Line, SlopUI::Body(26.f), FVector2D(960.f, 150.f), A.Value >= 0 || A.bKnockdown ? SlopUI::Accent : SlopUI::Amber, 1);
		}
	}
	return L + 2;
}

#undef LOCTEXT_NAMESPACE
