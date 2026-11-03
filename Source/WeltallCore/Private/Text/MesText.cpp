#include "Text/MesText.h"

#include "Foundation/NumberText.h"

#include <algorithm>
#include <array>
#include <limits>

namespace Weltall
{
	namespace
	{
		constexpr uint8 End = 0x00;
		constexpr uint8 NewLine = 0x01;
		constexpr uint8 NewPage = 0x02;
		constexpr uint8 More = 0x03;
		constexpr uint8 Extended = 0x0F;
		/** Codes from here take a second byte. */
		constexpr uint8 LeadByte = 0xFE;
		constexpr uint8 BoxByte = 0xFF;
		/** One-byte glyphs below this are 8 pixels wide, two units; every other glyph is 12, three. */
		constexpr uint8 FirstWideGlyph = 0x61;
		constexpr uint8 FirstGlyph = 0x10;
		constexpr int32 NarrowUnits = 2;
		constexpr int32 WideUnits = 3;

		/** An extended code (0F, this byte, its operands) and its mark's word. */
		struct FExtendedMark
		{
			uint8 Code = 0;
			std::string_view Name;
			uint8 Operands = 0;
		};

		constexpr std::array<FExtendedMark, 16> ExtendedMarks = {{
			{0x00, "wait", 1}, {0x01, "speed", 1}, {0x02, "close", 1}, {0x03, "table", 2},
			{0x04, "item", 0}, {0x05, "name", 1}, {0x06, "weapon", 1}, {0x07, "table24", 1},
			{0x08, "table25", 1}, {0x09, "number", 1}, {0x0A, "digits", 1}, {0x0B, "mark", 1},
			{0x0C, "signed", 1}, {0x0D, "auto", 1}, {0x0E, "delay", 1}, {0x0F, "button", 1},
		}};

		/** A glyph or code with a mark of its own word. */
		struct FNamedCode
		{
			std::string_view Name;
			std::array<uint8, 2> Bytes{};
			uint8 Count = 0;
		};

		constexpr std::array<FNamedCode, 7> NamedCodes = {{
			{"page", {NewPage}, 1}, {"more", {More}, 1}, {"Circle", {0x6E}, 1}, {"Square", {0x6F}, 1},
			{"Triangle", {0x70}, 1}, {"Cross", {0x7F}, 1}, {"box", {BoxByte, BoxByte}, 2},
		}};

		const FNamedCode* NamedCodeOf(std::span<const uint8> Codes)
		{
			for (const FNamedCode& Named : NamedCodes)
			{
				if (Codes.size() >= Named.Count && std::equal(Named.Bytes.begin(), Named.Bytes.begin() + Named.Count, Codes.begin()))
				{
					return &Named;
				}
			}
			return nullptr;
		}

		void AppendMark(std::string& Text, std::string_view Name, std::span<const uint8> Operands = {})
		{
			Text += '{';
			Text += Name;
			for (const uint8 Operand : Operands)
			{
				Text += ' ';
				NumberText::AppendHex(Text, Operand, 2);
			}
			Text += '}';
		}

		void AppendRaw(std::string& Text, std::span<const uint8> Bytes)
		{
			Text += '{';
			for (const uint8 Byte : Bytes)
			{
				NumberText::AppendHex(Text, Byte, 2);
			}
			Text += '}';
		}

		/** The bytes an extended code takes after its 0F: its own byte and its operands. */
		std::size_t ExtendedBytes(uint8 Code)
		{
			return Code < ExtendedMarks.size() ? 1u + ExtendedMarks[Code].Operands : 1u;
		}

