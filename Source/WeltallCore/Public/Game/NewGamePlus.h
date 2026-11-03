#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	class FGameState;

	/** What a New Game+ takes from the clear file; each part left out is as the original's new game has it. */
	struct FNewGamePlusCarry
	{
		bool bMoney = false;
		bool bEquipment = false;
		bool bItems = false;
		bool bDeathblows = false;
		bool bLevels = false;
		/** The story's flag for the 7-AP Deathblows and the gears' Infinity (the story flags' bit 14, set in Shevat). */
		bool bLimiter = false;
		/** Elly a member again from Merkava on, the game a Keep Elly game (KeepElly.h). */
		bool bKeepElly = false;

		bool operator==(const FNewGamePlusCarry&) const = default;
	};

	/**
	 * A New Game+: the story from its beginning after the clear file (ClearFile.h), a clear game from its start, so every
	 * look can be switched whenever its character is in the party, with what the player chose carried over from the clear
	 * file. Nothing carried is anything the field's scripts test to move the story on.
	 */
	namespace NewGamePlus
	{
		/**
		 * The new game made the New Game+ of the clear file's state ClearFile, marked so (EClearGame::NewGamePlus, or
		 * EClearGame::KeepElly with Keep Elly chosen).
		 * - Money: the clear file's.
		 * - Equipment: every weapon, armor and accessory, gear weapon and gear part the clear file holds or has on, added to
		 *   the new game's inventory; the new game's characters keep what they have on. Gear frames, engines and armor are
		 *   no items (the gear shop tunes them up in place) and stay behind.
		 * - Items: the clear file's added to the new game's, but the story's key items (the item table's use flag 10h).
		 * - Deathblows: each character's learned Deathblows, its gear Deathblows, and its counters toward the rest.
		 * - Levels: each character's levels and experience, HP and EP (full), stats, abilities and gear abilities, the
		 *   Infinity attacks of levels 50, 60 and 70, Maria's gear Deathblows (hers come by level), and the AP per turn the
		 *   level gives (7 only with the 7-AP flag).
		 * - Limiter: the 7-AP flag.
		 * ItemTable: the game data's items, 10h bytes an item.
		 */
		WELTALLCORE_API void Carry(const FGameState& ClearFile, FGameState& NewGame, const FNewGamePlusCarry& Carry, std::span<const uint8> ItemTable);
	}
}
