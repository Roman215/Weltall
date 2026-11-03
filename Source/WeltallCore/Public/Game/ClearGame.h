#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * What a game is after the ending (ClearFile.h, NewGamePlus.h, KeepElly.h). Every kind unlocks Fei's and Elly's looks
	 * (FCharacterLooks) and marks its saves with a star; the clear file's and Keep Elly's also bring Elly back, which
	 * their saves keep beside the original's view of her away. The values are the saves' (SaveLayout's Port::ClearGame).
	 */
	enum class EClearGame : uint8
	{
		None = 0,
		/** The clear file's game, aboard the Yggdrasil before Deus with Elly back in the party, and every save made in it. */
		ClearFile = 1,
		/** A new game started from a save of the clear file's game, the story from its beginning. */
		NewGamePlus = 3,
		/** A New Game+ in which Elly is a member again from Merkava on. */
		KeepElly = 4,
	};

	/** Whether a game of the kind keeps its own Elly beside the original's view of her away in its saves. */
	constexpr bool KeepsEllyApart(EClearGame Kind)
	{
		return Kind == EClearGame::ClearFile || Kind == EClearGame::KeepElly;
	}
}
