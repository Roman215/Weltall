#include "Media/MoviePlayer.h"

#include "Formats/StrBitstream.h"
#include "Sound/GameSoundSystem.h"

namespace Weltall
{
	namespace
	{
		/** The sound read ahead of the picture, and the most pictures decoded ahead to get it. */
		constexpr std::size_t SoundLeadFrames = IGameSound::SampleRate / 2;
		constexpr std::size_t MaxPicturesAhead = 16;
		/** The player's CD volume fades take 40 of the driver's 120 Hz steps, a skip's 10. */
		constexpr int32 FullCdVolume = 0x7FFF;
		constexpr int32 FadeSteps = 40;
		constexpr uint32 FadeOutFrames = 3;
		constexpr int32 SkipFadeSteps = 10;
		constexpr uint16 CrossBit = FPadState::Bit(EPadButton::Cross);
		constexpr uint16 CircleBit = FPadState::Bit(EPadButton::Circle);
		constexpr uint16 StartBit = FPadState::Bit(EPadButton::Start);
		constexpr int32 FirstRepeatDelay = 90;
		/**
		 * The drive delivers 15 pictures a second; a vertical blank is 263 lines of 3413 clocks of the
		 * NTSC GPU's 53,693,175 Hz. The movie's time counts 1 / (44,100 x 53,693,175) seconds, so a
		 * vertical blank, a sound frame and a picture are each a whole number of them.
		 */
		constexpr int64 PicturesPerSecond = 15;
		constexpr int64 VblankClocks = 263 * 3413;
		constexpr int64 GpuClockHz = 53693175;
		constexpr int64 SoundRate = IGameSound::SampleRate;
		constexpr int64 FrameUnits = GpuClockHz;
		constexpr int64 VblankUnits = VblankClocks * SoundRate;
		constexpr int64 PictureFrames = SoundRate / PicturesPerSecond;
		constexpr int64 PictureUnits = PictureFrames * FrameUnits;
		/**
		 * Steering the time to the sound heard: a picture's time apart or more is made up at once (a stall); less is
		 * made up an eighth a vertical blank, which smooths the host's sound taken in bursts; 4 ms or less is left.
		 */
		constexpr int64 SettledUnits = SoundRate * 4 / 1000 * FrameUnits;
		constexpr int64 SteeringDivisor = 8;
		/** Sound waiting and not taken for half a second: nothing plays it (a host without an audio thread). */
		constexpr uint64 UntakenVblanks = 30;

		uint32 PicturesDue(int64 Clock)
		{
			return Clock < 0 ? 1 : static_cast<uint32>(Clock / PictureUnits) + 1;
		}
	}

	FMoviePlayer::FMoviePlayer(const FCdImage& InImage, FMovieLocation InLocation, const FMoviePlayback& InPlayback, FGameSoundSystem& InSound,
		std::optional<FMovieSubtitles> InSubtitles)
		: Image(InImage), Location(InLocation), Playback(InPlayback), Sound(InSound), Subtitles(std::move(InSubtitles))
	{
	}

	FMoviePlayer::~FMoviePlayer()
	{
		if (bStarted && !bFinished)
		{
			StopSound();
		}
	}

	TResult<bool> FMoviePlayer::Start()
	{
		if (bStarted)
		{
			return !bFinished;
		}
		bStarted = true;
		Sound.Lock()->SetCdVolume(0, 0);
		Rewind();
		return ShowDue();
	}

	TResult<bool> FMoviePlayer::Vblank(FPadState Held)
	{
		if (!bStarted)
		{
			const TResult<bool> Started = Start();
			if (!Started || !*Started)
			{
				return Started;
			}
		}
		if (bFinished)
		{
			return false;
		}
		if (Playback.bSkippable)
		{
			PollSkip(Held);
		}
		if (StopVblanks > 0 && --StopVblanks == 0)
		{
			Finish();
			return false;
		}
		++Vblanks;
		AdvanceClock();
		return bPicturesStopped ? true : ShowDue();
	}

