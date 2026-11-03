#include "Media/CdImage.h"

#include "Foundation/FileByteSource.h"
#include "Media/ChdDisc.h"
#include "Media/CueSheet.h"
#include "Media/DiscAssets.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Weltall
{
	namespace
	{
		constexpr std::array<uint8, 12> SyncPattern = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
		constexpr std::size_t ModeOffset = 15;
		constexpr std::size_t Mode1DataOffset = 16;
		constexpr std::size_t SubheaderOffset = 16;
		constexpr std::size_t Mode2DataOffset = 24;

		TResult<std::string> ReadText(const std::filesystem::path& Path)
		{
			std::ifstream Stream(Path, std::ios::binary);
			if (!Stream)
			{
				return MakeError("cannot open " + Path.string());
			}
			std::ostringstream Text;
			Text << Stream.rdbuf();
			return Text.str();
		}

		/** A raw sector with the 96 bytes of its subchannel after it, as some rippers write them. */
		constexpr uint32 SectorWithSubchannel = CdSector::RawSize + 96;

		bool HasSyncAt(const IByteSource& Source, uint64 Offset)
		{
			std::array<uint8, SyncPattern.size()> Head{};
			return Source.Read(Offset, Head) && Head == SyncPattern;
		}

		/**
		 * A file that starts with the CD sync pattern holds raw sectors, with their subchannel after each where the
		 * next sync lies past it; anything else is cooked.
		 */
		uint32 DetectSectorSize(const IByteSource& Source)
		{
			if (!HasSyncAt(Source, 0))
			{
				return CdSector::CookedSize;
			}
			return !HasSyncAt(Source, CdSector::RawSize) && HasSyncAt(Source, SectorWithSubchannel) ? SectorWithSubchannel : CdSector::RawSize;
		}

		/** Raw sectors with their subchannel between them, Offset bytes into a file: the sectors alone, back to back. */
		class FSubchannelSkipped final : public IByteSource
		{
		public:
			FSubchannelSkipped(std::shared_ptr<const IByteSource> InSource, uint64 InOffset) : Source(std::move(InSource)), Offset(InOffset) {}

			uint64 Size() const override
			{
				const uint64 Bytes = Source->Size() - std::min(Source->Size(), Offset);
				return Bytes / SectorWithSubchannel * CdSector::RawSize;
			}

			bool Read(uint64 At, std::span<uint8> Destination) const override
			{
				if (At > Size() || Destination.size() > Size() - At)
				{
					return false;
				}
				for (std::size_t Done = 0; Done < Destination.size();)
				{
					const uint64 Sector = (At + Done) / CdSector::RawSize;
					const uint64 Within = (At + Done) % CdSector::RawSize;
					const std::size_t Take = static_cast<std::size_t>(std::min<uint64>(CdSector::RawSize - Within, Destination.size() - Done));
					if (!Source->Read(Offset + Sector * SectorWithSubchannel + Within, Destination.subspan(Done, Take)))
					{
						return false;
					}
					Done += Take;
				}
				return true;
			}

		private:
			std::shared_ptr<const IByteSource> Source;
			uint64 Offset;
		};
	}

	TResult<FCdImage> FCdImage::Open(const std::filesystem::path& Path)
	{
		std::filesystem::path DataPath = Path;
		uint32 SectorSize = 0;
		uint64 TrackOffset = 0;
		auto Extension = Path.extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](char C) { return static_cast<char>(std::tolower(static_cast<unsigned char>(C))); });
		if (Extension == ".cue")
		{
			auto Text = ReadText(Path);
			if (!Text)
			{
				return Text.Error();
			}
			auto Tracks = ParseCueSheet(*Text, Path.parent_path());
			if (!Tracks)
			{
				return MakeError(Path.string() + ": " + Tracks.Error().Message);
			}
			const auto DataTrack = std::find_if(Tracks->begin(), Tracks->end(), [](const FCueTrack& Track) { return !Track.IsAudio(); });
			if (DataTrack == Tracks->end())
			{
				return MakeError(Path.string() + " has no data track");
			}
			DataPath = DataTrack->File;
			SectorSize = DataTrack->SectorSize;
			TrackOffset = static_cast<uint64>(DataTrack->StartFrame) * SectorSize;
		}
		auto File = FFileByteSource::Open(DataPath);
		if (!File)
		{
			return File.Error();
		}
		std::shared_ptr<const IByteSource> Source = std::move(*File);
		if (SectorSize == 0 && IsChd(*Source))
		{
			return OpenChdImage(std::move(Source));
		}
		if (SectorSize == 0)
		{
			SectorSize = DetectSectorSize(*Source);
		}
		if (SectorSize == SectorWithSubchannel)
		{
			return FCdImage(std::make_shared<FSubchannelSkipped>(std::move(Source), TrackOffset), CdSector::RawSize, 0);
		}
		if (SectorSize != CdSector::RawSize && SectorSize != CdSector::CookedSize)
		{
			return MakeError(DataPath.string() + ": unsupported sector size " + std::to_string(SectorSize));
		}
		return FCdImage(std::move(Source), SectorSize, TrackOffset);
	}

	FCdImage::FCdImage(std::shared_ptr<const IByteSource> InSource, uint32 InSectorSize, uint64 InTrackOffset)
		: Source(std::move(InSource))
		, SectorSize(InSectorSize)
		, TrackOffset(InTrackOffset)
		, Sectors(static_cast<uint32>((Source->Size() - std::min(Source->Size(), InTrackOffset)) / InSectorSize))
	{
	}

	bool FCdImage::ReadRaw(uint32 Lba, std::span<uint8> Destination) const
	{
		if (Lba >= Sectors || !Source->Read(TrackOffset + static_cast<uint64>(Lba) * SectorSize, Destination))
		{
			return false;
		}
		if (RawPatch && IsRaw())
		{
			for (std::size_t Sector = 0; Sector * SectorSize < Destination.size(); ++Sector)
			{
				const uint32 Read = Lba + static_cast<uint32>(Sector);
				if (Read >= RawPatchSectors[0] && Read < RawPatchSectors[1]
					&& !RawPatch->Apply(Read, Destination.subspan(Sector * SectorSize, std::min<std::size_t>(SectorSize, Destination.size() - Sector * SectorSize))))
				{
					return false;
				}
			}
		}
		return true;
	}

	TResult<std::vector<uint8>> FCdImage::ReadSourceSector(uint32 Lba) const
	{
		std::vector<uint8> Bytes(SectorSize);
		if (!ReadRaw(Lba, Bytes)) return MakeError("cannot read sector " + std::to_string(Lba));
		return Bytes;
	}

	TResult<std::optional<FFileReplacement>> FCdImage::ReadReplacement(const FIndexRecord& Record) const
	{
		if (Assets)
		{
			TResult<std::optional<std::vector<uint8>>> Override = Assets->ReadOverride(Record);
			if (!Override)
			{
				return Override.Error();
			}
			if (*Override)
			{
				return std::optional{FFileReplacement{std::move(**Override), static_cast<uint64>(std::max(Record.Size, 0))}};
			}
		}
		return Mods ? Mods->Read(Record) : TResult<std::optional<FFileReplacement>>(std::optional<FFileReplacement>{});
	}

	TResult<std::optional<FFileReplacement>> FCdImage::ReadModFileAt(uint32 Lba) const
	{
		return Mods ? Mods->ReadAt(Lba) : TResult<std::optional<FFileReplacement>>(std::optional<FFileReplacement>{});
	}

	TResult<std::optional<std::vector<uint8>>> FCdImage::ReadMusicReplacement(uint64 Key) const
	{
		return Assets ? Assets->ReadMusic(Key) : TResult<std::optional<std::vector<uint8>>>(std::optional<std::vector<uint8>>{});
	}

	std::shared_ptr<const FMessageOverrides> FCdImage::MessageOverrides() const
	{
		return Assets ? Assets->Messages() : nullptr;
	}

	std::shared_ptr<const FMesFontSheet> FCdImage::FontSheet() const
	{
		return Assets ? Assets->FontSheet() : nullptr;
	}

	TResult<FCdSectorHeader> FCdImage::ReadHeader(uint32 Lba) const
	{
		if (!IsRaw())
		{
			if (Lba >= Sectors)
			{
				return MakeError("sector " + std::to_string(Lba) + " is outside the image");
			}
			return FCdSectorHeader{};
		}
		std::array<uint8, CdSector::RawSize> Head{};
		bool bExternal = false;
		if (Assets)
		{
			const auto Override = Assets->ReadMovieSector(Lba, Head);
			if (!Override) return Override.Error();
			bExternal = *Override;
		}
		if (!bExternal && !ReadRaw(Lba, std::span<uint8>(Head).first(Mode2DataOffset)))
		{
			return MakeError("cannot read sector " + std::to_string(Lba));
		}
		FCdSectorHeader Header;
		Header.Mode = Head[ModeOffset];
		if (Header.Mode == 2)
		{
			Header.FileNumber = Head[SubheaderOffset];
			Header.Channel = Head[SubheaderOffset + 1];
			Header.Submode = Head[SubheaderOffset + 2];
			Header.CodingInfo = Head[SubheaderOffset + 3];
		}
		return Header;
	}

	TResult<std::vector<uint8>> FCdImage::ReadUserData(uint32 Lba) const
	{
		std::vector<uint8> Sector(SectorSize);
		bool bExternal = false;
		if (Assets)
		{
			const auto Override = Assets->ReadMovieSector(Lba, Sector);
			if (!Override) return Override.Error();
			bExternal = *Override;
		}
		if (!bExternal && !ReadRaw(Lba, Sector))
		{
			return MakeError("cannot read sector " + std::to_string(Lba));
		}
		if (!IsRaw())
		{
			return Sector;
		}
		const uint8 Mode = Sector[ModeOffset];
		if (Mode == 1)
		{
			return std::vector<uint8>(Sector.begin() + Mode1DataOffset, Sector.begin() + Mode1DataOffset + CdSector::Form1DataSize);
		}
		const bool bForm2 = (Sector[SubheaderOffset + 2] & CdSector::SubmodeForm2) != 0;
		const uint32 DataSize = bForm2 ? CdSector::Form2DataSize : CdSector::Form1DataSize;
		return std::vector<uint8>(Sector.begin() + Mode2DataOffset, Sector.begin() + Mode2DataOffset + DataSize);
	}

	TResult<std::vector<uint8>> FCdImage::ReadForm1(uint32 Lba, uint64 Size) const
	{
		// The sectors come off the image in one read, then each gives its user data.
		const uint64 Count = (Size + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize;
		if (Count == 0)
		{
			return std::vector<uint8>();
		}
		if (Count > Sectors || Lba > Sectors - Count)
		{
			return MakeError("cannot read sector " + std::to_string(std::max<uint64>(Lba, Sectors)));
		}
		std::vector<uint8> Raw(static_cast<std::size_t>(Count * SectorSize));
		if (!Source->Read(TrackOffset + static_cast<uint64>(Lba) * SectorSize, Raw))
		{
			return MakeError("cannot read sectors " + std::to_string(Lba) + " to " + std::to_string(Lba + Count - 1));
		}
		if (!IsRaw())
		{
			Raw.resize(static_cast<std::size_t>(Size));
			return Raw;
		}
		std::vector<uint8> Data;
		Data.reserve(static_cast<std::size_t>(Size));
		for (std::size_t Sector = 0; Data.size() < Size; ++Sector)
		{
			const uint8* Bytes = Raw.data() + Sector * SectorSize;
			const std::size_t Offset = Bytes[ModeOffset] == 1 ? Mode1DataOffset : Mode2DataOffset;
			const std::size_t Take = std::min<std::size_t>(CdSector::Form1DataSize, static_cast<std::size_t>(Size - Data.size()));
			Data.insert(Data.end(), Bytes + Offset, Bytes + Offset + Take);
		}
		return Data;
	}
}
