#pragma once

#include "Foundation/Result.h"
#include "Foundation/Sha1.h"
#include "Foundation/Types.h"
#include "Text/MessageTable.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * What the port's story changes in a battle event's text, written as text:
	 *
	 *   battle event 19                 the event's script, by the number a formation names
	 *   message 0 from page 2           its message 0 begins at its second page
	 *   message 2 from page 1 after     or after pages of the text's own, told first (MesText.h's text,
	 *   {                               between a line of '{' and a line of '}'), in the window the
	 *   He had caught up.               message was written for
	 *   }
	 *
	 * The disc's words stay the disc's: the text says where a message of it begins, and carries only what it tells
	 * before that. A ';' begins a comment. A text that names the disc's table it was written for may also give a
	 * message anew, in its window (Make shareable writes such texts):
	 *
	 *   battle event 19
	 *   disc 5b1e...                    the disc's table (DiscIdentity.h): on any other the text is left out
	 *   message 3 anew size 48 4        message 3 given anew
	 *   { ... }
	 *
	 * Or the event's table given whole, which Export Disc 2 writes:
	 *
	 *   battle event 19 whole           the event whose table it stands for
	 *   disc 5b1e...                    the disc's table it was written for (DiscIdentity.h)
	 *   exported 0c4d...                the export's mark: left as written, the text changes nothing
	 *   message 0 size 48 4             every message, numbered from 0 in their order, with its window
	 *   { ... }
	 */
	class FBattleEventTextPatch
	{
	public:
		/** Reads the text; the error names the line. */
		WELTALLCORE_API static TResult<FBattleEventTextPatch> Parse(std::string_view Text);

		int32 Event() const { return EventNumber; }
		/** Whether the text gives the event's table whole (Replace), not where its messages begin (Apply). */
		bool IsWhole() const { return bWhole; }
		/** For a whole table, or a text that names it, the identity (MessageTableIdentity) of the disc's table it was written for. */
		const std::optional<FSha1::FDigest>& WrittenFor() const { return WrittenForTable; }
		/** A whole table's messages in their order; none for any other text. */
		const std::vector<FAddedMessage>& WholeTable() const { return WholeMessages; }

		/**
		 * The event's message table with the changes made; none where it lacks a message or a page the text names,
		 * where a message's window is smaller than what is told before it, or where Identity, the disc's table's (before
		 * the player's messages), is not the one the text names: another table than the one it was written for, which
		 * is left as it is. None for a whole table.
		 */
		WELTALLCORE_API std::optional<std::vector<uint8>> Apply(std::span<const uint8> Messages, const FSha1::FDigest* Identity = nullptr) const;

		/**
		 * A whole table's messages as a table, in place of DiscMessages, the disc's own (before the player's messages);
		 * none where DiscMessages are not the table the text was written for, or the text gives no whole table.
		 */
		WELTALLCORE_API std::optional<std::vector<uint8>> Replace(std::span<const uint8> DiscMessages) const;

	private:
		/** A message that begins at a later page of its own, after the text's own pages when it has any. */
		struct FBeginning
		{
			int32 Message = 0;
			int32 Page = 1;
			/** The codes told first, ended by 00; empty without. */
			std::vector<uint8> Before;
		};

		/** A message given anew, in the window given. */
		struct FAnew
		{
			int32 Message = 0;
			FAddedMessage Given;
		};

		int32 EventNumber = 0;
		std::vector<FBeginning> Beginnings;
		std::vector<FAnew> AnewMessages;
		bool bWhole = false;
		std::optional<FSha1::FDigest> WrittenForTable;
		std::vector<FAddedMessage> WholeMessages;
	};

	/**
	 * A battle event's message table written as the text that gives it whole ("battle event Event whole"), with the
	 * export's mark: read and replaced, it gives the same messages back.
	 */
	WELTALLCORE_API TResult<std::string> WriteWholeBattleEventText(int32 Event, std::span<const uint8> Messages);

	/**
	 * A battle event's table given whole, as an author edits Export Disc 2's, written as the text that changes the disc's
	 * table as it does and carries only what the author changed (Make shareable): "battle event N" naming the disc's
	 * table, then each message the author changed, begun at a later page of the disc's own where it is so (after pages
	 * of the author's, where they fit its window), else given anew. Checked to give the whole table's messages before it
	 * is given back; empty where the table is the disc's. An error where DiscMessages, the disc's table before the
	 * player's messages, is not the one the whole table was written for, or where the count of its messages differs.
	 */
	WELTALLCORE_API TResult<std::string> WriteShareableBattleEventText(const FBattleEventTextPatch& Whole, std::span<const uint8> DiscMessages);
}
