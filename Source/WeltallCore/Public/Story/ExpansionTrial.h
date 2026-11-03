#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"

#include <string>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FStoryPacks;

	/** What a Disc 2 expansion's documents came to, tried on the player's second disc (TryExpansion). */
	struct FExpansionTrial
	{
		/**
		 * The documents written for another disc than the player's, by name ("Folder/disc2/map-617.txt"), in the order
		 * of their maps then their battle events: as its map or event loads, each is left out and the disc's own plays,
		 * but a map of its own, which then cannot be played.
		 */
		std::vector<std::string> ForAnotherDisc;
		/** What stops a map of the expansion as it loads, each error naming its map and document. */
		std::vector<FError> Faults;
	};

	/**
	 * Tries the expanded second disc's documents of Story on the disc a game of the second disc reads its files from
	 * (Image and Index: FGameSession::Image, the first disc where the player has it, which holds every map), as the game
	 * puts them in each field map and battle event they are written for with the player's settings Options, so that the
	 * player can be told before playing which are for another disc. The disc is read as the game reads it but for the
	 * player's Assets, as Export Disc 2 reads it. A map the disc does not hold is another disc's: the second alone holds
	 * only its own part of the story's. An error where the disc's battle events cannot be read.
	 */
	WELTALLCORE_API TResult<FExpansionTrial> TryExpansion(const FStoryPacks& Story, const FCdImage& Image, const FGameFileIndex& Index, const FEventOptions& Options);
}
