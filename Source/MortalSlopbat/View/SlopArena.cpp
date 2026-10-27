#include "View/SlopArena.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Core/SlopLog.h"

namespace SlopArenaImpl
{
	using FList = TArray<TSharedPtr<FJsonValue>>;

	double StageNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		return O->TryGetNumberField(Key, V) ? V : Default;
	}

	// The stage data's positions are Blender metres: x along the stage, y away from the camera,
	// z up. The game's world is centimetres with the stage along Y and depth along +X.
	FVector StagePos(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const FList* A = nullptr;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3)
		{
			return FVector::ZeroVector;
		}
		return FVector((*A)[1]->AsNumber(), (*A)[0]->AsNumber(), (*A)[2]->AsNumber()) * 100.0;
	}

	FVector StageDir(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		return (StagePos(O, Key) / 100.0).GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, -1).GetSafeNormal());
	}

	FLinearColor StageCol(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Alpha = 1.f)
	{
		const FList* A = nullptr;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3)
		{
			return FLinearColor(1, 1, 1, Alpha);
		}
		return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), A->Num() > 3 ? (float)(*A)[3]->AsNumber() : Alpha);
	}

	UStaticMesh* Plane()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}

	// Glow materials that breathe or flicker, by what they are: fire flickers fast, slop and
	// cracks swell slowly, candles shiver.
	bool GlowMotion(const FString& Name, float& Speed, float& Depth)
	{
		if (Name.Contains(TEXT("furnace")) || Name.Contains(TEXT("fire")) || Name.Contains(TEXT("ember")))
		{
			Speed = 7.f;
			Depth = 0.25f;
			return true;
		}
		if (Name.Contains(TEXT("candle")) || Name.Contains(TEXT("lantern")) || Name.Contains(TEXT("jar")))
		{
			Speed = 5.f;
			Depth = 0.1f;
			return true;
		}
		if (Name.Contains(TEXT("slop")) || Name.Contains(TEXT("crack")))
		{
			Speed = 1.1f;
			Depth = 0.12f;
			return true;
		}
		return false;
	}
}

using namespace SlopArenaImpl;

ASlopArena::ASlopArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	SetRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Set"));
	SetRoot->SetMobility(EComponentMobility::Static);
	SetRoot->SetupAttachment(Root);
	SetRoot->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
}

void ASlopArena::BareFloor()
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	C->SetRelativeLocation(FVector(0, 0, -25));
	C->SetRelativeScale3D(FVector(14, 30, 0.5));
	C->RegisterComponent();
	UDirectionalLightComponent* D = NewObject<UDirectionalLightComponent>(this);
	D->SetupAttachment(Root);
	D->SetRelativeRotation(FRotator(-40, 30, 0));
	D->SetIntensity(3.f);
	D->RegisterComponent();
}

