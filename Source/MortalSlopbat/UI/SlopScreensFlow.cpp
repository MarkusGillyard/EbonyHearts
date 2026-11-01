#include "UI/SlopScreens.h"
#include "UI/SlopStyle.h"
#include "UI/SlopWidgets.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopMatch.h"
#include "View/SlopShowroom.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SlopScreensFlow"

using namespace Slop;

namespace SlopScreensFlowImpl
{
	const FFighterDef* Def(FName Id) { return GetContent().Fighter(Id); }
	TSharedPtr<FJsonObject> StoryOf(FName Fighter) { return Json::Obj(Json::Obj(GetContent().Story, TEXT("fighters")), *Fighter.ToString()); }

	FText Upper(const FString& S) { return FText::FromString(S.ToUpper()); }

	// A portrait tile: a fighter's face in a hairline frame, or a sealed mark when there is none to
	// show. The frame's colour and the numbers over it are live, so a cursor is never colour alone.
	TSharedRef<SWidget> Portrait(FName Id, bool bSealed, const FVector2D& Size, TAttribute<FSlateColor> Tone)
	{
		TSharedRef<SOverlay> Face = SNew(SOverlay);
		Face->AddSlot()[SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(SlopUI::Panel)];
		const FSlateBrush* B = bSealed ? nullptr : SlopUI::Portrait(Id);
		if (B)
		{
			Face->AddSlot()[SNew(SImage).Image(B).ColorAndOpacity(Tone)];
		}
		else
		{
			Face->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Size.X * 0.46f).HeightOverride(Size.X * 0.46f)[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::SpikeHollow)).ColorAndOpacity(SlopUI::Faint)]
			];
		}
		return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)[Face];
	}

	TArray<SlopUI::FPrompt> BackOnly() { return {{SlopUI::EPrompt::Back, LOCTEXT("PBack", "Back")}}; }

	// --- Fighter select ------------------------------------------------------------------------------

	class FSelect : public FSlopScreen
	{
	public:
		EMode Mode;
		TArray<FName> Ids;
		int32 Cursor[2] = {0, 1};
		bool bLocked[2] = {false, false};
		bool bCostume[2] = {false, false};
		int32 Outfit[2] = {0, 0};
		FName OutfitBody(int32 Side) const
		{
			const TArray<FName> O = GI->OutfitsOf(Ids[Cursor[Side]]);
			return Outfit[Side] > 0 && O.IsValidIndex(Outfit[Side]) ? O[Outfit[Side]] : NAME_None;
		}
		// Training and Versus CPU: after the player's fighter, the other side's.
		bool bPickingDummy = false;
		static constexpr int32 Cols = 4;

		// The roster, then the secret fighters: a locked one keeps its place as a sealed tile.
		explicit FSelect(EMode InMode) : Mode(InMode), Ids(USlopGameInstance::SelectOrder()) {}

		bool TwoSides() const { return Mode == EMode::Versus; }
		bool Locked(int32 I) const { return !GI->IsUnlocked(Ids[I]); }
		// A random fighter that may be picked, other than `Not`.
		int32 RandomPick(int32 Not) const
		{
			TArray<int32> Open;
			for (int32 I = 0; I < Ids.Num(); ++I)
			{
				if (I != Not && !Locked(I))
				{
					Open.Add(I);
				}
			}
			return Open.Num() > 0 ? Open[FMath::RandRange(0, Open.Num() - 1)] : FMath::Max(0, Not);
		}

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
				Cursor[0] = RandomPick(-1);
				Cursor[1] = RandomPick(-1);
			}
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				// Two great figures, cut off at the shins, with the roster at their feet.
				Room->SetLayout(400.0, 1520.0, 1190.0, 1040.0);
			}
		}

		// Where a player's cursor is: 0 none, 1 first player, 2 second, 3 both.
		int32 CursorsOn(int32 I) const
		{
			const bool b0 = Cursor[0] == I && (!bPickingDummy || !bLocked[0]);
			const bool b1 = (TwoSides() || bPickingDummy) && Cursor[1] == I;
			return (b0 ? 1 : 0) | (b1 ? 2 : 0);
		}

		TSharedRef<SWidget> Tile(int32 I)
		{
			auto Border = [this, I]()
			{
				const int32 On = CursorsOn(I);
				return On == 3 ? SlopUI::Text : (On == 1 ? SlopUI::Spark : (On == 2 ? SlopUI::Second : SlopUI::Line));
			};
			const bool bSealed = Locked(I);
			const FVector2D Size(116, 138);
			TSharedRef<SOverlay> Face = SNew(SOverlay);
			// Sealed: nothing of who waits here shows, only that someone does.
			Face->AddSlot()[Portrait(Ids[I], bSealed, Size, TAttribute<FSlateColor>::CreateLambda([this, I]() { return FSlateColor(CursorsOn(I) ? FLinearColor::White : FLinearColor(0.42f, 0.4f, 0.46f)); }))];
			Face->AddSlot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity_Lambda(Border)];
			Face->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(8, 4)
			[
				SNew(STextBlock).Font(SlopUI::Strong(22.f)).ColorAndOpacity_Lambda(Border).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
				.Text_Lambda([this, I]() { const int32 On = CursorsOn(I); return On == 3 ? LOCTEXT("Both", "1 2") : (On == 2 ? LOCTEXT("Two", "2") : (On == 1 ? LOCTEXT("One", "1") : FText::GetEmpty())); })
			];
			return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
				.OnMouseButtonDown_Lambda([this, I](const FGeometry&, const FPointerEvent&)
				{
					// While the other side's fighter is picked (Training, Versus CPU), the mouse
					// picks that one.
					const int32 Side = bPickingDummy ? 1 : 0;
					Cursor[Side] = I;
					Confirm(Side);
					return FReply::Handled();
				})
				.OnMouseMove_Lambda([this, I](const FGeometry&, const FPointerEvent&)
				{
					const int32 Side = bPickingDummy ? 1 : 0;
					if (Cursor[Side] != I && !bLocked[Side])
					{
						// The figure follows the cursor (Tick).
						Cursor[Side] = I;
						Sound(TEXT("ui_move"));
					}
					return FReply::Unhandled();
				})
				[
					Face
				];
		}

		bool SideShown(int32 Side) const { return Side == 0 || TwoSides() || bPickingDummy; }

		FText Waiting() const
		{
			return Mode == EMode::Arcade ? LOCTEXT("Await", "The Outer Gods are watching.")
				: Mode == EMode::Survival ? LOCTEXT("Waves", "Wave after wave, until you fall.")
				: Mode == EMode::Training ? LOCTEXT("Partner", "Then a sparring partner to practise on.")
				: Mode == EMode::Cpu ? FText::Format(LOCTEXT("CpuSide", "Then the CPU's fighter ({0})."), FGameFlow::DifficultyName(FMath::Clamp(GI->Settings.Difficulty, 0, 2)))
				: LOCTEXT("RivalTrials", "Your rival will stand for the trials.");
		}

		// Only what helps the pick: who they are, how they fight, and what this screen can change.
		FText Notes(int32 Side) const
		{
			if (!SideShown(Side))
			{
				return Waiting();
			}
			if (Locked(Cursor[Side]))
			{
				return FText::FromString(Json::Str(Json::Obj(GetContent().Story, TEXT("secret")), TEXT("hint"), TEXT("Someone is watching.")));
			}
			FString S;
			if (Mode == EMode::Trials && Side == 0)
			{
				// How far through this fighter's trials, as the original showed.
				const TArray<TSharedPtr<FJsonValue>>* List = Json::Arr(GetContent().Trials, *Ids[Cursor[Side]].ToString());
				S += FText::Format(LOCTEXT("TrialsDone", "Trials cleared: {0} of {1}"), GI->Progress.TrialsDone(Ids[Cursor[Side]]).Num(), List ? List->Num() : 0).ToString();
			}
			const auto Add = [&S](const FText& L)
			{
				if (!S.IsEmpty())
				{
					S += TEXT("\n");
				}
				S += L.ToString();
			};
			if (GI->OutfitsOf(Ids[Cursor[Side]]).Num() > 1)
			{
				static const TMap<FName, FText> Names = {{TEXT("wren"), LOCTEXT("OutfitAttire", "her attire")}, {TEXT("wren_mma"), LOCTEXT("OutfitMma", "MMA gear")},
					{TEXT("wren_muay_thai"), LOCTEXT("OutfitThai", "Muay Thai gear")}, {TEXT("wren_gi"), LOCTEXT("OutfitGi", "BJJ gi")}, {TEXT("wren_uniform"), LOCTEXT("OutfitUniform", "the uniform")},
					{TEXT("bram"), LOCTEXT("OutfitWatch", "his uniform")}, {TEXT("bram_coat_off"), LOCTEXT("OutfitCoatOff", "coat off")}};
				const FName Body = OutfitBody(Side).IsNone() ? Ids[Cursor[Side]] : OutfitBody(Side);
				Add(FText::Format(LOCTEXT("OutfitLine", "{0}: outfit ({1})"), SlopUI::ActionKey(GI, EGameAction::HK, Side), Names.FindRef(Body)));
			}
			else if (GI->Progress.IsUnlocked(TEXT("costume:") + Ids[Cursor[Side]].ToString()))
			{
				Add(FText::Format(LOCTEXT("Alt", "{0}: alternate colours ({1})"), SlopUI::ActionKey(GI, EGameAction::HK, Side), bCostume[Side] ? LOCTEXT("On", "on") : LOCTEXT("Off", "off")));
			}
			return FText::FromString(S);
		}

		TSharedRef<SWidget> Info(int32 Side)
		{
			const ETextJustify::Type Justify = Side == 0 ? ETextJustify::Left : ETextJustify::Right;
			const EHorizontalAlignment Align = Side == 0 ? HAlign_Left : HAlign_Right;
			// As wide as the roster between the two sides leaves, on the narrowest canvas.
			return SNew(SBox).WidthOverride(610.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(Align)
				[
					SNew(STextBlock).Font(SlopUI::Caps(22.f)).ColorAndOpacity(SlopUI::Good).Text_Lambda([this, Side]() { return bLocked[Side] && !(bPickingDummy && Side == 0) ? LOCTEXT("Ready", "READY") : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(Align)
				[
					SNew(STextBlock).Font(SlopUI::Call(68.f)).ColorAndOpacity(SlopUI::Text).ShadowOffset(FVector2D(2, 3)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))
					.Justification(Justify).AutoWrapText(true)
					.Text_Lambda([this, Side]() { return !SideShown(Side) ? FText::GetEmpty() : (Locked(Cursor[Side]) ? FText::FromString(TEXT("?")) : Upper(Def(Ids[Cursor[Side]])->Name)); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0, 2, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Caps(21.f)).ColorAndOpacity(Side == 0 ? SlopUI::Spark : SlopUI::Second).Justification(Justify).AutoWrapText(true).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
					.Text_Lambda([this, Side]()
					{
						if (!SideShown(Side) || Locked(Cursor[Side]))
						{
							return FText::GetEmpty();
						}
						const FFighterDef* D = Def(Ids[Cursor[Side]]);
						return FText::FromString((D->Title + TEXT("   ·   ") + D->Style).ToUpper());
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(Align).Padding(0, 6, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Body(24.f)).ColorAndOpacity(SlopUI::Dim).Justification(Justify).AutoWrapText(true).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
					.Text_Lambda([this, Side]() { return Notes(Side); })
				]
			];
		}

		FText ModeLine() const
		{
			switch (Mode)
			{
			case EMode::Arcade: return LOCTEXT("MArcade", "ARCADE");
			case EMode::Survival: return LOCTEXT("MSurvival", "SURVIVAL");
			case EMode::Versus: return LOCTEXT("MVersus", "VERSUS");
			case EMode::Cpu: return LOCTEXT("MCpu", "VERSUS CPU");
			case EMode::Training: return LOCTEXT("MTraining", "TRAINING");
			case EMode::Trials: return LOCTEXT("MTrials", "TRIALS");
			default: return FText::GetEmpty();
			}
		}

		virtual TSharedRef<SWidget> Build() override
		{
			TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
			for (int32 R = 0; R * Cols < Ids.Num(); ++R)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 C = 0; C < Cols && R * Cols + C < Ids.Num(); ++C)
				{
					Row->AddSlot().AutoWidth().Padding(4)[Tile(R * Cols + C)];
				}
				Grid->AddSlot().AutoHeight().HAlign(HAlign_Center)[Row];
			}
			// The figures themselves stand behind this (ASlopShowroom): each side's name lies at its
			// feet, and the roster between them.
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopCentre, FVector2D(0.f, 40.f),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SlopUI::CapsLabel(ModeLine(), 21.f, SlopUI::Spark)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Display(46.f)).ColorAndOpacity(SlopUI::Text).ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))
					.Text_Lambda([this]() { return bPickingDummy ? LOCTEXT("ChooseDummy", "Choose Your Opponent") : LOCTEXT("Choose", "Choose Your Fighter"); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)[SlopUI::Rule(560.f)],
				FVector2D(0.5f, 0.f));
			SlopUI::Place(C, SlopUI::FootCentre, FVector2D(0.f, -88.f), Grid, FVector2D(0.5f, 1.f));
			SlopUI::Place(C, SlopUI::FootLeft, FVector2D(84.f, -88.f), Info(0), FVector2D(0.f, 1.f));
			SlopUI::Place(C, SlopUI::FootRight, FVector2D(-84.f, -88.f), Info(1), FVector2D(1.f, 1.f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::Move, LOCTEXT("SelMove", "Choose")}, {SlopUI::EPrompt::Confirm, LOCTEXT("SelPick", "Pick")},
				{SlopUI::EPrompt::Next, LOCTEXT("SelRandom", "Random")}, {SlopUI::EPrompt::Back, LOCTEXT("SelBack", "Back")}}, SlopUI::EVeil::Foot);
		}

		// The figures follow the cursors.
		virtual void Tick(float Dt) override
		{
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				Room->Show(0, Locked(Cursor[0]) ? NAME_None : Ids[Cursor[0]], bCostume[0], OutfitBody(0));
				Room->Show(1, SideShown(1) && !Locked(Cursor[1]) ? Ids[Cursor[1]] : NAME_None, bCostume[1], OutfitBody(1));
			}
			if (Then && (FinishIn -= Dt) <= 0.f)
			{
				TFunction<void()> Go = MoveTemp(Then);
				Then = nullptr;
				Go();
			}
		}

		// The win pose plays for a moment before the next screen.
		void Finish(TFunction<void()> Go)
		{
			GI->Flow.Config.Outfit[0] = OutfitBody(0);
			GI->Flow.Config.Outfit[1] = OutfitBody(1);
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
			if (Locked(Cursor[bPickingDummy ? 1 : Side]))
			{
				Sound(TEXT("ui_locked"));
				return;
			}
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
				F.Config.RoundsToWin = FMath::Clamp(GI->Settings.VersusRounds, 1, 5);
				F.Config.RoundSeconds = GI->Settings.RoundSeconds;
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
				Finish([this]() { GI->EnterFight(true); });
				break;
			case EMode::Trials:
				F.StartTrials(P1);
				F.Config.bCostume[0] = bCostume[0];
				Finish([this]() { GI->EnterFight(false); });
				break;
			case EMode::Training:
			case EMode::Cpu:
			default:
				F.EndArcade();
				F.EndSurvival();
				F.Config = FMatchConfig();
				F.Config.Mode = Mode == EMode::Cpu ? EMode::Cpu : EMode::Training;
				F.Config.Difficulty = FMath::Clamp(GI->Settings.Difficulty, 0, 2);
				F.Config.P1 = P1;
				F.Config.bCostume[0] = bCostume[0];
				F.Config.RoundsToWin = Mode == EMode::Cpu ? FMath::Clamp(GI->Settings.VersusRounds, 1, 5) : 1;
				F.Config.RoundSeconds = Mode == EMode::Cpu ? GI->Settings.RoundSeconds : 90;
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
					Cursor[Side] = RandomPick(Cursor[Side]);
					bCostume[Side] = false;
					Outfit[Side] = 0;
					Sound(TEXT("ui_move"));
				}
				break;
			case EUi::Alt:
				if (GI->OutfitsOf(Ids[Cursor[Side]]).Num() > 1)
				{
					Outfit[Side] = (Outfit[Side] + 1) % GI->OutfitsOf(Ids[Cursor[Side]]).Num();
					Sound(TEXT("ui_move"));
				}
				else if (GI->Progress.IsUnlocked(TEXT("costume:") + Ids[Cursor[Side]].ToString()))
				{
					bCostume[Side] = !bCostume[Side];
					Sound(TEXT("ui_move"));
				}
				else
				{
					// Not yet earned: say so, as the original did.
					Sound(TEXT("ui_locked"));
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
				Labels.Add(SlopUI::StageName(Id));
			}
			// Random, after the grounds, as the original had it.
			Labels.Add(LOCTEXT("RandomStage", "Random"));
			// Random's picture is all of theirs, two by two.
			TSharedRef<SUniformGridPanel> All = SNew(SUniformGridPanel).SlotPadding(FMargin(4.f));
			for (int32 I = 0; I < Ids.Num(); ++I)
			{
				All->AddSlot(I % 2, I / 2)[SNew(SImage).Image(SlopUI::StagePicture(Ids[I]))];
			}
			SlopUI::FMenuStyle Style;
			Style.Width = 540.f;
			Style.Size = 33.f;
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 72.f), SlopUI::Heading(LOCTEXT("ChooseStage", "Choose the Ground")));
			SlopUI::Place(C, SlopUI::MidLeft, FVector2D(130.f, 20.f),
				SlopUI::Menu(Labels, [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, [this](int32 I) { Sel = I; Sound(TEXT("ui_move")); }, Style),
				FVector2D(0.f, 0.5f));
			// The ground itself (renders of the rebuilt sets) and its words below, the picture 16:9.
			SlopUI::Place(C, SlopUI::MidRight, FVector2D(-110.f, -30.f), SNew(SBox).WidthOverride(1080.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(1080.f).HeightOverride(607.5f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(SlopUI::Panel)]
							+ SOverlay::Slot()
							[
								SNew(SImage).Image_Lambda([this]() { return Ids.IsValidIndex(Sel) ? SlopUI::StagePicture(Ids[Sel]) : nullptr; })
								.Visibility_Lambda([this]() { return Ids.IsValidIndex(Sel) && SlopUI::StagePicture(Ids[Sel]) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							]
							+ SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this]() { return Ids.IsValidIndex(Sel) ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })[All]]
							+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity(SlopUI::Line)]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
					[
						SNew(STextBlock).Font(SlopUI::Display(34.f)).ColorAndOpacity(SlopUI::Text)
						.Text_Lambda([this]() { return Ids.IsValidIndex(Sel) ? SlopUI::StageName(Ids[Sel]) : LOCTEXT("RandomName", "Random"); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
					[
						SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Dim).AutoWrapText(true)
						.Text_Lambda([this]() { return Ids.IsValidIndex(Sel) ? SlopUI::StageBlurb(Ids[Sel]) : LOCTEXT("RandomBlurb", "Any of the grounds above, chosen as the fight begins."); })
					]
				], FVector2D(1.f, 0.5f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::UpDown, LOCTEXT("StMove", "Choose")}, {SlopUI::EPrompt::Confirm, LOCTEXT("StPick", "Select")}, {SlopUI::EPrompt::Back, LOCTEXT("StBack", "Back")}});
		}
		void Choose()
		{
			Sound(TEXT("ui_select"));
			GI->Flow.Config.Stage = Ids.IsValidIndex(Sel) ? Ids[Sel] : Ids[FMath::RandRange(0, Ids.Num() - 1)];
			GI->EnterFight(GI->Flow.Config.Mode != EMode::Training);
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			case EUi::Up: Sel = Step(Sel, Ids.Num() + 1, -1); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, Ids.Num() + 1, 1); Sound(TEXT("ui_move")); break;
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
			// Once: the ladder shown is built from this plan, and must be the ladder climbed.
			if (Plan[0].Num() > 0)
			{
				return;
			}
			const int32 Seed = (int32)(FDateTime::Now().GetTicks() % 100000);
			for (int32 T = 0; T < 3; ++T)
			{
				Plan[T] = GI->Flow.PlanArcade(Fighter, T, Seed + T);
			}
			Sel = FMath::Clamp(GI->Settings.Difficulty, 0, 2);
		}
		FText RungText(const FRung& R) const
		{
			if (R.Kind == TEXT("boss")) return LOCTEXT("Boss", "The Ebony Heart Fragment");
			if (R.Kind == TEXT("mirror")) return LOCTEXT("Mirror", "The Reflection");
			const FText N = FText::FromString(Def(R.Opponent)->Name);
			return R.Kind == TEXT("rival") ? FText::Format(LOCTEXT("RivalName", "{0} (rival)"), N) : N;
		}
		// One rung: who waits there, in a card a tower's column deep.
		TSharedRef<SWidget> Rung(int32 T, const FRung& R, int32 Number)
		{
			const bool bFace = R.Kind != TEXT("boss");
			const FName Face = R.Kind == TEXT("mirror") ? Fighter : R.Opponent;
			auto Tone = [this, T]() { return FSlateColor(Sel == T ? FLinearColor::White : FLinearColor(0.5f, 0.48f, 0.54f)); };
			return SNew(SBox).HeightOverride(70.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
				[
					SNew(SBox).WidthOverride(26.f).HAlign(HAlign_Right)[SNew(STextBlock).Font(SlopUI::Strong(21.f)).ColorAndOpacity(SlopUI::Faint).Text(FText::AsNumber(Number))]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[Portrait(Face, !bFace, FVector2D(52, 62), TAttribute<FSlateColor>::CreateLambda(Tone))]
					+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity(SlopUI::Line)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(SlopUI::Body(25.f)).AutoWrapText(true).Text(RungText(R)).ColorAndOpacity_Lambda(Tone)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
				]
			];
		}

		FText Record(int32 T) const
		{
			// This fighter's record on the tower, as the original showed under each ladder.
			const int32 Clears = GI->Progress.Clears(Fighter, T);
			if (Clears <= 0)
			{
				return LOCTEXT("NotCleared", "Not yet cleared");
			}
			const double Secs = GI->Progress.BestTime(Fighter, T);
			return FText::Format(LOCTEXT("TowerRecord", "Cleared {0}x   ·   best {1}:{2}   ·   {3} pts"), Clears, (int32)Secs / 60,
				FText::FromString(FString::Printf(TEXT("%02d"), (int32)Secs % 60)), FText::AsNumber(GI->Progress.BestScore(Fighter, T)));
		}

		virtual TSharedRef<SWidget> Build() override
		{
			if (Plan[0].Num() == 0)
			{
				Enter();
			}
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 T = 0; T < 3; ++T)
			{
				TSharedRef<SVerticalBox> Ladder = SNew(SVerticalBox);
				Ladder->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Font(SlopUI::Display(38.f)).Text(FText::Format(LOCTEXT("TowerN", "{0} Tower"), FGameFlow::DifficultyName(T)))
					.ColorAndOpacity_Lambda([this, T]() { return FSlateColor(Sel == T ? SlopUI::Text : SlopUI::Dim); })
				];
				Ladder->AddSlot().AutoHeight().Padding(-8, 8, 0, 14)[SlopUI::Rule(440.f)];
				// Read from the top of the tower down, the boss first, as it is climbed bottom up.
				for (int32 I = Plan[T].Num() - 1; I >= 0; --I)
				{
					Ladder->AddSlot().AutoHeight()[Rung(T, Plan[T][I], I + 1)];
				}
				Ladder->AddSlot().AutoHeight().Padding(0, 14, 0, 0)
				[
					SNew(STextBlock).Font(SlopUI::Body(23.f)).AutoWrapText(true).Text(Record(T))
					.ColorAndOpacity_Lambda([this, T]() { return FSlateColor(Sel == T ? SlopUI::Warn : SlopUI::Faint); })
				];
				Row->AddSlot().AutoWidth().Padding(14, 0)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
					.OnMouseButtonDown_Lambda([this, T](const FGeometry&, const FPointerEvent&) { Sel = T; Choose(); return FReply::Handled(); })
					.OnMouseMove_Lambda([this, T](const FGeometry&, const FPointerEvent&)
					{
						if (Sel != T)
						{
							Sel = T;
							Sound(TEXT("ui_move"));
						}
						return FReply::Unhandled();
					})
					[
						SNew(SBox).WidthOverride(520.f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity_Lambda([this, T]() { return Sel == T ? SlopUI::Raised.CopyWithNewOpacity(0.55f) : SlopUI::Panel.CopyWithNewOpacity(0.6f); })
							]
							+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity_Lambda([this, T]() { return Sel == T ? SlopUI::Spark : SlopUI::Line; })]
							+ SOverlay::Slot().Padding(FMargin(26, 22))[Ladder]
						]
					]
				];
			}
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopCentre, FVector2D(0.f, 44.f),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SlopUI::CapsLabel(Upper(Def(Fighter)->Name), 21.f, SlopUI::Spark)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)[SlopUI::Title(LOCTEXT("ChooseTower", "Choose Your Tower"), 46.f)],
				FVector2D(0.5f, 0.f));
			SlopUI::Place(C, SlopUI::Middle, FVector2D(0.f, 34.f), Row, FVector2D(0.5f, 0.5f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::Sideways, LOCTEXT("TwMove", "Choose")}, {SlopUI::EPrompt::Confirm, LOCTEXT("TwPick", "Climb")}, {SlopUI::EPrompt::Back, LOCTEXT("TwBack", "Back")}}, SlopUI::EVeil::Full);
		}
		void Choose()
		{
			// The climb begins with the fighter's call, as in the original.
			Sound(TEXT("select_fighter"));
			const bool bCos = GI->Flow.Config.bCostume[0];
			GI->Flow.SetNow(GI->Now());
			GI->Flow.StartArcade(Fighter, Sel, Plan[Sel]);
			GI->Flow.Config.bCostume[0] = bCos;
			GI->EnterFight(true);
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			// Left and right choose a tower too, the original's keys (it laid them side by side).
			case EUi::Up:
			case EUi::Left: Sel = Step(Sel, 3, -1); Sound(TEXT("ui_move")); break;
			case EUi::Down:
			case EUi::Right: Sel = Step(Sel, 3, 1); Sound(TEXT("ui_move")); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->Goto(SlopScreens::Select(EMode::Arcade)); break;
			default: break;
			}
		}
	};

	// --- Face-off ------------------------------------------------------------------------------------------

	// The staging before a fight, over the match itself: both fighters are already on their stage,
	// and the match's camera goes from one to the other and then back into its live framing, so there
	// is no cut between the face-off and the fight. This screen only holds what is said over it.
	class FFaceOff : public FSlopScreen
	{
	public:
		virtual bool ShowsMatch() const override { return true; }
		FText Above, AboveSmall;
		FLinearColor AboveColour = SlopUI::Dim;
		float AboveSize = 23.f;
		TArray<FText> Lines;
		bool bCry = false;
		bool bGathered = false;
		// Each piece's opacity, eased toward what the beat wants; the letterbox goes last.
		float Name[2] = {0.f, 0.f};
		float Versus = 0.f, Ground = 0.f, Box = 0.f;
		float Released = 0.f;
		float T = 0.f;

		// What the rung or mode has to say before this fight.
		void Gather()
		{
			if (bGathered)
			{
				return;
			}
			bGathered = true;
			const FMatchConfig& C = GI->Flow.Config;
			const FFighterDef* A = Def(C.P1);
			const FFighterDef* B = Def(C.P2);
			if (const FRung* R = GI->Flow.CurrentRung())
			{
				Above = FText::Format(LOCTEXT("Fight", "FIGHT {0} OF {1}"), GI->Flow.RungIndex() + 1, GI->Flow.Rungs().Num());
				if (R->Lines.Num() > 0)
				{
					for (const FString& L : R->Lines)
					{
						Lines.Add(FText::FromString(L));
					}
					bCry = R->Kind == TEXT("boss");
				}
				else if (R->Kind == TEXT("rival"))
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
				else if (R->Kind == TEXT("servant"))
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("servant_line"))));
				}
				else if (R->Kind == TEXT("mirror"))
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("reflection"))));
				}
				else if (R->Kind == TEXT("boss"))
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("boss_intro"))));
					bCry = true;
				}
				else if (R->Kind == TEXT("secret"))
				{
					// Off the ladder: no fight count, only the old man and what he says.
					Above = FText::FromString(Json::Str(Json::Obj(GetContent().Story, TEXT("secret")), TEXT("above"), TEXT("The still water")));
					AboveColour = SlopUI::Qhi;
					AboveSize = 34.f;
					if (const TArray<TSharedPtr<FJsonValue>>* L = Json::Arr(Json::Obj(GetContent().Story, TEXT("secret")), TEXT("lines")))
					{
						for (const TSharedPtr<FJsonValue>& V : *L)
						{
							Lines.Add(FText::FromString(V->AsString()));
						}
					}
					// He knows how she got here, and only he calls her that.
					if (GI->Flow.InStory() && Lines.Num() > 0)
					{
						Lines.Last() = FText::FromString(Json::Str(Json::Obj(GetContent().Story, TEXT("story")), GI->Flow.ArcadeContinues() > 0 ? TEXT("grandmaster_after_continues") : TEXT("grandmaster_line")));
					}
				}
			}
			else if (C.Mode == EMode::Cpu)
			{
				Above = LOCTEXT("VersusCpuAbove", "VERSUS CPU");
				AboveSmall = FGameFlow::DifficultyName(C.Difficulty);
			}
			if (C.Mode == EMode::Versus || C.Mode == EMode::Cpu)
			{
				// Rounds and clock, when they are not the usual first to two on 90 seconds.
				FString Rule;
				if (C.RoundsToWin != 2)
				{
					Rule = FText::Format(LOCTEXT("FirstTo", "First to {0}"), C.RoundsToWin).ToString();
				}
				if (C.RoundSeconds != 90)
				{
					const FString Clock = C.RoundSeconds > 0 ? FText::Format(LOCTEXT("ClockS", "{0} seconds a round"), C.RoundSeconds).ToString() : LOCTEXT("NoClockRule", "no clock").ToString();
					Rule = Rule.IsEmpty() ? Clock : Rule + TEXT(", ") + Clock;
				}
				if (!Rule.IsEmpty())
				{
					AboveSmall = AboveSmall.IsEmpty() ? FText::FromString(Rule) : FText::FromString(AboveSmall.ToString() + TEXT("   ·   ") + Rule);
				}
			}
			else if (GI->Flow.InSurvival())
			{
				const bool bBoss = C.P2 == TEXT("slopbat");
				Above = FText::Format(LOCTEXT("Wave", "WAVE {0}"), GI->Flow.SurvivalWaves() + 1);
				AboveColour = bBoss ? SlopUI::Danger : SlopUI::Warn;
				AboveSize = 34.f;
				AboveSmall = FGameFlow::DifficultyName(C.Difficulty);
				if (bBoss)
				{
					Lines.Add(FText::FromString(Json::Str(GetContent().Story, TEXT("boss_intro"))));
					bCry = true;
				}
			}
		}

		// A fighter's name and what they are, at the foot of their side of the screen.
		TSharedRef<SWidget> Card(int32 Side)
		{
			const FMatchConfig& C = GI->Flow.Config;
			const FFighterDef* D = Def(Side == 0 ? C.P1 : C.P2);
			const bool bReflection = Side == 1 && C.bMirrorDark;
			const FString Who = bReflection ? LOCTEXT("Reflection", "THE REFLECTION").ToString() : D->Name.ToUpper();
			const FString What = bReflection ? FString() : (D->Title + (D->Style.IsEmpty() ? FString() : TEXT("   ·   ") + D->Style)).ToUpper();
			const ETextJustify::Type Justify = Side == 0 ? ETextJustify::Left : ETextJustify::Right;
			const float Size = Who.Len() <= 12 ? 78.f : (Who.Len() <= 18 ? 60.f : 46.f);
			const float Slide = Side == 0 ? -46.f : 46.f;
			return SNew(SBox).WidthOverride(700.f)
				.RenderTransform_Lambda([this, Side, Slide]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(Slide * (1.f - Name[Side]), 0.f))); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(SlopUI::Call(Size)).Text(FText::FromString(Who)).Justification(Justify).AutoWrapText(true)
						.ColorAndOpacity_Lambda([this, Side]() { return FSlateColor(SlopUI::Text.CopyWithNewOpacity(Name[Side])); })
						.ShadowOffset(FVector2D(2, 3)).ShadowColorAndOpacity_Lambda([this, Side]() { return FLinearColor(0, 0, 0, 0.8f * Name[Side]); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
					[
						SNew(STextBlock).Font(SlopUI::Caps(22.f)).Text(FText::FromString(What)).Justification(Justify).AutoWrapText(true)
						.ColorAndOpacity_Lambda([this, Side]() { return FSlateColor(SlopUI::Spark.CopyWithNewOpacity(Name[Side])); })
						.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity_Lambda([this, Side]() { return FLinearColor(0, 0, 0, 0.8f * Name[Side]); })
					]
				];
		}

		virtual TSharedRef<SWidget> Build() override
		{
			Gather();
			const FMatchConfig& C = GI->Flow.Config;
			TSharedRef<SConstraintCanvas> Cv = SlopUI::Canvas();
			auto Bar = [this](float Dir)
			{
				return SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(SlopUI::Ink)
					.RenderTransform_Lambda([this, Dir]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, Dir * 92.f * (1.f - SlopUI::EaseOut(Box))))); });
			};
			// The letterbox reaches the screen's edges whatever the margin; the words stay inside it.
			TSharedRef<SConstraintCanvas> Bars = SlopUI::Canvas();
			SlopUI::Fill(Bars, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, 88.f), Bar(-1.f));
			SlopUI::Fill(Bars, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(0.f, -88.f, 0.f, 88.f), Bar(1.f));
			TSharedRef<SHorizontalBox> Top = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Above).Font(AboveSize > 30.f ? SlopUI::Display(AboveSize) : SlopUI::Caps(AboveSize))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(AboveColour.CopyWithNewOpacity(Box)); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(AboveSmall.IsEmpty() ? 0 : 26, 0, 0, 0)
				[
					SNew(STextBlock).Text(AboveSmall).Font(SlopUI::Body(23.f)).ColorAndOpacity_Lambda([this]() { return FSlateColor(SlopUI::Dim.CopyWithNewOpacity(Box)); })
				];
			SlopUI::Place(Cv, SlopUI::TopCentre, FVector2D(0.f, 44.f), Top, FVector2D(0.5f, 0.5f));
			// The ground named as the camera finds it.
			SlopUI::Place(Cv, SlopUI::Middle, FVector2D(0.f, 300.f),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(SlopUI::StageName(C.Stage)).Font(SlopUI::Display(44.f))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(SlopUI::Text.CopyWithNewOpacity(Ground)); })
					.ShadowOffset(FVector2D(2, 3)).ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0, 0, 0, 0.85f * Ground); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10, 0, 0)
				[
					SNew(SBox).WidthOverride(640.f).HeightOverride(4.f)[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Rule)).ColorAndOpacity_Lambda([this]() { return SlopUI::Line.CopyWithNewOpacity(Ground); })]
				],
				FVector2D(0.5f, 0.5f));
			// VS only as a quiet mark on the seam between them; the fighters are the composition.
			SlopUI::Place(Cv, SlopUI::Middle, FVector2D(0.f, -30.f),
				SNew(STextBlock).Text(LOCTEXT("VS", "VS")).Font(SlopUI::Call(70.f))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(SlopUI::Text.CopyWithNewOpacity(Versus)); })
				.ShadowOffset(FVector2D(2, 3)).ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0, 0, 0, 0.9f * Versus); }),
				FVector2D(0.5f, 0.5f));
			if (Lines.Num() > 0)
			{
				TSharedRef<SVerticalBox> Said = SNew(SVerticalBox);
				for (const FText& L : Lines)
				{
					Said->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(STextBlock).Font(SlopUI::Body(29.f)).Text(L).AutoWrapText(true).Justification(ETextJustify::Center)
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(SlopUI::Text.CopyWithNewOpacity(Versus)); })
						.ShadowOffset(FVector2D(1, 2)).ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0, 0, 0, 0.95f * Versus); })
					];
				}
				SlopUI::Place(Cv, SlopUI::Middle, FVector2D(0.f, 70.f), SNew(SBox).WidthOverride(900.f)[Said], FVector2D(0.5f, 0.f));
			}
			SlopUI::Place(Cv, SlopUI::FootLeft, FVector2D(84.f, -112.f), Card(0), FVector2D(0.f, 1.f));
			SlopUI::Place(Cv, SlopUI::FootRight, FVector2D(-84.f, -112.f), Card(1), FVector2D(1.f, 1.f));
			SlopUI::Place(Cv, SlopUI::FootRight, FVector2D(-72.f, -24.f), SlopUI::Prompts(GI, {{SlopUI::EPrompt::Confirm, LOCTEXT("FaceSkip", "Skip")}}), FVector2D(1.f, 1.f));
			return SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SlopUI::SStage).FullBleed(true)[Bars]]
				+ SOverlay::Slot()[SNew(SlopUI::SStage)[Cv]];
		}

		virtual void Enter() override
		{
			GI->Audio->Play(TEXT("gong"), -6.f, 0.8f, ESlopBus::Ui);
			if (bCry)
			{
				GI->Audio->Play(TEXT("screech"), -6.f);
			}
			if (ASlopMatch* M = GI->GetMatch())
			{
				M->BeginPrelude(GI->Settings.IntroLength == TEXT("short"), Lines.Num() > 0);
			}
		}

		virtual void Tick(float Dt) override
		{
			ASlopMatch* M = GI->GetMatch();
			if (!M)
			{
				return;
			}
			T += Dt;
			const EPrelude Beat = M->PreludeBeat();
			const auto Ease = [Dt](float& V, bool bOn, float Rate) { V = SlopUI::ReducedMotion() ? (bOn ? 1.f : 0.f) : FMath::FInterpConstantTo(V, bOn ? 1.f : 0.f, Dt, Rate); };
			const bool bHeld = M->InPrelude();
			Ease(Box, bHeld, 3.5f);
			Ease(Ground, Beat == EPrelude::Ground, 3.f);
			Ease(Name[0], Beat == EPrelude::First || Beat == EPrelude::Both, 4.f);
			Ease(Name[1], Beat == EPrelude::Second || Beat == EPrelude::Both, 4.f);
			Ease(Versus, Beat == EPrelude::Both, bHeld ? 4.f : 12.f);
			if (!bHeld && (Released += Dt) >= 0.42f)
			{
				// Last statement: this replaces the screen, and this object with it.
				GI->Goto(SlopScreens::MatchScreen());
			}
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			ASlopMatch* M = GI->GetMatch();
			if (!M || !M->InPrelude() || T < 0.4f)
			{
				return;
			}
			if (E.Action == EUi::Confirm || E.Action == EUi::Back || E.Action == EUi::Pause)
			{
				M->SkipPrelude();
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
			auto Fill = [this](int32 I) { return Sel == I ? SlopUI::Raised : SlopUI::Panel; };
			auto Edge = [this](int32 I) { return Sel == I ? SlopUI::Spark : SlopUI::Line; };
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			for (int32 I = 0; I < Rows.Num(); ++I)
			{
				const int32 D = Rows[I];
				List->AddSlot().AutoHeight().Padding(0, 5)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity_Lambda([Fill, I]() { return Fill(I); })]
					+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity_Lambda([Edge, I]() { return Edge(I); })]
					+ SOverlay::Slot().Padding(FMargin(26, 13))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SlopUI::Label(RowName(D), 28.f, SlopUI::Text)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(320.f).HAlign(HAlign_Right)
							[
								SNew(STextBlock).Font(SlopUI::Display(30.f)).Text_Lambda([this, D]()
								{
									const int32 S = GI->Settings.Assign[D];
									return S == 0 ? LOCTEXT("P1", "◀  Player 1  ▶") : S == 1 ? LOCTEXT("P2", "◀  Player 2  ▶") : LOCTEXT("Nobody", "◀  Nobody  ▶");
								})
								.ColorAndOpacity_Lambda([this, D]() { const int32 S = GI->Settings.Assign[D]; return S == 0 ? SlopUI::Spark : S == 1 ? SlopUI::Second : SlopUI::Faint; })
							]
						]
					]
				];
			}
			const int32 Go = Rows.Num();
			List->AddSlot().AutoHeight().Padding(0, 22, 0, 0)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity_Lambda([Fill, Go]() { return Fill(Go); })]
				+ SOverlay::Slot()[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Frame)).ColorAndOpacity_Lambda([Edge, Go]() { return Edge(Go); })]
				+ SOverlay::Slot().Padding(FMargin(26, 13))
				[
					SNew(STextBlock).Font(SlopUI::Display(29.f)).Text(LOCTEXT("ToSelect", "Continue to Fighter Select"))
					.ColorAndOpacity_Lambda([this]() { return Valid() ? SlopUI::Text : SlopUI::Faint; })
				]
			];
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 72.f), SlopUI::Heading(LOCTEXT("Controllers", "Who Plays with What")));
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 250.f), SNew(SBox).WidthOverride(1300.f)[List]);
			SlopUI::Place(C, SlopUI::FootLeft, FVector2D(130.f, -116.f), SNew(SBox).WidthOverride(1300.f)
				[
					SNew(STextBlock).Font(SlopUI::Body(25.f)).ColorAndOpacity(SlopUI::Warn).AutoWrapText(true)
					.Text_Lambda([this]()
					{
						// A pad connected before the game started is unknown until it sends input, so say how to bring it in.
						const FText Seen = GI->Router->ConnectedPadCount() == 0
							? LOCTEXT("NoPads", "Both keyboard halves are always here. A controller shows up once you press a button on it.") : FText::GetEmpty();
						return Valid() ? Seen : FText::Format(LOCTEXT("Need", "Each player needs at least one device. {0}"),
							GI->Router->ConnectedPadCount() == 0 ? LOCTEXT("NeedPad", "Connect a controller and press a button on it to have one each.") : LOCTEXT("NeedAssign", "Give each player a row."));
					})
				], FVector2D(0.f, 1.f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::UpDown, LOCTEXT("DevRow", "Device")}, {SlopUI::EPrompt::Sideways, LOCTEXT("DevSide", "Give to a player")},
				{SlopUI::EPrompt::Confirm, LOCTEXT("DevGo", "Continue")}, {SlopUI::EPrompt::Back, LOCTEXT("DevBack", "Back")}}, SlopUI::EVeil::Full);
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
			// Confirm on any row continues, as in the original (left and right assign the rows).
			case EUi::Confirm:
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
			auto Frames = [D](const FMoveDef& M)
			{
				// A parry lists what it catches and when; anything else that strikes nothing (a
				// buff, a step) only how long it takes.
				if (M.bParry)
				{
					TArray<FString> Lv;
					for (ELevel L : {ELevel::High, ELevel::Mid, ELevel::Low, ELevel::Overhead})
					{
						if (M.Parries(L))
						{
							Lv.Add(LevelName(L));
						}
					}
					// A catch whose answer is an instant kill says what it needs.
					const FMoveDef* Answer = D->Move(M.ParryCounter);
					const FString Kill = Answer && Answer->bKill ? FString::Printf(TEXT("   with %d Qhi the catch becomes %s, an instant kill"), FMath::RoundToInt(Answer->QhiCost), *Answer->Name) : FString();
					return FString::Printf(TEXT("parry   catches %s on frames %d-%d   total %d%s%s"), *FString::Join(Lv, TEXT(", ")), M.ParryFrames.X, M.ParryFrames.Y, M.Total,
						M.ParryDamage > 0 ? *FString::Printf(TEXT("   stings %d"), M.ParryDamage) : TEXT(""), *Kill);
				}
				// A projectile's advantage depends on how far it travels, so only what it is.
				if (M.Projectile.bHas)
				{
					return FString::Printf(TEXT("%s projectile   %d dmg   leaves on frame %d   total %d"), LevelName(M.Projectile.Level), M.Damage + M.Projectile.Damage, M.Projectile.Frame, M.Total);
				}
				// His changes of shape: when each takes (from a standstill, and inside a combo) and its price.
				if (M.bShapeshift)
				{
					const FQhiRules& Q = D->Qhi;
					if (M.FormTarget.IsNone())
					{
						return FString::Printf(TEXT("takes on frame %d (%d inside a combo)   costs half the Qhi   Qhi-enhanced"), M.FormFrame, Q.ShiftFrame);
					}
					const FFighterDef* T = D->FormSources.FindRef(M.FormTarget);
					const double Upkeep = M.FormUpkeep * (T ? T->ShapeUpkeep : 1.0);
					return FString::Printf(TEXT("takes on frame %d (%d inside a combo)   costs %d Qhi%s%s"), M.FormFrame, Q.ShiftFrame, FMath::RoundToInt(M.FormCost),
						M.bFormEnhanced ? TEXT("   Qhi-enhanced") : TEXT(""), Upkeep != 1.0 ? *FString::Printf(TEXT("   upkeep x%.0f"), Upkeep) : TEXT(""));
				}
				if (M.bRevert)
				{
					return FString::Printf(TEXT("his own face back on frame %d (%d inside a combo)   no Qhi returned"), M.FormFrame, D->Qhi.ShiftFrame);
				}
				if (M.bKill)
				{
					const FString Gate = M.KillRequires == TEXT("standing") ? FString(TEXT("only begun against a standing opponent; a blow in its startup breaks it"))
						: M.KillRequires == TEXT("combo") ? FString::Printf(TEXT("only to end a combo of %d+ hits through two of his shapes; spends all his Qhi"), M.KillCombo)
						: FString();
					return FString::Printf(TEXT("INSTANT KILL   startup %d   %s"), M.Startup, *Gate);
				}
				if (!M.IsAttack() && !M.Throw.bHas)
				{
					FString S = FString::Printf(TEXT("total %d frames"), M.Total);
					if (M.Feint != FIntPoint::ZeroValue)
					{
						S += FString::Printf(TEXT("   cancels into anything on frames %d-%d"), M.Feint.X, M.Feint.Y);
					}
					if (M.Ward != FIntPoint::ZeroValue)
					{
						S += FString::Printf(TEXT("   breaks projectiles on frames %d-%d"), M.Ward.X, M.Ward.Y);
					}
					return S;
				}
				const int32 Hit = M.HitAdvantage();
				return FString::Printf(TEXT("%s   %d dmg   startup %d  active %d  recovery %d   on block %+d   on hit %s%s"),
					LevelName(M.Level), M.Damage + (M.Throw.bHas ? M.Throw.Damage : 0) + (M.Projectile.bHas ? M.Projectile.Damage : 0), M.Startup, M.Active, M.Recovery,
					M.BlockAdvantage(), Hit >= 999 ? TEXT("knockdown") : *FString::Printf(TEXT("%+d"), Hit),
					M.SelfCancels > 0 ? *FString::Printf(TEXT("   up to %d in a row"), M.SelfCancels + 1) : TEXT(""));
			};
			auto Section = [this](const FText& T) { Rows.Add(TEXT("#") + T.ToString()); };
			auto Cost = [](const FMoveDef& M) { return M.QhiCost > 0.0 ? FString::Printf(TEXT("   Qhi %d"), FMath::RoundToInt(M.QhiCost)) : FString(); };
			const bool bQhi = D->Resource == EResource::Qhi;
			if (bQhi)
			{
				// His meter's rules, as the owner set them: the move list is where a player learns them.
				const FQhiRules& Q = D->Qhi;
				Section(LOCTEXT("QhiRules", "QHI (his meter: he has no run meter of his own)"));
				Rows.Add(FString::Printf(TEXT("Glide|%s|floats forward in place of running, draining Qhi"), *FCommand::Pretty(TEXT("F+RN"))));
				Rows.Add(TEXT("Borrowed Face|") + FCommand::Pretty(D->NotationOf(TEXT("borrowed_face"))) + FString::Printf(TEXT("|becomes the opponent facing him, stronger by Qhi (%d%% damage, %d%% taken, %d%% speed) for half the Qhi; the shape has a run meter and burns Qhi while worn and with every blow it takes; the same input gives it back"),
					FMath::RoundToInt(Q.CopyPower * 100), FMath::RoundToInt(Q.CopyGuard * 100), FMath::RoundToInt(Q.CopySpeed * 100)));
				Rows.Add(TEXT("Shapes|") + FCommand::Pretty(TEXT("RN+LP")) + TEXT(" ...|from his form library: every fighter's shape (Run or Down + Run with a button; listed below), remembered, not enhanced"));
				Rows.Add(FString::Printf(TEXT("Inside a combo|—|once a blow of his lands, a change of shape takes on frame %d, at most %d in one combo, for its own price"), Q.ShiftFrame, Q.MaxShifts));
				Rows.Add(FString::Printf(TEXT("Spent|—|an empty meter leaves him boxing: a weak, careful guard with slips and parries, slow and easily hurt, until %d Qhi has crept back"), FMath::RoundToInt(Q.RecoverAt)));
				Rows.Add(FString::Printf(TEXT("Defence|—|blows hurt him %d%% more, magic %d%% less; a caught blow gives back %d Qhi, once every %d seconds"),
					FMath::RoundToInt((D->PhysDef - 1.0) * 100), FMath::RoundToInt((1.0 - D->MagicDef) * 100), FMath::RoundToInt(Q.DefendGain), Q.DefendGainEvery / 60));
			}
			Section(LOCTEXT("Specials", "SPECIALS"));
			for (const FCommand& C : D->Commands)
			{
				const FMoveDef* M = D->Move(C.Move);
				if (M && M->Kind == EMoveKind::Special)
				{
					Rows.Add(FString::Printf(TEXT("%s|%s|%s%s"), *M->Name, *FCommand::Pretty(C.Text), *Frames(*M), *Cost(*M)));
				}
			}
			if (bQhi && D->SpentForm)
			{
				Section(LOCTEXT("WhenSpent", "WHEN SPENT (boxing)"));
				for (const FCommand& C : D->SpentForm->Commands)
				{
					const FMoveDef* M = D->SpentForm->Move(C.Move);
					if (M && M->Kind == EMoveKind::Special)
					{
						Rows.Add(FString::Printf(TEXT("%s|%s|%s"), *M->Name, *FCommand::Pretty(C.Text), *Frames(*M)));
					}
				}
			}
			Section(bQhi ? LOCTEXT("CombosQhi", "DIAL-A-COMBOS (each link spends a little Qhi)") : LOCTEXT("Combos", "DIAL-A-COMBOS (each link spends run meter)"));
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
				Section(LOCTEXT("Finisher", "HEARTED (after the deciding knockout)"));
				const FMoveDef* M = D->Move(D->FinisherCmd.Move);
				Rows.Add(FString::Printf(TEXT("%s|%s|%s range"), M ? *M->Name : TEXT("?"), *FCommand::Pretty(D->FinisherCmd.Text), *D->FinisherRange.ToString()));
				// The stage's own, where there is one.
				if (const FMoveDef* Vat = D->Move(TEXT("stage_vat")))
				{
					Rows.Add(FString::Printf(TEXT("%s|%s|close, at the Rendering Yards"), *Vat->Name, *FCommand::Pretty(Vat->StageInput)));
				}
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
			Rows.Add(LOCTEXT("RunRow", "Run|Hold RN + →|empties the blue meter; it refills when you stop").ToString());
			Section(LOCTEXT("Rules", "RULES"));
			Rows.Add(LOCTEXT("R0", "→ is toward the opponent, ← away, whichever side you are on.||").ToString());
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
					List->AddSlot().AutoHeight().Padding(0, 14, 0, 4)[SlopUI::CapsLabel(FText::FromString(R.Mid(1)), 23.f, SlopUI::Spark)];
					continue;
				}
				TArray<FString> P;
				R.ParseIntoArray(P, TEXT("|"), false);
				P.SetNum(3);
				List->AddSlot().AutoHeight().Padding(0, 3)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.34f).Padding(0, 0, 16, 0)[SNew(STextBlock).Font(SlopUI::Strong(25.f)).ColorAndOpacity(SlopUI::Text).Text(FText::FromString(P[0])).AutoWrapText(true)]
					+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(0, 0, 16, 0)[SNew(STextBlock).Font(SlopUI::Strong(25.f)).ColorAndOpacity(SlopUI::Warn).Text(FText::FromString(SlopUI::ButtonsIn(GI, P[1]))).AutoWrapText(true)]
					+ SHorizontalBox::Slot().FillWidth(0.46f)[SNew(STextBlock).Font(SlopUI::Body(22.f)).ColorAndOpacity(SlopUI::Dim).Text(FText::FromString(P[2])).AutoWrapText(true)]
				];
			}
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 50.f), SlopUI::Heading(FText::Format(LOCTEXT("MovesOf", "{0}"), FText::FromString(Def(Fighter)->Name)), LOCTEXT("MovesSmall", "MOVES"), 50.f));
			// Clipped: rows that wrap make the page taller than its count, and must not run under the prompts.
			SlopUI::Place(C, SlopUI::TopCentre, FVector2D(0.f, 180.f), SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)[List], FVector2D(0.5f, 0.f), FVector2D(1660.f, 800.f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::UpDown, LOCTEXT("MovesScroll", "Scroll")}, {SlopUI::EPrompt::Sideways, LOCTEXT("MovesOther", "Other fighter")}, {SlopUI::EPrompt::Back, LOCTEXT("MovesBack", "Back")}}, SlopUI::EVeil::Full);
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			const TArray<FName> R = GI->Playable();
			switch (E.Action)
			{
			case EUi::Up: Scroll = FMath::Max(0, Scroll - 1); GI->RebuildScreen(); break;
			// Far enough that the last rows always come into view, however many lines the page's rows wrap to.
			case EUi::Down: Scroll = FMath::Min(FMath::Max(0, Rows.Num() - 8), Scroll + 1); GI->RebuildScreen(); break;
			case EUi::Left:
			case EUi::Right:
				if (!bOverlay)
				{
					const int32 I = FMath::Max(0, R.IndexOfByKey(Fighter));
					Fighter = R[(I + (E.Action == EUi::Right ? 1 : R.Num() - 1)) % R.Num()];
				}
				else if (const ASlopMatch* Mt = GI->GetMatch())
				{
					// Paused, between the two in the fight: your moves or the opponent's.
					Fighter = Fighter == Mt->Config.P1 ? Mt->Config.P2 : Mt->Config.P1;
				}
				Rows.Reset();
				Scroll = 0;
				Sound(TEXT("ui_move"));
				GI->RebuildScreen();
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
			for (const FName& Id : GI->Playable())
			{
				if (GI->Progress.EndingSeen(Id))
				{
					Seen.Add(Id);
				}
			}
			Sel = FMath::Clamp(GI->ExtrasChoice, 0, 5);
		}
		// The fourth item is the secret fighter's: a clue until he is unlocked, then his move list.
		bool SecretOpen() const { return GI->IsUnlocked(FGameFlow::SecretFighter()); }
		TArray<FText> Labels() const
		{
			return {LOCTEXT("Endings", "Endings"), LOCTEXT("Records", "Records"), LOCTEXT("MoveLists", "Move Lists"),
				SecretOpen() ? FText::FromString(Def(FGameFlow::SecretFighter())->Name) : LOCTEXT("Unknown", "???"), LOCTEXT("Credits", "Credits"), LOCTEXT("Back", "Back")};
		}
		// The records as a table: one row a fighter, a column for each thing they can do.
		TSharedRef<SWidget> Records() const
		{
			TSharedRef<SVerticalBox> V = SNew(SVerticalBox);
			V->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
			[
				SNew(STextBlock).Font(SlopUI::Strong(25.f)).ColorAndOpacity(SlopUI::Text).Text(FText::FromString(FString::Printf(TEXT("Matches %d   ·   Wins %d   ·   Hearted %d   ·   Flawless %d"),
					GI->Progress.Stat(TEXT("matches")), GI->Progress.Stat(TEXT("wins")), GI->Progress.Stat(TEXT("finishers")), GI->Progress.Stat(TEXT("flawless")))))
			];
			auto Row = [&V](TArray<FString> Cells, bool bHead)
			{
				TSharedRef<SHorizontalBox> H = SNew(SHorizontalBox);
				static const float Share[] = {0.22f, 0.22f, 0.22f, 0.22f, 0.06f, 0.06f};
				for (int32 C = 0; C < Cells.Num(); ++C)
				{
					H->AddSlot().FillWidth(Share[C]).Padding(0, 0, 10, 0)
					[
						SNew(STextBlock).Text(FText::FromString(Cells[C])).Font(bHead ? SlopUI::Caps(17.f) : (C == 0 ? SlopUI::Strong(22.f) : SlopUI::Body(21.f))).ColorAndOpacity(bHead ? SlopUI::Spark : (C == 0 ? SlopUI::Text : SlopUI::Dim))
					];
				}
				V->AddSlot().AutoHeight().Padding(0, 3)[H];
			};
			Row({TEXT("FIGHTER"), FGameFlow::DifficultyName(0).ToString().ToUpper(), FGameFlow::DifficultyName(1).ToString().ToUpper(), FGameFlow::DifficultyName(2).ToString().ToUpper(), TEXT("WAVES"), TEXT("TRIALS")}, true);
			int32 TrialsDone = 0, TrialsAll = 0;
			TArray<FString> Colours;
			for (const FName& Id : GI->Playable())
			{
				const FFighterDef* D = Def(Id);
				TArray<FString> Cells = {D->Name};
				// Each tower: times cleared, the best time and score.
				for (int32 T = 0; T < 3; ++T)
				{
					const int32 Best = GI->Progress.BestScore(Id, T);
					const double Secs = GI->Progress.BestTime(Id, T);
					const int32 Clears = GI->Progress.Clears(Id, T);
					Cells.Add(Clears > 0 ? FString::Printf(TEXT("%dx, %d:%02d, %s"), Clears, (int32)Secs / 60, (int32)Secs % 60, *FText::AsNumber(Best).ToString()) : FString(TEXT("—")));
				}
				const int32 Done = GI->Progress.TrialsDone(Id).Num();
				const TArray<TSharedPtr<FJsonValue>>* List = Json::Arr(GetContent().Trials, *Id.ToString());
				const int32 All = List ? List->Num() : 0;
				TrialsDone += Done;
				TrialsAll += All;
				Cells.Add(FString::FromInt(GI->Progress.BestSurvival(Id)));
				Cells.Add(FString::Printf(TEXT("%d/%d"), Done, All));
				Row(Cells, false);
				if (GI->Progress.IsUnlocked(TEXT("costume:") + Id.ToString()))
				{
					Colours.Add(D->Name);
				}
			}
			FString S = FString::Printf(TEXT("Trials cleared: %d of %d"), TrialsDone, TrialsAll);
			S += FString::Printf(TEXT("\nAlternate colours unlocked: %s"), Colours.Num() > 0 ? *FString::Join(Colours, TEXT(", ")) : TEXT("none yet (clear arcade with a fighter)"));
			S += FString::Printf(TEXT("\nThe Heartwater as a versus stage: %s"), GI->Progress.IsUnlocked(TEXT("arena:roost")) ? TEXT("unlocked") : TEXT("clear arcade once"));
			V->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[SNew(STextBlock).Font(SlopUI::Body(23.f)).ColorAndOpacity(SlopUI::Dim).AutoWrapText(true).Text(FText::FromString(S))];
			return V;
		}

		virtual TSharedRef<SWidget> Build() override
		{
			if (Seen.Num() == 0)
			{
				Enter();
			}
			SlopUI::FMenuStyle Style;
			Style.Width = 400.f;
			Style.Size = 32.f;
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 72.f), SlopUI::Heading(LOCTEXT("ExtrasT", "Extras")));
			SlopUI::Place(C, SlopUI::TopLeft, FVector2D(130.f, 270.f), SlopUI::Menu(Labels(), [this]() { return Sel; }, nullptr, [this](int32 I) { Sel = I; Choose(); }, [this](int32 I) { Sel = I; Sound(TEXT("ui_move")); }, Style));
			SlopUI::Place(C, SlopUI::TopRight, FVector2D(-110.f, 270.f), SlopUI::PanelBox(SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true)
					.Visibility_Lambda([this]() { return Sel == 1 ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.Text_Lambda([this]()
					{
						switch (Sel)
						{
						case 0:
						{
							// Every fighter: the endings seen to choose from, the rest saying how to earn them.
							FString S = Seen.Num() > 0 ? LOCTEXT("EndingsSeen", "Endings (left and right to choose, confirm to watch):\n\n").ToString()
								: LOCTEXT("NoEndings", "Clear an arcade tower to see a fighter's ending.\n\n").ToString();
							for (const FName& Id : GI->Playable())
							{
								const FFighterDef* D = Def(Id);
								const int32 I = Seen.IndexOfByKey(Id);
								FString First, Rest;
								if (D->Name.StartsWith(TEXT("The ")) || !D->Name.Split(TEXT(" "), &First, &Rest))
								{
									First = D->Name;
								}
								S += I != INDEX_NONE ? (I == Page ? TEXT("▶ ") : TEXT("   ")) + D->Name + TEXT("\n")
									: FString::Printf(TEXT("   %s: clear any arcade tower with %s\n"), *D->Name, *First);
							}
							return FText::FromString(S);
						}
						case 2: return LOCTEXT("MovesD", "Every fighter's moves, combos and frame data.");
						case 3: return SecretOpen() ? LOCTEXT("SecretD", "Unlocked: choose him on any select screen. Confirm for his moves, his Qhi and his shapes.")
							: FText::FromString(Json::Str(Json::Obj(GetContent().Story, TEXT("secret")), TEXT("hint")));
						case 4: return LOCTEXT("CreditsD", "Who made this, and with what.");
						default: return FText::GetEmpty();
						}
					})
				]
				+ SOverlay::Slot()
				[
					SNew(SBox).Visibility_Lambda([this]() { return Sel == 1 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })[Records()]
				]), FVector2D(1.f, 0.f), FVector2D(1240.f, 660.f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::UpDown, LOCTEXT("ExMove", "Choose")}, {SlopUI::EPrompt::Confirm, LOCTEXT("ExPick", "Select")}, {SlopUI::EPrompt::Back, LOCTEXT("ExBack", "Back")}}, SlopUI::EVeil::Full);
		}
		void Choose()
		{
			Sound(Sel == 3 && !SecretOpen() ? TEXT("ui_locked") : TEXT("ui_select"));
			GI->ExtrasChoice = Sel == 5 ? 0 : Sel;
			switch (Sel)
			{
			case 0: if (Seen.IsValidIndex(Page)) GI->Goto(SlopScreens::Ending(Seen[Page], false)); break;
			case 2: GI->Goto(SlopScreens::MoveList(FContent::Roster()[0], false)); break;
			case 3:
				if (SecretOpen())
				{
					GI->Goto(SlopScreens::MoveList(FGameFlow::SecretFighter(), false));
				}
				break;
			case 4: GI->Goto(SlopScreens::Credits(true)); break;
			case 5: GI->Goto(SlopScreens::MainMenu()); break;
			default: break;
			}
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			switch (E.Action)
			{
			// "???" can be chosen before the secret fighter is unlocked: it gives a clue.
			case EUi::Up: Sel = Step(Sel, 6, -1, [](int32) { return true; }); Sound(TEXT("ui_move")); break;
			case EUi::Down: Sel = Step(Sel, 6, 1, [](int32) { return true; }); Sound(TEXT("ui_move")); break;
			case EUi::Left: if (Sel == 0 && Seen.Num()) Page = (Page + Seen.Num() - 1) % Seen.Num(); break;
			case EUi::Right: if (Sel == 0 && Seen.Num()) Page = (Page + 1) % Seen.Num(); break;
			case EUi::Confirm: Choose(); break;
			case EUi::Back: Sound(TEXT("ui_back")); GI->ExtrasChoice = 0; GI->Goto(SlopScreens::MainMenu()); break;
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
		// Read before the first build, which shows the first page.
		void LoadPages()
		{
			if (Pages.Num() > 0)
			{
				return;
			}
			const bool bStoryEnd = bFromArcade && GI->Flow.InStory();
			if (const TArray<TSharedPtr<FJsonValue>>* E = bStoryEnd ? Json::Arr(Json::Obj(GetContent().Story, TEXT("story")), TEXT("ending")) : Json::Arr(StoryOf(Fighter), TEXT("ending")))
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
		virtual void Enter() override
		{
			GI->Audio->PlayMusic(TEXT("music_ending"));
			if (bFromArcade && GI->Flow.InStory())
			{
				GI->Progress.Unlock(TEXT("story:cleared"));
			}
			// The champion in the light on the left, celebrating, and the story beside them, as
			// the original had it (the port had shown the text alone).
			if (ASlopShowroom* Room = GI->OpenShowroom())
			{
				Room->SetLayout(450.0, 2600.0, 1100.0, 900.0);
				Room->Show(0, Fighter, false);
				Room->Show(1, NAME_None, false);
				Room->Celebrate(0);
			}
		}
		virtual void Tick(float Dt) override { T += Dt; }
		double T = 0.0;
		double PageAt = 0.0;
		virtual TSharedRef<SWidget> Build() override
		{
			LoadPages();
			// Each page joins the ones before it, fading in.
			TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
			for (int32 I = 0; I <= FMath::Min(Page, Pages.Num() - 1); ++I)
			{
				const bool bNew = I == Page;
				Lines->AddSlot().AutoHeight().Padding(0, 0, 0, 28)
				[
					SNew(STextBlock).Font(SlopUI::Body(32.f)).AutoWrapText(true).Text(FText::FromString(Pages[I])).LineHeightPercentage(1.12f)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.85f))
					.ColorAndOpacity_Lambda([this, bNew]()
					{
						FLinearColor C = SlopUI::Text;
						C.A = bNew ? (float)FMath::Clamp((T - PageAt) / 0.8, 0.0, 1.0) : 1.f;
						return FSlateColor(C);
					})
				];
			}
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Fill(C, FAnchors(0.42f, 0.f, 1.f, 1.f), FMargin(0.f), SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::FadeSide)).RenderTransformPivot(FVector2D(0.5f, 0.5f)).RenderTransform(FSlateRenderTransform(FScale2f(-1.f, 1.f))));
			SlopUI::Place(C, SlopUI::TopRight, FVector2D(-120.f, 120.f), SlopUI::Heading(FText::FromString(Def(Fighter)->Name), LOCTEXT("EndingSmall", "ENDING"), 62.f), FVector2D(1.f, 0.f), FVector2D(860.f, 150.f));
			SlopUI::Place(C, SlopUI::TopRight, FVector2D(-120.f, 310.f), Lines, FVector2D(1.f, 0.f), FVector2D(860.f, 640.f));
			return SlopUI::Shell(GI, C, {{SlopUI::EPrompt::Confirm, LOCTEXT("EndGo", "Go on")}}, SlopUI::EVeil::Foot);
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Confirm)
			{
				Sound(TEXT("ui_select"));
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
				else
				{
					PageAt = T;
					GI->RebuildScreen();
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
		bool bFromExtras = false;
		explicit FCredits(bool bE) : bFromExtras(bE) {}
		virtual TSharedRef<SWidget> Build() override
		{
			const FText Body = LOCTEXT("CreditsBody",
				"A game by Markus Gillyard\n\n"
				"Design, characters and story: Markus Gillyard.\n"
				"Programming, animation, stages and synthesised sound: made for this game.\n\n"
				"Bodies: MakeHuman (makehumancommunity.org) base mesh, skins, eyes and hair, CC0, made with MPFB for Blender.\n"
				"Surfaces: ambientCG (ambientcg.com) and Poly Haven (polyhaven.com), CC0.\n"
				"Props and skies: Poly Haven, CC0.   Sound effects: Kenney (kenney.nl), CC0.\n"
				"Motion capture: Open Motion Project by ACCAD / The Ohio State University, CC BY 3.0 (creativecommons.org/licenses/by/3.0); retargeted and retimed for this game.\n"
				"Typefaces: Cinzel by The Cinzel Project Authors; Source Sans 3 by Adobe; SIL Open Font License 1.1. "
				"Symbols: Droid Sans Fallback, The Android Open Source Project, Apache License 2.0.\n\n"
				"Ebony Hearts uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.\n"
				"Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.\n"
				"Full licence texts ship with the game in the Licenses folder.");
			TSharedRef<SConstraintCanvas> C = SlopUI::Canvas();
			SlopUI::Place(C, SlopUI::TopCentre, FVector2D(0.f, 56.f), SNew(SBox).WidthOverride(620.f).HeightOverride(206.f)[SNew(SImage).Image(SlopUI::Brush(SlopUI::ETex::Logo))], FVector2D(0.5f, 0.f));
			SlopUI::Place(C, SlopUI::TopCentre, FVector2D(0.f, 278.f),
				SNew(SBox).WidthOverride(1500.f)[SNew(STextBlock).Font(SlopUI::Body(25.f)).ColorAndOpacity(SlopUI::Text).AutoWrapText(true).Justification(ETextJustify::Center).Text(Body)], FVector2D(0.5f, 0.f));
			return SlopUI::Shell(GI, C, BackOnly(), SlopUI::EVeil::Full);
		}
		virtual void OnUi(const FUiEvent& E) override
		{
			if (E.Action == EUi::Back || E.Action == EUi::Confirm)
			{
				Sound(TEXT("ui_back"));
				GI->Goto(bFromExtras ? SlopScreens::Extras() : SlopScreens::MainMenu());
			}
		}
	};
}

