#include "View/SlopVfx.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Anim/SlopMeshRig.h"
#include "View/SlopMatch.h"

using namespace Slop;
using Slop::Anim::WorldToUE;

static UStaticMesh* VfxShape(const TCHAR* Name)
{
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
}

ASlopVfx::ASlopVfx()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASlopVfx::Setup(ASlopMatch* InMatch)
{
	Match = InMatch;
	UMaterialInterface* Add = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_GlowAdd.M_GlowAdd"));
	UMaterialInterface* Opaque = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_GlowOpaque.M_GlowOpaque"));
	struct FSpec { const TCHAR* Shape; FLinearColor Color; float Intensity; bool bAdd; };
	const FSpec Specs[(int32)EKind::Count] = {
		{TEXT("Cube"), FLinearColor(1.f, 0.75f, 0.35f), 12.f, true},  // spark
		{TEXT("Cube"), FLinearColor(1.f, 0.45f, 0.1f), 10.f, true},   // ember
		{TEXT("Sphere"), FLinearColor(0.32f, 0.02f, 0.01f), 1.f, false}, // blood
		{TEXT("Sphere"), FLinearColor(0.3f, 0.28f, 0.24f), 0.6f, true}, // dust
		{TEXT("Sphere"), FLinearColor(0.4f, 1.f, 0.2f), 6.f, true},    // slop
		{TEXT("Plane"), FLinearColor(1.f, 0.88f, 0.65f), 3.f, true},   // ring (camera-facing quad)
		{TEXT("Plane"), FLinearColor(1.f, 0.85f, 0.6f), 2.f, true},    // soft flash disc
		{TEXT("Cube"), FLinearColor(0.9f, 0.9f, 0.86f), 2.f, true},    // pale flakes (salt, ash, tallow)
	};
	for (int32 K = 0; K < (int32)EKind::Count; ++K)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetStaticMesh(VfxShape(Specs[K].Shape));
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->NumCustomDataFloats = 1;
		UMaterialInterface* Base = Specs[K].bAdd ? Add : Opaque;
		if (K == (int32)EKind::Ring)
		{
			Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Ring.M_Ring"));
		}
		else if (K == (int32)EKind::Disc)
		{
			Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_SoftDisc.M_SoftDisc"));
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Specs[K].Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Specs[K].Intensity);
		C->SetMaterial(0, M);
		C->RegisterComponent();
		Mats.Add(M);
		Ism.Add(C);
	}
}

void ASlopVfx::Spawn(EKind K, const FVector& P, const FVector& V, double Life, double Size, double Grow, double Drag, double Gravity)
{
	TArray<FParticle>& A = Parts[(int32)K];
	if (A.Num() > (bReduced ? 120 : 400))
	{
		return;
	}
	A.Add({P, V, Life, 0.0, Size, Grow, Drag, Gravity, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f)});
}

void ASlopVfx::Burst(EKind K, const FVector& P, int32 N, double Speed, double Life, double Size, const FVector& Bias)
{
	const int32 Count = bReduced ? FMath::Max(1, N / 3) : N;
	for (int32 I = 0; I < Count; ++I)
	{
		const FVector Dir = (FMath::VRand() + Bias).GetSafeNormal();
		Spawn(K, P, Dir * Speed * FMath::FRandRange(0.4, 1.0), Life * FMath::FRandRange(0.6, 1.0), Size * FMath::FRandRange(0.6, 1.2));
	}
}

void ASlopVfx::BurstColor(const FVector& P, const FLinearColor& C, int32 N, double Speed, double Life)
{
	// The nearest of the effect families to the colour asked for.
	const FLinearColor H = C.LinearRGBToHSV();
	EKind K = EKind::Ember;
	if (H.B > 0.75f && H.G < 0.25f) K = EKind::Pale;
	else if (H.R > 70.f && H.R < 160.f) K = EKind::Slop;
	else if (H.G < 0.3f) K = EKind::Dust;
	Burst(K, P, N, Speed, Life, 0.03, FVector(0, 0.5, 0));
}