bool ASlopArena::Build(FName Stage, bool bHighGraphics)
{
	StageId = Stage;
	bHigh = bHighGraphics;
	Rng.Initialize(GetTypeHash(Stage));
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Stages") / Stage.ToString() + TEXT(".json");
	FString Text;
	TSharedPtr<FJsonObject> D;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), D) || !D.IsValid())
	{
		UE_LOG(LogSlop, Error, TEXT("Stage %s: no data at %s"), *Stage.ToString(), *Path);
		BareFloor();
		return false;
	}
	D->TryGetStringField(TEXT("music"), Music);
	const TSharedPtr<FJsonObject>* Units = nullptr;
	if (D->TryGetObjectField(TEXT("units"), Units))
	{
		ExposureEV100 = StageNum(*Units, TEXT("exposure_ev100"), 0.0);
	}
	FString Step;
	D->TryGetStringField(TEXT("footstep"), Step);
	Footstep = Step == TEXT("wood") ? TEXT("kenney:footstep_wood") : TEXT("kenney:footstep_concrete");
	FootstepDb = Step == TEXT("wood") ? -2.5f : 2.5f;

	// What each piece is made of decides whether it casts a shadow: glass, flames and water don't.
	TMap<FString, bool> Shadowless;
	const FList* Objects = nullptr;
	if (D->TryGetArrayField(TEXT("objects"), Objects))
	{
		for (const TSharedPtr<FJsonValue>& V : *Objects)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			bool bAllLight = true;
			const FList* M = nullptr;
			if (O->TryGetArrayField(TEXT("materials"), M))
			{
				for (const TSharedPtr<FJsonValue>& N : *M)
				{
					const FString S = N->AsString();
					bAllLight &= S.StartsWith(TEXT("GLOW_")) || S.StartsWith(TEXT("WATER_"));
				}
			}
			Shadowless.Add(O->GetStringField(TEXT("name")), bAllLight);
		}
	}
	TMap<FString, TSharedPtr<FJsonObject>> Swaying;
	const FList* Animated = nullptr;
	if (D->TryGetArrayField(TEXT("animated"), Animated))
	{
		for (const TSharedPtr<FJsonValue>& V : *Animated)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (O->GetStringField(TEXT("kind")) == TEXT("sway") && O->HasField(TEXT("pivot_ue_cm")))
			{
				Swaying.Add(O->GetStringField(TEXT("object")), O);
			}
		}
	}
	FString Bolt_;
	const FList* Fx = nullptr;
	if (D->TryGetArrayField(TEXT("fx"), Fx))
	{
		for (const TSharedPtr<FJsonValue>& V : *Fx)
		{
			if (V->AsObject()->GetStringField(TEXT("kind")) == TEXT("lightning"))
			{
				V->AsObject()->TryGetStringField(TEXT("object"), Bolt_);
			}
		}
	}

	const FString Dir = FString::Printf(TEXT("/Game/Slop/Stages/%s"), *Stage.ToString());
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> Breathing;
	int32 Missing = 0;
	const FList* Placements = nullptr;
	if (D->TryGetArrayField(TEXT("placements"), Placements))
	{
		for (const TSharedPtr<FJsonValue>& V : *Placements)
		{
			const FString Name = V->AsObject()->GetStringField(TEXT("object"));
			if (Name.StartsWith(TEXT("ORIGIN_MARKER")))
			{
				continue;
			}
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Dir, *Name, *Name), nullptr, LOAD_Quiet | LOAD_NoWarn);
			if (!M)
			{
				++Missing;
				continue;
			}
			const TSharedPtr<FJsonObject>* Sw = Swaying.Find(Name);
			const bool bMoves = Sw != nullptr || Name == Bolt_;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetupAttachment(SetRoot);
			C->SetStaticMesh(M);
			// Still pieces are static so the lights that do not move can keep their shadows.
			C->SetMobility(bMoves ? EComponentMobility::Movable : EComponentMobility::Static);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(!Shadowless.FindRef(Name));
			for (int32 I = 0; I < C->GetNumMaterials(); ++I)
			{
				UMaterialInterface* Mi = C->GetMaterial(I);
				float Speed = 0.f, Depth = 0.f;
				if (Mi && Mi->GetName().Contains(TEXT("GLOW_")) && GlowMotion(Mi->GetName(), Speed, Depth))
				{
					TObjectPtr<UMaterialInstanceDynamic>& Dyn = Breathing.FindOrAdd(Mi->GetName());
					if (!Dyn)
					{
						Dyn = UMaterialInstanceDynamic::Create(Mi, this);
						Mats.Add(Dyn);
						FGlowPulse& P = Pulses.AddDefaulted_GetRef();
						P.M = Dyn;
						Dyn->GetScalarParameterValue(FName(TEXT("Intensity")), P.Base);
						P.Speed = Speed;
						P.Depth = Depth;
						P.Phase = Rng.FRandRange(0.f, 6.28f);
					}
					C->SetMaterial(I, Dyn);
				}
			}
			C->RegisterComponent();
			Pieces.Add(Name, C);
			if (Sw)
			{
				const TSharedPtr<FJsonObject> O = *Sw;
				FSway S;
				S.C = C;
				// Pivot and axis in the imported frame (the set root's space).
				const FList& P = O->GetArrayField(TEXT("pivot_ue_cm"));
				S.Pivot = FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
				const FList* A = nullptr;
				S.Axis = O->TryGetArrayField(TEXT("axis_ue"), A) && A->Num() >= 3 ? FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()).GetSafeNormal() : FVector(1, 0, 0);
				S.Amp = StageNum(O, TEXT("amp"), 3.0);
				S.Speed = StageNum(O, TEXT("speed"), 0.8);
				S.Phase = Rng.FRandRange(0.f, 6.28f);
				Sways.Add(S);
			}
			if (Name == Bolt_)
			{
				Bolt = C;
				C->SetVisibility(false);
			}
		}
	}
	if (Pieces.Num() == 0)
	{
		UE_LOG(LogSlop, Error, TEXT("Stage %s: none of its meshes are imported under %s"), *Stage.ToString(), *Dir);
		BareFloor();
	}
	else if (Missing > 0)
	{
		UE_LOG(LogSlop, Warning, TEXT("Stage %s: %d meshes missing"), *Stage.ToString(), Missing);
	}
	const FList* L = nullptr;
	if (D->TryGetArrayField(TEXT("lights"), L))
	{
		Lights(*L);
	}
	Atmosphere(D);
	if (Fx)
	{
		Effects(*Fx);
	}
	UE_LOG(LogSlop, Log, TEXT("Stage %s: %d pieces, %d lights flickering, %d swaying, %d effects"), *Stage.ToString(), Pieces.Num(), Flickers.Num(), Sways.Num(), Layers.Num());
	return Pieces.Num() > 0;
}