using namespace SlopScreensFlowImpl;

namespace SlopScreens
{
	TSharedPtr<FSlopScreen> Select(EMode Mode) { return Named(MakeShared<FSelect>(Mode), TEXT("Select")); }
	TSharedPtr<FSlopScreen> Stage() { return Named(MakeShared<FStage>(), TEXT("Stage")); }
	TSharedPtr<FSlopScreen> Tower(FName Fighter) { return Named(MakeShared<FTower>(Fighter), TEXT("Tower")); }
	TSharedPtr<FSlopScreen> FaceOff() { return Named(MakeShared<FFaceOff>(), TEXT("FaceOff")); }
	TSharedPtr<FSlopScreen> Devices() { return Named(MakeShared<FDevices>(), TEXT("Devices")); }
	TSharedPtr<FSlopScreen> MoveList(FName Fighter, bool bOverlay) { return Named(MakeShared<FMoveList>(Fighter, bOverlay), TEXT("MoveList")); }
	TSharedPtr<FSlopScreen> Extras() { return Named(MakeShared<FExtras>(), TEXT("Extras")); }
	TSharedPtr<FSlopScreen> Ending(FName Fighter, bool bFromArcade) { return Named(MakeShared<FEnding>(Fighter, bFromArcade), TEXT("Ending")); }
	TSharedPtr<FSlopScreen> Credits(bool bFromExtras) { return Named(MakeShared<FCredits>(bFromExtras), TEXT("Credits")); }
	TSharedPtr<FSlopScreen> TrialsPick() { return Named(MakeShared<FSelect>(EMode::Trials), TEXT("Select")); }
}

#undef LOCTEXT_NAMESPACE
