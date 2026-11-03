#pragma once

#include "Foundation/ByteSource.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscModFiles.h"
#include "Media/DiscRawPatch.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Weltall
{
	class FDiscAssetStore;
	class FMesFontSheet;
	class FMessageOverrides;
	struct FIndexRecord;
	namespace CdSector
	{
		constexpr uint32 RawSize = 2352;
		constexpr uint32 CookedSize = 2048;
		constexpr uint32 Form1DataSize = 2048;
		constexpr uint32 Form2DataSize = 2324;
		/** Submode bits of a Mode 2 subheader. */
		constexpr uint8 SubmodeVideo = 0x02;
		constexpr uint8 SubmodeAudio = 0x04;
		constexpr uint8 SubmodeData = 0x08;
		constexpr uint8 SubmodeForm2 = 0x20;
	}

	/** Mode and CD-ROM XA subheader of one sector. */
	struct FCdSectorHeader
	{
		uint8 Mode = 1;
		uint8 FileNumber = 0;
		uint8 Channel = 0;
		uint8 Submode = CdSector::SubmodeData;
		/** How an XA-ADPCM sector's sound is coded (FXaCodingInfo). */
		uint8 CodingInfo = 0;

		bool IsForm2() const { return Mode == 2 && (Submode & CdSector::SubmodeForm2) != 0; }
	};

	/**
	 * The data track of a CD image: raw 2352-byte sectors (every Mode 2 form is readable) or cooked
	 * 2048-byte ones (Form 1 data only; Form 2 sectors such as XA audio are simply absent).
	 */
	class FCdImage
	{
	public:
		/** Opens a .cue sheet (its first data track), a CHD (the same) or a single .bin/.iso/.img file. */
		WELTALLCORE_API static TResult<FCdImage> Open(const std::filesystem::path& Path);

		/** A data track of SectorSize-byte sectors starting TrackOffset bytes into Source. */
		WELTALLCORE_API FCdImage(std::shared_ptr<const IByteSource> InSource, uint32 InSectorSize, uint64 InTrackOffset);

		bool IsRaw() const { return SectorSize == CdSector::RawSize; }
		uint32 SectorCount() const { return Sectors; }

		/** Original sectors, including XA headers and padding, for lossless asset export. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadSourceSector(uint32 Lba) const;
		void SetAssetStore(std::shared_ptr<FDiscAssetStore> Store) const { Assets = std::move(Store); }
		const std::shared_ptr<FDiscAssetStore>& GetAssetStore() const { return Assets; }
		/** A mod's files standing for some of the disc's, beneath the asset store's replacements; none: the disc's own. */
		void SetModFiles(std::shared_ptr<const FDiscModFiles> Files) const { Mods = std::move(Files); }
		/** The changes to the game's code that the mod's files in use need; none without a mod. */
		FModProgramChanges ModChanges() const { return Mods ? Mods->Changes() : FModProgramChanges{}; }
		/**
		 * A patch of the image's raw sectors (a translation's movie patch) applied as sectors FirstLba to EndLba (not
		 * included) are read, the ones it is made for, so a read elsewhere never waits for its index; none: the disc's own.
		 */
		void SetRawPatch(std::shared_ptr<const FDiscRawPatch> Patch, uint32 FirstLba = 0, uint32 EndLba = 0) const
		{
			RawPatch = std::move(Patch);
			RawPatchSectors = {FirstLba, EndLba};
		}
		/** No asset store, no mod and no raw patch: the disc's files as they are, which the exports read. */
		void UseDiscAlone() const
		{
			Assets.reset();
			Mods.reset();
			RawPatch.reset();
		}
		/**
		 * A native file read in place of the disc's: the asset store's replacement (its own size taken as the record's),
		 * else a mod's; none where neither has one.
		 */
		WELTALLCORE_API TResult<std::optional<FFileReplacement>> ReadReplacement(const FIndexRecord& Record) const;
		/** The mod's file whose record starts at sector Lba, which the disc's file system reads in its file's place; none without one. */
		WELTALLCORE_API TResult<std::optional<FFileReplacement>> ReadModFileAt(uint32 Lba) const;
		/** The asset store's recording of a music sequence by its key (Weltall::MusicSequenceKey), none without one. */
		WELTALLCORE_API TResult<std::optional<std::vector<uint8>>> ReadMusicReplacement(uint64 Key) const;
		/** The player's messages the asset store gives, which the readers of the game's text put in place; none without them. */
		WELTALLCORE_API std::shared_ptr<const FMessageOverrides> MessageOverrides() const;
		/** The player's font sheet the asset store gives, which the font's load draws into its glyphs; none without one. */
		WELTALLCORE_API std::shared_ptr<const FMesFontSheet> FontSheet() const;

		WELTALLCORE_API TResult<FCdSectorHeader> ReadHeader(uint32 Lba) const;

		/** The sector's user data: 2048 bytes for Mode 1 and Form 1, 2324 for Form 2. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadUserData(uint32 Lba) const;

		/** Size bytes of Form 1 user data from consecutive sectors, starting at Lba. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadForm1(uint32 Lba, uint64 Size) const;

	private:
		WELTALLCORE_API bool ReadRaw(uint32 Lba, std::span<uint8> Destination) const;

		std::shared_ptr<const IByteSource> Source;
		uint32 SectorSize;
		uint64 TrackOffset;
		uint32 Sectors;
		mutable std::shared_ptr<FDiscAssetStore> Assets;
		mutable std::shared_ptr<const FDiscModFiles> Mods;
		mutable std::shared_ptr<const FDiscRawPatch> RawPatch;
		mutable std::array<uint32, 2> RawPatchSectors{};
	};
}
