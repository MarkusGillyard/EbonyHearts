#include "View/SlopVfx.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Anim/SlopMeshRig.h"
#include "View/SlopMatch.h"
#include "View/SlopFighterActor.h"
#include "Core/SlopAudio.h"
#include "Core/SlopGameInstance.h"

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

// How each projectile kind looks, after the original's table: fire and energy glow and light
// their surroundings; thrown steel, tar and the anchor are solid and unlit; the ground slap and
// the wind are only dust and streaks along their path.
namespace
{
	enum class EProjSolid : uint8 { None, Blade, Harpoon, Anchor, Tar };
	struct FProjLook { FLinearColor Core; float Glow; float Light; float CoreSize; EProjSolid Solid; int32 Trail; float TrailEvery = 0.03f; float TrailSize = 0.f; };

	const FProjLook& LookOf(FName Kind)
	{
		// Trail: -1 none, else the particle kind (cast) streamed behind it.
		static const TMap<FName, FProjLook> Looks = {
			{TEXT("ember"), {FLinearColor(1.f, 0.55f, 0.15f), 8.f, 9.f, 1.f, EProjSolid::None, 1}},
			{TEXT("flare"), {FLinearColor(1.f, 0.22f, 0.12f), 9.f, 10.f, 0.8f, EProjSolid::None, 1}},
			{TEXT("orb"), {FLinearColor(0.8f, 0.9f, 1.f), 6.f, 6.f, 1.f, EProjSolid::None, -1}},
			{TEXT("wave"), {FLinearColor(0.55f, 0.8f, 1.f), 0.5f, 5.f, 0.25f, EProjSolid::None, 5}},
			{TEXT("bubble"), {FLinearColor(0.55f, 0.95f, 0.85f), 0.22f, 2.f, 0.9f, EProjSolid::None, -1}},
			{TEXT("storm"), {FLinearColor(0.6f, 1.f, 0.4f), 9.f, 12.f, 1.f, EProjSolid::None, 4}},
			{TEXT("tar"), {FLinearColor(0.05f, 0.08f, 0.04f), 0.f, 0.f, 0.f, EProjSolid::Tar, 4}},
			{TEXT("sickle"), {FLinearColor(0.75f, 0.8f, 0.85f), 0.f, 0.f, 0.f, EProjSolid::Blade, -1}},
			{TEXT("harpoon"), {FLinearColor(0.8f, 0.62f, 0.3f), 0.f, 0.f, 0.f, EProjSolid::Harpoon, -1}},
			{TEXT("anchor"), {FLinearColor(0.25f, 0.25f, 0.27f), 0.f, 0.f, 0.f, EProjSolid::Anchor, 3}},
			{TEXT("quake"), {FLinearColor(0.55f, 0.45f, 0.32f), 0.f, 0.f, 0.f, EProjSolid::None, 3, 0.015f, 0.09f}},
			{TEXT("gust"), {FLinearColor(0.75f, 0.82f, 0.8f), 0.f, 0.f, 0.f, EProjSolid::None, 3, 0.008f, 0.13f}},
		};
		static const FProjLook Default = {FLinearColor(0.9f, 0.85f, 0.6f), 6.f, 6.f, 1.f, EProjSolid::None, -1};
		const FProjLook* L = Looks.Find(Kind);
		return L ? *L : Default;
	}
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
		if (E.bKO)
		{
			// The deciding blow, as in the original: a wide white ring and a bright flash.
			Spawn(EKind::Ring, P, FVector::ZeroVector, 0.34, 0.2, 7.5, 0, 0);
			Flash(P, FLinearColor(1.f, 0.9f, 0.7f), 14.f);
		}
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
	{
		// It breaks up into what it was made of: fire into embers, steel into sparks, tar into
		// slime, the bubble into spray, the anchor and the ground's shock into dust.
		const FProjLook& L = LookOf(E.Kind);
		const EKind K = L.Glow > 0.f && E.Kind != TEXT("bubble") && E.Kind != TEXT("wave") ? EKind::Ember
			: (L.Solid == EProjSolid::Blade || L.Solid == EProjSolid::Harpoon) ? EKind::Spark
			: E.Kind == TEXT("bubble") || E.Kind == TEXT("wave") ? EKind::Pale
			: L.Solid == EProjSolid::Tar ? EKind::Slop : EKind::Dust;
		Burst(K, FVector(E.Point.X, E.Point.Y, 0.2), 10, K == EKind::Dust ? 1.4 : 3.0, 0.25, K == EKind::Dust ? 0.07 : 0.025);
		break;
	}
	case EEvent::ThrowBreak:
	{
		// Two grips torn apart: a quick flash between them.
		const FVector Mid((M->Sim.Fighters[0].Pos.X + M->Sim.Fighters[1].Pos.X) * 0.5, 1.3, 0.1);
		Flash(Mid, FLinearColor(1.f, 1.f, 1.f), 4.f);
		Burst(EKind::Pale, Mid, 8, 2.2, 0.25, 0.015);
		break;
	}
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

UStaticMeshComponent* ASlopVfx::ProjPart(const TCHAR* Shape, UMaterialInterface* Material)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(VfxShape(Shape));
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetMaterial(0, Material);
	C->RegisterComponent();
	return C;
}

