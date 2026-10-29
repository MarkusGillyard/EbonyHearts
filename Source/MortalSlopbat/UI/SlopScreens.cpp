#include "UI/SlopScreens.h"
#include "UI/SlopStyle.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopMatch.h"
#include "UI/SlopHud.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SlopScreens"

using namespace Slop;

int32 FSlopScreen::Step(int32 Index, int32 Count, int32 Delta, TFunction<bool(int32)> Enabled)
{
	if (Count <= 0)
	{
		return 0;
	}
	int32 I = Index;
	for (int32 N = 0; N < Count; ++N)
	{
		I = (I + Delta + Count) % Count;
		if (!Enabled || Enabled(I))
		{
			return I;
		}
	}
	return Index;
}

void FSlopScreen::Sound(const TCHAR* Name)
{
	if (GI && GI->Audio)
	{
		GI->Audio->Play(Name, -4.f, 1.f, ESlopBus::Ui);
	}
}

namespace SlopScreensImpl
{
	FText Upper(const FString& S) { return FText::FromString(S.ToUpper()); }
	const FFighterDef* Def(FName Id) { return GetContent().Fighter(Id); }
	FString Commas(int32 N) { return FText::AsNumber(N).ToString(); }

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

	FText StoryLine(const TCHAR* Key)
	{
		return FText::FromString(Json::Str(GetContent().Story, Key));
	}

	TSharedPtr<FJsonObject> StoryOf(FName Fighter)
	{
		return Json::Obj(Json::Obj(GetContent().Story, TEXT("fighters")), *Fighter.ToString());
	}

	// A screen built from a heading, a few lines and a menu: results, pause, offers, prompts.
	class FListScreen : public FSlopScreen
	{
	public:
		struct FItem
		{
			FText Label;
			TFunction<void()> Action;
			TFunction<bool()> Enabled;
		};
		FText Heading;
		float HeadingSize = 96.f;
		FLinearColor HeadingColor = SlopUI::Text;
		struct FLine { TAttribute<FText> Text; float Size; FLinearColor Color; bool bDisplay; };
		TArray<FLine> Lines;
		TArray<FItem> Items;
		int32 Sel = 0;
		TFunction<void()> OnBack;
		TFunction<void(float)> OnTick;
		// When set, the label of item I as of now (empty: its fixed label); read whenever it builds.
		TFunction<FText(int32)> LiveLabel;
		bool bOverlay = false;
		bool bMatchBehind = false;
		float Top = 200.f;

		void Line(const TAttribute<FText>& T, float Size = 32.f, const FLinearColor& C = SlopUI::Dim, bool bDisplay = false) { Lines.Add({T, Size, C, bDisplay}); }
		void Item(const FText& L, TFunction<void()> A, TFunction<bool()> E = nullptr) { Items.Add({L, MoveTemp(A), MoveTemp(E)}); }
		virtual bool ShowsMatch() const override { return bMatchBehind || bOverlay; }

