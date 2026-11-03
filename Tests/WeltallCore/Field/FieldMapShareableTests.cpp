#include "Field/EventAssembly.h"
#include "Field/FieldMapPatch.h"
#include "Field/FieldMapShareable.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	/** A small disc map: an entrance, a talk of three messages, a test that chooses between two, an unused script. */
	constexpr std::string_view DiscScripts =
		"actors 3\n"
		"unsigned 7F0\n"
		"entrances\n"
		"entrance x 10 z 20 layer 0 camera 2 facing keep\n"
		"actor 0 script 0:\n"
		"    Return\n"
		"actor 1 script 2:\n"
		"    Message msg0 0\n"
		"    Message msg1 0\n"
		"    Message msg2 0\n"
		"    End\n"
		"actor 2 script 2:\n"
		"    If var[010] == #1 else Other\n"
		"    Message msg1 0\n"
		"    End\n"
		"Other:\n"
		"    Message msg2 0\n"
		"    End\n";

	FBytes Assembled(std::string_view Scripts)
	{
		const auto Section = AssembleEventScripts(Scripts);
		if (!Section)
		{
			FAIL(Section.Error().Message);
		}
		return *Section;
	}

	FBytes DiscMessages()
	{
		const std::vector<FAddedMessage> Own = {*MesMessageOfText("Good morning."), *MesMessageOfText("Fine weather.{page}\nIsn't it?"), *MesMessageOfText("Farewell.")};
		return *BuildMessageTable(Own);
	}

	/** The disc's text: map 5 is the small map. */
	struct FSmallDisc final : IDiscText
	{
		FBytes Messages = DiscMessages();

		std::span<const uint8> BattleEventMessages(int32) const override { return {}; }
		std::span<const uint8> FieldMapMessages(int32 Number) const override { return Number == 5 ? std::span<const uint8>(Messages) : std::span<const uint8>(); }
	};

	/** The small map given whole with Scripts in place of the disc's, and Messages after the head (the disc's three, written as the export writes them, by default). */
	std::string WholeText(std::string_view Scripts, std::string_view Messages = {})
	{
		const FBytes Section = Assembled(DiscScripts);
		const FBytes Table = DiscMessages();
		std::string Text = "map 5 whole\ndisc " + FSha1::ToHex(*FieldMapIdentity(Section, Table)) + "\n";
		if (Messages.empty())
		{
			const std::string Export = *WriteWholeFieldMapText(5, Section, Table);
			const std::size_t First = Export.find("\nmessage ");
			Text += Export.substr(First, Export.find("\nactors ") - First) + "\n";
		}
		else
		{
			Text += Messages;
		}
		return Text + std::string(Scripts);
	}

	/** The shareable text of the whole map, the port leaving the disc's scripts as they are or as Fixed gives them. */
	TResult<std::string> Shareable(const std::string& Whole, const FBytes* Fixed = nullptr)
	{
		const auto Patch = FFieldMapPatch::Parse(Whole);
		if (!Patch)
		{
			FAIL(Patch.Error().Message);
		}
		const FSmallDisc Disc;
		const FBytes Section = Assembled(DiscScripts);
		const auto Port = FEventScripts::Parse(Fixed != nullptr ? *Fixed : Section);
		REQUIRE(Port);
		return WriteShareableFieldMapText(*Patch, Section, Disc.Messages, Disc, *Port);
	}

	std::string Written(const std::string& Whole)
	{
		const auto Text = Shareable(Whole);
		if (!Text)
		{
			FAIL(Text.Error().Message);
		}
		return *Text;
	}

	int32 Count(std::string_view Text, std::string_view Word)
	{
		int32 Found = 0;
		for (std::size_t At = Text.find(Word); At != std::string_view::npos; At = Text.find(Word, At + 1))
		{
			++Found;
		}
		return Found;
	}

	std::string Scripts(std::string_view Talk, std::string_view Test = "    If var[010] == #1 else Other\n    Message msg1 0\n    End\nOther:\n    Message msg2 0\n    End\n")
	{
		return "actors 3\nunsigned 7F0\nentrances\nentrance x 10 z 20 layer 0 camera 2 facing keep\nactor 0 script 0:\n    Return\nactor 1 script 2:\n"
			+ std::string(Talk) + "actor 2 script 2:\n" + std::string(Test);
	}

	constexpr std::string_view Talk = "    Message msg0 0\n    Message msg1 0\n    Message msg2 0\n    End\n";
}

