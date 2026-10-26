#pragma once

#include "Media/DiscIdentification.h"
#include "Media/Iso9660.h"
#include "Media/VoiceLanguage.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Weltall
{
	/** A file in the folder the game does not play from, or plays from with something missing, and why, in a player's words. */
	struct FLibraryNote
	{
		std::filesystem::path File;
		std::string Text;
		/** The game plays from it all the same. */
		bool bPlays = false;
	};

	/** The player's discs found in a folder, at most one per supported release, and what else the folder holds. */
	class FGameLibrary
	{
	public:
		/**
		 * Opens every disc image in the folder and its subfolders (.cue sheets, .chd files, and .bin, .iso or .img
		 * files no sheet names), hidden files and folders left out (IsHiddenFile), and keeps the ones it recognizes,
		 * a patched copy of a release (a fan translation) in place of the original. What it cannot use, it notes: an
		 * image it cannot read, a disc of another game or release, a disc image of a kind it does not read, an
		 * original a patched copy replaces.
		 */
		WELTALLCORE_API static FGameLibrary Scan(const std::filesystem::path& Folder);

		/** The disc of a release, or null when the folder has none. */
		WELTALLCORE_API const FIso9660* Find(EDiscRelease Release) const;
		/**
		 * The game data played: a translation's copy of the USA release or of the supported undub when present, otherwise
		 * the USA release, otherwise the undub. Number is one or two.
		 */
		WELTALLCORE_API const FIso9660* PlayableDisc(int32 Number) const;

		/** The file a release's disc was found in; empty when there is none. */
		WELTALLCORE_API std::filesystem::path FileOf(EDiscRelease Release) const;

		WELTALLCORE_API std::vector<EDiscRelease> Releases() const;

		const std::vector<FLibraryNote>& Notes() const { return LibraryNotes; }

		/**
		 * Takes the discs of a newer scan this library lacks, keeping its own (which the game may be reading), and the
		 * newer scan's notes.
		 */
		WELTALLCORE_API void Adopt(FGameLibrary&& Newer);

	private:
		struct FFound
		{
			std::unique_ptr<FIso9660> Disc;
			std::filesystem::path File;
			/** A patched copy of its release (DiscIdentity's bPatched), which plays in place of the original. */
			bool bPatched = false;
		};

		std::map<EDiscRelease, FFound> Discs;
		std::vector<FLibraryNote> LibraryNotes;
	};
}