		/** The codes a mark's words (what stands between its braces) spell; false where they are no mark. */
		bool AppendCodesOfMark(std::string_view Words, std::vector<uint8>& Codes)
		{
			const std::size_t Space = Words.find(' ');
			const std::string_view Name = Words.substr(0, Space);
			std::string_view Rest = Space == std::string_view::npos ? std::string_view() : Words.substr(Space + 1);
			for (const FNamedCode& Named : NamedCodes)
			{
				if (Words == Named.Name)
				{
					Codes.insert(Codes.end(), Named.Bytes.begin(), Named.Bytes.begin() + Named.Count);
					return true;
				}
			}
			for (const FExtendedMark& Mark : ExtendedMarks)
			{
				if (Name != Mark.Name)
				{
					continue;
				}
				Codes.insert(Codes.end(), {Extended, Mark.Code});
				for (uint8 Operand = 0; Operand < Mark.Operands; ++Operand)
				{
					const std::size_t Digits = std::min(Rest.find(' '), Rest.size());
					const std::optional<uint32> Value = Digits <= 2 ? NumberText::Hex(Rest.substr(0, Digits)) : std::nullopt;
					if (!Value)
					{
						return false;
					}
					Codes.push_back(static_cast<uint8>(*Value));
					Rest.remove_prefix(std::min(Digits + 1, Rest.size()));
				}
				return Rest.empty();
			}
			if (Words.size() == 3 && Words.front() == 'x')
			{
				const std::optional<uint32> Code = NumberText::Hex(Words.substr(1));
				if (!Code || *Code < ExtendedMarks.size())
				{
					return false;
				}
				Codes.insert(Codes.end(), {Extended, static_cast<uint8>(*Code)});
				return true;
			}
			const std::optional<uint32> Raw = Words.size() == 2 || Words.size() == 4 ? NumberText::Hex(Words) : std::nullopt;
			// A byte as it is: no 00, which ends the message, and two bytes only for a glyph of two.
			if (!Raw || (Words.size() == 2 && *Raw == End) || (Words.size() == 4 && *Raw >> 8 < LeadByte))
			{
				return false;
			}
			if (Words.size() == 4)
			{
				Codes.push_back(static_cast<uint8>(*Raw >> 8));
			}
			Codes.push_back(static_cast<uint8>(*Raw));
			return true;
		}
	}

	std::string MesTextOfCodes(std::span<const uint8> Codes)
	{
		return MesTextOfCodes(Codes, FMesCharacters::Plain());
	}

	std::optional<std::vector<uint8>> MesCodesOfText(std::string_view Text)
	{
		return MesCodesOfText(Text, FMesCharacters::Plain());
	}

	std::size_t MesMessageSize(std::span<const uint8> Codes)
	{
		std::size_t At = 0;
		while (At < Codes.size() && Codes[At] != End)
		{
			const std::span<const uint8> Rest = Codes.subspan(At);
			std::size_t Taken = 1;
			if (const FNamedCode* Named = NamedCodeOf(Rest))
			{
				Taken = Named->Count;
			}
			else if (Rest[0] == Extended && Rest.size() > 1 && Rest.size() > ExtendedBytes(Rest[1]))
			{
				Taken = 1 + ExtendedBytes(Rest[1]);
			}
			else if (Rest[0] >= LeadByte && Rest.size() > 1)
			{
				Taken = 2;
			}
			At += Taken;
		}
		return std::min(At + 1, Codes.size());
	}

	std::vector<FMesCode> MesGlyphsOfCodes(std::span<const uint8> Codes)
	{
		std::vector<FMesCode> Glyphs;
		Codes = Codes.first(MesMessageSize(Codes));
		while (!Codes.empty() && Codes.front() != End)
		{
			const uint8 Code = Codes.front();
			std::size_t Taken = 1;
			if (Code == Extended && Codes.size() > 1 && Codes.size() > ExtendedBytes(Codes[1]))
			{
				Taken = 1 + ExtendedBytes(Codes[1]);
			}
			else if (Code >= LeadByte && Codes.size() > 1)
			{
				Glyphs.push_back({Code, Codes[1]});
				Taken = 2;
			}
			else if (Code >= FirstGlyph)
			{
				Glyphs.push_back({0, Code});
			}
			Codes = Codes.subspan(Taken);
		}
		return Glyphs;
	}

