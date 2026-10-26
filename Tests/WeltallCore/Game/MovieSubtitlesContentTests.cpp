#include "Game/GameSession.h"
#include "Media/GameMovies.h"
#include "Media/MoviePlayer.h"
#include "Mods/PerfectWorks.h"
#include "Support/AppleDouble.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

using namespace Weltall;

namespace
{
	FGameFileIndex IndexOf(const FIso9660& Disc)
	{
		auto Program = ReadBootExecutable(Disc);
		REQUIRE(Program);
		auto Index = FGameFileIndex::Read(*Program);
		REQUIRE(Index);
		return std::move(*Index);
	}

	void SetSubtitles(FGameSession& Session, EVoiceLanguage Voices, EMovieSubtitles Subtitles)
	{
		FPlayerSettings Settings = Session.Settings();
		Settings.VoiceLanguage = Voices;
		Settings.MovieSubtitles = Subtitles;
		Session.SetSettings(Settings);
	}

	bool HasSubtitles(const FGameSession& Session, int32 Movie)
	{
		const auto Subtitles = Session.MovieSubtitles(Movie);
		REQUIRE(Subtitles);
		return Subtitles->has_value();
	}
}

TEST_CASE("The supported undubs subtitle the movies their patch lists, every line's picture in its file", "[Media][Content][Voices]")
{
	const std::map<EDiscRelease, std::map<int32, std::size_t>> Expected{
		{EDiscRelease::UndubDisc1, {{1, 1}, {2, 6}, {5, 2}, {6, 2}, {7, 8}, {8, 15}, {9, 14}, {13, 43}}},
		{EDiscRelease::UndubDisc2, {{0, 8}, {4, 40}, {8, 6}}},
	};
	for (const auto& [Release, Movies] : Expected)
	{
		REQUIRE_DISC(Disc, Release);
		INFO(DiscReleaseId(Release));
		const FGameFileIndex Index = IndexOf(*Disc);
		const FMovieSubtitleTable& Table = Index.MovieSubtitles();
		REQUIRE(Table.Entries().size() == Movies.size());
		for (const auto& [Movie, Lines] : Movies)
		{
			INFO("movie " << Movie);
			const FMovieSubtitleEntry* Entry = Table.Find(Movie);
			REQUIRE(Entry != nullptr);
			CHECK(Entry->Lines.size() == Lines);
			const std::optional<FMovieLocation> Location = FindMovie(Index, Movie);
			REQUIRE(Location);
			for (std::size_t Line = 1; Line < Entry->Lines.size(); ++Line)
			{
				CHECK(Entry->Lines[Line - 1].EndFrame <= Entry->Lines[Line].FirstFrame);
			}
			auto Subtitles = FMovieSubtitles::Load(Disc->Image(), Index, *Entry);
			REQUIRE(Subtitles);
			for (const FMovieSubtitleLine& Line : Entry->Lines)
			{
				const auto Picture = Subtitles->At(Line.FirstFrame);
				REQUIRE(Picture);
				bool bCovers = false;
				for (std::size_t Alpha = 3; Alpha < Picture->Pixels.size(); Alpha += 4)
				{
					bCovers = bCovers || Picture->Pixels[Alpha] == 255;
				}
				CHECK(bCovers);
			}
		}
	}
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		CHECK(IndexOf(*Disc).MovieSubtitles().Entries().empty());
	}
}

TEST_CASE("Movie subtitles show with the undub's sound on Auto, over either sound when on, and never when off", "[Game][Content][Voices]")
{
	REQUIRE_DISC(Usa1, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Undub1, EDiscRelease::UndubDisc1);
	constexpr int32 Opening = 13;
	constexpr int32 Logos = 16;
	auto Game = Testing::FGameLoopSession::Start(*Usa1);
	FGameSession& Session = *Game->Session;
	CHECK_FALSE(HasSubtitles(Session, Opening));
	const FGameFileIndex Japanese = IndexOf(*Undub1);
	Session.AddVoiceDisc(EVoiceLanguage::Japanese, 0, Undub1->Image(), Japanese);
	SetSubtitles(Session, EVoiceLanguage::Automatic, EMovieSubtitles::Automatic);
	CHECK_FALSE(HasSubtitles(Session, Opening));
	SetSubtitles(Session, EVoiceLanguage::Japanese, EMovieSubtitles::Automatic);
	CHECK(HasSubtitles(Session, Opening));
	CHECK_FALSE(HasSubtitles(Session, Logos));
	SetSubtitles(Session, EVoiceLanguage::English, EMovieSubtitles::On);
	CHECK(HasSubtitles(Session, Opening));
	CHECK_FALSE(HasSubtitles(Session, Logos));
	SetSubtitles(Session, EVoiceLanguage::Japanese, EMovieSubtitles::Off);
	CHECK_FALSE(HasSubtitles(Session, Opening));
	// On the second disc the first disc's undub has nothing to give.
	SetSubtitles(Session, EVoiceLanguage::Japanese, EMovieSubtitles::On);
	Session.Kernel().Disc = 1;
	CHECK_FALSE(HasSubtitles(Session, 4));

	auto Undub = Testing::FGameLoopSession::Start(*Undub1);
	CHECK(HasSubtitles(*Undub->Session, Opening));
	// With no USA disc, English falls back to the undub's own sound, which the subtitles go with.
	SetSubtitles(*Undub->Session, EVoiceLanguage::English, EMovieSubtitles::Automatic);
	CHECK(HasSubtitles(*Undub->Session, Opening));
}

