#include "UI/SlopScreens.h"
#include "UI/SlopStyle.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopMatch.h"
#include "View/SlopShowroom.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SlopScreensFlow"

using namespace Slop;

namespace SlopScreensFlowImpl
{
	const FFighterDef* Def(FName Id) { return GetContent().Fighter(Id); }
	TSharedPtr<FJsonObject> StoryOf(FName Fighter) { return Json::Obj(Json::Obj(GetContent().Story, TEXT("fighters")), *Fighter.ToString()); }

	FText StageName(FName Id)
	{
		if (Id == TEXT("rendering_yards")) return LOCTEXT("Yards", "The Rendering Yards");
		if (Id == TEXT("drowned_chapel")) return LOCTEXT("Chapel", "The Drowned Chapel");
		if (Id == TEXT("stilt_market")) return LOCTEXT("Market", "Lowmere Stilt Market");
		if (Id == TEXT("roost")) return LOCTEXT("Roost", "The Slopbat's Roost");
		return FText::FromName(Id);
	}

	FText StageBlurb(FName Id)
	{
		if (Id == TEXT("rendering_yards")) return LOCTEXT("YardsB", "Tallowmarket's iron floor between great vats of glowing slop, catwalks and furnace mouths. Gorm's home.");
		if (Id == TEXT("drowned_chapel")) return LOCTEXT("ChapelB", "Lowmere's sunken church: ankle-deep water on stone flags, broken pews, candles and a rose window full of moonlight. Mirela's home.");
		if (Id == TEXT("stilt_market")) return LOCTEXT("MarketB", "The market on stilts over the bog at night: planks and rope rails, strings of paper lanterns, mist. Bram's beat.");
		if (Id == TEXT("roost")) return LOCTEXT("RoostB", "Beneath the Sump: a ledge over a lake of slop, tar falls, bone and the drowned hoard, lit by green storm light.");
		return FText::GetEmpty();
	}

