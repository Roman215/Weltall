#include "Field/EventAssembly.h"
#include "Field/FieldMapPatch.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	/** A small map's scripts: actor 0's init and loop, actor 1's talk script with a test and two messages. */
	constexpr std::string_view MapScripts =
		"actors 2\n"
		"actor 0 script 0:\n"
		"    Return\n"
		"actor 0 script 1:\n"
		"    Yield\n"
		"    Jump @0x0001\n"
		"actor 1 script 2:\n"
		"    If var[010] == #1 else Other\n"
		"    Message msg0 0\n"
		"    End\n"
		"Other:\n"
		"    Message msg1 0\n"
		"    End\n";

	struct FMap
	{
		FEventScripts Scripts;
		FBytes Messages;
	};

	FMap Map()
	{
		const auto Section = AssembleEventScripts(MapScripts);
		REQUIRE(Section);
		auto Scripts = FEventScripts::Parse(*Section);
		REQUIRE(Scripts);
		const std::vector<FAddedMessage> Own = {*MesMessageOfText("Hello."), *MesMessageOfText("Go away.")};
		return {std::move(*Scripts), *BuildMessageTable(Own)};
	}

	FBytes CodeOf(const FEventScripts& Scripts)
	{
		return FBytes(Scripts.Code(), Scripts.Code() + Scripts.CodeSize());
	}

	std::string Refused(std::string_view Text)
	{
		const auto Patch = FFieldMapPatch::Parse(Text);
		return Patch ? std::string("read") : Patch.Error().Message;
	}

	/**
	 * A disc whose one battle event, number 3, has two messages: a narration of two pages, and a line; and whose one
	 * field map, number 5, has the same two, the first in a window wider and taller than its text, then one that starts
	 * past the table's bytes.
	 */
	struct FTestDiscText final : IDiscText
	{
		FBytes Table;
		FBytes MapTable;

		FTestDiscText()
		{
			const std::vector<FAddedMessage> Own = {*MesMessageOfText("First.{page}\nSecond,\nlonger."), *MesMessageOfText("Other.")};
			Table = *BuildMessageTable(Own);
			std::vector<FAddedMessage> OnMap = Own;
			OnMap[0].WidthUnits = 40;
			OnMap[0].Lines = 4;
			OnMap.push_back({});
			MapTable = *BuildMessageTable(OnMap);
		}

		std::span<const uint8> BattleEventMessages(int32 Event) const override
		{
			return Event == 3 ? std::span<const uint8>(Table) : std::span<const uint8>();
		}

		std::span<const uint8> FieldMapMessages(int32 Number) const override
		{
			return Number == 5 ? std::span<const uint8>(MapTable) : std::span<const uint8>();
		}
	};

	/** What applying Text to the small map says, with Disc's text to read when it is given: "applied", "left alone", or the error. */
	std::string Applied(std::string_view Text, const IDiscText* Disc = nullptr)
	{
		const auto Patch = FFieldMapPatch::Parse(Text);
		REQUIRE(Patch);
		FMap Target = Map();
		const FBytes Code = CodeOf(Target.Scripts);
		const FBytes Messages = Target.Messages;
		const auto Result = Patch->Apply(Target.Scripts, Target.Messages, Disc);
		if (!Result || !*Result)
		{
			// Nothing of a patch that does not go on whole.
			CHECK(CodeOf(Target.Scripts) == Code);
			CHECK(Target.Messages == Messages);
		}
		return !Result ? Result.Error().Message : *Result ? "applied" : "left alone";
	}
}

