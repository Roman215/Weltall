#include "Field/FieldMap.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesWindow.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

using namespace Weltall;

// The French translation (Atelier Traduction's v1.1, with the English voices and with the Japanese): its discs, made by
// its patch from the USA discs, are read from the folder WELTALL_PATCHED_GAMES names.

namespace
{
	struct FFrenchDisc
	{
		EDiscRelease Release;
		uint8 DiscNumber;
		std::string_view Title;
		std::size_t SubtitledMovies;
		std::size_t SubtitleLines;
	};

	constexpr std::array<FFrenchDisc, 4> FrenchDiscs{{
		{EDiscRelease::UsaDisc1, 1, "Xenogears (French translation) Disc 1", 8, 88},
		{EDiscRelease::UsaDisc2, 2, "Xenogears (French translation) Disc 2", 3, 57},
		{EDiscRelease::UndubDisc1, 1, "Xenogears (French translation, Japanese voices) Disc 1", 8, 92},
		{EDiscRelease::UndubDisc2, 2, "Xenogears (French translation, Japanese voices) Disc 2", 3, 54},
	}};
}

TEST_CASE("The French translation's discs are their releases, patched, with the undub's index and their own movie subtitles", "[Media][Content]")
{
	for (const FFrenchDisc& French : FrenchDiscs)
	{
		INFO(French.Title);
		REQUIRE_PATCHED_DISC(Disc, French.Release);
		const TResult<FDiscIdentity> Identity = IdentifyDisc(*Disc);
		REQUIRE(Identity);
		CHECK(Identity->Release == French.Release);
		CHECK(Identity->DiscNumber == French.DiscNumber);
		CHECK(Identity->bPatched);
		CHECK(std::string_view(Identity->Title) == French.Title);

		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		// The undub's code in the index's unused slots is no file.
		for (std::size_t Record = 280; Record <= 376; ++Record)
		{
			CHECK_FALSE(Index->Records()[Record].IsFile());
		}
		CHECK_FALSE(Index->Records()[381].IsFile());
		const std::vector<FMovieSubtitleEntry>& Movies = Index->MovieSubtitles().Entries();
		CHECK(Movies.size() == French.SubtitledMovies);
		std::size_t Lines = 0;
		for (const FMovieSubtitleEntry& Movie : Movies)
		{
			Lines += Movie.Lines.size();
			CHECK(FMovieSubtitles::Load(Disc->Image(), *Index, Movie));
		}
		CHECK(Lines == French.SubtitleLines);
	}
}

TEST_CASE("The French translation's font advances by its program's pixels", "[Text][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UndubDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMesFont& Font = Resident.Font;
	REQUIRE(Font.IsVariableWidth());
	// The space, i, l, m and W.
	CHECK(Font.WidthUnits({0, 0x10}) == 3);
	CHECK(Font.WidthUnits({0, 0x45}) == 3);
	CHECK(Font.WidthUnits({0, 0x48}) == 2);
	CHECK(Font.WidthUnits({0, 0x49}) == 8);
	CHECK(Font.WidthUnits({0, 0x36}) == 10);
	// é and Œ.
	CHECK(Font.WidthUnits({0, 0x63}) == 6);
	CHECK(Font.WidthUnits({0, 0x72}) == 12);
	// Its letters are drawn at 61h-72h, where the USA's font draws its wide digits and buttons: no button is read there.
	CHECK_FALSE(Font.FaceButton({0, 0x6E}));
	CHECK_FALSE(Font.FaceButton({0, 0x6F}));

	REQUIRE_DISC(Usa, EDiscRelease::UsaDisc1);
	CHECK_FALSE(Testing::LoadMesResident(*Usa).Font.IsVariableWidth());
}

TEST_CASE("A French message is typed in pixels: the spider's web found in the forest", "[Text][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Archive = LoadFieldArchive(Disc->Image(), *Index, 14);
	REQUIRE(Archive);
	const auto Table = FMessageTable::Parse(Archive->Section(EFieldSection::Messages));
	REQUIRE(Table);
	// "Tu as trouvé une toile d'araignée." in a window of 45 units: 34 glyphs, 165 pixels.
	const std::span<const uint8> Message = Table->String(3);
	FMesContext Context;
	Context.Font = &Resident.Font;
	Context.Tables = &Resident.NameTables;
	FMesWindow Window(Table->WidthUnits(3), Table->Lines(3));
	Window.SetSpeed(0xFF);
	Window.Queue(Message);
	Window.Update(Context);
	Window.Update(Context);
	const FMesWindow::FLine& Line = Window.Row(0);
	REQUIRE(Line.Glyphs.size() == 34);
	int32 Pixels = 0;
	for (const FMesPlacedGlyph& Glyph : Line.Glyphs)
	{
		CHECK(Glyph.XUnits == Pixels);
		Pixels += Resident.Font.WidthUnits(Glyph.Code);
	}
	CHECK(Line.WidthUnits == Pixels);
	CHECK(Pixels == 165);
}

TEST_CASE("A translation's disc with the Japanese voices plays before the USA's own disc", "[Media][Content]")
{
	REQUIRE_PATCHED_DISC(French, EDiscRelease::UndubDisc1);
	REQUIRE_DISC(Usa, EDiscRelease::UsaDisc1);
	const FGameLibrary Patched = FGameLibrary::Scan(*Testing::PatchedGamesFolder());
	const std::filesystem::path FrenchBin = std::filesystem::absolute(std::filesystem::path(Patched.FileOf(EDiscRelease::UndubDisc1)).replace_extension(".bin"));
	const std::filesystem::path UsaBin = std::filesystem::absolute(std::filesystem::path(WELTALL_GAMES_DIR) / "Xenogears (USA) (Disc 1).bin");
	REQUIRE(std::filesystem::exists(FrenchBin));
	REQUIRE(std::filesystem::exists(UsaBin));

	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallFrenchBesideUsa";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder, Error);
	for (const auto& [Name, Bin] : {std::pair{"a-usa.cue", UsaBin}, std::pair{"b-french.cue", FrenchBin}})
	{
		std::ofstream(Folder / Name) << "FILE \"" << Bin.string() << "\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
	}
	{
		const FGameLibrary Library = FGameLibrary::Scan(Folder);
		REQUIRE(Library.Find(EDiscRelease::UsaDisc1) != nullptr);
		REQUIRE(Library.Find(EDiscRelease::UndubDisc1) != nullptr);
		CHECK(Library.PlayableDisc(1) == Library.Find(EDiscRelease::UndubDisc1));
	}
	std::filesystem::remove_all(Folder, Error);
}