		virtual TSharedRef<SWidget> Build() override
		{
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
			Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, Top, 0, 24)[SlopUI::Title(Heading, HeadingSize, HeadingColor)];
			for (const FLine& L : Lines)
			{
				Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)
				[
					SNew(STextBlock).Text(L.Text).Font(L.bDisplay ? SlopUI::Display(L.Size) : SlopUI::Body(L.Size)).ColorAndOpacity(L.Color)
					.Justification(ETextJustify::Center).AutoWrapText(true).WrapTextAt(1400.f)
					.ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
				];
			}
			TArray<FText> Labels;
			for (int32 I = 0; I < Items.Num(); ++I)
			{
				const FText Live = LiveLabel ? LiveLabel(I) : FText();
				Labels.Add(Live.IsEmpty() ? Items[I].Label : Live);
			}
			Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 36, 0, 0)
			[
				SlopUI::Menu(Labels, [this]() { return Sel; }, [this](int32 I) { return !Items[I].Enabled || Items[I].Enabled(); },
					[this](int32 I) { Sel = I; Choose(); }, 520.f, 34.f, [this](int32 I) { Sel = I; Sound(TEXT("ui_move")); })
			];
			TSharedRef<SOverlay> O = SNew(SOverlay);
			if (bOverlay)
			{
				O->AddSlot()[SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(FLinearColor(0, 0, 0, 0.62f))];
			}
			else if (!bMatchBehind)
			{
				O->AddSlot()[SlopUI::Backdrop()];
			}
			O->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)[Col]]];
			O->AddSlot()[SlopUI::Hint([this]()
			{
				const EGlyphs G = GI->Router->Glyphs();
				return FText::Format(LOCTEXT("ListHint", "{0} to choose   ·   {1} to go back"), SlopUI::Confirm(G), SlopUI::Back(G));
			})];
			return O;
		}

		void Choose()
		{
			if (Items.IsValidIndex(Sel) && (!Items[Sel].Enabled || Items[Sel].Enabled()))
			{
				Sound(TEXT("ui_select"));
				// The action may replace this screen; keep it alive until the call returns.
				TSharedPtr<FSlopScreen> Keep = AsShared();
				Items[Sel].Action();
			}
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			auto On = [this](int32 I) { return !Items[I].Enabled || Items[I].Enabled(); };
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, Items.Num(), -1, On); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, Items.Num(), 1, On); Sound(TEXT("ui_move")); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back:
			case EUi::Pause:
				if (OnBack)
				{
					Sound(TEXT("ui_back"));
					TSharedPtr<FSlopScreen> Keep = AsShared();
					OnBack();
				}
				break;
			default: break;
			}
		}

		virtual void Tick(float Dt) override
		{
			if (OnTick)
			{
				OnTick(Dt);
			}
		}
	};

	// --- Title -------------------------------------------------------------------------------------

	class FTitle : public FSlopScreen
	{
	public:
		double T = 0;
		virtual TSharedRef<SWidget> Build() override
		{
			// The premise, a line at a time, as the original told it (story.json "premise").
			TSharedRef<SVerticalBox> Premise = SNew(SVerticalBox);
			int32 N = 0;
			if (const TArray<TSharedPtr<FJsonValue>>* Lines = Json::Arr(GetContent().Story, TEXT("premise")))
			{
				for (const TSharedPtr<FJsonValue>& V : *Lines)
				{
					const int32 I = N++;
					Premise->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)
					[
						SNew(STextBlock).Font(SlopUI::Body(28.f)).Text(FText::FromString(V->AsString()))
						.ColorAndOpacity_Lambda([this, I]()
						{
							FLinearColor C = SlopUI::Text;
							C.A = (float)FMath::Clamp((T - 0.6 - I * 1.1) * 0.9, 0.0, 1.0);
							return FSlateColor(C);
						})
					];
				}
			}
			if (N == 0)
			{
				Premise->AddSlot().AutoHeight().HAlign(HAlign_Center)[SlopUI::Label(LOCTEXT("Tag", "Every river in the world ends in the Sump."), 34.f, SlopUI::Dim)];
			}
			return SNew(SOverlay)
				+ SOverlay::Slot()[SlopUI::Backdrop()]
				+ SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 230, 0, 0)[SlopUI::Title(LOCTEXT("Name", "EBONY HEARTS"), 150.f, SlopUI::Accent)]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 40, 0, 0)[Premise]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 110, 0, 0)
					[
						SNew(STextBlock).Font(SlopUI::Body(40.f)).ColorAndOpacity_Lambda([this]()
						{
							FLinearColor C = SlopUI::Text;
							C.A = 0.55f + 0.45f * FMath::Sin(T * 3.0);
							return FSlateColor(C);
						})
						// Until something is pressed the game cannot know a pad connected before it started
						// (it shares the keyboard's device until it sends input), so any button.
						.Text_Lambda([this]() { return GI->Router->AnyPressed() ? FText::Format(LOCTEXT("Press", "Press {0}"), SlopUI::Confirm(GI->Router->Glyphs())) : LOCTEXT("PressAny", "Press any button"); })
					]
				]]]
				+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(40, 30)
				[
					SlopUI::Label(LOCTEXT("Copy", "© 2026 Markus Gillyard"), 22.f, SlopUI::Dim * 0.8f)
				];
		}
		virtual void Enter() override { GI->Audio->PlayMusic(TEXT("music_menu")); }
		virtual void Tick(float Dt) override { T += Dt; }
		virtual void OnUi(const FUiEvent& E) override
		{
			// Any button goes on: a back pressed here no longer quits (Quit is in the main menu).
			if (E.Action == EUi::Confirm || E.Action == EUi::Pause || E.Action == EUi::Back || E.Action == EUi::Next || E.Action == EUi::Alt || E.Action == EUi::Reset)
			{
				Sound(TEXT("ui_select"));
				GI->Goto(SlopScreens::MainMenu());
			}
		}
	};

	// --- Main menu ------------------------------------------------------------------------------------

	class FMainMenu : public FSlopScreen
	{
	public:
		struct FEntry { FText Label; FText Desc; FName Id; };
		TArray<FEntry> Entries;
		int32 Sel = 0;

		FMainMenu()
		{
			Entries = {
				{LOCTEXT("Arcade", "Arcade"), LOCTEXT("ArcadeD", "Climb a tower of the Slopbat's chosen to the roost itself. Your choice of tower sets the difficulty."), TEXT("arcade")},
				{LOCTEXT("Survival", "Survival"), LOCTEXT("SurvivalD", "An endless ladder: one round a fight, your health carried on with a little given back after each win, and the Slopbat waiting every fifth wave. How far can you climb?"), TEXT("survival")},
				{LOCTEXT("Versus", "Versus"), LOCTEXT("VersusD", "Two players on one machine. Any fighter, any stage, mirror matches welcome."), TEXT("versus")},
				{LOCTEXT("VersusCpu", "Versus CPU"), LOCTEXT("VersusCpuD", "One fight against the CPU: your fighter, theirs, and the stage. It fights at the CPU difficulty in Settings."), TEXT("cpu")},
				{LOCTEXT("Training", "Training"), LOCTEXT("TrainingD", "Practise against a dummy that stands, blocks, jumps or fights back. Move lists, frame data and hitboxes."), TEXT("training")},
				{LOCTEXT("Tutorial", "Tutorial"), LOCTEXT("TutorialD", "Learn to move, block, strike, combo, throw and finish, one step at a time."), TEXT("tutorial")},
				{LOCTEXT("Trials", "Trials"), LOCTEXT("TrialsD", "Every fighter's combos, one at a time, checked as you land them. Clear them all with every fighter."), TEXT("trials")},
				{LOCTEXT("Extras", "Extras"), LOCTEXT("ExtrasD", "Endings you have earned, your records, move lists, and anything else you have found."), TEXT("extras")},
				{LOCTEXT("Settings", "Settings"), LOCTEXT("SettingsD", "Video, sound, comfort, gore and effects, controls and controllers."), TEXT("settings")},
				{LOCTEXT("Quit", "Quit"), LOCTEXT("QuitD", "Leave the Sump."), TEXT("quit")},
			};
		}

		virtual void Enter() override
		{
			Sel = FMath::Clamp(GI->MenuChoice, 0, Entries.Num() - 1);
			GI->Audio->PlayMusic(TEXT("music_menu"));
		}

		virtual TSharedRef<SWidget> Build() override
		{
			TArray<FText> Labels;
			for (const FEntry& E : Entries)
			{
				Labels.Add(E.Label);
			}
			return SNew(SOverlay)
				+ SOverlay::Slot()[SlopUI::Backdrop()]
				+ SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)
				[
					SNew(SConstraintCanvas)
					+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(120, 80, 1200, 150)).AutoSize(true)[SlopUI::Title(LOCTEXT("Name", "EBONY HEARTS"), 96.f, SlopUI::Accent)]
					+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(140, 290, 470, 700)).AutoSize(true)
					[
						SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, 460.f, 34.f, [this](int32 I) { Sel = I; Sound(TEXT("ui_move")); })
					]
					+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(760, 330, 900, 230))
					[
						SlopUI::PanelBox(SNew(STextBlock).Font(SlopUI::Body(30.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true)
							.Text_Lambda([this]() { return Entries[Sel].Desc; }))
					]
					+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(780, 590, 900, 50))
					[
						SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Amber)
						.Text_Lambda([this]() { return GI->Progress.TutorialDone() ? FText::GetEmpty() : LOCTEXT("TutTip", "New to Ebony Hearts? The tutorial takes about five minutes."); })
					]
				]]]
				+ SOverlay::Slot()[SlopUI::Hint([this]()
				{
					const EGlyphs G = GI->Router->Glyphs();
					return FText::Format(LOCTEXT("MenuHint", "Up and down to choose   ·   {0} to select"), SlopUI::Confirm(G));
				})];
		}

		void Choose()
		{
			GI->MenuChoice = Sel;
			Sound(TEXT("ui_select"));
			const FName Id = Entries[Sel].Id;
			FGameFlow& F = GI->Flow;
			if (Id == TEXT("arcade")) GI->Goto(SlopScreens::Select(EMode::Arcade));
			else if (Id == TEXT("survival")) GI->Goto(SlopScreens::Select(EMode::Survival));
			else if (Id == TEXT("versus")) GI->Goto(SlopScreens::Devices());
			else if (Id == TEXT("cpu")) GI->Goto(SlopScreens::Select(EMode::Cpu));
			else if (Id == TEXT("training")) GI->Goto(SlopScreens::Select(EMode::Training));
			else if (Id == TEXT("tutorial"))
			{
				F.EndArcade();
				F.EndSurvival();
				F.Config = FMatchConfig();
				F.Config.Mode = EMode::Tutorial;
				F.Config.P1 = TEXT("ashka");
				F.Config.P2 = TEXT("gorm");
				F.Config.Stage = TEXT("rendering_yards");
				F.Config.RoundsToWin = 1;
				GI->StartMatch();
			}
			else if (Id == TEXT("trials")) GI->Goto(SlopScreens::Select(EMode::Trials));
			else if (Id == TEXT("extras")) GI->Goto(SlopScreens::Extras());
			else if (Id == TEXT("settings")) GI->Goto(SlopScreens::Settings(false));
			else if (Id == TEXT("quit")) GI->Quit();
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, Entries.Num(), -1); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, Entries.Num(), 1); Sound(TEXT("ui_move")); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Goto(SlopScreens::Title()); break;
			default: break;
			}
		}
	};

	// --- In a match --------------------------------------------------------------------------------------

	class FMatchScreen : public FSlopScreen
	{
	public:
		virtual bool ShowsMatch() const override { return true; }
		virtual TSharedRef<SWidget> Build() override { return SNullWidget::NullWidget; }
		virtual void OnUi(const FUiEvent& E) override
		{
			ASlopMatch* M = GI->GetMatch();
			if (!M || M->HasEnded())
			{
				return;
			}
			if (E.Action == EUi::Pause || (E.Action == EUi::Back && E.Device < (int32)EDevice::FirstPad))
			{
				if (M->Sim.Phase == EPhase::Finisher)
				{
					// Skipping a finisher goes straight to the result; it never traps the player.
					M->SkipFinisher();
					return;
				}
				M->SetPaused(true);
				GI->ShowOverlay(SlopScreens::Pause());
			}
			else if (E.Action == EUi::Next && M->Guide)
			{
				M->Guide->Skip();
			}
			else if (E.Action == EUi::Next && M->Dummy)
			{
				M->Dummy->NextMode();
			}
			else if (E.Action == EUi::Reset && (M->Config.Mode == EMode::Training || M->Config.Mode == EMode::Trials))
			{
				M->ResetPositions();
			}
		}
	};

	TSharedPtr<FSlopScreen> MakePause()
	{
		TSharedRef<FListScreen> S = MakeShared<FListScreen>();
		S->bOverlay = true;
		S->Heading = LOCTEXT("Paused", "PAUSED");
		S->Top = 250.f;
		TWeakPtr<FListScreen> W = S;
		auto Resume = [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				if (ASlopMatch* M = P->GI->GetMatch())
				{
					M->SetPaused(false);
				}
				P->GI->CloseOverlay();
			}
		};
		S->Item(LOCTEXT("Resume", "Resume"), Resume);
		S->OnBack = Resume;
		S->Item(LOCTEXT("MoveList", "Move List"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->ShowOverlay(SlopScreens::MoveList(P->GI->GetMatch()->Config.P1, true));
			}
		});
		S->Item(LOCTEXT("TrainingItem", "Training Options"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->ShowOverlay(SlopScreens::TrainingOptions());
			}
		}, [W]()
		{
			TSharedPtr<FListScreen> P = W.Pin();
			const EMode M = P ? P->GI->Flow.Config.Mode : EMode::Arcade;
			return M == EMode::Training || M == EMode::Trials;
		});
		S->Item(LOCTEXT("PauseSettings", "Settings"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->ShowOverlay(SlopScreens::Settings(true));
			}
		});
		S->Item(LOCTEXT("Restart", "Restart"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->RestartMatch();
			}
		}, [W]()
		{
			TSharedPtr<FListScreen> P = W.Pin();
			const EMode M = P ? P->GI->Flow.Config.Mode : EMode::Arcade;
			return M == EMode::Versus || M == EMode::Training || M == EMode::Trials || M == EMode::Cpu || M == EMode::Tutorial;
		});
		S->Item(LOCTEXT("CharSelect", "Character Select"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->CloseOverlay();
				P->GI->Goto(SlopScreens::Select(P->GI->Flow.Config.Mode));
			}
		}, [W]()
		{
			TSharedPtr<FListScreen> P = W.Pin();
			const EMode M = P ? P->GI->Flow.Config.Mode : EMode::Arcade;
			return M == EMode::Versus || M == EMode::Training || M == EMode::Trials || M == EMode::Cpu;
		});
		const USlopGameInstance* QG = USlopGameInstance::Get();
		const EMode QM = QG ? QG->Flow.Config.Mode : EMode::Versus;
		S->Item(QM == EMode::Arcade ? LOCTEXT("QuitArcade", "Quit Arcade") : (QM == EMode::Survival ? LOCTEXT("QuitSurvival", "Quit Survival") : LOCTEXT("QuitToMenu", "Main Menu")), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->Flow.EndArcade();
				P->GI->Flow.EndSurvival();
				P->GI->CloseOverlay();
				P->GI->Goto(SlopScreens::MainMenu());
			}
		});
		return S;
	}

	// --- Training options ------------------------------------------------------------------------------

	// The training views and the dummy, reachable from a controller (the keyboard also has F1 to F8).
	// Each row shows its current state and changes it in place.
	TSharedPtr<FSlopScreen> MakeTraining()
	{
		TSharedRef<FListScreen> S = MakeShared<FListScreen>();
		S->bOverlay = true;
		S->Heading = LOCTEXT("TrainingHead", "TRAINING");
		S->HeadingSize = 80.f;
		S->Top = 200.f;
		TWeakPtr<FListScreen> W = S;
		struct FRow
		{
			FText Name;
			TFunction<FText(ASlopMatch&, SSlopHud&)> Value;
			TFunction<void(ASlopMatch&, SSlopHud&)> Change;
		};
		const auto OnOff = [](bool b) { return b ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); };
		TSharedRef<TArray<FRow>> Rows = MakeShared<TArray<FRow>>();
		Rows->Add({LOCTEXT("TrDummy", "Dummy"), [](ASlopMatch& M, SSlopHud&) { return M.Dummy ? FText::FromString(M.Dummy->ModeName()) : FText(); },
			[](ASlopMatch& M, SSlopHud&) { if (M.Dummy) M.Dummy->NextMode(); }});
		Rows->Add({LOCTEXT("TrBoxes", "Hit and hurt boxes"), [OnOff](ASlopMatch& M, SSlopHud&) { return OnOff(M.bShowBoxes); },
			[](ASlopMatch& M, SSlopHud&) { M.bShowBoxes = !M.bShowBoxes; }});
		Rows->Add({LOCTEXT("TrFrames", "Frame data and advantage"), [OnOff](ASlopMatch&, SSlopHud& H) { return OnOff(H.bShowFrames); },
			[](ASlopMatch&, SSlopHud& H) { H.bShowFrames = !H.bShowFrames; }});
		Rows->Add({LOCTEXT("TrStates", "Fighter states"), [OnOff](ASlopMatch&, SSlopHud& H) { return OnOff(H.bShowStates); },
			[](ASlopMatch&, SSlopHud& H) { H.bShowStates = !H.bShowStates; }});
		Rows->Add({LOCTEXT("TrInput", "Input display"), [OnOff](ASlopMatch&, SSlopHud& H) { return OnOff(H.bInputDisplay); },
			[](ASlopMatch&, SSlopHud& H) { H.bInputDisplay = !H.bInputDisplay; }});
		Rows->Add({LOCTEXT("TrSpeed", "Speed"), [](ASlopMatch& M, SSlopHud&) { return M.bDebugSlow ? LOCTEXT("TrSlow", "Slow") : LOCTEXT("TrNormal", "Normal"); },
			[](ASlopMatch& M, SSlopHud&) { M.bDebugSlow = !M.bDebugSlow; }});
		const auto Label = [W, Rows](int32 I)
		{
			TSharedPtr<FListScreen> P = W.Pin();
			ASlopMatch* M = P && P->GI ? P->GI->GetMatch() : nullptr;
			SSlopHud* H = P && P->GI ? P->GI->GetHud() : nullptr;
			const FRow& R = (*Rows)[I];
			return M && H ? FText::Format(LOCTEXT("TrRow", "{0}: {1}"), R.Name, R.Value(*M, *H)) : R.Name;
		};
		const auto Back = [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				P->GI->ShowOverlay(SlopScreens::Pause());
			}
		};
		for (int32 I = 0; I < Rows->Num(); ++I)
		{
			S->Item((*Rows)[I].Name, [W, Rows, I]()
			{
				TSharedPtr<FListScreen> P = W.Pin();
				ASlopMatch* M = P ? P->GI->GetMatch() : nullptr;
				SSlopHud* H = P ? P->GI->GetHud() : nullptr;
				if (!M || !H)
				{
					return;
				}
				(*Rows)[I].Change(*M, *H);
				P->GI->RebuildScreen();
			}, I == 0 ? TFunction<bool()>([W]()
			{
				TSharedPtr<FListScreen> P = W.Pin();
				ASlopMatch* M = P ? P->GI->GetMatch() : nullptr;
				return M && M->Dummy && !M->Guide;
			}) : TFunction<bool()>());
		}
		S->Item(LOCTEXT("TrReset", "Reset positions"), [W]()
		{
			if (TSharedPtr<FListScreen> P = W.Pin())
			{
				if (ASlopMatch* M = P->GI->GetMatch())
				{
					M->ResetPositions();
					M->SetPaused(false);
				}
				P->GI->CloseOverlay();
			}
		});
		S->Item(LOCTEXT("TrBack", "Back"), Back);
		S->OnBack = Back;
		S->LiveLabel = [Rows, Label](int32 I) { return I < Rows->Num() ? Label(I) : FText(); };
		return S;
	}

	// --- Results ---------------------------------------------------------------------------------------

	TSharedPtr<FSlopScreen> MakeResults(const FMatchResult& R)
	{
		TSharedRef<FListScreen> S = MakeShared<FListScreen>();
		S->bMatchBehind = false;
		S->Top = 220.f;
		USlopGameInstance* GI = USlopGameInstance::Get();
		FGameFlow& F = GI->Flow;
		TWeakPtr<FListScreen> W = S;
		auto GoMenu = [GI]()
		{
			GI->Flow.EndArcade();
			GI->Flow.EndSurvival();
			GI->Goto(SlopScreens::MainMenu());
		};
		auto Flourish = [S, &R]()
		{
			FString P;
			if (R.bFlawless)
			{
				P += LOCTEXT("FlawlessV", "FLAWLESS VICTORY").ToString();
			}
			if (!R.Finisher.IsNone())
			{
				if (!P.IsEmpty())
				{
					P += TEXT("   ·   ");
				}
				P += R.Finisher.ToString().StartsWith(TEXT("stage_")) ? LOCTEXT("StageSlop", "STAGE SLOPTALITY").ToString() : LOCTEXT("Slop", "SLOPTALITY").ToString();
			}
			if (!P.IsEmpty())
			{
				S->Line(FText::FromString(P), 48.f, SlopUI::Accent, true);
			}
		};
		const EMode M = R.Mode;
		const TSharedPtr<FJsonObject> Secret = Json::Obj(GetContent().Story, TEXT("secret"));
		if (M == EMode::Arcade && F.OnSecretRung())
		{
			// The old master's challenge, won or lost, ends the run: there is no continue against
			// him, and the tower was already recorded when the Slopbat fell.
			const bool bWon = R.Winner == 0;
			const FName Fighter = R.P1;
			if (bWon)
			{
				const int32 Pts = F.AddArcadeScore(R);
				GI->Progress.Unlock(USlopGameInstance::UnlockKey(FGameFlow::SecretFighter()));
				GI->Progress.RaiseArcadeScore(Fighter, F.ArcadeTower(), F.ArcadeScore());
				S->Line(FText::Format(LOCTEXT("ScoreLine", "+{0} points   ·   total {1}"), FText::AsNumber(Pts), FText::AsNumber(F.ArcadeScore())), 30.f, SlopUI::Amber);
			}
			S->Heading = FText::FromString(Json::Str(Secret, bWon ? TEXT("won") : TEXT("lost")));
			S->HeadingSize = 44.f;
			if (bWon)
			{
				S->Line(FText::FromString(Json::Str(Secret, TEXT("unlocked"))), 30.f, SlopUI::Accent);
			}
			S->Item(LOCTEXT("Continue", "Continue"), [GI, Fighter]() { GI->Goto(SlopScreens::Ending(Fighter, true)); });
		}
		else if (M == EMode::Arcade)
		{
			if (R.Winner == 0)
			{
				const int32 Pts = F.AddArcadeScore(R);
				const int32 Total = F.ArcadeScore();
				const bool bMore = F.ArcadeAdvance(R.bFlawless, GI->Now());
				const FText Score = FText::Format(LOCTEXT("ScoreLine", "+{0} points   ·   total {1}"), FText::AsNumber(Pts), FText::AsNumber(Total));
				if (bMore)
				{
					S->Heading = FText::Format(LOCTEXT("WinsX", "{0} WINS"), Upper(Def(R.P1)->Name));
					Flourish();
					S->Line(Score, 30.f, SlopUI::Amber);
					const FRung* Next = F.CurrentRung();
					const FText Who = Next->Kind == TEXT("boss") ? LOCTEXT("TheSlopbat", "the Slopbat") : (Next->Kind == TEXT("mirror") ? LOCTEXT("TheReflection", "the Reflection") : FText::FromString(Def(Next->Opponent)->Name));
					S->Line(FText::Format(LOCTEXT("Next", "Next: {0}"), Who), 30.f, SlopUI::Dim);
					S->Item(LOCTEXT("Continue", "Continue"), [GI]() { GI->Goto(GI->Flow.bStrangerPending ? SlopScreens::StrangerOffer() : SlopScreens::FaceOff()); });
					S->Item(LOCTEXT("QuitArcade", "Quit Arcade"), GoMenu);
				}
				else
				{
					const int32 Tower = F.ArcadeTower();
					const int32 OldBest = GI->Progress.BestScore(R.P1, Tower);
					const double Secs = F.ArcadeSeconds();
					GI->Progress.RecordArcade(R.P1, Tower, Secs, Total);
					S->Heading = StoryLine(TEXT("boss_defeated"));
					S->HeadingSize = 48.f;
					S->Line(Score, 30.f, SlopUI::Amber);
					S->Line(FText::Format(LOCTEXT("TimeLine", "Time {0}:{1}   ·   continues {2}{3}"), (int32)Secs / 60, FText::FromString(FString::Printf(TEXT("%02d"), (int32)Secs % 60)),
						F.ArcadeContinues(), Total > OldBest ? LOCTEXT("NewBest", "   ·   NEW BEST SCORE") : FText::GetEmpty()), 30.f, Total > OldBest ? SlopUI::Accent : SlopUI::Dim);
					const FName Fighter = R.P1;
					if (F.SecretEarned() && !GI->IsUnlocked(FGameFlow::SecretFighter()))
					{
						// No continue all the way up: someone on the still water has been watching.
						S->Line(FText::FromString(Json::Str(Secret, TEXT("offer"))), 30.f, SlopUI::Text);
						S->Item(FText::FromString(Json::Str(Secret, TEXT("approach"), TEXT("Face him"))), [GI]()
						{
							GI->Flow.AddSecretRung();
							GI->Goto(SlopScreens::FaceOff());
						});
						S->Item(FText::FromString(Json::Str(Secret, TEXT("leave"), TEXT("Walk on"))), [GI, Fighter]() { GI->Goto(SlopScreens::Ending(Fighter, true)); });
					}
					else
					{
						S->Item(LOCTEXT("Continue", "Continue"), [GI, Fighter]() { GI->Goto(SlopScreens::Ending(Fighter, true)); });
					}
				}
			}
			else
			{
				S->Heading = LOCTEXT("ContinueQ", "CONTINUE?");
				S->HeadingSize = 110.f;
				TSharedRef<double> Count = MakeShared<double>(10.99);
				S->Line(TAttribute<FText>::CreateLambda([Count]() { return FText::AsNumber(FMath::Max(0, (int32)*Count)); }), 140.f, SlopUI::Rust * 1.3f, true);
				S->Item(LOCTEXT("Retry", "Retry"), [GI]()
				{
					GI->Flow.ArcadeRetry();
					GI->Goto(SlopScreens::FaceOff());
				});
				S->Item(LOCTEXT("QuitArcade", "Quit Arcade"), GoMenu);
				S->OnTick = [Count, GoMenu](float Dt)
				{
					*Count -= Dt;
					if (*Count <= 0.0)
					{
						*Count = 1e9;
						GoMenu();
					}
				};
			}
		}
		else if (M == EMode::Survival)
		{
			const FFighterDef* P1 = Def(R.P1);
			if (R.Winner == 0)
			{
				F.SurvivalAdvance(R.Health[0], R.bFlawless);
				const int32 Waves = F.SurvivalWaves();
				S->Heading = FText::Format(LOCTEXT("WaveCleared", "WAVE {0} CLEARED"), Waves);
				Flourish();
				const int32 Carried = F.Config.P1Health > 0 ? F.Config.P1Health : P1->Health;
				const FText Next = F.Config.P2 == TEXT("slopbat") ? LOCTEXT("TheSlopbat", "the Slopbat") : FText::FromString(Def(F.Config.P2)->Name);
				S->Line(FText::Format(LOCTEXT("Carried", "Health carried on: {0}%   ·   Next: wave {1}, {2} ({3})"), FMath::RoundToInt(100.0 * Carried / P1->Health),
					Waves + 1, Next, FGameFlow::DifficultyName(F.Config.Difficulty)), 30.f, SlopUI::Dim);
				S->Item(LOCTEXT("Continue", "Continue"), [GI]() { GI->Goto(SlopScreens::FaceOff()); });
				S->Item(LOCTEXT("QuitSurvival", "Quit Survival"), GoMenu);
			}
			else
			{
				const int32 Waves = F.SurvivalWaves();
				const bool bBest = GI->Progress.RecordSurvival(R.P1, Waves);
				S->Heading = FText::Format(LOCTEXT("Survived", "{0} SURVIVED {1} {1}|plural(one=WAVE,other=WAVES)"), Upper(P1->Name), Waves);
				S->HeadingSize = 80.f;
				S->Line(bBest && Waves > 0 ? LOCTEXT("NewRecord", "NEW RECORD") : FText::Format(LOCTEXT("BestWith", "Best with {0}: {1}"), FText::FromString(P1->Name), GI->Progress.BestSurvival(R.P1)),
					34.f, bBest && Waves > 0 ? SlopUI::Accent : SlopUI::Dim);
				const FName Fighter = R.P1;
				S->Item(LOCTEXT("TryAgain", "Try Again"), [GI, Fighter]()
				{
					GI->Flow.StartSurvival(Fighter, (int32)(FDateTime::Now().GetTicks() % 100000));
					GI->Goto(SlopScreens::FaceOff());
				});
				S->Item(LOCTEXT("CharSelect", "Character Select"), [GI]() { GI->Flow.EndSurvival(); GI->Goto(SlopScreens::Select(EMode::Survival)); });
				S->Item(LOCTEXT("MainMenu", "Main Menu"), GoMenu);
			}
		}
		else if (M == EMode::Stranger)
		{
			S->Heading = LOCTEXT("TheStranger", "THE STRANGER");
			S->HeadingSize = 80.f;
			if (!R.StrangerLine.IsEmpty())
			{
				S->Line(FText::Format(LOCTEXT("Quote", "“{0}”"), FText::FromString(R.StrangerLine)), 36.f, SlopUI::Text);
			}
			if (F.Config.bFromArcade && F.InArcade())
			{
				S->Item(LOCTEXT("Continue", "Continue"), [GI]()
				{
					GI->Flow.bStrangerPending = false;
					GI->Flow.ResumeArcade();
					GI->Goto(SlopScreens::FaceOff());
				});
			}
			else
			{
				S->Item(LOCTEXT("Extras", "Extras"), [GI]() { GI->Goto(SlopScreens::Extras()); });
				S->Item(LOCTEXT("MainMenu", "Main Menu"), GoMenu);
			}
		}
		else if (M == EMode::Tutorial)
		{
			S->Heading = LOCTEXT("TutDone", "TUTORIAL COMPLETE");
			S->HeadingSize = 80.f;
			GI->Progress.SetTutorialDone();
			S->Item(LOCTEXT("MainMenu", "Main Menu"), GoMenu);
		}
		else
		{
			S->Heading = R.Winner >= 0 ? FText::Format(LOCTEXT("WinsX", "{0} WINS"), Upper(Def(R.Winner == 0 ? R.P1 : R.P2)->Name)) : LOCTEXT("DrawH", "DRAW");
			if (M == EMode::Versus && R.Winner >= 0)
			{
				S->Line(FText::Format(LOCTEXT("PlayerN", "Player {0}"), R.Winner + 1), 34.f, R.Winner == 0 ? SlopUI::Accent : SlopUI::Amber);
			}
			else if (M == EMode::Cpu)
			{
				// Who that was, for a mirror match, and at what difficulty.
				S->Line(FText::Format(R.Winner == 0 ? LOCTEXT("YouBeatCpu", "You beat the CPU ({0})") : (R.Winner == 1 ? LOCTEXT("CpuBeatYou", "The CPU wins ({0})") : LOCTEXT("CpuDraw", "Against the CPU ({0})")),
					FGameFlow::DifficultyName(F.Config.Difficulty)), 34.f, R.Winner == 1 ? SlopUI::Amber : SlopUI::Accent);
			}
			Flourish();
			S->Item(LOCTEXT("Rematch", "Rematch"), [GI]() { GI->RestartMatch(); });
			S->Item(LOCTEXT("CharSelect", "Character Select"), [GI, M]() { GI->Goto(SlopScreens::Select(M)); });
			S->Item(LOCTEXT("MainMenu", "Main Menu"), GoMenu);
		}
		S->OnBack = nullptr;
		return S;
	}

	TSharedPtr<FSlopScreen> MakeDataError()
	{
		TSharedRef<FListScreen> S = MakeShared<FListScreen>();
		S->Heading = LOCTEXT("DataErr", "THE GAME DATA IS DAMAGED");
		S->HeadingSize = 64.f;
		S->HeadingColor = SlopUI::Rust * 1.4f;
		int32 N = 0;
		for (const FString& E : GetContent().Errors)
		{
			if (++N > 8)
			{
				break;
			}
			S->Line(FText::FromString(E), 24.f, SlopUI::Dim);
		}
		USlopGameInstance* GI = USlopGameInstance::Get();
		S->Item(LOCTEXT("Quit", "Quit"), [GI]() { GI->Quit(); });
		return S;
	}
}

