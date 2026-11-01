#include "View/SlopMatch.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Anim/SlopMeshRig.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "Core/SlopLog.h"
#include "View/SlopArena.h"
#include "View/SlopFightCamera.h"
#include "View/SlopFighterActor.h"
#include "View/SlopVfx.h"

using namespace Slop;

ASlopMatch::ASlopMatch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	// Keeps ticking while the world is paused: the match runs its own pause.
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

USlopGameInstance* ASlopMatch::GI() const { return Cast<USlopGameInstance>(GetGameInstance()); }

bool ASlopMatch::Start(const FMatchConfig& InConfig)
{
	Config = InConfig;
	FContent& C = GetContent();
	const FFighterDef* D1 = C.Fighter(Config.P1);
	const FFighterDef* D2 = C.Fighter(Config.P2);
	if (!D1 || !D2 || C.Errors.Num() > 0)
	{
		UE_LOG(LogSlop, Error, TEXT("Match refused: fighters %s/%s, %d data errors"), *Config.P1.ToString(), *Config.P2.ToString(), C.Errors.Num());
		return false;
	}
	const EMode M = Config.Mode;
	const bool bPractice = M == EMode::Training || M == EMode::Tutorial || M == EMode::Trials;
	Sim.bTraining = bPractice;
	Sim.RoundsToWin = FMath::Max(1, Config.RoundsToWin);
	Sim.RoundLength = FMath::Max(0, Config.RoundSeconds) * 60;
	if (D1->Move(TEXT("stage_vat")) && Config.Stage == TEXT("rendering_yards"))
	{
		Sim.StageFinisher = TEXT("stage_vat");
	}
	Sim.bIntro = !bPractice;
	Sim.Setup(D1, D2);
	if ((M == EMode::Arcade || M == EMode::Survival) && Config.P2Health > 0)
	{
		// The arcade sets the bosses' health by tower.
		Sim.Fighters[1].HealthMax = Config.P2Health;
		Sim.Fighters[1].Health = Config.P2Health;
		Sim.Fighters[1].bGuardian = Config.bGuardian;
		Sim.Fighters[1].bVessel = Config.GuardianPhase == 2;
		Sim.Fighters[1].bNoVessel = Config.GuardianPhase == 1;
		Sim.Fighters[1].NextCharge = 120;
	}
	if (M == EMode::Survival && Config.P1Health > 0)
	{
		Sim.Fighters[0].Health = FMath::Min(Sim.Fighters[0].MaxHealth(), Config.P1Health);
	}
	Sim.Fighters[0].OutfitBody = Config.Outfit[0];
	Sim.Fighters[1].OutfitBody = Config.Outfit[1];
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Arena = W->SpawnActor<ASlopArena>(P);
	Arena->bStrobe = GI()->Settings.bFlashes;
	Arena->Build(Config.Stage, GI()->Settings.Graphics != TEXT("low"));
	for (int32 I = 0; I < 2; ++I)
	{
		Views[I] = W->SpawnActor<ASlopFighterActor>(P);
		if (!Views[I]->Setup(Sim.Fighters[I], USlopGameInstance::MeshFor(Sim.Fighters[I])))
		{
			return false;
		}
		Views[I]->bSweats = true;
		DressView(I);
	}
	Camera = W->SpawnActor<ASlopFightCamera>(P);
	Camera->Bind(&Sim);
	Camera->Snap();
	Vfx = W->SpawnActor<ASlopVfx>(P);
	Vfx->Setup(this);
	Director.Setup(this);
	ApplySettings();
#if !UE_BUILD_SHIPPING
	// Development: -SlopHideFighters leaves the stage empty and the two far apart, so the camera
	// takes in the ground (its picture for the stage screen). Kept hidden every frame, as each
	// round start shows the bodies again.
	bDevHideFighters = FParse::Param(FCommandLine::Get(), TEXT("SlopHideFighters"));
	if (bDevHideFighters)
	{
		Sim.Fighters[0].Pos.X = -2.6;
		Sim.Fighters[1].Pos.X = 2.6;
		Camera->Snap();
	}
	// Development: -SlopQhi=<n> starts a Qhi fighter's meter at n (0 shows the spent body).
	double StartQhi = 0.0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SlopQhi="), StartQhi))
	{
		for (FFighter& F : Sim.Fighters)
		{
			if (F.UsesQhi())
			{
				F.Qhi = FMath::Clamp(StartQhi, 0.0, F.QhiRules().Max);
				F.QhiIdle = 0;
			}
		}
	}
	// Development: -SlopHealth=<n> gives both fighters that much health every round, to reach a match's
	// end quickly (the finish, the finisher's call, the results).
	int32 StartHealth = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SlopHealth="), StartHealth) && StartHealth > 0)
	{
		for (FFighter& F : Sim.Fighters)
		{
			F.HealthMax = StartHealth;
			F.Health = StartHealth;
		}
	}
	// Development: -SlopMarks=<0..1> starts both fighters as beaten as that on head, body and legs
	// (to look at the skin's marks without fighting for them).
	float StartMarks = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SlopMarks="), StartMarks))
	{
		for (ASlopFighterActor* V : Views)
		{
			for (int32 R = 0; R < 3; ++R)
			{
				V->TakeBlow(R, (int32)(StartMarks * 0.33f * V->Anim.Fighter->Def->Health + 0.5f));
			}
		}
	}
#endif
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(Camera);
		PC->SetShowMouseCursor(false);
	}
	if (M == EMode::Training || M == EMode::Trials)
	{
		TUniquePtr<FTrainingDummy> D = MakeUnique<FTrainingDummy>(Sim, 1, Config.Difficulty);
		Dummy = D.Get();
		Brains[1] = MoveTemp(D);
		Sim.bRefillHealth = M == EMode::Training;
		Sim.bInfiniteRun = M == EMode::Trials;
	}
	else if (M != EMode::Tutorial)
	{
		const int32 Diff = FMath::Clamp(Config.Difficulty, 0, 2);
		FGameFlow& F = GI()->Flow;
		for (int32 I = 0; I < 2; ++I)
		{
			if (F.IsCpu(I))
			{
				if (Sim.Fighters[I].Def->bBoss)
				{
					Brains[I] = MakeUnique<FBossBrain>(Sim, I, Diff);
				}
				else if (Sim.Fighters[I].bGuardian)
				{
					Brains[I] = MakeUnique<FGuardianBrain>(Sim, I, Diff);
				}
				else
				{
					TUniquePtr<FCpuBrain> Cpu = MakeUnique<FCpuBrain>(Sim, I, Config.CpuTier >= 0 ? Config.CpuTier : Diff, (int32)(FPlatformTime::Cycles() & 0x7fff) + I * 101);
					CpuBrains[I] = Cpu.Get();
					Brains[I] = MoveTemp(Cpu);
				}
			}
		}
		const int32 Auto = GI()->AutoPlayLevel;
		if (Auto >= 0 && !Brains[0])
		{
			// Development autoplay: the CPU takes the player's side.
			TUniquePtr<FCpuBrain> Cpu = MakeUnique<FCpuBrain>(Sim, 0, Auto, (int32)(FPlatformTime::Cycles() & 0x7fff));
			CpuBrains[0] = Cpu.Get();
			Brains[0] = MoveTemp(Cpu);
		}
	}
	if (M == EMode::Tutorial)
	{
		Guide = MakeUnique<FTutorial>(this, GI());
	}
	else if (M == EMode::Trials)
	{
		Guide = MakeUnique<FTrialsRunner>(this, GI());
	}
	GI()->Audio->PlayMusic(Arena->Music);
	Clock.Reset();
	GI()->Note(FString::Printf(TEXT("Match: %s v %s, %s, %s, difficulty %d"), *Config.P1.ToString(), *Config.P2.ToString(), ModeName(M), *Config.Stage.ToString(), Config.Difficulty));
	return true;
}