void ASlopArena::Lights(const TArray<TSharedPtr<FJsonValue>>& List)
{
	for (const TSharedPtr<FJsonValue>& V : List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FString Type = O->GetStringField(TEXT("type"));
		const FString Name = O->GetStringField(TEXT("name"));
		const bool bShadows = O->GetBoolField(TEXT("shadows"));
		ULightComponent* Light = nullptr;
		float Intensity = 0.f;
		if (Type == TEXT("directional"))
		{
			UDirectionalLightComponent* Dl = NewObject<UDirectionalLightComponent>(this);
			Intensity = StageNum(O, TEXT("lux"), 2.0);
			Dl->SetLightSourceAngle(StageNum(O, TEXT("source_angle_deg"), 1.0));
			Dl->SetDynamicShadowDistanceMovableLight(bHigh ? 4000.f : 2500.f);
			Dl->SetDynamicShadowCascades(bHigh ? 3 : 2);
			Dl->SetCastShadows(bShadows);
			// Screen-space contact shadows ground the fighters' feet and darken the folds of their
			// bodies where the shadow maps are too coarse.
			Dl->ContactShadowLength = bShadows ? 0.03f : 0.f;
			// The shadowing key is the one directional light that lights translucency and fog.
			Dl->SetForwardShadingPriority(bShadows ? 1 : 0);
			Dl->SetRelativeRotation(StageDir(O, TEXT("dir")).Rotation());
			Light = Dl;
		}
		else
		{
			UPointLightComponent* Pl = nullptr;
			if (Type == TEXT("spot"))
			{
				USpotLightComponent* Sl = NewObject<USpotLightComponent>(this);
				Sl->SetInnerConeAngle(StageNum(O, TEXT("cone_inner_deg"), 20.0));
				Sl->SetOuterConeAngle(StageNum(O, TEXT("cone_outer_deg"), 40.0));
				Sl->SetRelativeRotation(StageDir(O, TEXT("dir")).Rotation());
				Pl = Sl;
			}
			else
			{
				Pl = NewObject<UPointLightComponent>(this);
			}
			Pl->SetIntensityUnits(ELightUnits::Candelas);
			Intensity = StageNum(O, TEXT("intensity_candela"), 10.0);
			Pl->SetAttenuationRadius(StageNum(O, TEXT("radius_m"), 6.0) * 100.0);
			Pl->SetSourceRadius(StageNum(O, TEXT("source_radius_m"), 0.1) * 100.0);
			Pl->SetCastShadows(bShadows && bHigh);
			Pl->ContactShadowLength = bShadows && bHigh ? 0.02f : 0.f;
			Pl->SetRelativeLocation(StagePos(O, TEXT("pos")));
			Light = Pl;
		}
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetLightColor(StageCol(O, TEXT("color")));
		Light->SetIntensity(Intensity);
		Light->SetSpecularScale(StageNum(O, TEXT("specular_scale"), 1.0));
		// In volumetric fog the broad key and rim would haze the whole frame; the stage's own lamps
		// and shafts scatter (the data's values were set for Blender's volume, about 3x stronger).
		Light->SetVolumetricScatteringIntensity(Type == TEXT("directional") ? 0.15f : FMath::Min(4.0, StageNum(O, TEXT("volumetric_scattering"), 0.4)));
		FString Affects;
		if (O->TryGetStringField(TEXT("affects"), Affects) && Affects == TEXT("fighters_only"))
		{
			// The camera fill lights the fighters (lighting channel 1) and nothing behind them.
			Light->LightingChannels.bChannel0 = false;
			Light->LightingChannels.bChannel1 = true;
		}
		Light->RegisterComponent();
		if (Name == TEXT("storm"))
		{
			Storm = Light;
			StormBase = Intensity / 7.f;
			Light->SetIntensity(0.f);
			NextFlash = Rng.FRandRange(3.f, 7.f);
			continue;
		}
		const float Flicker = StageNum(O, TEXT("flicker"));
		if (Flicker > 0.f)
		{
			FFlicker F;
			F.Light = Light;
			F.Base = Intensity;
			F.Depth = Flicker;
			F.Speed = FMath::Max(0.3, StageNum(O, TEXT("flicker_speed"), 4.0));
			F.Phase = Rng.FRandRange(0.f, 6.28f);
			Flickers.Add(F);
		}
	}
}

