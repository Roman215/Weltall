#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Input/PadState.h"

#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Weltall
{
	/** A glyph as text stores it: one byte (lead 0), or a lead byte (FEh, FFh) and a second byte. */
	struct FMesCode
	{
		uint8 Lead = 0;
		uint8 Code = 0;

		bool operator==(const FMesCode&) const = default;
	};

	/**
	 * The game's message font: glyphs of 11 rows of 12 pixels, and the box glyph the kernel carries
	 * for codes the font lacks (FF FF). The kernel draws a glyph into a 12 x 13 cell of 2-bit values:
	 * 1 for its body, moved one pixel right and down, 2 for an outline around the body (3 where a
	 * body pixel also borders another), clipped to the cell.
	 */
	class FMesFont
	{
	public:
		static constexpr int32 CellWidth = 12;
		static constexpr int32 CellHeight = 13;
		static constexpr std::size_t GlyphBytes = 22;
		static constexpr int32 GlyphColumns = 12;
		static constexpr int32 GlyphRows = 11;
		using FCell = std::array<uint8, CellWidth * CellHeight>;
		/** A glyph's pixels: a row each, its leftmost pixel in bit 15 (bits 3 to 0 are never drawn). */
		using FGlyphRows = std::array<uint16, GlyphRows>;

		/** The font as its file unpacks (directory 1, file 6), and the executable's box glyph. */
		WELTALLCORE_API static TResult<FMesFont> Parse(std::span<const uint8> Font, std::span<const uint8> BoxGlyph);

		/** The lead byte from which codes take a second byte. */
		uint8 LeadByte() const { return SecondPageLead; }

		/** The code of the font's first glyph (its header's). */
		uint32 FirstGlyphCode() const { return FirstCode; }

		/**
		 * How far the text advances after a glyph, in the font's units (UnitPixels): 2 for the narrow ones, 3 for the rest;
		 * a variable-width font's pixels.
		 */
		WELTALLCORE_API int32 WidthUnits(FMesCode Code) const;

		/** Code's glyph advances Units from now on, whatever the game's rule gives it (a font sheet's width). */
		WELTALLCORE_API void SetWidthUnits(FMesCode Code, int32 Units);

		/**
		 * The French translation's font, which its program types at any pixel: each code's advance in pixels, as its table
		 * gives it to the code's last byte (a two-byte code's lead is not looked at). Its glyphs are then drawn with a drop
		 * shadow (DrawMesGlyph), and a window's cursor and its lines' widths count pixels.
		 */
		WELTALLCORE_API void SetVariableWidths(const std::array<uint8, 256>& Pixels);
		/**
		 * The same, with the widths its glyph drawing takes, which differ below the first code: it looks them up 64 KB
		 * past the table (the code's distance from the first taken as 16 bits), its typing just before it.
		 */
		WELTALLCORE_API void SetVariableWidths(const std::array<uint8, 256>& Pixels, const std::array<uint8, 256>& DrawnPixels);

		/** The width a variable-width glyph is drawn with (its first page's code). */
		int32 VariableDrawWidth(uint8 Code) const { return VariableDrawWidths[Code]; }

		bool IsVariableWidth() const { return bVariableWidth; }

		/** The pixels in a unit of WidthUnits, of a window's cursor and of its lines' widths: 4, or 1 for a variable-width font. */
		int32 UnitPixels() const { return bVariableWidth ? 1 : 4; }

		/** Where the kernel puts a window's "more" arrow after its text, in pixels from its x: the cursor X × 4, or X + 4 for a variable-width font. */
		int32 CursorPixels(int32 X) const { return bVariableWidth ? X + 4 : X * 4; }

		/** The glyph's cell; empty for a code the font does not hold. */
		WELTALLCORE_API FCell Cell(FMesCode Code) const;

		/** The glyph's pixels; none for a code the font does not hold. */
		WELTALLCORE_API std::optional<FGlyphRows> Rows(FMesCode Code) const;
		/**
		 * A first-page code's rows as the variable-width program reads them, 22 bytes a code counted back from the first
		 * glyph: below it the font file's header, then nothing (zeros; on a console what lies before the font).
		 */
		WELTALLCORE_API FGlyphRows VariableWidthRows(uint8 Code) const;

		/** Draws Rows for Code's glyph from now on; false (changing nothing) for a code the font does not hold. */
		WELTALLCORE_API bool SetRows(FMesCode Code, const FGlyphRows& Rows);

		/**
		 * The face button Code's glyph shows. The English text writes the four as glyphs of their own, which count only
		 * where the font as the disc holds it draws their pictures: a translation may put letters there (the Spanish
		 * one's ò ó ö) and its own symbols elsewhere.
		 */
		WELTALLCORE_API std::optional<EPadButton> FaceButton(FMesCode Code) const;

	private:
		/** Where Code's glyph is in Glyphs, or the box; none for a code the font does not hold. */
		std::optional<std::span<const uint8>> GlyphOf(FMesCode Code) const;

		std::vector<uint8> Glyphs;
		/** The file's bytes before its glyphs. */
		std::vector<uint8> Header;
		std::array<uint8, GlyphBytes> Box{};
		uint8 SecondPageLead = 0;
		uint32 SecondPageOffset = 0;
		uint32 NarrowCount = 0;
		uint32 SecondPageNarrowCount = 0;
		uint32 FirstCode = 0;
		/** The face buttons whose pictures the disc's font draws at their codes, a bit each. */
		uint8 DrawnFaceButtons = 0;
		/** The glyphs a font sheet gives widths of their own, by their code's lead << 8 | code. */
		std::unordered_map<uint16, int32> OwnWidths;
		bool bVariableWidth = false;
		std::array<uint8, 256> VariableWidths{};
		std::array<uint8, 256> VariableDrawWidths{};
	};
}
