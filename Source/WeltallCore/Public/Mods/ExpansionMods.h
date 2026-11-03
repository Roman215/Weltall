#pragma once

#include "Foundation/Types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * What a mod's mod.txt says of it, a line each, its first word naming what the rest of the line gives:
	 *
	 *   name Elly's errand
	 *   author A. Writer
	 *   version 1.2
	 *
	 * Blank lines and lines beginning ';' are passed over, and so are the words it does not know (a later release's).
	 */
	struct FModInfo
	{
		std::string Name;
		std::string Author;
		std::string Version;

		bool operator==(const FModInfo&) const = default;
	};

	WELTALLCORE_API FModInfo ParseModInfo(std::string_view Text);

	/** The longest mod folder's name a save keeps, in bytes: an expansion in a folder of a longer one cannot be played. */
	inline constexpr std::size_t ExpansionFolderBytes = 64;
	/** The folder Extract example scenes writes the port's examples to, which a save made before saves kept their expansion's plays. */
	inline constexpr std::string_view ExamplesFolder = "Disc 2 examples";

	/** A Disc 2 expansion a mod in the Mods folder holds: the documents of the mod's disc2 folder (StoryPacks.h). */
	struct FExpansionMod
	{
		/** The mod's folder's name, which a game of the expansion keeps to know it by. */
		std::string Folder;
		std::filesystem::path Documents;
		/** Its mod.txt's, with the folder's name where that gives none. */
		FModInfo Info;
	};

	/** Whether the Disc 2 expansions of the Mods folder are played, and which one the second disc's question offers. */
	struct FExpansionOptions
	{
		/** Off, the second disc is the original's whatever the Mods folder holds, and nothing is asked. */
		bool bEnabled = true;
		/** The folder of the expansion offered; empty, or one that cannot be played: the first that can. */
		std::string Folder;

		bool operator==(const FExpansionOptions&) const = default;
	};

	/** The mods of Folder (the Mods folder) with a disc2 folder, in the order of their folders' names; hidden ones left out. */
	WELTALLCORE_API std::vector<FExpansionMod> FindExpansionMods(const std::filesystem::path& Folder);
}