void ASlopMatch::ApplySettings()
{
	const FSlopSettings& S = GI()->Settings;
	if (Camera)
	{
		Camera->bShakeEnabled = S.bScreenShake;
		Camera->SetLook(S.Graphics != TEXT("low"), S.bPostFx, Arena ? Arena->ExposureEV100 : 0.f);
	}
	for (ASlopFighterActor* V : Views)
	{
		if (V)
		{
			V->Anim.bSmoothing = S.bMotionSmoothing;
			V->bFlashes = S.bFlashes;
			V->SetBlood(S.Gore == TEXT("off") ? 0.f : (S.Gore == TEXT("low") ? 0.4f : 1.f));
		}
	}
	if (Vfx)
	{
		Vfx->bReduced = S.Effects == TEXT("reduced");
		Vfx->bFlashes = S.bFlashes;
		Vfx->Gore = S.Gore;
	}
}

void ASlopMatch::EndPlay(const EEndPlayReason::Type Reason)
{
	for (TObjectPtr<ASlopFighterActor>& V : Views)
	{
		if (V)
		{
			V->Destroy();
			V = nullptr;
		}
	}
	if (Camera) Camera->Destroy();
	if (Arena) Arena->Destroy();
	if (Vfx) Vfx->Destroy();
	Super::EndPlay(Reason);
}

void ASlopMatch::SetPaused(bool bPause)
{
	if (bEnded && bPause)
	{
		return;
	}
	bPaused = bPause;
	if (!bPause && Dummy)
	{
		// The difficulty may have changed in Settings from the pause menu.
		Dummy->SetLevel(GI()->Settings.Difficulty);
	}
	GI()->Router->ResetAll();
	GI()->Router->StopRumble();
	GI()->Router->bFightActive = !bPause;
	GI()->Audio->PauseMusic(bPause);
}

void ASlopMatch::BeginPrelude(bool bShort, bool bWords)
{
	if (!Sim.bIntro || Sim.Phase != EPhase::Intro)
	{
		return;
	}
	bPreludeShort = bShort;
	PreludeT = 0.0;
	PreludeBoth = bWords ? 3.4 : 1.25;
	Prelude = bShort ? EPrelude::Both : EPrelude::Ground;
	bPreludeCalled[0] = bPreludeCalled[1] = false;
	// Held longer than any beat can run; the prelude's end releases it.
	Sim.IntroHold = 60 * 60;
	if (!bShort)
	{
		// Each stands ready until their shot, and then gives their performance.
		for (FFighter& F : Sim.Fighters)
		{
			F.SetState(EState::Idle);
		}
	}
	PreludeStep(0.0);
}