	std::string MesTextOfCodes(std::span<const uint8> Codes, const FMesCharacters& Characters)
	{
		std::string Text;
		while (!Codes.empty() && Codes.front() != End)
		{
			const uint8 Code = Codes.front();
			std::size_t Taken = 1;
			if (Code == NewLine)
			{
				Text += '\n';
			}
			else if (const FNamedCode* Named = NamedCodeOf(Codes))
			{
				AppendMark(Text, Named->Name);
				Taken = Named->Count;
			}
			else if (Code == Extended && Codes.size() > 1 && Codes.size() > ExtendedBytes(Codes[1]))
			{
				Taken = 1 + ExtendedBytes(Codes[1]);
				if (Codes[1] < ExtendedMarks.size())
				{
					AppendMark(Text, ExtendedMarks[Codes[1]].Name, Codes.subspan(2, Taken - 2));
				}
				else
				{
					Text += "{x";
					NumberText::AppendHex(Text, Codes[1], 2);
					Text += '}';
				}
			}
			else if (Code >= LeadByte && Codes.size() > 1)
			{
				if (const std::string_view Character = Characters.CharacterOf({Code, Codes[1]}); !Character.empty())
				{
					Text += Character;
				}
				else
				{
					AppendRaw(Text, Codes.first(2));
				}
				Taken = 2;
			}
			else if (const std::string_view Character = Characters.CharacterOf({0, Code}); !Character.empty())
			{
				Text += Character;
			}
			else
			{
				AppendRaw(Text, Codes.first(1));
			}
			Codes = Codes.subspan(Taken);
		}
		return Text;
	}

	std::optional<std::vector<uint8>> MesCodesOfText(std::string_view Text, const FMesCharacters& Characters)
	{
		std::vector<uint8> Codes;
		while (!Text.empty())
		{
			if (Text.front() == '{')
			{
				const std::size_t Close = Text.find('}');
				if (Close == std::string_view::npos || !AppendCodesOfMark(Text.substr(1, Close - 1), Codes))
				{
					return std::nullopt;
				}
				Text.remove_prefix(Close + 1);
				continue;
			}
			if (Text.front() == '\n')
			{
				Codes.push_back(NewLine);
				Text.remove_prefix(1);
				continue;
			}
			const std::optional<FMesCharacterCode> Code = Characters.CodeAt(Text);
			if (!Code)
			{
				return std::nullopt;
			}
			if (Code->Code.Lead != 0)
			{
				Codes.push_back(Code->Code.Lead);
			}
			Codes.push_back(Code->Code.Code);
			Text.remove_prefix(Code->Bytes);
		}
		Codes.push_back(End);
		return Codes;
	}

	FMesExtent MesExtentOfCodes(std::span<const uint8> Codes)
	{
		static const FMesGlyphWidths GamesOwn;
		return MesExtentOfCodes(Codes, GamesOwn);
	}

	FMesExtent MesExtentOfCodes(std::span<const uint8> Codes, const FMesGlyphWidths& Widths)
	{
		FMesExtent Extent{0, 1};
		int32 LineUnits = 0;
		int32 PageLines = 1;
		while (!Codes.empty() && Codes.front() != End)
		{
			const uint8 Code = Codes.front();
			std::size_t Taken = 1;
			if (Code == Extended && Codes.size() > 1)
			{
				Taken = std::min(Codes.size(), 1 + ExtendedBytes(Codes[1]));
			}
			else if (Code == NewLine || Code == NewPage)
			{
				LineUnits = 0;
				PageLines = Code == NewLine ? PageLines + 1 : 1;
				// The line break right after a new page is the page's own.
				if (Code == NewPage && Codes.size() > 1 && Codes[1] == NewLine)
				{
					Taken = 2;
				}
			}
			else if (Code >= FirstGlyph)
			{
				Taken = Code >= LeadByte && Codes.size() > 1 ? 2 : 1;
				const auto Own = Widths.empty() ? Widths.end() : Widths.find(static_cast<uint16>(Taken == 2 ? Code << 8 | Codes[1] : Code));
				LineUnits += Own != Widths.end() ? Own->second : Code < FirstWideGlyph ? NarrowUnits : WideUnits;
			}
			Extent.WidthUnits = std::max(Extent.WidthUnits, LineUnits);
			Extent.Lines = std::max(Extent.Lines, PageLines);
			Codes = Codes.subspan(Taken);
		}
		return Extent;
	}

