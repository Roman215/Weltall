#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Text/MesWindow.h"

#include <span>

namespace Weltall
{
	/**
	 * The kernel's string renderer, with which the menus and the battle make their text
	 * images: String typed on one line at once by Typing (a one-line window as wide as the text may run), each
	 * glyph's three units cleared in its 2-bit plane of Image on every row and then its cell ORed in, so the
	 * other plane and whatever lies past the new text stay. Returns the line's width in pixels.
	 */
	WELTALLCORE_API int32 RenderMesString(FMesContext& Context, FMesWindow& Typing, std::span<const uint8> String, FVramImage& Image, int32 Plane);

	/**
	 * The kernel's glyph drawing: a glyph's three units cleared in Plane on each of the cell's
	 * rows of an image Pitch units a row, X units from its left, and the cell ORed in. A variable-width font's glyph is
	 * drawn as the French translation's program draws it, X pixels from the left, with a drop shadow; StepBudget is the
	 * glyphs its step had left (FMesPlacedGlyph), which that program's drawing reads.
	 */
	WELTALLCORE_API void DrawMesGlyph(const FMesFont* Font, FMesCode Code, std::span<uint16> Units, int32 Pitch, int32 X, int32 Plane, uint8 StepBudget = 0);

	/**
	 * What a window's latest step typed, drawn into the image its strips share (Units, Pitch units a row), and each
	 * strip typed on handed to Upload in turn, the strip of the line the text is on last and whether or not a glyph
	 * was typed on it: the kernel uploads that strip after every step. The image is never cleared, so a line types
	 * over what the lines before it left in its plane, and an upload made before the next line is drawn is what a
	 * tick of its own would have made. A tick of the game's own typing makes the one upload; the player's instant
	 * text types a page's lines in one tick.
	 */
	template <typename TUpload>
	void DrawMesTypedLines(const FMesFont* Font, const FMesWindow& Window, std::span<uint16> Units, int32 Pitch, TUpload&& Upload)
	{
		const int32 Current = Window.CurrentStrip();
		const std::span<const FMesPlacedGlyph> Glyphs = Window.TypedGlyphs();
		for (const FMesTypedLine& Line : Window.TypedLines())
		{
			for (const FMesPlacedGlyph& Glyph : Glyphs.subspan(static_cast<std::size_t>(Line.First), static_cast<std::size_t>(Line.Count)))
			{
				DrawMesGlyph(Font, Glyph.Code, Units, Pitch, Glyph.XUnits, Line.Strip & 1, Glyph.StepBudget);
			}
			if (Line.Strip != Current)
			{
				Upload(static_cast<int32>(Line.Strip));
			}
		}
		Upload(Current);
	}
}