void ASlopVfx::DustAt(const FVector& P)
{
	Burst(EKind::Dust, FVector(P.X, 0.05, 0.1), 14, 1.6, 0.9, 0.09, FVector(0, 0.6, 0));
}

void ASlopVfx::BloodAt(const FVector& P, double Dir, double Amount)
{
	if (Gore == TEXT("off"))
	{
		Burst(EKind::Pale, P, (int32)(12 * Amount), 2.5, 0.6, 0.025, FVector(Dir, 0.4, 0));
		return;
	}
	Burst(EKind::Blood, P, (int32)((Gore == TEXT("low") ? 6 : 20) * Amount), 3.0, 0.7, 0.035, FVector(Dir, 0.5, 0));
}

void ASlopVfx::Flash(const FVector& P, const FLinearColor& C, float Intensity)
{
	if (!bFlashes)
	{
		Intensity *= 0.3f;
	}
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(RootComponent);
	L->SetMobility(EComponentMobility::Movable);
	L->SetRelativeLocation(WorldToUE(P));
	L->SetLightColor(C);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(350.f);
	L->SetCastShadows(false);
	L->RegisterComponent();
	Flashes.Add({L, 0.12, 0.0, Intensity});
}

void ASlopVfx::OnEvent(const FSimEvent& E)
{
	ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	const FVector P(E.Point.X, E.Point.Y, 0.25);
	switch (E.Type)
	{
	case EEvent::Hit:
	{
		const double S = FMath::Clamp(E.Damage / 100.0, 0.2, 1.4);
		const FFighter& A = M->Sim.Fighters[FMath::Clamp(E.By, 0, 1)];
		const FVector Along(A.Facing, 0.15, 0);
		const FMoveDef* Mv = A.Def->Move(E.Move);
		const FName Mat = Mv ? Mv->HitSound : NAME_None;
		const bool bMetal = Mat == TEXT("metal") || Mat == TEXT("blade") || Mat == TEXT("chain");
		// What a blow throws off is what it lands on: steel, brass and the specials' energy spark;
		// a fist or shin sprays sweat from the head and raises a faint puff from the body; a club
		// knocks out a little dust. The ring marks counters and the heaviest blows only.
		if (bMetal || E.bProj)
		{
			Burst(EKind::Spark, P, 10 + (int32)(S * 14), 5.0 + S * 4.0, 0.22, 0.025, Along);
		}
		else if (E.Level == ELevel::High)
		{
			Burst(EKind::Pale, P, 6 + (int32)(S * 10), 2.6 + S * 2.0, 0.35, 0.012, Along + FVector(0, 0.35, 0));
		}
		else
		{
			Burst(EKind::Dust, P, 3 + (int32)(S * 4), 1.0, 0.35, 0.045, Along * 0.6);
		}
		if (Mat == TEXT("wood"))
		{
			Burst(EKind::Dust, P, 5, 1.6, 0.4, 0.03, Along);
		}
		if (E.bCounter || S >= 1.0)
		{
			Spawn(EKind::Ring, P, FVector::ZeroVector, 0.16 + S * 0.06, 0.12, 3.0 + S * 2.0, 0, 0);
		}
		Spawn(EKind::Disc, P, FVector::ZeroVector, 0.07, 0.2 + S * 0.2, 1.0, 0, 0);
		Flash(P, FLinearColor(1.f, 0.8f, 0.55f), 2.5f + S * 5.f);
		if (Gore != TEXT("off") && !E.bProj && E.Damage > 0)
		{
			const int32 N = Gore == TEXT("low") ? 3 : 6 + (int32)(S * 10);
			Burst(EKind::Blood, P, N, 2.5 + S * 2.0, 0.6, 0.03, Along + FVector(0, 0.4, 0));
		}
		break;
	}
	case EEvent::Block:
	{
		// Forearms take a blow with a slap and a puff of dust off the cloth; a blocked weapon of
		// metal still sparks.
		const FFighter& A = M->Sim.Fighters[FMath::Clamp(E.By, 0, 1)];
		const FMoveDef* Mv = A.Def->Move(E.Move);
		const FName Mat = Mv ? Mv->HitSound : NAME_None;
		if (Mat == TEXT("metal") || Mat == TEXT("blade") || Mat == TEXT("chain") || E.bProj)
		{
			Burst(EKind::Spark, P, 8, 4.0, 0.16, 0.02);
			Flash(P, FLinearColor(0.7f, 0.85f, 1.f), 3.f);
		}
		else
		{
			Burst(EKind::Dust, P, 4, 0.9, 0.3, 0.04, FVector(A.Facing * 0.5, 0.2, 0));
		}
		break;
	}
	case EEvent::Armor:
		Burst(EKind::Ember, P, 12, 3.0, 0.3, 0.025);
		break;
	case EEvent::Clash:
		Burst(EKind::Ember, P, 24, 6.0, 0.35, 0.03);
		Flash(P, FLinearColor(1.f, 0.6f, 0.2f), 12.f);
		break;
	case EEvent::Land:
		if (!E.bSoft && E.Who >= 0)
		{
			const FVector2D F = M->Sim.Fighters[E.Who].Pos;
			Burst(EKind::Dust, FVector(F.X, 0.05, 0.1), 10, 1.4, 0.7, 0.08, FVector(0, 0.6, 0));
		}
		break;
	case EEvent::ProjectileEnd:
		Burst(EKind::Ember, FVector(E.Point.X, E.Point.Y, 0.2), 10, 3.0, 0.25, 0.025);
		break;
	case EEvent::Teleport:
		if (E.Who >= 0)
		{
			const FVector2D F = M->Sim.Fighters[E.Who].Pos;
			Burst(EKind::Slop, FVector(F.X, 0.1, 0.1), 16, 2.0, 0.5, 0.05, FVector(0, 1, 0));
		}
		break;
	default:
		break;
	}
}

