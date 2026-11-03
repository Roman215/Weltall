#include "Audio/GameSoundWave.h"

#include "Sound/GameSound.h"

UGameSoundWave::UGameSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	SetSampleRate(Weltall::IGameSound::SampleRate);
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	// The engine's desktop default makes 93 ms at a time, which every sound would wait through before it is heard.
	NumSamplesToGeneratePerCallback = ChunkFrames * NumChannels;
	// The game's sound code goes on while its sound is silent, as it does on the console.
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	bCanProcessAsync = true;
}

void UGameSoundWave::SetSound(std::shared_ptr<Weltall::IGameSound> InSound)
{
	Sound = MoveTemp(InSound);
}

int32 UGameSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	if (Sound)
	{
		Sound->Render(std::span<int16>(reinterpret_cast<int16*>(OutAudio.GetData()), NumSamples));
	}
	return NumSamples;
}
