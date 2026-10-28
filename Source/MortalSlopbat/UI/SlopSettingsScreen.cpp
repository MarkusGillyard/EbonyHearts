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

#define LOCTEXT_NAMESPACE "SlopSettings"

using namespace Slop;

namespace SlopSettingsImpl
{
	// One adjustable line of the settings screen.
	struct FRow
	{
		FText Label;
		// The value as shown; empty for actions.
		TFunction<FText()> Value;
		// Left (-1) or right (+1) changes the value; returns true when it changed.
		TFunction<bool(int32)> Change;
		// Confirm on the row (actions, rebinding).
		TFunction<void()> Activate;
		bool bDisplay = false;
		bool bHeader = false;
	};

	class FSettingsScreen : public FSlopScreen
	{
	public:
		bool bFromPause;
		int32 Tab = 0;
		int32 Sel = 0;
		// Erasing progress takes a second press within a few seconds.
		double EraseArmedAt = -100.0;
		TArray<FRow> Rows;
		// Rebinding: which half/pad and action is waiting for a key.
		int32 CaptureHalf = -1;
		EGameAction CaptureAction = EGameAction::HP;
		FText CaptureMessage;
		// A display change waiting to be kept or reverted.
		bool bPendingDisplay = false;
		double RevertAt = 0;
		FSlopSettings Before;

		explicit FSettingsScreen(bool bP) : bFromPause(bP) {}
		virtual bool ShowsMatch() const override { return bFromPause; }

