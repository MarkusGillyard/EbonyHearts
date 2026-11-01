#include "Core/SlopAudio.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "AudioDeviceManager.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "Core/SlopLog.h"

static const TCHAR* OriginalDir = TEXT("/Game/Slop/Audio/Original/");

namespace SlopAudioImpl
{
	// The speakers either side of a screen stand 30 degrees off centre, so the screen's width is theirs.
	constexpr float FrontArc = 30.f;
	// Where 5.1's rear speakers stand; the music's rear voices go straight to them.
	constexpr float RearAngle = 150.f;
	// Each rear voice, under the front: an ambience behind the listener, not a second band.
	constexpr float RearLevel = 0.42f;
	// A mono track played twice into two rears is one sound in the middle of the head. A few
	// milliseconds apart, and not the same few, they spread into a space around the listener.
	constexpr float RearDelay[2] = {0.013f, 0.021f};
	// Far enough that the panner reads a direction, not a place on top of the listener.
	constexpr float Reach = 400.f;
	// Share of a placed sound sent to the subwoofer, whose amplifier filters it to the low end.
	constexpr float SubBleed = 0.15f;
	// A placed sound's full level lands on one speaker, where stereo shares it between two.
	constexpr float PlacedLevel = 0.707f;
	constexpr float CheckEvery = 5.f;
}

using namespace SlopAudioImpl;

void USlopAudio::Init(UGameInstance* InOwner)
{
	Owner = InOwner;
	EffectLimit = NewObject<USoundConcurrency>(this);
	EffectLimit->Concurrency.MaxCount = 24;
	EffectLimit->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	UiLimit = NewObject<USoundConcurrency>(this);
	UiLimit->Concurrency.MaxCount = 4;
	UiLimit->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	Placement = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Placement->Attenuation;
	A.bAttenuate = false;
	A.bSpatialize = true;
	A.bAttenuateWithLPF = false;
	A.bEnableListenerFocus = false;
	A.bEnableOcclusion = false;
	A.bEnableReverbSend = false;
	A.NonSpatializedRadiusStart = 0.f;
	A.NonSpatializedRadiusEnd = 0.f;
	Bleed = NewObject<USoundClass>(this);
	Bleed->Properties.LFEBleed = SubBleed;
	if (GEngine && GEngine->GetAudioDeviceManager())
	{
		// A class made at run time is not found by the mix until it is registered.
		GEngine->GetAudioDeviceManager()->RegisterSoundClass(Bleed);
	}
}

void USlopAudio::AddLimiter()
{
	FSubmixEffectDynamicsProcessorSettings L;
	L.DynamicsProcessorType = ESubmixEffectDynamicsProcessorType::Limiter;
	L.PeakMode = ESubmixEffectDynamicsPeakMode::Peak;
	L.LinkMode = ESubmixEffectDynamicsChannelLinkMode::Peak;
	// Under full scale by what a fold of the channels onto fewer speakers can add back, looking ahead
	// so a blow's first sample is caught; linked, so a loud hit on one side cannot pull the picture
	// across the speakers.
	L.ThresholdDb = -3.f;
	// The defaults let a blow through: a 10 dB soft knee, and an analog response that has not risen
	// by the end of the look-ahead.
	L.KneeBandwidthDb = 0.f;
	L.bAnalogMode = false;
	L.LookAheadMsec = 3.f;
	L.AttackTimeMsec = 1.f;
	L.ReleaseTimeMsec = 120.f;
	Limiter = NewObject<USubmixEffectDynamicsProcessorPreset>(this);
	Limiter->SetSettings(L);
	UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(World(), Limiter);
}

bool USlopAudio::WantsSurround(const FString& Setting, int32 Channels)
{
	if (Setting == TEXT("stereo"))
	{
		return false;
	}
	return Setting == TEXT("surround") || Channels >= 4;
}

float USlopAudio::FrontAzimuth(float Pan) { return FMath::Clamp(Pan, -1.f, 1.f) * FrontArc; }

int32 USlopAudio::DefaultSinkChannels(const FString& Report)
{
	TArray<FString> Lines;
	Report.ParseIntoArrayLines(Lines);
	FString Sink;
	for (const FString& L : Lines)
	{
		if (L.StartsWith(TEXT("Default Sink: ")))
		{
			Sink = L.Mid(14).TrimStartAndEnd();
		}
	}
	for (const FString& L : Lines)
	{
		// id, name, driver, format, channels, rate, state.
		TArray<FString> Fields;
		L.ParseIntoArrayWS(Fields);
		if (Fields.Num() >= 5 && Fields[1] == Sink && Fields[4].EndsWith(TEXT("ch")))
		{
			return FMath::Max(1, FCString::Atoi(*Fields[4]));
		}
	}
	return 2;
}

void USlopAudio::AskChannels()
{
	if (bAsking)
	{
		return;
	}
	bAsking = true;
	// The engine always opens a 5.1 stream, so SDL's report of the device is only an echo of that;
	// the sound server knows the truth. On its own thread: starting a process can take a moment.
	TWeakObjectPtr<USlopAudio> Self(this);
	Async(EAsyncExecution::Thread, [Self]()
	{
		FString Report, Errors;
		int32 Code = -1;
		FPlatformProcess::ExecProcess(TEXT("/bin/sh"), TEXT("-c \"LC_ALL=C pactl info; LC_ALL=C pactl list short sinks\""), &Code, &Report, &Errors);
		const int32 Channels = DefaultSinkChannels(Report);
		AsyncTask(ENamedThreads::GameThread, [Self, Channels]()
		{
			if (USlopAudio* A = Self.Get())
			{
				A->bAsking = false;
				A->DeviceCount = Channels;
				A->RefreshOutput();
			}
		});
	});
}

