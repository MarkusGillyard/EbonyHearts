#include "View/SlopFighterActor.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/SlopLog.h"
#include "Core/SlopGameInstance.h"

using namespace Slop;

ASlopFighterActor::ASlopFighterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCastShadow(true);
	Body->bCastDynamicShadow = true;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Channel 1 too: each stage's camera fill lights only the fighters.
	Body->LightingChannels.bChannel1 = true;
	// Bones move every frame from the rig; bounds must follow them.
	Body->bComponentUseFixedSkelBounds = false;
	Form = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Form"));
	Form->SetupAttachment(Body);
	Form->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Form->SetVisibility(false);
	Form->LightingChannels.bChannel1 = true;
}

void ASlopFighterActor::UpdateForm(const FTransform& World)
{
	const FFighter& F = *Anim.Fighter;
	const FMoveDef* M = F.State == EState::Move ? F.Move : nullptr;
	const FName Want = M ? M->FormMesh : NAME_None;
	const bool bHide = M && M->bHideBody;
	if (bHide != bBodyHidden)
	{
		bBodyHidden = bHide;
		Body->SetVisibility(!bHide);
	}
	if (Want != ShownForm)
	{
		ShownForm = Want;
		UStaticMesh* Mesh = Want.IsNone() ? nullptr : LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Slop/Forms/%s.%s"), *Want.ToString(), *Want.ToString()));
		Form->SetStaticMesh(Mesh);
		Form->SetVisibility(Mesh != nullptr);
		Form->SetWorldScale3D(FVector::OneVector);
	}
	// A blade drawn off the back: its hilt leaves the scabbard for the hand.
	const bool bDrawn = M && M->bFormLeft && !Want.IsNone();
	if (HiltSlot != INDEX_NONE && bDrawn != bHiltDrawn)
	{
		bHiltDrawn = bDrawn;
		for (int32 L = 0; L < Body->GetNumLODs(); ++L)
		{
			Body->ShowMaterialSection(HiltSlot, 0, !bDrawn, L);
		}
	}
	if (ShownForm.IsNone())
	{
		return;
	}
	if (bHide)
	{
		// The beast in his place, nose the way he faces.
		Form->SetWorldLocationAndRotation(World.GetLocation() + FVector(0.0, 0.0, 95.0), FRotator(0.f, F.Facing > 0 ? 90.f : -90.f, 0.f));
	}
	else
	{
		// The weapon in the hand that strikes with it, along the forearm. (Facing left a pose is mirrored
		// onto the other arm, and the weapon goes with it.)
		const bool bLeft = (M && M->bFormLeft) != (F.Facing < 0);
		const FVector Hand = World.TransformPosition(MeshRig.RigToMeshPos(Anim.Rig.J(bLeft ? Anim::EJoint::HandL : Anim::EJoint::HandR).P));
		const FVector Elbow = World.TransformPosition(MeshRig.RigToMeshPos(Anim.Rig.J(bLeft ? Anim::EJoint::ForearmL : Anim::EJoint::ForearmR).P));
		Form->SetWorldLocationAndRotation(Hand, FRotationMatrix::MakeFromZ((Hand - Elbow).GetSafeNormal()).Rotator());
	}
}

