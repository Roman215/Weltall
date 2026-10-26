#include "Text/MessageOverrides.h"

#include "Foundation/FileByteSource.h"
#include "Foundation/HiddenFiles.h"
#include "Media/CdImage.h"
#include "Text/MesText.h"
#include "Text/MessageBlocks.h"
#include "Text/MessageTable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>

namespace Weltall
{
	namespace
	{
		constexpr uint64 MostFileBytes = 64ULL * 1024 * 1024;
		/** The marks that put something in at play, which a translation keeps. */
		constexpr std::array<std::string_view, 10> InsertMarks = {"item", "name", "table", "weapon", "table24", "table25", "number", "digits", "signed", "button"};
		constexpr std::array<std::string_view, 4> BattleHelps = {"items", "ethers", "gear-ethers", "specials"};

		std::optional<int32> Number(std::string_view Digits)
		{
			if (Digits.empty() || Digits.size() > 9 || !std::all_of(Digits.begin(), Digits.end(), [](char Digit) { return Digit >= '0' && Digit <= '9'; }))
			{
				return std::nullopt;
			}
			int32 Value = 0;
			for (const char Digit : Digits)
			{
				Value = Value * 10 + (Digit - '0');
			}
			return Value;
		}

		/** Whether Table names one of the game's tables of text (MessageTableIds.h). */
		bool IsGameTable(std::string_view Table)
		{
			const std::size_t Slash = Table.find('/');
			const std::string_view Family = Table.substr(0, Slash);
			const std::string_view Rest = Slash == std::string_view::npos ? std::string_view() : Table.substr(Slash + 1);
			if (Slash == std::string_view::npos)
			{
				return Family == "menu" || Family == "battle" || Family == "battling" || Family == "chapters";
			}
			if (Family == "battle-help")
			{
				return std::find(BattleHelps.begin(), BattleHelps.end(), Rest) != BattleHelps.end();
			}
			constexpr std::array<std::string_view, 6> Numbered = {"field", "names", "menu-help", "enemies", "battle-events", "worldmap"};
			return std::find(Numbered.begin(), Numbered.end(), Family) != Numbered.end() && Number(Rest).has_value();
		}

		/** What in Text keeps it from being the font's codes. */
		std::string Unwritable(std::string_view Text, const FMesCharacters& Characters)
		{
			while (!Text.empty())
			{
				if (Text.front() == '{')
				{
					const std::size_t Close = Text.find('}');
					if (Close == std::string_view::npos)
					{
						return "a { that no } closes";
					}
					const std::string_view Mark = Text.substr(0, Close + 1);
					if (!MesCodesOfText(Mark, Characters))
					{
						return "the mark " + std::string(Mark) + ", which is none of the marks";
					}
					Text.remove_prefix(Close + 1);
					continue;
				}
				if (Text.front() == '\n')
				{
					Text.remove_prefix(1);
					continue;
				}
				const std::optional<FMesCharacterCode> Code = Characters.CodeAt(Text);
				if (!Code)
				{
					const std::size_t Length = Utf8SequenceLength(Text);
					return Length == 0 ? std::string("bytes that are not UTF-8") : "\"" + std::string(Text.substr(0, Length)) + "\", which no glyph of the font is";
				}
				Text.remove_prefix(Code->Bytes);
			}
			return "a part it cannot read";
		}

		struct FMarks
		{
			std::multiset<std::string> Inserts;
			int32 Pages = 1;
			bool bCloses = false;
		};

		FMarks MarksOf(std::string_view Text)
		{
			FMarks Marks;
			for (std::size_t Open = Text.find('{'); Open != std::string_view::npos; Open = Text.find('{', Open + 1))
			{
				const std::size_t Close = Text.find('}', Open);
				if (Close == std::string_view::npos)
				{
					break;
				}
				const std::string_view Words = Text.substr(Open + 1, Close - Open - 1);
				const std::string_view Name = Words.substr(0, Words.find(' '));
				Marks.Pages += Name == "page";
				Marks.bCloses = Marks.bCloses || Name == "close" || Name == "auto";
				if (std::find(InsertMarks.begin(), InsertMarks.end(), Name) != InsertMarks.end())
				{
					Marks.Inserts.insert("{" + std::string(Words) + "}");
				}
			}
			return Marks;
		}

