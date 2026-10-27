#include "UI/SlopScreens.h"
#include "UI/SlopStyle.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopMatch.h"
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
			return SNew(SBox).WidthOverride(300).HeightOverride(240)
			[
				SNew(SBorder).BorderImage(SlopUI::Rounded()).BorderBackgroundColor_Lambda(Border).Padding(4)
				.OnMouseButtonDown_Lambda([this, I](const FGeometry&, const FPointerEvent&) { Cursor[0] = I; Confirm(0); return FReply::Handled(); })
				[
					SNew(SBorder).BorderImage(SlopUI::Rounded()).BorderBackgroundColor(FLinearColor(0.012f, 0.02f, 0.016f, 0.96f)).Padding(FMargin(16, 14))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
						[
							SlopUI::Title(FText::FromString(D->Name.Left(1)), 120.f, FLinearColor(FColor::FromHex(Json::Str(D->Look, TEXT("glow"), TEXT("#a8db38")))))
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SlopUI::Label(FText::FromString(D->Name.ToUpper()), 26.f, SlopUI::Text)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SlopUI::Label(FText::FromString(D->Style), 20.f, SlopUI::Dim)]
					]
				]
			];
		}

		TSharedRef<SWidget> Info(int32 Side)
		{
			return SlopUI::PanelBox(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(SlopUI::Display(46.f)).ColorAndOpacity(Side == 0 ? SlopUI::Accent : SlopUI::Amber)
					.Text_Lambda([this, Side]() { return FText::FromString(Def(Ids[Cursor[Side]])->Name); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Text)
					.Text_Lambda([this, Side]() { const FFighterDef* D = Def(Ids[Cursor[Side]]); return FText::FromString(D->Title + TEXT("   ·   ") + D->Style); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Body(24.f)).ColorAndOpacity(SlopUI::Dim).AutoWrapText(true)
					.Text_Lambda([this, Side]() { return FText::FromString(Json::Str(StoryOf(Ids[Cursor[Side]]), TEXT("bio"), Def(Ids[Cursor[Side]])->Blurb)); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Body(24.f)).ColorAndOpacity(SlopUI::Amber)
					.Text_Lambda([this, Side]()
					{
						const FName Id = Ids[Cursor[Side]];
						FString S;
						if (bLocked[Side] && !bPickingDummy)
						{
							S = LOCTEXT("Ready", "READY").ToString();
						}
						if (GI->Progress.IsUnlocked(TEXT("costume:") + Id.ToString()))
						{
							S += (S.IsEmpty() ? TEXT("") : TEXT("   ·   ")) + FText::Format(LOCTEXT("Alt", "{0}: alternate colours ({1})"),
								SlopUI::ActionKey(GI, EGameAction::HK, Side), bCostume[Side] ? LOCTEXT("On", "on") : LOCTEXT("Off", "off")).ToString();
						}
						return FText::FromString(S);
					})
				]);
		}

		virtual TSharedRef<SWidget> Build() override
		{
			TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
			for (int32 R = 0; R * Cols < Ids.Num(); ++R)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 C = 0; C < Cols && R * Cols + C < Ids.Num(); ++C)
				{
					Row->AddSlot().AutoWidth().Padding(8)[Tile(R * Cols + C)];
				}
				Grid->AddSlot().AutoHeight().HAlign(HAlign_Center)[Row];
			}
			TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 30, 1920, 110))
				[
					SNew(SBox).HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(SlopUI::Display(72.f)).ColorAndOpacity(SlopUI::Text)
						.Text_Lambda([this]() { return bPickingDummy ? LOCTEXT("ChooseDummy", "CHOOSE YOUR OPPONENT") : LOCTEXT("Choose", "CHOOSE YOUR FIGHTER"); })
					]
				]
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 170, 1920, 540))[SNew(SBox).HAlign(HAlign_Center)[Grid]]
				+ SConstraintCanvas::Slot().Offset(FMargin(120, 720, TwoSides() ? 820 : 1680, 260))[Info(0)];
			if (TwoSides() || Mode == EMode::Training)
			{
				Canvas->AddSlot().Offset(FMargin(TwoSides() ? 980 : 980, 720, 820, 260))[SNew(SBox).Visibility_Lambda([this]()
				{
					return (TwoSides() || bPickingDummy) ? EVisibility::Visible : EVisibility::Collapsed;
				})[Info(1)]];
			}
			return Frame(Canvas, [this]()
			{
				const EGlyphs G = GI->Router->Glyphs();
				return FText::Format(LOCTEXT("SelHint", "Directions to choose   ·   {0} or a punch to pick   ·   {1} to go back"), SlopUI::Confirm(G), SlopUI::Back(G));
			});
		}

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
				GI->Goto(SlopScreens::Stage());
				return;
			}
			bLocked[Side] = true;
			GI->Audio->Play(TEXT("select_fighter"), -2.f, 1.f, ESlopBus::Ui);
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
				GI->Goto(SlopScreens::Stage());
				return;
			}
			switch (Mode)
			{
			case EMode::Arcade:
				F.Config.bCostume[0] = bCostume[0];
				GI->Goto(SlopScreens::Tower(P1));
				break;
			case EMode::Survival:
				F.StartSurvival(P1, (int32)(FDateTime::Now().GetTicks() % 100000));
				F.Config.bCostume[0] = bCostume[0];
				GI->Goto(SlopScreens::FaceOff());
				break;
			case EMode::Trials:
				F.StartTrials(P1);
				F.Config.bCostume[0] = bCostume[0];
				GI->StartMatch();
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
				Cursor[1] = FMath::Max(0, Ids.IndexOfByKey(FName(Json::Str(StoryOf(P1), TEXT("rival"), TEXT("gorm")))));
				break;
			}
		}

		virtual void OnUi(const FUiEvent& E) override
		{
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
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("ChooseStage", "CHOOSE THE GROUND"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(160, 300, 560, 400))[SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, 560.f, 36.f)]
				+ SConstraintCanvas::Slot().Offset(FMargin(820, 300, 940, 260))
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
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("ChooseTower", "CHOOSE YOUR TOWER"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(160, 300, 520, 300))[SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, 520.f, 36.f)]
				+ SConstraintCanvas::Slot().Offset(FMargin(780, 260, 980, 640))
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
		bool bStarted = false;
		virtual void Enter() override { GI->Audio->Play(TEXT("gong"), -8.f, 0.9f, ESlopBus::Ui); }
		virtual TSharedRef<SWidget> Build() override
		{
			const FMatchConfig& C = GI->Flow.Config;
			const FFighterDef* A = Def(C.P1);
			const FFighterDef* B = Def(C.P2);
			FText Above;
			FText Line1, Line2;
			if (const FRung* R = GI->Flow.CurrentRung())
			{
				Above = FText::Format(LOCTEXT("Fight", "{0} Tower   ·   fight {1} of {2}"), FGameFlow::DifficultyName(GI->Flow.ArcadeTower()), GI->Flow.RungIndex() + 1, GI->Flow.Rungs().Num());
				if (R->Kind == TEXT("rival"))
				{
					if (const TArray<TSharedPtr<FJsonValue>>* L = Json::Arr(StoryOf(C.P1), TEXT("rival_lines")))
					{
						if (L->Num() >= 2)
						{
							Line1 = FText::FromString(TEXT("“") + (*L)[0]->AsString() + TEXT("”"));
							Line2 = FText::FromString(TEXT("“") + (*L)[1]->AsString() + TEXT("”"));
						}
					}
				}
				else if (R->Kind == TEXT("mirror"))
				{
					Line1 = FText::FromString(Json::Str(GetContent().Story, TEXT("reflection")));
				}
				else if (R->Kind == TEXT("boss"))
				{
					Line1 = FText::FromString(Json::Str(GetContent().Story, TEXT("boss_intro")));
				}
			}
			else if (GI->Flow.InSurvival())
			{
				Above = FText::Format(LOCTEXT("Wave", "Survival   ·   wave {0}   ·   {1}"), GI->Flow.SurvivalWaves() + 1, FGameFlow::DifficultyName(C.Difficulty));
			}
			const FText BName = C.bMirrorDark ? LOCTEXT("Reflection", "THE REFLECTION") : FText::FromString(B->Name.ToUpper());
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 120, 1920, 50))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(Above, 30.f, SlopUI::Dim)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 300, 900, 160))[SNew(SBox).HAlign(HAlign_Right)[SlopUI::Title(FText::FromString(A->Name.ToUpper()), 84.f, SlopUI::Accent)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(900, 310, 120, 160))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("VS", "VS"), 96.f, SlopUI::Rust * 1.3f)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(1020, 300, 900, 160))[SNew(SBox).HAlign(HAlign_Left)[SlopUI::Title(BName, 84.f, SlopUI::Amber)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 520, 1920, 50))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(StageName(C.Stage), 34.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(200, 620, 1520, 60))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(Line1, 32.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(200, 680, 1520, 60))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Label(Line2, 32.f, SlopUI::Dim)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 860, 1920, 50))[SNew(SBox).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(SlopUI::Body(28.f)).ColorAndOpacity(SlopUI::Dim).Text_Lambda([this]() { return T > 2.4 ? LOCTEXT("Loading", "Loading") : FText::GetEmpty(); })
				]],
				[this]() { return FText::Format(LOCTEXT("FaceHint", "{0} to begin"), SlopUI::Confirm(GI->Router->Glyphs())); });
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
			if (T > 2.6)
			{
				Begin();
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Confirm && T > 0.3)
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
									return S == 0 ? LOCTEXT("P1", "◂  PLAYER 1  ▸") : S == 1 ? LOCTEXT("P2", "◂  PLAYER 2  ▸") : LOCTEXT("Nobody", "◂  nobody  ▸");
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
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 60, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("Controllers", "WHO PLAYS WITH WHAT"), 72.f, SlopUI::Text)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(260, 240, 1400, 620))[List]
				+ SConstraintCanvas::Slot().Offset(FMargin(260, 860, 1400, 50))
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
				+ SConstraintCanvas::Slot().Offset(FMargin(120, 40, 1680, 90))[SlopUI::Title(FText::Format(LOCTEXT("MovesOf", "{0}: MOVES"), FText::FromString(Def(Fighter)->Name.ToUpper())), 60.f, SlopUI::Text)]
				+ SConstraintCanvas::Slot().Offset(FMargin(120, 150, 1680, 820))[List]
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
				+ SConstraintCanvas::Slot().Offset(FMargin(120, 60, 1200, 110))[SlopUI::Title(LOCTEXT("ExtrasT", "EXTRAS"), 84.f, SlopUI::Accent)]
				+ SConstraintCanvas::Slot().Offset(FMargin(140, 260, 460, 500))[SlopUI::Menu(Labels(), [this]() { return Sel; }, [this](int32 I) { return I != 3 || StrangerMet(); }, [this](int32 I) { Sel = I; Choose(); }, 460.f, 34.f)]
				+ SConstraintCanvas::Slot().Offset(FMargin(700, 260, 1100, 640))
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
								S += (I == Page ? TEXT("▸ ") : TEXT("   ")) + Def(Seen[I])->Name + TEXT("\n");
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
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 120, 1920, 120))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(FText::FromString(Def(Fighter)->Name.ToUpper()), 84.f, SlopUI::Amber)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(300, 380, 1320, 400))
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
				"Typefaces: New Rocker by Pablo Impallari, Brenda Gallo and Rodrigo Fuenzalida; Oswald by The Oswald Project Authors; SIL Open Font License 1.1.\n\n"
				"Made with Unreal® Engine. Unreal and its logo are Epic Games, Inc.'s trademarks or registered trademarks in the US and elsewhere.\n"
				"Full licence texts ship with the game in the Licenses folder.");
			return Frame(SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Offset(FMargin(0, 90, 1920, 110))[SNew(SBox).HAlign(HAlign_Center)[SlopUI::Title(LOCTEXT("CreditsT", "CREDITS"), 84.f, SlopUI::Accent)]]
				+ SConstraintCanvas::Slot().Offset(FMargin(240, 260, 1440, 700))
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
	TSharedPtr<FSlopScreen> Select(EMode Mode) { return MakeShared<FSelect>(Mode == EMode::Cpu ? EMode::Training : Mode); }
	TSharedPtr<FSlopScreen> Stage() { return MakeShared<FStage>(); }
	TSharedPtr<FSlopScreen> Tower(FName Fighter) { return MakeShared<FTower>(Fighter); }
	TSharedPtr<FSlopScreen> FaceOff() { return MakeShared<FFaceOff>(); }
	TSharedPtr<FSlopScreen> Devices() { return MakeShared<FDevices>(); }
	TSharedPtr<FSlopScreen> MoveList(FName Fighter, bool bOverlay) { return MakeShared<FMoveList>(Fighter, bOverlay); }
	TSharedPtr<FSlopScreen> Extras() { return MakeShared<FExtras>(); }
	TSharedPtr<FSlopScreen> Ending(FName Fighter, bool bFromArcade) { return MakeShared<FEnding>(Fighter, bFromArcade); }
	TSharedPtr<FSlopScreen> Credits() { return MakeShared<FCredits>(); }
	TSharedPtr<FSlopScreen> TrialsPick() { return MakeShared<FSelect>(EMode::Trials); }
}

#undef LOCTEXT_NAMESPACE