TEST_CASE("A map patch adds its scripts and messages after the map's own", "[Field]")
{
	const std::string_view Text =
		"; The second actor greets by the hour.\n"
		"map 12\n"
		"\n"
		"message Evening\n"
		"{\n"
		"Good evening.{wait 14}\n"
		"    Stay a while?\n"
		"}\n"
		"message Counted size 30 2\n"
		"{\n"
		"{digits 00} left.\n"
		"}\n"
		"define Hour var[7F0]\n"
		"unsigned 7F0\n"
		"detour @0x0005 to Greeting If var[010] == #1 else @0x0012\n"
		"expect @0x0000 Return\n"
		"\n"
		"Greeting:\n"
		"    If Hour > #18 else Daytime\n"
		"    Message Evening 0\n"
		"    Message Counted 0\n"
		"    End\n"
		"Daytime:\n"
		"    If var[010] == #1 else @0x0012\n"
		"    Jump @0x000D\n"
		"actor 0 script 1:\n"
		"    Yield\n"
		"    Jump @0x0001\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	if (!Patch)
	{
		FAIL(Patch.Error().Message);
	}
	CHECK(Patch->Map() == 12);
	FMap Target = Map();
	const FBytes Before = CodeOf(Target.Scripts);
	REQUIRE(Before.size() == 0x17);
	const auto Result = Patch->Apply(Target.Scripts, Target.Messages);
	if (!Result)
	{
		FAIL(Result.Error().Message);
	}
	REQUIRE(*Result);

	// The messages follow the map's two, in a window of their text's size or the one given.
	const auto Table = FMessageTable::Parse(Target.Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 4);
	CHECK(MesTextOfCodes(Table->String(0)) == "Hello.");
	CHECK(MesTextOfCodes(Table->String(2)) == "Good evening.{wait 14}\n    Stay a while?");
	CHECK(Table->WidthUnits(2) == 34);
	CHECK(Table->Lines(2) == 2);
	CHECK(MesTextOfCodes(Table->String(3)) == "{digits 00} left.");
	CHECK(Table->WidthUnits(3) == 30);
	CHECK(Table->Lines(3) == 2);

	// The map's code as it was but for the detoured test's first three bytes, now a jump to the added code.
	const FBytes After = CodeOf(Target.Scripts);
	FBytes Detoured = Before;
	Detoured[5] = 0x01;
	Detoured[6] = 0x17;
	Detoured[7] = 0x00;
	REQUIRE(After.size() > Before.size());
	CHECK(FBytes(After.begin(), After.begin() + 0x17) == Detoured);
	const FBytes Added = {
		0x02, 0xF0, 0x07, 0x12, 0x00, 0x42, 0x28, 0x00,  // 0017 If Hour > #18 else Daytime
		0xD2, 0x02, 0x00, 0x00,                          // 001F Message Evening 0
		0xD2, 0x03, 0x00, 0x00,                          // 0023 Message Counted 0
		0x00,                                            // 0027 End
		0x02, 0x10, 0x00, 0x01, 0x00, 0x40, 0x12, 0x00,  // 0028 Daytime: the map's own test
		0x01, 0x0D, 0x00,                                // 0030 Jump @0x000D
		0xC3, 0x01, 0x01, 0x00,                          // 0033 actor 0's new loop
	};
	CHECK(FBytes(After.begin() + 0x17, After.end()) == Added);
	CHECK(Target.Scripts.EntryPoint(0, 1) == 0x33);
	CHECK(Target.Scripts.EntryPoint(0, 0) == 0);
	CHECK(Target.Scripts.EntryPoint(1, 2) == 5);
	CHECK(Target.Scripts.IsVariableUnsigned(0x7F0));
	CHECK_FALSE(Target.Scripts.IsVariableUnsigned(0x7F2));
}

TEST_CASE("A map patch names a message of the disc's own, which is read as the patch is added", "[Field]")
{
	const FTestDiscText Disc;
	const std::string_view Text =
		"map 12\n"
		"message Told from event 3 message 0 page 2   ; the narration's second page\n"
		"message Whole from event 3 message 0\n"
		"message Ours\n"
		"{\n"
		"Mine.\n"
		"}\n"
		"Added:\n"
		"    Message Told 0\n"
		"    Message Whole 0\n"
		"    Message Ours 0\n"
		"    End\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	if (!Patch)
	{
		FAIL(Patch.Error().Message);
	}
	FMap Target = Map();
	const auto Result = Patch->Apply(Target.Scripts, Target.Messages, &Disc);
	if (!Result)
	{
		FAIL(Result.Error().Message);
	}
	REQUIRE(*Result);
	// After the map's two: the page alone in a window of its own size, the whole message, and the text's own.
	const auto Table = FMessageTable::Parse(Target.Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 5);
	CHECK(MesTextOfCodes(Table->String(2)) == "Second,\nlonger.");
	CHECK(Table->WidthUnits(2) == 14);
	CHECK(Table->Lines(2) == 2);
	CHECK(MesTextOfCodes(Table->String(3)) == "First.{page}\nSecond,\nlonger.");
	CHECK(MesTextOfCodes(Table->String(4)) == "Mine.");

	// It cannot be added without a disc's text, and a disc without the event, the message or the page is not its disc.
	CHECK(Applied(Text) == "line 2: a message of the disc's own, and no disc's text to read it from");
	CHECK(Applied("map 12\nmessage Told from event 3 message 1\n", &Disc) == "applied");
	CHECK(Applied("map 12\nmessage Told from event 4 message 0\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from event 3 message 2\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from event 3 message 0 page 3\n", &Disc) == "left alone");

	const std::string_view Shape = "line 2: a message of the disc's own reads 'message Name from event E message K' or 'message Name from map M message K', "
		"or with 'page P' after it";
	CHECK(Refused("map 1\nmessage Told from event 3\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 3 message 0 page 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 3 message 0 pages 1\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 256 message 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from scene 3 message 0\n") == Shape);
	CHECK(Refused("map 1\nmessage 1st from event 3 message 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 3 message 0\nmessage Told\n{\n}\n") == "line 3: a message named twice");

	// A map made from a disc map takes one into its own table the same way.
	const auto Own = FFieldMapPatch::Parse("map 1000 from 7\nmessage Told from event 3 message 0 page 1\nactors 1\nactor 0 script 0:\n    Message Told 0\n    Return\n");
	REQUIRE(Own);
	const auto Built = Own->Build(&Disc);
	REQUIRE(Built);
	const auto OwnTable = FMessageTable::Parse(Built->Messages);
	REQUIRE(OwnTable);
	REQUIRE(OwnTable->Count() == 1);
	CHECK(MesTextOfCodes(OwnTable->String(0)) == "First.");
	CHECK_FALSE(Own->Build());
}

