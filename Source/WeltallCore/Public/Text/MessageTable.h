#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * A table of messages, the form every text of the game takes: a count, each message's offset and
	 * window size (width in 4-pixel units, lines), then the strings, each ended by a zero byte.
	 *
	 * The game never checks a table's offsets, only reads the messages its scripts name, so a table is kept
	 * even where some of its messages start past its bytes (a translation's tool can leave unused ones so).
	 */
	class FMessageTable
	{
	public:
		/** Views a table in memory that outlives it. */
		WELTALLCORE_API static TResult<FMessageTable> Parse(std::span<const uint8> Bytes);

		int32 Count() const { return static_cast<int32>(Entries); }

		/**
		 * The message's bytes to the end of the table, as the game reads them: up to its zero byte. Empty past the
		 * count, and for a message that starts past the table's bytes, where the game reads memory the table does not hold.
		 */
		WELTALLCORE_API std::span<const uint8> String(int32 Index) const;

		/** Whether message Index, one of the table's, starts past its bytes. */
		WELTALLCORE_API bool StartsPastBytes(int32 Index) const;

		/** The window size the message was written for. */
		WELTALLCORE_API int32 WidthUnits(int32 Index) const;
		WELTALLCORE_API int32 Lines(int32 Index) const;

	private:
		std::span<const uint8> Bytes;
		uint32 Entries = 0;
	};

	/** A message to add to a table: its codes, ending with 00, and the window it is written for. */
	struct FAddedMessage
	{
		/** None for a message the disc's table starts past its bytes, which stays past the new table's. */
		std::vector<uint8> Codes;
		/** In units of 4 pixels. */
		uint8 WidthUnits = 0;
		uint8 Lines = 0;
	};

	/**
	 * Adds Messages after a message table's last, in its bytes: the first's index, none (changing nothing)
	 * where the bytes are not a table or its 16-bit offsets could not reach the last added.
	 */
	WELTALLCORE_API std::optional<int32> AppendMessages(std::vector<uint8>& Table, std::span<const FAddedMessage> Messages);

	/**
	 * The kernel's name tables (MES SYSDATA, directory 1 file 7): a count and one offset
	 * per table, most of them message tables (items, abilities, names), one the map from glyph
	 * indices to codes that numbers are written with.
	 */
	class FMesNameTables
	{
	public:
		/** The table that maps glyph indices to codes, which holds no text. */
		static constexpr int32 GlyphMapTable = 27;

		/** The unpacked file; the tables are kept. */
		WELTALLCORE_API static TResult<FMesNameTables> Parse(std::vector<uint8> File);

		int32 Count() const { return static_cast<int32>(Offsets.size()); }

		/** String Index of table Table, as the game's inserts read it; empty when there is none. */
		WELTALLCORE_API std::span<const uint8> String(int32 Table, int32 Index) const;

		/** Table Table's bytes, to the file's end; empty when there is none. */
		WELTALLCORE_API std::span<const uint8> Table(int32 Table) const;

		/** The two bytes (lead, code) the glyph map gives a glyph index; the map is table 27. */
		WELTALLCORE_API std::span<const uint8> GlyphMapEntry(int32 Index) const;

	private:
		std::vector<uint8> Data;
		std::vector<uint32> Offsets;
	};
}
