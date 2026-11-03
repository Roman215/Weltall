#include "Media/Iso9660.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <cctype>

namespace Weltall
{
	namespace
	{
		constexpr uint32 PrimaryVolumeDescriptorLba = 16;
		constexpr std::size_t RootRecordOffset = 156;
		constexpr uint8 DirectoryFlag = 0x02;
		/** Deeper trees are malformed; the bound keeps a corrupt image from looping forever. */
		constexpr std::size_t MaxDirectories = 4096;

		std::string NormalizePath(std::string_view Path)
		{
			std::string Normal;
			for (const char C : Path)
			{
				if (C == ';')
				{
					break;
				}
				Normal.push_back(C == '\\' ? '/' : static_cast<char>(std::toupper(static_cast<unsigned char>(C))));
			}
			const auto First = Normal.find_first_not_of('/');
			return First == std::string::npos ? std::string() : Normal.substr(First);
		}

		/** Parses one directory record; returns false for the "." and ".." records. */
		bool ParseRecord(std::span<const uint8> Record, const std::string& Parent, FIsoEntry& Entry)
		{
			const uint8 NameLength = Record[32];
			if (Record.size() < 33u + NameLength)
			{
				return false;
			}
			const std::string_view RawName(reinterpret_cast<const char*>(Record.data() + 33), NameLength);
			if (RawName == std::string_view("\0", 1) || RawName == std::string_view("\1", 1))
			{
				return false;
			}
			const std::string Name(RawName.substr(0, RawName.find(';')));
			Entry.Path = Parent.empty() ? Name : Parent + "/" + Name;
			Entry.Lba = LittleEndian::ReadU32(Record, 2);
			Entry.Size = LittleEndian::ReadU32(Record, 10);
			Entry.bDirectory = (Record[25] & DirectoryFlag) != 0;
			// The XA system-use area follows the name (padded to an even length) and is tagged "XA".
			const std::size_t SystemUse = 33u + NameLength + (NameLength % 2 == 0 ? 1u : 0u);
			Entry.XaAttributes = 0;
			if (Record.size() >= SystemUse + 8 && Record[SystemUse + 6] == 'X' && Record[SystemUse + 7] == 'A')
			{
				Entry.XaAttributes = static_cast<uint16>(Record[SystemUse + 4] << 8 | Record[SystemUse + 5]);
			}
			return true;
		}
	}

	TResult<FIso9660> FIso9660::Open(FCdImage Image)
	{
		auto Descriptor = Image.ReadUserData(PrimaryVolumeDescriptorLba);
		if (!Descriptor || Descriptor->size() < CdSector::Form1DataSize || (*Descriptor)[0] != 1
			|| std::string_view(reinterpret_cast<const char*>(Descriptor->data() + 1), 5) != "CD001")
		{
			return MakeError("no ISO 9660 primary volume descriptor");
		}
		const std::span<const uint8> Root(Descriptor->data() + RootRecordOffset, 34);

		struct FPendingDirectory
		{
			uint32 Lba;
			uint32 Size;
			std::string Path;
		};
		std::vector<FPendingDirectory> Pending{{LittleEndian::ReadU32(Root, 2), LittleEndian::ReadU32(Root, 10), std::string()}};
		std::vector<FIsoEntry> Entries;
		std::size_t Visited = 0;
		while (!Pending.empty())
		{
			if (++Visited > MaxDirectories)
			{
				return MakeError("directory tree is too deep or cyclic");
			}
			const FPendingDirectory Directory = Pending.back();
			Pending.pop_back();
			auto Data = Image.ReadForm1(Directory.Lba, Directory.Size);
			if (!Data)
			{
				return MakeError("cannot read directory " + Directory.Path + ": " + Data.Error().Message);
			}
			// Records never cross a sector boundary; a zero length byte pads to the next sector.
			for (std::size_t Offset = 0; Offset < Data->size();)
			{
				const uint8 Length = (*Data)[Offset];
				if (Length == 0)
				{
					Offset = (Offset / CdSector::Form1DataSize + 1) * CdSector::Form1DataSize;
					continue;
				}
				if (Offset + Length > Data->size())
				{
					break;
				}
				FIsoEntry Entry;
				if (ParseRecord(std::span<const uint8>(Data->data() + Offset, Length), Directory.Path, Entry))
				{
					if (Entry.bDirectory)
					{
						Pending.push_back({Entry.Lba, Entry.Size, Entry.Path});
					}
					Entries.push_back(std::move(Entry));
				}
				Offset += Length;
			}
		}
		return FIso9660(std::move(Image), std::move(Entries));
	}

	FIso9660::FIso9660(FCdImage InImage, std::vector<FIsoEntry> InEntries)
		: CdImage(std::move(InImage))
		, AllEntries(std::move(InEntries))
	{
	}

	const FIsoEntry* FIso9660::Find(std::string_view Path) const
	{
		const std::string Wanted = NormalizePath(Path);
		const auto Found = std::find_if(AllEntries.begin(), AllEntries.end(), [&Wanted](const FIsoEntry& Entry) { return NormalizePath(Entry.Path) == Wanted; });
		return Found == AllEntries.end() ? nullptr : &*Found;
	}

	TResult<std::vector<uint8>> FIso9660::ReadFile(const FIsoEntry& Entry) const
	{
		const TResult<std::optional<FFileReplacement>> Mod = Entry.bDirectory ? TResult<std::optional<FFileReplacement>>(std::nullopt) : CdImage.ReadModFileAt(Entry.Lba);
		if (!Mod)
		{
			return Mod.Error();
		}
		return ReadFileRange(Entry, 0, *Mod ? (*Mod)->Size : Entry.Size);
	}

	TResult<std::vector<uint8>> FIso9660::ReadFile(std::string_view Path) const
	{
		const FIsoEntry* Entry = Find(Path);
		if (Entry == nullptr)
		{
			return MakeError(std::string(Path) + " is not on the disc");
		}
		return ReadFile(*Entry);
	}

	TResult<std::vector<uint8>> FIso9660::ReadFileRange(const FIsoEntry& Entry, uint64 Offset, uint64 Size) const
	{
		if (Entry.bDirectory || !Entry.IsForm1Data())
		{
			return MakeError(Entry.Path + " is not a Form 1 data file");
		}
		// A mod's file standing for the disc's (the Italian translation's executable) is read in its place.
		TResult<std::optional<FFileReplacement>> Mod = CdImage.ReadModFileAt(Entry.Lba);
		if (!Mod)
		{
			return Mod.Error();
		}
		if (*Mod)
		{
			const uint64 ModSize = (*Mod)->Size;
			if (Offset > ModSize || Size > ModSize - Offset)
			{
				return MakeError("range outside " + Entry.Path);
			}
			std::vector<uint8>& Bytes = (*Mod)->Bytes;
			return std::vector<uint8>(Bytes.begin() + static_cast<std::ptrdiff_t>(Offset), Bytes.begin() + static_cast<std::ptrdiff_t>(Offset + Size));
		}
		if (Offset > Entry.Size || Size > Entry.Size - Offset)
		{
			return MakeError("range outside " + Entry.Path);
		}
		const uint32 FirstSector = static_cast<uint32>(Offset / CdSector::Form1DataSize);
		const uint64 Skip = Offset % CdSector::Form1DataSize;
		auto Data = CdImage.ReadForm1(Entry.Lba + FirstSector, Skip + Size);
		if (!Data)
		{
			return Data.Error();
		}
		Data->erase(Data->begin(), Data->begin() + static_cast<std::ptrdiff_t>(Skip));
		return Data;
	}
}
