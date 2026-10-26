#include "Text/MesFont.h"

#include "Foundation/Crc32.h"
#include "Foundation/LittleEndian.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t HeaderBytes = 14;
		constexpr int32 GlyphRows = FMesFont::GlyphRows;
		constexpr int32 GlyphColumns = FMesFont::GlyphColumns;
		constexpr uint8 BoxLead = 0xFF;
		constexpr uint8 BoxCode = 0xFF;

		struct FFaceButtonGlyph
		{
			uint8 Code = 0;
			/** The CRC-32 of the glyph's bytes in the USA release's font. */
			uint32 PictureCrc = 0;
			EPadButton Button = EPadButton::Cross;
		};
		constexpr std::array<FFaceButtonGlyph, 4> FaceButtonGlyphs{{{0x6E, 0x7B1ABF8Fu, EPadButton::Circle},
			{0x6F, 0x0DDA394Bu, EPadButton::Square}, {0x70, 0x27FB8567u, EPadButton::Triangle}, {0x7F, 0xC7E4980Bu, EPadButton::Cross}}};

		/** A glyph pixel: rows are big-endian with the leftmost pixel in bit 15. */
		bool IsSet(std::span<const uint8> Glyph, int32 X, int32 Y)
		{
			if (X < 0 || X >= GlyphColumns || Y < 0 || Y >= GlyphRows)
			{
				return false;
			}
			const uint16 Row = static_cast<uint16>(Glyph[static_cast<std::size_t>(Y) * 2] << 8 | Glyph[static_cast<std::size_t>(Y) * 2 + 1]);
			return (Row >> (15 - X) & 1) != 0;
		}
	}

	TResult<FMesFont> FMesFont::Parse(std::span<const uint8> Font, std::span<const uint8> BoxGlyph)
	{
		if (Font.size() < HeaderBytes || BoxGlyph.size() < GlyphBytes)
		{
			return MakeError("the message font is too short");
		}
		const uint16 GlyphsOffset = LittleEndian::ReadU16(Font, 2);
		if (GlyphsOffset > Font.size())
		{
			return MakeError("the message font's glyphs lie outside it");
		}
		FMesFont Result;
		Result.SecondPageLead = static_cast<uint8>(LittleEndian::ReadU16(Font, 4));
		Result.SecondPageOffset = LittleEndian::ReadU16(Font, 6);
		Result.NarrowCount = LittleEndian::ReadU16(Font, 8);
		Result.SecondPageNarrowCount = LittleEndian::ReadU16(Font, 10);
		Result.FirstCode = LittleEndian::ReadU16(Font, 12);
		Result.Glyphs.assign(Font.begin() + GlyphsOffset, Font.end());
		Result.Header.assign(Font.begin(), Font.begin() + GlyphsOffset);
		std::copy_n(BoxGlyph.begin(), GlyphBytes, Result.Box.begin());
		for (std::size_t Index = 0; Index < FaceButtonGlyphs.size(); ++Index)
		{
			const std::optional<std::span<const uint8>> Glyph = Result.GlyphOf({0, FaceButtonGlyphs[Index].Code});
			if (Glyph && Crc32(*Glyph) == FaceButtonGlyphs[Index].PictureCrc)
			{
				Result.DrawnFaceButtons = static_cast<uint8>(Result.DrawnFaceButtons | 1u << Index);
			}
		}
		return Result;
	}

	std::optional<EPadButton> FMesFont::FaceButton(FMesCode Code) const
	{
		for (std::size_t Index = 0; Index < FaceButtonGlyphs.size(); ++Index)
		{
			if (Code.Lead == 0 && Code.Code == FaceButtonGlyphs[Index].Code && (DrawnFaceButtons >> Index & 1) != 0)
			{
				return FaceButtonGlyphs[Index].Button;
			}
		}
		return std::nullopt;
	}

	void FMesFont::SetWidthUnits(FMesCode Code, int32 Units)
	{
		OwnWidths[static_cast<uint16>(Code.Lead << 8 | Code.Code)] = Units;
	}

	void FMesFont::SetVariableWidths(const std::array<uint8, 256>& Pixels)
	{
		SetVariableWidths(Pixels, Pixels);
	}

	void FMesFont::SetVariableWidths(const std::array<uint8, 256>& Pixels, const std::array<uint8, 256>& DrawnPixels)
	{
		VariableWidths = Pixels;
		VariableDrawWidths = DrawnPixels;
		bVariableWidth = true;
	}

	int32 FMesFont::WidthUnits(FMesCode Code) const
	{
		if (!OwnWidths.empty())
		{
			if (const auto Own = OwnWidths.find(static_cast<uint16>(Code.Lead << 8 | Code.Code)); Own != OwnWidths.end())
			{
				return Own->second;
			}
		}
		if (bVariableWidth)
		{
			return VariableWidths[Code.Code];
		}
		// As the kernel compares them: a one-byte code before the first glyph counts as narrow too.
		bool bNarrow = false;
		if (Code.Lead == 0)
		{
			bNarrow = static_cast<int32>(Code.Code) - static_cast<int32>(FirstCode) < static_cast<int32>(NarrowCount);
		}
		else if (Code.Lead == SecondPageLead)
		{
			bNarrow = Code.Code < SecondPageNarrowCount;
		}
		return bNarrow ? 2 : 3;
	}

	std::optional<std::span<const uint8>> FMesFont::GlyphOf(FMesCode Code) const
	{
		if (Code.Lead == BoxLead && Code.Code == BoxCode)
		{
			return std::span<const uint8>(Box);
		}
		// Codes before the first glyph, or past the font, have no glyph (the kernel would draw
		// whatever memory lies there; no text uses them).
		if (Code.Lead == 0 ? Code.Code < FirstCode : Code.Lead < SecondPageLead)
		{
			return std::nullopt;
		}
		const std::size_t Offset = Code.Lead == 0
			? (Code.Code - FirstCode) * GlyphBytes
			: SecondPageOffset + static_cast<std::size_t>(Code.Lead - SecondPageLead) * 0x1600 + Code.Code * GlyphBytes;
		if (Offset + GlyphBytes > Glyphs.size())
		{
			return std::nullopt;
		}
		return std::span<const uint8>(Glyphs).subspan(Offset, GlyphBytes);
	}

	std::optional<FMesFont::FGlyphRows> FMesFont::Rows(FMesCode Code) const
	{
		const std::optional<std::span<const uint8>> Glyph = GlyphOf(Code);
		if (!Glyph)
		{
			return std::nullopt;
		}
		FGlyphRows Rows{};
		for (std::size_t Row = 0; Row < Rows.size(); ++Row)
		{
			Rows[Row] = static_cast<uint16>((*Glyph)[Row * 2] << 8 | (*Glyph)[Row * 2 + 1]);
		}
		return Rows;
	}

	FMesFont::FGlyphRows FMesFont::VariableWidthRows(uint8 Code) const
	{
		if (Code >= FirstCode)
		{
			return Rows({0, Code}).value_or(FGlyphRows{});
		}
		// Counted from the file's start: the glyph's first byte lies this far before the glyphs.
		const std::ptrdiff_t Start = static_cast<std::ptrdiff_t>(Header.size()) - static_cast<std::ptrdiff_t>((FirstCode - Code) * GlyphBytes);
		const auto Byte = [&](std::ptrdiff_t At) -> uint32 {
			At += Start;
			if (At < 0)
			{
				return 0;
			}
			if (At < static_cast<std::ptrdiff_t>(Header.size()))
			{
				return Header[static_cast<std::size_t>(At)];
			}
			At -= static_cast<std::ptrdiff_t>(Header.size());
			return At < static_cast<std::ptrdiff_t>(Glyphs.size()) ? Glyphs[static_cast<std::size_t>(At)] : 0;
		};
		FGlyphRows Result{};
		for (std::size_t Row = 0; Row < Result.size(); ++Row)
		{
			const std::ptrdiff_t At = static_cast<std::ptrdiff_t>(Row * 2);
			Result[Row] = static_cast<uint16>(Byte(At) << 8 | Byte(At + 1));
		}
		return Result;
	}

	bool FMesFont::SetRows(FMesCode Code, const FGlyphRows& Rows)
	{
		const std::optional<std::span<const uint8>> Glyph = GlyphOf(Code);
		if (!Glyph)
		{
			return false;
		}
		const bool bBox = Glyph->data() == Box.data();
		uint8* const Bytes = bBox ? Box.data() : Glyphs.data() + (Glyph->data() - Glyphs.data());
		for (std::size_t Row = 0; Row < Rows.size(); ++Row)
		{
			Bytes[Row * 2] = static_cast<uint8>(Rows[Row] >> 8);
			Bytes[Row * 2 + 1] = static_cast<uint8>(Rows[Row] & 0xF0);
		}
		return true;
	}

	FMesFont::FCell FMesFont::Cell(FMesCode Code) const
	{
		const std::optional<std::span<const uint8>> Found = GlyphOf(Code);
		if (!Found)
		{
			return {};
		}
		const std::span<const uint8> Glyph = *Found;
		FCell Result{};
		for (int32 Y = 0; Y < CellHeight; ++Y)
		{
			for (int32 X = 0; X < CellWidth; ++X)
			{
				const int32 GlyphX = X - 1;
				const int32 GlyphY = Y - 1;
				bool bOutline = false;
				for (int32 Dy = -1; Dy <= 1 && !bOutline; ++Dy)
				{
					for (int32 Dx = -1; Dx <= 1; ++Dx)
					{
						if ((Dx != 0 || Dy != 0) && IsSet(Glyph, GlyphX + Dx, GlyphY + Dy))
						{
							bOutline = true;
							break;
						}
					}
				}
				Result[static_cast<std::size_t>(Y * CellWidth + X)] = static_cast<uint8>((IsSet(Glyph, GlyphX, GlyphY) ? 1 : 0) | (bOutline ? 2 : 0));
			}
		}
		return Result;
	}
}