	void FMoviePlayer::Skip()
	{
		if (bFinished || StopVblanks > 0)
		{
			return;
		}
		Sound.Lock()->SetCdVolume(0, SkipFadeSteps);
		// The program only waits out the fade: no picture follows.
		StopVblanks = SkipVblanks;
		Playback.bSkippable = false;
		bPicturesStopped = true;
	}

	void FMoviePlayer::Stop()
	{
		if (bStarted && !bFinished)
		{
			Finish();
		}
	}

	void FMoviePlayer::AdvanceClock()
	{
		Clock += VblankUnits;
		const std::optional<int64> Heard = HeardTime();
		if (!Heard)
		{
			return;
		}
		const int64 Behind = *Heard - Clock;
		if (Behind >= PictureUnits || Behind <= -PictureUnits)
		{
			Clock = *Heard;
		}
		else if (Behind > SettledUnits || Behind < -SettledUnits)
		{
			Clock += Behind / SteeringDivisor;
		}
	}

	std::optional<int64> FMoviePlayer::HeardTime()
	{
		const std::optional<uint32> Latency = Sound.HeardLatency();
		if (!Latency || SoundQueued == 0)
		{
			return std::nullopt;
		}
		const std::size_t Waiting = Sound.Lock().Spu().QueuedCdInput();
		const uint64 Taken = Waiting < SoundQueued ? SoundQueued - Waiting : 0;
		if (Taken != SoundTaken)
		{
			SoundTaken = Taken;
			SoundTakenVblank = Vblanks;
		}
		// Nothing has taken the sound, or nothing lately while it waits, or all of it is taken: the vertical blanks keep the time.
		if (Taken == 0 || Waiting == 0 || Vblanks - SoundTakenVblank > UntakenVblanks)
		{
			return std::nullopt;
		}
		return (SoundOrigin + static_cast<int64>(Taken) - static_cast<int64>(*Latency)) * FrameUnits;
	}

	TResult<bool> FMoviePlayer::ShowDue()
	{
		const uint32 Due = PicturesDue(Clock);
		while (!bFinished && PicturesShown < Due)
		{
			if (Decoded.empty())
			{
				if (bStreamEnded)
				{
					break;
				}
				const TResult<bool> Next = DecodeNext();
				if (!Next)
				{
					Finish();
					return Next.Error();
				}
				continue;
			}
			ShownFrame = Decoded.front().FrameNumber;
			Shown = std::move(Decoded.front().Picture);
			ShownSubtitle = std::move(Decoded.front().Subtitle);
			Decoded.pop_front();
			++PicturesShown;
			FadeFor(ShownFrame);
			if (ShownFrame >= Playback.EndFrame)
			{
				// The player stops as soon as it shows the end frame, which stays on screen; a rewinding
				// movie reads from its start again instead.
				if (!Playback.bRewinds)
				{
					Finish();
					return false;
				}
				Rewind();
			}
		}
		if (bFinished)
		{
			return false;
		}
		if (const TResult<bool> Read = ReadAhead(); !Read)
		{
			Finish();
			return Read.Error();
		}
		if (bStreamEnded && Decoded.empty() && PicturesDue(Clock) > PicturesShown)
		{
			// The last picture has had its time on screen.
			Finish();
		}
		return !bFinished;
	}

	TResult<bool> FMoviePlayer::ReadAhead()
	{
		std::size_t Queued = Sound.Lock().Spu().QueuedCdInput();
		while (!bStreamEnded && Queued < SoundLeadFrames && Decoded.size() < MaxPicturesAhead)
		{
			const TResult<bool> Next = DecodeNext();
			if (!Next)
			{
				return Next.Error();
			}
			Queued = Sound.Lock().Spu().QueuedCdInput();
		}
		return true;
	}

