#include "Field/EventAssembly.h"
#include "Field/FieldMapPatch.h"
#include "Mods/ExpansionMods.h"
#include "Story/StoryLibrary.h"
#include "Support/AppleDouble.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using namespace Weltall;

// The Disc 2 expansions of the Mods folder: each mod's disc2 folder, named by its mod.txt, read beside the port's own
// packs into the library a game takes its story from by the expansion it plays.
namespace
{
	struct FModsFolder
	{
		std::filesystem::path Path;

		explicit FModsFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
			std::filesystem::create_directories(Path);
		}

		~FModsFolder()
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}

		void Write(const std::filesystem::path& File, std::string_view Text) const
		{
			std::filesystem::create_directories((Path / File).parent_path());
			std::ofstream(Path / File, std::ios::binary) << Text;
		}
	};

	/** A map of its own over Lahan's house, numbered Number. */
	std::string Room(int32 Number)
	{
		return "map " + std::to_string(Number) + " from 7\nactors 1\nactor 0 script 0:\n    Return\n";
	}

	/** The port's own packs: one addition, to map 12. */
	FStoryPacks OwnPacks()
	{
		FStoryPacks Own;
		REQUIRE_FALSE(Own.Add("additions", "greeting.txt", "map 12\nAdded:\n    End\n"));
		return Own;
	}
}

TEST_CASE("A mod's mod.txt names it, its author and its version, each a line", "[Story][Mods]")
{
	const FModInfo Info = ParseModInfo("\xEF\xBB\xBF; Shared on the forum.\r\nname   Elly's errand  \r\n\r\nauthor A. Writer\nversion 1.2\nhomepage example\n");
	CHECK(Info.Name == "Elly's errand");
	CHECK(Info.Author == "A. Writer");
	CHECK(Info.Version == "1.2");
	CHECK(ParseModInfo("") == FModInfo{});
	CHECK(ParseModInfo("names Two\nname\n").Name.empty());
}

TEST_CASE("The Mods folder's expansions are its mods with a disc2 folder, in the order of their folders' names", "[Story][Mods]")
{
	const FModsFolder Mods("WeltallExpansionMods");
	Mods.Write("Zeboim/disc2/hall.txt", Room(730));
	Mods.Write("Zeboim/mod.txt", "name The Zeboim hall\nauthor A. Writer\n");
	// No mod.txt: the folder's name is the expansion's.
	Mods.Write("Abel/Disc2/scene.txt", Room(731));
	Mods.Write("PerfectWorks/readme.txt", "Not an expansion.");
	Mods.Write(".old/disc2/scene.txt", Room(732));
	Testing::WriteAppleDoubleCompanion(Mods.Path / "Zeboim");

	const std::vector<FExpansionMod> Found = FindExpansionMods(Mods.Path);
	REQUIRE(Found.size() == 2);
	CHECK(Found[0].Folder == "Abel");
	CHECK(Found[0].Info.Name == "Abel");
	CHECK(std::filesystem::equivalent(Found[0].Documents, Mods.Path / "Abel" / "Disc2"));
	CHECK(Found[1].Folder == "Zeboim");
	CHECK(Found[1].Info.Name == "The Zeboim hall");
	CHECK(Found[1].Info.Author == "A. Writer");
	CHECK(FindExpansionMods(Mods.Path / "missing").empty());
}

