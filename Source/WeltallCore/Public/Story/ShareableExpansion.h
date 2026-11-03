#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace Weltall
{
	class FGameFileIndex;
	class FIso9660;
	class FStoryPacks;

	/**
	 * Make shareable: a copy of a Disc 2 expansion (ExpansionMods.h) that carries only what its author changed or added,
	 * in the Mods folder beside it ("Folder (shareable)"). Every file of the expansion's folder is copied as it is but its
	 * disc2 documents given whole (Export Disc 2's form): one left as exported is left out, an edited one is written as
	 * the patch it amounts to, under its own name (FieldMapShareable.h, BattleEventTextPatch.h). The disc is the one a
	 * game of the second disc reads its files from, read through the mods' files without the player's Assets; the bytes
	 * the port's own fixes and documents change, every setting on, take no detour. Nothing is written until every
	 * document is read: the copy is the last step, in place of one there.
	 */
	class FShareableExpansion
	{
	public:
		static constexpr std::string_view Suffix = " (shareable)";

		/** The copy's folder: the expansion's own, with Suffix, beside it. */
		WELTALLCORE_API static std::filesystem::path CopyFolder(const std::filesystem::path& Expansion);

		/**
		 * The copy of Expansion, a folder of the Mods folder, made against Disc with Port, the port's own documents; an
		 * error where the folder or the disc's battle events cannot be read.
		 */
		WELTALLCORE_API static TResult<std::unique_ptr<FShareableExpansion>> Open(const FIso9660& Disc, const FGameFileIndex& Index, const FStoryPacks& Port,
			std::filesystem::path Expansion);
		WELTALLCORE_API ~FShareableExpansion();

		/**
		 * Reads the next few files, or once all are read writes the copy; true once it is written. An error names the
		 * document that cannot be shared and why ("Folder/disc2/map-617.txt: ...").
		 */
		WELTALLCORE_API TResult<bool> Step();

		/** How many of TotalFiles are done: the expansion's files, then the copy's writing. */
		WELTALLCORE_API int32 CompletedFiles() const;
		WELTALLCORE_API int32 TotalFiles() const;

	private:
		struct FState;
		explicit FShareableExpansion(std::unique_ptr<FState> InState);
		std::unique_ptr<FState> State;
	};
}
