#include "Text/MesText.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	using FBytes = std::vector<uint8>;

	FBytes Codes(std::string_view Text)
	{
		const auto Encoded = MesCodesOfText(Text);
		REQUIRE(Encoded);
		return *Encoded;
	}
}

TEST_CASE("A message's text is its glyphs' characters, line breaks and marks", "[Text]")
{
	// "Oh!", a pause of 14h, a new line, "0 a".
	const FBytes Greeting = {0x2E, 0x44, 0x57, 0x0F, 0x00, 0x14, 0x01, 0x16, 0x10, 0x3D, 0x00};
	CHECK(MesTextOfCodes(Greeting) == "Oh!{wait 14}\n0 a");
	CHECK(Codes("Oh!{wait 14}\n0 a") == Greeting);

	// Every mark, and what follows a message's end is not its text.
	const FBytes Marks = {
		0x02, 0x03, 0x0F, 0x01, 0x02, 0x0F, 0x02, 0x3C, 0x0F, 0x03, 0x01, 0x2A, 0x0F, 0x04, 0x0F, 0x05, 0x80, 0x0F, 0x06, 0x03,
		0x0F, 0x07, 0x00, 0x0F, 0x08, 0x09, 0x0F, 0x09, 0x00, 0x0F, 0x0A, 0x01, 0x0F, 0x0B, 0x07, 0x0F, 0x0C, 0x02, 0x0F, 0x0D, 0x3C,
		0x0F, 0x0E, 0x01, 0x0F, 0x0F, 0x04, 0x6E, 0x6F, 0x70, 0x7F, 0xFF, 0xFF, 0x8F, 0xFE, 0x12, 0x0F, 0x10, 0x04, 0x00, 0x20,
	};
	const std::string_view MarksText =
		"{page}{more}{speed 02}{close 3C}{table 01 2A}{item}{name 80}{weapon 03}{table24 00}{table25 09}{number 00}{digits 01}{mark 07}{signed 02}"
		"{auto 3C}{delay 01}{button 04}{Circle}{Square}{Triangle}{Cross}{box}{8F}{FE12}{x10}{04}";
	CHECK(MesTextOfCodes(Marks) == MarksText);
	CHECK(Codes(MarksText) == FBytes(Marks.begin(), Marks.end() - 1));
}

TEST_CASE("Codes cut short read as the bytes they are", "[Text]")
{
	// An extended code without its operand, a lead byte without its second: each a byte in braces, and back.
	for (const FBytes& Cut : {FBytes{0x20, 0x0F, 0x05}, FBytes{0x20, 0x0F}, FBytes{0x20, 0xFE}, FBytes{0xFF}})
	{
		const std::string Text = MesTextOfCodes(Cut);
		FBytes Whole = Cut;
		Whole.push_back(0x00);
		CHECK(Codes(Text) == Whole);
	}
	CHECK(MesTextOfCodes(FBytes{0x20, 0x0F, 0x05}) == "A{0F}{05}");
	CHECK(MesTextOfCodes({}) == "");
}

TEST_CASE("Text with a character or a mark the messages lack is refused", "[Text]")
{
	CHECK_FALSE(MesCodesOfText("A~B"));
	CHECK_FALSE(MesCodesOfText("{nothing}"));
	CHECK_FALSE(MesCodesOfText("{wait}"));
	CHECK_FALSE(MesCodesOfText("{wait 123}"));
	CHECK_FALSE(MesCodesOfText("{wait 14 15}"));
	CHECK_FALSE(MesCodesOfText("{item 01}"));
	CHECK_FALSE(MesCodesOfText("{00}"));
	CHECK_FALSE(MesCodesOfText("{1234}"));
	CHECK_FALSE(MesCodesOfText("{x05}"));
	CHECK_FALSE(MesCodesOfText("{page"));
	CHECK(MesCodesOfText(""));
}

TEST_CASE("A message's window is its widest line and its tallest page", "[Text]")
{
	const auto Extent = [](std::string_view Text) { return MesExtentOfCodes(Codes(Text)); };
	CHECK(Extent("Hello").WidthUnits == 10);
	CHECK(Extent("Hello").Lines == 1);
	CHECK(Extent("One\nThree\nTwo").WidthUnits == 10);
	CHECK(Extent("One\nThree\nTwo").Lines == 3);
	// A glyph past the plain characters is twelve pixels wide, three units; marks take no room.
	CHECK(Extent("A{8F}{FE12}{box}{wait 14}{name 00}").WidthUnits == 11);
	// Pages: the line break after a new page is the page's own.
	CHECK(Extent("One\nTwo{page}\nThree").Lines == 2);
	CHECK(Extent("One{page}Two\nThree\nFour").Lines == 3);
	CHECK(Extent("One{page}Two\nThree\nFour").WidthUnits == 10);

	const auto Message = MesMessageOfText("Oh dear...\nHe's lost.");
	REQUIRE(Message);
	CHECK(Message->WidthUnits == 20);
	CHECK(Message->Lines == 2);
	CHECK(Message->Codes.back() == 0x00);
	CHECK_FALSE(MesMessageOfText("A~B"));
	// Wider than a table's byte can say.
	CHECK_FALSE(MesMessageOfText(std::string(128, 'A')));
}

