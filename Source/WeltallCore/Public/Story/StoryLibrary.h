#pragma once

#include "Foundation/Result.h"
#include "Mods/ExpansionMods.h"
#include "Story/StoryPacks.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * The story a game can be played with: the port's own packs (Content/Story), and the Disc 2 expansions of the Mods
	 * folder, each with the port's packs beside it. A game plays one expansion, known by its mod's folder; one whose
	 * documents cannot be read is kept with what is wrong, for the player to see, and plays as none.
	 */
	class FStoryLibrary
	{
	public:
		struct FExpansion
		{
			FExpansionMod Mod;
			/** The port's packs with the expansion's documents; none where Problem says why not. */
			std::optional<FStoryPacks> Packs;
			std::optional<FError> Problem;
			/** Set where its documents are all as Export Disc 2 wrote them: it changes nothing of the disc, so it plays as none. */
			bool bUnchanged = false;
			/** Its documents written for another disc than the player's, once tried on it (ExpansionTrial.h); each is left out as it loads. */
			std::vector<std::string> ForAnotherDisc{};
		};

		FStoryLibrary() = default;
		/** Own, and each of Mods read beside it. */
		WELTALLCORE_API FStoryLibrary(FStoryPacks Own, std::span<const FExpansionMod> Mods);

		/** The port's own packs: the story of a game that plays no expansion. */
		const FStoryPacks& Own() const { return OwnPacks; }
		const std::vector<FExpansion>& Expansions() const { return Found; }

		/** The expansion of the mod in folder Folder that can be played, null where there is none. */
		WELTALLCORE_API const FExpansion* Expansion(std::string_view Folder) const;

		/** The expansion the second disc's question offers with Options: none where they turn expansions off or none can be played. */
		WELTALLCORE_API const FExpansion* Offered(const FExpansionOptions& Options) const;

		/** The story of a game that plays the expansion of folder Folder: the port's own packs where none of it can be played. */
		WELTALLCORE_API const FStoryPacks& For(std::string_view Folder) const;

		/** Keeps what trying the expansion of folder Folder on the player's second disc found: Documents, its documents written for another disc. */
		WELTALLCORE_API void SetForAnotherDisc(std::string_view Folder, std::vector<std::string> Documents);

	private:
		FStoryPacks OwnPacks;
		std::vector<FExpansion> Found;
	};
}
