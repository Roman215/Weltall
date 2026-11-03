#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Field/FieldRuntime.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Mods/ExpansionMods.h"
#include "Story/BattleEventTextPatch.h"
#include "Story/Disc2Export.h"
#include "Story/DiscIdentity.h"
#include "Story/StoryLibrary.h"
#include "Story/StoryPacks.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>

using namespace Weltall;

// Export Disc 2 writes the second disc as an expansion to edit: every map it holds and every battle event's table given
// whole, which changes nothing until edited; edited, a whole map plays in place of the disc's in a game of its expansion.

namespace
{
	struct FExportFolder
	{
		std::filesystem::path Mods;
		std::filesystem::path Path;

		explicit FExportFolder(const char* Name) : Mods(std::filesystem::temp_directory_path() / Name), Path(Mods / "Disc 2 (export)")
		{
			std::error_code Error;
			std::filesystem::remove_all(Mods, Error);
		}

		~FExportFolder()
		{
			std::error_code Error;
			std::filesystem::remove_all(Mods, Error);
		}
	};

	std::string ReadText(const std::filesystem::path& File)
	{
		std::ifstream Stream(File, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
	}

	/** The export of Disc into Folder, stepped to its end. */
	void Export(const FIso9660& Disc, const FGameFileIndex& Index, const std::filesystem::path& Folder)
	{
		auto Job = FDisc2Export::Open(Disc, Index, Folder);
		REQUIRE(Job);
		for (int32 Step = 0;; ++Step)
		{
			REQUIRE(Step < 1000);
			const TResult<bool> Done = (*Job)->Step();
			if (!Done)
			{
				FAIL(Done.Error().Message);
			}
			if (*Done)
			{
				break;
			}
		}
		CHECK((*Job)->CompletedFiles() == (*Job)->TotalFiles());
	}

	/** Text with message Message's text (a whole map's "msgN") given as Line instead. */
	std::string WithMessage(std::string Text, int32 Message, std::string_view Line)
	{
		const std::size_t Named = Text.find("message msg" + std::to_string(Message) + " size");
		REQUIRE(Named != std::string::npos);
		const std::size_t Begin = Text.find("{\n", Named) + 2;
		const std::size_t End = Text.find("\n}\n", Begin);
		return Text.replace(Begin, End - Begin, Line);
	}

	/** The field entering field map Number with Story in a game of story mode Mode, through its entry; the entry's error, if any. */
	std::optional<FError> Enter(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, const FStoryPacks& Story, EStoryMode Mode,
		int32 Number, const std::function<void(const FFieldRuntime&)>& Check)
	{
		Testing::FOracleTickSession Session(Files);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Story = &Story;
		Services.Options = FEventOptions::Original();
		Services.Game->SetStoryMode(Mode);
		FFieldRuntime Runtime(Services);
		if (std::optional<FError> Error = Runtime.EnterMap(Number))
		{
			return Error;
		}
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks; ++Tick)
		{
			const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
			REQUIRE_FALSE(Runtime.Tick(Vblanks));
		}
		Check(Runtime);
		return std::nullopt;
	}
}

TEST_CASE("Export Disc 2 writes every map the second disc holds and every battle event's table, each given back whole and changing nothing", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FExportFolder Folder("WeltallDisc2Export");
	Export(*Disc, *Index, Folder.Path);
	CHECK(ParseModInfo(ReadText(Folder.Path / "mod.txt")).Name == "Disc 2 (export)");

	// Every map the disc holds, written whole: built again, the scripts and messages of the disc's map it names.
	int32 Maps = 0;
	for (int32 Map = 0; Map < FieldMapCount; ++Map)
	{
		const std::filesystem::path File = Folder.Path / "disc2" / ("map-" + std::string(Map < 10 ? "00" : Map < 100 ? "0" : "") + std::to_string(Map) + ".txt");
		INFO("map " << Map);
		const bool bHeld = HoldsFieldMap(Disc->Image(), *Index, Map);
		REQUIRE(std::filesystem::exists(File) == bHeld);
		if (!bHeld)
		{
			continue;
		}
		++Maps;
		const std::string Text = ReadText(File);
		CHECK(ExportMark::LeftAsWritten(Text));
		const auto Whole = FFieldMapPatch::Parse(Text);
		REQUIRE(Whole);
		CHECK(Whole->IsWhole());
		CHECK(Whole->Map() == Map);
		const auto Archive = LoadFieldArchive(Disc->Image(), *Index, Map);
		REQUIRE(Archive);
		CHECK(Whole->WrittenFor() == FieldMapIdentity(Archive->Section(EFieldSection::EventScripts), Archive->Section(EFieldSection::Messages)));
		const auto Built = Whole->Build();
		REQUIRE(Built);
		CHECK(FieldMapIdentity(Built->EventScripts, Built->Messages) == Whole->WrittenFor());
	}
	CHECK(Maps == 205);

	// Every battle event with messages, its table given whole: in place of the disc's, the same table again.
	const auto Events = FBattleEventArchive::Load(Disc->Image(), *Index);
	REQUIRE(Events);
	int32 Tables = 0;
	for (int32 Event = 0; Event < Events->Count(); ++Event)
	{
		const std::filesystem::path File = Folder.Path / "disc2" / ("battle-event-" + std::string(Event < 10 ? "00" : Event < 100 ? "0" : "") + std::to_string(Event) + ".txt");
		INFO("event " << Event);
		const auto Table = FMessageTable::Parse(Events->Messages(Event));
		const bool bWritten = !Table || Table->Count() > 0;
		REQUIRE(std::filesystem::exists(File) == bWritten);
		if (!bWritten)
		{
			continue;
		}
		++Tables;
		const std::string Text = ReadText(File);
		CHECK(ExportMark::LeftAsWritten(Text));
		const auto Whole = FBattleEventTextPatch::Parse(Text);
		REQUIRE(Whole);
		const auto Replaced = Whole->Replace(Events->Messages(Event));
		REQUIRE(Replaced);
		CHECK(MessageTableIdentity(*Replaced) == MessageTableIdentity(Events->Messages(Event)));
	}
	CHECK(Tables > 0);

	// As written, the folder is an expansion that changes nothing; an edited document makes it one that plays.
	const auto Unchanged = [&Folder]()
	{
		const FStoryLibrary Library(FStoryPacks(), FindExpansionMods(Folder.Mods));
		REQUIRE(Library.Expansions().size() == 1);
		return Library.Expansions().front().bUnchanged && Library.Expansion("Disc 2 (export)") == nullptr;
	};
	CHECK(Unchanged());
	const std::filesystem::path Chair = Folder.Path / "disc2" / "map-617.txt";
	const std::string Exported = ReadText(Chair);
	std::ofstream(Chair, std::ios::binary) << WithMessage(Exported, 0, "Howdy, stranger.");
	CHECK_FALSE(Unchanged());
	{
		const FStoryLibrary Library(FStoryPacks(), FindExpansionMods(Folder.Mods));
		const FStoryLibrary::FExpansion* Expansion = Library.Expansion("Disc 2 (export)");
		REQUIRE(Expansion != nullptr);
		CHECK(Expansion->Packs->DocumentCount() == 1);
		CHECK(Expansion->Packs->LeftAsExported() == Maps + Tables - 1);
		REQUIRE(Expansion->Packs->WholeMap(617, true) != nullptr);
	}

	// Exported again, the edited document is kept.
	Export(*Disc, *Index, Folder.Path);
	CHECK(ReadText(Chair) != Exported);
}