TEST_CASE("A map patch names a field map's message of the disc's own, in the window its table gives it", "[Field]")
{
	const FTestDiscText Disc;
	const std::string_view Text =
		"map 12\n"
		"message Whole from map 5 message 0\n"
		"message Told from map 5 message 0 page 2\n"
		"Added:\n"
		"    Message Whole 0\n"
		"    Message Told 0\n"
		"    End\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	if (!Patch)
	{
		FAIL(Patch.Error().Message);
	}
	FMap Target = Map();
	const auto Result = Patch->Apply(Target.Scripts, Target.Messages, &Disc);
	if (!Result)
	{
		FAIL(Result.Error().Message);
	}
	REQUIRE(*Result);
	// The whole message keeps the window the map's table sizes it to; its page alone has only its own size.
	const auto Table = FMessageTable::Parse(Target.Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 4);
	CHECK(MesTextOfCodes(Table->String(2)) == "First.{page}\nSecond,\nlonger.");
	CHECK(Table->WidthUnits(2) == 40);
	CHECK(Table->Lines(2) == 4);
	CHECK(MesTextOfCodes(Table->String(3)) == "Second,\nlonger.");
	CHECK(Table->WidthUnits(3) == 14);
	CHECK(Table->Lines(3) == 2);

	// Read from the map's table, not the event's of the same number, and only from a disc that has the map and the message.
	CHECK(Applied("map 12\nmessage Told from map 5 message 1\n", &Disc) == "applied");
	CHECK(Applied("map 12\nmessage Told from map 3 message 0\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from event 5 message 0\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from map 5 message 3\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from map 5 message 1 page 2\n", &Disc) == "left alone");
	// A message the table starts past its bytes is one the disc does not hold.
	CHECK(Applied("map 12\nmessage Told from map 5 message 2\n", &Disc) == "left alone");
	CHECK(Applied("map 12\nmessage Told from map 5 message 0\n") == "line 2: a message of the disc's own, and no disc's text to read it from");

	// Only a map of the disc's has messages of the disc's own.
	const std::string_view Shape = "line 2: a message of the disc's own reads 'message Name from event E message K' or 'message Name from map M message K', "
		"or with 'page P' after it";
	CHECK(Refused("map 1\nmessage Told from map 730 message 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from map 5\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from map 5 message 0 page 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from map 729 message 0\n") == "read");

	// A map made from a disc map takes one into its own table the same way.
	const auto Own = FFieldMapPatch::Parse("map 1000 from 7\nmessage Whole from map 5 message 0\nactors 1\nactor 0 script 0:\n    Message Whole 0\n    Return\n");
	REQUIRE(Own);
	const auto Built = Own->Build(&Disc);
	REQUIRE(Built);
	const auto OwnTable = FMessageTable::Parse(Built->Messages);
	REQUIRE(OwnTable);
	REQUIRE(OwnTable->Count() == 1);
	CHECK(MesTextOfCodes(OwnTable->String(0)) == "First.{page}\nSecond,\nlonger.");
	CHECK(OwnTable->WidthUnits(0) == 40);
	CHECK(OwnTable->Lines(0) == 4);
}

TEST_CASE("A map patch names the player's setting it is added under, or none", "[Field]")
{
	const auto Named = FFieldMapPatch::Parse("map 12\noption fix.greeting\nexpect @0x0000 Return\n");
	REQUIRE(Named);
	CHECK(Named->Option() == "fix.greeting");
	const auto Unnamed = FFieldMapPatch::Parse("map 12\n");
	REQUIRE(Unnamed);
	CHECK(Unnamed->Option().empty());
	// The name is the setting's alone: the scripts are the same.
	CHECK(Applied("map 12\noption fix.greeting\nexpect @0x0000 Return\n") == "applied");
}

