#include "Foundation/Sha1.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

namespace
{
	/** A font of one glyph (code 10h, one pixel at its top left), narrow for its first glyph. */
	FMesFont MakeFont()
	{
		std::vector<uint8> Font = {0x3A, 0x01, 0x0E, 0x00, 0xFE, 0x00, 0x16, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00};
		std::vector<uint8> Glyph(FMesFont::GlyphBytes, 0);
		Glyph[0] = 0x80;
		Font.insert(Font.end(), Glyph.begin(), Glyph.end());
		const std::vector<uint8> Box(FMesFont::GlyphBytes, 0xFF);
		auto Parsed = FMesFont::Parse(Font, Box);
		REQUIRE(Parsed);
		return *Parsed;
	}

	/** A font file of Source's one-byte glyphs from 10h to Last, each code's glyph Source's of Glyph(code). */
	template <typename TGlyphOf>
	FMesFont CopyFont(const FMesFont& Source, uint8 Last, TGlyphOf&& Glyph)
	{
		constexpr uint8 First = 0x10;
		const uint16 Count = static_cast<uint16>(Last - First + 1);
		const uint16 SecondPage = static_cast<uint16>(Count * FMesFont::GlyphBytes);
		std::vector<uint8> Font = {0, 0, 0x0E, 0x00, 0xFE, 0x00, static_cast<uint8>(SecondPage), static_cast<uint8>(SecondPage >> 8), 0x51, 0x00, 0x00,
			0x00, First, 0x00};
		for (int32 Code = First; Code <= Last; ++Code)
		{
			const std::optional<FMesFont::FGlyphRows> Rows = Source.Rows({0, Glyph(static_cast<uint8>(Code))});
			REQUIRE(Rows);
			for (const uint16 Row : *Rows)
			{
				Font.push_back(static_cast<uint8>(Row >> 8));
				Font.push_back(static_cast<uint8>(Row));
			}
		}
		const std::vector<uint8> Box(FMesFont::GlyphBytes, 0xFF);
		auto Parsed = FMesFont::Parse(Font, Box);
		REQUIRE(Parsed);
		return *Parsed;
	}
}

TEST_CASE("A glyph's cell holds its body a pixel in and an outline around it", "[Text]")
{
	const FMesFont Font = MakeFont();
	const FMesFont::FCell Cell = Font.Cell({0, 0x10});
	const auto At = [&Cell](int32 X, int32 Y) { return Cell[static_cast<std::size_t>(Y * FMesFont::CellWidth + X)]; };
	CHECK(At(1, 1) == 1);
	for (const auto [X, Y] : {std::pair{0, 0}, std::pair{1, 0}, std::pair{2, 0}, std::pair{0, 1}, std::pair{2, 1}, std::pair{0, 2}, std::pair{1, 2}, std::pair{2, 2}})
	{
		CHECK(At(X, Y) == 2);
	}
	CHECK(At(3, 1) == 0);
	CHECK(At(1, 3) == 0);
}

TEST_CASE("Glyph widths are narrow for the first page's first glyphs, wide otherwise", "[Text]")
{
	const FMesFont Font = MakeFont();
	CHECK(Font.WidthUnits({0, 0x10}) == 2);
	CHECK(Font.WidthUnits({0, 0x11}) == 3);
	CHECK(Font.WidthUnits({0xFE, 0x00}) == 3);
	CHECK(Font.WidthUnits({0xFF, 0xFF}) == 3);
	// As the kernel compares, a code before the first glyph counts as narrow.
	CHECK(Font.WidthUnits({0, 0x04}) == 2);
}

TEST_CASE("Every glyph of the game's font draws as the kernel draws it", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMesFont& Font = Resident.Font;
	// The SHA-1 of the kernel's own drawing of all 300 glyphs (one-byte codes, then FE 00-3C, then
	// the box), run in an emulated CPU: each cell's 2-bit values, row by row.
	FSha1 Digest;
	const auto Add = [&Digest, &Font](FMesCode Code)
	{
		const FMesFont::FCell Cell = Font.Cell(Code);
		Digest.Update(Cell);
	};
	for (int32 Code = 0x10; Code < 0xFE; ++Code)
	{
		Add({0, static_cast<uint8>(Code)});
	}
	for (int32 Code = 0; Code < 0x3D; ++Code)
	{
		Add({0xFE, static_cast<uint8>(Code)});
	}
	Add({0xFF, 0xFF});
	CHECK(FSha1::ToHex(Digest.Finish()) == "6607f649c90574f2bd0a3c5f67b5dbfc12a0f39b");
}

