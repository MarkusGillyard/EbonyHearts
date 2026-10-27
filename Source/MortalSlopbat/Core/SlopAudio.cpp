#include "Core/SlopAudio.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"
#include "Core/SlopLog.h"

static const TCHAR* OriginalDir = TEXT("/Game/Slop/Audio/Original/");

void USlopAudio::Init(UGameInstance* InOwner)
{
	Owner = InOwner;
	EffectLimit = NewObject<USoundConcurrency>(this);
	EffectLimit->Concurrency.MaxCount = 24;
	EffectLimit->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	UiLimit = NewObject<USoundConcurrency>(this);
	UiLimit->Concurrency.MaxCount = 4;
	UiLimit->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
}

void USlopAudio::IndexFamilies()
{
	bIndexed = true;
	// Index the Kenney families ("impactPunch_heavy" -> its five variants). A cooked game has the
	// registry loaded already; an editor build may still be scanning, so scan this folder now.
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
#if WITH_EDITOR
	AR.ScanPathsSynchronous({TEXT("/Game/Slop/Audio/Kenney")}, false);
#endif
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(TEXT("/Game/Slop/Audio/Kenney")), Assets, true);
	for (const FAssetData& A : Assets)
	{
		const FString N = A.AssetName.ToString();
		int32 Us = INDEX_NONE;
		FString Family = N;
		if (N.FindLastChar(TEXT('_'), Us) && N.Mid(Us + 1).IsNumeric())
		{
			Family = N.Left(Us);
		}
		else
		{
			// "footstep01" style: trailing digits without an underscore.
			int32 I = N.Len();
			while (I > 0 && FChar::IsDigit(N[I - 1]))
			{
				--I;
			}
			Family = N.Left(I);
		}
		Families.FindOrAdd(TEXT("kenney:") + Family).Add(A.GetObjectPathString());
	}
	UE_LOG(LogSlop, Log, TEXT("Audio: %d Kenney families indexed"), Families.Num());
}

void USlopAudio::Preload()
{
	if (!bIndexed)
	{
		IndexFamilies();
	}
	TArray<FSoftObjectPath> Paths;
	for (const TPair<FString, TArray<FString>>& F : Families)
	{
		for (const FString& P : F.Value)
		{
			Paths.Add(FSoftObjectPath(P));
		}
	}
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
#if WITH_EDITOR
	AR.ScanPathsSynchronous({TEXT("/Game/Slop/Audio/Original")}, false);
#endif
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(TEXT("/Game/Slop/Audio/Original")), Assets, true);
	for (const FAssetData& A : Assets)
	{
		Paths.Add(A.GetSoftObjectPath());
	}
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &USlopAudio::OnPreloaded));
	UE_LOG(LogSlop, Log, TEXT("Audio: streaming %d sounds"), Paths.Num());
}

void USlopAudio::OnPreloaded()
{
	TArray<UObject*> Loaded;
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->GetLoadedAssets(Loaded);
	}
	for (UObject* O : Loaded)
	{
		if (USoundWave* W = Cast<USoundWave>(O))
		{
			Cache.Add(W->GetPathName(), W);
		}
	}
	UE_LOG(LogSlop, Log, TEXT("Audio: %d sounds ready"), Cache.Num());
	PreloadHandle.Reset();
}

UWorld* USlopAudio::World() const { return Owner ? Owner->GetWorld() : nullptr; }

void USlopAudio::SetVolumes(float InMaster, float InMusic, float InEffects, float InVoice)
{
	Master = InMaster;
	MusicVol = InMusic;
	EffectsVol = InEffects;
	VoiceVol = InVoice;
	if (Music)
	{
		Music->SetVolumeMultiplier(FMath::Max(0.0001f, Master * MusicVol));
	}
}

float USlopAudio::BusVolume(ESlopBus Bus) const
{
	switch (Bus)
	{
	case ESlopBus::Music: return Master * MusicVol;
	case ESlopBus::Voice: return Master * VoiceVol;
	default: return Master * EffectsVol;
	}
}

USoundWave* USlopAudio::Find(const FString& Name)
{
	FString Path;
	if (Name.StartsWith(TEXT("kenney:")))
	{
		if (!bIndexed)
		{
			IndexFamilies();
		}
		const TArray<FString>* Fam = Families.Find(Name);
		if (!Fam || Fam->Num() == 0)
		{
			return nullptr;
		}
		int32 Pick = FMath::RandRange(0, Fam->Num() - 1);
		const int32* Last = LastVariant.Find(Name);
		if (Last && Fam->Num() > 1 && Pick == *Last)
		{
			Pick = (Pick + 1) % Fam->Num();
		}
		LastVariant.Add(Name, Pick);
		Path = (*Fam)[Pick];
	}
	else
	{
		Path = FString(OriginalDir) + Name + TEXT(".") + Name;
	}
	if (TObjectPtr<USoundWave>* C = Cache.Find(Path))
	{
		return *C;
	}
	USoundWave* W = LoadObject<USoundWave>(nullptr, *Path);
	if (!W)
	{
		UE_LOG(LogSlop, Warning, TEXT("Sound missing: %s"), *Name);
	}
	Cache.Add(Path, W);
	return W;
}

void USlopAudio::Play(const FString& Name, float VolumeDb, float Pitch, ESlopBus Bus)
{
	UWorld* W = World();
	USoundWave* S = Find(Name);
	if (!W || !S)
	{
		return;
	}
	const float Vol = FMath::Pow(10.f, VolumeDb / 20.f) * BusVolume(Bus);
	if (Vol <= 0.0005f)
	{
		return;
	}
	UGameplayStatics::PlaySound2D(W, S, Vol, Pitch, 0.f, Bus == ESlopBus::Ui ? UiLimit.Get() : EffectLimit.Get());
}

void USlopAudio::PlayMusic(const FString& Name)
{
	if (Name == MusicName && Music && Music->IsPlaying())
	{
		// The same track carries on, and a paused one (a match left from its pause menu) resumes.
		Music->SetPaused(false);
		return;
	}
	MusicName = Name;
	if (Music)
	{
		Music->FadeOut(0.6f, 0.f);
		Music = nullptr;
	}
	UWorld* W = World();
	USoundWave* S = Find(Name);
	if (!W || !S)
	{
		return;
	}
	Music = UGameplayStatics::CreateSound2D(W, S, FMath::Max(0.0001f, Master * MusicVol), 1.f, 0.f, nullptr, true, false);
	if (Music)
	{
		Music->bIsUISound = true;
		Music->FadeIn(0.8f, FMath::Max(0.0001f, Master * MusicVol));
	}
}

void USlopAudio::PauseMusic(bool bPause)
{
	if (Music)
	{
		Music->SetPaused(bPause);
	}
}

void USlopAudio::StopAll()
{
	if (Music)
	{
		Music->Stop();
		Music = nullptr;
	}
	MusicName.Reset();
}