TEST_CASE("A map whose instructions are not the ones a patch expects is left alone", "[Field]")
{
	CHECK(Applied("map 12\nexpect @0x0000 Return\n") == "applied");
	CHECK(Applied("map 12\nexpect @0x0000 End\n") == "left alone");
	CHECK(Applied("map 12\ndetour @0x0005 to There If var[010] == #2 else @0x0012\nThere:\n End\n") == "left alone");
	// Past the map's bytecode.
	CHECK(Applied("map 12\nexpect @0x0016 Jump @0x0000\n") == "left alone");
	CHECK(Applied("map 12\nexpect @0x0100 End\n") == "left alone");
}

TEST_CASE("A map patch that cannot be read or does not fit is refused with its line", "[Field]")
{
	CHECK(Refused("message A\n{\nHi\n}\n") == "the text names no map");
	CHECK(Refused("map 1\nmap 2\n") == "line 2: 'map' takes the map's number, once");
	CHECK(Refused("map 1\nmessage 1st\n{\n}\n") == "line 2: a message reads 'message Name' or 'message Name size width lines'");
	CHECK(Refused("map 1\nmessage A size 300 2\n{\n}\n") == "line 2: a message reads 'message Name' or 'message Name size width lines'");
	CHECK(Refused("map 1\nmessage A\nHi\n}\n") == "line 3: a message's text begins after a line of '{'");
	CHECK(Refused("map 1\nmessage A\n{\nHi~\n}\n") == "line 4: a character or a mark the messages lack");
	CHECK(Refused("map 1\nmessage A\n{\nHi\n") == "the last message's text does not end with a line of '}'");
	CHECK(Refused("map 1\nmessage A\n{\n}\nmessage A\n{\n}\n") == "line 5: a message named twice");
	// A line of 128 characters is 256 units, one more than a table's byte holds: its window must be given.
	const std::string Wide = "\n{\n" + std::string(128, 'A') + "\n}\n";
	CHECK(Refused("map 1\nmessage Wide" + Wide) == "line 5: the message is wider or taller than a window's size can say: give its size");
	CHECK(Refused("map 1\nmessage Wide size 60 1" + Wide) == "read");
	CHECK(Refused("map 1\ndetour @0x0005 Somewhere End\n") == "line 2: a detour reads 'detour @offset to Label instruction'");
	CHECK(Refused("map 1\nexpect 5 End\n") == "line 2: an expectation reads 'expect @offset instruction'");
	CHECK(Refused("map 1\nexpect @0x0005\n") == "line 2: an expectation reads 'expect @offset instruction'");
	CHECK(Refused("map 1\noption\n") == "line 2: 'option' takes the setting's name, once");
	CHECK(Refused("map 1\noption fix.a fix.b\n") == "line 2: 'option' takes the setting's name, once");
	CHECK(Refused("map 1\noption fix.a\noption fix.b\n") == "line 3: 'option' takes the setting's name, once");

	// What only shows against a map: the scripts' own mistakes, by the text's lines.
	CHECK(Applied("map 12\n\nFly #1\n") == "line 3: no instruction is named 'Fly'");
	CHECK(Applied("map 12\nexpect @0x0000 Retrun\n") == "line 2: no instruction is named 'Retrun'");
	CHECK(Applied("map 12\ndetour @0x0000 to There Return\nThere:\n End\n") == "line 2: a detour takes an instruction of three bytes or more");
	CHECK(Applied("map 12\ndetour @0x0005 to Nowhere If var[010] == #1 else @0x0012\n") == "line 2: no label is named 'Nowhere'");
	CHECK(Applied("map 12\nactor 2 script 0:\n End\n") == "a script of actor 2, which the map does not have");
	CHECK(Applied("map 12\nactors 3\n") == "a patch does not change a map's actor count");
	CHECK(Applied("map 12\nmessage Twice\n{\n}\nTwice:\n End\n") == "line 5: a name defined twice: 'Twice'");
}