UMaterialInterface* ASlopVfx::SolidMaterial(const FLinearColor& Color, double Roughness, double Metallic, bool bTar)
{
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,
		bTar ? TEXT("/Game/Slop/Materials/M_Water.M_Water") : TEXT("/Game/Slop/Materials/M_Surface.M_Surface")), this);
	M->SetVectorParameterValue(bTar ? TEXT("Color") : TEXT("Tint"), Color);
	M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	if (!bTar)
	{
		M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	}
	Mats.Add(M);
	return M;
}

void ASlopVfx::SyncProjectiles(double Delta)
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
		const FProjLook& L = LookOf(P.Kind);
		FProjView* V = Projs.FindByPredicate([&P](const FProjView& X) { return X.Serial == P.Serial; });
		if (!V)
		{
			FProjView N;
			N.Serial = P.Serial;
			N.Kind = P.Kind;
			if (L.Glow > 0.f)
			{
				UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_GlowAdd.M_GlowAdd")), this);
				Mat->SetVectorParameterValue(TEXT("Color"), L.Core);
				Mat->SetScalarParameterValue(TEXT("Intensity"), L.Glow);
				Mats.Add(Mat);
				N.Mesh = ProjPart(TEXT("Sphere"), Mat);
			}
			UMaterialInterface* Steel = nullptr;
			switch (L.Solid)
			{
			case EProjSolid::Blade:
				Steel = SolidMaterial(L.Core, 0.3, 0.9, false);
				N.Part = ProjPart(TEXT("Cube"), Steel);
				break;
			case EProjSolid::Harpoon:
				Steel = SolidMaterial(FLinearColor(0.6f, 0.62f, 0.65f), 0.35, 0.9, false);
				N.Mesh = ProjPart(TEXT("Cone"), Steel);
				N.Part = ProjPart(TEXT("Cylinder"), SolidMaterial(L.Core, 0.45, 0.8, false));
				break;
			case EProjSolid::Anchor:
				Steel = SolidMaterial(L.Core, 0.55, 0.85, false);
				N.Mesh = ProjPart(TEXT("Cylinder"), Steel);
				N.Part = ProjPart(TEXT("Cylinder"), Steel);
				break;
			case EProjSolid::Tar:
				N.Part = ProjPart(TEXT("Sphere"), SolidMaterial(L.Core, 0.15, 0.0, true));
				break;
			default:
				break;
			}
			if (P.Def().bTether || P.Kind == TEXT("sickle"))
			{
				N.Chain = ProjPart(TEXT("Cylinder"), SolidMaterial(FLinearColor(0.35f, 0.35f, 0.33f), 0.5, 0.8, false));
			}
			if (L.Light > 0.f && N.Mesh)
			{
				N.Light = NewObject<UPointLightComponent>(this);
				N.Light->SetupAttachment(N.Mesh);
				N.Light->SetLightColor(L.Core);
				N.Light->SetIntensityUnits(ELightUnits::Candelas);
				N.Light->SetIntensity(L.Light * (bReduced ? 0.6f : 1.f));
				N.Light->SetAttenuationRadius(300.f);
				N.Light->SetCastShadows(false);
				N.Light->RegisterComponent();
			}
			Projs.Add(N);
			V = &Projs.Last();
		}
		const FVector At(P.Pos.X, P.Pos.Y, 0.0);
		const double Big = FMath::Max(P.Size.X, P.Size.Y);
		const double Dir = P.Vel.X != 0.0 ? FMath::Sign(P.Vel.X) : double(P.Facing);
		V->Spin += Delta * 14.0;
		if (V->Mesh && L.Glow > 0.f)
		{
			V->Mesh->SetWorldLocation(WorldToUE(At));
			V->Mesh->SetWorldScale3D(FVector(Big * L.CoreSize));
		}
		switch (L.Solid)
		{
		case EProjSolid::Blade:
			// A sickle blade turning end over end as it flies.
			V->Part->SetWorldLocation(WorldToUE(At));
			V->Part->SetWorldRotation(FRotator(0.f, 0.f, float(FMath::RadiansToDegrees(V->Spin))));
			V->Part->SetWorldScale3D(FVector(0.006, 0.3, 0.07));
			break;
		case EProjSolid::Harpoon:
		{
			// The barbed tip leading, the shaft behind it, along the line of flight (the engine's
			// cone and cylinder stand along their Z axis).
			const FRotator Along = FRotationMatrix::MakeFromZ((WorldToUE(At + FVector(Dir, 0.0, 0.0)) - WorldToUE(At)).GetSafeNormal()).Rotator();
			V->Mesh->SetWorldLocation(WorldToUE(At));
			V->Mesh->SetWorldRotation(Along);
			V->Mesh->SetWorldScale3D(FVector(0.07, 0.07, 0.3));
			V->Part->SetWorldLocation(WorldToUE(At - FVector(Dir * 0.45, 0.0, 0.0)));
			V->Part->SetWorldRotation(Along);
			V->Part->SetWorldScale3D(FVector(0.025, 0.025, 0.6));
			break;
		}
		case EProjSolid::Anchor:
			// A shank with its crown across the bottom, falling.
			V->Mesh->SetWorldLocation(WorldToUE(At + FVector(0.0, 0.08, 0.0)));
			V->Mesh->SetWorldScale3D(FVector(0.07, 0.07, 0.5));
			V->Part->SetWorldLocation(WorldToUE(At - FVector(0.0, 0.17, 0.0)));
			V->Part->SetWorldRotation(FRotator(0.f, 0.f, 90.f));
			V->Part->SetWorldScale3D(FVector(0.09, 0.09, 0.5));
			break;
		case EProjSolid::Tar:
			V->Part->SetWorldLocation(WorldToUE(At));
			V->Part->SetWorldScale3D(FVector(Big * 0.85, Big * 0.7, Big * 0.7));
			break;
		default:
			break;
		}
		if (V->Chain && M->Views[P.Owner])
		{
			// The line back to the thrower's nearer hand.
			const Slop::Anim::FAnimator& A = M->Views[P.Owner]->Anim;
			const FVector H0 = A.WorldPoint(A.Rig.HandTip(0)), H1 = A.WorldPoint(A.Rig.HandTip(1));
			const FVector Hand = (H0 - At).SizeSquared() < (H1 - At).SizeSquared() ? H0 : H1;
			const FVector From = WorldToUE(Hand), To = WorldToUE(At);
			const FVector Span = To - From;
			V->Chain->SetWorldLocation((From + To) * 0.5);
			V->Chain->SetWorldRotation(FRotationMatrix::MakeFromZ(Span.GetSafeNormal()).Rotator());
			V->Chain->SetWorldScale3D(FVector(0.016, 0.016, Span.Size() / 100.0));
		}
		if (L.Trail >= 0 && Delta > 0.0)
		{
			// Streamed at a steady rate whatever the frame rate.
			V->TrailT += Delta;
			const double Every = L.TrailEvery * (bReduced ? 2.0 : 1.0);
			while (V->TrailT >= Every)
			{
				V->TrailT -= Every;
				const EKind K = (EKind)L.Trail;
				const FVector Back(-Dir * FMath::FRandRange(0.5, 1.5), FMath::FRandRange(-0.3, 0.6), FMath::FRandRange(-0.2, 0.2));
				const FVector Jit(FMath::FRandRange(-0.5, 0.5) * P.Size.X, FMath::FRandRange(-0.5, 0.5) * P.Size.Y, 0.0);
				if (K == EKind::Ring)
				{
					Spawn(K, At + Jit, FVector(Dir * 1.0, 0, 0), 0.35, 0.1, 2.0, 1.0, 0);
				}
				else
				{
					const double Size = L.TrailSize > 0.f ? L.TrailSize : (K == EKind::Dust ? 0.06 : 0.02);
					Spawn(K, At + Jit, Back, K == EKind::Dust ? 0.5 : 0.35, Size, 0.4, 2.5, K == EKind::Dust ? -0.5 : 0.0);
				}
			}
		}
	}
	for (int32 I = Projs.Num() - 1; I >= 0; --I)
	{
		if (!Live.Contains(Projs[I].Serial))
		{
			for (UStaticMeshComponent* C : {Projs[I].Mesh.Get(), Projs[I].Part.Get(), Projs[I].Chain.Get()})
			{
				if (C) C->DestroyComponent();
			}
			if (Projs[I].Light) Projs[I].Light->DestroyComponent();
			Projs.RemoveAt(I);
		}
	}
}