bool ASlopFighterActor::Setup(const FFighter& Fighter, const FString& MeshPath)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogSlop, Error, TEXT("Fighter mesh missing: %s"), *MeshPath);
		return false;
	}
	FString Error;
	if (!MeshRig.Init(Mesh->GetRefSkeleton(), Fighter.Def->Height, Error))
	{
		UE_LOG(LogSlop, Error, TEXT("Fighter mesh %s: %s"), *MeshPath, *Error);
		return false;
	}
	Body->SetSkinnedAssetAndUpdate(Mesh);
	// A body put on over another (a change of shape) must not keep the last body's materials.
	Body->EmptyOverrideMaterials();
	BoundDef = Fighter.Def;
	BoundBody = Fighter.Def->Body;
	Marks[0] = Marks[1] = Marks[2] = 0.0;
	LastFlash = LastSweat = -1.f;
	bSwayValid = false;
	Anim.Setup(Fighter, GetContent().CommonPoses);
	MeshRig.ConfigureRig(Anim.Rig);
	Anim.SyncFromSim(true);
	Anim.Update(1.0, 0.0);
	Mats.Reset();
	IsSkin.Reset();
	HiltSlot = INDEX_NONE;
	bHiltDrawn = false;
	for (int32 I = 0; I < Body->GetNumMaterials(); ++I)
	{
		UMaterialInterface* M = Body->GetMaterial(I);
		UMaterialInstanceDynamic* D = Body->CreateDynamicMaterialInstance(I, M);
		Mats.Add(D);
		const FString Slot = Mesh->GetMaterials().IsValidIndex(I) ? Mesh->GetMaterials()[I].MaterialSlotName.ToString().ToLower() : FString();
		HiltSlot = Slot.Contains(TEXT("hilt")) ? I : HiltSlot;
		// Alternate colours dye the costume, never skin, faces or hair.
		IsSkin.Add(Slot.Contains(TEXT("skin")) || Slot.Contains(TEXT("body")) || Slot.Contains(TEXT("eye")) || Slot.Contains(TEXT("brow")) || Slot.Contains(TEXT("lash"))
			|| Slot.Contains(TEXT("face")) || Slot.Contains(TEXT("hair")));
	}
	const FString Glow = Json::Str(Fighter.Def->Look, TEXT("glow"), TEXT("#ffffff"));
	// The original's rim strengths were set for its brighter exposure and a stylised look; here, for
	// realistic bodies, they read at three tenths.
	SetAccent(FLinearColor(FColor::FromHex(Glow)), 0.3f * (float)Json::Num(Fighter.Def->Look, TEXT("accent"), 0.24));
	// Cloth below the hips (in the rest pose, in centimetres) swings, more toward the hem.
	SetScalar(TEXT("SwayTop"), float(0.53 * Anim.Rig.H * 100.0));
	SetScalar(TEXT("SwaySpan"), float(0.45 * Anim.Rig.H * 100.0));
	// The skin's regions for its marks: the head above the jaw within a head's breadth of the
	// body's axis, the legs below the hips, and nothing out past the trunk (the arms).
	SetScalar(TEXT("NeckZ"), float(0.85 * Anim.Rig.H * 100.0));
	SetScalar(TEXT("WaistZ"), float(0.5 * Anim.Rig.H * 100.0));
	SetScalar(TEXT("HeadRadius"), float(0.075 * Anim.Rig.H * 100.0));
	SetScalar(TEXT("BodyRadius"), float(0.125 * Anim.Rig.H * 100.0));
	bFragment = Fighter.Def->Body == TEXT("slopbat");
	bMarksGlow = Fighter.Def->Look.IsValid() && Fighter.Def->Look->HasField(TEXT("marks_glow"));
	FragmentForm = -1;
	UpdateVisual(1.0, 0.0);
	return true;
}

void ASlopFighterActor::FragmentColours(const FFighter& Fighter)
{
	// Its colour is its beholder's: the player facing it, or the Grandmaster wearing it. Each form
	// turns the two colours over and burns brighter.
	const FFighterDef* Fragment = GetContent().Fighter(TEXT("slopbat"));
	const TSharedPtr<FJsonObject> Seers = Fragment ? Json::Obj(Fragment->Raw, TEXT("observers")) : nullptr;
	if (!Seers.IsValid())
	{
		return;
	}
	const FFighter& Seer = Fighter.BaseDef->bBoss ? Fighter.Opponent() : Fighter;
	TSharedPtr<FJsonObject> Seen = Json::Obj(Seers, *Seer.BaseDef->Id.ToString());
	Seen = Seen.IsValid() ? Seen : Json::Obj(Seers, TEXT("default"));
	auto Colour = [&](const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return Seen.IsValid() && Seen->TryGetArrayField(Key, A) && A->Num() == 3 ? FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()) : FLinearColor(0.3f, 0.08f, 0.75f);
	};
	const bool bTurned = Fighter.BossForm == 1;
	SetVector(TEXT("TintA"), Colour(bTurned ? TEXT("b") : TEXT("a")));
	SetVector(TEXT("TintB"), Colour(bTurned ? TEXT("a") : TEXT("b")));
	SetScalar(TEXT("Galaxy"), 1.f + 0.45f * Fighter.BossForm);
}

