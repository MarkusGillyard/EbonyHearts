#include "View/SlopGuides.h"
#include "AI/SlopAI.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "UI/SlopStyle.h"
#include "View/SlopFighterActor.h"
#include "View/SlopMatch.h"

#define LOCTEXT_NAMESPACE "SlopGuides"

namespace Slop
{
	namespace
	{
		struct FStepDef { const TCHAR* Id; FText Text; };
		const TArray<FStepDef>& TutorialSteps()
		{
			static const TArray<FStepDef> S = {
				{TEXT("walk_f"), LOCTEXT("WalkF", "Walk toward Gorm: hold {0}.")},
				{TEXT("walk_b"), LOCTEXT("WalkB", "Walk away: hold {0}.")},
				{TEXT("jump"), LOCTEXT("Jump", "Jump with {0} (twice). Up-forward or up-back jumps are somersaults.")},
				{TEXT("crouch"), LOCTEXT("Crouch", "Crouch: hold {0}. High attacks pass over a crouching fighter.")},
				{TEXT("strikes"), LOCTEXT("Strikes", "Try all four strikes: High Punch {0}, Low Punch {1}, High Kick {2}, Low Kick {3}.")},
				{TEXT("block"), LOCTEXT("Block", "Gorm will swing at you. Hold Block {0} to stop his punches (3 blocks).")},
				{TEXT("block_low"), LOCTEXT("BlockLow", "Sweeps hit low: hold Down {0} and Block {1} together to block them (2 blocks).")},
				{TEXT("uppercut"), LOCTEXT("Uppercut", "The uppercut: Down + High Punch, up close. Land one on Gorm.")},
				{TEXT("special"), LOCTEXT("Special", "A special move. Ember Shot: Down, Forward, High Punch, in quick succession.")},
				{TEXT("chain"), LOCTEXT("Chain", "A dial-a-combo: High Punch, High Punch, Low Punch, then Back + High Kick, quickly. Each link spends run meter.")},
				{TEXT("run"), LOCTEXT("Run", "Run: hold Run {0} and Forward. Running empties the blue meter under your health.")},
				{TEXT("throw"), LOCTEXT("Throw", "Throw: get right up to Gorm and press Forward + Low Punch.")},
				{TEXT("finish"), LOCTEXT("Finish", "Knock Gorm out. When FINISH HIM appears, stand a sweep's distance away (about two steps) and enter Forward, Forward, Down + High Punch.")},
			};
			return S;
		}
	}

	// --- Tutorial ------------------------------------------------------------------------------------

	FTutorial::FTutorial(ASlopMatch* InMatch, USlopGameInstance* InGI) : M(InMatch), GI(InGI)
	{
		FFightSim& Sim = M->Sim;
		Sim.bRefillHealth = true;
		Sim.Phase = EPhase::Fight;
		for (FFighter& F : Sim.Fighters)
		{
			F.SetState(EState::Idle);
		}
		Next();
	}

	FText FTutorial::Keys(EGameAction A) const { return SlopUI::ActionKey(GI, A, 0); }

	void FTutorial::Next()
	{
		++StepIndex;
		Count = 0;
		Seen.Reset();
		T = 0;
		const TArray<FStepDef>& Steps = TutorialSteps();
		if (StepIndex >= Steps.Num())
		{
			bDone = true;
			PromptText = FText::GetEmpty();
			GI->Progress.SetTutorialDone();
			// A finished knockout leads to the results by itself; otherwise end here.
			if (M->Sim.Phase != EPhase::Finisher && M->Sim.Phase != EPhase::RoundEnd && M->Sim.Phase != EPhase::MatchEnd)
			{
				M->FinishFromGuide(0);
			}
			return;
		}
		const FStepDef& S = Steps[StepIndex];
		const FString Id = S.Id;
		FText Text = S.Text;
		if (Id == TEXT("walk_f")) Text = FText::Format(S.Text, Keys(EGameAction::Right));
		else if (Id == TEXT("walk_b")) Text = FText::Format(S.Text, Keys(EGameAction::Left));
		else if (Id == TEXT("jump")) Text = FText::Format(S.Text, Keys(EGameAction::Up));
		else if (Id == TEXT("crouch")) Text = FText::Format(S.Text, Keys(EGameAction::Down));
		else if (Id == TEXT("strikes")) Text = FText::Format(S.Text, Keys(EGameAction::HP), Keys(EGameAction::LP), Keys(EGameAction::HK), Keys(EGameAction::LK));
		else if (Id == TEXT("block")) Text = FText::Format(S.Text, Keys(EGameAction::BL));
		else if (Id == TEXT("block_low")) Text = FText::Format(S.Text, Keys(EGameAction::Down), Keys(EGameAction::BL));
		else if (Id == TEXT("run")) Text = FText::Format(S.Text, Keys(EGameAction::RN));
		else if (Id == TEXT("finish")) ArmFinish();
		const EGlyphs G = GI->Router->Glyphs();
		PromptText = FText::Format(LOCTEXT("StepN", "Step {0} of {1}.  {2}   ({3}: skip)"), StepIndex + 1, Steps.Num(), Text,
			G == EGlyphs::Keyboard ? FText::FromString(TEXT("Tab")) : LOCTEXT("R1", "R1"));
		GI->Audio->Play(TEXT("ui_select"), -4.f, 1.f, ESlopBus::Ui);
	}