TEST_CASE("The story library gives a game the expansion it plays beside the port's own packs, and none it cannot play", "[Story][Mods]")
{
	const FModsFolder Mods("WeltallStoryLibrary");
	Mods.Write("Abel/disc2/scene.txt", Room(730));
	Mods.Write("Abel/disc2/way-back.txt", "way back\nstory 253 to map 617 entrance 2 story 252\n");
	Mods.Write("Broken/disc2/scene.txt", "map 1\nmap 2\n");
	Mods.Write("Empty/disc2/notes.md", "Not a document.");
	Mods.Write("Zeboim/disc2/hall.txt", Room(730));
	Mods.Write(std::string(ExpansionFolderBytes + 1, 'L') + "/disc2/scene.txt", Room(731));

	const FStoryLibrary Library(OwnPacks(), FindExpansionMods(Mods.Path));
	REQUIRE(Library.Expansions().size() == 5);
	CHECK_FALSE(Library.Own().HasExpansion());

	// Each expansion that can be played has the port's own documents and its own; the same map may be each one's.
	const FStoryPacks& Abel = Library.For("Abel");
	CHECK(Abel.HasExpansion());
	REQUIRE(Abel.Documents().size() == 2);
	CHECK(Abel.Documents()[0].Name == "additions/greeting.txt");
	CHECK(Abel.Documents()[1].Name == "Abel/disc2/scene.txt");
	CHECK(Abel.Documents()[1].bExpansion);
	CHECK(Abel.WayBack() != nullptr);
	CHECK(Abel.MapOfItsOwn(730, true) == &Abel.Documents()[1]);
	const FStoryPacks& Zeboim = Library.For("Zeboim");
	REQUIRE(Zeboim.Documents().size() == 2);
	CHECK(Zeboim.Documents()[1].Name == "Zeboim/disc2/hall.txt");
	CHECK(Zeboim.WayBack() == nullptr);

	// One that cannot be read, holds no document, or sits in a folder a save cannot name is kept with why, and plays as none.
	const auto Problem = [&Library](std::size_t Expansion) { return Library.Expansions()[Expansion].Problem ? Library.Expansions()[Expansion].Problem->Message : std::string(); };
	CHECK(Problem(1) == "Broken/disc2/scene.txt: line 2: 'map' takes the map's number, once");
	CHECK(Problem(2) == "its disc2 folder holds no document of the second disc's story");
	CHECK(Problem(3) == "its folder's name is longer than a save can keep (64 bytes)");
	CHECK(Library.Expansion("Broken") == nullptr);
	CHECK(&Library.For("Broken") == &Library.Own());
	CHECK(&Library.For("Missing") == &Library.Own());
	CHECK(&Library.For("") == &Library.Own());

	// The question offers the one the player chose, else the first that can be played; nothing with expansions off.
	FExpansionOptions Options;
	REQUIRE(Library.Offered(Options) != nullptr);
	CHECK(Library.Offered(Options)->Mod.Folder == "Abel");
	Options.Folder = "Zeboim";
	CHECK(Library.Offered(Options)->Mod.Folder == "Zeboim");
	Options.Folder = "Broken";
	CHECK(Library.Offered(Options)->Mod.Folder == "Abel");
	Options.bEnabled = false;
	CHECK(Library.Offered(Options) == nullptr);
	CHECK(FStoryLibrary(OwnPacks(), {}).Offered(FExpansionOptions{}) == nullptr);

	// What trying an expansion on the player's second disc found is kept with it, by its folder.
	FStoryLibrary Tried(OwnPacks(), FindExpansionMods(Mods.Path));
	Tried.SetForAnotherDisc("Zeboim", {"Zeboim/disc2/hall.txt"});
	Tried.SetForAnotherDisc("Missing", {"Missing/disc2/hall.txt"});
	CHECK(Tried.Expansions()[4].ForAnotherDisc == std::vector<std::string>{"Zeboim/disc2/hall.txt"});
	CHECK(Tried.Expansions()[0].ForAnotherDisc.empty());
}

TEST_CASE("An expansion whose documents are all as Export Disc 2 wrote them changes nothing, and plays once one is edited", "[Story][Mods]")
{
	const auto Section = AssembleEventScripts("actors 1\nactor 0 script 0:\n    Return\n");
	REQUIRE(Section);
	const std::vector<FAddedMessage> Own = {*MesMessageOfText("Hello.")};
	const auto Exported = WriteWholeFieldMapText(12, *Section, *BuildMessageTable(Own));
	REQUIRE(Exported);
	const auto Other = WriteWholeFieldMapText(13, *Section, *BuildMessageTable(Own));
	REQUIRE(Other);
	std::string Edited = *Other;
	Edited.replace(Edited.find("Hello."), 6, "Howdy.");

	const FModsFolder Mods("WeltallStoryLibraryExport");
	Mods.Write("Disc 2 (export)/disc2/map-012.txt", *Exported);
	Mods.Write("Edited/disc2/map-012.txt", *Exported);
	Mods.Write("Edited/disc2/map-013.txt", Edited);
	const FStoryLibrary Library(OwnPacks(), FindExpansionMods(Mods.Path));
	REQUIRE(Library.Expansions().size() == 2);

	const FStoryLibrary::FExpansion& Unchanged = Library.Expansions()[0];
	CHECK(Unchanged.bUnchanged);
	CHECK_FALSE(Unchanged.Packs);
	REQUIRE(Unchanged.Problem);
	CHECK(Unchanged.Problem->Message == "its documents are as Export Disc 2 wrote them, which changes nothing until one is edited");
	CHECK(Library.Expansion("Disc 2 (export)") == nullptr);

	const FStoryLibrary::FExpansion& Changed = Library.Expansions()[1];
	CHECK_FALSE(Changed.bUnchanged);
	REQUIRE(Changed.Packs);
	CHECK(Changed.Packs->LeftAsExported() == 1);
	CHECK(Changed.Packs->WholeMap(12, true) == nullptr);
	REQUIRE(Changed.Packs->WholeMap(13, true) != nullptr);
	CHECK(Changed.Packs->WholeMap(13, true)->Name == "Edited/disc2/map-013.txt");
	REQUIRE(Library.Offered(FExpansionOptions{}) != nullptr);
	CHECK(Library.Offered(FExpansionOptions{})->Mod.Folder == "Edited");
}