TEST_CASE("A map of its own is built from its own scripts and messages alone", "[Field]")
{
	const std::string_view Text =
		"; A room over map 7.\n"
		"map 1000 from 7\n"
		"encounters from 15\n"
		"message Hello\n"
		"{\n"
		"Hello.\n"
		"}\n"
		"actors 1\n"
		"actor 0 script 0:\n"
		"    Message Hello 0\n"
		"    Return\n";
	const auto Own = FFieldMapPatch::Parse(Text);
	REQUIRE(Own);
	CHECK(Own->Map() == 1000);
	CHECK(Own->Base() == 7);
	CHECK(Own->EncountersFrom() == 15);
	const auto Built = Own->Build();
	REQUIRE(Built);
	// The same section as its scripts written alone, the message the first of its own table.
	const auto Alone = AssembleEventScripts("actors 1\nactor 0 script 0:\n    Message msg0 0\n    Return\n");
	REQUIRE(Alone);
	CHECK(Built->EventScripts == *Alone);
	const std::vector<FAddedMessage> Hello = {*MesMessageOfText("Hello.")};
	CHECK(Built->Messages == *BuildMessageTable(Hello));

	// Neither is the other.
	FMap Target = Map();
	const auto Applied = Own->Apply(Target.Scripts, Target.Messages);
	REQUIRE_FALSE(Applied);
	CHECK(Applied.Error().Message == "a map made from a disc map is built, not added to another map");
	const auto Patch = FFieldMapPatch::Parse("map 12\n");
	REQUIRE(Patch);
	CHECK_FALSE(Patch->Base());
	CHECK_FALSE(Patch->Build());
}

TEST_CASE("A map over its base map is a patch of that map's scripts and messages", "[Field]")
{
	const std::string_view Text =
		"; The same room, its second actor with something else to say.\n"
		"map 1000 over 7\n"
		"encounters from 15\n"
		"message Later\n"
		"{\n"
		"Come back later.\n"
		"}\n"
		"detour @0x0005 to Busy If var[010] == #1 else @0x0012\n"
		"Busy:\n"
		"    Message Later 0\n"
		"    End\n";
	const auto Over = FFieldMapPatch::Parse(Text);
	if (!Over)
	{
		FAIL(Over.Error().Message);
	}
	CHECK(Over->Map() == 1000);
	CHECK(Over->Base() == 7);
	CHECK(Over->KeepsBaseScripts());
	CHECK(Over->EncountersFrom() == 15);
	const auto From = FFieldMapPatch::Parse("map 1000 from 7\n");
	REQUIRE(From);
	CHECK_FALSE(From->KeepsBaseScripts());

	// Added to its base map's scripts as a patch of that map is: the map's own code and messages stay, by identity.
	FMap Base = Map();
	const FBytes Before = CodeOf(Base.Scripts);
	const auto Result = Over->Apply(Base.Scripts, Base.Messages);
	if (!Result)
	{
		FAIL(Result.Error().Message);
	}
	REQUIRE(*Result);
	const FBytes After = CodeOf(Base.Scripts);
	FBytes Detoured = Before;
	Detoured[5] = 0x01;
	Detoured[6] = 0x17;
	Detoured[7] = 0x00;
	REQUIRE(After.size() > Before.size());
	CHECK(FBytes(After.begin(), After.begin() + 0x17) == Detoured);
	const FBytes Added = {
		0xD2, 0x02, 0x00, 0x00,  // 0017 Message Later 0
		0x00,                    // 001B End
	};
	CHECK(FBytes(After.begin() + 0x17, After.end()) == Added);
	const auto Table = FMessageTable::Parse(Base.Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 3);
	CHECK(MesTextOfCodes(Table->String(0)) == "Hello.");
	CHECK(MesTextOfCodes(Table->String(2)) == "Come back later.");

	// It has no sections of its own to build, and a base map that is not the one it expects is not its base.
	const auto Built = Over->Build();
	REQUIRE_FALSE(Built);
	CHECK(Built.Error().Message == "a patch is added to its map, not built as one");
	CHECK(Applied("map 1000 over 7\nexpect @0x0000 End\n") == "left alone");
	CHECK(Applied("map 1000 over 7\nactors 3\n") == "a patch does not change a map's actor count");
}

TEST_CASE("A map of its own that cannot be what it says is refused with its line", "[Field]")
{
	const std::string_view Own = "line 1: a map of its own reads 'map N from M' or 'map N over M': N from 730 to 1023, M a map of the disc";
	CHECK(Refused("map 729 from 7\n") == Own);
	CHECK(Refused("map 1024 from 7\n") == Own);
	CHECK(Refused("map 1000 from 730\n") == Own);
	CHECK(Refused("map 729 over 7\n") == Own);
	CHECK(Refused("map 1000 over 730\n") == Own);
	CHECK(Refused("map 1000 onto 7\n") == "line 1: 'map' takes the map's number, once");
	CHECK(Refused("map 1000 from 7\n") == "read");
	CHECK(Refused("map 1000 from 7\nexpect @0x0000 Return\n") == "line 2: a map made from a disc map has none of its instructions to expect or detour");
	CHECK(Refused("map 1000 over 7\noption fix.rps_helper\n") == "line 2: a map of its own is not added under a setting");
	CHECK(Refused("map 1000 from 7\noption fix.rps_helper\n") == "line 2: a map of its own is not added under a setting");
	CHECK(Refused("map 12\nencounters from 3\n") == "line 2: only a map of its own takes another map's encounters");
	CHECK(Refused("map 1000 from 7\nencounters from 730\n") == "line 2: 'encounters from M' takes a map of the disc, once");
	CHECK(Refused("map 1000 from 7\nencounters from 3\nencounters from 4\n") == "line 3: 'encounters from M' takes a map of the disc, once");
	// What only shows as it is built: its scripts' own mistakes, by the text's lines.
	const auto Unbuilt = FFieldMapPatch::Parse("map 1000 from 7\nactors 1\n\nFly #1\n");
	REQUIRE(Unbuilt);
	const auto Built = Unbuilt->Build();
	REQUIRE_FALSE(Built);
	CHECK(Built.Error().Message == "line 4: no instruction is named 'Fly'");
}

