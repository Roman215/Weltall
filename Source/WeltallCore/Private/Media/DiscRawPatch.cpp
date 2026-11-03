#include "Media/DiscRawPatch.h"

#include "Foundation/FileByteSource.h"
#include "Foundation/LittleEndian.h"
#include "Media/CdImage.h"

#include <algorithm>
#include <string_view>

namespace Weltall
{
	namespace
	{
		constexpr std::string_view Mark = "PPF30";
		constexpr uint8 Method = 2;
		constexpr std::size_t HeaderBytes = 60;
		constexpr std::size_t ImageTypeAt = 56;
		constexpr std::size_t BlockCheckAt = 57;
		constexpr std::size_t UndoAt = 58;
		constexpr std::size_t CheckBytes = 1024;
		/** Where the block check's bytes lie in a plain image: sector 16's, 20h bytes in. */
		constexpr uint32 CheckSector = 16;
		constexpr std::size_t CheckInSector = 0x20;
		constexpr std::string_view DizBegin = "@BEGIN_FILE_ID.DIZ";
		constexpr std::string_view DizEnd = "@END_FILE_ID.DIZ";
		constexpr std::size_t DizLengthBytes = 2;
		constexpr std::size_t RecordHeader = 9;
		/** The windows the records are read through: the index's whole pass, and a sector's few records. */
		constexpr std::size_t IndexWindow = 1 << 20;
		constexpr std::size_t SectorWindow = 16 << 10;

		uint64 ReadU64(std::span<const uint8> Bytes, std::size_t Offset)
		{
			return uint64{LittleEndian::ReadU32(Bytes, Offset)} | uint64{LittleEndian::ReadU32(Bytes, Offset + 4)} << 32;
		}

		/** The records' bytes read through a window of the file that moves on as they are asked for. */
		class FWindow
		{
		public:
			FWindow(const FFileByteSource& InSource, uint64 InEnd, std::size_t InSize) : Source(InSource), End(InEnd), Size(InSize) {}

			/** Count bytes at Place; empty where they run past the records or cannot be read. */
			std::span<const uint8> At(uint64 Place, std::size_t Count)
			{
				if (Place > End || Count > End - Place)
				{
					return {};
				}
				if (Place < Start || Place + Count > Start + Bytes.size())
				{
					Start = Place;
					Bytes.resize(static_cast<std::size_t>(std::min<uint64>(std::max(Size, Count), End - Place)));
					if (!Source.Read(Start, Bytes))
					{
						Bytes.clear();
						return {};
					}
				}
				return std::span<const uint8>(Bytes).subspan(static_cast<std::size_t>(Place - Start), Count);
			}

		private:
			const FFileByteSource& Source;
			uint64 End;
			std::size_t Size;
			uint64 Start = 0;
			std::vector<uint8> Bytes;
		};

		/**
		 * Each sector the records write into, with the place of the first that does: the records run in order of their
		 * offsets in every patch Weltall reads (the team's four, checked); one out of order makes the index empty.
		 */
		std::vector<std::pair<uint32, uint64>> MakeIndex(std::unique_ptr<FFileByteSource> Source, uint64 At, uint64 End, bool bUndo)
		{
			std::vector<std::pair<uint32, uint64>> Index;
			FWindow Window(*Source, End, IndexWindow);
			uint64 Reached = 0;
			while (At < End)
			{
				const std::span<const uint8> Head = Window.At(At, RecordHeader);
				if (Head.empty())
				{
					return {};
				}
				const uint64 Offset = ReadU64(Head, 0);
				const uint8 Length = Head[8];
				if (Offset < Reached)
				{
					return {};
				}
				if (Length != 0)
				{
					const uint32 First = static_cast<uint32>(Offset / CdSector::RawSize);
					const uint32 Last = static_cast<uint32>((Offset + Length - 1) / CdSector::RawSize);
					for (uint32 Sector = First; Sector <= Last; ++Sector)
					{
						if (Index.empty() || Index.back().first < Sector)
						{
							Index.emplace_back(Sector, At);
						}
					}
					Reached = Offset + Length;
				}
				At += RecordHeader + Length * (bUndo ? 2u : 1u);
			}
			return Index;
		}
	}

	FDiscRawPatch::~FDiscRawPatch() = default;

