#include "View/SlopMatch.h"
#include "DrawDebugHelpers.h"
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
	const bool bPractice = M == EMode::Training || M == EMode::Tutorial || M == EMode::Stranger || M == EMode::Trials;
	Sim.bTraining = bPractice;
	Sim.RoundsToWin = FMath::Max(1, Config.RoundsToWin);
	Sim.bFinishersEnabled = M != EMode::Stranger;
	if (D1->Move(TEXT("stage_vat")) && Config.Stage == TEXT("rendering_yards"))
	{
		Sim.StageFinisher = TEXT("stage_vat");
	}
	Sim.bIntro = !bPractice;
	Sim.Setup(D1, D2);
	if ((M == EMode::Arcade || M == EMode::Survival) && Config.P2Health > 0)
	{
		// The arcade sets the Slopbat's health by tower.
		Sim.Fighters[1].HealthMax = Config.P2Health;
		Sim.Fighters[1].Health = Config.P2Health;
	}
	if (M == EMode::Survival && Config.P1Health > 0)
	{
		Sim.Fighters[0].Health = FMath::Min(Sim.Fighters[0].MaxHealth(), Config.P1Health);
	}
	if (M == EMode::Stranger)
	{
		// No announcer and no rounds: he is simply there.
		Sim.Phase = EPhase::Fight;
		for (FFighter& F : Sim.Fighters)
		{
			F.SetState(EState::Idle);
		}
	}
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Arena = W->SpawnActor<ASlopArena>(P);
	Arena->bStrobe = GI()->Settings.bFlashes;
	Arena->Build(Config.Stage, GI()->Settings.Graphics != TEXT("low"));
	for (int32 I = 0; I < 2; ++I)
	{
		Views[I] = W->SpawnActor<ASlopFighterActor>(P);
		if (!Views[I]->Setup(Sim.Fighters[I], USlopGameInstance::MeshFor(Sim.Fighters[I].Def->Id)))
		{
			return false;
		}
	}
	if (Config.bMirrorDark)
	{
		Views[1]->SetTint(FLinearColor(0.06f, 0.1f, 0.05f), 0.72f, 0.6f);
	}
	else if (Config.P1 == Config.P2)
	{
		// Mirror matches: the second player wears the alternate colours.
		Views[1]->SetCostume(FLinearColor(0.55f, 0.75f, 1.1f));
	}
	for (int32 I = 0; I < 2; ++I)
	{
		if (Config.bCostume[I] && !(I == 1 && Config.P1 == Config.P2))
		{
			Views[I]->SetCostume(FLinearColor(0.55f, 0.75f, 1.1f));
		}
	}
	Camera = W->SpawnActor<ASlopFightCamera>(P);
	Camera->Bind(&Sim);
	Camera->Snap();
	Vfx = W->SpawnActor<ASlopVfx>(P);
	Vfx->Setup(this);
	Director.Setup(this);
	ApplySettings();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(Camera);
		PC->SetShowMouseCursor(false);
	}
	if (M == EMode::Training || M == EMode::Trials)
	{
		TUniquePtr<FTrainingDummy> D = MakeUnique<FTrainingDummy>(Sim, 1);
		Dummy = D.Get();
		Brains[1] = MoveTemp(D);
		Sim.bRefillHealth = M == EMode::Training;
		Sim.bInfiniteRun = M == EMode::Trials;
	}
	else if (M == EMode::Stranger)
	{
		USlopGameInstance* G = GI();
		const FString Bow = Json::Str(Json::Obj(GetContent().Story, TEXT("stranger")), TEXT("bow"));
		Stranger = MakeUnique<FStrangerRules>(Sim, G->Progress.Stranger(), Bow, [G](int32 Stage) { G->Progress.SetStranger(Stage); });
		Sim.Hooks = Stranger.Get();
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
				else
				{
					Brains[I] = MakeUnique<FCpuBrain>(Sim, I, Diff, (int32)(FPlatformTime::Cycles() & 0x7fff) + I * 101);
				}
			}
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
	UE_LOG(LogSlop, Log, TEXT("Match: %s v %s, %s, %s, difficulty %d"), *Config.P1.ToString(), *Config.P2.ToString(), ModeName(M), *Config.Stage.ToString(), Config.Difficulty);
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
	Sim.Hooks = nullptr;
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
	GI()->Router->ResetAll();
	GI()->Router->bFightActive = !bPause;
	GI()->Audio->PauseMusic(bPause);
}

void ASlopMatch::ResetPositions()
{
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
	Camera->Snap();
}

