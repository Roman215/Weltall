#include "Media/DiscModFiles.h"

#include "Foundation/FileByteSource.h"
#include "Foundation/LittleEndian.h"
#include "Media/CdImage.h"
#include "Media/GameFileIndex.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A field map's header words: its scripts' offset (section 5) and the next section's, which ends them. */
		constexpr std::size_t ScriptsAt = 0x51 * 4;
		constexpr std::size_t ScriptsEndAt = 0x52 * 4;
		constexpr uint64 LargestFile = 512ULL * 1024 * 1024;

		/** Size bytes of Path from Offset (Size 0: from Offset to its end). */
		TResult<std::vector<uint8>> ReadPart(const std::filesystem::path& Path, uint64 Offset, uint64 Size)
		{
			const auto File = FFileByteSource::Open(Path);
			if (!File)
			{
				return File.Error();
			}
			const uint64 Whole = (*File)->Size();
			const uint64 Length = Size != 0 ? Size : Whole - std::min(Offset, Whole);
			if (Length == 0 || Length > LargestFile || Offset > Whole || Length > Whole - Offset)
			{
				return MakeError("a mod's file has an invalid size: " + Path.string());
			}
			std::vector<uint8> Bytes(static_cast<std::size_t>(Length));
			if (!(*File)->Read(Offset, Bytes))
			{
				return MakeError("cannot read a mod's file: " + Path.string());
			}
			return Bytes;
		}

		TResult<std::vector<uint8>> ReadWhole(const std::filesystem::path& Path)
		{
			return ReadPart(Path, 0, 0);
		}


		/** Scripts written over the map's own, from their offset to the next section's: cut, or ended with zeros, to fit. */
		std::optional<FError> SpliceScripts(std::vector<uint8>& Map, std::span<const uint8> Scripts, const std::filesystem::path& Path)
		{
			if (Map.size() < ScriptsEndAt + 4)
			{
				return MakeError("a mod's map is too short for its sections: " + Path.string());
			}
			const uint32 Start = LittleEndian::ReadU32(Map, ScriptsAt);
			const uint32 End = LittleEndian::ReadU32(Map, ScriptsEndAt);
			if (Start > End || End > Map.size())
			{
				return MakeError("a mod's map has no place for its scripts: " + Path.string());
			}
			const std::size_t Kept = std::min<std::size_t>(Scripts.size(), End - Start);
			std::copy_n(Scripts.begin(), Kept, Map.begin() + Start);
			std::fill(Map.begin() + Start + static_cast<std::ptrdiff_t>(Kept), Map.begin() + End, uint8{0});
			return std::nullopt;
		}

		TResult<FFileReplacement> ReadModFile(const FModFile& File)
		{
			TResult<std::vector<uint8>> Bytes = File.Bytes != nullptr ? TResult<std::vector<uint8>>(*File.Bytes) : ReadPart(File.Path, File.Offset, File.Size);
			if (!Bytes)
			{
				return Bytes.Error();
			}
			if (!File.Section5.empty())
			{
				const TResult<std::vector<uint8>> Scripts = ReadWhole(File.Section5);
				if (!Scripts)
				{
					return Scripts.Error();
				}
				if (std::optional<FError> Error = SpliceScripts(*Bytes, *Scripts, File.Path))
				{
					return *Error;
				}
			}
			FFileReplacement Replacement{std::move(*Bytes), 0};
			Replacement.Size = Replacement.Bytes.size();
			// The game reads whole sectors, and a packed stream may run on into the last one's padding.
			Replacement.Bytes.resize((Replacement.Size + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize * CdSector::Form1DataSize, 0);
			return Replacement;
		}
	}

	std::shared_ptr<const FDiscModFiles> FDiscModFiles::Make(const FGameFileIndex& Index, int32 Disc, std::span<const FModFile> Files,
		const FModProgramChanges& Changes)
	{
		auto Made = std::make_shared<FDiscModFiles>();
		Made->ProgramChanges = Changes;
		const std::vector<FIndexRecord>& Records = Index.Records();
		for (const FModFile& File : Files)
		{
			const int32 Record = Disc == 2 ? File.Record - SecondDiscShift : File.Record;
			if (Record < 0 || static_cast<std::size_t>(Record) >= Records.size() || !Records[static_cast<std::size_t>(Record)].IsFile())
			{
				continue;
			}
			Made->BySector[Records[static_cast<std::size_t>(Record)].Lba] = File;
		}
		return Made;
	}

	TResult<std::optional<FFileReplacement>> FDiscModFiles::Read(const FIndexRecord& Record) const
	{
		if (!Record.IsFile())
		{
			return std::optional<FFileReplacement>{};
		}
		return ReadAt(Record.Lba);
	}

	TResult<std::optional<FFileReplacement>> FDiscModFiles::ReadAt(uint32 Lba) const
	{
		const auto Found = BySector.find(Lba);
		if (Found == BySector.end())
		{
			return std::optional<FFileReplacement>{};
		}
		TResult<FFileReplacement> Replacement = ReadModFile(Found->second);
		if (!Replacement)
		{
			return Replacement.Error();
		}
		return std::optional{std::move(*Replacement)};
	}
}