void ASlopArena::Atmosphere(const TSharedPtr<FJsonObject>& D)
{
	const TSharedPtr<FJsonObject>* FogP = nullptr;
	if (D->TryGetObjectField(TEXT("fog"), FogP))
	{
		const TSharedPtr<FJsonObject> F = *FogP;
		UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
		Fog->SetupAttachment(Root);
		Fog->SetRelativeLocation(FVector(0, 0, StageNum(F, TEXT("fog_height_m")) * 100.0));
		// The data gives the engine's own density and falloff units, judged for Blender's volume;
		// here the same numbers fog far more, so they are halved.
		Fog->SetFogDensity(StageNum(F, TEXT("density"), 0.2) * 0.5);
		Fog->SetFogHeightFalloff(StageNum(F, TEXT("height_falloff"), 0.5));
		Fog->SetFogInscatteringColor(StageCol(F, TEXT("color")));
		Fog->SetStartDistance(150.f);
		// The sky dome sits beyond this, unfogged.
		Fog->SetFogCutoffDistance(30000.f);
		Fog->SetVolumetricFog(bHigh);
		Fog->SetVolumetricFogScatteringDistribution(0.35f);
		Fog->SetVolumetricFogAlbedo(FColor(150, 155, 160));
		Fog->SetVolumetricFogExtinctionScale(0.6f);
		Fog->SetVolumetricFogDistance(4000.f);
		Fog->RegisterComponent();
	}
	FString SkyName;
	D->TryGetStringField(TEXT("sky"), SkyName);
	UTextureCube* Cube = nullptr;
	if (!SkyName.IsEmpty() && SkyName != TEXT("none"))
	{
		Cube = LoadObject<UTextureCube>(nullptr, *FString::Printf(TEXT("/Game/Slop/Skies/HDR_%s.HDR_%s"), *SkyName, *SkyName), nullptr, LOAD_Quiet | LOAD_NoWarn);
	}
	// Ambient and reflections take their shape from a night sky even where none is seen.
	UTextureCube* AmbientCube = Cube ? Cube : LoadObject<UTextureCube>(nullptr, TEXT("/Game/Slop/Skies/HDR_lakeside_night.HDR_lakeside_night"));
	const TSharedPtr<FJsonObject>* AmbP = nullptr;
	FLinearColor Ambient(0.35f, 0.4f, 0.45f);
	float AmbientK = 0.3f;
	if (D->TryGetObjectField(TEXT("ambient"), AmbP))
	{
		Ambient = StageCol(*AmbP, TEXT("color"));
		AmbientK = StageNum(*AmbP, TEXT("intensity"), 0.3);
	}
	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	Sky->SetCubemap(AmbientCube);
	Sky->SetLightColor(Ambient);
	Sky->SetIntensity(AmbientK * 1.5f);
	Sky->bLowerHemisphereIsBlack = false;
	Sky->RegisterComponent();
	if (Cube)
	{
		UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Slop/Materials/M_Sky.M_Sky"), nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (SkyMat)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SkyMat, this);
			M->SetTextureParameterValue(TEXT("Sky"), Cube);
			// The HDRs are bright evening skies; at night only a little of them shows.
			M->SetScalarParameterValue(TEXT("Intensity"), StageNum(D, TEXT("sky_intensity"), 0.08));
			Mats.Add(M);
			UStaticMeshComponent* Dome = NewObject<UStaticMeshComponent>(this);
			Dome->SetupAttachment(Root);
			Dome->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Dome->SetMobility(EComponentMobility::Static);
			Dome->SetRelativeScale3D(FVector(800.0));
			Dome->SetRelativeRotation(FRotator(0.0, StageNum(D, TEXT("sky_yaw_deg"), 0.0), 0.0));
			Dome->SetCastShadow(false);
			Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Dome->SetMaterial(0, M);
			Dome->RegisterComponent();
		}
	}
}