void USlopAudio::SetSpeakers(const FString& Setting)
{
	Speakers = Setting;
	RefreshOutput();
	AskChannels();
}

void USlopAudio::RefreshOutput()
{
	const int32 Channels = DeviceCount;
	const bool bWant = WantsSurround(Speakers, Channels);
	if (Channels == OutputCount && bWant == bSurround)
	{
		return;
	}
	OutputCount = Channels;
	const bool bChanged = bWant != bSurround;
	bSurround = bWant;
	UE_LOG(LogSlop, Log, TEXT("Sound output: %d channels, %s mix"), Channels, bSurround ? TEXT("surround") : TEXT("stereo"));
	if (bChanged)
	{
		if (bSurround)
		{
			StartRears();
		}
		else
		{
			StopRears(0.3f);
		}
	}
}

bool USlopAudio::Listener(FVector& Location, FVector& Front, FVector& Right) const
{
	const UWorld* W = World();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	PC->GetAudioListenerPosition(Location, Front, Right);
	return true;
}

void USlopAudio::StartRears()
{
	UWorld* W = World();
	USoundWave* S = Music ? Cast<USoundWave>(Music->Sound) : nullptr;
	FVector Where, Front, Right;
	if (!W || !S || !bSurround || !Listener(Where, Front, Right))
	{
		return;
	}
	const float Level = FMath::Max(0.0001f, Master * MusicVol);
	for (int32 I = 0; I < 2; ++I)
	{
		MusicRear[I] = UGameplayStatics::SpawnSoundAtLocation(W, S, Where, FRotator::ZeroRotator, Level * RearLevel, 1.f, RearDelay[I], Placement, nullptr, false);
		if (MusicRear[I])
		{
			MusicRear[I]->bIsUISound = true;
			// A fade's level multiplies the voice's own: the same as the front's, so the two stay RearLevel apart.
			MusicRear[I]->FadeIn(0.8f, Level, RearDelay[I]);
		}
	}
	Tick(0.f, false);
}

void USlopAudio::StopRears(float FadeSeconds)
{
	for (TObjectPtr<UAudioComponent>& R : MusicRear)
	{
		if (R)
		{
			R->FadeOut(FadeSeconds, 0.f);
			R = nullptr;
		}
	}
}

void USlopAudio::Tick(float Dt, bool bQuiet)
{
	// The mixer is reached through a world, and there is none yet when the game instance starts.
	if (!bLimiterIn && World())
	{
		bLimiterIn = true;
		AddLimiter();
	}
	if (bQuiet && (CheckIn -= Dt) <= 0.f)
	{
		CheckIn = CheckEvery;
		AskChannels();
	}
	FVector Where, Front, Right;
	if (MusicRear[0] && Listener(Where, Front, Right))
	{
		const float Sin = FMath::Sin(FMath::DegreesToRadians(RearAngle));
		const float Cos = FMath::Cos(FMath::DegreesToRadians(RearAngle));
		for (int32 I = 0; I < 2; ++I)
		{
			if (MusicRear[I])
			{
				MusicRear[I]->SetWorldLocation(Where + (Front * Cos + Right * (I == 0 ? -Sin : Sin)) * Reach);
			}
		}
	}
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
	for (UAudioComponent* R : MusicRear)
	{
		if (R)
		{
			R->SetVolumeMultiplier(FMath::Max(0.0001f, Master * MusicVol) * RearLevel);
		}
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
	PlayAt(0.f, Name, VolumeDb, Pitch, Bus);
}

void USlopAudio::PlayAt(float Pan, const FString& Name, float VolumeDb, float Pitch, ESlopBus Bus)
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
	FVector Where, Front, Right;
	if (bSurround && Bus != ESlopBus::Ui && Listener(Where, Front, Right))
	{
		const float Angle = FMath::DegreesToRadians(FrontAzimuth(Pan));
		S->SoundClassObject = Bleed;
		UGameplayStatics::PlaySoundAtLocation(W, S, Where + (Front * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Reach, FRotator::ZeroRotator, Vol * PlacedLevel, Pitch, 0.f, Placement, EffectLimit);
		return;
	}
	S->SoundClassObject = nullptr;
	UGameplayStatics::PlaySound2D(W, S, Vol, Pitch, 0.f, Bus == ESlopBus::Ui ? UiLimit.Get() : EffectLimit.Get());
}

void USlopAudio::PlayMusic(const FString& Name)
{
	if (Name == MusicName && Music && Music->IsPlaying())
	{
		// The same track carries on, and a paused one (a match left from its pause menu) resumes.
		PauseMusic(false);
		return;
	}
	MusicName = Name;
	if (Music)
	{
		Music->FadeOut(0.6f, 0.f);
		Music = nullptr;
	}
	StopRears(0.6f);
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
		StartRears();
	}
}

void USlopAudio::PauseMusic(bool bPause)
{
	if (Music)
	{
		Music->SetPaused(bPause);
	}
	for (UAudioComponent* R : MusicRear)
	{
		if (R)
		{
			R->SetPaused(bPause);
		}
	}
}

void USlopAudio::StopAll()
{
	if (Music)
	{
		Music->Stop();
		Music = nullptr;
	}
	for (TObjectPtr<UAudioComponent>& R : MusicRear)
	{
		if (R)
		{
			R->Stop();
			R = nullptr;
		}
	}
	MusicName.Reset();
}
