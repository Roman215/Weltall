#pragma once

#include "Foundation/Result.h"
#include "Mods/ItalianPatch.h"
#include "Mods/PerfectWorks.h"

#include <filesystem>
#include <optional>

namespace Weltall
{
	class FGameLibrary;

	/** The mods found in the player's Mods folder, each in a folder of its own. */
	struct FFoundMods
	{
		/** Perfect Works Build, unpacked in Mods/PerfectWorks. */
		std::optional<FPerfectWorksRelease> PerfectWorks;
		/** SadNES cITy's Italian translation: its patch file, in a folder of Mods, with its movie patches beside it. */
		std::optional<FItalianPatch> Italian;
	};

	/** The mods in Folder, the Mods folder beside Games. */
	WELTALLCORE_API FFoundMods FindMods(const std::filesystem::path& Folder);

	/**
	 * Every English disc of Library given the files the options take from Mods (the undubs keep the USA's records), or none:
	 * the discs' own. The Italian translation in use translates the discs that run the USA release's program and leaves
	 * Perfect Works unused, both being translations of the script. An error names a disc whose index cannot be read.
	 */
	WELTALLCORE_API std::optional<FError> ApplyMods(const FGameLibrary& Library, const FFoundMods& Mods, const FPerfectWorksOptions& Options,
		const FItalianOptions& Italian = {});
}