		/** How the player's marks differ from the disc's message's, empty where they agree. */
		std::string MarkDifferences(const FMarks& Disc, const FMarks& Player)
		{
			std::vector<std::string> Parts;
			std::vector<std::string> Missing;
			std::vector<std::string> Added;
			std::set_difference(Disc.Inserts.begin(), Disc.Inserts.end(), Player.Inserts.begin(), Player.Inserts.end(), std::back_inserter(Missing));
			std::set_difference(Player.Inserts.begin(), Player.Inserts.end(), Disc.Inserts.begin(), Disc.Inserts.end(), std::back_inserter(Added));
			for (const std::string& Mark : Missing)
			{
				Parts.push_back("it lacks the disc's " + Mark);
			}
			for (const std::string& Mark : Added)
			{
				Parts.push_back("it has a " + Mark + " the disc's lacks");
			}
			if (Disc.Pages != Player.Pages)
			{
				Parts.push_back("it has " + std::to_string(Player.Pages) + " pages where the disc's has " + std::to_string(Disc.Pages));
			}
			if (Disc.bCloses != Player.bCloses)
			{
				Parts.push_back(Disc.bCloses ? "the disc's ends by itself ({close} or {auto}) and it doesn't" : "it ends by itself ({close} or {auto}) and the disc's doesn't");
			}
			std::string Joined;
			for (const std::string& Part : Parts)
			{
				Joined += (Joined.empty() ? "" : "; ") + Part;
			}
			return Joined;
		}

		/** One of a window's sizes for the player's text: its own, with the room the disc's left past its text; a smaller one kept. */
		int32 Remeasured(int32 Disc, int32 DiscText, int32 PlayerText)
		{
			return Disc >= DiscText ? PlayerText + (Disc - DiscText) : Disc;
		}

		std::string Size(int32 WidthUnits, int32 Lines)
		{
			return std::to_string(WidthUnits) + " x " + std::to_string(Lines);
		}

		bool IsTextFile(const std::filesystem::path& Path)
		{
			std::string Extension = Path.extension().string();
			std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
			return Extension == ".txt";
		}
	}

	void FReplacedMessages::CheckWindow(int32 Message, std::span<const uint8> Codes, int32 WidthUnits, int32 Lines) const
	{
		const auto Found = std::lower_bound(Messages.begin(), Messages.end(), Message);
		if (!Overrides || Found == Messages.end() || *Found != Message)
		{
			return;
		}
		const FMesExtent Disc = DiscExtents[static_cast<std::size_t>(Found - Messages.begin())];
		const FMesExtent Extent = Overrides->Extent(Codes);
		if ((Extent.WidthUnits > WidthUnits && Disc.WidthUnits <= WidthUnits) || (Extent.Lines > Lines && Disc.Lines <= Lines))
		{
			Overrides->ReportOnce(Table, Message, "is " + Size(Extent.WidthUnits, Extent.Lines) + ", larger than the " + Size(WidthUnits, Lines)
				+ " window it opens in: shorten or break its lines");
		}
	}

	void FReplacedMessages::CheckScreenWidth(int32 Message, int32 WidthUnits, int32 MostUnits) const
	{
		if (Overrides && WidthUnits > MostUnits && std::binary_search(Messages.begin(), Messages.end(), Message))
		{
			Overrides->ReportOnce(Table, Message, "opens a window " + std::to_string(WidthUnits) + " wide with what it shows beside its text (a face), where the screen holds "
				+ std::to_string(MostUnits) + ": it runs off the screen's edge. Shorten or break its lines");
		}
	}

	std::shared_ptr<const FMessageOverrides> FMessageOverrides::Parse(std::span<const FMessageFile> Files, const FMesCharacters& Characters, FReport Report,
		FMesGlyphWidths Widths)
	{
		auto Overrides = std::make_shared<FMessageOverrides>();
		Overrides->Report = std::move(Report);
		Overrides->Widths = std::move(Widths);
		const auto Say = [&Overrides](const std::string& Message)
		{
			if (Overrides->Report)
			{
				Overrides->Report(Message);
			}
		};
		for (const FMessageFile& File : Files)
		{
			std::vector<FMessageFileProblem> Problems;
			const std::vector<FMessageBlock> Blocks = ParseMessageBlocks(File.Contents, Problems);
			for (const FMessageFileProblem& Problem : Problems)
			{
				Say(File.Name + " line " + std::to_string(Problem.Line) + ": " + Problem.Message);
			}
			for (const FMessageBlock& Block : Blocks)
			{
				const std::string Where = File.Name + " line " + std::to_string(Block.Line);
				const std::size_t Slash = Block.Id.rfind('/');
				const std::string_view Table = std::string_view(Block.Id).substr(0, Slash == std::string::npos ? 0 : Slash);
				const std::optional<int32> Index = Slash == std::string::npos ? std::nullopt : Number(std::string_view(Block.Id).substr(Slash + 1));
				if (!Index || !IsGameTable(Table))
				{
					Say(Where + ": " + Block.Id + " is no message's id (Docs/Modding.md lists them)");
					continue;
				}
				std::optional<std::vector<uint8>> Codes = MesCodesOfText(Block.Text, Characters);
				if (!Codes)
				{
					Say(Where + ": " + Block.Id + " has " + Unwritable(Block.Text, Characters) + ": the disc's text is kept");
					continue;
				}
				std::map<int32, FOverride>& Messages = Overrides->Tables[std::string(Table)];
				if (const auto Earlier = Messages.find(*Index); Earlier != Messages.end())
				{
					Say(Where + ": " + Block.Id + " was given before, at " + Earlier->second.Where + ": this one is used");
				}
				Messages[*Index] = FOverride{std::move(*Codes), Block.Text, Where};
			}
		}
		return Overrides;
	}