// A wail (a move tagged "sound": Mirela's Keen) is seen as faint rings leaving the mouth while it
// can hit, so a daze from two metres off has something to show for it.
void ASlopVfx::SoundRings()
{
	ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const FFighter& F = M->Sim.Fighters[I];
		const FMoveDef* Mv = F.State == EState::Move ? F.Move : nullptr;
		if (!Mv || !Mv->HasTag(TEXT("sound")) || F.MF < Mv->Startup || F.MF >= Mv->Startup + Mv->Active || F.MF == SoundMF[I] || !M->Views[I])
		{
			continue;
		}
		SoundMF[I] = F.MF;
		if ((F.MF - Mv->Startup) % (bReduced ? 4 : 2) != 0)
		{
			continue;
		}
		const Slop::Anim::FAnimator& A = M->Views[I]->Anim;
		FVector Head = A.WorldPoint(A.Rig.J(Slop::Anim::EJoint::Head).P);
		Head.X += F.Facing * 0.18 * A.Rig.H / 1.8;
		Head.Z = 0.25;
		TArray<FParticle>& Rings = Parts[(int32)EKind::Ring];
		const int32 Before = Rings.Num();
		Spawn(EKind::Ring, Head, FVector(F.Facing * 6.5, 0, 0), 0.3, 0.06, 2.6, 1.5, 0);
		if (Rings.Num() > Before)
		{
			// Fainter than a counter's ring: it starts part-way faded.
			Rings.Last().Age = 0.12;
		}
	}
}