TEST_CASE("The font's narrow glyphs keep within their 8 pixels: a glyph typed over the one before leaves nothing of it", "[Text][Content]")
{
	// The kernel clears each glyph's 12-pixel cell in its plane before drawing it over the glyph before
	// (the menus' labels do so); clearing changes a pixel only where a narrow glyph, 8 pixels on, reaches
	// past its 8 pixels into the next one's cell: none does, so typing each glyph's cell over the image
	// as it stands (the field's windows) draws the same pixels.
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		const FMesResident Resident = Testing::LoadMesResident(*Disc);
		constexpr int32 Advance = 8;
		int32 Narrow = 0;
		for (int32 Lead = 0; Lead < 256; ++Lead)
		{
			for (int32 Code = 0; Code < 256; ++Code)
			{
				const FMesCode Glyph{static_cast<uint8>(Lead), static_cast<uint8>(Code)};
				if (Resident.Font.WidthUnits(Glyph) != 2)
				{
					continue;
				}
				const FMesFont::FCell Cell = Resident.Font.Cell(Glyph);
				Narrow += std::any_of(Cell.begin(), Cell.end(), [](uint8 Value) { return Value != 0; }) ? 1 : 0;
				bool bPast = false;
				for (int32 Y = 0; Y < FMesFont::CellHeight; ++Y)
				{
					for (int32 X = Advance; X < FMesFont::CellWidth; ++X)
					{
						bPast |= Cell[static_cast<std::size_t>(Y * FMesFont::CellWidth + X)] != 0;
					}
				}
				INFO("glyph " << Lead << " " << Code);
				CHECK_FALSE(bPast);
			}
		}
		CHECK(Narrow == 80);
	}
}

TEST_CASE("The face buttons' glyphs are buttons only where the font draws their pictures at their codes", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMesFont& Usa = Resident.Font;
	CHECK(Usa.FaceButton({0, 0x6E}) == EPadButton::Circle);
	CHECK(Usa.FaceButton({0, 0x6F}) == EPadButton::Square);
	CHECK(Usa.FaceButton({0, 0x70}) == EPadButton::Triangle);
	CHECK(Usa.FaceButton({0, 0x7F}) == EPadButton::Cross);
	CHECK_FALSE(Usa.FaceButton({0, 0x71}));
	CHECK_FALSE(Usa.FaceButton({0xFE, 0x6E}));

	// As the Spanish translation's font: a letter where the USA's Square is, the Circle's picture moved to another code.
	const uint8 Letter = MesCodeOfCharacter('o')->Code;
	const FMesFont Translated = CopyFont(Usa, 0x7F, [Letter](uint8 Code) { return Code == 0x6F ? Letter : Code == 0x71 ? uint8{0x6E} : Code; });
	CHECK(Translated.FaceButton({0, 0x6E}) == EPadButton::Circle);
	CHECK_FALSE(Translated.FaceButton({0, 0x6F}));
	CHECK(Translated.FaceButton({0, 0x70}) == EPadButton::Triangle);
	CHECK(Translated.FaceButton({0, 0x7F}) == EPadButton::Cross);
	CHECK_FALSE(Translated.FaceButton({0, 0x71}));

	// A font sheet drawn over the font later changes nothing: the disc's pictures decide.
	FMesFont Sheet = CopyFont(Usa, 0x7F, [](uint8 Code) { return Code; });
	REQUIRE(Sheet.SetRows({0, 0x6E}, *Usa.Rows({0, Letter})));
	CHECK(Sheet.FaceButton({0, 0x6E}) == EPadButton::Circle);
}

TEST_CASE("The font's star is found by its picture wherever the font draws it", "[Text][Content]")
{
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		CHECK(Testing::LoadMesResident(*Disc).Font.Star() == FMesCode{0, 0x6D});
	}
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMesFont& Usa = Resident.Font;

	// As the translations' fonts have it: a letter at 6Dh, the star moved further on.
	const uint8 Letter = MesCodeOfCharacter('I')->Code;
	const FMesFont Moved = CopyFont(Usa, 0x7F, [Letter](uint8 Code) { return Code == 0x6D ? Letter : Code == 0x7E ? uint8{0x6D} : Code; });
	CHECK(Moved.Star() == FMesCode{0, 0x7E});
	const FMesFont Without = CopyFont(Usa, 0x7F, [Letter](uint8 Code) { return Code == 0x6D ? Letter : Code; });
	CHECK_FALSE(Without.Star());
}

TEST_CASE("The French translation's font draws the star at D2h", "[Text][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(Testing::LoadMesResident(*Disc).Font.Star() == FMesCode{0, 0xD2});
}
