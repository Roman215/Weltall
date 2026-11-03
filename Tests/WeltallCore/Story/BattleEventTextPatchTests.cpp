#include "Story/BattleEventTextPatch.h"
#include "Story/DiscIdentity.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	/** An event's two messages: a narration of two pages, and a line. */
	FBytes Table()
	{
		const std::vector<FAddedMessage> Own = {*MesMessageOfText("Met him again.{page}\nHe stood\nin my way."), *MesMessageOfText("No choice.")};
		return *BuildMessageTable(Own);
	}

	FBattleEventTextPatch Patch(std::string_view Text)
	{
		const auto Read = FBattleEventTextPatch::Parse(Text);
		REQUIRE(Read);
		return *Read;
	}

	std::string Refused(std::string_view Text)
	{
		const auto Read = FBattleEventTextPatch::Parse(Text);
		return Read ? std::string("read") : Read.Error().Message;
	}

	/** Message Index of a table made by a patch, as text. */
	std::string Told(const std::optional<FBytes>& Changed, int32 Index)
	{
		REQUIRE(Changed);
		const auto Read = FMessageTable::Parse(*Changed);
		REQUIRE(Read);
		return MesTextOfCodes(Read->String(Index));
	}
}

TEST_CASE("A battle event's text begins a message at a later page of its own", "[Story]")
{
	const FBytes Disc = Table();
	const auto Whole = FMessageTable::Parse(Disc);
	REQUIRE(Whole);
	const FBattleEventTextPatch Later = Patch("; The first page is told elsewhere.\n\nbattle event 19\nmessage 0 from page 2\n");
	CHECK(Later.Event() == 19);
	const auto Changed = Later.Apply(Disc);
	REQUIRE(Changed);
	const auto Read = FMessageTable::Parse(*Changed);
	REQUIRE(Read);
	REQUIRE(Read->Count() == 2);
	// The words are the table's own from the page on, in the window the message was written for; the rest is as it was.
	CHECK(MesTextOfCodes(Read->String(0)) == "He stood\nin my way.");
	CHECK(Read->WidthUnits(0) == Whole->WidthUnits(0));
	CHECK(Read->Lines(0) == Whole->Lines(0));
	CHECK(MesTextOfCodes(Read->String(1)) == "No choice.");
	CHECK(Read->WidthUnits(1) == Whole->WidthUnits(1));

	// From its first page a message is whole.
	CHECK(Told(Patch("battle event 19\nmessage 0 from page 1\n").Apply(Disc), 0) == "Met him again.{page}\nHe stood\nin my way.");

	// A table without the page or the message is another than the one written for: nothing is made of it.
	CHECK_FALSE(Patch("battle event 19\nmessage 0 from page 3\n").Apply(Disc));
	CHECK_FALSE(Patch("battle event 19\nmessage 2 from page 1\n").Apply(Disc));
	CHECK_FALSE(Later.Apply(FBytes{}));
}

TEST_CASE("A battle event's text tells pages of its own before a message's", "[Story]")
{
	const FBytes Disc = Table();
	const std::string_view Text =
		"battle event 19\n"
		"message 0 from page 2 after\n"
		"{\n"
		"He caught up.{wait 1E}\n"
		"   We fell.\n"
		"}\n"
		"message 1 from page 1 after\n"
		"{\n"
		"One.{page}Two.\n"
		"}\n";
	const auto Changed = Patch(Text).Apply(Disc);
	// Its own pages, then the message's from the page named, each on a new page as the game's follow one another.
	CHECK(Told(Changed, 0) == "He caught up.{wait 1E}\n   We fell.{page}\nHe stood\nin my way.");
	CHECK(Told(Changed, 1) == "One.{page}Two.{page}\nNo choice.");

	// What is told first is typed in the message's own window: wider or taller than it, the table is not the one written for.
	CHECK_FALSE(Patch("battle event 19\nmessage 1 from page 1 after\n{\nA line too wide for it.\n}\n").Apply(Disc));
	CHECK_FALSE(Patch("battle event 19\nmessage 1 from page 1 after\n{\nTwo\nlines.\n}\n").Apply(Disc));
	CHECK(Patch("battle event 19\nmessage 1 from page 1 after\n{\nShort.\n}\n").Apply(Disc));
}