ASlopArena::FFxLayer& ASlopArena::AddLayer(EFx Kind, const TCHAR* Material, const FLinearColor& Color, float Intensity, int32 Count)
{
	FFxLayer& L = Layers.AddDefaulted_GetRef();
	L.Kind = Kind;
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
	I->SetupAttachment(Root);
	I->SetStaticMesh(Plane());
	I->SetMobility(EComponentMobility::Movable);
	I->NumCustomDataFloats = 1;
	I->SetCastShadow(false);
	I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Slop/Materials/%s.%s"), Material, Material));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R, Color.G, Color.B, 1.f));
	M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	M->SetScalarParameterValue(TEXT("Opacity"), Color.A);
	Mats.Add(M);
	I->SetMaterial(0, M);
	I->RegisterComponent();
	L.Ism = I;
	L.Parts.SetNum(FMath::Max(1, bHigh ? Count : Count / 2));
	TArray<FTransform> T;
	T.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.001)), L.Parts.Num());
	I->AddInstances(T, false, false, false);
	return L;
}

void ASlopArena::Respawn(FFxLayer& L, FPart& P, bool bAnywhere)
{
	const FVector R(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f));
	P.P = L.Center + R * L.Extent;
	P.Age = 0.f;
	P.Life = L.Life * Rng.FRandRange(0.7f, 1.3f);
	P.Size = L.Size * Rng.FRandRange(0.7f, 1.3f);
	P.Phase = Rng.FRandRange(0.f, 6.28f);
	P.V = L.Vel * Rng.FRandRange(0.8f, 1.2f);
	switch (L.Kind)
	{
	case EFx::Rain:
		// From the cloud box down to the floor; at the start, anywhere on the way.
		P.P.Z = bAnywhere ? Rng.FRandRange(0.f, (float)(L.Center.Z + L.Extent.Z)) : L.Center.Z + L.Extent.Z;
		P.Life = 1e9f;
		return;
	case EFx::Rise:
		P.V += FVector(Rng.FRandRange(-8.f, 8.f), Rng.FRandRange(-8.f, 8.f), 0.f);
		break;
	case EFx::Drift:
		P.V = FVector(Rng.FRandRange(-6.f, 6.f), Rng.FRandRange(-6.f, 6.f), Rng.FRandRange(-2.f, 4.f)) + L.Vel;
		break;
	case EFx::Mist:
		P.V = FVector(0.f, Rng.FRandRange(5.f, 15.f) * (Rng.FRand() < 0.5f ? -1.f : 1.f), 0.f);
		break;
	default:
		P.V = FVector::ZeroVector;
		break;
	}
	if (bAnywhere)
	{
		P.Age = Rng.FRandRange(0.f, P.Life);
	}
}