void ASlopMatch::SkipPrelude()
{
	if (InPrelude())
	{
		Prelude = EPrelude::None;
		bHandingOver = true;
		Sim.EndIntro();
		Camera->ClearOverride();
		GI()->Router->bFightActive = !bPaused;
	}
}

// The shots, in the simulation's metres. Every beat drifts, so nothing on screen is ever still, and
// the camera travels between fighters rather than cutting: one unbroken move from the first shot to
// the live framing, which is what lets the fight begin without a scene change.
void ASlopMatch::PreludeStep(double Delta)
{
	PreludeT += Delta;
	const FFighter& A = Sim.Fighters[0];
	const FFighter& B = Sim.Fighters[1];
	const double Mid = (A.Pos.X + B.Pos.X) * 0.5;
	const double Sep = FMath::Abs(B.Pos.X - A.Pos.X);
	// A fighter's head and shoulders, the camera out on the side they face.
	const auto Close = [](const FFighter& F, double K, FVector& Pos, FVector& Look)
	{
		const double H = F.Def->Height;
		Look = FVector(F.Pos.X + 0.1 * F.Facing, F.Pos.Y + H * 0.86, 0.0);
		Pos = Look + FVector(FMath::Lerp(0.95, 0.72, K) * F.Facing, FMath::Lerp(-0.1, -0.04, K), FMath::Lerp(2.45, 2.05, K) * FMath::Max(1.0, H / 1.8));
	};
	// Both, wide enough for this screen's shape, from low down.
	const double BothDist = FMath::Max(4.6, (Sep * 0.5 + 0.85) / (Camera->HalfWidthAt(1.0)));
	const auto Pair = [&](double K, FVector& Pos, FVector& Look)
	{
		Look = FVector(Mid, 1.02, 0.0);
		Pos = Look + FVector(0.0, FMath::Lerp(-0.42, -0.28, K), FMath::Lerp(BothDist + 0.5, BothDist, K));
	};
	const auto Sweep = [](const FVector& P0, const FVector& L0, const FVector& P1, const FVector& L1, double K, FVector& Pos, FVector& Look)
	{
		const double E = Anim::FPose::Ease(TEXT("inout"), FMath::Clamp(K, 0.0, 1.0));
		Pos = FMath::Lerp(P0, P1, E);
		Look = FMath::Lerp(L0, L1, E);
	};
	const double Rise = 0.75, One = 1.15, Over = 0.42, Close2 = 0.55;
	FVector Pos, Look, P0, L0, P1, L1;
	double T = PreludeT;
	if (bPreludeShort)
	{
		// The ground's wide view coming down onto the two of them.
		const double K = T / 0.7;
		Pair(0.0, P1, L1);
		Sweep(P1 + FVector(0.0, 1.0, 1.6), L1 + FVector(0.0, 0.3, 0.0), P1, L1, K, Pos, Look);
		if (T >= 0.7)
		{
			Pair((T - 0.7) / 1.0, Pos, Look);
		}
		Prelude = EPrelude::Both;
		if (T >= 1.7)
		{
			SkipPrelude();
			return;
		}
		Camera->Stage(Pos, Look);
		return;
	}
	const FVector Wide = FVector(Mid + A.Facing * -1.9, 1.7, 7.0);
	if (T < Rise)
	{
		Prelude = EPrelude::Ground;
		Close(A, 0.0, P1, L1);
		Sweep(Wide, FVector(Mid - A.Facing * 0.9, 1.3, 0.0), P1, L1, FMath::Pow(T / Rise, 1.4), Pos, Look);
	}
	else if ((T -= Rise) < One)
	{
		Prelude = EPrelude::First;
		Close(A, T / One, Pos, Look);
		if (!bPreludeCalled[0])
		{
			bPreludeCalled[0] = true;
			Sim.Fighters[0].SetState(EState::Intro);
		}
	}
	else if ((T -= One) < Over)
	{
		Prelude = EPrelude::Second;
		Close(A, 1.0, P0, L0);
		Close(B, 0.0, P1, L1);
		Sweep(P0, L0, P1, L1, T / Over, Pos, Look);
		if (!bPreludeCalled[1])
		{
			bPreludeCalled[1] = true;
			Sim.Fighters[1].SetState(EState::Intro);
		}
	}
	else if ((T -= Over) < One)
	{
		Prelude = EPrelude::Second;
		Close(B, T / One, Pos, Look);
	}
	else if ((T -= One) < Close2)
	{
		Prelude = EPrelude::Both;
		Close(B, 1.0, P0, L0);
		Pair(0.0, P1, L1);
		Sweep(P0, L0, P1, L1, T / Close2, Pos, Look);
	}
	else if ((T -= Close2) < PreludeBoth)
	{
		Prelude = EPrelude::Both;
		Pair(T / PreludeBoth, Pos, Look);
	}
	else
	{
		SkipPrelude();
		return;
	}
	Camera->Stage(Pos, Look);
}

void ASlopMatch::ResetPositions()
{
	Advantage.Reset();
	Sim.RestartRound();
	Sim.Phase = EPhase::Fight;
	for (FFighter& F : Sim.Fighters)
	{
		F.SetState(EState::Idle);
	}
	for (ASlopFighterActor* V : Views)
	{
		V->SyncFromSim(true);
	}
	for (TUniquePtr<IBrain>& B : Brains)
	{
		if (B)
		{
			B->Forget();
		}
	}
	Camera->Snap();
}

