#include "Support/AppleDouble.h"
#include "Text/MesText.h"
#include "Text/MessageOverrides.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	FBytes Codes(std::string_view Text)
	{
		const auto Encoded = MesCodesOfText(Text, FMesCharacters::Font());
		REQUIRE(Encoded);
		return *Encoded;
	}

	/**
	 * A field map's table: a speaker's line sized to its text, a line with a name put in (its window 22 units wider than
	 * its text, for the name), a speech of four lines in a window of two (it scrolls), and a short word.
	 */
	FBytes Table()
	{
		const std::array<FAddedMessage, 4> Messages = {{
			{Codes("Fei\n\"Hi!\""), 10, 2},
			{Codes("{name 00} is here."), 40, 1},
			{Codes("One\nTwo\nThree\nFour"), 10, 2},
			{Codes("Bye"), 6, 1},
		}};
		const auto Built = BuildMessageTable(Messages);
		REQUIRE(Built);
		return *Built;
	}

	struct FOverrides
	{
		explicit FOverrides(std::string_view Contents)
		{
			const std::array<FMessageFile, 1> Files = {{{"mine.txt", std::string(Contents)}}};
			Overrides = FMessageOverrides::Parse(Files, FMesCharacters::Font(), [this](const std::string& Line) { Reports.push_back(Line); });
		}

		bool Reported(std::string_view Words) const
		{
			return std::any_of(Reports.begin(), Reports.end(), [Words](const std::string& Line) { return Line.find(Words) != std::string::npos; });
		}

		std::shared_ptr<const FMessageOverrides> Overrides;
		std::vector<std::string> Reports;
	};

	FMessageTable Parsed(const std::vector<uint8>& Bytes)
	{
		const auto Table = FMessageTable::Parse(Bytes);
		REQUIRE(Table);
		return *Table;
	}

	FBytes Message(const FMessageTable& Table, int32 Index)
	{
		const std::span<const uint8> String = Table.String(Index);
		const std::span<const uint8> Codes = String.first(MesMessageSize(String));
		return FBytes(Codes.begin(), Codes.end());
	}
}

TEST_CASE("A message of the player's replaces the one of its id, its window sized for it", "[Text]")
{
	FOverrides Mine("message field/4/0\n{\nFei\n\"Hello there!\"\n}\nmessage field/4/1\n{\n{name 00} est ici, enfin.\n}\n");
	const FBytes Disc = Table();
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Disc, EMessageWindows::Table);
	REQUIRE(Applied);
	CHECK(Mine.Reports.empty());
	const FMessageTable Changed = Parsed(Applied->Table);
	REQUIRE(Changed.Count() == 4);
	CHECK(Message(Changed, 0) == Codes("Fei\n\"Hello there!\""));
	CHECK(Changed.WidthUnits(0) == 28);
	CHECK(Changed.Lines(0) == 2);
	// The room the disc's left for the name is kept beside the new text.
	CHECK(Changed.WidthUnits(1) == 2 * 16 + 22);
	CHECK(Changed.Lines(1) == 1);
	// The others are the disc's, windows and all.
	const FMessageTable Original = Parsed(Disc);
	for (const int32 Kept : {2, 3})
	{
		CHECK(Message(Changed, Kept) == Message(Original, Kept));
		CHECK(Changed.WidthUnits(Kept) == Original.WidthUnits(Kept));
		CHECK(Changed.Lines(Kept) == Original.Lines(Kept));
	}
	CHECK(Applied->Replaced.Table == "field/4");
	CHECK(Applied->Replaced.Messages == std::vector<int32>{0, 1});
	CHECK_FALSE(FMessageOverrides::Apply(Mine.Overrides, "field/5", Disc, EMessageWindows::Table));
}