bool ASlopFighterActor::Rebind(const FFighter& Fighter)
{
	if (Fighter.Def->Body != BoundBody)
	{
		const float KeepBlood = Blood;
		const bool bOk = Setup(Fighter, USlopGameInstance::MeshFor(Fighter));
		SetBlood(KeepBlood);
		return bOk;
	}
	// The same body in another form: its poses change, the skin keeps its marks.
	BoundDef = Fighter.Def;
	Anim.Setup(Fighter, GetContent().CommonPoses);
	MeshRig.ConfigureRig(Anim.Rig);
	Anim.SyncFromSim(true);
	return true;
}

void ASlopFighterActor::UpdateVisual(double Frac, double Delta)
{
	if (!MeshRig.IsValid())
	{
		return;
	}
	Anim.Update(Frac, Delta);
	if (bFragment && Anim.Fighter->BossForm != FragmentForm)
	{
		FragmentForm = Anim.Fighter->BossForm;
		FragmentColours(*Anim.Fighter);
	}
	MeshRig.Pose(Anim.Rig, Locals);
	const FTransform World = MeshRig.ComponentTransform(Anim.Root, Anim.WorldScale);
	MeshRig.SwingChains(Anim.Rig, World, Delta, Locals);
	if (Body->BoneSpaceTransforms.Num() == Locals.Num())
	{
		Body->BoneSpaceTransforms = Locals;
		Body->MarkRefreshTransformDirty();
	}
	Body->SetWorldTransform(World);
	UpdateForm(World);
	const float Flash = bFlashes ? (float)Anim.FlashAmount : 0.f;
	if (Flash != LastFlash)
	{
		// A blow brightens the body for a frame or two; strong enough to whiten it reads as a glitch.
		SetScalar(TEXT("Flash"), Flash * 1.8f);
		LastFlash = Flash;
	}
	SetScalar(TEXT("Glow"), (float)Anim.Glow * 3.f);
	if (bMarksGlow)
	{
		// A mark on the costume that shows only as the fighter's power rises (Ashka's lamp, through her shirt).
		for (int32 I = 0; I < Mats.Num(); ++I)
		{
			if (Mats[I] && !IsSkin[I])
			{
				Mats[I]->SetScalarParameterValue(TEXT("EmissiveStrength"), (float)Anim.Glow * 3.f);
			}
		}
	}
	StepSway(Delta);
	if (Anim.Fighter && Anim.Fighter->Sim && Anim.Fighter->Sim->bRefillHealth)
	{
		const int32 Hp = Anim.Fighter->Health;
		if (Hp == Anim.Fighter->MaxHealth() && LastHealth >= 0 && LastHealth < Hp && (Marks[0] > 0.0 || Marks[1] > 0.0 || Marks[2] > 0.0))
		{
			Marks[0] = Marks[1] = Marks[2] = 0.0;
			ShowMarks();
		}
		LastHealth = Hp;
	}
	if (bSweats && Anim.Fighter)
	{
		// About two minutes of fighting, or the health taken, brings it on fully.
		Exertion += Delta / 120.0;
		const double Hurt = double(Anim.Fighter->DamageTaken) / FMath::Max(1, Anim.Fighter->Def->Health);
		const float Sweat = (float)FMath::Clamp(Exertion * 0.6 + Hurt * 0.6, 0.0, 1.0);
		if (FMath::Abs(Sweat - LastSweat) > 0.01f)
		{
			SetScalar(TEXT("Sweat"), Sweat);
			LastSweat = Sweat;
		}
	}
}