void ASlopVfx::StormStep(double Delta)
{
	ASlopMatch* M = Match.Get();
	if (!M)
	{
		return;
	}
	StormTime += Delta;
	const FFighter* Storm = nullptr;
	for (const FFighter& F : M->Sim.Fighters)
	{
		if (F.State == EState::Move && F.Move && F.Move->HasTag(TEXT("storm")))
		{
			Storm = &F;
		}
	}
	if (!Storm)
	{
		if (StormLight)
		{
			StormLight->SetVisibility(false);
		}
		return;
	}
	if (!StormLight)
	{
		StormLight = NewObject<UPointLightComponent>(this);
		StormLight->SetupAttachment(RootComponent);
		StormLight->SetLightColor(FLinearColor(0.6f, 1.f, 0.4f));
		StormLight->SetIntensityUnits(ELightUnits::Candelas);
		StormLight->SetAttenuationRadius(900.f);
		StormLight->SetCastShadows(false);
		StormLight->RegisterComponent();
	}
	const FFighterDef& D = *Storm->Def;
	const FMoveDef& Mv = *Storm->Move;
	const FVector C(Storm->Pos.X, 1.4 * D.Scale(), 0.0);
	const float Calm = bReduced ? 0.6f : 1.f;
	StormLight->SetVisibility(true);
	StormLight->SetWorldLocation(WorldToUE(C + FVector(0.0, 0.6, 0.6)));
	if (Storm->MF < Mv.Startup)
	{
		// The charge: a pulse that quickens and brightens, and slime bursting off the body.
		const double K = double(Storm->MF) / FMath::Max(1, Mv.Startup);
		StormLight->SetIntensity(float((60.0 + K * 260.0) * (0.6 + 0.4 * FMath::Sin(StormTime * 40.0))) * Calm);
		if (FMath::FRand() < (0.35 + K * 0.4) * Delta * 60.0)
		{
			Burst(EKind::Slop, C + FVector(FMath::FRandRange(-0.8, 0.8), FMath::FRandRange(-0.6, 1.2), 0.2), bReduced ? 3 : 6, 2.5, 0.25, 0.03);
		}
		return;
	}
	if (Storm->MF < Mv.Startup + Mv.Active)
	{
		const int32 Began = M->Sim.TickCount - Storm->MF;
		if (StormFired != Began)
		{
			StormFired = Began;
			if (USlopGameInstance* G = M->GetGI())
			{
				if (G->Audio)
				{
					G->Audio->Play(TEXT("thunder"), 2.f);
				}
			}
			Flash(C, FLinearColor(0.7f, 1.f, 0.5f), 3.f);
		}
		StormLight->SetIntensity(400.f * Calm);
		// Lightning: bright streaks raking down round the body, thinned for the comfort setting.
		const int32 N = bReduced || !bFlashes ? 1 : 3;
		for (int32 I = 0; I < N; ++I)
		{
			const double A = FMath::FRandRange(0.0, UE_DOUBLE_TWO_PI);
			const double R = FMath::FRandRange(0.6, 2.4) * D.Scale();
			Spawn(EKind::Spark, FVector(C.X + FMath::Cos(A) * R, 3.2 * D.Scale(), FMath::Sin(A) * 0.3), FVector(FMath::FRandRange(-2.0, 2.0), -14.0, 0.0), 0.16, 0.05, 0, 0.5, 0);
		}
		return;
	}
	StormLight->SetIntensity(0.f);
}

void ASlopVfx::Step(double Delta)
{
	SyncProjectiles(Delta);
	SoundRings();
	StormStep(Delta);
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