TEST_CASE("The opening shows its first subtitle from frame 931 to 970, laid where the patch draws it", "[Media][Content][Voices]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UndubDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	auto Subtitles = Game->Session->MovieSubtitles(13);
	REQUIRE(Subtitles);
	REQUIRE(Subtitles->has_value());
	const std::optional<FMovieLocation> Location = FindMovie(*Game->Index, 13);
	REQUIRE(Location);
	FMoviePlayback Playback;
	// About ten sectors a frame: reading starts some frames before the line.
	Playback.StartSector = 9100;
	Playback.EndFrame = 1000;
	FMoviePlayer Player(Disc->Image(), *Location, Playback, *Game->Session->Sound().Sound(), std::move(*Subtitles));
	REQUIRE(Player.Start());
	REQUIRE(Player.FrameNumber() < 931);
	std::shared_ptr<const FMovieSubtitlePicture> First;
	while (Player.FrameNumber() < 975)
	{
		const uint32 Frame = Player.FrameNumber();
		INFO("frame " << Frame);
		if (Frame < 931 || Frame >= 971)
		{
			CHECK(Player.Subtitle() == nullptr);
		}
		else
		{
			REQUIRE(Player.Subtitle() != nullptr);
			if (!First)
			{
				First = Player.Subtitle();
				CHECK(Frame == 931);
			}
			CHECK(Player.Subtitle() == First);
		}
		const auto Playing = Player.Vblank({});
		REQUIRE(Playing);
		REQUIRE(*Playing);
	}
	REQUIRE(First);
	CHECK(First->X == 32);
	CHECK(First->Y == 189);
	CHECK(First->Width == 256);
	CHECK(First->Height == 14);
}

namespace
{
	/** The checkout's copy of Perfect Works Build 0.11.4, or none. */
	std::optional<FPerfectWorksRelease> PerfectWorks()
	{
		std::optional<FPerfectWorksRelease> Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
		return Release && Release->Version() == "0.11.4" ? std::move(Release) : std::nullopt;
	}

	void SetSource(FGameSession& Session, EMovieSubtitleSource Source)
	{
		FPlayerSettings Settings = Session.Settings();
		Settings.MovieSubtitleSource = Source;
		Session.SetSettings(Settings);
	}

	/** The opening's first subtitle: the undub's from frame 931, Perfect Works' from 934. */
	enum class EOpeningLines { None, Undub, PerfectWorks };
	EOpeningLines OpeningLines(const FGameSession& Session)
	{
		auto Subtitles = Session.MovieSubtitles(13);
		REQUIRE(Subtitles);
		if (!Subtitles->has_value())
		{
			return EOpeningLines::None;
		}
		return (*Subtitles)->At(932) != nullptr ? EOpeningLines::Undub : EOpeningLines::PerfectWorks;
	}
}

TEST_CASE("Perfect Works 0.11.4 subtitles the undub's movies with its own table, every line's picture in its file", "[Mods][Content][Voices]")
{
	const std::optional<FPerfectWorksRelease> Release = PerfectWorks();
	if (!Release)
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	const std::map<int32, std::map<int32, std::size_t>> Expected{
		{1, {{1, 1}, {2, 6}, {5, 2}, {6, 2}, {7, 5}, {8, 15}, {9, 10}, {13, 36}}},
		{2, {{0, 8}, {4, 37}, {8, 6}}},
	};
	for (const auto& [Disc, Movies] : Expected)
	{
		INFO("disc " << Disc);
		const FPerfectWorksSubtitles* Subtitles = Release->MovieSubtitles(Disc);
		REQUIRE(Subtitles != nullptr);
		REQUIRE(Subtitles->Table().Entries().size() == Movies.size());
		for (const auto& [Movie, Lines] : Movies)
		{
			INFO("movie " << Movie);
			const FMovieSubtitleEntry* Entry = Subtitles->Table().Find(Movie);
			REQUIRE(Entry != nullptr);
			CHECK(Entry->Lines.size() == Lines);
			auto Loaded = Subtitles->Load(*Entry);
			REQUIRE(Loaded);
			for (const FMovieSubtitleLine& Line : Entry->Lines)
			{
				const auto Picture = Loaded->At(Line.FirstFrame);
				REQUIRE(Picture);
				CHECK(std::any_of(Picture->Pixels.begin() + 3, Picture->Pixels.end(), [](uint8 Alpha) { return Alpha == 255; }));
			}
		}
	}
}