void ASlopMatch::Tick(float DeltaSeconds)
{
	if (CineT > 0.0)
	{
		CineT -= DeltaSeconds;
		if (CineT <= 0.0 && Camera)
		{
			Camera->ReleaseZoom();
		}
	}
	Super::Tick(DeltaSeconds);
	FrameEvents.Reset();
	SteppedThisFrame = 0;
	if (!bPaused)
	{
		const int32 N = Clock.Advance(DeltaSeconds);
		for (int32 I = 0; I < N; ++I)
		{
			if (bDebugPause && !bDebugStep)
			{
				break;
			}
			FixedTick();
			if (bDebugPause)
			{
				bDebugStep = false;
				break;
			}
		}
	}
	const bool bFrozen = bPaused || bDebugPause;
	const double Frac = bFrozen ? 1.0 : Clock.Frac();
	double D = bFrozen ? 0.0 : DeltaSeconds;
	if (bDebugPause && !bPaused)
	{
		D = SteppedThisFrame / 60.0;
	}
	for (ASlopFighterActor* V : Views)
	{
		V->UpdateVisual(Frac, D);
	}
	for (int32 I = 0; I < 2; ++I)
	{
		if (FormFade[I] > 0.f)
		{
			FormFade[I] = FMath::Max(0.f, FormFade[I] - (float)D * 4.f);
			Views[I]->SetDissolve(FormFade[I], FLinearColor(1.f, 0.78f, 0.3f));
		}
	}
	if (InPrelude())
	{
		PreludeStep(bFrozen ? 0.0 : DeltaSeconds);
	}
	else
	{
		Camera->Step(bPaused ? 0.0 : DeltaSeconds);
	}
	if (Sim.RoundNum != ShownRound)
	{
		// Both bodies have just been put back at their marks: the camera goes straight there, under
		// a moment's dark, rather than whipping across the stage.
		ShownRound = Sim.RoundNum;
		for (ASlopFighterActor* V : Views)
		{
			V->SyncFromSim(true);
		}
		Camera->Snap();
		GI()->Curtain(0.85f, 0.4f);
	}
#if !UE_BUILD_SHIPPING
	// Development: -SlopTraceCam logs both fighters and the camera every 6 ticks.
	static const bool bTraceCam = FParse::Param(FCommandLine::Get(), TEXT("SlopTraceCam"));
	if (bTraceCam && Sim.TickCount % 6 == 0)
	{
		const FVector C = Camera->GetActorLocation();
		UE_LOG(LogSlop, Display, TEXT("Trace: t%d p0 (%.2f %.2f) %s p1 (%.2f %.2f) %s cam (%.0f %.0f %.0f) rot %s"), Sim.TickCount,
			Sim.Fighters[0].Pos.X, Sim.Fighters[0].Pos.Y, StateName(Sim.Fighters[0].State), Sim.Fighters[1].Pos.X, Sim.Fighters[1].Pos.Y, StateName(Sim.Fighters[1].State),
			C.X, C.Y, C.Z, *Camera->GetActorRotation().ToString());
	}
#endif
	Director.Step(D);
	if (bDevHideFighters)
	{
		for (ASlopFighterActor* V : Views)
		{
			V->SetHidden(true);
		}
	}
	Vfx->Step(D);
	Arena->Step(D, Camera->GetActorLocation());

}

void ASlopMatch::FixedTick()
{
	if (KoFreeze > 0 && !bDebugPause)
	{
		// The deciding blow hangs in the air; the pads are left alone so presses stay latched.
		--KoFreeze;
		return;
	}
	if (!bDebugPause && (bDebugSlow || KoSlow > 0))
	{
		++SlowCount;
		if (KoSlow > 0)
		{
			--KoSlow;
		}
		if (SlowCount % (bDebugSlow ? 4 : 3) != 0)
		{
			return;
		}
	}
	USlopGameInstance* G = GI();
	FPadState Pads[2] = {G->Router->Poll(0), G->Router->Poll(1)};
	for (int32 I = 0; I < 2; ++I)
	{
		if (Brains[I])
		{
			Pads[I] = Brains[I]->Think();
		}
	}
	if (Dummy && Dummy->TakesPlayerPad())
	{
		// Recording the dummy: the player's own pad drives it while their fighter stands.
		Pads[1] = Dummy->Record(Pads[0]);
		Pads[0] = FPadState();
	}
	if (Guide)
	{
		Guide->FilterPads(Pads);
	}
	const double T0 = FPlatformTime::Seconds();
	Sim.Step(Pads);
	Advantage.Update(Sim);
	SimMicros += (FPlatformTime::Seconds() - T0) * 1e6;
	++TicksRun;
	++SteppedThisFrame;
	LastPads[0] = Pads[0];
	LastPads[1] = Pads[1];
	for (int32 I = 0; I < 2; ++I)
	{
		if (Views[I]->BoundDef != Sim.Fighters[I].Def)
		{
			RebindView(I);
		}
		else
		{
			Views[I]->SyncFromSim();
		}
	}
	for (const FSimEvent& E : Sim.Events)
	{
		FrameEvents.Add(E);
		HandleEvent(E);
		if (Guide)
		{
			Guide->OnEvent(E);
		}
		if (OnEvent)
		{
			OnEvent(E);
		}
	}
	if (Guide)
	{
		Guide->OnTick();
	}
}

