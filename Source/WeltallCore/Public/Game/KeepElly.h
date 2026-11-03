#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FGameState;

	/**
	 * Keep Elly: a New Game+ (NewGamePlus.h) in which Elly comes back after Golgoda. Her leaving and Golgoda play as the
	 * story has them; from Merkava's core on, the first place after them where the player has the party, she is a member
	 * again: joined, in the menus and in her Omnigear Regurus, for the player to bring into the party. Every scene plays as
	 * the game has it, and the party each scene builds is the scene's own. Its saves from there are the original's with
	 * her away (ClearFile::HideElly), her return kept beside them.
	 */
	namespace KeepElly
	{
		/** The story's progress in Merkava's core, where she is back. */
		inline constexpr uint16 ReturnStory = 298;

		/** Whether State is a Keep Elly game the story has brought to her return. */
		WELTALLCORE_API bool IsBack(const FGameState& State);

		/**
		 * Elly a member again, whatever the scripts last wrote of her, in a Keep Elly game past her return while Fei has
		 * joined: where the story takes the party from Fei (before Bethlehem, whose maps have no place for either), she is
		 * away with him until he is back. The party itself is left as it is.
		 */
		WELTALLCORE_API void Hold(FGameState& State);
	}
}
