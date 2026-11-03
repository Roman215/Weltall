#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <span>

namespace Weltall
{
	class FMenuSpriteSheet;
	class FMesFont;
	struct FMenuMainTables;

	/**
	 * The port's own frames of the menus' sheet, past the disc's: the options screen's orb and label on the
	 * title's arc and the Normal Menu's, the title's arc raised a place over an Exit orb at its foot, and Load over
	 * the File screen's Save, pieced from the sheet's own orbs and lettering as the game pieces its labels, so they
	 * look the game's.
	 */
	namespace MenuPortFrame
	{
		/** "Options" where the title's Sound label stands. */
		constexpr int32 TitleOptions = 0x200;
		/** The Normal Menu's eighth orb, a step on from Status up the arc, and its label. */
		constexpr int32 OptionsOrb = 0x201;
		constexpr int32 Options = 0x202;
		/** The red orb, where the arcs find a selected one: thirteen frames on. */
		constexpr int32 SelectedOptionsOrb = OptionsOrb + 0xD;
		/** The title's Sound, Continue and New Game a place further up the arc: their orbs (the red ones thirteen frames on) and labels. */
		constexpr int32 RaisedTitleOrbs = 0x203;
		constexpr int32 RaisedTitleLabels = 0x206;
		/** "Options" where the raised Sound label stands. */
		constexpr int32 RaisedTitleOptions = 0x209;
		/** The title's Load a place further up its inner arc, over the Normal Menu's Save (the red orb thirteen frames on). */
		constexpr int32 TopLoadOrb = 0x20A;
		constexpr int32 TopLoad = 0x20B;
	}

	/** The port's frames added to the sheet as the disc has it, its arcs' places from Tables. */
	void AddMenuPortFrames(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables);

	/**
	 * Whether the letters the port pieces "Options" from are the USA release's, in the images the Menu's archive uploads
	 * (Archive, later ones over earlier ones). A translation redraws them, and the pieces then spell nothing.
	 */
	bool HasUsaOptionsLetters(const FMenuSpriteSheet& Sheet, std::span<const FVramImage> Archive);

	/**
	 * The port's "Options" frames shown as a label of the translation's own that names an options screen, where the sheet
	 * has one Weltall knows (the Spanish translation's "Opcion", the Italian's "Opzioni"), in its lettering where "Options"
	 * stands; false, changing nothing, where it has none.
	 */
	bool UseTranslatedOptionsLabel(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables, std::span<const FVramImage> Archive);

	/**
	 * The port's "Options" frames shown as the word typed in Font, in the colours of the labels' lettering (its CLUT in
	 * Archive), a picture of the sheet's own, for a sheet whose letters are not the USA release's.
	 */
	void TypeMenuPortWords(FMenuSpriteSheet& Sheet, const FMenuMainTables& Tables, const FMesFont& Font, std::span<const FVramImage> Archive);
}