TEST_CASE("A whole map that plays as the disc's is written as nothing to share", "[Field]")
{
	CHECK(Written(WholeText(DiscScripts)).empty());
	// Labels, comments and blank lines are not what a map plays.
	CHECK(Written(WholeText(Scripts("    Message msg0 0 ; hello\nMine:\n\n    Message msg1 0\n    Message msg2 0\n    End\n"))).empty());
}

TEST_CASE("A message the author changed is given anew, and nothing else of the map", "[Field]")
{
	std::string Whole = WholeText(DiscScripts);
	Whole.replace(Whole.find("Farewell."), 9, "See you soon.");
	const std::string Text = Written(Whole);
	CHECK(Text.starts_with("map 5\ndisc "));
	CHECK(Text.find("message msg2 anew size ") != std::string::npos);
	CHECK(Text.find("See you soon.") != std::string::npos);
	// The disc's own words, and its scripts, are not in it.
	CHECK(Text.find("Good morning.") == std::string::npos);
	CHECK(Text.find("Fine weather.") == std::string::npos);
	CHECK(Count(Text, "Message") == 0);
	CHECK(Count(Text, "\n") == 6);
}

TEST_CASE("An instruction the author added is reached by a detour, the disc's code around it left in place", "[Field]")
{
	const std::string Text = Written(WholeText(Scripts("    Message msg0 0\n    Message msg1 0\n    Message msg0 0\n    Message msg2 0\n    End\n")));
	CHECK(Count(Text, "detour ") == 1);
	CHECK(Text.find("Part1:\n") != std::string::npos);
	// The detoured instruction itself, the one added, and the way back.
	CHECK(Count(Text, "    Message ") == 2);
	CHECK(Count(Text, "    Jump @0x") == 1);
	CHECK(Text.find("If ") == std::string::npos);
	CHECK(Text.find("End") == std::string::npos);
}

TEST_CASE("An operand the author changed is reached by a detour at its instruction", "[Field]")
{
	const std::string Text = Written(WholeText(Scripts("    Message msg0 0\n    Message msg2 0\n    Message msg2 0\n    End\n")));
	CHECK(Count(Text, "detour ") == 1);
	CHECK(Text.find("detour @0x") < Text.find(" Message msg1 0\n"));
	CHECK(Text.find("Part1:\n    Message msg2 0\n    Jump @0x") != std::string::npos);
	CHECK(Count(Text, "    Message ") == 1);
}

TEST_CASE("A script whose first instruction the author changed begins in the patch", "[Field]")
{
	const std::string Text = Written(WholeText(Scripts(Talk, "    If var[010] == #2 else Other\n    Message msg1 0\n    End\nOther:\n    Message msg2 0\n    End\n")));
	CHECK(Text.find("actor 2 script 2:\n    If var[010] == #2 else @0x") != std::string::npos);
	CHECK(Count(Text, "detour ") == 0);
	CHECK(Count(Text, "    Jump @0x") == 1);
	CHECK(Count(Text, "Message") == 0);
}

TEST_CASE("A script the author added begins in the patch, one taken away is no one's", "[Field]")
{
	const std::string Added = Written(WholeText(Scripts(Talk) + "actor 0 script 3:\n    Message msg2 0\n    End\n"));
	CHECK(Added.find("actor 0 script 3:\n    Message msg2 0\n    End\n") != std::string::npos);
	CHECK(Count(Added, "detour ") == 0);

	const std::string Taken = Written(WholeText("actors 3\nunsigned 7F0\nentrances\nentrance x 10 z 20 layer 0 camera 2 facing keep\nactor 0 script 0:\n    Return\nactor 2 script 2:\n"
		"    If var[010] == #1 else Other\n    Message msg1 0\n    End\nOther:\n    Message msg2 0\n    End\n"));
	CHECK(Taken.find("entry 1 2 @0x0000\n") != std::string::npos);
	CHECK(Count(Taken, "Message") == 0);

	// A script that now begins at other code of the disc's points there.
	const std::string Moved = Written(WholeText(Scripts("    Message msg1 0\n    Message msg2 0\n    End\n")));
	CHECK(Moved.find("entry 1 2 @0x") != std::string::npos);
	CHECK(Count(Moved, "Message") == 0);
	CHECK(Count(Moved, "\n") == 3);
}

