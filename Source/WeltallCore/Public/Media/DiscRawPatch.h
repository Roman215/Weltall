#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <future>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FFileByteSource;

	/**
	 * A PPF 3.0 patch of a raw (2352-byte) image, read in place on disk: "PPF30", the method byte 2, a 50-byte description,
	 * the image type (0, a plain image), a block check flag, an undo flag and a pad byte; with the block check, the 1024
	 * bytes the image holds at 9320h; then records of a u64 offset, a u8 length and the bytes (as many undo bytes after
	 * them with the undo flag); last, perhaps, a FILE_ID.DIZ ("@BEGIN_FILE_ID.DIZ", its text, "@END_FILE_ID.DIZ", its
	 * length in a u16). Its records are indexed by the sectors they write into on a worker as the patch opens, the
	 * patch's whole length read once; a sector asked for before the index is made waits for it.
	 */
	class FDiscRawPatch
	{
	public:
		/** The patch at Path; an error for a file that is not a PPF 3.0 patch of a plain image. */
		WELTALLCORE_API static TResult<std::shared_ptr<const FDiscRawPatch>> Open(const std::filesystem::path& Path);

		WELTALLCORE_API ~FDiscRawPatch();
		FDiscRawPatch(const FDiscRawPatch&) = delete;
		FDiscRawPatch& operator=(const FDiscRawPatch&) = delete;

		/** Whether Image is one the patch was made for: its block check's bytes there, where it has one. */
		WELTALLCORE_API bool Fits(const FCdImage& Image) const;

		/** Writes what the patch holds for raw sector Lba into Sector (its 2352 bytes); false where the patch cannot be read. */
		WELTALLCORE_API bool Apply(uint32 Lba, std::span<uint8> Sector) const;

		/** The sectors the patch writes into, once its index is made. */
		WELTALLCORE_API std::size_t SectorCount() const;

	private:
		/** Each sector the records write into, in order, with the place in the file of the first record that does. */
		using FIndex = std::vector<std::pair<uint32, uint64>>;

		FDiscRawPatch() = default;

		std::unique_ptr<FFileByteSource> Source;
		std::vector<uint8> Check;
		uint64 RecordsAt = 0;
		uint64 RecordsEnd = 0;
		bool bUndo = false;
		std::shared_future<FIndex> Index;
	};
}