TEST_CASE("A disc map given whole is built in place of the disc's scripts and messages, naming the disc's map it was written for", "[Field]")
{
	// The small map as Export Disc 2 writes it, built again: the same section, the same messages.
	const FMap Disc = Map();
	const auto Section = AssembleEventScripts(MapScripts);
	REQUIRE(Section);
	const auto Text = WriteWholeFieldMapText(5, *Section, Disc.Messages);
	REQUIRE(Text);
	CHECK(Text->starts_with("map 5 whole\ndisc "));
	CHECK(ExportMark::LeftAsWritten(*Text));
	const auto Whole = FFieldMapPatch::Parse(*Text);
	REQUIRE(Whole);
	CHECK(Whole->IsWhole());
	CHECK(Whole->Map() == 5);
	CHECK_FALSE(Whole->Base());
	CHECK(Whole->WrittenFor() == FieldMapIdentity(*Section, Disc.Messages));
	const auto Built = Whole->Build();
	REQUIRE(Built);
	CHECK(Built->EventScripts == *Section);
	CHECK(FieldMapIdentity(Built->EventScripts, Built->Messages) == Whole->WrittenFor());

	// Edited, it is built as edited, and still names the disc's map it was written for.
	std::string Edited = *Text;
	Edited.replace(Edited.find("Hello."), 6, "Howdy.");
	CHECK_FALSE(ExportMark::LeftAsWritten(Edited));
	const auto Changed = FFieldMapPatch::Parse(Edited);
	REQUIRE(Changed);
	CHECK(Changed->WrittenFor() == Whole->WrittenFor());
	const auto Rebuilt = Changed->Build();
	REQUIRE(Rebuilt);
	CHECK(Rebuilt->EventScripts == *Section);
	const auto Table = FMessageTable::Parse(Rebuilt->Messages);
	REQUIRE(Table);
	CHECK(MesTextOfCodes(Table->String(0)) == "Howdy.");
	CHECK(MesTextOfCodes(Table->String(1)) == "Go away.");
	CHECK(FieldMapIdentity(Rebuilt->EventScripts, Rebuilt->Messages) != Whole->WrittenFor());

	// It is never added to a map as a patch is.
	FMap Target = Map();
	const auto Applied = Whole->Apply(Target.Scripts, Target.Messages);
	REQUIRE_FALSE(Applied);
	CHECK(Applied.Error().Message == "a whole map is built in place of the disc's, not added to it");
}

TEST_CASE("A whole map that cannot be what it says is refused with its line", "[Field]")
{
	const std::string Disc = "disc " + std::string(40, 'a') + "\n";
	CHECK(Refused("map 730 whole\n" + Disc) == "line 1: a whole map is one of the disc's: 'map N whole', N below 730");
	CHECK(Refused("map 5 whole\nactors 1\n") == "a whole map names the disc's map it was written for: 'disc' and its SHA-1");
	CHECK(Refused("map 5 whole\ndisc 12ab\n") == "line 2: 'disc' takes the SHA-1 of the disc's map, once");
	CHECK(Refused("map 5 whole\n" + Disc + Disc) == "line 3: 'disc' takes the SHA-1 of the disc's map, once");
	CHECK(Refused("map 5\nexported " + std::string(40, 'b') + "\n") == "line 2: only a whole map carries an export's mark");
	CHECK(Refused("map 730 from 5\n" + Disc) == "line 2: only a disc map's patch or whole map names the disc's map it was written for");
	CHECK(Refused("map 730 over 5\n" + Disc) == "line 2: only a disc map's patch or whole map names the disc's map it was written for");
	CHECK(Refused("map 800\n" + Disc) == "line 2: only a disc map's patch or whole map names the disc's map it was written for");
	CHECK(Refused("map 5 whole\n" + Disc + "expect @0x0000 Return\n") == "line 3: a whole map gives every script itself, and expects or detours none of the disc's");
	CHECK(Refused("map 5 whole\n" + Disc + "option fix.rps_helper\n") == "line 3: a whole map is not added under a setting");
	CHECK(Refused("map 5 whole\n" + Disc + "encounters from 15\n") == "line 3: a whole map keeps the disc map's encounters");
}