void ASlopVfx::SyncProjectiles()
{
	ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	TSet<int32> Live;
	for (const FProjectile& P : M->Sim.Projectiles)
	{
		Live.Add(P.Serial);
		FProjView* V = Projs.FindByPredicate([&P](const FProjView& X) { return X.Serial == P.Serial; });
		if (!V)
		{
			FProjView N;
			N.Serial = P.Serial;
			N.Kind = P.Kind;
			N.Mesh = NewObject<UStaticMeshComponent>(this);
			N.Mesh->SetupAttachment(RootComponent);
			N.Mesh->SetStaticMesh(VfxShape(TEXT("Sphere")));
			N.Mesh->SetMobility(EComponentMobility::Movable);
			N.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			N.Mesh->SetCastShadow(false);
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_GlowAdd.M_GlowAdd")), this);
			const FString K = P.Kind.ToString();
			const FLinearColor C = K == TEXT("ember") || K == TEXT("flare") ? FLinearColor(1.f, 0.5f, 0.12f)
				: K == TEXT("wave") || K == TEXT("bubble") ? FLinearColor(0.55f, 0.75f, 1.f)
				: K == TEXT("tar") ? FLinearColor(0.2f, 0.5f, 0.15f)
				: K == TEXT("storm") ? FLinearColor(0.5f, 1.f, 0.4f) : FLinearColor(0.9f, 0.85f, 0.6f);
			Mat->SetVectorParameterValue(TEXT("Color"), C);
			Mat->SetScalarParameterValue(TEXT("Intensity"), 8.f);
			N.Mesh->SetMaterial(0, Mat);
			N.Mesh->RegisterComponent();
			Mats.Add(Mat);
			N.Light = NewObject<UPointLightComponent>(this);
			N.Light->SetupAttachment(N.Mesh);
			N.Light->SetLightColor(C);
			N.Light->SetIntensityUnits(ELightUnits::Candelas);
			N.Light->SetIntensity(6.f);
			N.Light->SetAttenuationRadius(300.f);
			N.Light->SetCastShadows(false);
			N.Light->RegisterComponent();
			Projs.Add(N);
			V = &Projs.Last();
		}
		V->Mesh->SetWorldLocation(WorldToUE(FVector(P.Pos.X, P.Pos.Y, 0.0)));
		const FVector2D S = P.Size * 100.0;
		V->Mesh->SetWorldScale3D(FVector(FMath::Max(S.X, S.Y) / 100.0));
	}
	for (int32 I = Projs.Num() - 1; I >= 0; --I)
	{
		if (!Live.Contains(Projs[I].Serial))
		{
			if (Projs[I].Light) Projs[I].Light->DestroyComponent();
			if (Projs[I].Mesh) Projs[I].Mesh->DestroyComponent();
			Projs.RemoveAt(I);
		}
	}
}

