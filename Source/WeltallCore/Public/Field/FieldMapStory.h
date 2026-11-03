#pragma once

#include "Field/EventContext.h"
#include "Field/EventScripts.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FStoryPacks;
	struct FFieldMap;
	struct FStoryDocument;

	/** The story a field map loads with: the port's documents (none: the disc's maps as they are), the player's settings, and whether the game plays the second disc expanded. */
	struct FFieldMapStory
	{
		const FStoryPacks* Packs = nullptr;
		FEventOptions Options;
		bool bExpandedStory = false;
	};

	/** The disc map field map Number is read from: a map of its own's base, any other map's own number. */
	WELTALLCORE_API int32 FieldMapReadFrom(int32 Number, const FFieldMapStory& Story);

	/**
	 * Puts the story in field map Number, Map read from the disc map FieldMapReadFrom names, and gives its scripts back
	 * parsed: a map of its own built from or over its base, a disc map given whole where the disc's map is the one it was
	 * written for, then the port's script fixes and the documents written for the map that the settings leave on
	 * (StoryPacks.h). A document the disc's map is not the one for is left out and added to LeftOut, a map of its own's
	 * too, its base's scripts then left as they are: such a map cannot be played. Image and Index are where a map's
	 * encounters and the messages a document names of the disc's own are read. An error names the document that cannot
	 * be built or added.
	 */
	WELTALLCORE_API TResult<FEventScripts> PutStoryInFieldMap(int32 Number, FFieldMap& Map, const FCdImage& Image, const FGameFileIndex& Index,
		const FFieldMapStory& Story, std::vector<const FStoryDocument*>& LeftOut);
}