	TResult<std::shared_ptr<const FMessageOverrides>> FMessageOverrides::Read(const std::filesystem::path& Folder, const FMesCharacters& Characters, FReport Report,
		FMesGlyphWidths Widths)
	{
		std::error_code Error;
		std::vector<std::filesystem::path> Paths;
		if (std::filesystem::is_directory(Folder, Error))
		{
			for (auto Entry = std::filesystem::recursive_directory_iterator(Folder, Error); !Error && Entry != std::filesystem::recursive_directory_iterator();
				Entry.increment(Error))
			{
				if (IsHiddenFile(Entry->path()) || (Entry.depth() == 0 && Entry->is_directory(Error) && Entry->path().filename() == OriginalsFolder))
				{
					Entry.disable_recursion_pending();
					continue;
				}
				if (!Entry->is_symlink(Error) && Entry->is_regular_file(Error) && IsTextFile(Entry->path()))
				{
					Paths.push_back(Entry->path());
				}
			}
			if (Error)
			{
				return MakeError("cannot list the text overrides: " + Error.message());
			}
		}
		std::sort(Paths.begin(), Paths.end());
		std::vector<FMessageFile> Files;
		for (const std::filesystem::path& Path : Paths)
		{
			const std::string Name = Path.lexically_relative(Folder).generic_string();
			const auto File = FFileByteSource::Open(Path);
			if (!File || (*File)->Size() > MostFileBytes)
			{
				return MakeError("cannot read the text override " + Name);
			}
			std::string Contents(static_cast<std::size_t>((*File)->Size()), '\0');
			if (!(*File)->Read(0, std::span<uint8>(reinterpret_cast<uint8*>(Contents.data()), Contents.size())))
			{
				return MakeError("cannot read the text override " + Name);
			}
			Files.push_back({Name, std::move(Contents)});
		}
		return Parse(Files, Characters, std::move(Report), std::move(Widths));
	}