TEST_CASE("A map given whole and edited plays in place of the disc's in a game of its expansion, where the disc's map is the one it names", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	constexpr int32 Tavern = 9;
	const auto Archive = LoadFieldArchive(Disc->Image(), Files->Index, Tavern);
	REQUIRE(Archive);
	const auto Text = WriteWholeFieldMapText(Tavern, Archive->Section(EFieldSection::EventScripts), Archive->Section(EFieldSection::Messages));
	REQUIRE(Text);
	const std::string Edited = WithMessage(*Text, 0, "Howdy, stranger.");

	std::string Played;
	std::size_t Code = 0;
	const auto Read = [&Played, &Code](const FFieldRuntime& Runtime)
	{
		Played = MesTextOfCodes(Runtime.Context().Messages.Table.String(0));
		Code = Runtime.Context().Scripts->CodeSize();
	};
	REQUIRE_FALSE(Enter(*Files, Resident, FStoryPacks(), EStoryMode::Expanded, Tavern, Read));
	const std::string DiscOwn = Played;
	const std::size_t DiscCode = Code;
	REQUIRE(DiscOwn != "Howdy, stranger.");

	// The expansion's whole map, with a patch of the port's own added after it as to the disc's.
	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "map-009.txt", Edited));
	REQUIRE_FALSE(Story.Add("additions", "tavern.txt", "map 9\nAdded:\n    End\n"));
	REQUIRE_FALSE(Enter(*Files, Resident, Story, EStoryMode::Expanded, Tavern, Read));
	CHECK(Played == "Howdy, stranger.");
	CHECK(Code == DiscCode + 1);
	for (const EStoryMode Mode : {EStoryMode::Unchosen, EStoryMode::Original})
	{
		REQUIRE_FALSE(Enter(*Files, Resident, Story, Mode, Tavern, Read));
		CHECK(Played == DiscOwn);
		CHECK(Code == DiscCode + 1);
	}

	// Written for another disc's map (its identity not this disc's), it is left out and the disc's own plays.
	std::string Elsewhere = Edited;
	const std::size_t Identity = Elsewhere.find("disc ") + 5;
	Elsewhere.replace(Identity, 40, std::string(40, '0'));
	FStoryPacks Other;
	REQUIRE_FALSE(Other.Add(FStoryPacks::ExpansionPack, "map-009.txt", Elsewhere));
	REQUIRE_FALSE(Enter(*Files, Resident, Other, EStoryMode::Expanded, Tavern, Read));
	CHECK(Played == DiscOwn);
	CHECK(Code == DiscCode);

	// More script actors than the disc's map has is refused as the map loads, naming the document.
	std::string Crowded = Edited;
	const std::size_t Actors = Crowded.find("\nactors ") + 8;
	Crowded.replace(Actors, Crowded.find('\n', Actors) - Actors, "200");
	FStoryPacks TooMany;
	REQUIRE_FALSE(TooMany.Add(FStoryPacks::ExpansionPack, "crowded.txt", Crowded));
	const auto Refused = Enter(*Files, Resident, TooMany, EStoryMode::Expanded, Tavern, Read);
	REQUIRE(Refused);
	CHECK(Refused->Message.find("disc2/crowded.txt: ") != std::string::npos);
}
