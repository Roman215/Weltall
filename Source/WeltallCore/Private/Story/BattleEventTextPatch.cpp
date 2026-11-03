#include "Story/BattleEventTextPatch.h"

#include "Field/EventAssemblyText.h"
#include "Foundation/NumberText.h"
#include "Story/DiscIdentity.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		/** The most event scripts a formation's byte can name, and the most messages or pages a text can. */
		constexpr int64 LastEvent = 0xFF;
		constexpr int64 LastMessage = 0xFFFF;
		constexpr int32 EveryPage = std::numeric_limits<int32>::max();
		constexpr std::string_view BeginningShape = "a message's beginning reads 'message K from page P', the pages counted from 1, or with 'after' and the text told first";

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}
	}

	TResult<FBattleEventTextPatch> FBattleEventTextPatch::Parse(std::string_view Text)
	{
		enum class EReading : uint8
		{
			Lines,
			TextBrace,
			TextLines,
		};

		FBattleEventTextPatch Patch;
		EReading Reading = EReading::Lines;
		bool bEvent = false;
		std::string Told;
		bool bToldBegun = false;
		bool bTellingAnew = false;
		int32 MarkLine = 0;
		for (int32 Number = 1; !Text.empty(); ++Number)
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Line.ends_with('\r'))
			{
				Line.remove_suffix(1);
			}
			if (Reading == EReading::TextLines)
			{
				if (Line != "}")
				{
					if (!MesCodesOfText(Line))
					{
						return LineError(Number, "a character or a mark the messages lack");
					}
					Told += bToldBegun ? "\n" : "";
					Told += Line;
					bToldBegun = true;
				}
				else
				{
					// Its lines were read one by one, so the whole has codes.
					std::vector<uint8>& Codes = Patch.bWhole ? Patch.WholeMessages.back().Codes
						: bTellingAnew ? Patch.AnewMessages.back().Given.Codes : Patch.Beginnings.back().Before;
					Codes = MesCodesOfText(Told).value_or(std::vector<uint8>());
					Reading = EReading::Lines;
				}
				continue;
			}
			const std::vector<std::string_view> Words = WordsOf(Line);
			if (Reading == EReading::TextBrace)
			{
				if (Words.size() != 1 || Words.front() != "{")
				{
					return LineError(Number, Patch.bWhole || bTellingAnew ? "a message's text begins after a line of '{'" : "the text told first begins after a line of '{'");
				}
				Reading = EReading::TextLines;
			}
			else if (Words.empty())
			{
			}
			else if (ExportMark::IsMark(Line))
			{
				MarkLine = Number;
			}
			else if (Words.front() == "battle")
			{
				const bool bWhole = Words.size() == 4 && Words[3] == "whole";
				const std::optional<int64> Event = (Words.size() == 3 || bWhole) && Words[1] == "event" ? NumberText::NumberWithin(Words[2], 0, LastEvent) : std::nullopt;
				if (!Event || bEvent)
				{
					return LineError(Number, "'battle event N' takes the event's number, once");
				}
				Patch.EventNumber = static_cast<int32>(*Event);
				Patch.bWhole = bWhole;
				bEvent = true;
			}
			else if (Words.front() == "disc")
			{
				const std::optional<FSha1::FDigest> Identity = Words.size() == 2 ? FSha1::FromHex(Words[1]) : std::nullopt;
				if (!Identity || Patch.WrittenForTable)
				{
					return LineError(Number, "'disc' takes the SHA-1 of the disc's table, once");
				}
				Patch.WrittenForTable = Identity;
			}
			else if (Words.front() == "message" && Words.size() == 5 && Words[2] == "size")
			{
				// "message K size width lines" and its text: a message of a whole table, numbered in their order.
				const std::optional<int64> Message = NumberText::NumberWithin(Words[1], 0, LastMessage);
				const std::optional<int64> Width = NumberText::NumberWithin(Words[3], 0, 0xFF);
				const std::optional<int64> Lines = NumberText::NumberWithin(Words[4], 0, 0xFF);
				if (!Patch.bWhole || !Message || !Width || !Lines || *Message != static_cast<int64>(Patch.WholeMessages.size()))
				{
					return LineError(Number, "a whole table's messages read 'message K size width lines', numbered from 0 in their order");
				}
				Patch.WholeMessages.push_back({{}, static_cast<uint8>(*Width), static_cast<uint8>(*Lines)});
				Told.clear();
				bToldBegun = false;
				Reading = EReading::TextBrace;
			}
			else if (Words.front() == "message" && Words.size() == 6 && Words[2] == "anew" && Words[3] == "size")
			{
				// "message K anew size width lines" and its text: one of the disc's messages given anew.
				const std::optional<int64> Message = NumberText::NumberWithin(Words[1], 0, LastMessage);
				const std::optional<int64> Width = NumberText::NumberWithin(Words[4], 0, 0xFF);
				const std::optional<int64> Lines = NumberText::NumberWithin(Words[5], 0, 0xFF);
				if (Patch.bWhole || !Message || !Width || !Lines)
				{
					return LineError(Number, "a message given anew reads 'message K anew size width lines', in a text that does not give the table whole");
				}
				const auto Given = [&Message](const FAnew& Other) { return Other.Message == *Message; };
				if (std::any_of(Patch.AnewMessages.begin(), Patch.AnewMessages.end(), Given)
					|| std::any_of(Patch.Beginnings.begin(), Patch.Beginnings.end(), [&Message](const FBeginning& Other) { return Other.Message == *Message; }))
				{
					return LineError(Number, "a message given anew twice, or begun too");
				}
				Patch.AnewMessages.push_back({static_cast<int32>(*Message), {{}, static_cast<uint8>(*Width), static_cast<uint8>(*Lines)}});
				Told.clear();
				bToldBegun = false;
				bTellingAnew = true;
				Reading = EReading::TextBrace;
			}
			else if (Words.front() == "message")
			{
				// "message K from page P", or "message K from page P after" and the text told first.
				const bool bAfter = Words.size() == 6 && Words[5] == "after";
				const bool bFromPage = (Words.size() == 5 || bAfter) && Words[2] == "from" && Words[3] == "page";
				const std::optional<int64> Message = bFromPage ? NumberText::NumberWithin(Words[1], 0, LastMessage) : std::nullopt;
				const std::optional<int64> Page = bFromPage ? NumberText::NumberWithin(Words[4], 1, LastMessage) : std::nullopt;
				if (!Message || !Page)
				{
					return LineError(Number, BeginningShape);
				}
				if (std::any_of(Patch.Beginnings.begin(), Patch.Beginnings.end(), [&Message](const FBeginning& Other) { return Other.Message == *Message; })
					|| std::any_of(Patch.AnewMessages.begin(), Patch.AnewMessages.end(), [&Message](const FAnew& Other) { return Other.Message == *Message; }))
				{
					return LineError(Number, "a message given two beginnings, or given anew too");
				}
				Patch.Beginnings.push_back({static_cast<int32>(*Message), static_cast<int32>(*Page), {}});
				if (bAfter)
				{
					Told.clear();
					bToldBegun = false;
					bTellingAnew = false;
					Reading = EReading::TextBrace;
				}
			}
			else
			{
				return LineError(Number, "a battle event's text takes 'battle event N' and its messages' beginnings");
			}
		}
		if (Reading != EReading::Lines)
		{
			return MakeError(Patch.bWhole || bTellingAnew ? "the last message's text does not end with a line of '}'" : "the text told first does not end with a line of '}'");
		}
		if (!bEvent)
		{
			return MakeError("the text names no battle event");
		}
		if (!Patch.bWhole && MarkLine != 0)
		{
			return LineError(MarkLine, "only a whole table carries an export's mark");
		}
		if (Patch.bWhole && !Patch.WrittenForTable)
		{
			return MakeError("a whole table names the disc's table it was written for: 'disc' and its SHA-1");
		}
		if (Patch.bWhole && !Patch.Beginnings.empty())
		{
			return MakeError("a whole table gives every message itself, and begins none of the disc's at a later page");
		}
		return Patch;
	}

	std::optional<std::vector<uint8>> FBattleEventTextPatch::Replace(std::span<const uint8> DiscMessages) const
	{
		if (!bWhole || MessageTableIdentity(DiscMessages) != WrittenForTable)
		{
			return std::nullopt;
		}
		return BuildMessageTable(WholeMessages);
	}

	TResult<std::string> WriteWholeBattleEventText(int32 Event, std::span<const uint8> Messages)
	{
		const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
		const std::optional<FSha1::FDigest> Identity = MessageTableIdentity(Messages);
		if (!Table || !Identity)
		{
			return MakeError("the event's messages cannot be read");
		}
		std::string Head = "battle event ";
		NumberText::Append(Head, Event);
		Head += " whole\ndisc ";
		Head += FSha1::ToHex(*Identity);
		Head += "\n";
		std::string Body;
		for (int32 Message = 0; Message < Table->Count(); ++Message)
		{
			Body += "\nmessage ";
			NumberText::Append(Body, Message);
			Body += " size ";
			NumberText::Append(Body, Table->WidthUnits(Message));
			Body += " ";
			NumberText::Append(Body, Table->Lines(Message));
			Body += "\n{\n";
			Body += MesTextOfCodes(Table->String(Message));
			Body += "\n}\n";
		}
		return ExportMark::Marked(Head, Body);
	}

	std::optional<std::vector<uint8>> FBattleEventTextPatch::Apply(std::span<const uint8> Messages, const FSha1::FDigest* Identity) const
	{
		const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
		const auto Lacks = [&Table](int32 Message) { return Message >= Table->Count(); };
		if (bWhole || !Table || (WrittenForTable && (Identity == nullptr || *Identity != *WrittenForTable))
			|| std::any_of(Beginnings.begin(), Beginnings.end(), [&Lacks](const FBeginning& Beginning) { return Lacks(Beginning.Message); })
			|| std::any_of(AnewMessages.begin(), AnewMessages.end(), [&Lacks](const FAnew& Anew) { return Lacks(Anew.Message); }))
		{
			return std::nullopt;
		}
		std::vector<FAddedMessage> Changed;
		for (int32 Message = 0; Message < Table->Count(); ++Message)
		{
			if (const auto Anew = std::find_if(AnewMessages.begin(), AnewMessages.end(), [Message](const FAnew& Other) { return Other.Message == Message; });
				Anew != AnewMessages.end())
			{
				Changed.push_back(Anew->Given);
				continue;
			}
			const auto Beginning = std::find_if(Beginnings.begin(), Beginnings.end(), [Message](const FBeginning& Other) { return Other.Message == Message; });
			std::optional<std::vector<uint8>> Codes = MesPagesOfCodes(Table->String(Message), Beginning != Beginnings.end() ? Beginning->Page : 1, EveryPage);
			if (!Codes)
			{
				return std::nullopt;
			}
			if (Beginning != Beginnings.end() && !Beginning->Before.empty())
			{
				// What is told first is typed in the message's own window.
				const FMesExtent Extent = MesExtentOfCodes(Beginning->Before);
				if (Extent.WidthUnits > Table->WidthUnits(Message) || Extent.Lines > Table->Lines(Message))
				{
					return std::nullopt;
				}
				Codes = MesPagesJoined(Beginning->Before, *Codes);
			}
			Changed.push_back({std::move(*Codes), static_cast<uint8>(Table->WidthUnits(Message)), static_cast<uint8>(Table->Lines(Message))});
		}
		return BuildMessageTable(Changed);
	}
}
