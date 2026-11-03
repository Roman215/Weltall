#pragma once

#include "Foundation/Sha1.h"
#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Weltall
{
	/**
	 * What a document that gives a disc's map or battle event text whole was written for, so that it never stands in for
	 * another disc's: the SHA-1 of the scripts and the messages as the game reads them, each message its codes to its
	 * zero byte and its window, so that a table built again from its text is the same table. None where the messages
	 * are not a message table.
	 */
	WELTALLCORE_API std::optional<FSha1::FDigest> FieldMapIdentity(std::span<const uint8> EventScripts, std::span<const uint8> Messages);
	WELTALLCORE_API std::optional<FSha1::FDigest> MessageTableIdentity(std::span<const uint8> Messages);

	/**
	 * A document Export Disc 2 writes carries the SHA-1 of its own text on a line "exported H": H is the text's without
	 * that line, its carriage returns and a byte order mark left out. Left as it was written, the document is the disc's
	 * own and changes nothing.
	 */
	namespace ExportMark
	{
		/** The text Head then Body with the mark between them. */
		WELTALLCORE_API std::string Marked(std::string_view Head, std::string_view Body);

		/** Whether Text carries the mark and is the text it was written as. */
		WELTALLCORE_API bool LeftAsWritten(std::string_view Text);

		/** Whether a document's line is the mark, which its reader passes over. */
		WELTALLCORE_API bool IsMark(std::string_view Line);
	}
}