TEST_CASE("A battle event's text that cannot be read is refused with its line", "[Story]")
{
	CHECK(Refused("message 0 from page 2\n") == "the text names no battle event");
	CHECK(Refused("battle event\n") == "line 1: 'battle event N' takes the event's number, once");
	CHECK(Refused("battle event 256\n") == "line 1: 'battle event N' takes the event's number, once");
	CHECK(Refused("battle event 1\nbattle event 2\n") == "line 2: 'battle event N' takes the event's number, once");
	const std::string_view Beginning =
		"line 2: a message's beginning reads 'message K from page P', the pages counted from 1, or with 'after' and the text told first";
	CHECK(Refused("battle event 1\nmessage 0\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 0\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 at page 2\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 2 before\n") == Beginning);
	CHECK(Refused("battle event 1\nmessage 0 from page 2\nmessage 0 from page 3\n") == "line 3: a message given two beginnings, or given anew too");
	CHECK(Refused("battle event 1\nactor 0 script 0:\n") == "line 2: a battle event's text takes 'battle event N' and its messages' beginnings");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\nHe caught up.\n}\n") == "line 3: the text told first begins after a line of '{'");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\n{\nHe caught up~\n}\n") == "line 4: a character or a mark the messages lack");
	CHECK(Refused("battle event 1\nmessage 0 from page 2 after\n{\nHe caught up.\n") == "the text told first does not end with a line of '}'");
	CHECK(Refused("battle event 1\n; nothing changed\n") == "read");
}

TEST_CASE("A battle event's table given whole stands for the disc's table it was written for, and no other", "[Story]")
{
	const FBytes Disc = Table();
	const auto Text = WriteWholeBattleEventText(19, Disc);
	REQUIRE(Text);
	CHECK(Text->starts_with("battle event 19 whole\ndisc "));
	CHECK(ExportMark::LeftAsWritten(*Text));
	const FBattleEventTextPatch Whole = Patch(*Text);
	CHECK(Whole.IsWhole());
	CHECK(Whole.Event() == 19);
	CHECK(Whole.WrittenFor() == MessageTableIdentity(Disc));
	const auto Same = Whole.Replace(Disc);
	REQUIRE(Same);
	CHECK(MessageTableIdentity(*Same) == MessageTableIdentity(Disc));
	CHECK_FALSE(Whole.Apply(Disc));

	// Edited, its messages stand for the disc's, each in the window the text gives it.
	std::string Edited = *Text;
	Edited.replace(Edited.find("No choice."), 10, "Not again.");
	CHECK_FALSE(ExportMark::LeftAsWritten(Edited));
	const auto Changed = Patch(Edited).Replace(Disc);
	CHECK(Told(Changed, 0) == Told(Disc, 0));
	CHECK(Told(Changed, 1) == "Not again.");
	const auto Read = FMessageTable::Parse(*Changed);
	REQUIRE(Read);
	const auto Own = FMessageTable::Parse(Disc);
	REQUIRE(Own);
	CHECK(Read->WidthUnits(1) == Own->WidthUnits(1));
	CHECK(Read->Lines(1) == Own->Lines(1));

	// Another disc's table is left as it is.
	const std::vector<FAddedMessage> Else = {*MesMessageOfText("Met him again.")};
	CHECK_FALSE(Patch(Edited).Replace(*BuildMessageTable(Else)));
}

TEST_CASE("A whole battle event table that cannot be what it says is refused with its line", "[Story]")
{
	const std::string Disc = "disc " + std::string(40, 'c') + "\n";
	CHECK(Refused("battle event 19 whole\nmessage 0 size 10 1\n{\nA.\n}\n") == "a whole table names the disc's table it was written for: 'disc' and its SHA-1");
	CHECK(Refused("battle event 19\nexported " + std::string(40, 'b') + "\n") == "line 2: only a whole table carries an export's mark");
	CHECK(Refused("battle event 19 whole\n" + Disc + "message 1 size 10 1\n{\nA.\n}\n")
		== "line 3: a whole table's messages read 'message K size width lines', numbered from 0 in their order");
	CHECK(Refused("battle event 19\nmessage 0 size 10 1\n") == "line 2: a whole table's messages read 'message K size width lines', numbered from 0 in their order");
	CHECK(Refused("battle event 19 whole\n" + Disc + "message 0 from page 2\n")
		== "a whole table gives every message itself, and begins none of the disc's at a later page");
	CHECK(Refused("battle event 19 whole\n" + Disc + "message 0 size 10 1\n{\nA.\n") == "the last message's text does not end with a line of '}'");
}

TEST_CASE("A battle event's text gives a message anew in a window of its own, the others as they are", "[Story]")
{
	const FBytes Disc = Table();
	const FBattleEventTextPatch Anew = Patch("battle event 19\nmessage 1 anew size 30 2\n{\nNot this time.\n}\nmessage 0 from page 2\n");
	const auto Changed = Anew.Apply(Disc);
	CHECK(Told(Changed, 0) == "He stood\nin my way.");
	CHECK(Told(Changed, 1) == "Not this time.");
	const auto Read = FMessageTable::Parse(*Changed);
	REQUIRE(Read);
	CHECK(Read->Count() == 2);
	CHECK(Read->WidthUnits(1) == 30);
	CHECK(Read->Lines(1) == 2);

	// A table without the message is another table than the one it was written for.
	CHECK_FALSE(Patch("battle event 19\nmessage 2 anew size 30 2\n{\nNot this time.\n}\n").Apply(Disc));
}

