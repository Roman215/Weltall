#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/CdImage.h"
#include "Media/MovieSubtitles.h"
#include "Media/PsxExecutable.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	/** Where one file of the game's index starts on the disc and how long it is. */
	struct FIndexRecord
	{
		uint32 Lba = 0;
		/** In bytes; a negative size heads a group of that many records, 0 marks an unused slot. */
		int32 Size = 0;

		bool IsFile() const { return Size > 0; }
		bool operator==(const FIndexRecord&) const = default;
	};

	/**
	 * The game's own file index, which the boot executable carries: every file on the disc as a
	 * record, and a directory table giving the record each directory starts at. The game names a file
	 * by its directory and a 1-based number counted from the directory's first record, and those names
	 * are the same on both discs.
	 */
	class FGameFileIndex
	{
	public:
		static constexpr int32 DirectoryCount = 61;

		WELTALLCORE_API static TResult<FGameFileIndex> Read(const FPsxExecutable& Executable);

		const std::vector<FIndexRecord>& Records() const { return AllRecords; }

		/** File Number of Directory, or null when the directory or the file does not exist. */
		WELTALLCORE_API const FIndexRecord* Find(int32 Directory, int32 Number) const;

		/** A file's name within the directories: in the directory starting closest before it (the lowest of directories starting together), its number. */
		struct FFileName
		{
			int32 Directory = 0;
			int32 Number = 0;
		};
		/** Record's name, or none for a record before every directory's start. */
		WELTALLCORE_API std::optional<FFileName> NameOf(std::size_t Record) const;

		/** The movie subtitles the supported undub keeps in the index's spare space; none on other discs. */
		const FMovieSubtitleTable& MovieSubtitles() const { return Subtitles; }

		/**
		 * The record the French translation's kernel counts its own files from (file 1 itself): the base the undub keeps
		 * in the index's spare space for the records it appends, so that each disc names its own, which the translation
		 * follows with its logo and credits. None on other discs.
		 */
		std::optional<std::size_t> TranslationFilesStart() const { return TranslationStart; }

	private:
		std::vector<FIndexRecord> AllRecords;
		/** 1-based record numbers; 0 and FFFFh mean no directory. */
		std::array<uint16, DirectoryCount> DirectoryStarts{};
		FMovieSubtitleTable Subtitles;
		std::optional<std::size_t> TranslationStart;
	};

	/**
	 * A file's sectors in full: its bytes, then the rest of its last sector. The game always reads
	 * whole sectors, and compressed streams can end in that padding; a file's own bytes are the first
	 * Size of them.
	 */
	WELTALLCORE_API TResult<std::vector<uint8>> ReadFileSectors(const FCdImage& Image, const FIndexRecord& Record);

	/** A file's own bytes: the disc's to the size its record gives, a mod's to its own (which may be longer). */
	WELTALLCORE_API TResult<std::vector<uint8>> ReadFileBytes(const FCdImage& Image, const FIndexRecord& Record);
}
