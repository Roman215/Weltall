#pragma once

#include "Foundation/Types.h"
#include "Input/ButtonGlyphs.h"
#include "Text/MesButtonWords.h"
#include "Text/MesWindow.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FMesFont;

	/**
	 * Finds the buttons a window's lines show: the font's glyphs of the PlayStation's face buttons (FMesFont::FaceButton),
	 * which the English text writes for the physical buttons, and the buttons a line names in words (FindMesButtonWords).
	 * Kept from line to line, it allocates only as lines grow.
	 */
	class FMesButtonCells
	{
	public:
		/**
		 * Adds to Out the cells of the sprite Primitive, which shows Pixels of Text's row Row from pixel First of the line
		 * on: each of Font's button glyphs it shows whole and each button the line names in words, whose word may stand on
		 * the next row. On the line being typed a name counts from its first letter, the rest of it and its word read ahead
		 * in the string by Font's lead byte (not without Font).
		 */
		WELTALLCORE_API void Add(const FMesWindow& Text, int32 Row, int32 First, int32 Pixels, const FMesFont* Font, uint32 Primitive,
			std::vector<FTextButtonCell>& Out);

		/** The same for a line typed whole at a stroke (FMesWindow::TypeLine's glyphs), which a sprite Pixels wide shows from its start. */
		WELTALLCORE_API void AddTyped(std::span<const FMesPlacedGlyph> Glyphs, int32 Pixels, const FMesFont* Font, uint32 Primitive,
			std::vector<FTextButtonCell>& Out);

		/**
		 * The same for a line kept whole (a logged message's), which a sprite Pixels wide shows from pixel First of the line
		 * on; a name's word read on into Next, the line below it (empty for none).
		 */
		WELTALLCORE_API void AddKept(std::span<const FMesPlacedGlyph> Glyphs, std::span<const FMesPlacedGlyph> Next, int32 First, int32 Pixels,
			const FMesFont* Font, uint32 Primitive, std::vector<FTextButtonCell>& Out);

	private:
		/** The cells of Glyphs, the names read in Ahead (the glyphs and whatever follows them). */
		void Find(std::span<const FMesPlacedGlyph> Glyphs, int32 First, int32 Pixels, const FMesFont* Font, uint32 Primitive,
			std::vector<FTextButtonCell>& Out);

		std::vector<FMesButtonWord> Words;
		std::vector<FMesPlacedGlyph> Ahead;
		std::vector<FMesCode> Pending;
	};
}
