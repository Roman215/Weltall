#include "Formats/StrBitstream.h"
#include "Media/GameFileIndex.h"
#include "Media/MovieStream.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <initializer_list>
#include <vector>

using namespace Weltall;

// The Italian translation's movie patches re-encode the movies with their subtitles drawn in, in the original files'
// sectors; their video sectors are marked as data, some frames take ten sectors, and three movies end where the new
// encoding met the original's: a last frame cut short (dropped, as the console's drive would deliver it incomplete),
// and on disc 2 movie 8 with the Japanese sound the USA's last seven frames after one dropped. The tests' pairs hold
// one patch each: the English sound's with the US commands, the Japanese sound's with the Japanese commands.

namespace
{
	constexpr uint32 MovieSectorBytes = 2336;

	struct FPatchedMovie
	{
		uint32 Record = 0;
		/** The movie's last whole frame, and the one frame before it that no longer streams whole (0 for none). */
		uint32 LastFrame = 0;
		uint32 DroppedFrame = 0;
		/** The furthest frame the game plays of the movie: a field script's, the battle event's or the title's. */
		uint32 FurthestPlayed = 0;
	};

	/** Streams every frame of each movie, each bitstream expanded, and checks the frames are those listed. */
	void CheckMovies(const FIso9660& Disc, std::initializer_list<FPatchedMovie> Movies)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		for (const FPatchedMovie& Movie : Movies)
		{
			INFO("record " << Movie.Record);
			const FIndexRecord& File = Index->Records()[Movie.Record];
			FMovieStream Stream(Disc.Image(), File.Lba, static_cast<uint32>(File.Size) / MovieSectorBytes);
			std::vector<int16> Sound;
			std::vector<uint32> Frames;
			for (;;)
			{
				auto Frame = Stream.NextFrame(Sound);
				REQUIRE(Frame);
				if (!Frame->has_value())
				{
					break;
				}
				INFO("frame " << (*Frame)->FrameNumber);
				REQUIRE(DecodeStrBitstream((*Frame)->Bitstream, (*Frame)->Width, (*Frame)->Height));
				Frames.push_back((*Frame)->FrameNumber);
			}
			std::vector<uint32> Expected;
			for (uint32 Number = 1; Number <= Movie.LastFrame; ++Number)
			{
				if (Number != Movie.DroppedFrame)
				{
					Expected.push_back(Number);
				}
			}
			CHECK(Frames == Expected);
			CHECK(Movie.LastFrame >= Movie.FurthestPlayed);
			CHECK_FALSE(Sound.empty());
		}
	}
}

TEST_CASE("The Italian movie patch with the English sound streams every frame the game plays of its disc 1 movies", "[Media][Content]")
{
	REQUIRE_ITALIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckMovies(*Disc, {{2, 380, 0, 377}, {3, 369, 0, 367}, {6, 610, 0, 608}, {7, 303, 301, 300}, {8, 1062, 0, 1059},
		{9, 887, 0, 884}, {10, 523, 0, 520}, {14, 4466, 0, 4464}});
}

TEST_CASE("The Italian movie patch with the English sound streams every frame the game plays of its disc 2 movies", "[Media][Content]")
{
	REQUIRE_ITALIAN_DISC(Disc, EDiscRelease::UsaDisc2);
	CheckMovies(*Disc, {{1, 6725, 0, 6725}, {5, 3800, 0, 3800}, {9, 901, 0, 900}});
}

TEST_CASE("The Italian movie patch with the Japanese sound streams every frame the game plays of its disc 1 movies", "[Media][Content]")
{
	REQUIRE_ITALIAN_JP_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckMovies(*Disc, {{2, 380, 0, 377}, {3, 370, 0, 367}, {6, 610, 0, 608}, {7, 302, 0, 300}, {8, 1062, 0, 1059},
		{9, 887, 0, 884}, {10, 523, 0, 520}, {14, 4466, 0, 4464}});
}

TEST_CASE("The Italian movie patch with the Japanese sound streams every frame the game plays of its disc 2 movies", "[Media][Content]")
{
	REQUIRE_ITALIAN_JP_DISC(Disc, EDiscRelease::UsaDisc2);
	CheckMovies(*Disc, {{1, 6725, 0, 6725}, {5, 3800, 0, 3800}, {9, 901, 894, 900}});
}
