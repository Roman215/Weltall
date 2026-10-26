#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The renderer types a whole line in one step: up to 100 glyphs. */
		constexpr uint8 LineGlyphs = 100;
		constexpr int32 UnitPixels = 4;
		constexpr int32 GlyphUnits = 3;

		/** A variable-width font's glyph, as its program expands it: 12 rows of 16 pixels, 4 bits a pixel, as two words a row. */
		constexpr int32 ShadowCellRows = FMesFont::GlyphRows + 1;
		using FShadowHalf = std::array<uint32, ShadowCellRows>;

		/**
		 * The glyph's body (1) and its drop shadow (2) one pixel right, one down and one right and down, in the plane's
		 * bits of each pixel's nibble (pixel 0 in the low nibble): the left 8 pixels and the right 8.
		 */
		std::array<FShadowHalf, 2> ShadowCell(const FMesFont::FGlyphRows& Rows, int32 Plane)
		{
			const uint32 Body = Plane == 0 ? 0x1u : 0x4u;
			const uint32 Shadow = Plane == 0 ? 0x2u : 0x8u;
			std::array<FShadowHalf, 2> Cell{};
			for (int32 Row = 0; Row < FMesFont::GlyphRows; ++Row)
			{
				const uint32 Bits = Rows[static_cast<std::size_t>(Row)];
				for (std::size_t Half = 0; Half < Cell.size(); ++Half)
				{
					const uint32 Byte = (Half == 0 ? Bits >> 8 : Bits) & 0xFF;
					uint32 Bodies = 0;
					uint32 Shadows = 0;
					for (int32 Pixel = 0; Pixel < 8; ++Pixel)
					{
						if ((Byte << Pixel & 0x80) != 0)
						{
							Bodies |= Body << 4 * Pixel;
							Shadows |= Shadow << 4 * Pixel;
						}
					}
					// The left half's last pixel throws its shadow on the right half's first.
					const uint32 Spill = Half == 1 && (Bits >> 8 & 1) != 0 ? Shadow : 0;
					const uint32 Right = Shadows << 4;
					FShadowHalf& Words = Cell[Half];
					Words[static_cast<std::size_t>(Row)] |= Bodies | Right | Spill;
					Words[static_cast<std::size_t>(Row) + 1] = Right | Shadows | Spill;
				}
			}
			return Cell;
		}

		/** The image's 32-bit word of two units at At (linear: a row's last word runs on into the next row, as memory does). */
		uint32 WordAt(std::span<const uint16> Units, std::size_t At)
		{
			return At + 1 < Units.size() ? static_cast<uint32>(Units[At] | Units[At + 1] << 16) : 0;
		}

		void SetWord(std::span<uint16> Units, std::size_t At, uint32 Value)
		{
			if (At + 1 < Units.size())
			{
				Units[At] = static_cast<uint16>(Value);
				Units[At + 1] = static_cast<uint16>(Value >> 16);
			}
		}

		/**
		 * Half a cell into the image's word at Word (its unit in row 0), Offset pixels into it, rows 1 to 12: the plane is
		 * cleared first only where the cell starts the word; what passes the word goes into the next, its plane cleared
		 * first. Every word written has its row 0 zeroed.
		 */
		void BlitShadowHalf(std::span<uint16> Units, int32 Pitch, std::size_t Word, int32 Offset, int32 Width, const FShadowHalf& Half, int32 Plane)
		{
			const uint32 Kept = Plane == 0 ? 0xCCCCCCCCu : 0x33333333u;
			SetWord(Units, Word, 0);
			for (std::size_t Row = 0; Row < Half.size(); ++Row)
			{
				const std::size_t At = Word + (Row + 1) * static_cast<std::size_t>(Pitch);
				const uint32 Before = Offset == 0 ? WordAt(Units, At) & Kept : WordAt(Units, At);
				SetWord(Units, At, Before | Half[Row] << 4 * Offset);
			}
			if (Offset + Width <= 8)
			{
				return;
			}
			SetWord(Units, Word + 2, 0);
			for (std::size_t Row = 0; Row < Half.size(); ++Row)
			{
				const std::size_t At = Word + 2 + (Row + 1) * static_cast<std::size_t>(Pitch);
				SetWord(Units, At, (WordAt(Units, At) & Kept) | Half[Row] >> (32 - 4 * Offset));
			}
		}

		/**
		 * A variable-width font's glyph at pixel X: its left 8 pixels from X's 8-pixel group, the right 8 a word on for a
		 * glyph 9 pixels wide or more. Its program takes the first part as wide as the glyph, or as 8 when its step has 9
		 * glyphs or more left (a register the routine reads unset); only pixels past the glyph's advance differ.
		 */
		void DrawVariableWidthGlyph(const FMesFont& Font, FMesCode Code, std::span<uint16> Units, int32 Pitch, int32 X, int32 Plane, uint8 StepBudget)
		{
			// Its program draws a two-byte code's last byte as a glyph of the first page, a code below the first from
			// the bytes before the glyphs.
			const FMesFont::FGlyphRows Rows = Font.VariableWidthRows(Code.Code);
			const std::array<FShadowHalf, 2> Cell = ShadowCell(Rows, Plane);
			const int32 Width = Font.VariableDrawWidth(Code.Code);
			const std::size_t Word = static_cast<std::size_t>(X >> 3) * 2;
			BlitShadowHalf(Units, Pitch, Word, X & 7, StepBudget >= 9 ? 8 : std::min(Width, 8), Cell[0], Plane);
			if (Width >= 9)
			{
				BlitShadowHalf(Units, Pitch, Word + 2, X & 7, std::min(Width - 8, 8), Cell[1], Plane);
			}
		}
	}

	void DrawMesGlyph(const FMesFont* Font, FMesCode Code, std::span<uint16> Units, int32 Pitch, int32 X, int32 Plane, uint8 StepBudget)
	{
		if (Font != nullptr && Font->IsVariableWidth())
		{
			DrawVariableWidthGlyph(*Font, Code, Units, Pitch, X, Plane, StepBudget);
			return;
		}
		const uint16 Kept = Plane == 0 ? 0xCCCC : 0x3333;
		const FMesFont::FCell Cell = Font != nullptr ? Font->Cell(Code) : FMesFont::FCell{};
		for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
		{
			for (int32 Unit = 0; Unit < GlyphUnits; ++Unit)
			{
				Units[static_cast<std::size_t>(Row * Pitch + X + Unit)] &= Kept;
			}
			for (int32 Pixel = 0; Pixel < FMesFont::CellWidth; ++Pixel)
			{
				const uint32 Value = Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Pixel)];
				uint16& Unit = Units[static_cast<std::size_t>(Row * Pitch + X + Pixel / UnitPixels)];
				Unit = static_cast<uint16>(Unit | Value << (Pixel % UnitPixels * 4 + Plane * 2));
			}
		}
	}

	int32 RenderMesString(FMesContext& Context, FMesWindow& Typing, std::span<const uint8> String, FVramImage& Image, int32 Plane)
	{
		const FMesWindow::FLine& Line = Typing.TypeLine(Context, String, LineGlyphs);
		for (const FMesPlacedGlyph& Glyph : Line.Glyphs)
		{
			DrawMesGlyph(Context.Font, Glyph.Code, Image.Units, Image.Rect.Width, Glyph.XUnits, Plane, Glyph.StepBudget);
		}
		return Line.WidthUnits * (Context.Font != nullptr ? Context.Font->UnitPixels() : UnitPixels);
	}
}
