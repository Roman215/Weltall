#include "Story/DiscIdentity.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	FBytes Table(std::string_view First, std::string_view Second)
	{
		const std::vector<FAddedMessage> Messages = {*MesMessageOfText(First), *MesMessageOfText(Second)};
		return *BuildMessageTable(Messages);
	}
}

TEST_CASE("A map's identity is its scripts and each message's codes and window, whatever else its table's bytes hold", "[Story]")
{
	const FBytes Scripts = {0x01, 0x02, 0x03};
	const FBytes Messages = Table("Hello.", "Go away.");
	const auto Identity = FieldMapIdentity(Scripts, Messages);
	REQUIRE(Identity);
	CHECK(FieldMapIdentity(Scripts, Messages) == Identity);

	// Bytes past the last message's end are not read by the game, nor by the identity.
	FBytes Padded = Messages;
	Padded.insert(Padded.end(), {0x55, 0x66});
	CHECK(FieldMapIdentity(Scripts, Padded) == Identity);

	// A byte of the scripts, a message's code or its window changes it.
	CHECK(FieldMapIdentity(FBytes{0x01, 0x02, 0x04}, Messages) != Identity);
	CHECK(FieldMapIdentity(Scripts, Table("Hello!", "Go away.")) != Identity);
	std::vector<FAddedMessage> Wider = {*MesMessageOfText("Hello."), *MesMessageOfText("Go away.")};
	Wider[0].WidthUnits = static_cast<uint8>(Wider[0].WidthUnits + 1);
	CHECK(FieldMapIdentity(Scripts, *BuildMessageTable(Wider)) != Identity);
	CHECK(MessageTableIdentity(Messages) != Identity);
	CHECK(MessageTableIdentity(Messages) == MessageTableIdentity(Padded));
	CHECK(MessageTableIdentity(Messages) != MessageTableIdentity(Table("Hello.", "Go home.")));

	// Bytes that are no message table have none.
	CHECK_FALSE(FieldMapIdentity(Scripts, FBytes{}));
	CHECK_FALSE(MessageTableIdentity(FBytes{}));
}

TEST_CASE("The export's mark is the SHA-1 of its document's own text, which any edit but the lines' endings breaks", "[Story]")
{
	const std::string Text = ExportMark::Marked("map 5 whole\ndisc 00\n", "\nmessage msg0 size 14 1\n{\nHello.\n}\n");
	CHECK(Text.starts_with("map 5 whole\ndisc 00\nexported "));
	CHECK(Text.ends_with("\nmessage msg0 size 14 1\n{\nHello.\n}\n"));
	CHECK(ExportMark::LeftAsWritten(Text));

	// Saved with Windows' line endings or a byte order mark, it is the same text.
	std::string Windows;
	for (const char Character : Text)
	{
		Windows += Character == '\n' ? "\r\n" : std::string(1, Character);
	}
	CHECK(ExportMark::LeftAsWritten(Windows));
	CHECK(ExportMark::LeftAsWritten("\xEF\xBB\xBF" + Text));

	// A letter, a line added or the mark's own digits changed: edited.
	std::string Letter = Text;
	Letter[Letter.find("Hello")] = 'J';
	CHECK_FALSE(ExportMark::LeftAsWritten(Letter));
	CHECK_FALSE(ExportMark::LeftAsWritten(Text + "; a note\n"));
	std::string Digit = Text;
	const std::size_t Mark = Digit.find("exported ") + 9;
	Digit[Mark] = Digit[Mark] == '0' ? '1' : '0';
	CHECK_FALSE(ExportMark::LeftAsWritten(Digit));
	CHECK_FALSE(ExportMark::LeftAsWritten("map 5 whole\n"));

	CHECK(ExportMark::IsMark("exported " + std::string(40, 'a')));
	CHECK(ExportMark::IsMark("  exported\t" + std::string(40, 'A') + " \r"));
	CHECK_FALSE(ExportMark::IsMark("exported 12ab"));
	CHECK_FALSE(ExportMark::IsMark("exported" + std::string(40, 'a')));
	CHECK_FALSE(ExportMark::IsMark("; exported " + std::string(40, 'a')));
}