void ASlopMatch::DressView(int32 I)
{
	ASlopFighterActor* V = Views[I];
	const FFighter& F = Sim.Fighters[I];
	const FLinearColor Alt(0.55f, 0.75f, 1.1f);
	if (Config.bMirrorDark && I == 1)
	{
		V->SetTint(FLinearColor(0.06f, 0.1f, 0.05f), 0.72f, 0.6f);
	}
	if (F.Form == EForm::Borrowed)
	{
		// A borrowed shape is lit from inside by the Grandmaster's Qhi: a strong gold rim over the
		// other fighter's own colours, so who is who never needs guessing.
		V->SetAccent(FLinearColor(FColor::FromHex(Json::Str(F.BaseDef->Look, TEXT("glow"), TEXT("#ffcf5a")))), 0.14f);
		return;
	}
	if (F.Form == EForm::Spent)
	{
		// Spent: the Qhi's rim has gone out.
		V->SetAccent(FLinearColor::Black, 0.f);
	}
	else
	{
		V->SetAccent(FLinearColor(FColor::FromHex(Json::Str(F.BaseDef->Look, TEXT("glow"), TEXT("#ffffff")))), 0.3f * (float)Json::Num(F.BaseDef->Look, TEXT("accent"), 0.24));
	}
	if ((Config.P1 == Config.P2 && I == 1 && !Config.bMirrorDark) || (Config.bCostume[I] && !(I == 1 && Config.P1 == Config.P2)))
	{
		// Mirror matches: the second player wears the alternate colours.
		V->SetCostume(Alt);
	}
}

void ASlopMatch::RebindView(int32 I)
{
	const FName WasBody = Views[I]->BoundBody;
	if (!Views[I]->Rebind(Sim.Fighters[I]))
	{
		UE_LOG(LogSlop, Error, TEXT("No body for %s's shape %s"), *Sim.Fighters[I].BaseDef->Id.ToString(), *Sim.Fighters[I].Def->Id.ToString());
		return;
	}
	Views[I]->bSweats = true;
	ApplySettings();
	DressView(I);
	if (Views[I]->BoundBody != WasBody)
	{
		FormFade[I] = 1.f;
	}
}

void ASlopMatch::FinishFromGuide(int32 Winner)
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	Announce(NSLOCTEXT("SlopMatch", "WellFought", "WELL FOUGHT"), NSLOCTEXT("SlopMatch", "Awaits", "The Sump awaits."));
	Result = FMatchResult();
	Result.Winner = Winner;
	Result.P1 = Sim.Fighters[0].BaseDef->Id;
	Result.P2 = Sim.Fighters[1].BaseDef->Id;
	Result.Mode = Config.Mode;
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (OnFinished)
		{
			OnFinished(Result);
		}
	}), 2.8f, false);
}