void ASlopMatch::Tick(float DeltaSeconds)
{
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
	Camera->Step(bPaused ? 0.0 : DeltaSeconds);
	Director.Step(D);
	Vfx->Step(D);
	Arena->Step(D, Camera->GetActorLocation());
	if (bShowBoxes)
	{
		DrawDebugBoxes();
	}
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
	if (Guide)
	{
		Guide->FilterPads(Pads);
	}
	const double T0 = FPlatformTime::Seconds();
	Sim.Step(Pads);
	SimMicros += (FPlatformTime::Seconds() - T0) * 1e6;
	++TicksRun;
	++SteppedThisFrame;
	LastPads[0] = Pads[0];
	LastPads[1] = Pads[1];
	for (ASlopFighterActor* V : Views)
	{
		V->SyncFromSim();
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
	Result.P1 = Sim.Fighters[0].Def->Id;
	Result.P2 = Sim.Fighters[1].Def->Id;
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
		Views[E.Who]->Anim.Impact(Dir, Strength, E.Level, false);
		// Every blow moves the camera a little; the big ones move it a lot, along the blow.
		Camera->AddShake(0.004 + Strength * 0.022 + (bHeavy ? 0.014 : 0.0) + (E.bCounter ? 0.01 : 0.0));
		Camera->Kick(FVector(Dir, bHeavy ? -0.25 : 0.0, 0.0), 0.02 + Strength * 0.045 + (bHeavy ? 0.02 : 0.0));
		break;
	}
	case EEvent::Block:
		Views[E.Who]->Anim.Impact(FMath::Sign(Sim.Fighters[E.Who].Pos.X - Sim.Fighters[E.By].Pos.X), 0.35, ELevel::Mid, true);
		Camera->AddShake(0.004);
		break;
	case EEvent::Armor:
		Camera->AddShake(0.012);
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
		Camera->Snap();
		break;
	case EEvent::MatchEnd:
		if (!bEnded)
		{
			bEnded = true;
			Result.Winner = E.Winner;
			Result.Wins[0] = Sim.Wins[0];
			Result.Wins[1] = Sim.Wins[1];
			Result.P1 = Sim.Fighters[0].Def->Id;
			Result.P2 = Sim.Fighters[1].Def->Id;
			Result.Finisher = E.Move;
			Result.bFlawless = E.bFlawless;
			Result.Mode = Config.Mode;
			Result.StrangerLine = !E.Text.IsEmpty() ? E.Text : (Stranger ? Stranger->LastLine() : FString());
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

void ASlopMatch::PlayEventSound(const FSimEvent& E)
{
	USlopAudio* A = GI()->Audio;
	const float R = FMath::FRandRange(0.95f, 1.05f);
	switch (E.Type)
	{
	case EEvent::Move:
		if (const FMoveDef* M = Sim.Fighters[E.Who].Def->Move(E.Move))
		{
			const FString Sw = M->Swing.ToString();
			if (Sw == TEXT("light")) A->Play(TEXT("whoosh_light"), -9.f, FMath::FRandRange(0.94f, 1.08f));
			else if (Sw == TEXT("medium")) A->Play(TEXT("whoosh_medium"), -8.f, FMath::FRandRange(0.94f, 1.06f));
			else if (Sw == TEXT("heavy")) A->Play(TEXT("whoosh_heavy"), -7.f, FMath::FRandRange(0.94f, 1.04f));
			else if (!Sw.IsEmpty()) A->Play(Sw, -3.f);
		}
		break;
	case EEvent::Hit:
		if (E.bThrow)
		{
			A->Play(TEXT("slam"), 0.f);
		}
		else
		{
			// A recorded punch for the contact over the synthesised body of the blow.
			A->Play(E.Damage < 55 ? TEXT("hit_light") : (E.Damage < 95 ? TEXT("hit_medium") : TEXT("hit_heavy")), -2.f, R);
			A->Play(E.Damage < 70 ? TEXT("kenney:impactPunch_medium") : TEXT("kenney:impactPunch_heavy"), -3.f, R);
		}
		if (E.bProj)
		{
			A->Play(TEXT("burst"), -4.f);
		}
		break;
	case EEvent::Block:
		A->Play(TEXT("block"), -3.f, R);
		A->Play(TEXT("kenney:impactSoft_medium"), -6.f, R);
		break;
	case EEvent::Armor: A->Play(TEXT("armor"), -2.f); break;
	case EEvent::Throw: A->Play(TEXT("grab"), -2.f); break;
	case EEvent::ThrowBreak: A->Play(TEXT("throw_break"), 0.f); break;
	case EEvent::Land:
		if (E.bSoft)
		{
			A->Play(Arena->Footstep, -14.f, R);
		}
		else
		{
			A->Play(TEXT("land"), -4.f);
			A->Play(TEXT("kenney:impactSoft_heavy"), -6.f, R);
		}
		break;
	case EEvent::Projectile: A->Play(TEXT("cast_") + E.Kind.ToString(), -4.f); break;
	case EEvent::Clash: A->Play(TEXT("burst"), -2.f); break;
	case EEvent::Jump: A->Play(Arena->Footstep, -12.f, 1.1f); break;
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

void ASlopMatch::DrawDebugBoxes() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* W = GetWorld();
	auto Box = [W](const FRect& R, const FColor& C)
	{
		const FVector Center = Anim::WorldToUE(FVector(R.X + R.W * 0.5, R.Y + R.H * 0.5, 0.0));
		const FVector Extent(2.0, R.W * 50.0, R.H * 50.0);
		DrawDebugBox(W, Center, Extent, C, false, -1.f, SDPG_Foreground, 1.5f);
	};
	TArray<FRect> Hurt;
	TArray<TPair<int32, FRect>> Hits;
	for (const FFighter& F : Sim.Fighters)
	{
		F.Hurtboxes(Hurt);
		for (const FRect& R : Hurt)
		{
			Box(R, FColor(80, 160, 255));
		}
		F.ActiveHitboxes(Hits);
		for (const TPair<int32, FRect>& H : Hits)
		{
			Box(H.Value, FColor(255, 60, 40));
		}
		Box(F.Pushbox(), FColor(255, 220, 80));
	}
	for (const FProjectile& P : Sim.Projectiles)
	{
		Box(P.Rect(), FColor(255, 120, 40));
	}
#endif
}