TEST_CASE("A release whose subtitles lack a file of pictures gives none for that disc", "[Mods][Content]")
{
	const std::optional<FPerfectWorksRelease> Release = PerfectWorks();
	if (!Release)
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	const std::filesystem::path From = std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks" / "gamefiles";
	const std::filesystem::path Copy = std::filesystem::temp_directory_path() / "WeltallPerfectWorksSubtitles";
	std::filesystem::remove_all(Copy);
	std::filesystem::create_directories(Copy / "fmv1");
	std::filesystem::copy(From / "sub_executable", Copy / "sub_executable", std::filesystem::copy_options::recursive);
	REQUIRE(FPerfectWorksSubtitles::Find(Copy, 1) == std::nullopt);
	for (int32 Record = 4150; Record <= 4156; ++Record)
	{
		const std::string Name = std::to_string(Record) + ".str";
		std::filesystem::copy_file(From / "fmv1" / Name, Copy / "fmv1" / Name);
	}
	CHECK_FALSE(FPerfectWorksSubtitles::Find(Copy, 1).has_value());
	std::filesystem::copy_file(From / "fmv1" / "4157.str", Copy / "fmv1" / "4157.str");
	CHECK(FPerfectWorksSubtitles::Find(Copy, 1).has_value());
	CHECK_FALSE(FPerfectWorksSubtitles::Find(Copy, 2).has_value());
	// macOS's companion of the subtitles' program, which a folder may list before it, is passed over.
	Testing::WriteAppleDoubleCompanion(Copy / "sub_executable" / "disc1" / "SLUS_006.64");
	CHECK(FPerfectWorksSubtitles::Find(Copy, 1).has_value());
	std::filesystem::remove_all(Copy);
}

TEST_CASE("Movie subtitles come from the source chosen, Perfect Works' first on Auto while the game uses it, either where only it is given", "[Game][Content][Voices]")
{
	REQUIRE_DISC(Usa1, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Undub1, EDiscRelease::UndubDisc1);
	const std::optional<FPerfectWorksRelease> Release = PerfectWorks();
	if (!Release)
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	auto Game = Testing::FGameLoopSession::Start(*Usa1);
	FGameSession& Session = *Game->Session;
	CHECK(Session.SoundSources() == FSoundSources{true, false, false, false});

	// Perfect Works alone: its subtitles over the English sound when on; Auto waits for a Japanese sound there is none of.
	Session.SetPerfectWorksSubtitles({Release->MovieSubtitles(1), Release->MovieSubtitles(2)}, false);
	CHECK(Session.SoundSources() == FSoundSources{true, false, true, false});
	SetSubtitles(Session, EVoiceLanguage::Automatic, EMovieSubtitles::Automatic);
	CHECK(OpeningLines(Session) == EOpeningLines::None);
	SetSubtitles(Session, EVoiceLanguage::Automatic, EMovieSubtitles::On);
	SetSource(Session, EMovieSubtitleSource::Undub);
	CHECK(OpeningLines(Session) == EOpeningLines::PerfectWorks);

	// Both: the source chosen; Auto takes the undub's unless the game uses the mod.
	const FGameFileIndex Japanese = IndexOf(*Undub1);
	Session.AddVoiceDisc(EVoiceLanguage::Japanese, 0, Undub1->Image(), Japanese);
	CHECK(Session.SoundSources() == FSoundSources{true, true, true, false});
	SetSubtitles(Session, EVoiceLanguage::Japanese, EMovieSubtitles::Automatic);
	CHECK(OpeningLines(Session) == EOpeningLines::Undub);
	SetSource(Session, EMovieSubtitleSource::PerfectWorks);
	CHECK(OpeningLines(Session) == EOpeningLines::PerfectWorks);
	SetSource(Session, EMovieSubtitleSource::Automatic);
	CHECK(OpeningLines(Session) == EOpeningLines::Undub);
	Session.SetPerfectWorksSubtitles({Release->MovieSubtitles(1), Release->MovieSubtitles(2)}, true);
	CHECK(OpeningLines(Session) == EOpeningLines::PerfectWorks);
	SetSubtitles(Session, EVoiceLanguage::English, EMovieSubtitles::Automatic);
	CHECK(OpeningLines(Session) == EOpeningLines::None);

	// The undub alone again, Perfect Works chosen: the undub's.
	Session.SetPerfectWorksSubtitles({}, false);
	SetSubtitles(Session, EVoiceLanguage::Japanese, EMovieSubtitles::Automatic);
	SetSource(Session, EMovieSubtitleSource::PerfectWorks);
	CHECK(OpeningLines(Session) == EOpeningLines::Undub);
}
