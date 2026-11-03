#include "Field/FieldMapPatch.h"

#include "EventAssemblyText.h"
#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"
#include "Field/FieldMap.h"
#include "Foundation/NumberText.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		constexpr std::size_t JumpBytes = 3;
		/** The last field map a number can name: places from 400h on are the world map's. */
		constexpr int64 LastFieldMap = 0x3FF;
		/** The most battle events a formation's byte can name, and the most messages or pages a text can. */
		constexpr int64 LastBattleEvent = 0xFF;
		constexpr int64 LastMessage = 0xFFFF;
		constexpr int32 EveryPage = std::numeric_limits<int32>::max();

		FError LineError(int32 Line, std::string_view Words)
		{
			std::string Message = "line ";
			NumberText::Append(Message, Line);
			Message += ": ";
			Message += Words;
			return MakeError(std::move(Message));
		}

		/** The offset in "@offset". */
		std::optional<uint16> OffsetOf(std::string_view Word)
		{
			const std::optional<int64> Offset = Word.starts_with('@') ? NumberText::NumberWithin(Word.substr(1), 0, 0xFFFF) : std::nullopt;
			return Offset ? std::optional<uint16>(static_cast<uint16>(*Offset)) : std::nullopt;
		}

		std::string Joined(std::span<const std::string_view> Words)
		{
			std::string Text;
			for (const std::string_view Word : Words)
			{
				Text += Text.empty() ? "" : " ";
				Text += Word;
			}
			return Text;
		}

		/** A message's line read: the message, its text still to come where bText, in the window Size where one is given. */
		struct FMessageLine
		{
			FFieldMapPatch::FNamedMessage Message;
			bool bText = false;
			std::optional<FMesExtent> Size;
		};

		/**
		 * "message Name" or "message Name size W L", a text to come; "message Name from event E message K [page P]" or
		 * "... from map M message K [page P]", a message of the disc's own; any of them with "anew" after the name msgK, the
		 * map's own message K given anew.
		 */
		TResult<FMessageLine> ReadMessageLine(int32 Number, std::vector<std::string_view> Words)
		{
			FMessageLine Read;
			if (Words.size() > 2 && Words[2] == "anew")
			{
				const std::optional<int64> Own = Words[1].starts_with("msg") ? NumberText::NumberWithin(Words[1].substr(3), 0, LastMessage) : std::nullopt;
				if (!Own)
				{
					return LineError(Number, "a message given anew is named by its number in the map: 'message msgK anew'");
				}
				Read.Message.Anew = static_cast<int32>(*Own);
				Words.erase(Words.begin() + 2);
			}
			Read.Message.Name = Words.size() > 1 ? std::string(Words[1]) : std::string();
			const bool bDisc = Words.size() > 2 && Words[2] == "from";
			if (bDisc)
			{
				const bool bPage = Words.size() == 9 && Words[7] == "page";
				const bool bFieldMap = Words.size() > 3 && Words[3] == "map";
				const bool bDiscShaped = (Words.size() == 7 || bPage) && (bFieldMap || Words[3] == "event") && Words[5] == "message";
				const std::optional<int64> Table = bDiscShaped ? NumberText::NumberWithin(Words[4], 0, bFieldMap ? FieldMapCount - 1 : LastBattleEvent) : std::nullopt;
				const std::optional<int64> DiscMessage = bDiscShaped ? NumberText::NumberWithin(Words[6], 0, LastMessage) : std::nullopt;
				const std::optional<int64> Page = bPage ? NumberText::NumberWithin(Words[8], 1, LastMessage) : std::optional<int64>(0);
				if (!IsName(Words[1]) || !Table || !DiscMessage || !Page)
				{
					return LineError(Number, "a message of the disc's own reads 'message Name from event E message K' or 'message Name from map M message K', "
						"or with 'page P' after it");
				}
				Read.Message.Disc = FFieldMapPatch::FDiscMessage{Number, bFieldMap, static_cast<int32>(*Table), static_cast<int32>(*DiscMessage), static_cast<int32>(*Page)};
				return Read;
			}
			const bool bSized = Words.size() == 5 && Words[2] == "size";
			const std::optional<int64> Width = bSized ? NumberText::NumberWithin(Words[3], 0, 0xFF) : std::nullopt;
			const std::optional<int64> Lines = bSized ? NumberText::NumberWithin(Words[4], 0, 0xFF) : std::nullopt;
			if ((Words.size() != 2 && !bSized) || !IsName(Words[1]) || (bSized && (!Width || !Lines)))
			{
				return LineError(Number, "a message reads 'message Name' or 'message Name size width lines'");
			}
			Read.bText = true;
			Read.Size = bSized ? std::optional<FMesExtent>(FMesExtent{static_cast<int32>(*Width), static_cast<int32>(*Lines)}) : std::nullopt;
			return Read;
		}

		/** "entrance K x N z N layer N camera N facing N": entrance K's number and its record's bytes; none for any other line. */
		TResult<std::optional<std::pair<int32, std::vector<uint8>>>> ReadEntranceLine(int32 Number, std::span<const std::string_view> Words)
		{
			constexpr std::size_t EntranceWords = 12;
			constexpr int64 LastEntrance = 0x1FFF;
			const std::optional<int64> Index = Words.size() == EntranceWords ? NumberText::NumberWithin(Words[1], 0, LastEntrance) : std::nullopt;
			if (Words.size() != EntranceWords || !Index)
			{
				return std::optional<std::pair<int32, std::vector<uint8>>>();
			}
			std::vector<std::string_view> Record(Words.begin(), Words.end());
			Record.erase(Record.begin() + 1);
			// Assembled on its own line, so that an error names the text's.
			TResult<FEventAssembly> Assembled = AssembleEventText(std::string(static_cast<std::size_t>(Number - 1), '\n') + Joined(Record));
			if (!Assembled)
			{
				return Assembled.Error();
			}
			return std::optional<std::pair<int32, std::vector<uint8>>>(std::pair(static_cast<int32>(*Index), std::move(Assembled->Code)));
		}
	}

	TResult<FFieldMapPatch> FFieldMapPatch::Parse(std::string_view Text)
	{
		enum class EReading : uint8
		{
			Lines,
			MessageBrace,
			MessageText,
		};

		FFieldMapPatch Patch;
		EReading Reading = EReading::Lines;
		bool bMap = false;
		int32 OptionLine = 0;
		int32 EncountersLine = 0;
		int32 DiscLine = 0;
		int32 MarkLine = 0;
		int32 FirstAnewLine = 0;
		std::string MessageText;
		bool bMessageBegun = false;
		std::optional<FMesExtent> MessageSize;
		for (int32 Number = 1; !Text.empty(); ++Number)
		{
			const std::size_t End = std::min(Text.find('\n'), Text.size());
			std::string_view Line = Text.substr(0, End);
			Text.remove_prefix(std::min(End + 1, Text.size()));
			if (Line.ends_with('\r'))
			{
				Line.remove_suffix(1);
			}
			std::string_view Script;
			if (Reading == EReading::MessageText)
			{
				if (Line != "}")
				{
					if (!MesCodesOfText(Line))
					{
						return LineError(Number, "a character or a mark the messages lack");
					}
					MessageText += bMessageBegun ? "\n" : "";
					MessageText += Line;
					bMessageBegun = true;
				}
				else
				{
					// Its lines were read one by one, so the whole has codes; its window is the one given, else its text's own.
					std::vector<uint8> Codes = MesCodesOfText(MessageText).value_or(std::vector<uint8>());
					const FMesExtent Extent = MessageSize ? *MessageSize : MesExtentOfCodes(Codes);
					if (Extent.WidthUnits > 0xFF || Extent.Lines > 0xFF)
					{
						return LineError(Number, "the message is wider or taller than a window's size can say: give its size");
					}
					Patch.Messages.back().Message = FAddedMessage{std::move(Codes), static_cast<uint8>(Extent.WidthUnits), static_cast<uint8>(Extent.Lines)};
					Reading = EReading::Lines;
				}
			}
			else if (const std::vector<std::string_view> Words = WordsOf(Line); Reading == EReading::MessageBrace)
			{
				if (Words.size() != 1 || Words.front() != "{")
				{
					return LineError(Number, "a message's text begins after a line of '{'");
				}
				Reading = EReading::MessageText;
			}
			else if (Words.empty())
			{
			}
			else if (ExportMark::IsMark(Line))
			{
				MarkLine = Number;
			}
			else if (Words.front() == "map")
			{
				// "map N", "map N whole", or for a map of its own "map N from M" or "map N over M".
				const bool bOver = Words.size() == 4 && Words[2] == "over";
				const bool bOwn = bOver || (Words.size() == 4 && Words[2] == "from");
				const bool bWhole = Words.size() == 3 && Words[2] == "whole";
				const std::optional<int64> Map = Words.size() == 2 || bOwn || bWhole ? NumberText::NumberWithin(Words[1], 0, 0xFFFF) : std::nullopt;
				if (!Map || bMap)
				{
					return LineError(Number, "'map' takes the map's number, once");
				}
				if (bWhole && *Map >= FieldMapCount)
				{
					return LineError(Number, "a whole map is one of the disc's: 'map N whole', N below 730");
				}
				Patch.bWhole = bWhole;
				if (bOwn)
				{
					const std::optional<int64> Base = NumberText::NumberWithin(Words[3], 0, FieldMapCount - 1);
					if (*Map < FieldMapCount || *Map > LastFieldMap || !Base)
					{
						return LineError(Number, "a map of its own reads 'map N from M' or 'map N over M': N from 730 to 1023, M a map of the disc");
					}
					Patch.BaseMap = static_cast<int32>(*Base);
					Patch.bKeepsBaseScripts = bOver;
				}
				Patch.MapNumber = static_cast<int32>(*Map);
				bMap = true;
			}
			else if (Words.front() == "disc")
			{
				const std::optional<FSha1::FDigest> Identity = Words.size() == 2 ? FSha1::FromHex(Words[1]) : std::nullopt;
				if (!Identity || Patch.WrittenForMap)
				{
					return LineError(Number, "'disc' takes the SHA-1 of the disc's map, once");
				}
				Patch.WrittenForMap = Identity;
				DiscLine = Number;
			}
			else if (Words.front() == "option")
			{
				if (Words.size() != 2 || !Patch.OptionName.empty())
				{
					return LineError(Number, "'option' takes the setting's name, once");
				}
				Patch.OptionName = Words[1];
				OptionLine = Number;
			}
			else if (Words.front() == "encounters")
			{
				const std::optional<int64> From = Words.size() == 3 && Words[1] == "from" ? NumberText::NumberWithin(Words[2], 0, FieldMapCount - 1) : std::nullopt;
				if (!From || Patch.EncountersMap)
				{
					return LineError(Number, "'encounters from M' takes a map of the disc, once");
				}
				Patch.EncountersMap = static_cast<int32>(*From);
				EncountersLine = Number;
			}
			else if (Words.front() == "message")
			{
				TResult<FMessageLine> Read = ReadMessageLine(Number, Words);
				if (!Read)
				{
					return Read.Error();
				}
				const auto Twice = [&Read](const FNamedMessage& Message)
					{ return Message.Name == Read->Message.Name || (Read->Message.Anew && Message.Anew == Read->Message.Anew); };
				if (std::any_of(Patch.Messages.begin(), Patch.Messages.end(), Twice))
				{
					return LineError(Number, "a message named twice");
				}
				if (Read->Message.Anew && !FirstAnewLine)
				{
					FirstAnewLine = Number;
				}
				Patch.Messages.push_back(std::move(Read->Message));
				if (Read->bText)
				{
					MessageSize = Read->Size;
					MessageText.clear();
					bMessageBegun = false;
					Reading = EReading::MessageBrace;
				}
			}
			else if (Words.front() == "entrance" && Words.size() > 1 && NumberText::Parse(Words[1]))
			{
				TResult<std::optional<std::pair<int32, std::vector<uint8>>>> Entrance = ReadEntranceLine(Number, Words);
				if (!Entrance || !*Entrance)
				{
					return Entrance ? LineError(Number, "an entrance given anew reads 'entrance K x N z N layer N camera N facing N'") : Entrance.Error();
				}
				if (std::any_of(Patch.Entrances.begin(), Patch.Entrances.end(), [&Entrance](const FEntrance& Other) { return Other.Index == (*Entrance)->first; }))
				{
					return LineError(Number, "an entrance given anew twice");
				}
				Patch.Entrances.push_back({Number, (*Entrance)->first, std::move((*Entrance)->second)});
			}
			else if (Words.front() == "expect" || Words.front() == "detour")
			{
				// "expect @offset instruction", or "detour @offset to Label instruction".
				const bool bDetour = Words.front() == "detour";
				const std::size_t InstructionAt = bDetour ? 4 : 2;
				const std::optional<uint16> Offset = Words.size() > InstructionAt ? OffsetOf(Words[1]) : std::nullopt;
				if (!Offset || (bDetour && (Words[2] != "to" || !IsName(Words[3]))))
				{
					return LineError(Number, bDetour ? "a detour reads 'detour @offset to Label instruction'" : "an expectation reads 'expect @offset instruction'");
				}
				Patch.Sites.push_back({Number, *Offset, Joined(std::span(Words).subspan(InstructionAt)), bDetour ? std::string(Words[3]) : std::string()});
			}
			else
			{
				Script = Line;
			}
			Patch.Assembly += Script;
			Patch.Assembly += '\n';
		}
		if (Reading != EReading::Lines)
		{
			return MakeError("the last message's text does not end with a line of '}'");
		}
		if (!bMap)
		{
			return MakeError("the text names no map");
		}
		if (!Patch.bWhole && MarkLine != 0)
		{
			return LineError(MarkLine, "only a whole map carries an export's mark");
		}
		if (Patch.WrittenForMap && (Patch.BaseMap || Patch.MapNumber >= FieldMapCount))
		{
			return LineError(DiscLine, "only a disc map's patch or whole map names the disc's map it was written for");
		}
		if (!Patch.Entrances.empty() && (!Patch.WrittenForMap || Patch.bWhole))
		{
			// The table's length is not recorded: only the map's identity tells that the record is one of its entrances.
			return LineError(Patch.Entrances.front().Line, "an entrance is given anew only by a patch that names the disc's map it was written for ('disc')");
		}
		if (FirstAnewLine != 0 && (Patch.bWhole || (Patch.BaseMap && !Patch.bKeepsBaseScripts)))
		{
			return LineError(FirstAnewLine, "a map built whole numbers its messages in their order: none is given anew");
		}
		if (Patch.bWhole)
		{
			if (!Patch.WrittenForMap)
			{
				return MakeError("a whole map names the disc's map it was written for: 'disc' and its SHA-1");
			}
			if (!Patch.Sites.empty())
			{
				return LineError(Patch.Sites.front().Line, "a whole map gives every script itself, and expects or detours none of the disc's");
			}
			if (!Patch.OptionName.empty())
			{
				return LineError(OptionLine, "a whole map is not added under a setting");
			}
			if (Patch.EncountersMap)
			{
				return LineError(EncountersLine, "a whole map keeps the disc map's encounters");
			}
		}
		if (Patch.BaseMap)
		{
			if (!Patch.Sites.empty() && !Patch.bKeepsBaseScripts)
			{
				return LineError(Patch.Sites.front().Line, "a map made from a disc map has none of its instructions to expect or detour");
			}
			if (!Patch.OptionName.empty())
			{
				return LineError(OptionLine, "a map of its own is not added under a setting");
			}
		}
		else if (Patch.EncountersMap)
		{
			return LineError(EncountersLine, "only a map of its own takes another map's encounters");
		}
		return Patch;
	}

	std::vector<std::pair<std::string, std::string>> FFieldMapPatch::MessageDefinitions(int32 First) const
	{
		std::vector<std::pair<std::string, std::string>> Definitions;
		int64 Number = First;
		for (const FNamedMessage& Message : Messages)
		{
			if (Message.Anew)
			{
				continue;
			}
			std::string Operand = "msg";
			NumberText::Append(Operand, Number++);
			Definitions.emplace_back(Message.Name, std::move(Operand));
		}
		return Definitions;
	}

	bool FFieldMapPatch::PutEntrances(FEventScripts& Scripts) const
	{
		if (Entrances.empty())
		{
			return true;
		}
		// The table ends before the first instruction a script reaches, at the latest.
		const FEventThreads Threads = FollowEventThreads(Scripts);
		const std::size_t TableEnd = Threads.Reached.empty() ? Scripts.CodeSize() : Threads.Reached.front();
		for (const FEntrance& Entrance : Entrances)
		{
			const std::size_t At = 1 + static_cast<std::size_t>(Entrance.Index) * EntranceBytes;
			if (Scripts.CodeSize() == 0 || Scripts.Code()[0] != EntranceMark || At + EntranceBytes > TableEnd
				|| !Scripts.Replace(At, std::span<const uint8>(Scripts.Code() + At, EntranceBytes), Entrance.Bytes))
			{
				return false;
			}
		}
		return true;
	}

	TResult<bool> FFieldMapPatch::AddedMessages(const IDiscText* Disc, std::vector<FAddedMessage>& Out) const
	{
		for (const FNamedMessage& Message : Messages)
		{
			if (!Message.Disc)
			{
				Out.push_back(Message.Message);
				continue;
			}
			const FDiscMessage& Named = *Message.Disc;
			if (Disc == nullptr)
			{
				return LineError(Named.Line, "a message of the disc's own, and no disc's text to read it from");
			}
			const TResult<FMessageTable> Table = FMessageTable::Parse(Named.bFieldMap ? Disc->FieldMapMessages(Named.Table) : Disc->BattleEventMessages(Named.Table));
			if (!Table || Named.Message >= Table->Count() || Table->StartsPastBytes(Named.Message))
			{
				return false;
			}
			const bool bOnePage = Named.Page != 0;
			std::optional<std::vector<uint8>> Codes = MesPagesOfCodes(Table->String(Named.Message), bOnePage ? Named.Page : 1, bOnePage ? Named.Page : EveryPage);
			if (!Codes)
			{
				return false;
			}
			// A field map's table sizes its windows (a battle event's scripts size theirs); a page has no size but its own.
			if (Named.bFieldMap && !bOnePage)
			{
				Out.push_back({std::move(*Codes), static_cast<uint8>(Table->WidthUnits(Named.Message)), static_cast<uint8>(Table->Lines(Named.Message))});
				continue;
			}
			const FMesExtent Extent = MesExtentOfCodes(*Codes);
			if (Extent.WidthUnits > 0xFF || Extent.Lines > 0xFF)
			{
				return LineError(Named.Line, "the disc's message is wider or taller than a window's size can say");
			}
			Out.push_back({std::move(*Codes), static_cast<uint8>(Extent.WidthUnits), static_cast<uint8>(Extent.Lines)});
		}
		return true;
	}

	TResult<FFieldMapOfItsOwn> FFieldMapPatch::Build(const IDiscText* Disc) const
	{
		if ((!BaseMap && !bWhole) || bKeepsBaseScripts)
		{
			return MakeError("a patch is added to its map, not built as one");
		}
		std::vector<FAddedMessage> Own;
		const TResult<bool> Read = AddedMessages(Disc, Own);
		if (!Read)
		{
			return Read.Error();
		}
		if (!*Read)
		{
			return MakeError("the disc lacks a message the map names of its own");
		}
		std::optional<std::vector<uint8>> Table = BuildMessageTable(Own);
		if (!Table)
		{
			return MakeError("the map's messages do not make a message table");
		}
		const std::vector<std::pair<std::string, std::string>> Definitions = MessageDefinitions(0);
		FEventAssemblyContext Context;
		Context.Definitions = Definitions;
		const TResult<FEventAssembly> Assembled = AssembleEventText(Assembly, Context);
		if (!Assembled)
		{
			return Assembled.Error();
		}
		TResult<std::vector<uint8>> Section = BuildEventScriptSection(*Assembled);
		if (!Section)
		{
			return Section.Error();
		}
		return FFieldMapOfItsOwn{std::move(*Section), std::move(*Table)};
	}

	TResult<bool> FFieldMapPatch::Apply(FEventScripts& Scripts, std::vector<uint8>& Table, const IDiscText* Disc, const FSha1::FDigest* Identity) const
	{
		if (BaseMap && !bKeepsBaseScripts)
		{
			return MakeError("a map made from a disc map is built, not added to another map");
		}
		if (bWhole)
		{
			return MakeError("a whole map is built in place of the disc's, not added to it");
		}
		if (WrittenForMap && (Identity == nullptr || *Identity != *WrittenForMap))
		{
			return false;
		}
		const std::span<const uint8> Code(Scripts.Code(), Scripts.CodeSize());
		for (const FSite& Site : Sites)
		{
			// Assembled where it lies, on the line it is written on, so that an error names the text's line.
			FEventAssemblyContext There;
			There.Origin = Site.Offset;
			const TResult<FEventAssembly> Expected = AssembleEventText(std::string(static_cast<std::size_t>(Site.Line - 1), '\n') + Site.Instruction, There);
			if (!Expected)
			{
				return Expected.Error();
			}
			if (!Site.Label.empty() && Expected->Code.size() < JumpBytes)
			{
				return LineError(Site.Line, "a detour takes an instruction of three bytes or more");
			}
			if (Site.Offset > Code.size() || Code.size() - Site.Offset < Expected->Code.size()
				|| !std::equal(Expected->Code.begin(), Expected->Code.end(), Code.begin() + Site.Offset))
			{
				return false;
			}
		}
		std::vector<FAddedMessage> Given;
		const TResult<bool> Read = AddedMessages(Disc, Given);
		if (!Read)
		{
			return Read.Error();
		}
		if (!*Read)
		{
			return false;
		}
		// The messages given anew take their numbers' places; the others follow the map's own.
		std::vector<std::pair<int32, FAddedMessage>> Anew;
		std::vector<FAddedMessage> Added;
		for (std::size_t Message = 0; Message < Messages.size(); ++Message)
		{
			if (Messages[Message].Anew)
			{
				Anew.emplace_back(*Messages[Message].Anew, std::move(Given[Message]));
			}
			else
			{
				Added.push_back(std::move(Given[Message]));
			}
		}
		std::vector<uint8> NewTable = Table;
		if (!Anew.empty())
		{
			const TResult<FMessageTable> Own = FMessageTable::Parse(Table);
			if (!Own || std::any_of(Anew.begin(), Anew.end(), [&Own](const auto& Message) { return Message.first >= Own->Count(); }))
			{
				return false;
			}
			std::optional<std::vector<uint8>> Replaced = ReplaceMessages(Table, Anew);
			if (!Replaced)
			{
				return MakeError("the map's message table cannot take the messages given anew");
			}
			NewTable = std::move(*Replaced);
		}
		const std::optional<int32> First = AppendMessages(NewTable, Added);
		if (!First)
		{
			return MakeError("the map's message table cannot take the patch's messages");
		}
		const std::vector<std::pair<std::string, std::string>> Definitions = MessageDefinitions(*First);
		if (Code.size() > 0xFFFF)
		{
			return MakeError("the map's bytecode leaves no room after it");
		}
		FEventAssemblyContext After;
		After.Origin = static_cast<uint16>(Code.size());
		After.Definitions = Definitions;
		const TResult<FEventAssembly> Assembled = AssembleEventText(Assembly, After);
		if (!Assembled)
		{
			return Assembled.Error();
		}
		if (Assembled->Actors)
		{
			return MakeError("a patch does not change a map's actor count");
		}
		FEventScripts Patched = Scripts;
		if (!Patched.Append(Assembled->Code))
		{
			return MakeError("the map's bytecode cannot take the patch's");
		}
		for (const FSite& Site : Sites)
		{
			if (Site.Label.empty())
			{
				continue;
			}
			const std::optional<uint16> Target = Assembled->Label(Site.Label);
			if (!Target)
			{
				return LineError(Site.Line, "no label is named '" + Site.Label + "'");
			}
			const std::array<uint8, JumpBytes> Jump = {static_cast<uint8>(EventOpcode::Jump), static_cast<uint8>(*Target), static_cast<uint8>(*Target >> 8)};
			Patched.Replace(Site.Offset, Code.subspan(Site.Offset, JumpBytes), Jump);
		}
		for (const FEventEntryPoint& Entry : Assembled->EntryPoints)
		{
			if (Entry.Actor >= Patched.ActorCount())
			{
				return MakeError("a script of actor " + std::to_string(Entry.Actor) + ", which the map does not have");
			}
			Patched.SetEntryPoint(Entry.Actor, Entry.Script, Entry.Offset);
		}
		for (const uint16 Address : Assembled->UnsignedVariables)
		{
			Patched.SetVariableUnsigned(Address);
		}
		if (!PutEntrances(Patched))
		{
			return false;
		}
		Scripts = std::move(Patched);
		Table = std::move(NewTable);
		return true;
	}

	namespace
	{
		/** A map's messages, each named as its scripts name it with its window, then its script section's listing. */
		TResult<std::string> WriteFieldMapSections(std::span<const uint8> EventScripts, std::span<const uint8> Messages)
		{
			const TResult<FEventScripts> Scripts = FEventScripts::Parse(EventScripts);
			const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
			if (!Scripts || !Table)
			{
				return MakeError("the map's scripts or messages cannot be read");
			}
			std::string Text;
			for (int32 Message = 0; Message < Table->Count(); ++Message)
			{
				Text += "\nmessage msg";
				NumberText::Append(Text, Message);
				Text += " size ";
				NumberText::Append(Text, Table->WidthUnits(Message));
				Text += " ";
				NumberText::Append(Text, Table->Lines(Message));
				Text += "\n{\n";
				Text += MesTextOfCodes(Table->String(Message));
				Text += "\n}\n";
			}
			Text += "\n";
			Text += DisassembleEventScripts(*Scripts);
			return Text;
		}
	}

	TResult<std::string> WriteFieldMapText(int32 Number, int32 Base, std::span<const uint8> EventScripts, std::span<const uint8> Messages)
	{
		TResult<std::string> Sections = WriteFieldMapSections(EventScripts, Messages);
		if (!Sections)
		{
			return Sections;
		}
		std::string Text = "map ";
		NumberText::Append(Text, Number);
		Text += " from ";
		NumberText::Append(Text, Base);
		Text += "\n";
		return Text + *Sections;
	}

	TResult<std::string> WriteWholeFieldMapText(int32 Number, std::span<const uint8> EventScripts, std::span<const uint8> Messages)
	{
		const TResult<std::string> Sections = WriteFieldMapSections(EventScripts, Messages);
		const std::optional<FSha1::FDigest> Identity = FieldMapIdentity(EventScripts, Messages);
		if (!Sections || !Identity)
		{
			return MakeError("the map's scripts or messages cannot be read");
		}
		std::string Head = "map ";
		NumberText::Append(Head, Number);
		Head += " whole\ndisc ";
		Head += FSha1::ToHex(*Identity);
		Head += "\n";
		return ExportMark::Marked(Head, *Sections);
	}
}