TEST_CASE("The player's messages are read from the text files below the folder, but the originals and hidden ones", "[Text]")
{
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallTextOverridesFolder";
	std::filesystem::remove_all(Folder);
	const auto Write = [&Folder](const std::filesystem::path& File, std::string_view Text)
	{
		std::filesystem::create_directories((Folder / File).parent_path());
		std::ofstream(Folder / File, std::ios::binary) << Text;
	};
	Write("field/004.txt", "message field/4/0\n{\nFei\n\"Hello there!\"\n}\n");
	Testing::WriteAppleDoubleCompanion(Folder / "field" / "004.txt");
	Write("original/field/004.txt", "message field/4/3\n{\nSo long\n}\n");
	Write(".backup/old.txt", "message field/4/2\n{\nOld\n}\n");
	std::vector<std::string> Reports;
	const auto Read = FMessageOverrides::Read(Folder, FMesCharacters::Font(), [&Reports](const std::string& Line) { Reports.push_back(Line); });
	std::filesystem::remove_all(Folder);
	REQUIRE(Read);
	CHECK(Reports.empty());
	const FBytes Disc = Table();
	const auto Applied = FMessageOverrides::Apply(*Read, "field/4", Disc, EMessageWindows::Table);
	REQUIRE(Applied);
	CHECK(Applied->Replaced.Messages == std::vector<int32>{0});
}

TEST_CASE("A message is measured with the widths a font sheet gives its glyphs", "[Text]")
{
	FMesCharacters Accented = FMesCharacters::Font();
	Accented.Set({0, 0x93}, "\xC3\xA9");
	const std::array<FMessageFile, 1> Files = {{{"mine.txt", "message field/4/3\n{\nR\xC3\xA9" "el\n}\n"}}};
	const auto Wide = FMessageOverrides::Parse(Files, Accented, nullptr);
	const auto Narrow = FMessageOverrides::Parse(Files, Accented, nullptr, FMesGlyphWidths{{0x93, 2}});
	const auto AsWide = FMessageOverrides::Apply(Wide, "field/4", Table(), EMessageWindows::Table);
	const auto AsNarrow = FMessageOverrides::Apply(Narrow, "field/4", Table(), EMessageWindows::Table);
	REQUIRE(AsWide);
	REQUIRE(AsNarrow);
	CHECK(Parsed(AsWide->Table).WidthUnits(3) == 9);
	CHECK(Parsed(AsNarrow->Table).WidthUnits(3) == 8);
}

TEST_CASE("A window the disc made smaller than its text keeps its size", "[Text]")
{
	FOverrides Mine("message field/4/2\n{\nUn\nDeux\nTrois\nQuatre\nCinq\n}\n");
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Table(), EMessageWindows::Table);
	REQUIRE(Applied);
	const FMessageTable Changed = Parsed(Applied->Table);
	CHECK(Changed.Lines(2) == 2);
	CHECK(Changed.WidthUnits(2) == 12);
}

TEST_CASE("A field message too large for any window the field draws keeps the disc's", "[Text]")
{
	FOverrides Mine("message field/4/3\n{\nThis line is far too long for the widest window.\n}\nmessage field/4/0\n{\nOne\nTwo\nThree\nFour\nFive\nSix\nSeven\nEight\n"
		"Nine\nTen\nEleven\nTwelve\nThirteen\nFourteen\n}\n");
	const FBytes Disc = Table();
	CHECK_FALSE(FMessageOverrides::Apply(Mine.Overrides, "field/4", Disc, EMessageWindows::Table));
	CHECK(Mine.Reported("field/4/3 (mine.txt line 1): needs a window of 96 x 1"));
	CHECK(Mine.Reported("field/4/0 (mine.txt line 5): needs a window of 16 x 14"));
	// Another table's program draws the text where it will: it is used, and its size logged.
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Disc, EMessageWindows::Program);
	REQUIRE(Applied);
	CHECK(Parsed(Applied->Table).WidthUnits(3) == 6);
	CHECK(Mine.Reported("field/4/3 (mine.txt line 1): is 96 x 1, larger than any of its table on the disc (18 x 4)"));
}

