#include "Battle/BattleEvent.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Story/BattleEventTextPatch.h"
#include "Story/DiscIdentity.h"
#include "Support/GameDiscs.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <limits>
#include <string>

using namespace Weltall;

// Make shareable writes an edited whole battle event table as the text that changes the disc's table as it does:
// every table of both discs, as exported and as a test edits it.

namespace
{
	constexpr std::string_view Edit = "Edited by a test.";

	/** Each battle event's table of Disc, as exported, then with its first message in the test's words and its last message's first page left out. */
	void CheckEveryTable(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Events = FBattleEventArchive::Load(Disc.Image(), *Index);
		REQUIRE(Events);
		int32 Tables = 0;
		int32 Begun = 0;
		for (int32 Event = 0; Event < Events->Count(); ++Event)
		{
			const std::span<const uint8> Messages = Events->DiscMessages(Event);
			const auto Table = FMessageTable::Parse(Messages);
			if (!Table || Table->Count() == 0)
			{
				continue;
			}
			INFO("event " << Event);
			++Tables;
			const auto Exported = WriteWholeBattleEventText(Event, Messages);
			REQUIRE(Exported);
			const auto Unedited = FBattleEventTextPatch::Parse(*Exported);
			REQUIRE(Unedited);
			const auto Nothing = WriteShareableBattleEventText(*Unedited, Messages);
			if (!Nothing)
			{
				FAIL(Nothing.Error().Message);
			}
			CHECK(Nothing->empty());

			std::string Edited = *Exported;
			const std::size_t First = Edited.find("{\n", Edited.find("\nmessage 0 size")) + 2;
			Edited.replace(First, Edited.find("\n}\n", First) - First, Edit);
			// The last message, where it has a second page, from that page on.
			const int32 Last = Table->Count() - 1;
			const bool bPages = Last > 0 && MesPagesOfCodes(Table->String(Last), 2, std::numeric_limits<int32>::max()).has_value();
			if (bPages)
			{
				const std::size_t Begin = Edited.find("{\n", Edited.find("\nmessage " + std::to_string(Last) + " size")) + 2;
				const std::size_t End = Edited.find("\n}\n", Begin);
				const std::string Rest = MesTextOfCodes(*MesPagesOfCodes(Table->String(Last), 2, std::numeric_limits<int32>::max()));
				Edited.replace(Begin, End - Begin, Rest);
			}
			const auto Whole = FBattleEventTextPatch::Parse(Edited);
			REQUIRE(Whole);
			const auto Text = WriteShareableBattleEventText(*Whole, Messages);
			if (!Text)
			{
				FAIL(Text.Error().Message);
			}
			CHECK(Text->starts_with("battle event " + std::to_string(Event) + "\ndisc " + FSha1::ToHex(*MessageTableIdentity(Messages)) + "\n"));
			CHECK(Text->find("message 0 anew size ") != std::string::npos);
			CHECK(Text->find(Edit) != std::string::npos);
			CHECK(std::count(Text->begin(), Text->end(), '{') == 1);
			if (bPages && Text->find("message " + std::to_string(Last) + " from page 2\n") != std::string::npos)
			{
				++Begun;
			}
		}
		CHECK(Tables > 0);
		CHECK(Begun > 0);
	}
}

TEST_CASE("Every battle event table of the second disc, edited, is shared as the changes it makes", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	CheckEveryTable(*Disc);
}

TEST_CASE("Every battle event table of the first disc, edited, is shared as the changes it makes", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryTable(*Disc);
}
