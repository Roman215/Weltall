#include "Field/FieldMapShareable.h"

#include "EventAssemblyText.h"
#include "EventScriptMatch.h"
#include "Field/FieldMapPatch.h"
#include "ShareableCode.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <limits>
#include <set>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;
		using namespace EventScriptMatch;

		constexpr int32 EveryPage = std::numeric_limits<int32>::max();

		/** A message as a window shows it: its codes to its end, its window, and whether its table starts it past its bytes. */
		struct FShownMessage
		{
			std::vector<uint8> Codes;
			int32 WidthUnits = 0;
			int32 Lines = 0;
			bool bPastBytes = false;

			bool operator==(const FShownMessage&) const = default;
		};

		FShownMessage ShownMessage(const FMessageTable& Table, int32 Index)
		{
			const std::span<const uint8> String = Table.String(Index);
			const std::span<const uint8> Codes = String.first(MesMessageSize(String));
			return {std::vector<uint8>(Codes.begin(), Codes.end()), Table.WidthUnits(Index), Table.Lines(Index), Table.StartsPastBytes(Index)};
		}

		/** The disc's message K of a whole map's message named msgK, K one of the disc table's Count; none for any other name. */
		std::optional<int32> OwnNumber(const std::string& Name, int32 Count)
		{
			const std::optional<int64> Number = Name.starts_with("msg") ? NumberText::NumberWithin(std::string_view(Name).substr(3), 0, Count - 1) : std::nullopt;
			return Number && Name == "msg" + std::to_string(*Number) ? std::optional<int32>(static_cast<int32>(*Number)) : std::nullopt;
		}

		std::string ClauseOf(const FFieldMapPatch::FDiscMessage& Named)
		{
			std::string Clause = Named.bFieldMap ? "from map " : "from event ";
			Clause += std::to_string(Named.Table) + " message " + std::to_string(Named.Message);
			return Named.Page != 0 ? Clause + " page " + std::to_string(Named.Page) : Clause;
		}

		/** The words that name Shown as one of the disc map's own messages, whole or a page of one, as a patch reads them; none where it is none of them. */
		std::optional<std::string> DiscClauseOf(const FShownMessage& Shown, const FMessageTable& Disc, int32 Map)
		{
			for (int32 Index = 0; Index < Disc.Count() && !Shown.bPastBytes; ++Index)
			{
				const std::string Named = "from map " + std::to_string(Map) + " message " + std::to_string(Index);
				const std::optional<std::vector<uint8>> Whole = Disc.StartsPastBytes(Index) ? std::nullopt : MesPagesOfCodes(Disc.String(Index), 1, EveryPage);
				if (Whole && *Whole == Shown.Codes && Disc.WidthUnits(Index) == Shown.WidthUnits && Disc.Lines(Index) == Shown.Lines)
				{
					return Named;
				}
				// A page takes the window of its own text.
				for (int32 Page = 1; Whole; ++Page)
				{
					const std::optional<std::vector<uint8>> Codes = MesPagesOfCodes(Disc.String(Index), Page, Page);
					if (!Codes)
					{
						break;
					}
					const FMesExtent Extent = MesExtentOfCodes(*Codes);
					if (*Codes == Shown.Codes && Extent.WidthUnits == Shown.WidthUnits && Extent.Lines == Shown.Lines)
					{
						return Named + " page " + std::to_string(Page);
					}
				}
			}
			return std::nullopt;
		}

		/** How the patch numbers and gives the whole map's messages. */
		struct FMessagePlan
		{
			/** Each whole message's number in the patched table: one of the disc's own, or past the disc's count in their order. */
			std::vector<int32> Numbers;
			/** The name of each whole message the patch adds after the disc's own; empty for the disc's own. */
			std::vector<std::string> Names;
			std::string Lines;
		};

		/**
		 * The whole map's messages named msgK stand for the disc's own message K, given anew where they differ from it; the
		 * others follow the disc's own. Each given is named where it is one of the disc's own, else written out.
		 */
		FMessagePlan PlanMessages(const FFieldMapPatch& Whole, const FMessageTable& Built, const FMessageTable& Disc)
		{
			FMessagePlan Plan;
			int32 Added = 0;
			const std::vector<FFieldMapPatch::FNamedMessage>& Named = Whole.NamedMessages();
			for (std::size_t Index = 0; Index < Named.size(); ++Index)
			{
				const FShownMessage Shown = ShownMessage(Built, static_cast<int32>(Index));
				const std::optional<int32> Own = OwnNumber(Named[Index].Name, Disc.Count());
				Plan.Numbers.push_back(Own ? *Own : Disc.Count() + Added++);
				Plan.Names.push_back(Own ? std::string() : Named[Index].Name);
				if (Own && Shown == ShownMessage(Disc, *Own))
				{
					continue;
				}
				Plan.Lines += "message " + Named[Index].Name + (Own ? " anew" : "");
				if (const std::optional<std::string> Clause = Named[Index].Disc ? ClauseOf(*Named[Index].Disc) : DiscClauseOf(Shown, Disc, Whole.Map()))
				{
					Plan.Lines += " " + *Clause + "\n";
					continue;
				}
				Plan.Lines += " size " + std::to_string(Shown.WidthUnits) + " " + std::to_string(Shown.Lines) + "\n{\n" + MesTextOfCodes(Shown.Codes) + "\n}\n";
			}
			return Plan;
		}

		std::string DirectionText(uint8 Direction)
		{
			return Direction == KeepDirection ? std::string(Keep) : std::to_string(Direction);
		}

		std::size_t FirstReached(const FProgram& Program)
		{
			return Program.Steps.empty() ? 0 : Program.Steps.front().Offset;
		}

		/** The lines that give the disc's entrances anew where the edited map's differ; an error where their number differs. */
		TResult<std::string> EntranceLines(const FProgram& Edited, const FProgram& Disc)
		{
			const std::span<const uint8> Mine(Edited.Scripts->Code(), Edited.Scripts->CodeSize());
			const std::span<const uint8> Theirs(Disc.Scripts->Code(), Disc.Scripts->CodeSize());
			const bool bMine = !Mine.empty() && Mine[0] == EntranceMark;
			const bool bTheirs = !Theirs.empty() && Theirs[0] == EntranceMark;
			const std::size_t Count = EntranceRecords(Mine, FirstReached(Edited));
			if (bMine != bTheirs || Count != EntranceRecords(Theirs, FirstReached(Disc)))
			{
				return MakeError("it has " + std::to_string(Count) + " entrances where the disc's map has " + std::to_string(EntranceRecords(Theirs, FirstReached(Disc)))
					+ ": a patch gives the disc's anew, never more or fewer");
			}
			std::string Lines;
			for (std::size_t Record = 0; Record < Count; ++Record)
			{
				const std::size_t At = 1 + Record * EntranceBytes;
				if (std::equal(Mine.begin() + static_cast<std::ptrdiff_t>(At), Mine.begin() + static_cast<std::ptrdiff_t>(At + EntranceBytes), Theirs.begin() + static_cast<std::ptrdiff_t>(At)))
				{
					continue;
				}
				const FEventEntrance Entrance = *Edited.Scripts->Entrance(static_cast<int32>(Record));
				Lines += "entrance " + std::to_string(Record) + " x " + std::to_string(Entrance.X) + " z " + std::to_string(Entrance.Z) + " layer "
					+ std::to_string(Entrance.Layer) + " camera " + DirectionText(Entrance.CameraDirection) + " facing " + DirectionText(Entrance.Facing) + "\n";
			}
			return Lines;
		}

		/** The line that marks the variables the edited map reads unsigned and the disc's does not; an error for one the disc's reads so and the edited map does not. */
		TResult<std::string> UnsignedLine(const FEventScripts& Edited, const FEventScripts& Disc)
		{
			std::string Line;
			for (uint32 Address = 0; Address < FEventScripts::VariableCount * 2; Address += 2)
			{
				if (Disc.IsVariableUnsigned(Address) && !Edited.IsVariableUnsigned(Address))
				{
					std::string Variable;
					AppendHex(Variable, Address, 3);
					return MakeError("it reads var[" + Variable + "] signed where the disc's map reads it unsigned: a patch only marks variables unsigned");
				}
				if (Edited.IsVariableUnsigned(Address) && !Disc.IsVariableUnsigned(Address))
				{
					Line += Line.empty() ? "unsigned" : "";
					Line += " ";
					AppendHex(Line, Address, 3);
				}
			}
			return Line.empty() ? Line : Line + "\n";
		}

		/** Where the patched map does not play as the whole map: a script that parts from it, a message, an entrance or a variable; none where it plays the same. */
		std::optional<std::string> Difference(const FProgram& Edited, const FMessageTable& Built, const FProgram& Patched, const FMessageTable& Table,
			const FMessagePlan& Plan, const FEventScripts& Disc)
		{
			const FEventScripts& Mine = *Edited.Scripts;
			const FEventScripts& Theirs = *Patched.Scripts;
			for (int32 Actor = 0; Actor < Mine.ActorCount(); ++Actor)
			{
				for (int32 Script = 0; Script < FEventScripts::ScriptsPerActor; ++Script)
				{
					// A script that begins where no instruction does, which the patch leaves as the disc's, plays nothing either way.
					const bool bNowhere = Edited.StepAt(Mine.EntryPoint(Actor, Script)) < 0 && Theirs.EntryPoint(Actor, Script) == Disc.EntryPoint(Actor, Script);
					const bool bUsed = IsScriptUsed(Mine, Actor, Script) && !bNowhere;
					const std::optional<uint16> Parts = bUsed ? PartsFrom(Edited, Mine.EntryPoint(Actor, Script), Patched, Theirs.EntryPoint(Actor, Script)) : std::nullopt;
					if ((bUsed != IsScriptUsed(Theirs, Actor, Script) && !bNowhere) || Parts)
					{
						std::string Where;
						AppendHex(Where, Parts ? *Parts : Mine.EntryPoint(Actor, Script), 4);
						return "actor " + std::to_string(Actor) + "'s script " + std::to_string(Script) + " at " + Where;
					}
				}
			}
			for (int32 Message = 0; Message < Built.Count(); ++Message)
			{
				const int32 Number = Plan.Numbers[static_cast<std::size_t>(Message)];
				if (Number >= Table.Count() || ShownMessage(Table, Number) != ShownMessage(Built, Message))
				{
					return "message " + std::to_string(Message);
				}
			}
			const std::span<const uint8> MyCode(Mine.Code(), Mine.CodeSize());
			const std::span<const uint8> TheirCode(Theirs.Code(), Theirs.CodeSize());
			const std::size_t Records = EntranceRecords(MyCode, FirstReached(Edited));
			if (Records != EntranceRecords(TheirCode, FirstReached(Patched)) || !std::equal(MyCode.begin(), MyCode.begin() + static_cast<std::ptrdiff_t>(Records * EntranceBytes),
				TheirCode.begin()))
			{
				return "its entrances";
			}
			for (uint32 Address = 0; Address < FEventScripts::VariableCount * 2; Address += 2)
			{
				if (Mine.IsVariableUnsigned(Address) != Theirs.IsVariableUnsigned(Address))
				{
					return "its unsigned variables";
				}
			}
			return std::nullopt;
		}
	}

	TResult<std::string> WriteShareableFieldMapText(const FFieldMapPatch& Whole, std::span<const uint8> DiscScripts, std::span<const uint8> DiscMessages,
		const IDiscText& Disc, const FEventScripts& Fixed)
	{
		const std::optional<FSha1::FDigest> Identity = FieldMapIdentity(DiscScripts, DiscMessages);
		if (!Whole.IsWhole() || !Identity || Identity != Whole.WrittenFor())
		{
			return MakeError(Whole.IsWhole() ? "it was written for another disc's map than this disc's" : "it does not give a disc map whole");
		}
		const TResult<FFieldMapOfItsOwn> Built = Whole.Build(&Disc);
		if (!Built)
		{
			return Built.Error();
		}
		const TResult<FEventScripts> Mine = FEventScripts::Parse(Built->EventScripts);
		const TResult<FEventScripts> Theirs = FEventScripts::Parse(DiscScripts);
		const TResult<FMessageTable> MyTable = FMessageTable::Parse(Built->Messages);
		const TResult<FMessageTable> TheirTable = FMessageTable::Parse(DiscMessages);
		if (!Mine || !Theirs || !MyTable || !TheirTable)
		{
			return MakeError("its scripts or messages, or the disc map's, cannot be read");
		}
		if (Mine->ActorCount() != Theirs->ActorCount())
		{
			return MakeError("it has " + std::to_string(Mine->ActorCount()) + " script actors where the disc's map has " + std::to_string(Theirs->ActorCount())
				+ ": a patch keeps the disc's");
		}
		const FMessagePlan Plan = PlanMessages(Whole, *MyTable, *TheirTable);
		FKinds Kinds;
		// A message past the whole map's table is the same only where it is past the patched table too, by its number.
		const int32 PatchedCount = TheirTable->Count() + static_cast<int32>(std::count_if(Plan.Names.begin(), Plan.Names.end(), [](const std::string& Name) { return !Name.empty(); }));
		const auto PlannedNumber = [&Plan, PatchedCount](int32 Message)
		{
			const bool bPlanned = Message < static_cast<int32>(Plan.Numbers.size());
			return bPlanned ? Plan.Numbers[static_cast<std::size_t>(Message)] : Message >= PatchedCount ? Message : 0x10000 + Message;
		};
		const auto OwnNumber = [](int32 Message) { return Message; };
		const FProgram Edited = ReadProgram(*Mine, Kinds, PlannedNumber);
		const FProgram DiscProgram = ReadProgram(*Theirs, Kinds, OwnNumber);
		const TResult<std::string> Entrances = EntranceLines(Edited, DiscProgram);
		const TResult<std::string> Unsigned = UnsignedLine(*Mine, *Theirs);
		if (!Entrances || !Unsigned)
		{
			return !Entrances ? Entrances.Error() : Unsigned.Error();
		}

		// The port's own changes to the disc's code are no place for a detour.
		std::vector<uint8> Changed(Theirs->CodeSize(), 0);
		for (std::size_t Byte = 0; Byte < Changed.size(); ++Byte)
		{
			Changed[Byte] = Byte >= Fixed.CodeSize() || Fixed.Code()[Byte] != Theirs->Code()[Byte] ? 1 : 0;
		}
		const FCorrespondence Match = Correspond(Edited, DiscProgram, Changed);
		std::set<std::string> Names(Plan.Names.begin(), Plan.Names.end());
		const auto MessageText = [&Plan, &TheirTable](std::string& Out, int32 Message)
		{
			const bool bPlanned = Message < static_cast<int32>(Plan.Numbers.size());
			const int32 Number = bPlanned ? Plan.Numbers[static_cast<std::size_t>(Message)] : Message;
			Out += bPlanned && Number >= TheirTable->Count() ? Plan.Names[static_cast<std::size_t>(Message)] : "msg" + std::to_string(Number);
		};
		const TResult<std::string> Code = WriteShareableCode(Edited, DiscProgram, Match, MessageText, Names);
		if (!Code)
		{
			return Code.Error();
		}
		const std::string Body = Plan.Lines + *Entrances + *Unsigned + *Code;
		if (Body.empty())
		{
			return std::string();
		}
		const std::string Text = "map " + std::to_string(Whole.Map()) + "\ndisc " + FSha1::ToHex(*Identity) + "\n" + Body;

		// The patch on the disc's map must play as the whole map does, and go on the map as the port leaves it too.
		const TResult<FFieldMapPatch> Written = FFieldMapPatch::Parse(Text);
		if (!Written)
		{
			return MakeError("the patch written for it cannot be read: " + Written.Error().Message);
		}
		FEventScripts Patched = *Theirs;
		std::vector<uint8> Messages(DiscMessages.begin(), DiscMessages.end());
		FEventScripts PortPatched = Fixed;
		std::vector<uint8> PortMessages = Messages;
		const TResult<bool> Applied = Written->Apply(Patched, Messages, &Disc, &*Identity);
		const TResult<bool> PortApplied = Written->Apply(PortPatched, PortMessages, &Disc, &*Identity);
		if (!Applied || !*Applied || !PortApplied || !*PortApplied)
		{
			return !Applied ? Applied.Error() : !PortApplied ? PortApplied.Error() : MakeError("the patch written for it does not go on the disc's map");
		}
		const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
		const FProgram PatchedProgram = ReadProgram(Patched, Kinds, OwnNumber);
		if (const std::optional<std::string> Where = Table ? Difference(Edited, *MyTable, PatchedProgram, *Table, Plan, *Theirs) : std::optional<std::string>("its messages"))
		{
			return MakeError("the patch written for it would not play as it does (" + *Where + ")");
		}
		return Text;
	}
}