using namespace SlopScreensImpl;

namespace SlopScreens
{
	TSharedPtr<FSlopScreen> Title() { return Named(MakeShared<FTitle>(), TEXT("Title")); }
	TSharedPtr<FSlopScreen> MainMenu() { return Named(MakeShared<FMainMenu>(), TEXT("MainMenu")); }
	TSharedPtr<FSlopScreen> MatchScreen() { return Named(MakeShared<FMatchScreen>(), TEXT("Match")); }
	TSharedPtr<FSlopScreen> Pause() { return Named(MakePause(), TEXT("Pause")); }
	TSharedPtr<FSlopScreen> TrainingOptions() { return Named(MakeTraining(), TEXT("TrainingOptions")); }
	TSharedPtr<FSlopScreen> Results(const FMatchResult& R) { return Named(MakeResults(R), TEXT("Results")); }
	TSharedPtr<FSlopScreen> DataError() { return Named(MakeDataError(), TEXT("DataError")); }

	TSharedPtr<FSlopScreen> StrangerOffer()
	{
		TSharedRef<FListScreen> S = MakeShared<FListScreen>();
		S->ScreenName = TEXT("StrangerOffer");
		const TSharedPtr<FJsonObject> St = Json::Obj(GetContent().Story, TEXT("stranger"));
		S->Heading = FText::FromString(Json::Str(St, TEXT("offer"), TEXT("A stranger in white watches the fight from the far walkway.")));
		S->HeadingSize = 48.f;
		S->Top = 320.f;
		USlopGameInstance* GI = USlopGameInstance::Get();
		S->Item(FText::FromString(Json::Str(St, TEXT("approach"), TEXT("Approach him"))), [GI]()
		{
			GI->Flow.bStrangerPending = false;
			GI->Flow.StartStranger(true);
			GI->StartMatch();
		});
		S->Item(FText::FromString(Json::Str(St, TEXT("leave"), TEXT("Walk on"))), [GI]()
		{
			GI->Flow.bStrangerPending = false;
			GI->Goto(SlopScreens::FaceOff());
		});
		return S;
	}
}

#undef LOCTEXT_NAMESPACE
