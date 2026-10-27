#pragma once

#include "UObject/Object.h"
#include "SlopAudio.generated.h"

class USoundWave;
class UAudioComponent;
class USoundConcurrency;

UENUM()
enum class ESlopBus : uint8 { Effects, Voice, Ui, Music };

// Every sound the game plays: effects with a little pitch variety and a cap on how many play at
// once, one music loop at a time, volumes by bus from the settings. Sounds are looked up by
// short name ("hit_heavy", "kenney:impactPunch_heavy") and loaded once.
UCLASS()
class USlopAudio : public UObject
{
	GENERATED_BODY()
public:
	void Init(UGameInstance* InOwner);
	void SetVolumes(float Master, float Music, float Effects, float Voice);
	// Plays a one-shot. `Name` may name a family ("kenney:impactPunch_heavy") to pick one of its
	// numbered variants at random, never the same one twice running.
	void Play(const FString& Name, float VolumeDb = 0.f, float Pitch = 1.f, ESlopBus Bus = ESlopBus::Effects);
	void PlayMusic(const FString& Name);
	void PauseMusic(bool bPause);
	void StopAll();
	USoundWave* Find(const FString& Name);

private:
	UWorld* World() const;
	float BusVolume(ESlopBus Bus) const;
	UPROPERTY()
	TObjectPtr<UGameInstance> Owner;
	UPROPERTY()
	TMap<FString, TObjectPtr<USoundWave>> Cache;
	TMap<FString, TArray<FString>> Families;
	TMap<FString, int32> LastVariant;
	UPROPERTY()
	TObjectPtr<UAudioComponent> Music;
	UPROPERTY()
	TObjectPtr<USoundConcurrency> EffectLimit;
	UPROPERTY()
	TObjectPtr<USoundConcurrency> UiLimit;
	FString MusicName;
	float Master = 0.8f, MusicVol = 0.5f, EffectsVol = 0.9f, VoiceVol = 0.9f;
};