TEST_CASE("A battle event's text that names the disc's table it was written for changes that table alone", "[Story]")
{
	const FBytes Disc = Table();
	const auto Identity = MessageTableIdentity(Disc);
	REQUIRE(Identity);
	const FBattleEventTextPatch Named = Patch("battle event 19\ndisc " + FSha1::ToHex(*Identity) + "\nmessage 0 from page 2\n");
	CHECK(Named.WrittenFor() == Identity);
	CHECK(Told(Named.Apply(Disc, &*Identity), 0) == "He stood\nin my way.");
	FSha1::FDigest Other = *Identity;
	Other[3] ^= 0x10;
	CHECK_FALSE(Named.Apply(Disc, &Other));
	CHECK_FALSE(Named.Apply(Disc));
}

TEST_CASE("A message given anew that cannot be what it says is refused with its line", "[Story]")
{
	const std::string Disc = "disc " + std::string(40, 'a') + "\n";
	const std::string Shape = "a message given anew reads 'message K anew size width lines', in a text that does not give the table whole";
	CHECK(Refused("battle event 19 whole\n" + Disc + "message 0 anew size 10 1\n{\nA.\n}\n") == "line 3: " + Shape);
	CHECK(Refused("battle event 19\nmessage 0 anew size 300 1\n{\nA.\n}\n") == "line 2: " + Shape);
	CHECK(Refused("battle event 19\nmessage 0 anew size 10 1\n{\nA.\n}\nmessage 0 anew size 10 1\n{\nB.\n}\n")
		== "line 6: a message given anew twice, or begun too");
	CHECK(Refused("battle event 19\nmessage 0 from page 2\nmessage 0 anew size 10 1\n{\nB.\n}\n") == "line 3: a message given anew twice, or begun too");
	CHECK(Refused("battle event 19\nmessage 0 anew size 10 1\n{\nB.\n}\nmessage 0 from page 2\n") == "line 6: a message given two beginnings, or given anew too");
	CHECK(Refused("battle event 19\nmessage 0 anew size 10 1\nB.\n}\n") == "line 3: a message's text begins after a line of '{'");
	CHECK(Refused("battle event 19\n" + Disc + "message 0 anew size 10 1\n{\nA.\n}\n") == "read");
}

namespace
{
	/** The text Make shareable writes for the event's table given whole, edited by Edit. */
	TResult<std::string> Shared(const FBytes& Disc, const std::function<void(std::string&)>& Edit)
	{
		auto Whole = WriteWholeBattleEventText(19, Disc);
		REQUIRE(Whole);
		Edit(*Whole);
		const auto Read = FBattleEventTextPatch::Parse(*Whole);
		if (!Read)
		{
			FAIL(Read.Error().Message);
		}
		return WriteShareableBattleEventText(*Read, Disc);
	}

	void Replace(std::string& Text, std::string_view Was, std::string_view Now)
	{
		const std::size_t At = Text.find(Was);
		REQUIRE(At != std::string::npos);
		Text.replace(At, Was.size(), Now);
	}
}

TEST_CASE("A whole battle event table is shared as the changes it makes to the disc's", "[Story]")
{
	const FBytes Disc = Table();
	CHECK(*Shared(Disc, [](std::string&) {}) == "");

	// The first page left out is the message begun at its second; another message's words are given anew.
	const auto Text = Shared(Disc, [](std::string& Whole)
		{
			Replace(Whole, "Met him again.{page}\n", "");
			Replace(Whole, "No choice.", "One choice.");
		});
	if (!Text)
	{
		FAIL(Text.Error().Message);
	}
	const std::string Head = "battle event 19\ndisc " + FSha1::ToHex(*MessageTableIdentity(Disc)) + "\n";
	CHECK(*Text == Head + "message 0 from page 2\nmessage 1 anew size " + std::to_string(FMessageTable::Parse(Disc)->WidthUnits(1)) + " "
		+ std::to_string(FMessageTable::Parse(Disc)->Lines(1)) + "\n{\nOne choice.\n}\n");

	// A page of the author's told before the disc's own.
	const auto Told = Shared(Disc, [](std::string& Whole) { Replace(Whole, "Met him again.", "Not again."); });
	REQUIRE(Told);
	CHECK(*Told == Head + "message 0 from page 2 after\n{\nNot again.\n}\n");
	CHECK(Told->find("He stood") == std::string::npos);
}

TEST_CASE("A whole battle event table that cannot be shared is refused with the reason", "[Story]")
{
	const FBytes Disc = Table();
	const auto Other = Shared(Disc, [](std::string& Whole) { Whole[Whole.find("disc ") + 5] = Whole[Whole.find("disc ") + 5] == 'a' ? 'b' : 'a'; });
	REQUIRE_FALSE(Other);
	CHECK(Other.Error().Message == "it was written for another disc's table than this disc's");
	const auto Fewer = Shared(Disc, [](std::string& Whole) { Whole.erase(Whole.find("\nmessage 1 ")); Whole += "\n"; });
	REQUIRE_FALSE(Fewer);
	CHECK(Fewer.Error().Message == "it has 1 messages where the disc's table has 2: the battle's scripts name the disc's");
}
