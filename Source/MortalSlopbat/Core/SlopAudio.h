#pragma once

#include "UObject/Object.h"
#include "Engine/StreamableManager.h"
#include "SlopAudio.generated.h"

class USoundAttenuation;
class USoundClass;
class USoundWave;
class UAudioComponent;
class USoundConcurrency;
class USubmixEffectDynamicsProcessorPreset;

UENUM()
enum class ESlopBus : uint8 { Effects, Voice, Ui, Music };

// Every sound the game plays: effects with a little pitch variety and a cap on how many play at
// once, one music loop at a time, volumes by bus from the settings. Sounds are looked up by
// short name ("hit_heavy", "kenney:impactPunch_heavy") and loaded once.
//
// Nearly every sound is mono, and the mix follows the speakers in use. On two channels (TV speakers,
// headphones, stereo speakers, most soundbars) they play down the middle, so nothing can cancel or
// go missing however the output folds them. With rear speakers, the fight spreads across the front
// by where each fighter stands, calls hold the centre, the music opens out behind the listener and
// heavy blows reach the subwoofer. A limiter on the main mix guards every layout against clipping.
UCLASS()
class USlopAudio : public UObject
{
	GENERATED_BODY()
public:
	void Init(UGameInstance* InOwner);
	void SetVolumes(float Master, float Music, float Effects, float Voice);
	// The speakers setting: "auto" follows the output in use, "stereo" and "surround" insist.
	void SetSpeakers(const FString& Setting);
	// Keeps the music's rear voices behind the listener. While `bQuiet` (no fight on screen), also
	// follows a change of output, such as another device chosen in the system's sound settings.
	void Tick(float Dt, bool bQuiet);
	bool IsSurround() const { return bSurround; }
	int32 OutputChannels() const { return OutputCount; }
	// Whether these speakers take the surround mix: any layout with rear channels.
	static bool WantsSurround(const FString& Setting, int32 Channels);
	// The channels of the default output in a sound server's report (`pactl info` then `pactl list
	// short sinks`); 2 when it cannot be told, the mix that is safe everywhere.
	static int32 DefaultSinkChannels(const FString& Report);
	// The direction a place across the screen (-1 left, 1 right) is heard from, in degrees.
	static float FrontAzimuth(float Pan);
	// Plays a one-shot. `Name` may name a family ("kenney:impactPunch_heavy") to pick one of its
	// numbered variants at random, never the same one twice running.
	void Play(const FString& Name, float VolumeDb = 0.f, float Pitch = 1.f, ESlopBus Bus = ESlopBus::Effects);
	// The same, from a place across the screen (-1 left, 1 right), heard as such with surround speakers.
	void PlayAt(float Pan, const FString& Name, float VolumeDb = 0.f, float Pitch = 1.f, ESlopBus Bus = ESlopBus::Effects);
	void PlayMusic(const FString& Name);
	void PauseMusic(bool bPause);
	void StopAll();
	USoundWave* Find(const FString& Name);
	// Streams every sound in the background at start-up, so a fight never waits on a load the
	// first time a variant plays.
	void Preload();

private:
	UWorld* World() const;
	void AskChannels();
	void RefreshOutput();
	void StartRears();
	void StopRears(float FadeSeconds);
	bool Listener(FVector& Location, FVector& Front, FVector& Right) const;
	void AddLimiter();
	void IndexFamilies();
	void OnPreloaded();
	bool bIndexed = false;
	TSharedPtr<FStreamableHandle> PreloadHandle;
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
	// The music's two rear voices, with surround speakers.
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicRear[2];
	// How a sound is placed around the listener: direction only, its level left alone.
	UPROPERTY()
	TObjectPtr<USoundAttenuation> Placement;
	// Sends a share of a placed sound to the subwoofer.
	UPROPERTY()
	TObjectPtr<USoundClass> Bleed;
	UPROPERTY()
	TObjectPtr<USubmixEffectDynamicsProcessorPreset> Limiter;
	FString Speakers = TEXT("auto");
	bool bSurround = false;
	// What the sound server last said of the default output, and what the mix was set for.
	int32 DeviceCount = 2;
	int32 OutputCount = 0;
	bool bAsking = false;
	bool bLimiterIn = false;
	float CheckIn = 0.f;
	FString MusicName;
	float Master = 0.8f, MusicVol = 0.5f, EffectsVol = 0.9f, VoiceVol = 0.9f;
};