	TSharedRef<SWidget> Frame(const TSharedRef<SWidget>& Canvas, TFunction<FText()> HintFn, bool bBackdrop = true)
	{
		TSharedRef<SOverlay> O = SNew(SOverlay);
		if (bBackdrop)
		{
			O->AddSlot()[SlopUI::Backdrop()];
		}
		O->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)[Canvas]]];
		O->AddSlot()[SlopUI::Hint(HintFn)];
		return O;
	}

	// --- Fighter select ------------------------------------------------------------------------------

	class FSelect : public FSlopScreen
	{
	public:
		EMode Mode;
		TArray<FName> Ids;
		int32 Cursor[2] = {0, 1};
		bool bLocked[2] = {false, false};
		bool bCostume[2] = {false, false};
		// Training: after the player's fighter, the dummy.
		bool bPickingDummy = false;
		static constexpr int32 Cols = 3;

		explicit FSelect(EMode InMode) : Mode(InMode)
		{
			for (const FName& Id : FContent::Roster())
			{
				if (Def(Id))
				{
					Ids.Add(Id);
				}
			}
		}

		bool TwoSides() const { return Mode == EMode::Versus; }

		virtual void Enter() override
		{
			GI->Router->SetSolo(!TwoSides());
			GI->Audio->PlayMusic(TEXT("music_select"));
			const FMatchConfig& C = GI->Flow.Config;
			Cursor[0] = FMath::Max(0, Ids.IndexOfByKey(C.P1));
			Cursor[1] = TwoSides() ? FMath::Max(0, Ids.IndexOfByKey(C.P2)) : 1;
			if (GI->AutoPlayLevel >= 0)
			{
				// Unattended runs pick their fighters at random, for variety.
				Cursor[0] = FMath::RandRange(0, Ids.Num() - 1);
				Cursor[1] = FMath::RandRange(0, Ids.Num() - 1);
			}
		}

		TSharedRef<SWidget> Tile(int32 I)
		{
			const FFighterDef* D = Def(Ids[I]);
			auto Border = [this, I]()
			{
				const bool b0 = Cursor[0] == I && (!bPickingDummy || !bLocked[0]);
				const bool b1 = (TwoSides() && Cursor[1] == I) || (bPickingDummy && Cursor[1] == I);
				if (b0 && b1) return FLinearColor(0.9f, 0.8f, 0.3f);
				if (b0) return SlopUI::Accent;
				if (b1) return SlopUI::Amber;
				return SlopUI::PanelEdge;
			};
			TSharedRef<SOverlay> Face = SNew(SOverlay);
			if (const FSlateBrush* Portrait = SlopUI::Portrait(Ids[I]))
			{
				Face->AddSlot()[SNew(SImage).Image(Portrait)];
			}
			else
			{
				Face->AddSlot().VAlign(VAlign_Center).HAlign(HAlign_Center)
				[
					SlopUI::Title(FText::FromString(D->Name.Left(1)), 120.f, FLinearColor(FColor::FromHex(Json::Str(D->Look, TEXT("glow"), TEXT("#a8db38")))))
				];
			}
			// The name along the bottom, over a band that darkens the portrait under it.
			Face->AddSlot().VAlign(VAlign_Bottom)
			[
				SNew(SBorder).BorderImage(SlopUI::Box()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.62f)).Padding(FMargin(4, 4)).HAlign(HAlign_Center)
				[
					SlopUI::Label(FText::FromString(D->Name.ToUpper()), 22.f, SlopUI::Text)
				]
			];
			return SNew(SBox).WidthOverride(198).HeightOverride(238)
			[
				SNew(SBorder).BorderImage(SlopUI::Rounded()).BorderBackgroundColor_Lambda(Border).Padding_Lambda([this, I]()
				{
					const bool bOn = Cursor[0] == I || ((TwoSides() || bPickingDummy) && Cursor[1] == I);
					return FMargin(bOn ? 6.f : 3.f);
				})
				.OnMouseButtonDown_Lambda([this, I](const FGeometry&, const FPointerEvent&) { Cursor[0] = I; Confirm(0); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(SlopUI::Box()).BorderBackgroundColor(FLinearColor(0.05f, 0.07f, 0.06f, 0.92f)).Padding(0)
					[
						Face
					]
				]
			];
		}

		bool SideShown(int32 Side) const { return Side == 0 || TwoSides() || bPickingDummy; }

		TSharedRef<SWidget> Info(int32 Side)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(SlopUI::Display(54.f)).ColorAndOpacity(Side == 0 ? SlopUI::Accent : SlopUI::Amber).ShadowOffset(FVector2D(2, 2))
					.Text_Lambda([this, Side]() { return SideShown(Side) ? FText::FromString(Def(Ids[Cursor[Side]])->Name.ToUpper()) : FText::FromString(TEXT("?")); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Body(22.f)).ColorAndOpacity(SlopUI::Text).Justification(ETextJustify::Center).AutoWrapText(true).ShadowOffset(FVector2D(1, 1))
					.Text_Lambda([this, Side]()
					{
						if (!SideShown(Side))
						{
							return Mode == EMode::Arcade ? LOCTEXT("Await", "The Slopbat's chosen await.")
								: Mode == EMode::Survival ? LOCTEXT("Waves", "Wave after wave, until you fall.")
								: LOCTEXT("Rival", "Your rival will stand for the trials.");
						}
						const FFighterDef* D = Def(Ids[Cursor[Side]]);
						FString S = D->Title + TEXT("   ·   ") + D->Style + TEXT("\n") + Json::Str(StoryOf(Ids[Cursor[Side]]), TEXT("bio"), D->Blurb);
						if (bLocked[Side] && !(bPickingDummy && Side == 0))
						{
							S += TEXT("\n") + LOCTEXT("Ready", "READY").ToString();
						}
						if (GI->Progress.IsUnlocked(TEXT("costume:") + Ids[Cursor[Side]].ToString()))
						{
							S += TEXT("\n") + FText::Format(LOCTEXT("Alt", "{0}: alternate colours ({1})"),
								SlopUI::ActionKey(GI, EGameAction::HK, Side), bCostume[Side] ? LOCTEXT("On", "on") : LOCTEXT("Off", "off")).ToString();
						}
						return FText::FromString(S);
					})
				];
		}

		virtual TSharedRef<SWidget> Build() override
		{
			TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
			for (int32 R = 0; R * Cols < Ids.Num(); ++R)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 C = 0; C < Cols && R * Cols + C < Ids.Num(); ++C)
				{
					Row->AddSlot().AutoWidth().Padding(6)[Tile(R * Cols + C)];
				}
				Grid->AddSlot().AutoHeight().HAlign(HAlign_Center)[Row];
			}
			// The figures themselves stand behind this, over the left and right panels
			// (ASlopShowroom); their names and stories go beneath them.
			TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 30, 1920, 110))
				[
					SNew(SBox).HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(SlopUI::Display(72.f)).ColorAndOpacity(SlopUI::Text).ShadowOffset(FVector2D(2, 2))
						.Text_Lambda([this]() { return bPickingDummy ? LOCTEXT("ChooseDummy", "CHOOSE YOUR OPPONENT") : LOCTEXT("Choose", "CHOOSE YOUR FIGHTER"); })
					]
				]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(640, 290, 640, 760))[SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Top)[Grid]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(30, 800, 560, 220))[Info(0)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(1330, 800, 560, 220))[Info(1)];
			return Frame(Canvas, [this]()
			{
				const EGlyphs G = GI->Router->Glyphs();
				return FText::Format(LOCTEXT("SelHint", "Directions to choose   ·   {0} or a punch to pick   ·   {2} for a random fighter   ·   {1} to go back"), SlopUI::Confirm(G), SlopUI::Back(G), SlopUI::NextName(G));
			}, false);
		}

		// The figures follow the cursors.
		virtual void Tick(float Dt) override
		{
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				Room->Show(0, Ids[Cursor[0]], bCostume[0]);
				Room->Show(1, SideShown(1) ? Ids[Cursor[1]] : NAME_None, bCostume[1]);
			}
			if (Then && (FinishIn -= Dt) <= 0.f)
			{
				TFunction<void()> Go = MoveTemp(Then);
				Then = nullptr;
				Go();
			}
		}

		virtual void Leave() override { GI->CloseShowroom(); }

		// The win pose plays for a moment before the next screen.
		void Finish(TFunction<void()> Go)
		{
			Then = MoveTemp(Go);
			FinishIn = 0.9f;
		}
		TFunction<void()> Then;
		float FinishIn = 0.f;

		void Move(int32 Side, int32 DX, int32 DY)
		{
			int32& C = Cursor[Side];
			const int32 Rows = (Ids.Num() + Cols - 1) / Cols;
			int32 X = C % Cols, Y = C / Cols;
			X = (X + DX + Cols) % Cols;
			Y = (Y + DY + Rows) % Rows;
			C = FMath::Min(Y * Cols + X, Ids.Num() - 1);
			Sound(TEXT("ui_move"));
		}

		void Confirm(int32 Side)
		{
			if (bPickingDummy)
			{
				GI->Flow.Config.P2 = Ids[Cursor[1]];
				GI->Flow.Config.bCostume[1] = bCostume[1];
				bLocked[1] = true;
				GI->Audio->Play(TEXT("select_fighter"), -2.f, 1.f, ESlopBus::Ui);
				if (ASlopShowroom* Room = GI->OpenShowroom())
				{
					Room->Celebrate(1);
				}
				Finish([this]() { GI->Goto(SlopScreens::Stage()); });
				return;
			}
			bLocked[Side] = true;
			GI->Audio->Play(TEXT("select_fighter"), -2.f, 1.f, ESlopBus::Ui);
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				Room->Celebrate(Side);
			}
			FGameFlow& F = GI->Flow;
			const FName P1 = Ids[Cursor[0]];
			if (TwoSides())
			{
				if (!(bLocked[0] && bLocked[1]))
				{
					return;
				}
				F.EndArcade();
				F.EndSurvival();
				F.Config = FMatchConfig();
				F.Config.Mode = EMode::Versus;
				F.Config.P1 = P1;
				F.Config.P2 = Ids[Cursor[1]];
				F.Config.bCostume[0] = bCostume[0];
				F.Config.bCostume[1] = bCostume[1];
				Finish([this]() { GI->Goto(SlopScreens::Stage()); });
				return;
			}
			switch (Mode)
			{
			case EMode::Arcade:
				F.Config.bCostume[0] = bCostume[0];
				Finish([this, P1]() { GI->Goto(SlopScreens::Tower(P1)); });
				break;
			case EMode::Survival:
				F.StartSurvival(P1, (int32)(FDateTime::Now().GetTicks() % 100000));
				F.Config.bCostume[0] = bCostume[0];
				Finish([this]() { GI->Goto(SlopScreens::FaceOff()); });
				break;
			case EMode::Trials:
				F.StartTrials(P1);
				F.Config.bCostume[0] = bCostume[0];
				Finish([this]() { GI->StartMatch(); });
				break;
			case EMode::Training:
			default:
				F.EndArcade();
				F.EndSurvival();
				F.Config = FMatchConfig();
				F.Config.Mode = EMode::Training;
				F.Config.P1 = P1;
				F.Config.bCostume[0] = bCostume[0];
				F.Config.RoundsToWin = 1;
				bPickingDummy = true;
				Cursor[1] = Cursor[0];
				break;
			}
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			if (Then)
			{
				return;
			}
			const int32 Side = bPickingDummy ? 1 : (TwoSides() ? FMath::Clamp(E.Side, 0, 1) : 0);
			if (TwoSides() && E.Side < 0)
			{
				return;
			}
			if (bLocked[Side] && !bPickingDummy && E.Action != EUi::Back)
			{
				return;
			}
			switch (E.Action)
			{
			case EUi::Left: Move(Side, -1, 0); break;
			case EUi::Right: Move(Side, 1, 0); break;
			case EUi::Up: Move(Side, 0, -1); break;
			case EUi::Down: Move(Side, 0, 1); break;
			case EUi::Confirm: Confirm(Side); break;
			case EUi::Next:
				// A random fighter (never the one already under the cursor), to pick or move on from.
				if (Ids.Num() > 1)
				{
					Cursor[Side] = (Cursor[Side] + FMath::RandRange(1, Ids.Num() - 1)) % Ids.Num();
					bCostume[Side] = false;
					Sound(TEXT("ui_move"));
				}
				break;
			case EUi::Alt:
				if (GI->Progress.IsUnlocked(TEXT("costume:") + Ids[Cursor[Side]].ToString()))
				{
					bCostume[Side] = !bCostume[Side];
					Sound(TEXT("ui_move"));
				}
				break;
			case EUi::Back:
				Sound(TEXT("ui_back"));
				if (bPickingDummy)
				{
					bPickingDummy = false;
					bLocked[0] = false;
				}
				else if (bLocked[Side])
				{
					bLocked[Side] = false;
				}
				else
				{
					GI->Goto(TwoSides() ? SlopScreens::Devices() : SlopScreens::MainMenu());
				}
				break;
			default: break;
			}
		}
	};

	// --- Stage select ----------------------------------------------------------------------------------

	class FStage : public FSlopScreen
	{
	public:
		TArray<FName> Ids;
		int32 Sel = 0;
		virtual void Enter() override
		{
			Ids = FGameFlow::Arenas();
			if (GI->Progress.IsUnlocked(TEXT("arena:roost")))
			{
				Ids.Add(FGameFlow::BossArena());
			}
			Sel = FMath::Max(0, Ids.IndexOfByKey(GI->Flow.Config.Stage));
		}
		virtual TSharedRef<SWidget> Build() override
		{
			if (Ids.Num() == 0)
			{
				Enter();
			}
			TArray<FText> Labels;
			for (const FName& Id : Ids)
			{
				Labels.Add(StageName(Id));
			}
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("ChooseStage", "CHOOSE THE GROUND"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 300, 560, 400))[SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, 560.f, 36.f)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(820, 300, 940, 260))
				[
					SlopUI::PanelBox(SNew(STextBlock).Font(SlopUI::Body(30.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Text_Lambda([this]() { return StageBlurb(Ids[Sel]); }))
				],
				[this]() { return FText::Format(LOCTEXT("StageHint", "{0} to choose   ·   {1} to go back"), SlopUI::Confirm(GI->Router->Glyphs()), SlopUI::Back(GI->Router->Glyphs())); });
		}
		void Choose()
		{
			Sound(TEXT("ui_select"));
			GI->Flow.Config.Stage = Ids[Sel];
			if (GI->Flow.Config.Mode == EMode::Training)
			{
				GI->StartMatch();
			}
			else
			{
				GI->Goto(SlopScreens::FaceOff());
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, Ids.Num(), -1); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, Ids.Num(), 1); Sound(TEXT("ui_move")); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Goto(SlopScreens::Select(GI->Flow.Config.Mode)); break;
			default: break;
			}
		}
	};

	// --- Arcade tower ----------------------------------------------------------------------------------

	class FTower : public FSlopScreen
	{
	public:
		FName Fighter;
		int32 Sel = 1;
		TArray<FRung> Plan[3];
		explicit FTower(FName F) : Fighter(F) {}
		virtual void Enter() override
		{
			const int32 Seed = (int32)(FDateTime::Now().GetTicks() % 100000);
			for (int32 T = 0; T < 3; ++T)
			{
				Plan[T] = GI->Flow.PlanArcade(Fighter, T, Seed + T);
			}
			Sel = FMath::Clamp(GI->Settings.Difficulty, 0, 2);
		}
		FText RungText(const FRung& R) const
		{
			if (R.Kind == TEXT("boss")) return LOCTEXT("Boss", "The Slopbat");
			if (R.Kind == TEXT("mirror")) return LOCTEXT("Mirror", "The Reflection");
			const FText N = FText::FromString(Def(R.Opponent)->Name);
			return R.Kind == TEXT("rival") ? FText::Format(LOCTEXT("Rival", "{0} (rival)"), N) : N;
		}
		virtual TSharedRef<SWidget> Build() override
		{
			if (Plan[0].Num() == 0)
			{
				Enter();
			}
			TArray<FText> Labels;
			for (int32 T = 0; T < 3; ++T)
			{
				Labels.Add(FText::Format(LOCTEXT("TowerN", "{0} Tower"), FGameFlow::DifficultyName(T)));
			}
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("ChooseTower", "CHOOSE YOUR TOWER"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 300, 520, 300))[SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, 520.f, 36.f)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(780, 260, 980, 640))
				[
					SlopUI::PanelBox(SNew(STextBlock).Font(SlopUI::Body(30.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Text_Lambda([this]()
					{
						FString S;
						for (int32 I = 0; I < Plan[Sel].Num(); ++I)
						{
							S += FString::Printf(TEXT("%d.  %s\n"), Plan[Sel].Num() - I, *RungText(Plan[Sel][Plan[Sel].Num() - 1 - I]).ToString());
						}
						const int32 Best = GI->Progress.BestScore(Fighter, Sel);
						if (Best > 0)
						{
							S += TEXT("\n") + FText::Format(LOCTEXT("BestScore", "Best score {0}"), FText::AsNumber(Best)).ToString();
						}
						return FText::FromString(S);
					}))
				],
				[this]() { return FText::Format(LOCTEXT("TowerHint", "{0} to climb   ·   {1} to go back"), SlopUI::Confirm(GI->Router->Glyphs()), SlopUI::Back(GI->Router->Glyphs())); });
		}
		void Choose()
		{
			Sound(TEXT("ui_select"));
			const bool bCos = GI->Flow.Config.bCostume[0];
			GI->Flow.SetNow(GI->Now());
			GI->Flow.StartArcade(Fighter, Sel, Plan[Sel]);
			GI->Flow.Config.bCostume[0] = bCos;
			GI->Goto(SlopScreens::FaceOff());
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, 3, -1); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, 3, 1); Sound(TEXT("ui_move")); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Goto(SlopScreens::Select(EMode::Arcade)); break;
			default: break;
			}
		}
	};

	// --- Face-off ------------------------------------------------------------------------------------------

	class FFaceOff : public FSlopScreen
	{
	public:
		double T = 0;
		// How long it stays: longer when there are words to read.
		double Left = 3.4;
		bool bStarted = false;
		virtual void Enter() override
		{
			GI->Audio->Play(TEXT("gong"), -6.f, 0.8f, ESlopBus::Ui);
			GI->Audio->PlayMusic(TEXT("music_select"));
			const FMatchConfig& C = GI->Flow.Config;
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				// Both fighters large, either side of the VS, in a rust haze.
				Room->SetLayout(440, 1480, 831, 602);
				Room->SetMood(FLinearColor(0.07f, 0.03f, 0.02f));
				Room->SetReflection(1, C.bMirrorDark);
				Room->Show(0, C.P1, C.bCostume[0]);
				Room->Show(1, C.P2, C.bCostume[1] || (C.P1 == C.P2 && !C.bMirrorDark));
			}
		}
		virtual void Leave() override { GI->CloseShowroom(); }
		virtual TSharedRef<SWidget> Build() override
		{
			const FMatchConfig& C = GI->Flow.Config;
			const FFighterDef* A = Def(C.P1);
			const FFighterDef* B = Def(C.P2);
			FText Above, AboveSmall;
			FLinearColor AboveColour = SlopUI::Dim;
			float AboveSize = 24.f;
			TArray<FText> Lines;
			if (const FRung* R = GI->Flow.CurrentRung())
			{
				Above = FText::Format(LOCTEXT("Fight", "Fight {0} of {1}"), GI->Flow.RungIndex() + 1, GI->Flow.Rungs().Num());
				if (R->Kind == TEXT("rival"))
				{
					if (const TArray<TSharedPtr<FJsonValue>>* L = Json::Arr(StoryOf(C.P1), TEXT("rival_lines")))
					{
						if (L->Num() >= 2)
						{
							Lines.Add(FText::FromString(A->Name + TEXT(": “") + (*L)[0]->AsString() + TEXT("”")));
							Lines.Add(FText::FromString(B->Name + TEXT(": “") + (*L)[1]->AsString() + TEXT("”")));
						}
					}
				}
				else if (R->Kind == TEXT("mirror"))
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("reflection"))));
				}
				else if (R->Kind == TEXT("boss"))
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("boss_intro"))));
					GI->Audio->Play(TEXT("screech"), -6.f);
				}
				if (Lines.Num() > 0)
				{
					Left = 5.2;
				}
			}
			else if (GI->Flow.InSurvival())
			{
				const bool bBoss = C.P2 == TEXT("slopbat");
				Above = FText::Format(LOCTEXT("Wave", "WAVE {0}"), GI->Flow.SurvivalWaves() + 1);
				AboveColour = bBoss ? SlopUI::Rust * 1.4f : SlopUI::Amber;
				AboveSize = 54.f;
				AboveSmall = FGameFlow::DifficultyName(C.Difficulty);
				if (bBoss)
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("boss_intro"))));
					GI->Audio->Play(TEXT("screech"), -6.f);
					Left = 4.6;
				}
			}
			const FText BName = C.bMirrorDark ? LOCTEXT("Reflection", "THE REFLECTION") : FText::FromString(B->Name.ToUpper());
			TSharedRef<SVerticalBox> Said = SNew(SVerticalBox);
			for (const FText& L : Lines)
			{
				Said->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
				[
					SNew(STextBlock).Font(SlopUI::Body(28.f)).ColorAndOpacity(SlopUI::Text).Text(L).AutoWrapText(true).Justification(ETextJustify::Center).ShadowOffset(FVector2D(1, 1))
				];
			}
			TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, AboveSize > 30.f ? 50 : 70, 1920, 80))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(Above, AboveSize, AboveColour)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 125, 1920, 40))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(AboveSmall, 24.f, SlopUI::Dim)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(760, 310, 400, 260))[SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SlopUI::Title(LOCTEXT("VS", "VS"), 200.f, SlopUI::Rust * 1.3f)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(560, 580, 800, 50))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(StageName(C.Stage), 30.f, SlopUI::Dim)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(400, 640, 1120, 260))[Said]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(120, 930, 640, 80))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(FText::FromString(A->Name.ToUpper()), 60.f, SlopUI::Accent)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(1160, 930, 640, 80))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(BName, 60.f, SlopUI::Amber)]];
			return Frame(Canvas, [this]() { return FText::Format(LOCTEXT("FaceHint", "{0} to begin"), SlopUI::Confirm(GI->Router->Glyphs())); }, false);
		}
		void Begin()
		{
			if (!bStarted)
			{
				bStarted = true;
				GI->StartMatch();
			}
		}
		virtual void Tick(float Dt) override
		{
			T += Dt;
			if (T >= Left)
			{
				Begin();
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Confirm && T > 0.4)
			{
				Begin();
			}
		}
	};

	// --- Controllers for Versus ---------------------------------------------------------------------------

	class FDevices : public FSlopScreen
	{
	public:
		int32 Sel = 0;
		TArray<int32> Rows;
		virtual void Enter() override
		{
			GI->Router->SetSolo(false);
			Refresh();
		}
		void Refresh()
		{
			Rows = {0, 1};
			for (int32 D = (int32)EDevice::FirstPad; D < NumDevices; ++D)
			{
				if (GI->Router->IsPadConnected(D))
				{
					Rows.Add(D);
				}
			}
			Sel = FMath::Clamp(Sel, 0, Rows.Num());
		}
		FText RowName(int32 D) const
		{
			if (D == 0) return LOCTEXT("KbL", "Keyboard, left half (W A S D, U I O, J K L)");
			if (D == 1) return LOCTEXT("KbR", "Keyboard, arrows and number pad");
			return FText::Format(LOCTEXT("Pad", "Controller {0}: {1}"), D - 1, FText::FromString(GI->Router->PadName(D)));
		}
		bool Valid() const
		{
			bool bSide[2] = {false, false};
			for (int32 D : Rows)
			{
				const int32 S = GI->Settings.Assign[D];
				if (S >= 0)
				{
					bSide[S] = true;
				}
			}
			return bSide[0] && bSide[1];
		}
		virtual TSharedRef<SWidget> Build() override
		{
			Refresh();
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			for (int32 I = 0; I < Rows.Num(); ++I)
			{
				const int32 D = Rows[I];
				List->AddSlot().AutoHeight().Padding(0, 6)
				[
					SNew(SBorder).BorderImage(SlopUI::Rounded()).Padding(FMargin(20, 10))
					.BorderBackgroundColor_Lambda([this, I]() { return Sel == I ? FLinearColor(0.04f, 0.07f, 0.02f, 0.95f) : FLinearColor(0.01f, 0.016f, 0.013f, 0.8f); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[SlopUI::Label(RowName(D), 30.f, SlopUI::Text)]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(300).HAlign(HAlign_Right)
							[
								SNew(STextBlock).Font(SlopUI::Display(34.f)).Text_Lambda([this, D]()
								{
									const int32 S = GI->Settings.Assign[D];
									return S == 0 ? LOCTEXT("P1", "◀  PLAYER 1  ▶") : S == 1 ? LOCTEXT("P2", "◀  PLAYER 2  ▶") : LOCTEXT("Nobody", "◀  nobody  ▶");
								})
								.ColorAndOpacity_Lambda([this, D]() { const int32 S = GI->Settings.Assign[D]; return S == 0 ? SlopUI::Accent : S == 1 ? SlopUI::Amber : SlopUI::Dim; })
							]
						]
					]
				];
			}
			List->AddSlot().AutoHeight().Padding(0, 24)
			[
				SNew(SBorder).BorderImage(SlopUI::Rounded()).Padding(FMargin(20, 10))
				.BorderBackgroundColor_Lambda([this]() { return Sel == Rows.Num() ? FLinearColor(0.04f, 0.07f, 0.02f, 0.95f) : FLinearColor(0.01f, 0.016f, 0.013f, 0.8f); })
				[
					SNew(STextBlock).Font(SlopUI::Body(32.f)).Text(LOCTEXT("ToSelect", "Continue to fighter select"))
					.ColorAndOpacity_Lambda([this]() { return Valid() ? SlopUI::Text : SlopUI::Dim * 0.6f; })
				]
			];
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("Controllers", "WHO PLAYS WITH WHAT"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(260, 240, 1400, 620))[List]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(260, 860, 1400, 50))
				[
					SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Amber)
					.Text_Lambda([this]() { return Valid() ? FText::GetEmpty() : LOCTEXT("Need", "Each player needs at least one device. Connect a controller to have one each."); })
				],
				[this]() { return FText::Format(LOCTEXT("DevHint", "Up and down to choose a device   ·   left and right to give it to a player   ·   {0} to go back"), SlopUI::Back(GI->Router->Glyphs())); });
		}
		virtual void Tick(float Dt) override
		{
			const int32 Before = Rows.Num();
			Refresh();
			if (Rows.Num() != Before)
			{
				GI->RebuildScreen();
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, Rows.Num() + 1, -1); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, Rows.Num() + 1, 1); Sound(TEXT("ui_move")); break;
			case EUi::Left:
			case EUi::Right:
				if (Sel < Rows.Num())
				{
					int32& S = GI->Settings.Assign[Rows[Sel]];
					// Cycle: player 1, player 2, nobody.
					const int32 Order[] = {0, 1, -1};
					int32 I = S == 0 ? 0 : (S == 1 ? 1 : 2);
					I = (I + (E.Action == EUi::Right ? 1 : 2)) % 3;
					S = Order[I];
					GI->SaveSettings();
					Sound(TEXT("ui_move"));
				}
				break;
			case EUi::Confirm:
				if (Sel == Rows.Num())
				{
					if (Valid())
					{
						Sound(TEXT("ui_select"));
						GI->Flow.Config.Mode = EMode::Versus;
						GI->Goto(SlopScreens::Select(EMode::Versus));
					}
					else
					{
						Sound(TEXT("ui_locked"));
					}
				}
				break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Router->SetSolo(true); GI->Goto(SlopScreens::MainMenu()); break;
			default: break;
			}
		}
	};

	// --- Move list --------------------------------------------------------------------------------------------

	class FMoveList : public FSlopScreen
	{
	public:
		FName Fighter;
		bool bOverlay;
		int32 Scroll = 0;
		TArray<FString> Rows;
		FMoveList(FName F, bool bO) : Fighter(F), bOverlay(bO) {}
		virtual bool ShowsMatch() const override { return bOverlay; }

		void Collect()
		{
			const FFighterDef* D = Def(Fighter);
			Rows.Reset();
			auto Frames = [](const FMoveDef& M)
			{
				const int32 Hit = M.HitAdvantage();
				return FString::Printf(TEXT("%s   %d dmg   startup %d  active %d  recovery %d   on block %+d   on hit %s"),
					LevelName(M.Level), M.Damage + (M.Throw.bHas ? M.Throw.Damage : 0) + (M.Projectile.bHas ? M.Projectile.Damage : 0), M.Startup, M.Active, M.Recovery,
					M.BlockAdvantage(), Hit >= 999 ? TEXT("knockdown") : *FString::Printf(TEXT("%+d"), Hit));
			};
			auto Section = [this](const FText& T) { Rows.Add(TEXT("#") + T.ToString()); };
			Section(LOCTEXT("Specials", "SPECIALS"));
			for (const FCommand& C : D->Commands)
			{
				const FMoveDef* M = D->Move(C.Move);
				if (M && M->Kind == EMoveKind::Special)
				{
					Rows.Add(FString::Printf(TEXT("%s|%s|%s"), *M->Name, *FCommand::Pretty(C.Text), *Frames(*M)));
				}
			}
			Section(LOCTEXT("Combos", "DIAL-A-COMBOS (each link spends run meter)"));
			// Chains from each opener, written as the full string of presses.
			TFunction<void(const FMoveDef&, const FString&, const FString&, int32)> Walk = [&](const FMoveDef& M, const FString& Inputs, const FString& Names, int32 Depth)
			{
				for (const FChainLink& L : M.Chain)
				{
					const FMoveDef* Next = D->Move(L.Move);
					if (!Next || Depth > 5)
					{
						continue;
					}
					const FString In = Inputs + TEXT(", ") + FCommand::Pretty(L.Input);
					const FString Nm = Names + TEXT(" → ") + Next->Name;
					Rows.Add(FString::Printf(TEXT("%s|%s|"), *Nm, *In));
					Walk(*Next, In, Nm, Depth + 1);
				}
			};
			TSet<FName> Openers;
			for (const FCommand& C : D->Commands)
			{
				const FMoveDef* M = D->Move(C.Move);
				if (M && M->Chain.Num() > 0 && M->Kind != EMoveKind::Chain && !Openers.Contains(C.Move))
				{
					Openers.Add(C.Move);
					Walk(*M, FCommand::Pretty(C.Text), M->Name, 0);
				}
			}
			if (D->bHasFinisher)
			{
				Section(LOCTEXT("Finisher", "SLOPTALITY (after the deciding knockout)"));
				const FMoveDef* M = D->Move(D->FinisherCmd.Move);
				Rows.Add(FString::Printf(TEXT("%s|%s|%s range"), M ? *M->Name : TEXT("?"), *FCommand::Pretty(D->FinisherCmd.Text), *D->FinisherRange.ToString()));
			}
			Section(LOCTEXT("Basics", "BASICS"));
			for (const FCommand& C : D->Commands)
			{
				const FMoveDef* M = D->Move(C.Move);
				if (M && (M->Kind == EMoveKind::Normal || M->IsThrowKind()))
				{
					Rows.Add(FString::Printf(TEXT("%s|%s|%s"), *M->Name, *FCommand::Pretty(C.Text), *Frames(*M)));
				}
			}
			for (const FCommand& C : D->AirCommands)
			{
				const FMoveDef* M = D->Move(C.Move);
				if (M)
				{
					Rows.Add(FString::Printf(TEXT("%s|Air %s|%s"), *M->Name, *FCommand::Pretty(C.Text), *Frames(*M)));
				}
			}
			Section(LOCTEXT("Rules", "RULES"));
			Rows.Add(LOCTEXT("R1", "Block is a button: standing block stops high, mid and overhead attacks; crouching block stops lows and mids.||").ToString());
			Rows.Add(LOCTEXT("R2", "Throw breaks: press a punch just after being grabbed.||").ToString());
			Rows.Add(LOCTEXT("R3", "Damage scales from the third hit of a combo; juggles end at 7 juggle points.||").ToString());
			Rows.Add(LOCTEXT("R4", "Only specials and projectiles chip a blocker; one projectile per fighter at a time.||").ToString());
			Rows.Add(LOCTEXT("R5", "Counter hit: a blow landing during the other fighter's wind-up does 15% more and stuns longer.||").ToString());
		}

		virtual TSharedRef<SWidget> Build() override
		{
			if (Rows.Num() == 0)
			{
				Collect();
			}
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			const int32 Visible = 17;
			for (int32 I = Scroll; I < FMath::Min(Rows.Num(), Scroll + Visible); ++I)
			{
				const FString& R = Rows[I];
				if (R.StartsWith(TEXT("#")))
				{
					List->AddSlot().AutoHeight().Padding(0, 14, 0, 4)[SlopUI::Label(FText::FromString(R.Mid(1)), 30.f, SlopUI::Accent)];
					continue;
				}
				TArray<FString> P;
				R.ParseIntoArray(P, TEXT("|"), false);
				P.SetNum(3);
				List->AddSlot().AutoHeight().Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.42f)[SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Text).Text(FText::FromString(P[0])).AutoWrapText(true)]
					+ SHorizontalBox::Slot().FillWidth(0.22f)[SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Amber).Text(FText::FromString(P[1]))]
					+ SHorizontalBox::Slot().FillWidth(0.36f)[SNew(STextBlock).Font(SlopUI::Body(20.f)).ColorAndOpacity(SlopUI::Dim).Text(FText::FromString(P[2])).AutoWrapText(true)]
				];
			}
			TSharedRef<SOverlay> O = SNew(SOverlay);
			O->AddSlot()[bOverlay ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))) : SlopUI::Backdrop()];
			O->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)
			[
				SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(120, 40, 1680, 90))[SlopUI::Title(FText::Format(LOCTEXT("MovesOf", "{0}: MOVES"), FText::FromString(Def(Fighter)->Name.ToUpper())), 60.f, SlopUI::Text)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(120, 150, 1680, 820))[List]
			]]];
			O->AddSlot()[SlopUI::Hint([this]() { return FText::Format(LOCTEXT("MovesHint", "Up and down to scroll   ·   left and right for another fighter   ·   {0} to go back"), SlopUI::Back(GI->Router->Glyphs())); })];
			return O;
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			const TArray<FName>& R = FContent::Roster();
			switch (E.Action)
			{
			case EUi::Up: Scroll = FMath::Max(0, Scroll - 1); GI->RebuildScreen(); break;
			case EUi::Down: Scroll = FMath::Min(FMath::Max(0, Rows.Num() - 17), Scroll + 1); GI->RebuildScreen(); break;
			case EUi::Left:
			case EUi::Right:
				if (!bOverlay)
				{
					const int32 I = FMath::Max(0, R.IndexOfByKey(Fighter));
					Fighter = R[(I + (E.Action == EUi::Right ? 1 : R.Num() - 1)) % R.Num()];
					Rows.Reset();
					Scroll = 0;
					Sound(TEXT("ui_move"));
					GI->RebuildScreen();
				}
				break;
			case EUi::Back:
			case EUi::Pause:
			case EUi::Confirm:
				Sound(TEXT("ui_back"));
				if (bOverlay)
				{
					GI->ShowOverlay(SlopScreens::Pause());
				}
				else
				{
					GI->Goto(SlopScreens::Extras());
				}
				break;
			default: break;
			}
		}
	};

	// --- Extras, endings, credits ---------------------------------------------------------------------------

	class FExtras : public FSlopScreen
	{
	public:
		int32 Sel = 0;
		int32 Page = 0;
		TArray<FName> Seen;
		virtual void Enter() override
		{
			Seen.Reset();
			for (const FName& Id : FContent::Roster())
			{
				if (GI->Progress.EndingSeen(Id))
				{
					Seen.Add(Id);
				}
			}
		}
		bool StrangerMet() const { return GI->Progress.Stranger() > 0; }
		TArray<FText> Labels() const
		{
			return {LOCTEXT("Endings", "Endings"), LOCTEXT("Records", "Records"), LOCTEXT("MoveLists", "Move Lists"),
				StrangerMet() ? LOCTEXT("Stranger", "The Stranger in White") : LOCTEXT("Unknown", "???"), LOCTEXT("Credits", "Credits"), LOCTEXT("Back", "Back")};
		}
		FText Records() const
		{
			FString S;
			for (const FName& Id : FContent::Roster())
			{
				const FFighterDef* D = Def(Id);
				FString Line = D->Name + TEXT(":  ");
				for (int32 T = 0; T < 3; ++T)
				{
					const int32 Best = GI->Progress.BestScore(Id, T);
					const double Secs = GI->Progress.BestTime(Id, T);
					Line += FString::Printf(TEXT("%s %s   "), *FGameFlow::DifficultyName(T).ToString(),
						Best > 0 ? *FString::Printf(TEXT("%s (%d:%02d)"), *FText::AsNumber(Best).ToString(), (int32)Secs / 60, (int32)Secs % 60) : TEXT("—"));
				}
				Line += FString::Printf(TEXT("   survival %d   trials %d"), GI->Progress.BestSurvival(Id), GI->Progress.TrialsDone(Id).Num());
				S += Line + TEXT("\n");
			}
			S += FString::Printf(TEXT("\nMatches %d   ·   wins %d   ·   Sloptalities %d   ·   flawless victories %d"),
				GI->Progress.Stat(TEXT("matches")), GI->Progress.Stat(TEXT("wins")), GI->Progress.Stat(TEXT("finishers")), GI->Progress.Stat(TEXT("flawless")));
			return FText::FromString(S);
		}
		virtual TSharedRef<SWidget> Build() override
		{
			if (Seen.Num() == 0)
			{
				Enter();
			}
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(120, 60, 1200, 110))[SlopUI::Title(LOCTEXT("ExtrasT", "EXTRAS"), 84.f, SlopUI::Accent)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(140, 260, 460, 500))[SlopUI::Menu(Labels(), [this]() { return Sel; }, [this](int32 I) { return I != 3 || StrangerMet(); }, [this](int32 I) { Sel = I; Choose(); }, 460.f, 34.f)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(700, 260, 1100, 640))
				[
					SlopUI::PanelBox(SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Text_Lambda([this]()
					{
						switch (Sel)
						{
						case 0:
						{
							if (Seen.Num() == 0) return LOCTEXT("NoEndings", "Clear an arcade tower to see a fighter's ending.");
							FString S = LOCTEXT("EndingsSeen", "Endings seen (left and right to choose, confirm to watch):\n\n").ToString();
							for (int32 I = 0; I < Seen.Num(); ++I)
							{
								S += (I == Page ? TEXT("▶ ") : TEXT("   ")) + Def(Seen[I])->Name + TEXT("\n");
							}
							return FText::FromString(S);
						}
						case 1: return Records();
						case 2: return LOCTEXT("MovesD", "Every fighter's moves, combos and frame data.");
						case 3: return StrangerMet() ? LOCTEXT("StrangerD", "Seek out the stranger again, on the stilt market's far walkway.") : LOCTEXT("UnknownD", "Something waits for those who fight without a scratch.");
						case 4: return LOCTEXT("CreditsD", "Who made this, and with what.");
						default: return FText::GetEmpty();
						}
					}))
				],
				[this]() { return FText::Format(LOCTEXT("ExtrasHint", "{0} to choose   ·   {1} to go back"), SlopUI::Confirm(GI->Router->Glyphs()), SlopUI::Back(GI->Router->Glyphs())); });
		}
		void Choose()
		{
			Sound(TEXT("ui_select"));
			switch (Sel)
			{
			case 0: if (Seen.IsValidIndex(Page)) GI->Goto(SlopScreens::Ending(Seen[Page], false)); break;
			case 2: GI->Goto(SlopScreens::MoveList(FContent::Roster()[0], false)); break;
			case 3:
				if (StrangerMet())
				{
					GI->Flow.StartStranger(false);
					GI->StartMatch();
				}
				break;
			case 4: GI->Goto(SlopScreens::Credits()); break;
			case 5: GI->Goto(SlopScreens::MainMenu()); break;
			default: break;
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, 6, -1, [this](int32 I) { return I != 3 || StrangerMet(); }); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, 6, 1, [this](int32 I) { return I != 3 || StrangerMet(); }); Sound(TEXT("ui_move")); break;
			case EUi::Left: if (Sel == 0 && Seen.Num()) Page = (Page + Seen.Num() - 1) % Seen.Num(); break;
			case EUi::Right: if (Sel == 0 && Seen.Num()) Page = (Page + 1) % Seen.Num(); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Goto(SlopScreens::MainMenu()); break;
			default: break;
			}
		}
	};

	class FEnding : public FSlopScreen
	{
	public:
		FName Fighter;
		bool bFromArcade;
		int32 Page = 0;
		TArray<FString> Pages;
		FEnding(FName F, bool b) : Fighter(F), bFromArcade(b) {}
		virtual void Enter() override
		{
			GI->Audio->PlayMusic(TEXT("music_ending"));
			if (const TArray<TSharedPtr<FJsonValue>>* E = Json::Arr(StoryOf(Fighter), TEXT("ending")))
			{
				for (const TSharedPtr<FJsonValue>& V : *E)
				{
					Pages.Add(V->AsString());
				}
			}
			if (Pages.Num() == 0)
			{
				Pages.Add(Def(Fighter)->Name);
			}
		}
		virtual TSharedRef<SWidget> Build() override
		{
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 120, 1920, 120))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(FText::FromString(Def(Fighter)->Name.ToUpper()), 84.f, SlopUI::Amber)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(300, 380, 1320, 400))
				[
					SNew(STextBlock).Font(SlopUI::Body(38.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Justification(ETextJustify::Center)
					.Text_Lambda([this]() { return Pages.IsValidIndex(Page) ? FText::FromString(Pages[Page]) : FText::GetEmpty(); })
				],
				[this]() { return FText::Format(LOCTEXT("EndHint", "{0} to go on"), SlopUI::Confirm(GI->Router->Glyphs())); });
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Confirm)
			{
				if (++Page >= Pages.Num())
				{
					if (bFromArcade)
					{
						GI->Flow.EndArcade();
						GI->Goto(SlopScreens::Credits());
					}
					else
					{
						GI->Goto(SlopScreens::Extras());
					}
				}
			}
			else if (E.Action == EUi::Back && !bFromArcade)
			{
				GI->Goto(SlopScreens::Extras());
			}
		}
	};

	class FCredits : public FSlopScreen
	{
	public:
		virtual TSharedRef<SWidget> Build() override
		{
			const FText Body = LOCTEXT("CreditsBody",
				"A game by Markus Gillyard\n\n"
				"Design, characters and story: Markus Gillyard. The stranger in white comes from Markus Gillyard's Wyseforge mythology.\n"
				"Programming, animation, stages and synthesised sound: made for this game.\n\n"
				"Bodies: MakeHuman (makehumancommunity.org) base mesh, skins, eyes and hair, CC0, made with MPFB for Blender.\n"
				"Surfaces: ambientCG (ambientcg.com) and Poly Haven (polyhaven.com), CC0.\n"
				"Props and skies: Poly Haven, CC0.   Sound effects: Kenney (kenney.nl), CC0.\n"
				"Motion capture: Open Motion Project by ACCAD / The Ohio State University, CC BY 3.0 (creativecommons.org/licenses/by/3.0); retargeted and retimed for this game.\n"
				"Typefaces: New Rocker by Pablo Impallari, Brenda Gallo and Rodrigo Fuenzalida; Oswald by The Oswald Project Authors; SIL Open Font License 1.1. "
				"Symbols: Droid Sans Fallback, The Android Open Source Project, Apache License 2.0.\n\n"
				"Mortal Slopbat uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.\n"
				"Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.\n"
				"Full licence texts ship with the game in the Licenses folder.");
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(0, 90, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("CreditsT", "CREDITS"), 84.f, SlopUI::Accent)]]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(240, 260, 1440, 700))
				[
					SNew(STextBlock).Font(SlopUI::Body(28.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Justification(ETextJustify::Center).Text(Body)
				],
				[this]() { return FText::Format(LOCTEXT("CreditsHint", "{0} to go back"), SlopUI::Back(GI->Router->Glyphs())); });
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Back || E.Action == EUi::Confirm)
			{
				Sound(TEXT("ui_back"));
				GI->Goto(SlopScreens::MainMenu());
			}
		}
	};
}