TEST_CASE("A message the author added is named where it is one of the disc's own", "[Field]")
{
	std::string Messages = WholeText(DiscScripts);
	Messages = Messages.substr(Messages.find("message msg0"), Messages.find("actors 3") - Messages.find("message msg0"));
	Messages += "message Again\n{\nFine weather.{page}\nIsn't it?\n}\nmessage Asked\n{\nIsn't it?\n}\nmessage New\n{\nA word of my own.\n}\n";
	const std::string Text = Written(WholeText(Scripts("    Message msg0 0\n    Message Again 0\n    Message Asked 0\n    Message New 0\n    End\n"), Messages));
	CHECK(Text.find("message Again from map 5 message 1\n") != std::string::npos);
	CHECK(Text.find("message Asked from map 5 message 1 page 2\n") != std::string::npos);
	CHECK(Text.find("message New size ") != std::string::npos);
	CHECK(Text.find("Isn't it?") == std::string::npos);
	CHECK(Text.find("    Message Again 0\n    Message Asked 0\n    Message New 0\n") != std::string::npos);
}

TEST_CASE("An entrance the author moved is given anew", "[Field]")
{
	const std::string Text = Written(WholeText("actors 3\nunsigned 7F0\nentrances\nentrance x 15 z 25 layer 1 camera keep facing 3\nactor 0 script 0:\n    Return\n"
		"actor 1 script 2:\n" + std::string(Talk) + "actor 2 script 2:\n    If var[010] == #1 else Other\n    Message msg1 0\n    End\nOther:\n    Message msg2 0\n    End\n"));
	CHECK(Text == Text.substr(0, Text.find("disc ")) + Text.substr(Text.find("disc "), 46) + "entrance 0 x 15 z 25 layer 1 camera keep facing 3\n");
}

TEST_CASE("A detour is never made at bytes the port's fixes change", "[Field]")
{
	const std::string Whole = WholeText(Scripts("    Message msg0 0\n    Message msg1 0\n    Message msg0 0\n    Message msg2 0\n    End\n"));
	const std::string Plain = Written(Whole);
	const std::size_t Site = Plain.find("detour @0x");
	REQUIRE(Site != std::string::npos);
	const auto Offset = std::stoul(Plain.substr(Site + 10, 4), nullptr, 16);

	// The port changes that instruction's operand: the script begins in the patch instead.
	FBytes Fixed = Assembled(DiscScripts);
	const auto Parsed = FEventScripts::Parse(Fixed);
	REQUIRE(Parsed);
	const std::size_t CodeStart = Fixed.size() - Parsed->CodeSize();
	Fixed[CodeStart + Offset + 2] ^= 0x40;
	const auto Moved = Shareable(Whole, &Fixed);
	if (!Moved)
	{
		FAIL(Moved.Error().Message);
	}
	CHECK(Moved->find("detour ") == std::string::npos);
	CHECK(Moved->find("actor 1 script 2:\n    Message msg0 0\n    Message msg1 0\n    Message msg0 0\n    Jump @0x") != std::string::npos);
}

TEST_CASE("A whole map that changes what a patch cannot give is refused with the reason", "[Field]")
{
	const auto Error = [](const std::string& Whole)
	{
		const auto Text = Shareable(Whole);
		return Text ? std::string("written") : Text.Error().Message;
	};
	CHECK(Error(WholeText("actors 4" + Scripts(Talk).substr(8))) == "it has 4 script actors where the disc's map has 3: a patch keeps the disc's");
	CHECK(Error(WholeText("actors 3\nentrances" + Scripts(Talk).substr(Scripts(Talk).find("\nentrance ")))) ==
		"it reads var[7F0] signed where the disc's map reads it unsigned: a patch only marks variables unsigned");
	CHECK(Error(WholeText("actors 3\nunsigned 7F0 014\nentrances" + Scripts(Talk).substr(Scripts(Talk).find("\nentrance ")))) == "written");
	const std::string Two = "actors 3\nunsigned 7F0\nentrances\nentrance x 10 z 20 layer 0 camera 2 facing keep\nentrance x 1 z 2 layer 0 camera 2 facing keep\n";
	CHECK(Error(WholeText(Two + Scripts(Talk).substr(Scripts(Talk).find("actor 0 script 0:")))) ==
		"it has 2 entrances where the disc's map has 1: a patch gives the disc's anew, never more or fewer");
	std::string Other = WholeText(DiscScripts);
	Other.replace(Other.find("disc ") + 5, 2, Other.substr(Other.find("disc ") + 5, 2) == "00" ? "11" : "00");
	CHECK(Error(Other) == "it was written for another disc's map than this disc's");
}

TEST_CASE("Unsigned variables the author marked are marked by the patch", "[Field]")
{
	const std::string Text = Written(WholeText("actors 3\nunsigned 7F0 014\nentrances" + Scripts(Talk).substr(Scripts(Talk).find("\nentrance "))));
	CHECK(Text.find("\nunsigned 014\n") != std::string::npos);
}
