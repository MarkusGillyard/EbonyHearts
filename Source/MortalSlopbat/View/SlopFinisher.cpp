#include "View/SlopFinisher.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Anim/SlopMeshRig.h"
#include "Anim/SlopPose.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"
#include "View/SlopFightCamera.h"
#include "View/SlopFighterActor.h"
#include "View/SlopMatch.h"
#include "View/SlopVfx.h"

namespace Slop
{
	void FFinisherDirector::Setup(ASlopMatch* InMatch)
	{
		M = InMatch;
		// The slop pool Nix drags his victim into.
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(M);
		P->SetupAttachment(M->GetRootComponent());
		P->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		P->SetMobility(EComponentMobility::Movable);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_GlowOpaque.M_GlowOpaque")), M);
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.55f, 0.08f));
		Mat->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
		P->SetMaterial(0, Mat);
		P->SetVisibility(false);
		P->RegisterComponent();
		Pool = P;
	}

	double FFinisherDirector::EaseRange(double A, double B, double T)
	{
		return Anim::FPose::Ease(TEXT("inout"), FMath::Clamp((T - A) / FMath::Max(B - A, 1.0), 0.0, 1.0));
	}

	bool FFinisherDirector::Once(const TCHAR* Key, double At)
	{
		if (T >= At && !Fired.Contains(Key))
		{
			Fired.Add(Key);
			return true;
		}
		return false;
	}

	void FFinisherDirector::OnEvent(const FSimEvent& E)
	{
		switch (E.Type)
		{
		case EEvent::Finisher:
			bActive = true;
			MoveId = E.Move;
			Who = E.Who;
			T = 0;
			Fired.Reset();
			M->Camera->ReleaseZoom();
			break;
		case EEvent::RoundStart:
			Reset();
			break;
		case EEvent::KO:
			if (E.bBoss)
			{
				Boss = E.Who;
				BossT = 0;
			}
			break;
		case EEvent::Hit:
			if (E.bKill)
			{
				// An instant kill lands in slow motion, close: the touch, the moment and the blow.
				M->KoSlowExternal(45);
				M->Camera->AddShake(0.06);
				M->Vfx->FlashAt(FVector(E.Point.X, E.Point.Y, 0.2), FLinearColor(1.f, 0.82f, 0.45f), 30.f);
				M->GetGI()->Audio->Play(TEXT("palm_strike"), 2.f);
				M->GetGI()->Audio->Play(TEXT("bell"), -3.f, 0.6f);
				KillBy = E.By;
				KillT = 0;
			}
			break;
		default:
			break;
		}
	}

	void FFinisherDirector::Reset()
	{
		bActive = false;
		BossT = -1;
		KillT = -1;
		if (Pool.IsValid())
		{
			Pool->SetVisibility(false);
		}
		for (int32 I = 0; I < 2; ++I)
		{
			ASlopFighterActor* V = M->Views[I];
			V->Anim.ExtraScale = FVector::OneVector;
			V->Anim.Sink = 0;
			V->Anim.LiftExtra = FVector::ZeroVector;
			V->Anim.SpinExtra = 0;
			V->Anim.PoseOverride = NAME_None;
			V->SetDissolve(0.f);
			V->SetHidden(false);
			if (!(M->Config.bMirrorDark && I == 1))
			{
				V->SetTint(FLinearColor::Black, 0.f);
			}
		}
		M->Camera->ClearOverride();
	}

	void FFinisherDirector::CloseUp(const FVector& Mid, double Face, double Dist, double From, double To)
	{
		if (T >= From && T <= To)
		{
			M->Camera->SetOverride(Mid + FVector(-Face * 0.6, 0.3, Dist), Mid + FVector(0, 0.1, 0), 1.0);
		}
		else if (T > To)
		{
			M->Camera->ClearOverride();
		}
	}

	void FFinisherDirector::Step(double Delta)
	{
		BossStep(Delta);
		KillStep(Delta);
		if (!bActive || M->Sim.FinisherWho < 0)
		{
			return;
		}
		USlopAudio* Audio = M->GetGI()->Audio;
		ASlopVfx* Fx = M->Vfx;
		const FFighter& F = M->Sim.Fighters[Who];
		const FFighter& VF = M->Sim.Fighters[1 - Who];
		ASlopFighterActor* V = M->Views[1 - Who];
		ASlopFighterActor* A = M->Views[Who];
		T = (double)F.MF;
		const double Face = (double)F.Facing;
		const FVector Mid((F.Pos.X + VF.Pos.X) * 0.5, 1.0, 0.0);
		const FVector VPos(VF.Pos.X, VF.Pos.Y, 0.0);
		const bool bGore = Fx->Gore != TEXT("off");
		const FString Id = MoveId.ToString();
		Anim::FAnimator& VA = V->Anim;
		if (Id == TEXT("last_light"))
		{
			CloseUp(Mid, Face, 3.4, 0, 200);
			if (T > 6 && T < 40 && (int32)T % 3 == 0)
			{
				Fx->BurstColor(A->Anim.StrikePoint(TEXT("hand_l")), FLinearColor(1.f, 0.55f, 0.15f), 6, 1.0, 0.4);
			}
			if (Once(TEXT("ignite"), 40))
			{
				Audio->Play(TEXT("ignite"), 0.f);
				Fx->FlashAt(VPos + FVector(0, 1.2, 0.3), FLinearColor(1.f, 0.6f, 0.2f), 40.f);
			}
			if (T > 40)
			{
				V->SetTint(FLinearColor(0.05f, 0.035f, 0.03f), (float)FMath::Clamp((T - 40) / 70, 0.0, 1.0));
				V->SetDissolve((float)FMath::Clamp((T - 85) / 90, 0.0, 1.0), FLinearColor(1.f, 0.45f, 0.1f));
				if ((int32)T % 4 == 0 && T < 190)
				{
					Fx->BurstColor(VPos + FVector(FMath::FRandRange(-0.2, 0.2), 0.3 + FMath::FRand() * 1.5, 0.2), FLinearColor(1.f, 0.5f, 0.15f), 14, 2.2, 0.7);
				}
			}
			if (Once(TEXT("ash"), 170))
			{
				Audio->Play(TEXT("crumble"), -2.f);
				Fx->DustAt(VPos);
			}
		}
		else if (Id == TEXT("rendered_down"))
		{
			CloseUp(Mid, Face, 4.4, 0, 250);
			const FVector Over(F.Pos.X + 0.25 * Face, F.Pos.Y + 2.1 * F.Scale(), 0.0);
			const FVector Front(F.Pos.X + 1.0 * Face * F.Scale(), 0.0, 0.0);
			if (T < 30)
			{
				VA.LiftExtra = (Over - VPos) * EaseRange(8, 30, T);
				VA.PoseOverride = T < 14 ? FName(TEXT("thrown_hold")) : FName(TEXT("thrown_lift"));
			}
			else if (T < 86)
			{
				const double K = EaseRange(70, 86, T);
				VA.LiftExtra = FMath::Lerp(Over - VPos, Front - VPos, K);
				VA.PoseOverride = K < 0.6 ? FName(TEXT("thrown_lift")) : FName(TEXT("down"));
			}
			else
			{
				VA.LiftExtra = Front - VPos;
				VA.PoseOverride = TEXT("down");
			}
			if (Once(TEXT("slam"), 86))
			{
				Audio->Play(TEXT("slam"), 2.f);
				M->Camera->AddShake(0.07);
				Fx->DustAt(Front);
				Fx->BloodAt(Front + FVector(0, 0.3, 0), -Face, 0.8);
			}
			if (Once(TEXT("stomp"), 168))
			{
				Audio->Play(TEXT("stamp"), 3.f);
				M->Camera->AddShake(0.1);
				Fx->DustAt(Front);
				Fx->DustAt(Front + FVector(0.3 * Face, 0, 0));
				Fx->BloodAt(Front + FVector(0, 0.2, 0), Face, 1.0);
				Fx->BloodAt(Front + FVector(0, 0.2, 0), -Face, 1.0);
			}
			if (T >= 168)
			{
				VA.ExtraScale = FVector(1.25, FMath::Lerp(1.0, 0.07, EaseRange(168, 171, T)), 1.25);
				VA.LiftExtra.Y = 0;
			}
		}
		else if (Id == TEXT("undertow_pool"))
		{
			CloseUp(Mid, Face, 4.6, 0, 230);
			if (Once(TEXT("pool"), 26) && Pool.IsValid())
			{
				Pool->SetWorldLocation(Anim::WorldToUE(FVector(VF.Pos.X, 0.012, 0.0)));
				Pool->SetVisibility(true);
				Audio->Play(TEXT("splash"), -2.f);
			}
			if (Pool.IsValid() && Pool->IsVisible())
			{
				const double R = 0.05 + 0.85 * EaseRange(26, 60, T);
				Pool->SetWorldScale3D(FVector(R * 2.0 * 0.8, R * 2.0, 0.01));
			}
			if (T > 40)
			{
				VA.PoseOverride = TEXT("air_hit");
			}
			if (T > 60)
			{
				VA.Sink = VF.Def->Height * 1.3 * EaseRange(60, 190, T);
				if ((int32)T % 5 == 0 && T < 200)
				{
					Fx->BurstColor(FVector(VF.Pos.X, 0.05, 0.1), FLinearColor(0.5f, 0.9f, 0.2f), 10, 1.5, 0.6);
				}
			}
			if (Once(TEXT("sink"), 70))
			{
				Audio->Play(TEXT("sink"), 0.f);
			}
			if (Once(TEXT("gone"), 196))
			{
				V->SetHidden(true);
				Audio->Play(TEXT("splash"), -6.f, 0.8f);
			}
		}
		else if (Id == TEXT("last_note"))
		{
			CloseUp(Mid, Face, 4.0, 0, 240);
			if (Once(TEXT("sing"), 30))
			{
				Audio->Play(TEXT("scream"), 0.f, 0.85f, ESlopBus::Voice);
			}
			if (T > 30 && T < 200)
			{
				VA.LiftExtra = FVector(FMath::Sin(T * 1.9) * 0.012, 0, 0);
				V->SetTint(FLinearColor(0.86f, 0.85f, 0.8f), (float)FMath::Clamp((T - 40) / 90, 0.0, 1.0), 0.2f);
				VA.PoseOverride = TEXT("dazed");
			}
			if (T > 140)
			{
				V->SetDissolve((float)FMath::Clamp((T - 140) / 80, 0.0, 1.0), FLinearColor(0.95f, 0.95f, 0.9f));
				if ((int32)T % 3 == 0 && T < 225)
				{
					Fx->BurstColor(VPos + FVector(FMath::FRandRange(-0.25, 0.25), 0.4 + FMath::FRand() * 1.2, 0.2), FLinearColor(0.92f, 0.92f, 0.88f), 12, 1.0, 0.8);
				}
			}
			if (Once(TEXT("crumble"), 150))
			{
				Audio->Play(TEXT("crumble"), 0.f);
			}
			if (Once(TEXT("heap"), 215))
			{
				Fx->DustAt(VPos);
			}
		}
		else if (Id == TEXT("led_astray"))
		{
			// Wren opens her hand; the marsh-lights gather, string out into the dark behind her
			// opponent, and lead them away into the fog until the last light winks out.
			CloseUp(Mid, Face, 4.8, 0, 236);
			const FVector Palm = A->Anim.StrikePoint(TEXT("hand_l"));
			if (T > 12 && T < 70 && (int32)T % 4 == 0)
			{
				Fx->BurstColor(Palm, FLinearColor(0.37f, 0.95f, 0.84f), 3, 0.6, 0.6);
			}
			if (Once(TEXT("chime"), 14))
			{
				Audio->Play(TEXT("bell"), -8.f, 1.6f);
			}
			const double Away = 7.0 * EaseRange(70, 215, T);
			if (T > 40 && T < 220 && (int32)T % 5 == 0)
			{
				// The lights ahead of them, always a little further off.
				for (int32 I = 1; I <= 3; ++I)
				{
					Fx->BurstColor(VPos + FVector(-Face * 0.15 * I, 1.1 + 0.15 * FMath::Sin(T * 0.08 + I), -Away - 0.9 * I), FLinearColor(0.37f, 0.95f, 0.84f), 2, 0.3, 0.7);
				}
			}
			if (T > 50)
			{
				VA.PoseOverride = TEXT("dazed");
				VA.LiftExtra = FVector(-Face * 0.3 * EaseRange(70, 215, T), 0, -Away);
				V->SetTint(FLinearColor(0.01f, 0.015f, 0.02f), (float)(0.75 * EaseRange(90, 215, T)));
			}
			if (Once(TEXT("follow"), 70))
			{
				Audio->Play(TEXT("far_whoosh"), -8.f, 0.8f);
			}
			if (Once(TEXT("gone"), 216))
			{
				V->SetHidden(true);
				Audio->Play(TEXT("splash"), -10.f, 0.7f);
				Audio->Play(TEXT("bell"), -12.f, 1.9f);
			}
		}
		else if (Id == TEXT("still_water"))
		{
			// Two fingers to the brow, and the Qhi stops everything in the body: it stills, darkens
			// to standing water, and falls as water.
			CloseUp(Mid, Face, 3.6, 0, 225);
			const FVector Brow = VPos + FVector(0, 1.55 * VF.Scale(), 0.15);
			if (T > 8 && T < 30 && (int32)T % 3 == 0)
			{
				Fx->BurstColor(A->Anim.StrikePoint(TEXT("hand_l")), FLinearColor(1.f, 0.8f, 0.35f), 3, 0.5, 0.5);
			}
			if (Once(TEXT("touch"), 30))
			{
				Audio->Play(TEXT("palm_strike"), -2.f, 0.9f);
				Audio->Play(TEXT("bell"), -4.f, 0.7f);
				Fx->FlashAt(Brow, FLinearColor(1.f, 0.82f, 0.45f), 18.f);
				Fx->BurstColor(Brow, FLinearColor(1.f, 0.8f, 0.35f), 16, 2.0, 0.5);
			}
			if (T > 30)
			{
				VA.PoseOverride = TEXT("dazed");
				V->SetTint(FLinearColor(0.03f, 0.06f, 0.08f), (float)FMath::Clamp((T - 40) / 70, 0.0, 1.0), 0.9f);
			}
			if (T > 120)
			{
				V->SetDissolve((float)FMath::Clamp((T - 120) / 60, 0.0, 1.0), FLinearColor(0.55f, 0.75f, 0.85f));
				if ((int32)T % 3 == 0 && T < 185)
				{
					Fx->BurstColor(VPos + FVector(FMath::FRandRange(-0.25, 0.25), 0.3 + FMath::FRand() * 1.4, 0.2), FLinearColor(0.92f, 0.92f, 0.88f), 10, 1.0, 0.6);
				}
			}
			if (Once(TEXT("fall"), 170))
			{
				Audio->Play(TEXT("splash"), 0.f, 0.9f);
				Fx->DustAt(VPos);
			}
			if (Once(TEXT("gone"), 186))
			{
				V->SetHidden(true);
			}
		}
		else if (Id == TEXT("lights_out"))
		{
			if (T < 70)
			{
				CloseUp(Mid, Face, 4.2, 0, 70);
			}
			if (Once(TEXT("crack"), 70))
			{
				Audio->Play(TEXT("hit_heavy"), 3.f);
				Audio->Play(TEXT("far_whoosh"), 0.f);
				M->Camera->AddShake(0.06);
				Fx->FlashAt(VPos + FVector(0, 1.3, 0.2), FLinearColor(1.f, 0.95f, 0.8f), 30.f);
				Fx->BloodAt(VPos + FVector(0, 1.4, 0), Face, 0.7);
			}
			if (T >= 70)
			{
				// Straight up and out of the top of the picture; the camera stays on Bram, looking up after them.
				const double U = FMath::Clamp((T - 70) / 50, 0.0, 1.0);
				VA.LiftExtra = FVector(Face * 2.0 * U, 3.0 * U + 9.0 * U * U, -1.5 * U);
				VA.SpinExtra = (T - 70) * 22.0;
				VA.PoseOverride = TEXT("air_hit");
				M->Camera->SetOverride(FVector(F.Pos.X - Face * 0.5, 1.4, 6.0), FVector(F.Pos.X + Face * 0.6, 2.4, 0.0), 1.0);
			}
			if (Once(TEXT("gone"), 122))
			{
				V->SetHidden(true);
			}
			if (Once(TEXT("twinkle"), 150))
			{
				Fx->FlashAt(FVector(F.Pos.X + Face * 1.6, 4.1, -1.5), FLinearColor(1.f, 1.f, 0.9f), 60.f);
				Audio->Play(TEXT("ui_select"), -2.f, 1.6f);
			}
			if (T > 175)
			{
				M->Camera->ClearOverride();
			}
		}
		else if (Id == TEXT("crush_depth"))
		{
			CloseUp(Mid, Face, 4.4, 0, 250);
			const FVector Helmet(F.Pos.X + 0.15 * Face, F.Def->Height * 0.9, 0.0);
			if (Once(TEXT("fire"), 24))
			{
				Audio->Play(TEXT("cast_harpoon"), 0.f);
			}
			if (T > 60 && T < 140)
			{
				const double K = EaseRange(60, 100, T);
				VA.LiftExtra = FVector((F.Pos.X + 0.6 * Face - VF.Pos.X) * K, 0, 0);
				VA.PoseOverride = TEXT("thrown_hold");
			}
			if (Once(TEXT("reel"), 60))
			{
				Audio->Play(TEXT("chain"), 0.f);
			}
			if (T >= 100)
			{
				const double K2 = EaseRange(100, 140, T);
				const FVector Start(F.Pos.X + 0.6 * Face, 0, 0);
				VA.LiftExtra = FMath::Lerp(Start, Helmet, K2) - VPos;
				VA.ExtraScale = FVector::OneVector * FMath::Lerp(1.0, 0.04, K2);
				VA.PoseOverride = TEXT("air_hit");
			}
			if (Once(TEXT("gulp"), 140))
			{
				V->SetHidden(true);
				Audio->Play(TEXT("gulp"), 2.f);
				Fx->BurstColor(Helmet, FLinearColor(0.6f, 0.9f, 0.8f), 20, 1.6, 0.8);
			}
		}
		else if (Id == TEXT("stage_vat"))
		{
			// The Rendering Yards' centre vat: its lip 3.3 m up, 6.6 m behind the fighting line.
			const FVector Vat(0.0, 3.3, -6.6);
			if (T < 14)
			{
				CloseUp(Mid, Face, 4.4, 0, 14);
			}
			if (Once(TEXT("upper"), 14))
			{
				Audio->Play(TEXT("hit_heavy"), 3.f);
				M->Camera->AddShake(0.06);
			}
			if (T >= 14)
			{
				const double U = FMath::Clamp((T - 14) / 56, 0.0, 1.0);
				const FVector Arc = FMath::Lerp(VPos, Vat, U) + FVector(0, 4.0 * FMath::Sin(U * UE_DOUBLE_PI), 0);
				VA.LiftExtra = Arc - VPos;
				VA.SpinExtra = (T - 14) * 14.0;
				VA.PoseOverride = TEXT("air_hit");
				M->Camera->SetOverride(FVector(VPos.X * 0.5, 3.0, 7.0), Arc + FVector(0, 0.5, 0), 1.0);
			}
			if (Once(TEXT("splash"), 70))
			{
				V->SetHidden(true);
				Audio->Play(TEXT("splash"), 3.f);
				Audio->Play(TEXT("bell"), -2.f);
				Fx->BurstColor(Vat + FVector(0, 0.2, 0), FLinearColor(0.5f, 0.95f, 0.2f), 30, 4.0, 0.9);
			}
			if (T > 120)
			{
				M->Camera->ClearOverride();
			}
		}
		else
		{
			CloseUp(Mid, Face, 4.0, 0, 180);
			if (T > 50)
			{
				V->SetDissolve((float)FMath::Clamp((T - 50) / 100, 0.0, 1.0), FLinearColor(0.6f, 1.f, 0.3f));
			}
		}
	}

	// The Slopbat's defeat: it blackens into slop and sinks into its pool.
	void FFinisherDirector::BossStep(double Delta)
	{
		if (BossT < 0)
		{
			return;
		}
		const double Before = BossT;
		BossT += Delta;
		ASlopFighterActor* V = M->Views[Boss];
		const FFighter& F = M->Sim.Fighters[Boss];
		if (BossT > 0.6)
		{
			const double K = FMath::Clamp((BossT - 0.6) / 3.0, 0.0, 1.0);
			V->SetTint(FLinearColor(0.04f, 0.07f, 0.02f), (float)K, 0.8f);
			V->Anim.Sink = F.Def->Height * 1.2 * Anim::FPose::Ease(TEXT("in"), K);
			if (FMath::FRand() < 0.4f)
			{
				M->Vfx->BurstColor(FVector(F.Pos.X + FMath::FRandRange(-0.6, 0.6), 0.1 + FMath::FRand() * 1.2, 0.2), FLinearColor(0.5f, 0.95f, 0.2f), 10, 2.0, 0.7);
			}
		}
		if (BossT > 0.6 && Before <= 0.6)
		{
			M->GetGI()->Audio->Play(TEXT("screech"), 0.f, 0.8f, ESlopBus::Voice);
		}
		if (BossT > 2.0 && Before <= 2.0)
		{
			M->GetGI()->Audio->Play(TEXT("splash"), 2.f, 0.7f);
		}
		if (BossT > 3.8)
		{
			V->SetHidden(true);
		}
	}

	// An instant kill: a slow close-up of the blow, then the fight camera again.
	void FFinisherDirector::KillStep(double Delta)
	{
		if (KillT < 0 || KillBy < 0 || KillBy > 1)
		{
			return;
		}
		KillT += Delta;
		const FFighter& D = M->Sim.Fighters[KillBy];
		const FFighter& P = M->Sim.Fighters[1 - KillBy];
		const FVector Mid((D.Pos.X + P.Pos.X) * 0.5, 1.25, 0.0);
		if (KillT < 1.6)
		{
			M->Camera->SetOverride(Mid + FVector(-(double)D.Facing * 1.4, 0.25, 2.6), Mid + FVector(0, 0.05, 0), 1.0);
		}
		else if (KillT > 2.4)
		{
			M->Camera->ClearOverride();
			KillT = -1;
		}
	}
}