void ASlopArena::Effects(const TArray<TSharedPtr<FJsonValue>>& List)
{
	for (const TSharedPtr<FJsonValue>& V : List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FString Kind = O->GetStringField(TEXT("kind"));
		const FLinearColor C = StageCol(O, TEXT("color"));
		if (Kind == TEXT("rain"))
		{
			FFxLayer& L = AddLayer(EFx::Rain, TEXT("M_SoftDisc"), C, 1.2f * C.A, (int32)StageNum(O, TEXT("count"), 800));
			L.Center = StagePos(O, TEXT("box_center"));
			L.Extent = StagePos(O, TEXT("box_extent")).GetAbs();
			L.Vel = StagePos(O, TEXT("velocity"));
			L.Size = StageNum(O, TEXT("streak_m"), 0.4) * 100.0;
		}
		else if (Kind == TEXT("smoke") || Kind == TEXT("steam") || Kind == TEXT("splash_mist"))
		{
			const bool bSmoke = Kind == TEXT("smoke");
			// Unlit puffs: their colour is what they reflect, scaled to the night's light.
			FFxLayer& L = AddLayer(EFx::Rise, TEXT("M_Puff"), C, bSmoke ? 0.25f : 0.3f, bSmoke ? 10 : 8);
			const double Radius = StageNum(O, TEXT("radius_m"), 0.6) * 100.0;
			L.Center = StagePos(O, TEXT("pos"));
			L.Extent = FVector(Radius, Radius, 10.0);
			L.Vel = FVector(0, 0, StageNum(O, TEXT("rise_m_s"), 0.8) * 100.0);
			L.Size = StageNum(O, TEXT("size_m"), Radius / 50.0) * 100.0;
			L.Grow = bSmoke ? 2.5f : 1.8f;
			L.Life = bSmoke ? 7.f : 3.5f;
		}
		else if (Kind == TEXT("embers") || Kind == TEXT("motes"))
		{
			const bool bEmbers = Kind == TEXT("embers");
			FFxLayer& L = AddLayer(EFx::Drift, TEXT("M_SoftDisc"), C, bEmbers ? 6.f : 0.8f * C.A, (int32)StageNum(O, TEXT("count"), 60));
			L.Center = StagePos(O, TEXT("box_center"));
			L.Extent = StagePos(O, TEXT("box_extent")).GetAbs();
			L.Vel = FVector(0, 0, StageNum(O, TEXT("rise_m_s"), 0.05) * 100.0);
			L.Size = StageNum(O, TEXT("size_m"), bEmbers ? 0.04 : 0.03) * 100.0;
			L.Life = bEmbers ? 5.f : 9.f;
		}
		else if (Kind == TEXT("glints"))
		{
			FFxLayer& L = AddLayer(EFx::Twinkle, TEXT("M_SoftDisc"), C, 10.f, (int32)StageNum(O, TEXT("count"), 6));
			L.Center = StagePos(O, TEXT("center"));
			L.Extent = StagePos(O, TEXT("extent")).GetAbs();
			L.Size = 7.f;
			L.Life = 2.5f;
		}
		else if (Kind == TEXT("mist"))
		{
			const FList* S = nullptr;
			FVector2D Size(20.0, 10.0);
			if (O->TryGetArrayField(TEXT("size"), S) && S->Num() >= 2)
			{
				Size = FVector2D((*S)[0]->AsNumber(), (*S)[1]->AsNumber());
			}
			FLinearColor MC = C;
			MC.A = StageNum(O, TEXT("density"), 0.3) * 0.35;
			FFxLayer& L = AddLayer(EFx::Mist, TEXT("M_Puff"), MC, 0.2f, FMath::Clamp((int32)(Size.X * Size.Y / 60.0), 6, 24));
			L.Center = StagePos(O, TEXT("center"));
			// Size is (along the stage, depth) in metres.
			L.Extent = FVector(Size.Y * 50.0, Size.X * 50.0, 20.0);
			L.Size = FMath::Clamp(Size.Y * 40.0, 400.0, 1400.0);
			L.Life = 16.f;
		}
		else if (Kind == TEXT("drips"))
		{
			FFxLayer& L = AddLayer(EFx::Drips, TEXT("M_Ring"), FLinearColor(0.6f, 0.7f, 0.85f), 0.5f, 10);
			L.Center = StagePos(O, TEXT("box_center")) + FVector(0, 0, 0.5);
			L.Extent = StagePos(O, TEXT("box_extent")).GetAbs();
			L.Size = 40.f;
			L.Life = 1.4f;
		}
	}
	for (FFxLayer& L : Layers)
	{
		for (FPart& P : L.Parts)
		{
			Respawn(L, P, true);
		}
	}
}

