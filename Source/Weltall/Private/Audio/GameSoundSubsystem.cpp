#include "Audio/GameSoundSubsystem.h"

#include "Audio/GameSoundWave.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "WeltallLog.h"

bool UGameSoundSubsystem::Start(const Weltall::FIso9660& Disc)
{
	Stop();
	auto Started = Weltall::FGameSoundSession::Start(Disc);
	if (!Started)
	{
		UE_LOG(LogWeltall, Error, TEXT("The game's sound cannot start: %hs"), Started.Error().Message.c_str());
		return false;
	}
	GameSound = std::move(*Started);
	const UWorld* World = GetGameInstance()->GetWorld();
	if (World == nullptr || !World->GetAudioDevice().IsValid())
	{
		Unheard = std::make_unique<Weltall::FUnheardGameSound>(*GameSound->Sound());
		return true;
	}
	UGameSoundWave* Wave = NewObject<UGameSoundWave>(this);
	Wave->SetSound(GameSound->Sound());
	Output = UGameplayStatics::CreateSound2D(GetGameInstance(), Wave, 1.0f, 1.0f, 0.0f, nullptr, true, false);
	if (Output)
	{
		// A sample made waits on average half the wave's chunk, then the mixer's queued buffers (it keeps at least two)
		// and half the one it is mixing into, before the device has it.
		const FAudioDeviceHandle Device = World->GetAudioDevice();
		const double MixerFrames = (FMath::Max(Device->GetNumBuffers(), 2) + 0.5) * Device->GetBufferLength();
		const uint32 Latency = static_cast<uint32>(MixerFrames * Weltall::IGameSound::SampleRate / Device->GetSampleRate() + UGameSoundWave::ChunkFrames / 2);
		GameSound->Sound()->SetHeard(Latency);
		UE_LOG(LogWeltall, Log, TEXT("The game's sound is heard %u ms after it is made, besides the device's own latency"),
			Latency * 1000 / Weltall::IGameSound::SampleRate);
		Output->Play();
		Output->SetPaused(bHeld);
	}
	return true;
}

void UGameSoundSubsystem::SetHeld(bool bInHeld)
{
	if (bHeld != bInHeld)
	{
		bHeld = bInHeld;
		if (Output)
		{
			Output->SetPaused(bHeld);
		}
	}
}

void UGameSoundSubsystem::Stop()
{
	if (Output)
	{
		Output->Stop();
		Output = nullptr;
	}
	Unheard.reset();
	// The wave keeps the SPU and the driver for as long as the audio thread may still ask for samples.
	GameSound.reset();
}

void UGameSoundSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

void UGameSoundSubsystem::Tick(float DeltaTime)
{
	if (!bHeld)
	{
		Unheard->Advance(DeltaTime);
	}
}

ETickableTickType UGameSoundSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGameSoundSubsystem::IsTickable() const
{
	return Unheard != nullptr;
}

UWorld* UGameSoundSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

TStatId UGameSoundSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameSoundSubsystem, STATGROUP_Tickables);
}