// A coat's tails and a gown's hem lag the body: a lightly damped spring (about 1.6 Hz) driven by
// the hips' acceleration, so walking off, stopping, a kick or a blow sets the fabric swinging.
// Up and down counts half (cloth floats a little in a jump); the swing is held to 10 cm at the hem.
void ASlopFighterActor::StepSway(double Delta)
{
	if (Delta <= 0.0)
	{
		return;
	}
	const FVector P = Anim.WorldPoint(Anim.Rig.J(Slop::Anim::EJoint::Hips).P);
	if (!bSwayValid || (P - SwayLastP).Size() > 1.0)
	{
		// First frame, or a jump in place (a new round): start at rest.
		SwayX = SwayV = SwayLastV = FVector::ZeroVector;
		SwayLastP = P;
		bSwayValid = true;
		return;
	}
	const FVector V = (P - SwayLastP) / Delta;
	FVector A = (V - SwayLastV) / Delta;
	A.Y *= 0.5;
	// A throw or a launch flings the body far harder than cloth follows: cap what drives it.
	A = A.GetClampedToMaxSize(25.0);
	SwayLastP = P;
	SwayLastV = V;
	const double W = UE_DOUBLE_TWO_PI * 1.6, Z = 0.35;
	const int32 N = FMath::Clamp(FMath::CeilToInt(Delta * 240.0), 1, 16);
	const double H = Delta / N;
	for (int32 I = 0; I < N; ++I)
	{
		SwayV += (-W * W * SwayX - 2.0 * Z * W * SwayV - A) * H;
		SwayX += SwayV * H;
	}
	const double Most = 0.1 * Anim.Rig.H / 1.8;
	if (SwayX.Size() > Most)
	{
		SwayX = SwayX.GetSafeNormal() * Most;
		SwayV *= 0.5;
	}
	const FVector Ue = Slop::Anim::WorldToUE(SwayX) - Slop::Anim::WorldToUE(FVector::ZeroVector);
	SetVector(TEXT("SwayVec"), FLinearColor(Ue.X, Ue.Y, Ue.Z, 0.f));
}

void ASlopFighterActor::TakeBlow(int32 Region, int32 Damage)
{
	if (!Anim.Fighter || Damage <= 0 || Region < 0 || Region > 2)
	{
		return;
	}
	Marks[Region] = FMath::Min(1.0, Marks[Region] + Damage / (0.33 * FMath::Max(1, Anim.Fighter->MaxHealth())));
	ShowMarks();
}

void ASlopFighterActor::SetBlood(float Amount)
{
	Blood = FMath::Clamp(Amount, 0.f, 1.f);
	ShowMarks();
}

void ASlopFighterActor::ShowMarks()
{
	SetVector(TEXT("Damage"), FLinearColor((float)Marks[0], (float)Marks[1], (float)Marks[2], Blood));
	// Legs beaten past a third start to favour the lead one; badly beaten, a clear limp.
	Anim.Limp = FMath::Clamp((Marks[2] - 0.3) / 0.5, 0.0, 1.0);
}

void ASlopFighterActor::SetScalar(FName Param, float Value)
{
	for (UMaterialInstanceDynamic* M : Mats)
	{
		if (M)
		{
			M->SetScalarParameterValue(Param, Value);
		}
	}
}

void ASlopFighterActor::SetVector(FName Param, const FLinearColor& Value)
{
	for (UMaterialInstanceDynamic* M : Mats)
	{
		if (M)
		{
			M->SetVectorParameterValue(Param, Value);
		}
	}
}

void ASlopFighterActor::SetTint(const FLinearColor& Color, float Amount, float Gloss)
{
	SetVector(TEXT("TintColor"), Color);
	SetScalar(TEXT("TintAmount"), Amount);
	SetScalar(TEXT("TintGloss"), Gloss);
}

void ASlopFighterActor::SetDissolve(float Amount, const FLinearColor& Color)
{
	SetVector(TEXT("DissolveColor"), Color);
	SetScalar(TEXT("Dissolve"), Amount);
}

void ASlopFighterActor::SetCostume(const FLinearColor& ClothTint)
{
	for (int32 I = 0; I < Mats.Num(); ++I)
	{
		if (Mats[I] && !IsSkin[I])
		{
			Mats[I]->SetVectorParameterValue(TEXT("Tint"), ClothTint);
		}
	}
}

void ASlopFighterActor::SetAccent(const FLinearColor& Color, float Strength)
{
	SetVector(TEXT("AccentColor"), Color);
	SetScalar(TEXT("AccentStrength"), Strength);
}

void ASlopFighterActor::SetHidden(bool bHide)
{
	Body->SetVisibility(!bHide, true);
}
