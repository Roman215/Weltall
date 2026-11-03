#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"

#include <memory>

#include "GameSoundWave.generated.h"

namespace Weltall
{
	class IGameSound;
}

/**
 * A game's sound as the audio mixer plays it: 44.1 kHz stereo made on the audio render thread by the
 * game's own sound, for as long as the wave plays.
 */
UCLASS()
class WELTALL_API UGameSoundWave final : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	/** The frames the game's sound makes at a time: a few milliseconds, so that what it makes is soon heard. */
	static constexpr int32 ChunkFrames = 512;

	UGameSoundWave(const FObjectInitializer& ObjectInitializer);

	/** The sound the wave plays; set before it starts. */
	void SetSound(std::shared_ptr<Weltall::IGameSound> InSound);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	std::shared_ptr<Weltall::IGameSound> Sound;
};