void ASlopMatch::HandleEvent(const FSimEvent& E)
{
	Vfx->OnEvent(E);
	Director.OnEvent(E);
	PlayEventSound(E);
	switch (E.Type)
	{
	case EEvent::Hit:
	{
		const FFighter& V = Sim.Fighters[E.Who];
		const FFighter& A = Sim.Fighters[E.By];
		const double Dir = FMath::Sign(V.Pos.X - A.Pos.X);
		const double Strength = FMath::Clamp(E.Damage / 100.0, 0.15, 1.3);
		const bool bHeavy = E.Effect == TEXT("launch") || E.Effect == TEXT("knockdown") || E.Effect == TEXT("sweep") || E.Effect == TEXT("crumple") || E.bThrow;
		Views[E.Who]->Anim.Impact(Dir, Strength, E.Level, false, E.bProj ? FVector::ZeroVector : Views[E.By]->Anim.StrikeDirection(), E.bCounter);
		if (!E.bThrow)
		{
			Views[E.By]->Anim.Recoil(Strength);
		}
		// A throw is taken on the back and shoulders.
		Views[E.Who]->TakeBlow(E.bThrow ? 1 : Views[E.Who]->Anim.RegionAt(E.Point, E.Level), E.Damage);
		// The struck player's pad feels it: in weight with the blow, longest for a knockout.
		GI()->Router->Rumble(E.Who, E.bKO ? 1.f : (float)FMath::Clamp(0.2 + Strength * 0.5, 0.2, 0.8), E.bKO ? 0.45f : (bHeavy ? 0.22f : 0.12f));
		// Light and medium blows leave the camera still (a realistic side-on view reads best steady);
		// heavy ones and counters kick it along the blow, and only blows that put a body down or
		// send it flying shake it.
		if (bHeavy || E.bCounter || Strength >= 0.8)
		{
			Camera->Kick(FVector(Dir, bHeavy ? -0.25 : 0.0, 0.0), 0.02 + Strength * 0.045 + (bHeavy ? 0.02 : 0.0));
		}
		if (bHeavy)
		{
			Camera->AddShake(0.008 + Strength * 0.016);
		}
		break;
	}
	case EEvent::Block:
		Views[E.Who]->Anim.Impact(FMath::Sign(Sim.Fighters[E.Who].Pos.X - Sim.Fighters[E.By].Pos.X), 0.35, ELevel::Mid, true);
		Views[E.By]->Anim.Recoil(0.5);
		break;
	case EEvent::Parry:
	{
		// A caught blow: the guard takes it and the attacker is thrown back off it, open. A shin
		// check bruises the kicking leg.
		const double Dir = FMath::Sign(Sim.Fighters[E.By].Pos.X - Sim.Fighters[E.Who].Pos.X);
		Views[E.Who]->Anim.Impact(-Dir, 0.3, ELevel::Mid, true);
		Views[E.By]->Anim.Impact(Dir, 0.6, E.Level == ELevel::Low ? ELevel::Low : ELevel::Mid, false);
		Views[E.By]->Anim.Recoil(1.0);
		Views[E.By]->TakeBlow(E.Level == ELevel::Low ? 2 : 1, E.Damage);
		Camera->Kick(FVector(Dir, 0.0, 0.0), 0.03);
		break;
	}
	case EEvent::Armor:
		Camera->AddShake(0.012);
		break;
	case EEvent::BossCue:
		Camera->AddShake(E.Kind == TEXT("cine") ? 0.012 : 0.03);
		if (E.Kind == TEXT("cine"))
		{
			Camera->PunchIn(0.34, E.Who);
			CineT = 1.7;
		}
		break;
	case EEvent::Form:
		if (E.Kind == TEXT("borrowed") || E.Kind == TEXT("spent") || E.Kind == TEXT("broken"))
		{
			Camera->AddShake(E.Kind == TEXT("borrowed") ? 0.012 : 0.008);
			GI()->Router->Rumble(E.Who, 0.35f, 0.18f);
		}
		break;
	case EEvent::Throw:
		Camera->AddShake(0.008);
		break;
	case EEvent::Clash:
		Camera->AddShake(0.02);
		break;
	case EEvent::KO:
		// The deciding blow freezes the fight, the camera closes in on the beaten fighter, and
		// the fall plays in slow motion.
		KoFreeze = E.bFinish ? 18 : 14;
		KoSlow = E.bFinish ? 34 : 50;
		Camera->AddShake(0.07);
		Camera->PunchIn(E.bFinish ? 0.32 : 0.26, E.Who);
		break;
	case EEvent::FinishPrompt:
		// The finish is played at the normal framing, so the distance can be judged.
		Camera->ReleaseZoom();
		break;
	case EEvent::RoundEnd:
		if (E.Winner == 0)
		{
			TimeLeft += Sim.TimerSeconds();
		}
		break;
	case EEvent::RoundStart:
		Camera->ReleaseZoom();
		KoFreeze = 0;
		KoSlow = 0;
		for (ASlopFighterActor* V : Views)
		{
			V->SyncFromSim(true);
		}
		// After the staging the camera is already travelling to the fight: a snap here would jump there
		// and swing back.
		if (!bHandingOver)
		{
			Camera->Snap();
		}
		bHandingOver = false;
		break;
	case EEvent::MatchEnd:
		if (!bEnded)
		{
			bEnded = true;
#if !UE_BUILD_SHIPPING
			// Development: how a Qhi fighter's CPU used its meter this match.
			for (int32 I = 0; I < 2; ++I)
			{
				if (CpuBrains[I] && Sim.Fighters[I].UsesQhi() && FParse::Param(FCommandLine::Get(), TEXT("SlopAiLog")))
				{
					UE_LOG(LogTemp, Display, TEXT("SlopAiLog %s (side %d): %s"), *Sim.Fighters[I].BaseDef->Id.ToString(), I, *CpuBrains[I]->Telemetry().Summary());
				}
			}
#endif
			Result.Winner = E.Winner;
			Result.Wins[0] = Sim.Wins[0];
			Result.Wins[1] = Sim.Wins[1];
			Result.P1 = Sim.Fighters[0].BaseDef->Id;
			Result.P2 = Sim.Fighters[1].BaseDef->Id;
			Result.Finisher = E.Move;
			Result.bFlawless = E.bFlawless;
			Result.Mode = Config.Mode;
			Result.Health[0] = Sim.Fighters[0].Health;
			Result.Health[1] = Sim.Fighters[1].Health;
			Result.TimeLeft = TimeLeft;
			// The scene stays a moment on the result before the screen changes.
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (OnFinished)
				{
					OnFinished(Result);
				}
			}), 1.2f, false);
		}
		break;
	default:
		break;
	}
}

float ASlopMatch::ScreenPan(int32 Side) const
{
	if (Side < 0 || Side > 1 || !Camera)
	{
		return 0.f;
	}
	const double Half = FMath::Max(Camera->HalfWidthAt(FMath::Max(Camera->Pos.Z, 1.0)), 0.1);
	return (float)FMath::Clamp((Sim.Fighters[Side].Pos.X - Camera->LookAt.X) / Half, -1.0, 1.0);
}

