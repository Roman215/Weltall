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

	/**
	 * Export Disc 2: the second disc written as a Disc 2 expansion to edit, a folder of the Mods folder
	 * (ExpansionMods.h) with a mod.txt and, in its disc2 folder, every field map the disc holds given whole
	 * (map-617.txt) and every battle event's message table given whole (battle-event-019.txt), as the disc and the
	 * mods in use give them, without the player's Assets. Each document carries the export's mark (DiscIdentity.h), so
	 * the folder changes nothing until one is edited. A file already there is kept.
	 */
	class FDisc2Export
	{
	public:
		/** The folder of the Mods folder the export writes. */
		static constexpr std::string_view Folder = "Disc 2 (export)";

		/** An export of Disc, the second disc, into ModFolder (the Mods folder's Folder); an error where its battle events cannot be read. */
		WELTALLCORE_API static TResult<std::unique_ptr<FDisc2Export>> Open(const FIso9660& Disc, const FGameFileIndex& Index, std::filesystem::path ModFolder);
		WELTALLCORE_API ~FDisc2Export();

		/** Writes the next few files; true once every file is written. An error names the map or event that cannot be written. */
		WELTALLCORE_API TResult<bool> Step();

		/** How many of the disc's maps and events are done, of TotalFiles: the maps the disc keeps placeholders for counted as they are passed. */
		WELTALLCORE_API int32 CompletedFiles() const;
		WELTALLCORE_API int32 TotalFiles() const;

	private:
		struct FState;
		explicit FDisc2Export(std::unique_ptr<FState> InState);
		std::unique_ptr<FState> State;
	};
}