TEST_CASE("A message whose marks differ from the disc's is used and logged", "[Text]")
{
	FOverrides Mine("message field/4/1\n{\nHere at last.{page}\nTruly.{close 3C}\n}\n");
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Table(), EMessageWindows::Script);
	REQUIRE(Applied);
	CHECK(Message(Parsed(Applied->Table), 1) == Codes("Here at last.{page}\nTruly.{close 3C}"));
	// A script sizes these windows: the table's sizes stay the disc's.
	CHECK(Parsed(Applied->Table).WidthUnits(1) == 40);
	REQUIRE(Mine.Reports.size() == 1);
	CHECK(Mine.Reported("lacks the disc's {name 00}"));
	CHECK(Mine.Reported("it has 2 pages where the disc's has 1"));
	CHECK(Mine.Reported("it ends by itself"));
}

TEST_CASE("Messages the font cannot write or no table holds are logged and left out", "[Text]")
{
	FOverrides Mine(
		"message field/4/0\n{\nCaf\xC3\xA9\n}\n"
		"message field/4/1\n{\n{nom 00}\n}\n"
		"message feild/4/2\n{\nTypo\n}\n"
		"message field/4/9\n{\nPast the end\n}\n"
		"message field/4/3\n{\nFirst\n}\n"
		"message field/4/3\n{\nSecond\n}\n");
	CHECK(Mine.Reported("mine.txt line 1: field/4/0 has \"\xC3\xA9\", which no glyph of the font is"));
	CHECK(Mine.Reported("mine.txt line 5: field/4/1 has the mark {nom 00}, which is none of the marks"));
	CHECK(Mine.Reported("mine.txt line 9: feild/4/2 is no message's id"));
	CHECK(Mine.Reported("mine.txt line 21: field/4/3 was given before, at mine.txt line 17"));
	CHECK(Mine.Overrides->Count() == 2);
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Table(), EMessageWindows::Table);
	REQUIRE(Applied);
	CHECK(Message(Parsed(Applied->Table), 3) == Codes("Second"));
	CHECK(Mine.Reported("field/4/9 (mine.txt line 13): names no message: the table has 4"));
}

TEST_CASE("A replaced message is logged once where its script's window is smaller than its text", "[Text]")
{
	FOverrides Mine("message field/4/0\n{\nFei\n\"Hello there!\"\n}\n");
	const auto Applied = FMessageOverrides::Apply(Mine.Overrides, "field/4", Table(), EMessageWindows::Table);
	REQUIRE(Applied);
	const FMessageTable Changed = Parsed(Applied->Table);
	Applied->Replaced.CheckWindow(0, Changed.String(0), 30, 2);
	CHECK(Mine.Reports.empty());
	Applied->Replaced.CheckWindow(0, Changed.String(0), 20, 2);
	Applied->Replaced.CheckWindow(0, Changed.String(0), 20, 2);
	REQUIRE(Mine.Reports.size() == 1);
	CHECK(Mine.Reported("field/4/0 (mine.txt line 1): is 28 x 2, larger than the 20 x 2 window it opens in"));
	// A window with a face beside the text, wider than the field's screen holds.
	Applied->Replaced.CheckScreenWidth(0, 28 + 17, 70);
	CHECK(Mine.Reports.size() == 1);
	Applied->Replaced.CheckScreenWidth(0, 75, 70);
	REQUIRE(Mine.Reports.size() == 2);
	CHECK(Mine.Reported("field/4/0 (mine.txt line 1): opens a window 75 wide with what it shows beside its text (a face), where the screen holds 70"));
	Applied->Replaced.CheckScreenWidth(3, 75, 70);
	// The disc's own text did not fit a window of one line either: that one scrolls as the disc's does.
	Applied->Replaced.CheckWindow(0, Changed.String(0), 30, 1);
	Applied->Replaced.CheckWindow(3, Changed.String(3), 2, 1);
	CHECK(Mine.Reports.size() == 2);
}
