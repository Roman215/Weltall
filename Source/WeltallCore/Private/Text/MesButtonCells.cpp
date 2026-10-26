#include "Text/MesButtonCells.h"

#include "Text/MesFont.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 ButtonGlyphPixels = 12;
		constexpr int32 UnitPixels = 4;
		/** How far a name is read ahead on the line being typed. */
		constexpr std::size_t Lookahead = 40;
	}

	void FMesButtonCells::Add(const FMesWindow& Text, int32 Row, int32 First, int32 Pixels, const FMesFont* Font, uint32 Primitive,
		std::vector<FTextButtonCell>& Out)
	{
		const FMesWindow::FLine& Line = Text.Row(Row);
		Ahead.assign(Line.Glyphs.begin(), Line.Glyphs.end());
		// A name's word may stand on the next line: the row below is read on, and on the line being typed the string.
		// The glyphs read on get places past the row's: only their order matters to a name.
		int16 Next = static_cast<int16>(Line.WidthUnits + 1);
		const int32 Cursor = Text.CursorRow();
		if (Row < Cursor && Row + 1 < Text.Lines())
		{
			Ahead.push_back({MesLineEnd, Line.WidthUnits});
			for (const FMesPlacedGlyph& Glyph : Text.Row(Row + 1).Glyphs)
			{
				Ahead.push_back({Glyph.Code, Next++});
			}
		}
		if ((Row == Cursor || Row + 1 == Cursor) && Font != nullptr)
		{
			Text.PendingGlyphs(Font->LeadByte(), Lookahead, Pending, Row == Cursor);
			for (const FMesCode& Code : Pending)
			{
				Ahead.push_back({Code, Next++});
			}
		}
		Find(Line.Glyphs, First, Pixels, Font, Primitive, Out);
	}

	void FMesButtonCells::AddTyped(std::span<const FMesPlacedGlyph> Glyphs, int32 Pixels, const FMesFont* Font, uint32 Primitive,
		std::vector<FTextButtonCell>& Out)
	{
		Ahead.assign(Glyphs.begin(), Glyphs.end());
		Find(Glyphs, 0, Pixels, Font, Primitive, Out);
	}

	void FMesButtonCells::Find(std::span<const FMesPlacedGlyph> Glyphs, int32 First, int32 Pixels, const FMesFont* Font, uint32 Primitive,
		std::vector<FTextButtonCell>& Out)
	{
		const int32 Unit = Font != nullptr ? Font->UnitPixels() : UnitPixels;
		for (const FMesPlacedGlyph& Glyph : Glyphs)
		{
			const int32 At = Glyph.XUnits * Unit - First;
			if (Font == nullptr || At < 0 || At + ButtonGlyphPixels > Pixels)
			{
				continue;
			}
			if (const std::optional<EPadButton> Button = Font->FaceButton(Glyph.Code))
			{
				Out.push_back({Primitive, static_cast<int16>(At), ButtonGlyphPixels, *Button});
			}
		}
		FindMesButtonWords(Ahead, Words);
		for (const FMesButtonWord& Word : Words)
		{
			if (Word.FirstGlyph >= Glyphs.size())
			{
				continue;
			}
			const int32 At = Glyphs[Word.FirstGlyph].XUnits * Unit - First;
			// A name ends where the glyph after it starts; one whose end is still to be typed covers what of it is, its
			// picture showing from its first letter.
			const bool bWhole = Word.FirstGlyph + Word.GlyphCount < Glyphs.size();
			const int32 Width = bWhole ? (Word.EndUnits - Glyphs[Word.FirstGlyph].XUnits) * Unit : Pixels - At;
			if (At >= 0 && Width > 0 && At + Width <= Pixels)
			{
				Out.push_back({Primitive, static_cast<int16>(At), static_cast<int16>(Width), Word.Button, Word.bDirections});
			}
		}
	}
}