	void FTutorial::ArmFinish()
	{
		FFightSim& Sim = M->Sim;
		Sim.bTraining = false;
		Sim.bRefillHealth = false;
		Sim.bFinishersEnabled = true;
		Sim.RoundsToWin = 1;
		Sim.Wins[0] = Sim.Wins[1] = 0;
		Sim.Timer = FFightSim::RoundTicks;
		Sim.Fighters[1].Health = 140;
	}

	double FTutorial::Reach(const FFighter& F, FName Move) const
	{
		double R = 0;
		if (const FMoveDef* Mv = F.Def->Move(Move))
		{
			for (const FFrameBox& B : Mv->Hitboxes)
			{
				R = FMath::Max(R, B.Rect.EndX());
			}
		}
		return R + 0.22 * M->Sim.Fighters[1 - F.Index].Scale();
	}

	// Gorm's part: still, except when a blocking step needs him to attack.
	void FTutorial::FilterPads(FPadState Pads[2])
	{
		++T;
		Pads[1] = FPadState();
		if (bDone || StepIndex < 0 || StepIndex >= TutorialSteps().Num())
		{
			return;
		}
		const FString Id = TutorialSteps()[StepIndex].Id;
		const FFighter& G = M->Sim.Fighters[1];
		if ((Id == TEXT("block") || Id == TEXT("block_low")) && G.IsActionable())
		{
			// He closes in until the swing would land, so a held block has something to stop.
			const FName Swing = Id == TEXT("block") ? FName(TEXT("hp")) : FName(TEXT("sweep"));
			if (M->Sim.Distance() > Reach(G, Swing) - 0.1)
			{
				Pads[1] = FPadIntent::Pad(6, G.Facing);
			}
			else if (T % 70 == 0)
			{
				Pads[1] = FPadIntent::Pad(Id == TEXT("block") ? 5 : 4, G.Facing, Id == TEXT("block") ? BtnHP : BtnLK);
			}
		}
	}

	void FTutorial::OnEvent(const FSimEvent& E)
	{
		if (bDone || StepIndex < 0 || StepIndex >= TutorialSteps().Num())
		{
			return;
		}
		const FString Id = TutorialSteps()[StepIndex].Id;
		const FFighter& P = M->Sim.Fighters[0];
		if (Id == TEXT("jump") && E.Type == EEvent::Jump && E.Who == 0 && ++Count >= 2) Next();
		else if (Id == TEXT("strikes") && E.Type == EEvent::Move && E.Who == 0)
		{
			FString Mv = E.Move.ToString();
			Mv.RemoveFromEnd(TEXT("2"));
			if (Mv == TEXT("hp") || Mv == TEXT("lp") || Mv == TEXT("hk") || Mv == TEXT("lk"))
			{
				Seen.Add(FName(*Mv));
				if (Seen.Num() >= 4) Next();
			}
		}
		else if (Id == TEXT("block") && E.Type == EEvent::Block && E.Who == 0 && ++Count >= 3) Next();
		else if (Id == TEXT("block_low") && E.Type == EEvent::Block && E.Who == 0 && P.IsCrouching() && ++Count >= 2) Next();
		else if (Id == TEXT("uppercut") && E.Type == EEvent::Hit && E.By == 0 && E.Move == TEXT("uppercut")) Next();
		else if (Id == TEXT("special") && E.Type == EEvent::Projectile && E.Who == 0) Next();
		else if (Id == TEXT("chain") && E.Type == EEvent::Chain && E.Who == 0 && E.Move == TEXT("ashka_spin_chain")) Next();
		else if (Id == TEXT("throw") && E.Type == EEvent::Throw && E.By == 0) Next();
		else if (Id == TEXT("finish"))
		{
			if (E.Type == EEvent::FinisherEnd && E.Who == 0)
			{
				Next();
			}
			else if (E.Type == EEvent::RoundEnd && E.How != TEXT("finisher"))
			{
				PromptText = LOCTEXT("Again", "Not quite: knock him out, then enter the finisher at sweep distance. Again!");
				RetryAt = M->Sim.TickCount + 96;
			}
		}
	}

	void FTutorial::OnTick()
	{
		if (bDone || StepIndex < 0 || StepIndex >= TutorialSteps().Num())
		{
			return;
		}
		if (RetryAt >= 0 && M->Sim.TickCount >= RetryAt)
		{
			RetryAt = -1;
			M->ResetPositions();
			ArmFinish();
			--StepIndex;
			Next();
			return;
		}
		const FString Id = TutorialSteps()[StepIndex].Id;
		const EState S = M->Sim.Fighters[0].State;
		if ((Id == TEXT("walk_f") && S == EState::WalkF) || (Id == TEXT("walk_b") && S == EState::WalkB) || (Id == TEXT("crouch") && S == EState::Crouch))
		{
			if (++Count > 40) Next();
		}
		else if (Id == TEXT("run") && S == EState::Run && ++Count > 25)
		{
			Next();
		}
	}

