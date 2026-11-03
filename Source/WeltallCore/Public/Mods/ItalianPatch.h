#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscModFiles.h"
#include "Media/DiscRawPatch.h"

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

namespace Weltall
{
	/** The movies SadNES cITy's translation plays: the discs' own, or a movie patch's, with the English sound or the Japanese. */
	enum class EItalianMovies : uint8
	{
		Original,
		English,
		Japanese,
	};

	/** The player's choices for SadNES cITy's Italian translation: its patcher's two (voices, commands) and its movies. */
	struct FItalianOptions
	{
		/** The translation is used at all; off, the game is the discs' own whatever the rest say. */
		bool bEnabled = false;
		bool bJapaneseVoices = false;
		bool bJapaneseCommands = false;
		EItalianMovies Movies = EItalianMovies::English;

		bool operator==(const FItalianOptions&) const = default;
	};

	/**
	 * SadNES cITy's Italian translation as its archives give it: the patch file holding every file it changes for each of
	 * its variants (u32 count, u32 1, 8 zero bytes, then 16-byte entries: u16 record in the first disc's numbering, u32
	 * offset, u32 size, u32 the variants' bits, u16 0), and beside it the movie patches, one a disc for each sound.
	 */
	class FItalianPatch
	{
	public:
		/** The patch in the first folder of Folder (the Mods folder) that holds one, in the order of the folders' names. */
		WELTALLCORE_API static std::optional<FItalianPatch> Find(const std::filesystem::path& Folder);
		/** The patch at Path with the movie patches beside it; none where the file is not the team's patch. */
		WELTALLCORE_API static std::optional<FItalianPatch> Read(const std::filesystem::path& Path);

		const std::filesystem::path& Path() const { return File; }

		/**
		 * The files of the variant Options choose for disc Disc (1 or 2), numbered as the first disc's records, read from
		 * the patch as they are asked for; the executable is not among them (Executable).
		 */
		WELTALLCORE_API std::vector<FModFile> Files(const FItalianOptions& Options, int32 Disc) const;

		/** The variant's executable for disc Disc, read now. */
		WELTALLCORE_API TResult<std::vector<uint8>> Executable(const FItalianOptions& Options, int32 Disc) const;

		/** Disc Disc's movie patch with the Japanese sound or the English, if one lies beside the patch. */
		WELTALLCORE_API const std::optional<std::filesystem::path>& MoviePatch(int32 Disc, bool bJapaneseSound) const;

		/**
		 * Disc Disc's movie patch for Movies, opened the first time it is asked for and kept (its index is made once); none
		 * for the discs' own movies or a sound whose patch is not there.
		 */
		WELTALLCORE_API TResult<std::shared_ptr<const FDiscRawPatch>> OpenMovies(int32 Disc, EItalianMovies Movies) const;

		/** The executable's record as the patch numbers it, the first disc's (the second disc's own is 17). */
		static constexpr int32 ExecutableRecord = 22;

	private:
		struct FEntry
		{
			int32 Record = 0;
			uint64 Offset = 0;
			uint64 Size = 0;
			uint32 Bits = 0;
		};

		/** Whether an entry belongs to the variant: it carries the variant's bit for each of the three choices. */
		static bool InVariant(const FEntry& Entry, const FItalianOptions& Options, int32 Disc);

		std::filesystem::path File;
		std::vector<FEntry> Entries;
		/** The movie patches by disc (first index) and sound (second: English, Japanese), and those opened. */
		std::array<std::array<std::optional<std::filesystem::path>, 2>, 2> Movies;
		mutable std::array<std::array<std::shared_ptr<const FDiscRawPatch>, 2>, 2> Opened;
	};
}
