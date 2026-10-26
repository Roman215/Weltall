#include "Foundation/LittleEndian.h"
#include "Support/OracleDigest.h"
#include "Support/XorShift32.h"
#include "Text/MesFont.h"
#include "Text/MesRender.h"
#include "Text/MesWindow.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FOracleDigest;
using Weltall::Testing::FXorShift32;

// The French translation's text: a font whose program types it at any pixel. Its glyph drawing is held to the digest the
// oracle gives for the same cases drawn by the translation's own routine.
namespace
{
	constexpr uint8 CaseCode = 0x20;
	constexpr int32 Cases = 256;
	constexpr int32 ImageRows = 14;
	constexpr const char* OriginalDigest = "5fa63a83d5230c48e2919dbaac5351befa95b0da";

	/** A font file of the game's layout with glyphs from code 10h to Last, all blank, and the box glyph's bytes. */
	std::vector<uint8> BlankFontFile(uint8 Last)
	{
		std::vector<uint8> File(14 + (Last - 0x10 + 1) * FMesFont::GlyphBytes, 0);
		LittleEndian::WriteU16(File, 2, 14);
		LittleEndian::WriteU16(File, 4, 0xFE);
		LittleEndian::WriteU16(File, 6, 5236);
		LittleEndian::WriteU16(File, 8, 0x51);
		LittleEndian::WriteU16(File, 12, 0x10);
		return File;
	}

	FMesFont VariableWidthFont(std::span<const uint8> Glyph, uint8 Width)
	{
		std::vector<uint8> File = BlankFontFile(CaseCode);
		std::copy(Glyph.begin(), Glyph.end(), File.begin() + 14 + (CaseCode - 0x10) * FMesFont::GlyphBytes);
		const std::array<uint8, FMesFont::GlyphBytes> Box{};
		FMesFont Font = *FMesFont::Parse(File, Box);
		std::array<uint8, 256> Widths{};
		Widths[CaseCode] = Width;
		Font.SetVariableWidths(Widths);
		return Font;
	}

	/** A font whose every glyph from 10h to 7Fh advances by its code's low four bits plus one. */
	FMesFont StepFont()
	{
		const std::array<uint8, FMesFont::GlyphBytes> Box{};
		FMesFont Font = *FMesFont::Parse(BlankFontFile(0x7F), Box);
		std::array<uint8, 256> Widths{};
		for (std::size_t Code = 0; Code < Widths.size(); ++Code)
		{
			Widths[Code] = static_cast<uint8>((Code & 0xF) + 1);
		}
		Font.SetVariableWidths(Widths);
		return Font;
	}
}

TEST_CASE("A variable-width glyph is drawn as the French translation's routine draws it", "[Text][Oracle]")
{
	FSha1 Total;
	for (uint32 Seed = 1; Seed <= Cases; ++Seed)
	{
		FXorShift32 G(Seed);
		std::array<uint8, FMesFont::GlyphBytes> Glyph{};
		for (std::size_t Row = 0; Row < FMesFont::GlyphRows; ++Row)
		{
			const uint32 Bits = G.Next() & 0xFFFF;
			Glyph[Row * 2] = static_cast<uint8>(Bits >> 8);
			Glyph[Row * 2 + 1] = static_cast<uint8>(Bits);
		}
		const uint8 Width = static_cast<uint8>(1 + G.Below(12));
		const int32 Pitch = 36 + 2 * static_cast<int32>(G.Below(28));
		const int32 X = static_cast<int32>(G.Below(static_cast<uint32>((Pitch - 3) * 4 + 1)));
		const int32 Plane = static_cast<int32>(G.Below(2));
		std::vector<uint16> Units(static_cast<std::size_t>(Pitch * ImageRows));
		for (uint16& Unit : Units)
		{
			Unit = static_cast<uint16>(G.Next());
		}
		const FMesFont Font = VariableWidthFont(Glyph, Width);
		DrawMesGlyph(&Font, {0, CaseCode}, Units, Pitch, X, Plane);
		FOracleDigest Image;
		for (const uint16 Unit : Units)
		{
			Image.I16(Unit);
		}
		const FSha1::FDigest Digest = Image.Finish();
		Total.Update(Digest);
	}
	CHECK(FSha1::ToHex(Total.Finish()) == OriginalDigest);
}