using namespace SlopScreensFlowImpl;

namespace SlopScreens
{
	TSharedPtr<FSlopScreen> Select(EMode Mode) { return Named(MakeShared<FSelect>(Mode == EMode::Cpu ? EMode::Training : Mode), TEXT("Select")); }
	TSharedPtr<FSlopScreen> Stage() { return Named(MakeShared<FStage>(), TEXT("Stage")); }
	TSharedPtr<FSlopScreen> Tower(FName Fighter) { return Named(MakeShared<FTower>(Fighter), TEXT("Tower")); }
	TSharedPtr<FSlopScreen> FaceOff() { return Named(MakeShared<FFaceOff>(), TEXT("FaceOff")); }
	TSharedPtr<FSlopScreen> Devices() { return Named(MakeShared<FDevices>(), TEXT("Devices")); }
	TSharedPtr<FSlopScreen> MoveList(FName Fighter, bool bOverlay) { return Named(MakeShared<FMoveList>(Fighter, bOverlay), TEXT("MoveList")); }
	TSharedPtr<FSlopScreen> Extras() { return Named(MakeShared<FExtras>(), TEXT("Extras")); }
	TSharedPtr<FSlopScreen> Ending(FName Fighter, bool bFromArcade) { return Named(MakeShared<FEnding>(Fighter, bFromArcade), TEXT("Ending")); }
	TSharedPtr<FSlopScreen> Credits() { return Named(MakeShared<FCredits>(), TEXT("Credits")); }
	TSharedPtr<FSlopScreen> TrialsPick() { return Named(MakeShared<FSelect>(EMode::Trials), TEXT("Select")); }
}

#undef LOCTEXT_NAMESPACE
