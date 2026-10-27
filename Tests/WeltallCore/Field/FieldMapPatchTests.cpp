#include "Field/EventAssembly.h"
#include "Field/FieldMapPatch.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"

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

	/** A disc whose one battle event, number 3, has two messages: a narration of two pages, and a line. */
	struct FTestDiscText final : IDiscText
	{
		FBytes Table;

		FTestDiscText()
		{
			const std::vector<FAddedMessage> Own = {*MesMessageOfText("First.{page}\nSecond,\nlonger."), *MesMessageOfText("Other.")};
			Table = *BuildMessageTable(Own);
		}

		std::span<const uint8> BattleEventMessages(int32 Event) const override
		{
			return Event == 3 ? std::span<const uint8>(Table) : std::span<const uint8>();
		}

		std::span<const uint8> FieldMapMessages(int32 Map) const override
		{
			return Map == 5 ? std::span<const uint8>(Table) : std::span<const uint8>();
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
	CHECK(Applied("map 12\nmessage Told from map 5 message 1\n", &Disc) == "applied");
	CHECK(Applied("map 12\nmessage Told from map 5 message 0 page 2\n", &Disc) == "applied");
	CHECK(Applied("map 12\nmessage Told from map 6 message 0\n", &Disc) == "left alone");

	const std::string_view Shape = "line 2: a message of the disc's own reads 'message Name from event E message K' or 'message Name from map M message K', or with 'page P' after it";
	CHECK(Refused("map 1\nmessage Told from event 3\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 3 message 0 page 0\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from event 3 message 0 pages 1\n") == Shape);
	CHECK(Refused("map 1\nmessage Told from map 3 msg 0\n") == Shape);
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