TEST_CASE("A variable-width font advances by its table's pixels and counts its units as pixels", "[Text]")
{
	const FMesFont Font = StepFont();
	CHECK(Font.IsVariableWidth());
	CHECK(Font.UnitPixels() == 1);
	CHECK(Font.WidthUnits({0, 0x23}) == 4);
	// The lead byte of a two-byte code is not looked at.
	CHECK(Font.WidthUnits({0xFE, 0x23}) == 4);
	CHECK(Font.CursorPixels(10) == 14);

	const std::array<uint8, FMesFont::GlyphBytes> Box{};
	const FMesFont Game = *FMesFont::Parse(BlankFontFile(0x7F), Box);
	CHECK_FALSE(Game.IsVariableWidth());
	CHECK(Game.UnitPixels() == 4);
	CHECK(Game.CursorPixels(10) == 40);
}

TEST_CASE("A window types a variable-width font in pixels, breaking lines only where the text does", "[Text]")
{
	const FMesFont Font = StepFont();
	FMesContext Context;
	Context.Font = &Font;

	SECTION("The cursor counts pixels; a line typed past the width goes on")
	{
		// A 3-unit window (12 pixels): 21h, 2Fh, 25h advance 2, 16 and 6 pixels.
		FMesWindow Window(3, 2);
		Window.SetSpeed(8);
		const std::array<uint8, 4> Text{0x21, 0x2F, 0x25, 0x00};
		Window.Queue(Text);
		Window.Update(Context);
		Window.Update(Context);
		const FMesWindow::FLine& Line = Window.Row(0);
		REQUIRE(Line.Glyphs.size() == 3);
		CHECK(Line.Glyphs[0].XUnits == 0);
		CHECK(Line.Glyphs[1].XUnits == 2);
		CHECK(Line.Glyphs[2].XUnits == 18);
		CHECK(Line.WidthUnits == 24);
		CHECK(Window.CurrentLine() == 0);
	}

	SECTION("A line's end moves the cursor past any window's pixels; the next step starts the next line")
	{
		FMesWindow Window(127, 2);
		Window.SetSpeed(8);
		const std::array<uint8, 4> Text{0x21, 0x01, 0x22, 0x00};
		Window.Queue(Text);
		Window.Update(Context);
		CHECK(Window.State().X == 0x400);
		Window.Update(Context);
		Window.Update(Context);
		CHECK(Window.CurrentLine() == 1);
		REQUIRE(Window.Row(1).Glyphs.size() == 1);
		CHECK(Window.Row(1).Glyphs[0].XUnits == 0);
	}

	SECTION("A new line starts once the cursor is past the window's width in pixels")
	{
		// A 3-unit window: 12 pixels. 2Bh advances 12: at 12 the line goes on, and the glyph after passes it.
		FMesWindow Window(3, 2);
		Window.SetSpeed(1);
		const std::array<uint8, 5> Text{0x2B, 0x21, 0x22, 0x23, 0x00};
		Window.Queue(Text);
		Window.Update(Context);
		Window.Update(Context);
		CHECK(Window.CurrentLine() == 0);
		CHECK(Window.State().X == 14);
		Window.Update(Context);
		CHECK(Window.Row(0).WidthUnits == 14);
		CHECK(Window.CurrentLine() == 1);
		CHECK(Window.State().X == 3);
	}
}

TEST_CASE("The string renderer gives a variable-width line's width in pixels", "[Text]")
{
	const FMesFont Font = StepFont();
	FMesContext Context;
	Context.Font = &Font;
	FMesWindow Typing(57, 1);
	FVramImage Image{{0, 0, 60, 14}, std::vector<uint16>(60 * 14, 0)};
	const std::array<uint8, 4> Text{0x21, 0x2F, 0x25, 0x00};
	CHECK(RenderMesString(Context, Typing, Text, Image, 0) == 24);
}