namespace
{
	/** A small map that begins with two entrances, then its scripts. */
	constexpr std::string_view EntranceScripts =
		"actors 1\n"
		"entrances\n"
		"entrance x 10 z 20 layer 0 camera 2 facing keep\n"
		"entrance x -30 z 40 layer 1 camera keep facing 3\n"
		"actor 0 script 0:\n"
		"    Return\n";

	FEventScripts ScriptsOf(std::string_view Text)
	{
		const auto Section = AssembleEventScripts(Text);
		REQUIRE(Section);
		auto Scripts = FEventScripts::Parse(*Section);
		REQUIRE(Scripts);
		return std::move(*Scripts);
	}

	std::string DiscLine(const FSha1::FDigest& Identity)
	{
		return "disc " + FSha1::ToHex(Identity) + "\n";
	}
}

TEST_CASE("A patch that names the disc's map it was written for goes on that map alone", "[Field]")
{
	const FMap Disc = Map();
	const auto Section = AssembleEventScripts(MapScripts);
	REQUIRE(Section);
	const auto Identity = FieldMapIdentity(*Section, Disc.Messages);
	REQUIRE(Identity);
	const std::string Text = "map 12\n" + DiscLine(*Identity) + "message Extra\n{\nOne more.\n}\nactor 1 script 3:\n    Message Extra 0\n    End\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	REQUIRE(Patch);
	CHECK(Patch->WrittenFor() == Identity);

	FMap Target = Map();
	auto Result = Patch->Apply(Target.Scripts, Target.Messages, nullptr, &*Identity);
	REQUIRE(Result);
	CHECK(*Result);
	CHECK(FMessageTable::Parse(Target.Messages)->Count() == 3);

	// Another map's identity, or none to check against, leaves the map as it is.
	FSha1::FDigest Other = *Identity;
	Other[0] ^= 1;
	FMap Elsewhere = Map();
	const FBytes Code = CodeOf(Elsewhere.Scripts);
	Result = Patch->Apply(Elsewhere.Scripts, Elsewhere.Messages, nullptr, &Other);
	REQUIRE(Result);
	CHECK_FALSE(*Result);
	Result = Patch->Apply(Elsewhere.Scripts, Elsewhere.Messages);
	REQUIRE(Result);
	CHECK_FALSE(*Result);
	CHECK(CodeOf(Elsewhere.Scripts) == Code);
	CHECK(Elsewhere.Messages == Disc.Messages);
}

TEST_CASE("A patch gives one of the map's own messages anew in its place, the others following the map's", "[Field]")
{
	const FTestDiscText Disc;
	const std::string_view Text =
		"map 12\n"
		"message msg1 anew size 20 3\n"
		"{\n"
		"Leave me be.\n"
		"}\n"
		"message Extra\n"
		"{\n"
		"One more.\n"
		"}\n"
		"message msg0 anew from map 5 message 1\n"
		"actor 0 script 2:\n"
		"    Message Extra 0\n"
		"    Message msg1 0\n"
		"    End\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	if (!Patch)
	{
		FAIL(Patch.Error().Message);
	}
	REQUIRE(Patch->NamedMessages().size() == 3);
	CHECK(Patch->NamedMessages()[0].Anew == 1);
	CHECK_FALSE(Patch->NamedMessages()[1].Anew);

	FMap Target = Map();
	const FBytes Before = CodeOf(Target.Scripts);
	const auto Result = Patch->Apply(Target.Scripts, Target.Messages, &Disc);
	if (!Result)
	{
		FAIL(Result.Error().Message);
	}
	REQUIRE(*Result);
	const auto Table = FMessageTable::Parse(Target.Messages);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 3);
	CHECK(MesTextOfCodes(Table->String(0)) == "Other.");
	CHECK(MesTextOfCodes(Table->String(1)) == "Leave me be.");
	CHECK(Table->WidthUnits(1) == 20);
	CHECK(Table->Lines(1) == 3);
	CHECK(MesTextOfCodes(Table->String(2)) == "One more.");

	// The map's own scripts name the messages as before; the added message is numbered after the map's own.
	const FBytes After = CodeOf(Target.Scripts);
	CHECK(FBytes(After.begin(), After.begin() + static_cast<std::ptrdiff_t>(Before.size())) == Before);
	const FBytes Added = {
		0xD2, 0x02, 0x00, 0x00,  // Message Extra 0
		0xD2, 0x01, 0x00, 0x00,  // Message msg1 0
		0x00,                    // End
	};
	CHECK(FBytes(After.begin() + static_cast<std::ptrdiff_t>(Before.size()), After.end()) == Added);
}

