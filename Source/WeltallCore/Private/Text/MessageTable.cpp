#include "Text/MessageTable.h"

#include "Foundation/LittleEndian.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint16 EmptyTable = 0xFFFF;
		constexpr std::size_t OffsetsStart = 4;
	}

	TResult<FMessageTable> FMessageTable::Parse(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < OffsetsStart)
		{
			return MakeError("a message table is too short");
		}
		FMessageTable Table;
		Table.Bytes = Bytes;
		const uint16 Last = LittleEndian::ReadU16(Bytes, 0);
		Table.Entries = Last == EmptyTable ? 0 : Last + 1u;
		if (OffsetsStart + Table.Entries * 4 > Bytes.size())
		{
			return MakeError("a message table's offsets run past it");
		}
		return Table;
	}

	std::optional<int32> AppendMessages(std::vector<uint8>& Table, std::span<const FAddedMessage> Messages)
	{
		const TResult<FMessageTable> Parsed = FMessageTable::Parse(Table);
		if (!Parsed)
		{
			return std::nullopt;
		}
		const std::size_t Old = static_cast<std::size_t>(Parsed->Count());
		if (Messages.empty())
		{
			return static_cast<int32>(Old);
		}
		const std::size_t Count = Old + Messages.size();
		// Each added message takes an offset and a layout: everything after the header moves by both.
		const std::size_t Moved = Messages.size() * 4;
		std::size_t End = Table.size() + Moved;
		std::vector<std::size_t> Offsets;
		for (const FAddedMessage& Message : Messages)
		{
			Offsets.push_back(End);
			End += Message.Codes.size();
		}
		// A message the table starts past its bytes starts at the built table's end, past its bytes still.
		bool bPastBytes = std::ranges::any_of(Messages, [](const FAddedMessage& Message) { return Message.Codes.empty(); });
		for (std::size_t Index = 0; Index < Old; ++Index)
		{
			bPastBytes = bPastBytes || Parsed->StartsPastBytes(static_cast<int32>(Index));
		}
		if (Count - 1 >= EmptyTable || Offsets.back() > EmptyTable || (bPastBytes && End > EmptyTable))
		{
			return std::nullopt;
		}
		std::vector<uint8> Built(OffsetsStart + Count * 4, 0);
		LittleEndian::WriteU16(Built, 0, static_cast<uint16>(Count - 1));
		for (std::size_t Index = 0; Index < Old; ++Index)
		{
			const std::size_t Offset = Parsed->StartsPastBytes(static_cast<int32>(Index)) ? End : LittleEndian::ReadU16(Table, OffsetsStart + Index * 2) + Moved;
			LittleEndian::WriteU16(Built, OffsetsStart + Index * 2, static_cast<uint16>(Offset));
			Built[OffsetsStart + (Count + Index) * 2] = Table[OffsetsStart + (Old + Index) * 2];
			Built[OffsetsStart + (Count + Index) * 2 + 1] = Table[OffsetsStart + (Old + Index) * 2 + 1];
		}
		for (std::size_t Added = 0; Added < Messages.size(); ++Added)
		{
			const std::size_t Index = Old + Added;
			const std::size_t Offset = Messages[Added].Codes.empty() ? End : Offsets[Added];
			LittleEndian::WriteU16(Built, OffsetsStart + Index * 2, static_cast<uint16>(Offset));
			Built[OffsetsStart + (Count + Index) * 2] = Messages[Added].WidthUnits;
			Built[OffsetsStart + (Count + Index) * 2 + 1] = Messages[Added].Lines;
		}
		Built.insert(Built.end(), Table.begin() + static_cast<std::ptrdiff_t>(OffsetsStart + Old * 4), Table.end());
		for (const FAddedMessage& Message : Messages)
		{
			Built.insert(Built.end(), Message.Codes.begin(), Message.Codes.end());
		}
		Table = std::move(Built);
		return static_cast<int32>(Old);
	}

	std::span<const uint8> FMessageTable::String(int32 Index) const
	{
		if (Index < 0 || static_cast<uint32>(Index) >= Entries || StartsPastBytes(Index))
		{
			return {};
		}
		return Bytes.subspan(LittleEndian::ReadU16(Bytes, OffsetsStart + static_cast<std::size_t>(Index) * 2));
	}

	bool FMessageTable::StartsPastBytes(int32 Index) const
	{
		return Index >= 0 && static_cast<uint32>(Index) < Entries && LittleEndian::ReadU16(Bytes, OffsetsStart + static_cast<std::size_t>(Index) * 2) >= Bytes.size();
	}

	int32 FMessageTable::WidthUnits(int32 Index) const
	{
		if (Index < 0 || static_cast<uint32>(Index) >= Entries)
		{
			return 0;
		}
		return Bytes[OffsetsStart + (Entries + static_cast<std::size_t>(Index)) * 2];
	}

	int32 FMessageTable::Lines(int32 Index) const
	{
		if (Index < 0 || static_cast<uint32>(Index) >= Entries)
		{
			return 0;
		}
		return Bytes[OffsetsStart + (Entries + static_cast<std::size_t>(Index)) * 2 + 1];
	}

	TResult<FMesNameTables> FMesNameTables::Parse(std::vector<uint8> File)
	{
		if (File.size() < 4)
		{
			return MakeError("the kernel's name tables are too short");
		}
		const uint32 Count = LittleEndian::ReadU32(File, 0);
		if (4 + static_cast<std::size_t>(Count) * 4 > File.size())
		{
			return MakeError("the kernel's name table list runs past its file");
		}
		FMesNameTables Tables;
		for (uint32 Table = 0; Table < Count; ++Table)
		{
			const uint32 Offset = LittleEndian::ReadU32(File, 4 + static_cast<std::size_t>(Table) * 4);
			if (Offset >= File.size())
			{
				return MakeError("a kernel name table lies outside its file");
			}
			Tables.Offsets.push_back(Offset);
		}
		Tables.Data = std::move(File);
		return Tables;
	}

	std::span<const uint8> FMesNameTables::String(int32 Table, int32 Index) const
	{
		if (Table < 0 || Table >= Count())
		{
			return {};
		}
		const TResult<FMessageTable> Parsed = FMessageTable::Parse(std::span<const uint8>(Data).subspan(Offsets[static_cast<std::size_t>(Table)]));
		return Parsed ? Parsed->String(Index) : std::span<const uint8>();
	}

	std::span<const uint8> FMesNameTables::Table(int32 Table) const
	{
		if (Table < 0 || Table >= Count())
		{
			return {};
		}
		return std::span<const uint8>(Data).subspan(Offsets[static_cast<std::size_t>(Table)]);
	}

	std::span<const uint8> FMesNameTables::GlyphMapEntry(int32 Index) const
	{
		if (GlyphMapTable >= Count() || Index < 0)
		{
			return {};
		}
		const std::size_t Offset = Offsets[static_cast<std::size_t>(GlyphMapTable)] + static_cast<std::size_t>(Index) * 2;
		return Offset + 2 <= Data.size() ? std::span<const uint8>(Data).subspan(Offset, 2) : std::span<const uint8>();
	}
}
