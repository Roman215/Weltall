#include "Game/MovieProgram.h"

#include "Formats/Lzss.h"
#include "Foundation/Sha1.h"
#include "Game/GameSession.h"
#include "Media/GameFileIndex.h"
#include "Sound/GameSoundSession.h"

#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** The request's kinds: the movie with its sound, and its sound alone, which the retail program never plays. */
		constexpr uint8 WithSound = 1;
		constexpr uint8 SoundAlone = 2;
		/** L1, R1, Start and Select together start the console again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		/** The Movie program's file, and the SHA-1 of the program unpacked as the Italian translation's Japanese commands have it. */
		constexpr int32 ProgramDirectory = 1;
		constexpr int32 ProgramFile = 18;
		constexpr std::string_view CircleSkippingProgram = "657b2b8037bbd0b5f1eee346d608064b99654295";

		/** Whether the disc's Movie program is the one a Circle skips (FMoviePlayback::bCircleSkips). */
		TResult<bool> SkipsOnCircle(const FGameSession& Session)
		{
			const FIndexRecord* Record = Session.Index().Find(ProgramDirectory, ProgramFile);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks the Movie program");
			}
			const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Session.Image(), *Record);
			if (!Sectors)
			{
				return Sectors.Error();
			}
			const TResult<std::vector<uint8>> Program = UnpackLzss(*Sectors);
			if (!Program)
			{
				return Program.Error();
			}
			return FSha1::ToHex(FSha1::Hash(*Program)) == CircleSkippingProgram;
		}
	}

	TResult<std::unique_ptr<FMovieProgram>> FMovieProgram::Start(FGameSession& Session)
	{
		std::unique_ptr<FMovieProgram> Program(new FMovieProgram(Session));
		const FMovieProgramRequest& Request = Session.Kernel().Movie;
		// The CD volume goes to nothing before anything plays.
		Session.Sound().Sound()->Lock()->SetCdVolume(0, 0);
		if (Request.Type == FMovieProgramRequest::NoMovie)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Movie)) + " asked for no movie shows its debug menu, which the port does not run");
		}
		const uint8 Kind = Request.Type & FMovieProgramRequest::KindBits;
		if (Kind == SoundAlone)
		{
			Program->End();
			return Program;
		}
		if (Kind != WithSound)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Movie)) + ": movie " + std::to_string(Request.Number) + " of kind " +
							 std::to_string(Kind) + " is read from a layout of the disc the retail game does not have");
		}
		const TResult<FGameDisc> Disc = Session.MovieDisc();
		if (!Disc)
		{
			return Disc.Error();
		}
		const std::optional<FMovieLocation> Location = FindMovie(*Disc->Index, Request.Number);
		if (!Location)
		{
			// Past the disc's movies: none is played, and the program goes on to the next.
			Program->End();
			return Program;
		}
		if (Location->SectorCount == 0)
		{
			// The disc's stand-in for a movie, too short to hold a sector.
			Session.Kernel().Movie.NextProgram = EGameProgram::DebugMenu;
			Program->End();
			return Program;
		}
		FMoviePlayback Playback;
		Playback.EndFrame = (Request.Type & FMovieProgramRequest::EndFrameGiven) != 0 ? Request.EndFrame : FMoviePlayback::DefaultEndFrame;
		Playback.bSkippable = Request.NoSkip == 0;
		const TResult<bool> CircleSkips = SkipsOnCircle(Session);
		if (!CircleSkips)
		{
			return CircleSkips.Error();
		}
		Playback.bCircleSkips = *CircleSkips;
		TResult<std::optional<FMovieSubtitles>> Subtitles = Session.MovieSubtitles(Request.Number);
		if (!Subtitles)
		{
			return Subtitles.Error();
		}
		Program->MoviePlayer = std::make_unique<FMoviePlayer>(*Disc->Image, *Location, Playback, *Session.Sound().Sound(), std::move(*Subtitles));
		const TResult<bool> Started = Program->MoviePlayer->Start();
		if (!Started)
		{
			return Started.Error();
		}
		if (!*Started)
		{
			Program->End();
		}
		return Program;
	}

	std::optional<FError> FMovieProgram::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (ProgramState != EGameProgramState::Running)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Movie)) + " has ended");
		}
		for (const FPadState Held : HeldPerVblank)
		{
			Session.Vblank(Held);
			const TResult<bool> Playing = MoviePlayer->Vblank(Held);
			if (!Playing)
			{
				return Playing.Error();
			}
			if (Session.Pad().Held() == SoftResetButtons)
			{
				MoviePlayer->Stop();
				ProgramState = EGameProgramState::SoftReset;
				return std::nullopt;
			}
			if (!*Playing)
			{
				End();
				return std::nullopt;
			}
		}
		return std::nullopt;
	}

	void FMovieProgram::End()
	{
		Session.Kernel().NextProgram = Session.Kernel().Movie.NextProgram;
		ProgramState = EGameProgramState::Ended;
	}
}