void ASlopArena::StepLightning(double Dt)
{
	if (!Storm)
	{
		return;
	}
	auto Show = [this](bool bOn) { if (Bolt) Bolt->SetVisibility(bOn); };
	if (FlashT < 0.0)
	{
		NextFlash -= Dt;
		if (NextFlash <= 0.0)
		{
			FlashT = 0.0;
			NextFlash = Rng.FRandRange(5.f, 11.f);
		}
		return;
	}
	FlashT += Dt;
	const double T = FlashT;
	float K = 0.f;
	bool bBolt = false;
	if (bStrobe)
	{
		// Three pulses then a fade: 7x, 1.5x, 5x of the base, gone by 0.55 s.
		if (T < 0.07) { K = 7.f; bBolt = true; }
		else if (T < 0.12) { K = 1.5f; }
		else if (T < 0.2) { K = 5.f; bBolt = true; }
		else if (T < 0.55) { K = 5.f * (float)(1.0 - (T - 0.2) / 0.35); }
	}
	else if (T < 2.4)
	{
		// Flashes off: a slow swell instead of a strobe.
		K = 2.f * FMath::Sin((float)(T / 2.4 * UE_DOUBLE_PI));
	}
	Storm->SetIntensity(StormBase * K);
	Show(bBolt);
	if (T > (bStrobe ? 0.55 : 2.4))
	{
		FlashT = -1.0;
		Storm->SetIntensity(0.f);
		Show(false);
	}
}