	std::optional<FAppliedMessages> FMessageOverrides::Apply(const std::shared_ptr<const FMessageOverrides>& Overrides, std::string_view Table,
		std::span<const uint8> Bytes, EMessageWindows Windows)
	{
		if (!Overrides)
		{
			return std::nullopt;
		}
		const auto Found = Overrides->Tables.find(Table);
		const TResult<FMessageTable> Parsed = FMessageTable::Parse(Bytes);
		if (Found == Overrides->Tables.end() || !Parsed)
		{
			return std::nullopt;
		}
		const int32 Count = Parsed->Count();
		std::vector<std::span<const uint8>> DiscMessages;
		FMesExtent Largest{0, 0};
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const std::span<const uint8> String = Parsed->String(Index);
			DiscMessages.push_back(String.first(MesMessageSize(String)));
			const FMesExtent Extent = MesExtentOfCodes(DiscMessages.back());
			Largest = {std::max(Largest.WidthUnits, Extent.WidthUnits), std::max(Largest.Lines, Extent.Lines)};
		}
		std::vector<FAddedMessage> Messages;
		std::vector<int32> Replaced;
		std::vector<FMesExtent> ReplacedExtents;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const std::span<const uint8> Original = DiscMessages[static_cast<std::size_t>(Index)];
			FAddedMessage Message{std::vector<uint8>(Original.begin(), Original.end()), static_cast<uint8>(Parsed->WidthUnits(Index)),
				static_cast<uint8>(Parsed->Lines(Index))};
			const auto Override = Found->second.find(Index);
			if (Override == Found->second.end())
			{
				Messages.push_back(std::move(Message));
				continue;
			}
			const std::vector<uint8>& Codes = Override->second.Codes;
			if (const std::string Differences = MarkDifferences(MarksOf(MesTextOfCodes(Original)), MarksOf(Override->second.Text)); !Differences.empty())
			{
				Overrides->ReportOnce(Table, Index, Differences + " (it is used)");
			}
			const FMesExtent Before = MesExtentOfCodes(Original);
			const FMesExtent After = Overrides->Extent(Codes);
			const int32 Width = Remeasured(Message.WidthUnits, Before.WidthUnits, After.WidthUnits);
			const int32 Lines = Remeasured(Message.Lines, Before.Lines, After.Lines);
			const bool bTooLarge = Windows == EMessageWindows::Table
				&& (Width > std::max<int32>(MostTableWidthUnits, Message.WidthUnits) || Lines > std::max<int32>(MostTableLines, Message.Lines));
			if (bTooLarge)
			{
				Overrides->ReportOnce(Table, Index, "needs a window of " + Size(Width, Lines) + ", larger than the field draws (" + Size(MostTableWidthUnits, MostTableLines)
					+ ") and the disc's (" + Size(Message.WidthUnits, Message.Lines) + "): the disc's text is kept");
				Messages.push_back(std::move(Message));
				continue;
			}
			if (Windows == EMessageWindows::Program && (After.WidthUnits > Largest.WidthUnits || After.Lines > Largest.Lines))
			{
				Overrides->ReportOnce(Table, Index, "is " + Size(After.WidthUnits, After.Lines) + ", larger than any of its table on the disc ("
					+ Size(Largest.WidthUnits, Largest.Lines) + "): it may not fit where the game shows it");
			}
			if (Windows == EMessageWindows::Table)
			{
				Message.WidthUnits = static_cast<uint8>(Width);
				Message.Lines = static_cast<uint8>(Lines);
			}
			Message.Codes = Codes;
			Messages.push_back(std::move(Message));
			Replaced.push_back(Index);
			ReplacedExtents.push_back(Before);
		}
		for (auto Past = Found->second.lower_bound(Count); Past != Found->second.end(); ++Past)
		{
			Overrides->ReportOnce(Table, Past->first, "names no message: the table has " + std::to_string(Count));
		}
		if (Replaced.empty())
		{
			return std::nullopt;
		}
		std::optional<std::vector<uint8>> Built = BuildMessageTable(Messages);
		if (!Built)
		{
			Overrides->ReportOnce(Table, Replaced.front(), "makes its table larger than the 64 KiB a table can hold: the disc's text of the table is kept");
			return std::nullopt;
		}
		return FAppliedMessages{std::move(*Built), FReplacedMessages{Overrides, std::string(Table), std::move(Replaced), std::move(ReplacedExtents)}};
	}

	std::size_t FMessageOverrides::Count() const
	{
		std::size_t Total = 0;
		for (const auto& [Table, Messages] : Tables)
		{
			Total += Messages.size();
		}
		return Total;
	}

	const FMessageOverrides::FOverride* FMessageOverrides::Find(std::string_view Table, int32 Index) const
	{
		const auto Found = Tables.find(Table);
		if (Found == Tables.end())
		{
			return nullptr;
		}
		const auto Message = Found->second.find(Index);
		return Message != Found->second.end() ? &Message->second : nullptr;
	}

	const std::string* FMessageOverrides::Text(std::string_view Table, int32 Index) const
	{
		const FOverride* Override = Find(Table, Index);
		return Override != nullptr ? &Override->Text : nullptr;
	}

	std::vector<int32> FMessageOverrides::Messages(std::string_view Table) const
	{
		std::vector<int32> Numbers;
		if (const auto Found = Tables.find(Table); Found != Tables.end())
		{
			for (const auto& [Index, Override] : Found->second)
			{
				Numbers.push_back(Index);
			}
		}
		return Numbers;
	}

	void FMessageOverrides::ReportOnce(std::string_view Table, int32 Index, const std::string& Message) const
	{
		const FOverride* Override = Find(Table, Index);
		const std::string Line = std::string(Table) + "/" + std::to_string(Index) + (Override != nullptr ? " (" + Override->Where + ")" : "") + ": " + Message;
		{
			const std::lock_guard Lock(Guard);
			if (!Reported.insert(Line).second)
			{
				return;
			}
		}
		if (Report)
		{
			Report(Line);
		}
	}

	std::optional<FAppliedMessages> ApplyMessageOverrides(const FCdImage& Image, std::string_view Table, std::span<const uint8> Bytes, EMessageWindows Windows)
	{
		return FMessageOverrides::Apply(Image.MessageOverrides(), Table, Bytes, Windows);
	}
}