	std::optional<std::vector<uint8>> MesPagesOfCodes(std::span<const uint8> Codes, int32 From, int32 To)
	{
		std::vector<uint8> Pages;
		int32 Page = 1;
		while (!Codes.empty() && Codes.front() != End)
		{
			const uint8 Code = Codes.front();
			std::size_t Taken = 1;
			// A page's mark belongs to neither page: it stays only between two pages kept.
			bool bKept = Page >= From && Page <= To;
			if (Code == Extended && Codes.size() > 1)
			{
				Taken = std::min(Codes.size(), 1 + ExtendedBytes(Codes[1]));
			}
			else if (Code == NewPage)
			{
				// The line break right after a new page is the page's own.
				Taken = Codes.size() > 1 && Codes[1] == NewLine ? 2 : 1;
				bKept = Page >= From && Page < To;
				++Page;
			}
			else if (Code >= LeadByte && Codes.size() > 1)
			{
				Taken = 2;
			}
			if (bKept)
			{
				Pages.insert(Pages.end(), Codes.begin(), Codes.begin() + static_cast<std::ptrdiff_t>(Taken));
			}
			Codes = Codes.subspan(Taken);
		}
		if (Page < From)
		{
			return std::nullopt;
		}
		Pages.push_back(End);
		return Pages;
	}

	std::vector<uint8> MesPagesJoined(std::span<const uint8> First, std::span<const uint8> Second)
	{
		constexpr int32 EveryPage = std::numeric_limits<int32>::max();
		std::vector<uint8> Joined = MesPagesOfCodes(First, 1, EveryPage).value_or(std::vector<uint8>{End});
		// The first's end becomes the page's mark, with the line break the game's pages begin with.
		Joined.back() = NewPage;
		Joined.push_back(NewLine);
		const std::vector<uint8> Rest = MesPagesOfCodes(Second, 1, EveryPage).value_or(std::vector<uint8>{End});
		Joined.insert(Joined.end(), Rest.begin(), Rest.end());
		return Joined;
	}

	std::optional<FAddedMessage> MesMessageOfText(std::string_view Text)
	{
		std::optional<std::vector<uint8>> Codes = MesCodesOfText(Text);
		if (!Codes)
		{
			return std::nullopt;
		}
		const FMesExtent Extent = MesExtentOfCodes(*Codes);
		if (Extent.WidthUnits > 0xFF || Extent.Lines > 0xFF)
		{
			return std::nullopt;
		}
		return FAddedMessage{std::move(*Codes), static_cast<uint8>(Extent.WidthUnits), static_cast<uint8>(Extent.Lines)};
	}

	std::optional<std::vector<uint8>> BuildMessageTable(std::span<const FAddedMessage> Messages)
	{
		// A table without messages: FFFF for the last message's number, and the word after it.
		std::vector<uint8> Table = {0xFF, 0xFF, 0x00, 0x00};
		if (!AppendMessages(Table, Messages))
		{
			return std::nullopt;
		}
		return Table;
	}

	std::optional<std::vector<uint8>> ReplaceMessages(std::span<const uint8> Table, std::span<const std::pair<int32, FAddedMessage>> Replacements)
	{
		const TResult<FMessageTable> Parsed = FMessageTable::Parse(Table);
		if (!Parsed || std::ranges::any_of(Replacements, [&Parsed](const auto& Replacement) { return Replacement.first < 0 || Replacement.first >= Parsed->Count(); }))
		{
			return std::nullopt;
		}
		std::vector<FAddedMessage> Messages;
		for (int32 Index = 0; Index < Parsed->Count(); ++Index)
		{
			const auto Replacement = std::ranges::find_if(Replacements, [Index](const auto& Candidate) { return Candidate.first == Index; });
			if (Replacement != Replacements.end())
			{
				Messages.push_back(Replacement->second);
				continue;
			}
			// String is empty for a message past the table's bytes, which an added message without codes stays.
			const std::span<const uint8> String = Parsed->String(Index);
			const std::span<const uint8> Codes = String.first(MesMessageSize(String));
			Messages.push_back({std::vector<uint8>(Codes.begin(), Codes.end()), static_cast<uint8>(Parsed->WidthUnits(Index)), static_cast<uint8>(Parsed->Lines(Index))});
		}
		return BuildMessageTable(Messages);
	}
}