		static FText OnOff(bool b) { return b ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); }
		static FText Percent(double V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100))); }

		FRow Toggle(const FText& L, bool* V, bool bApply = false)
		{
			FRow R;
			R.Label = L;
			R.Value = [V]() { return OnOff(*V); };
			R.Change = [this, V, bApply](int32) { *V = !*V; Changed(bApply); return true; };
			R.Activate = [this, V, bApply]() { *V = !*V; Changed(bApply); };
			return R;
		}

		FRow Slider(const FText& L, double* V, double Lo = 0.0, double Hi = 1.0, double StepV = 0.1)
		{
			FRow R;
			R.Label = L;
			R.Value = [V]() { return Percent(*V); };
			R.Change = [this, V, Lo, Hi, StepV](int32 D)
			{
				const double N = FMath::Clamp(FMath::RoundToDouble((*V + D * StepV) * 100.0) / 100.0, Lo, Hi);
				if (N == *V)
				{
					return false;
				}
				*V = N;
				Changed(false);
				GI->Audio->Play(TEXT("ui_move"), -4.f, 1.f, ESlopBus::Effects);
				return true;
			};
			// Confirm steps up and wraps from the top back to the bottom, as in the original, so a
			// slider can be set with one button.
			R.Activate = [this, V, Lo, Hi, StepV]()
			{
				*V = *V >= Hi - 1e-6 ? Lo : FMath::Clamp(FMath::RoundToDouble((*V + StepV) * 100.0) / 100.0, Lo, Hi);
				Changed(false);
			};
			return R;
		}

		FRow Choice(const FText& L, TArray<FText> Names, TFunction<int32()> Get, TFunction<void(int32)> Set, bool bDisplay = false)
		{
			FRow R;
			R.Label = L;
			R.bDisplay = bDisplay;
			R.Value = [Names, Get]() { return Names.IsValidIndex(Get()) ? Names[Get()] : FText::GetEmpty(); };
			R.Change = [this, Names, Get, Set, bDisplay](int32 D)
			{
				Set((Get() + D + Names.Num()) % Names.Num());
				Changed(bDisplay);
				return true;
			};
			return R;
		}

		FRow Action(const FText& L, TFunction<void()> A, TFunction<FText()> V = nullptr)
		{
			FRow R;
			R.Label = L;
			R.Activate = MoveTemp(A);
			R.Value = V ? V : TFunction<FText()>([]() { return FText::GetEmpty(); });
			return R;
		}

		FRow Header(const FText& L)
		{
			FRow R;
			R.Label = L;
			R.bHeader = true;
			return R;
		}

		void Changed(bool bDisplay)
		{
			FSlopSettings& S = GI->Settings;
			if (bDisplay)
			{
				// A display change must be confirmed, or it reverts by itself.
				if (!bPendingDisplay)
				{
					bPendingDisplay = true;
				}
				RevertAt = GI->Now() + 15.0;
				GI->ApplySettings(true);
			}
			else
			{
				GI->ApplySettings(false);
				S.Save();
			}
			if (ASlopMatch* M = GI->GetMatch())
			{
				(void)M;
			}
		}

		void KeepDisplay()
		{
			bPendingDisplay = false;
			Before = GI->Settings;
			GI->Settings.Save();
		}

		void RevertDisplay()
		{
			FSlopSettings& S = GI->Settings;
			S.WindowMode = Before.WindowMode;
			S.Resolution = Before.Resolution;
			S.bVSync = Before.bVSync;
			S.FrameCap = Before.FrameCap;
			S.bDynamicRes = Before.bDynamicRes;
			S.Graphics = Before.Graphics;
			bPendingDisplay = false;
			GI->ApplySettings(true);
			GI->Notice(LOCTEXT("Reverted", "Display settings reverted"));
		}

		void BuildRows()
		{
			Rows.Reset();
			FSlopSettings& S = GI->Settings;
			switch (Tab)
			{
			case 0:
			{
				Rows.Add(Choice(LOCTEXT("Window", "Window"), {LOCTEXT("Windowed", "Window"), LOCTEXT("Borderless", "Full screen (borderless)"), LOCTEXT("Exclusive", "Full screen (exclusive)")},
					[&S]() { return S.WindowMode; }, [&S](int32 V) { S.WindowMode = V; }, true));
				static const FIntPoint Res[] = {{1280, 720}, {1600, 900}, {1920, 1080}, {1920, 1200}, {2560, 1440}, {3440, 1440}, {3840, 2160}};
				TArray<FText> Names;
				for (const FIntPoint& P : Res)
				{
					Names.Add(FText::FromString(FString::Printf(TEXT("%d × %d"), P.X, P.Y)));
				}
				Rows.Add(Choice(LOCTEXT("Resolution", "Resolution (window and exclusive)"), Names, [&S]()
				{
					for (int32 I = 0; I < UE_ARRAY_COUNT(Res); ++I)
					{
						if (Res[I] == S.Resolution) return I;
					}
					return 2;
				}, [&S](int32 V) { S.Resolution = Res[V]; }, true));
				Rows.Add(Toggle(LOCTEXT("VSync", "VSync"), &S.bVSync, true));
				Rows.Last().bDisplay = true;
				static const int32 Caps[] = {-1, 30, 60, 120, 144, 0};
				Rows.Add(Choice(LOCTEXT("Cap", "Frame rate cap"), {LOCTEXT("CDisplay", "Match the display"), LOCTEXT("C30", "30"), LOCTEXT("C60", "60"), LOCTEXT("C120", "120"), LOCTEXT("C144", "144"), LOCTEXT("None", "None")},
					[&S]() { for (int32 I = 0; I < UE_ARRAY_COUNT(Caps); ++I) { if (Caps[I] == S.FrameCap) return I; } return 0; }, [&S](int32 V) { S.FrameCap = Caps[V]; }, true));
				Rows.Add(Choice(LOCTEXT("RenderRes", "Render resolution"), {LOCTEXT("ResDynamic", "Dynamic (holds the frame rate)"), LOCTEXT("ResFull", "Full")},
					[&S]() { return S.bDynamicRes ? 0 : 1; }, [&S](int32 V) { S.bDynamicRes = V == 0; }, true));
				Rows.Add(Choice(LOCTEXT("Graphics", "Graphics"), {LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic (finer shadows and effects; a faster GPU)"), LOCTEXT("Low", "Low (no depth of field, simpler shadows)")},
					[&S]() { return S.Graphics == TEXT("low") ? 2 : (S.Graphics == TEXT("epic") ? 1 : 0); },
					[&S](int32 V) { S.Graphics = V == 2 ? TEXT("low") : (V == 1 ? TEXT("epic") : TEXT("high")); }, true));
				Rows.Add(Toggle(LOCTEXT("PostFx", "Vignette and grain"), &S.bPostFx));
				Rows.Add(Toggle(LOCTEXT("Smooth", "Motion smoothing between ticks"), &S.bMotionSmoothing));
				break;
			}
			case 1:
				Rows.Add(Slider(LOCTEXT("Master", "Master volume"), &S.MasterVolume));
				Rows.Add(Slider(LOCTEXT("Music", "Music"), &S.MusicVolume));
				Rows.Add(Slider(LOCTEXT("Effects", "Effects"), &S.EffectsVolume));
				Rows.Add(Slider(LOCTEXT("Voices", "Cries and screams"), &S.VoiceVolume));
				break;
			case 2:
				Rows.Add(Toggle(LOCTEXT("Shake", "Camera shake"), &S.bScreenShake));
				Rows.Add(Toggle(LOCTEXT("Flashes", "Flashes"), &S.bFlashes));
				Rows.Add(Choice(LOCTEXT("Fx", "Effects"), {LOCTEXT("Full", "Full"), LOCTEXT("Reduced", "Reduced")}, [&S]() { return S.Effects == TEXT("reduced") ? 1 : 0; },
					[&S](int32 V) { S.Effects = V ? TEXT("reduced") : TEXT("full"); }));
				Rows.Add(Choice(LOCTEXT("Gore", "Blood"), {LOCTEXT("GFull", "Full"), LOCTEXT("GLow", "Low"), LOCTEXT("GOff", "Off")},
					[&S]() { return S.Gore == TEXT("low") ? 1 : (S.Gore == TEXT("off") ? 2 : 0); }, [&S](int32 V) { S.Gore = V == 1 ? TEXT("low") : (V == 2 ? TEXT("off") : TEXT("full")); }));
				Rows.Add(Slider(LOCTEXT("UIScale", "Text size"), &S.UIScale, 0.85, 1.25, 0.05));
				Rows.Add(Toggle(LOCTEXT("InputDisp", "Input display in fights"), &S.bInputDisplay));
				Rows.Add(Choice(LOCTEXT("Diff", "CPU difficulty (Versus CPU, Training)"), {FGameFlow::DifficultyName(0), FGameFlow::DifficultyName(1), FGameFlow::DifficultyName(2)},
					[&S]() { return S.Difficulty; }, [&S](int32 V) { S.Difficulty = V; }));
				break;
			case 3:
			default:
			{
				Rows.Add(Slider(LOCTEXT("Dead", "Stick dead zone"), &S.DeadZone, 0.05, 0.6, 0.05));
				Rows.Add(Toggle(LOCTEXT("Vibe", "Vibration"), &S.bVibration));
				Rows.Add(Choice(LOCTEXT("Glyphs", "Button names"), {LOCTEXT("Auto", "Automatic"), LOCTEXT("PS", "PlayStation"), LOCTEXT("Xb", "Xbox")},
					[&S]() { return S.Glyphs == TEXT("playstation") ? 1 : (S.Glyphs == TEXT("xbox") ? 2 : 0); },
					[&S](int32 V) { S.Glyphs = V == 1 ? TEXT("playstation") : (V == 2 ? TEXT("xbox") : TEXT("auto")); }));
				for (int32 H = 0; H < 3; ++H)
				{
					Rows.Add(Header(H == 0 ? LOCTEXT("KL", "KEYBOARD, LEFT HALF") : (H == 1 ? LOCTEXT("KR", "KEYBOARD, RIGHT HALF") : LOCTEXT("PadH", "CONTROLLERS (directions are always the D-pad and left stick; the triggers also block and run)"))));
					for (int32 A = H == 2 ? (int32)EGameAction::HP : 0; A < (int32)EGameAction::Count; ++A)
					{
						const EGameAction Act = (EGameAction)A;
						Rows.Add(Action(ActionLabel(Act), [this, H, Act]() { BeginCapture(H, Act); }, [this, H, Act]()
						{
							const FSlopSettings& St = GI->Settings;
							if (CaptureHalf == H && CaptureAction == Act)
							{
								return LOCTEXT("Press", "press a key…");
							}
							return H < 2 ? SlopUI::KeyName(St.Keys[H][(int32)Act], EGlyphs::Keyboard)
								: SlopUI::KeyName(St.PadButtons[(int32)Act], GI->Router->Glyphs() == EGlyphs::Keyboard ? EGlyphs::Xbox : GI->Router->Glyphs());
						}));
					}
				}
				Rows.Add(Action(LOCTEXT("ResetKeys", "Reset all bindings to the defaults"), [this]()
				{
					GI->Settings.ResetBindings();
					GI->Settings.Save();
					GI->Notice(LOCTEXT("KeysReset", "Bindings reset"));
				}));
				break;
			}
			}
			if (Tab == 2)
			{
				Rows.Add(Action(LOCTEXT("ResetAll", "Reset every setting"), [this]()
				{
					GI->Settings.ResetAll();
					GI->ApplySettings(true);
					GI->Settings.Save();
					GI->Notice(LOCTEXT("AllReset", "All settings reset"));
				}));
				// As in the original: records, unlocks, endings and the stranger's memory, on a second press.
				Rows.Add(Action(LOCTEXT("Erase", "Erase progress (records, unlocks, endings, the stranger's memory)"), [this]()
				{
					if (GI->Now() - EraseArmedAt <= 4.0)
					{
						GI->Progress.Reset();
						GI->Progress.Save();
						EraseArmedAt = -100.0;
						GI->Notice(LOCTEXT("Erased", "Progress erased"));
					}
					else
					{
						EraseArmedAt = GI->Now();
						GI->Notice(FText::Format(LOCTEXT("EraseAgain", "Press {0} again to erase all progress"), SlopUI::Confirm(GI->Router->Glyphs())));
					}
				}));
			}
			Sel = FMath::Clamp(Sel, 0, Rows.Num() - 1);
			while (Rows.IsValidIndex(Sel) && Rows[Sel].bHeader)
			{
				++Sel;
			}
		}

		void BeginCapture(int32 Half, EGameAction A)
		{
			CaptureHalf = Half;
			CaptureAction = A;
			CaptureMessage = FText::GetEmpty();
			GI->Router->BeginCapture();
		}

		void FinishCapture(const FCapture& C)
		{
			GI->Router->EndCapture();
			FSlopSettings& S = GI->Settings;
			const int32 Half = CaptureHalf;
			CaptureHalf = -1;
			if (C.Key == EKeys::Escape || C.Key == EKeys::Gamepad_Special_Right)
			{
				CaptureMessage = LOCTEXT("Cancelled", "Unchanged.");
				return;
			}
			if (Half < 2)
			{
				if (C.Key.IsGamepadKey() || C.Key.IsMouseButton())
				{
					CaptureMessage = LOCTEXT("NeedKey", "That is not a keyboard key; the binding is unchanged.");
					return;
				}
				if (FSlopSettings::IsReservedKey(C.Key))
				{
					CaptureMessage = FText::Format(LOCTEXT("Reserved", "{0} is kept for the menus, training and the window; choose another key."), C.Key.GetDisplayName());
					return;
				}
				S.RebindKey(Half, CaptureAction, C.Key);
			}
			else
			{
				if (!C.Key.IsGamepadKey())
				{
					CaptureMessage = LOCTEXT("NeedButton", "Press a controller button; the binding is unchanged.");
					return;
				}
				if (!S.RebindPad(CaptureAction, C.Key))
				{
					CaptureMessage = LOCTEXT("ReservedPad", "That button pauses, resets, moves on in training, or is a direction; choose another.");
					return;
				}
			}
			S.Save();
			CaptureMessage = LOCTEXT("Saved", "Saved. A key already in use swaps places, so nothing is bound twice.");
		}

		virtual void Enter() override
		{
			Before = GI->Settings;
			BuildRows();
		}

		virtual void Leave() override
		{
			if (bPendingDisplay)
			{
				RevertDisplay();
			}
			GI->Router->EndCapture();
			GI->Settings.Save();
			if (ASlopMatch* M = GI->GetMatch())
			{
				M->ApplySettings();
			}
		}

		virtual TSharedRef<SWidget> Build() override
		{
			if (Rows.Num() == 0)
			{
				BuildRows();
			}
			const TArray<FText> Tabs = {LOCTEXT("TVideo", "Video"), LOCTEXT("TAudio", "Sound"), LOCTEXT("TComfort", "Comfort and game"), LOCTEXT("TControls", "Controls")};
			TSharedRef<SHorizontalBox> TabRow = SNew(SHorizontalBox);
			for (int32 I = 0; I < Tabs.Num(); ++I)
			{
				TabRow->AddSlot().AutoWidth().Padding(0, 0, 36, 0)
				[
					SNew(STextBlock).Text(Tabs[I]).Font(SlopUI::Display(40.f)).ColorAndOpacity_Lambda([this, I]() { return Tab == I ? SlopUI::Accent : SlopUI::Dim; })
				];
			}
			// As many rows as fit above the message line; the window follows the selection.
			TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			const int32 Visible = 11;
			const int32 First = FMath::Clamp(Sel - Visible / 2, 0, FMath::Max(0, Rows.Num() - Visible));
			if (First > 0)
			{
				List->AddSlot().AutoHeight().HAlign(HAlign_Center)[SlopUI::Label(LOCTEXT("More", "▲"), 20.f, SlopUI::Dim)];
			}
			for (int32 I = First; I < FMath::Min(Rows.Num(), First + Visible); ++I)
			{
				const FRow& R = Rows[I];
				if (R.bHeader)
				{
					List->AddSlot().AutoHeight().Padding(0, 14, 0, 2)[SlopUI::Label(R.Label, 24.f, SlopUI::Amber)];
					continue;
				}
				List->AddSlot().AutoHeight().Padding(0, 2)
				[
					SNew(SBorder).BorderImage(SlopUI::Rounded()).Padding(FMargin(18, 6))
					.BorderBackgroundColor_Lambda([this, I]() { return Sel == I ? FLinearColor(0.04f, 0.07f, 0.02f, 0.95f) : FLinearColor(0.01f, 0.016f, 0.013f, 0.75f); })
					.OnMouseButtonDown_Lambda([this, I](const FGeometry&, const FPointerEvent&) { Sel = I; Press(); return FReply::Handled(); })
					.OnMouseMove_Lambda([this, I](const FGeometry&, const FPointerEvent&)
					{
						if (Sel != I)
						{
							Sel = I;
							Sound(TEXT("ui_move"));
						}
						return FReply::Unhandled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Font(SlopUI::Body(28.f)).Text(R.Label).ColorAndOpacity_Lambda([this, I]() { return Sel == I ? SlopUI::Accent : SlopUI::Text; })]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(SlopUI::Body(28.f)).ColorAndOpacity(SlopUI::Text).Text_Lambda([V = R.Value]() { return V ? V() : FText::GetEmpty(); })]
					]
				];
			}
			if (First + Visible < Rows.Num())
			{
				List->AddSlot().AutoHeight().HAlign(HAlign_Center)[SlopUI::Label(LOCTEXT("MoreDown", "▼"), 20.f, SlopUI::Dim)];
			}
			TSharedRef<SOverlay> O = SNew(SOverlay);
			O->AddSlot()[bFromPause ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(SlopUI::Box()).ColorAndOpacity(FLinearColor(0, 0, 0, 0.82f))) : SlopUI::Backdrop()];
			O->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)
			[
				SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 50, 1600, 90))[SlopUI::Title(LOCTEXT("SettingsT", "SETTINGS"), 72.f, SlopUI::Text)]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 150, 1600, 60))[TabRow]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 230, 1600, 700))[List]
				+ SConstraintCanvas::Slot().Alignment(FVector2D::ZeroVector).Offset(FMargin(160, 950, 1600, 50))
				[
					SNew(STextBlock).Font(SlopUI::Body(26.f)).ColorAndOpacity(SlopUI::Amber).Text_Lambda([this]()
					{
						if (bPendingDisplay)
						{
							return FText::Format(LOCTEXT("Keep", "Keep these display settings? {0} to keep · reverting in {1} s"),
								SlopUI::Confirm(GI->Router->Glyphs()), FMath::Max(0, FMath::CeilToInt(RevertAt - GI->Now())));
						}
						return CaptureMessage;
					})
				]
			]]];
			O->AddSlot()[SlopUI::Hint([this]()
			{
				const EGlyphs G = GI->Router->Glyphs();
				return FText::Format(LOCTEXT("SetHint", "Up and down to choose   ·   left and right to change   ·   {0} for the next page   ·   {1} to go back"),
					G == EGlyphs::Keyboard ? FText::FromString(TEXT("Tab")) : LOCTEXT("R1", "R1"), SlopUI::Back(G));
			})];
			return O;
		}

		void Press()
		{
			if (bPendingDisplay)
			{
				KeepDisplay();
				return;
			}
			if (Rows.IsValidIndex(Sel))
			{
				if (Rows[Sel].Activate)
				{
					Rows[Sel].Activate();
				}
				else if (Rows[Sel].Change)
				{
					Rows[Sel].Change(1);
				}
				Sound(TEXT("ui_select"));
			}
		}

		void Move(int32 D)
		{
			Sel = Step(Sel, Rows.Num(), D, [this](int32 I) { return !Rows[I].bHeader; });
			Sound(TEXT("ui_move"));
			GI->RebuildScreen();
		}

		virtual void Tick(float Dt) override
		{
			if (CaptureHalf >= 0)
			{
				if (TOptional<FCapture> C = GI->Router->TakeCapture())
				{
					FinishCapture(*C);
				}
			}
			if (bPendingDisplay && GI->Now() >= RevertAt)
			{
				RevertDisplay();
			}
		}

		virtual void OnUi(const FUiEvent& E) override
		{
			if (CaptureHalf >= 0)
			{
				return;
			}
			switch (E.Action)
			{
			case EUi::Up: Move(-1); break;
			case EUi::Down: Move(1); break;
			case EUi::Left:
			case EUi::Right:
				if (Rows.IsValidIndex(Sel) && Rows[Sel].Change)
				{
					Rows[Sel].Change(E.Action == EUi::Right ? 1 : -1);
				}
				break;
			case EUi::Next:
			case EUi::Prev:
				Tab = (Tab + (E.Action == EUi::Next ? 1 : 3)) % 4;
				Sel = 0;
				BuildRows();
				Sound(TEXT("ui_move"));
				GI->RebuildScreen();
				break;
			case EUi::Confirm: Press(); break;
			case EUi::Back:
			case EUi::Pause:
				Sound(TEXT("ui_back"));
				if (bPendingDisplay)
				{
					RevertDisplay();
				}
				if (bFromPause)
				{
					GI->ShowOverlay(SlopScreens::Pause());
				}
				else
				{
					GI->Goto(SlopScreens::MainMenu());
				}
				break;
			default: break;
			}
		}
	};
}

using namespace SlopSettingsImpl;

namespace SlopScreens
{
	TSharedPtr<FSlopScreen> Settings(bool bFromPause)
	{
		TSharedPtr<FSlopScreen> S = MakeShared<FSettingsScreen>(bFromPause);
		S->ScreenName = TEXT("Settings");
		return S;
	}
}

#undef LOCTEXT_NAMESPACE