	void FMoviePlayer::FadeFor(uint32 Frame)
	{
		if (!bFadedIn && Frame > Playback.StartFrame)
		{
			bFadedIn = true;
			Sound.Lock()->SetCdVolume(FullCdVolume, FadeSteps);
		}
		if (!bFadingOut && Frame + FadeOutFrames >= Playback.EndFrame)
		{
			bFadingOut = true;
			Sound.Lock()->SetCdVolume(0, FadeSteps);
		}
	}

	void FMoviePlayer::Rewind()
	{
		// Reading starts over at the start sector: the drive's sound is cut and silent until the next fade-in.
		if (Stream)
		{
			StopSound();
		}
		const uint32 First = Location.FirstLba + std::min(Playback.StartSector, Location.SectorCount);
		Stream.emplace(Image, First, Location.SectorCount - (First - Location.FirstLba));
		Decoded.clear();
		SoundOrigin = Clock / FrameUnits;
		SoundQueued = 0;
		SoundTaken = 0;
		SoundTakenVblank = Vblanks;
		// The undub's subtitles start again as the player asks for the movie's file again.
		if (Subtitles)
		{
			Subtitles->Restart();
		}
		bStreamEnded = false;
		bFadedIn = false;
		bFadingOut = false;
	}

	void FMoviePlayer::StopSound()
	{
		const FLockedGameSound Locked = Sound.Lock();
		Locked->SetCdVolume(0, 0);
		Locked.Spu().ClearCdInput();
	}

	void FMoviePlayer::Finish()
	{
		bFinished = true;
		Decoded.clear();
		StopSound();
	}

	TResult<bool> FMoviePlayer::DecodeNext()
	{
		SoundScratch.clear();
		TResult<std::optional<FStrFrame>> Frame = Stream->NextFrame(SoundScratch);
		if (!Frame)
		{
			return Frame.Error();
		}
		if (Playback.bSound)
		{
			SoundQueued += Sound.Lock().Spu().QueueCdInput(SoundScratch);
		}
		if (!Frame->has_value())
		{
			bStreamEnded = true;
			return false;
		}
		const FStrFrame& Compressed = **Frame;
		if (Compressed.FrameNumber < Playback.StartFrame)
		{
			// Its sound comes before the first picture shown.
			SoundOrigin -= PictureFrames;
			return true;
		}
		const TResult<std::vector<uint16>> Codes = DecodeStrBitstream(Compressed.Bitstream, Compressed.Width, Compressed.Height);
		if (!Codes)
		{
			return Codes.Error();
		}
		TResult<FMdecPicture> Picture = DecodeMdecPicture(*Codes, Compressed.Width, Compressed.Height, FMdecTables::Standard());
		if (!Picture)
		{
			return Picture.Error();
		}
		Decoded.push_back({std::make_shared<const FMdecPicture>(std::move(*Picture)), Compressed.FrameNumber,
			Subtitles ? Subtitles->At(Compressed.FrameNumber) : nullptr});
		return true;
	}

	void FMoviePlayer::PollSkip(FPadState Buttons)
	{
		// The program reads the pad with an auto-repeat: buttons held unchanged count as pressed again
		// once they have been read unchanged more than 90 times, then every other reading.
		uint32 Previous = Polled;
		Polled = Buttons.Buttons;
		if (Previous == Polled && Polled != 0)
		{
			if (++RepeatCount > RepeatDelay)
			{
				Previous = 0;
				RepeatCount = 0;
				RepeatDelay = 1;
			}
		}
		else
		{
			RepeatCount = 0;
			RepeatDelay = FirstRepeatDelay;
		}
		// Each button's test is of its bit not read before and read now; the translation's tests Circle now, after Cross before.
		const uint32 Now = Playback.bCircleSkips ? CircleBit : CrossBit;
		const bool bSkip = ((Polled & ~Previous) & StartBit) != 0 || ((Previous & CrossBit) == 0 && (Polled & Now) != 0);
		if (StopVblanks == 0 && bSkip)
		{
			Sound.Lock()->SetCdVolume(0, SkipFadeSteps);
			StopVblanks = SkipVblanks;
		}
	}
}