TEST_CASE("A message given anew that the map does not have leaves the map as it is", "[Field]")
{
	CHECK(Applied("map 12\nmessage msg2 anew\n{\nNo such one.\n}\n") == "left alone");
	CHECK(Applied("map 12\nmessage msg1 anew\n{\nThere is.\n}\n") == "applied");
}

TEST_CASE("A patch that names the disc's map gives its entrances anew, within the map's table", "[Field]")
{
	const FEventScripts Disc = ScriptsOf(EntranceScripts);
	const FSha1::FDigest Any{};
	const std::string Text = "map 12\n" + DiscLine(Any) + "entrance 1 x 50 z -60 layer 2 camera 4 facing 5\n";
	const auto Patch = FFieldMapPatch::Parse(Text);
	if (!Patch)
	{
		FAIL(Patch.Error().Message);
	}
	FEventScripts Target = Disc;
	FBytes Messages = *BuildMessageTable({});
	auto Result = Patch->Apply(Target, Messages, nullptr, &Any);
	REQUIRE(Result);
	REQUIRE(*Result);
	const auto Given = Target.Entrance(1);
	REQUIRE(Given);
	CHECK(Given->X == 50);
	CHECK(Given->Z == -60);
	CHECK(Given->Layer == 2);
	CHECK(Given->CameraDirection == 4);
	CHECK(Given->Facing == 5);
	CHECK(Target.Entrance(0)->X == 10);
	CHECK(FBytes(Target.Code() + 15, Target.Code() + Target.CodeSize()) == FBytes(Disc.Code() + 15, Disc.Code() + Disc.CodeSize()));

	// An entrance past the table would be the code's bytes: the map is left as it is.
	const auto Past = FFieldMapPatch::Parse("map 12\n" + DiscLine(Any) + "entrance 2 x 50 z -60 layer 2 camera 4 facing 5\n");
	REQUIRE(Past);
	FEventScripts Untouched = Disc;
	Result = Past->Apply(Untouched, Messages, nullptr, &Any);
	REQUIRE(Result);
	CHECK_FALSE(*Result);
	CHECK(FBytes(Untouched.Code(), Untouched.Code() + Untouched.CodeSize()) == FBytes(Disc.Code(), Disc.Code() + Disc.CodeSize()));
}

TEST_CASE("Messages and entrances given anew that cannot be what they say are refused with their line", "[Field]")
{
	const std::string Disc = DiscLine(FSha1::FDigest{});
	CHECK(Refused("map 12\nmessage Hello anew\n{\nHi.\n}\n") == "line 2: a message given anew is named by its number in the map: 'message msgK anew'");
	CHECK(Refused("map 12\nmessage msg1 anew\n{\nHi.\n}\nmessage msg1 anew from map 5 message 0\n") == "line 6: a message named twice");
	CHECK(Refused("map 12\nmessage msg1 anew size 20\n") == "line 2: a message reads 'message Name' or 'message Name size width lines'");
	CHECK(Refused("map 730 from 5\nmessage msg1 anew\n{\nHi.\n}\n") == "line 2: a map built whole numbers its messages in their order: none is given anew");
	CHECK(Refused("map 5 whole\n" + Disc + "message msg1 anew\n{\nHi.\n}\n") == "line 3: a map built whole numbers its messages in their order: none is given anew");
	CHECK(Refused("map 730 over 5\nmessage msg1 anew\n{\nHi.\n}\n") == "read");
	CHECK(Refused("map 12\nentrance 1 x 50 z -60 layer 2 camera 4 facing 5\n")
		== "line 2: an entrance is given anew only by a patch that names the disc's map it was written for ('disc')");
	CHECK(Refused("map 5 whole\n" + Disc + "entrance 1 x 50 z -60 layer 2 camera 4 facing 5\n")
		== "line 3: an entrance is given anew only by a patch that names the disc's map it was written for ('disc')");
	CHECK(Refused("map 12\n" + Disc + "entrance 1 x 50 z -60 layer 2 camera 4 facing 5\nentrance 1 x 0 z 0 layer 0 camera 0 facing 0\n")
		== "line 4: an entrance given anew twice");
	CHECK(Refused("map 12\n" + Disc + "entrance 1 x 50 z -60 layer 2 camera 4\n") == "line 3: an entrance given anew reads 'entrance K x N z N layer N camera N facing N'");
	CHECK(Refused("map 12\n" + Disc + "entrance 1 x 99999 z -60 layer 2 camera 4 facing 5\n") == "line 3: an entrance's number is out of range");
}