	TResult<std::shared_ptr<const FDiscRawPatch>> FDiscRawPatch::Open(const std::filesystem::path& Path)
	{
		auto Source = FFileByteSource::Open(Path);
		auto Indexing = FFileByteSource::Open(Path);
		if (!Source || !Indexing)
		{
			return MakeError("cannot open a movie patch: " + Path.string());
		}
		const uint64 Size = (*Source)->Size();
		std::vector<uint8> Header(HeaderBytes);
		if (Size < HeaderBytes || !(*Source)->Read(0, Header) || std::string_view(reinterpret_cast<const char*>(Header.data()), Mark.size()) != Mark
			|| Header[Mark.size()] != Method)
		{
			return MakeError("not a PPF 3.0 patch: " + Path.string());
		}
		if (Header[ImageTypeAt] != 0)
		{
			return MakeError("a PPF patch of another kind of image than a plain one: " + Path.string());
		}
		std::shared_ptr<FDiscRawPatch> Patch(new FDiscRawPatch());
		Patch->bUndo = Header[UndoAt] != 0;
		Patch->RecordsAt = HeaderBytes;
		if (Header[BlockCheckAt] != 0)
		{
			Patch->Check.resize(CheckBytes);
			if (Size < HeaderBytes + CheckBytes || !(*Source)->Read(HeaderBytes, Patch->Check))
			{
				return MakeError("a PPF patch cut short: " + Path.string());
			}
			Patch->RecordsAt += CheckBytes;
		}
		Patch->RecordsEnd = Size;
		std::vector<uint8> Tail(DizEnd.size() + DizLengthBytes);
		if (Size >= Patch->RecordsAt + Tail.size() && (*Source)->Read(Size - Tail.size(), Tail)
			&& std::string_view(reinterpret_cast<const char*>(Tail.data()), DizEnd.size()) == DizEnd)
		{
			const uint64 Diz = DizBegin.size() + LittleEndian::ReadU16(Tail, DizEnd.size()) + Tail.size();
			std::vector<uint8> Begin(DizBegin.size());
			if (Diz > Size - Patch->RecordsAt || !(*Source)->Read(Size - Diz, Begin)
				|| std::string_view(reinterpret_cast<const char*>(Begin.data()), Begin.size()) != DizBegin)
			{
				return MakeError("a PPF patch's FILE_ID.DIZ is malformed: " + Path.string());
			}
			Patch->RecordsEnd = Size - Diz;
		}
		Patch->Source = std::move(*Source);
		Patch->Index = std::async(std::launch::async, MakeIndex, std::move(*Indexing), Patch->RecordsAt, Patch->RecordsEnd, Patch->bUndo).share();
		return std::shared_ptr<const FDiscRawPatch>(std::move(Patch));
	}

	bool FDiscRawPatch::Fits(const FCdImage& Image) const
	{
		if (Check.empty())
		{
			return true;
		}
		const TResult<std::vector<uint8>> Sector = Image.ReadSourceSector(CheckSector);
		return Image.IsRaw() && Sector && std::equal(Check.begin(), Check.end(), Sector->begin() + CheckInSector);
	}

	bool FDiscRawPatch::Apply(uint32 Lba, std::span<uint8> Sector) const
	{
		const FIndex& Sectors = Index.get();
		const auto Found = std::lower_bound(Sectors.begin(), Sectors.end(), Lba, [](const std::pair<uint32, uint64>& Entry, uint32 Wanted) { return Entry.first < Wanted; });
		if (Found == Sectors.end() || Found->first != Lba)
		{
			return true;
		}
		const uint64 SectorStart = static_cast<uint64>(Lba) * CdSector::RawSize;
		const uint64 SectorEnd = SectorStart + Sector.size();
		FWindow Window(*Source, RecordsEnd, SectorWindow);
		for (uint64 At = Found->second; At < RecordsEnd;)
		{
			const std::span<const uint8> Head = Window.At(At, RecordHeader);
			if (Head.empty())
			{
				return false;
			}
			const uint64 Offset = ReadU64(Head, 0);
			const uint8 Length = Head[8];
			if (Offset >= SectorEnd)
			{
				break;
			}
			const std::span<const uint8> Bytes = Window.At(At + RecordHeader, Length);
			if (Bytes.size() != Length)
			{
				return false;
			}
			for (uint64 Place = std::max(Offset, SectorStart); Place < std::min(Offset + Length, SectorEnd); ++Place)
			{
				Sector[static_cast<std::size_t>(Place - SectorStart)] = Bytes[static_cast<std::size_t>(Place - Offset)];
			}
			At += RecordHeader + Length * (bUndo ? 2u : 1u);
		}
		return true;
	}

	std::size_t FDiscRawPatch::SectorCount() const
	{
		return Index.get().size();
	}
}