	// --- Trials ------------------------------------------------------------------------------------------

	FTrialsRunner::FTrialsRunner(ASlopMatch* InMatch, USlopGameInstance* InGI) : M(InMatch), GI(InGI)
	{
		Fighter = M->Config.P1;
		if (const TArray<TSharedPtr<FJsonValue>>* L = Json::Arr(GetContent().Trials, *Fighter.ToString()))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FTrial Tr;
				Tr.Name = Json::Str(O, TEXT("name"));
				for (const TSharedPtr<FJsonValue>& S : *Json::Arr(O, TEXT("steps"))) Tr.Steps.Add(S->AsString());
				for (const TSharedPtr<FJsonValue>& S : *Json::Arr(O, TEXT("moves"))) Tr.Moves.Add(FName(*S->AsString()));
				Trials.Add(Tr);
			}
		}
		FFightSim& Sim = M->Sim;
		Sim.bRefillHealth = true;
		Sim.Phase = EPhase::Fight;
		for (FFighter& F : Sim.Fighters)
		{
			F.SetState(EState::Idle);
		}
		// Start at the first trial not yet completed.
		const TArray<FString> Done = GI->Progress.TrialsDone(Fighter);
		while (Index < Trials.Num() && Done.Contains(Trials[Index].Name))
		{
			++Index;
		}
		if (Index >= Trials.Num())
		{
			Index = 0;
		}
		Begin();
	}

	void FTrialsRunner::Begin()
	{
		if (Trials.Num() == 0)
		{
			PromptText = LOCTEXT("NoTrials", "No trials for this fighter yet.");
			return;
		}
		Check = FTrialCheck();
		Check.Moves = Trials[Index].Moves;
		Check.Who = 0;
		Show();
		GI->Audio->Play(TEXT("ui_select"), -4.f, 1.f, ESlopBus::Ui);
	}

	void FTrialsRunner::Show(const FString& Note)
	{
		const FTrial& Tr = Trials[Index];
		TArray<FString> Pretty;
		for (const FString& S : Tr.Steps)
		{
			Pretty.Add(FCommand::Pretty(S));
		}
		FString Ticks;
		for (int32 I = 0; I < Tr.Moves.Num(); ++I)
		{
			Ticks += I < Check.Progress ? TEXT("●") : TEXT("○");
		}
		const bool bDoneBefore = GI->Progress.TrialsDone(Fighter).Contains(Tr.Name);
		const EGlyphs G = GI->Router->Glyphs();
		PromptText = FText::FromString(FString::Printf(TEXT("Trial %d of %d:  %s%s\n%s     %s%s   (%s: next trial, %s: reset)"), Index + 1, Trials.Num(), *Tr.Name,
			bDoneBefore ? TEXT("  ✓") : TEXT(""), *FString::Join(Pretty, TEXT("   ")), *Ticks, Note.IsEmpty() ? TEXT("") : *(TEXT("   ") + Note),
			G == EGlyphs::Keyboard ? TEXT("Tab") : TEXT("R1"), G == EGlyphs::Keyboard ? TEXT("F4") : (G == EGlyphs::PlayStation ? TEXT("Create") : TEXT("View"))));
	}

	void FTrialsRunner::OnEvent(const FSimEvent& E)
	{
		if (Trials.Num() == 0 || BusyUntil >= 0)
		{
			return;
		}
		switch (Check.Feed(E))
		{
		case FTrialCheck::EResult::Hit: Show(); break;
		case FTrialCheck::EResult::Dropped: Show(TEXT("dropped")); break;
		case FTrialCheck::EResult::Done:
		{
			const bool bFirst = GI->Progress.TrialDone(Fighter, Trials[Index].Name);
			Show();
			M->Announce(LOCTEXT("TrialDone", "TRIAL COMPLETE"), bFirst ? FText::FromString(Trials[Index].Name) : FText::GetEmpty());
			GI->Audio->Play(TEXT("victory"), -6.f);
			BusyUntil = M->Sim.TickCount + 132;
			break;
		}
		default: break;
		}
	}

	void FTrialsRunner::OnTick()
	{
		if (BusyUntil >= 0 && M->Sim.TickCount >= BusyUntil)
		{
			BusyUntil = -1;
			Advance();
		}
	}

	void FTrialsRunner::Skip()
	{
		if (BusyUntil < 0)
		{
			GI->Audio->Play(TEXT("ui_move"), -6.f, 1.f, ESlopBus::Ui);
			Advance();
		}
	}

	void FTrialsRunner::Advance()
	{
		if (Trials.Num() == 0)
		{
			return;
		}
		if (GI->Progress.TrialsDone(Fighter).Num() >= Trials.Num() && Index == Trials.Num() - 1)
		{
			M->Announce(LOCTEXT("AllDone", "ALL TRIALS COMPLETE"), FText::GetEmpty());
		}
		Index = (Index + 1) % Trials.Num();
		M->ResetPositions();
		Begin();
	}
}

#undef LOCTEXT_NAMESPACE