void ASlopMatch::PlayEventSound(const FSimEvent& E)
{
	USlopAudio* A = GI()->Audio;
	const float R = FMath::FRandRange(0.95f, 1.05f);
	// Whatever a fighter does is heard from where they stand. Calls, gongs and the like take the centre.
	const float Here = ScreenPan(E.Who);
	switch (E.Type)
	{
	case EEvent::Move:
		if (const FMoveDef* M = Sim.Fighters[E.Who].Def->Move(E.Move))
		{
			const FString Sw = M->Swing.IsNone() ? FString() : M->Swing.ToString();
			if (Sw == TEXT("light")) A->PlayAt(Here, TEXT("whoosh_light"), -9.f, FMath::FRandRange(0.94f, 1.08f));
			else if (Sw == TEXT("medium")) A->PlayAt(Here, TEXT("whoosh_medium"), -8.f, FMath::FRandRange(0.94f, 1.06f));
			else if (Sw == TEXT("heavy")) A->PlayAt(Here, TEXT("whoosh_heavy"), -7.f, FMath::FRandRange(0.94f, 1.04f));
			else if (!Sw.IsEmpty()) A->PlayAt(Here, Sw, -3.f);
		}
		break;
	case EEvent::Hit:
		if (E.bThrow)
		{
			A->PlayAt(Here, TEXT("slam"), 0.f);
		}
		else
		{
			// The synthesised body of the blow, pitched by where it lands (a face hit sharp, a body
			// hit deeper, a leg kick low), and a recorded contact for what lands it: a fist or shin
			// on the face, a thud in the body, or the weapon's own wood, brass or steel. The Kenney
			// files are all peak-normalised, so the short wood and metal hits measure 4-10 dB under
			// the punches (Tools/audio_levels.py); they are raised as far as their peaks allow.
			const FMoveDef* M = Sim.Fighters[E.By].Def->Move(E.Move);
			const FName Mat = M ? M->HitSound : NAME_None;
			const bool bHeavy = E.Damage >= 70;
			const float Pitch = E.Level == ELevel::Low ? 0.86f : (E.Level == ELevel::Mid ? 0.94f : 1.05f);
			A->PlayAt(Here, E.Damage < 55 ? TEXT("hit_light") : (E.Damage < 95 ? TEXT("hit_medium") : TEXT("hit_heavy")), -2.f, R * Pitch);
			if (Mat == TEXT("wood"))
			{
				A->PlayAt(Here, bHeavy ? TEXT("kenney:impactWood_heavy") : TEXT("kenney:impactWood_medium"), -1.f, R);
			}
			else if (Mat == TEXT("metal"))
			{
				A->PlayAt(Here, bHeavy ? TEXT("kenney:impactMetal_heavy") : TEXT("kenney:impactPlate_medium"), -1.f, R);
			}
			else if (Mat == TEXT("blade"))
			{
				A->PlayAt(Here, TEXT("kenney:impactMetal_light"), -3.f, R * 1.15f);
				A->PlayAt(Here, TEXT("kenney:impactPunch_medium"), -7.f, R);
			}
			else if (Mat == TEXT("chain"))
			{
				A->PlayAt(Here, TEXT("kenney:impactMetal_medium"), -2.f, R * 1.1f);
			}
			else if (E.Level == ELevel::High)
			{
				A->PlayAt(Here, bHeavy ? TEXT("kenney:impactPunch_heavy") : TEXT("kenney:impactPunch_medium"), -3.f, R * 1.04f);
			}
			else if (E.Level == ELevel::Mid)
			{
				A->PlayAt(Here, TEXT("kenney:impactSoft_heavy"), -3.f, R * 0.95f);
				A->PlayAt(Here, TEXT("kenney:impactPunch_medium"), -8.f, R * 0.9f);
			}
			else
			{
				A->PlayAt(Here, TEXT("kenney:impactSoft_medium"), -3.f, R * 0.88f);
			}
		}
		if (E.bProj)
		{
			A->PlayAt(Here, TEXT("burst"), -4.f);
		}
		break;
	case EEvent::Block:
	{
		// A guarded blow: the arms take it, or the weapon's own material on the forearms.
		const FMoveDef* M = Sim.Fighters[E.By].Def->Move(E.Move);
		const FName Mat = M ? M->HitSound : NAME_None;
		A->PlayAt(Here, TEXT("block"), -3.f, R);
		const bool bWeapon = Mat == TEXT("wood") || Mat == TEXT("metal") || Mat == TEXT("blade") || Mat == TEXT("chain");
		A->PlayAt(Here, Mat == TEXT("wood") ? TEXT("kenney:impactWood_light") : (bWeapon ? TEXT("kenney:impactPlate_light") : TEXT("kenney:impactSoft_medium")),
			bWeapon ? -3.f : -6.f, R);
		break;
	}
	case EEvent::Armor: A->PlayAt(Here, TEXT("armor"), -2.f); break;
	case EEvent::Parry:
		if (E.Kind == TEXT("ward"))
		{
			A->PlayAt(Here, TEXT("bell"), -6.f, 1.4f);
			break;
		}
		if (E.Kind == TEXT("weave"))
		{
			A->PlayAt(Here, TEXT("whoosh_light"), -6.f, 0.85f);
			break;
		}
		// The guard's slap with a hard knock over it; a shin check is bone on bone, duller and lower.
		A->PlayAt(Here, TEXT("block"), -2.f, 0.9f);
		A->PlayAt(Here, E.Level == ELevel::Low ? TEXT("kenney:impactPunch_heavy") : TEXT("kenney:impactWood_medium"), -4.f, E.Level == ELevel::Low ? 0.8f : 1.1f);
		break;
	case EEvent::Throw: A->PlayAt(Here, TEXT("grab"), -2.f); break;
	case EEvent::ThrowBreak: A->PlayAt(Here, TEXT("throw_break"), 0.f); break;
	case EEvent::Land:
		if (E.bSoft)
		{
			A->PlayAt(Here, Arena->Footstep, -14.f + Arena->FootstepDb, R);
		}
		else
		{
			A->PlayAt(Here, TEXT("land"), -4.f);
			A->PlayAt(Here, TEXT("kenney:impactSoft_heavy"), -6.f, R);
		}
		break;
	case EEvent::Projectile:
		// Two kinds have no cast of their own (silent in the original too): Gorm's quake gets the
		// ground crumbling as the wave runs out from the slap, the Fragment's gust a far rush of air.
		// The Grandmaster's palms sound as the orb and the wave.
		A->PlayAt(Here, E.Kind == TEXT("quake") ? FString(TEXT("crumble")) : E.Kind == TEXT("gust") ? FString(TEXT("far_whoosh"))
			: E.Kind == TEXT("qhi") ? FString(TEXT("cast_orb")) : E.Kind == TEXT("qhi_low") ? FString(TEXT("cast_wave")) : TEXT("cast_") + E.Kind.ToString(), -4.f);
		break;
	case EEvent::Clash: A->PlayAt(Here, TEXT("burst"), -2.f); break;
	case EEvent::BossCue:
		// The Vessel filling, and breaking; the bell on a sequence that has caught someone.
		if (E.Kind == TEXT("vessel")) { A->PlayAt(Here, TEXT("gong"), 0.f, 0.35f); A->PlayAt(Here, TEXT("cast_wave"), -3.f, 0.4f); }
		else if (E.Kind == TEXT("vessel_broken")) { A->PlayAt(Here, TEXT("burst"), 0.f, 0.7f); A->PlayAt(Here, TEXT("bell"), -3.f, 0.6f); }
		else if (E.Kind == TEXT("cine")) { A->PlayAt(Here, TEXT("bell"), -6.f, 0.5f); }
		else if (E.Kind == TEXT("flow")) { A->PlayAt(Here, TEXT("cast_wave"), -2.f, 0.5f); A->PlayAt(Here, TEXT("gong"), -5.f, 0.4f); }
		break;
	case EEvent::Form:
		// A bell for the Qhi taking a shape or letting it go; dust and a low gong when it gives out.
		if (E.Kind == TEXT("borrowed")) { A->PlayAt(Here, TEXT("bell"), -2.f, 0.8f); A->PlayAt(Here, TEXT("cast_wave"), -6.f, 0.7f); }
		else if (E.Kind == TEXT("own")) A->PlayAt(Here, TEXT("bell"), -5.f, 1.05f);
		else if (E.Kind == TEXT("spent") || E.Kind == TEXT("broken")) { A->PlayAt(Here, TEXT("gong"), -4.f, 0.55f); A->PlayAt(Here, TEXT("crumble"), -6.f, 0.9f); }
		else if (E.Kind == TEXT("recentred")) A->PlayAt(Here, TEXT("bell"), -8.f, 1.25f);
		else if (E.Kind == TEXT("failed")) A->PlayAt(Here, TEXT("burst"), -6.f, 0.7f);
		else if (E.Kind == TEXT("refused")) A->PlayAt(Here, TEXT("ui_locked"), -8.f, 0.9f, ESlopBus::Ui);
		break;
	case EEvent::Jump: A->PlayAt(Here, Arena->Footstep, -12.f + Arena->FootstepDb, 1.1f); break;
	case EEvent::KO: A->Play(TEXT("ko_hit"), 0.f); break;
	case EEvent::RoundStart: A->Play(TEXT("gong"), -4.f); break;
	case EEvent::Fight: A->Play(TEXT("fight"), -2.f); break;
	case EEvent::FinishPrompt: A->Play(TEXT("finish"), -1.f); break;
	case EEvent::FinisherEnd: A->Play(TEXT("sloptality"), -2.f); break;
	case EEvent::WinPose: A->Play(TEXT("victory"), -6.f, 1.f, ESlopBus::Effects); break;
	case EEvent::RoundEnd:
		if (E.How == TEXT("timeout"))
		{
			A->Play(TEXT("gong"), -6.f, 0.8f);
		}
		break;
	default:
		break;
	}
}

void ASlopMatch::CollectBoxes(TArray<TPair<FRect, FLinearColor>>& Out) const
{
	TArray<FRect> Hurt;
	TArray<TPair<int32, FRect>> Hits;
	for (const FFighter& F : Sim.Fighters)
	{
		F.Hurtboxes(Hurt);
		for (const FRect& R : Hurt)
		{
			Out.Add({R, FLinearColor(FColor(80, 160, 255))});
		}
		F.ActiveHitboxes(Hits);
		for (const TPair<int32, FRect>& H : Hits)
		{
			Out.Add({H.Value, FLinearColor(FColor(255, 60, 40))});
		}
		Out.Add({F.Pushbox(), FLinearColor(FColor(255, 220, 80))});
	}
	for (const FProjectile& P : Sim.Projectiles)
	{
		Out.Add({P.Rect(), FLinearColor(FColor(255, 120, 40))});
	}
}
