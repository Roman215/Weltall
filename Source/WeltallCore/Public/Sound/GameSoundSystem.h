#pragma once

#include "Sound/GameSound.h"
#include "Sound/GameSoundClock.h"
#include "Sound/SoundDriver.h"
#include "Sound/SoundVolumes.h"
#include "Sound/Spu.h"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>

namespace Weltall
{
	class FGameSoundSystem;

	/**
	 * The game's sound, held by the game thread: while this lives, the audio thread waits before its
	 * next interrupt. Hold it for a few calls, never across a frame.
	 */
	class FLockedGameSound
	{
	public:
		FSoundDriver& Driver() const { return *DriverPointer; }
		FSoundDriver* operator->() const { return DriverPointer; }
		FSpu& Spu() const { return *SpuPointer; }

	private:
		friend class FGameSoundSystem;

		FLockedGameSound(std::mutex& Mutex, FSoundDriver& InDriver, FSpu& InSpu)
			: Lock(Mutex), DriverPointer(&InDriver), SpuPointer(&InSpu)
		{
		}

		std::unique_lock<std::mutex> Lock;
		FSoundDriver* DriverPointer;
		FSpu* SpuPointer;
	};

	/**
	 * The game's sound as the player hears it: the SPU, with the game's sound driver running on its
	 * timer between the SPU's samples from the moment the game starts it. The game's code calls the
	 * driver from its own thread through Lock(): as on the console, a call takes effect at the next
	 * interrupt while what it returns is current (an effect just started is playing). The audio
	 * thread makes the samples, holding the lock for one interrupt and the samples up to the next at
	 * a time, so neither side keeps the other waiting long; it allocates nothing.
	 */
	class FGameSoundSystem final : public IGameSound
	{
	public:
		/** Starts the SPU and the driver as the game's start-up does. */
		WELTALLCORE_API explicit FGameSoundSystem(const FSoundDriverTables& Tables, const FSoundDriverOptions& Options = {});

		/** Game thread: the driver and the SPU, for a few calls. */
		WELTALLCORE_API FLockedGameSound Lock();

		/**
		 * Game thread: the player's volumes, from the next samples on. Each voice is heard at its kind's volume as of its
		 * last note (ESoundKind), the replacement music at the music's, the CD input at the movies', and all of it at the
		 * master volume.
		 */
		WELTALLCORE_API void SetVolumes(const FSoundVolumes& Volumes);

		/** Audio thread: the next samples, the driver's interrupts run among them as they fall due. */
		WELTALLCORE_API virtual void Render(std::span<int16> Stereo) override;

		/**
		 * Any thread: the host plays the samples to a listener, each heard LatencyFrames frames on average after it is
		 * made, by its own buffering of them. Until it says so, nothing hears them (a host without an audio device).
		 */
		void SetHeard(uint32 LatencyFrames) { HeardLatencyFrames.store(LatencyFrames, std::memory_order_relaxed); }
		/** How long a sample just made waits before it is heard, in frames; none while nothing hears the sound. */
		std::optional<uint32> HeardLatency() const
		{
			const int64 Frames = HeardLatencyFrames.load(std::memory_order_relaxed);
			return Frames < 0 ? std::nullopt : std::optional<uint32>(static_cast<uint32>(Frames));
		}

	private:
		/** The voices' gains after an interrupt, which may have started notes of another kind on them. */
		void SetVoiceGains();

		std::mutex Mutex;
		FSpu Spu;
		FSoundDriver Driver;
		FGameSoundClock Clock;
		/** The volumes as gains: master, then each ESoundKind's. */
		int32 MasterGain = FSpu::FullGain;
		std::array<int32, 3> KindGains{FSpu::FullGain, FSpu::FullGain, FSpu::FullGain};
		std::atomic<int64> HeardLatencyFrames{-1};
	};
}