void ASlopArena::Step(double Delta, const FVector& CameraPos)
{
	Time += Delta;
	for (FFlicker& F : Flickers)
	{
		const float N = 0.5f * FMath::Sin(Time * F.Speed + F.Phase) + 0.3f * FMath::Sin(Time * F.Speed * 2.7f + F.Phase * 1.7f) + 0.2f * FMath::Sin(Time * F.Speed * 5.3f);
		F.Light->SetIntensity(F.Base * FMath::Max(0.f, 1.f + F.Depth * N));
	}
	for (FGlowPulse& P : Pulses)
	{
		const float N = 0.6f * FMath::Sin(Time * P.Speed + P.Phase) + 0.4f * FMath::Sin(Time * P.Speed * 2.3f + P.Phase * 1.3f);
		P.M->SetScalarParameterValue(TEXT("Intensity"), P.Base * (1.f + P.Depth * N));
	}
	for (FSway& S : Sways)
	{
		// A pendulum about its hook, with a smaller swing across it.
		const double W = S.Speed * 2.0;
		const FQuat Q = FQuat(S.Axis, FMath::DegreesToRadians(S.Amp * FMath::Sin(Time * W + S.Phase)))
			* FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(0.6 * S.Amp * FMath::Sin(Time * W * 0.77 + S.Phase * 1.9)));
		S.C->SetRelativeLocationAndRotation(S.Pivot - Q.RotateVector(S.Pivot), Q);
	}
	StepLightning(Delta);

	const FVector ToCam = (CameraPos - FVector(0, 0, 150)).GetSafeNormal(UE_SMALL_NUMBER, FVector(-1, 0, 0));
	const FQuat Facing = FRotationMatrix::MakeFromZ(ToCam).ToQuat();
	static const FQuat Flat = FQuat::Identity;
	const float Dt = (float)Delta;
	for (FFxLayer& L : Layers)
	{
		TArray<FTransform> T;
		T.Reserve(L.Parts.Num());
		for (int32 I = 0; I < L.Parts.Num(); ++I)
		{
			FPart& P = L.Parts[I];
			P.Age += Dt;
			P.P += P.V * Dt;
			if (L.Kind == EFx::Rain && P.P.Z < 0.0)
			{
				Respawn(L, P, false);
			}
			else if (P.Age >= P.Life)
			{
				Respawn(L, P, false);
			}
			const float U = P.Age / P.Life;
			float Fade = 1.f;
			FQuat R = Facing;
			FVector S(P.Size / 100.0);
			switch (L.Kind)
			{
			case EFx::Rain:
				// A thin streak along the fall, turned to face the camera.
				R = FRotationMatrix::MakeFromZY(ToCam, P.V.GetSafeNormal()).ToQuat();
				S = FVector(0.012, P.Size / 100.0, 1.0);
				break;
			case EFx::Rise:
				Fade = FMath::Sin(U * UE_PI) * (1.f - U * 0.5f);
				S = FVector(P.Size / 100.0 * (1.0 + L.Grow * U));
				break;
			case EFx::Drift:
				P.V += FVector(FMath::Sin(Time * 0.7 + P.Phase), FMath::Cos(Time * 0.5 + P.Phase), 0.0) * 3.0 * Dt;
				Fade = FMath::Min(1.f, FMath::Min(U * 4.f, (1.f - U) * 4.f)) * (0.6f + 0.4f * FMath::Sin(Time * 6.0 + P.Phase));
				break;
			case EFx::Twinkle:
				Fade = FMath::Pow(FMath::Max(0.f, FMath::Sin(U * UE_PI)), 6.f);
				S = FVector(P.Size / 100.0 * (0.4 + Fade));
				break;
			case EFx::Mist:
				Fade = FMath::Sin(U * UE_PI);
				break;
			case EFx::Drips:
				R = Flat;
				Fade = 1.f - U;
				S = FVector(P.Size / 100.0 * (0.2 + U), P.Size / 100.0 * (0.2 + U), 1.0);
				break;
			}
			T.Add(FTransform(R, P.P, S));
			L.Ism->SetCustomDataValue(I, 0, Fade, false);
		}
		L.Ism->BatchUpdateInstancesTransforms(0, T, false, true, true);
	}
}