TEST_CASE("A message's pages are taken out of it as a message of their own", "[Text]")
{
	const FBytes Three = Codes("One,{wait 14}\nfirst{page}\nTwo{page}Three\nlast");
	const auto Pages = [&Three](int32 From, int32 To)
	{
		const auto Taken = MesPagesOfCodes(Three, From, To);
		return Taken ? MesTextOfCodes(*Taken) : std::string("none");
	};
	CHECK(Pages(1, 1) == "One,{wait 14}\nfirst");
	CHECK(Pages(2, 2) == "Two");
	CHECK(Pages(3, 3) == "Three\nlast");
	// The marks between the pages kept stay, each with the line break that is its own.
	CHECK(Pages(1, 2) == "One,{wait 14}\nfirst{page}\nTwo");
	CHECK(Pages(2, 3) == "Two{page}Three\nlast");
	CHECK(Pages(1, 99) == MesTextOfCodes(Three));
	CHECK(Pages(4, 4) == "none");

	// A mark's operand of 00 ends nothing, and what is taken ends as a message does.
	const auto First = MesPagesOfCodes(Codes("A{wait 00}B{page}C"), 1, 1);
	REQUIRE(First);
	CHECK(MesTextOfCodes(*First) == "A{wait 00}B");
	CHECK(First->back() == 0x00);
	CHECK(First->size() == 6);
}

TEST_CASE("Messages build a table the game reads", "[Text]")
{
	const std::vector<FAddedMessage> Messages = {*MesMessageOfText("First"), *MesMessageOfText("The\nsecond"), *MesMessageOfText("")};
	const auto Built = BuildMessageTable(Messages);
	REQUIRE(Built);
	const auto Table = FMessageTable::Parse(*Built);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 3);
	CHECK(MesTextOfCodes(Table->String(0)) == "First");
	CHECK(MesTextOfCodes(Table->String(1)) == "The\nsecond");
	CHECK(MesTextOfCodes(Table->String(2)) == "");
	CHECK(Table->WidthUnits(0) == 10);
	CHECK(Table->WidthUnits(1) == 12);
	CHECK(Table->Lines(1) == 2);

	const auto Empty = BuildMessageTable({});
	REQUIRE(Empty);
	const auto None = FMessageTable::Parse(*Empty);
	REQUIRE(None);
	CHECK(None->Count() == 0);
}

TEST_CASE("A table whose last message starts past its bytes is kept, that message read as none, and stays so as the table is rebuilt", "[Text]")
{
	// Two messages, and a third whose offset is one past the table's end, as a translation's tool can leave unused ones.
	FBytes Bytes = {0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x12, 0x00, 0x15, 0x00, 2, 1, 2, 1, 2, 1, 0x21, 0x00, 0x22, 0x00};
	const auto Table = FMessageTable::Parse(Bytes);
	REQUIRE(Table);
	REQUIRE(Table->Count() == 3);
	const auto Message = [](std::span<const uint8> String) { return FBytes(String.begin(), String.begin() + static_cast<std::ptrdiff_t>(MesMessageSize(String))); };
	CHECK(Message(Table->String(0)) == FBytes{0x21, 0x00});
	CHECK(Message(Table->String(1)) == FBytes{0x22, 0x00});
	CHECK_FALSE(Table->StartsPastBytes(1));
	CHECK(Table->StartsPastBytes(2));
	CHECK(Table->String(2).empty());

	// Added messages move the others, but not that one into them.
	REQUIRE(AppendMessages(Bytes, std::vector<FAddedMessage>{*MesMessageOfText("C")}) == 3);
	const auto Appended = FMessageTable::Parse(Bytes);
	REQUIRE(Appended);
	CHECK(Message(Appended->String(1)) == FBytes{0x22, 0x00});
	CHECK(Appended->String(2).empty());
	CHECK(MesTextOfCodes(Appended->String(3)) == "C");

	// Built again from its messages, as the player's text rebuilds a table.
	std::vector<FAddedMessage> Messages;
	for (int32 Index = 0; Index < Appended->Count(); ++Index)
	{
		Messages.push_back({Message(Appended->String(Index)), static_cast<uint8>(Appended->WidthUnits(Index)), static_cast<uint8>(Appended->Lines(Index))});
	}
	const auto Built = BuildMessageTable(Messages);
	REQUIRE(Built);
	const auto Rebuilt = FMessageTable::Parse(*Built);
	REQUIRE(Rebuilt);
	CHECK(Message(Rebuilt->String(0)) == FBytes{0x21, 0x00});
	CHECK(Rebuilt->StartsPastBytes(2));
	CHECK(MesTextOfCodes(Rebuilt->String(3)) == "C");
}