void ASlopVfx::Step(double Delta)
{
	SyncProjectiles();
	for (int32 K = 0; K < (int32)EKind::Count; ++K)
	{
		TArray<FParticle>& A = Parts[K];
		for (int32 I = A.Num() - 1; I >= 0; --I)
		{
			FParticle& P = A[I];
			P.Age += Delta;
			if (P.Age >= P.Life)
			{
				A.RemoveAtSwap(I);
				continue;
			}
			P.V.Y -= P.Gravity * Delta;
			P.V *= FMath::Max(0.0, 1.0 - P.Drag * Delta);
			P.P += P.V * Delta;
			if (P.P.Y < 0.0 && K == (int32)EKind::Blood)
			{
				P.P.Y = 0.005;
				P.V = FVector::ZeroVector;
				P.Gravity = 0;
			}
			P.Size += P.Grow * Delta * ((K == (int32)EKind::Ring || K == (int32)EKind::Disc) ? 1.0 : 0.1);
		}
		UInstancedStaticMeshComponent* C = Ism[K];
		TArray<FTransform> T;
		T.Reserve(A.Num());
		for (const FParticle& P : A)
		{
			const double Fade = 1.0 - P.Age / P.Life;
			double S = P.Size * (K == (int32)EKind::Ring ? 1.0 : FMath::Max(0.15, Fade));
			FVector Scale(S);
			FRotator R = P.R;
			if (K == (int32)EKind::Ring || K == (int32)EKind::Disc)
			{
				// Quads facing the camera (which looks along +X); they widen and fade.
				static const FRotator Facing = FRotationMatrix::MakeFromZ(FVector(-1, 0, 0)).Rotator();
				R = Facing;
				Scale = FVector(S, S, 1.0);
			}
			else if (K == (int32)EKind::Spark)
			{
				// Streaks along their motion.
				const FVector VU = WorldToUE(P.V) / 100.0;
				R = VU.Rotation();
				Scale = FVector(S * (1.0 + P.V.Size() * 0.6), S * 0.35, S * 0.35);
			}
			T.Add(FTransform(R, WorldToUE(P.P), Scale));
		}
		C->ClearInstances();
		if (T.Num() > 0)
		{
			C->AddInstances(T, false, false, false);
			for (int32 I = 0; I < A.Num(); ++I)
			{
				C->SetCustomDataValue(I, 0, (float)(1.0 - A[I].Age / A[I].Life), I == A.Num() - 1);
			}
		}
	}
	for (int32 I = Flashes.Num() - 1; I >= 0; --I)
	{
		FFlashLight& F = Flashes[I];
		F.Age += Delta;
		if (F.Age >= F.Life)
		{
			if (F.L) F.L->DestroyComponent();
			Flashes.RemoveAtSwap(I);
			continue;
		}
		F.L->SetIntensity(F.Peak * (1.f - (float)(F.Age / F.Life)));
	}
}
