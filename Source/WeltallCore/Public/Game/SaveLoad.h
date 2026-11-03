#pragma once

#include "Foundation/Result.h"
#include "Game/GameState.h"

namespace Weltall
{
	class FGearPartTables;
	class FMesNameTables;
	class FOriginalSave;

	/** Fixes of the Menu's load: each is on unless the original's behavior is asked for. */
	struct FSaveLoadOptions
	{
		/** A gear's overweight penalty follows the save's weight tolerance; the original's follows the state's before the load. */
		bool bPenaltyFromSavedTolerance = true;
		/**
		 * A pilot keeps the gear Deathblows the parts of the gear in use grant; in the original, those
		 * of the last of the pilot's gears in id order, until the Gear screen shows the pilot.
		 */
		bool bDeathblowsFromGearInUse = true;

		/** The Menu's load as the original does it. */
		static constexpr FSaveLoadOptions Original() { return {false, false}; }
	};

	/**
	 * The game state after the Menu loads a save. State is the state the game holds when the save is
	 * loaded (after start-up, the new game the kernel loads at boot). The save's regions are copied
	 * over it, every gear's derived fields recomputed from its parts, the names turned back into the
	 * message encoding, and the play time, the battle scripts' globals and the second disc's story mode
	 * (the save's, none chosen in one the game made) restored. Without bExpansionPlayed (the port cannot play the
	 * save's Disc 2 expansion) a save of the expanded second disc is the original second disc's game at its point.
	 *
	 * A save keeps only part of each gear's record, and the rest is not all derived: those bytes stay
	 * as they were in State. A save whose gear names a record the tables lack, or whose name uses a
	 * glyph the glyph map lacks, is refused: the game cannot write one.
	 */
	WELTALLCORE_API TResult<FGameState> LoadOriginalSave(const FOriginalSave& Save, FGameState State, const FGearPartTables& Tables,
		const FMesNameTables& NameTables, const FSaveLoadOptions& Options = {}, bool bExpansionPlayed = true);
}
