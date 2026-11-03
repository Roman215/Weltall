#pragma once

#include "Foundation/Types.h"
#include "Game/WayBackPoint.h"

namespace Weltall
{
	class FGameState;
	struct FEventContext;

	/**
	 * The clear file: after the ending, a game that goes on aboard the Yggdrasil before Deus, the whole world open and the
	 * final dungeon ahead, with the levels, items and gears the game ended with, Elly back in the party in her Omnigear
	 * Regurus and Id unlocked. Its saves are the original's at that point, as the game itself leaves Elly (away), so a
	 * PlayStation loads them as ordinary saves; the port keeps her return beside them (SaveWrite.h).
	 */
	namespace ClearFile
	{
		/** Where it starts: the Yggdrasil's bridge, entered as from the world map's way into the ship, in the last chapter's free roaming. */
		inline constexpr FWayBackPoint Start{290, 3, 340};

		/**
		 * A game at the ending made the clear file's: put back to its point before Deus (the place, the story, what the
		 * final dungeon's scripts and battles wrote, its map taken back), boarded (the gears restored, the ship landed), the
		 * party Fei, Elly and the first member free, marked a clear game.
		 */
		WELTALLCORE_API void Make(FGameState& State);

		/**
		 * What the original sees of a clear game: Elly away, as the story leaves her (not joined, in Vierge), the first
		 * member free in her place in the party, or with nobody free the members after her closing up.
		 */
		WELTALLCORE_API void HideElly(FGameState& State);

		/**
		 * Whether the field waits for good on the ending's last picture, where the game is over: the ending map's first
		 * actor parked in a wait that jumps back to itself, as its script ends once the still has faded in.
		 */
		WELTALLCORE_API bool IsEndingWaiting(const FEventContext& Context);
	}
}
