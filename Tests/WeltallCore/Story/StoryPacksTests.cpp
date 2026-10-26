#include "Field/EventAssembly.h"
#include "Story/StoryPacks.h"
#include "Support/AppleDouble.h"
#include "Text/MesText.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>

using namespace Weltall;

namespace
{
	/** A map of one actor whose init returns, and one message. */
	struct FMap
	{
		FEventScripts Scripts;
		std::vector<uint8> Messages;
	};

	FMap Map()
	{
		const auto Section = AssembleEventScripts("actors 1\nactor 0 script 0:\n    Return\n");
		REQUIRE(Section);
		auto Scripts = FEventScripts::Parse(*Section);
		REQUIRE(Scripts);
		const std::vector<FAddedMessage> Own = {*MesMessageOfText("Hello.")};
		return {std::move(*Scripts), *BuildMessageTable(Own)};
	}

	/** A document of map Number that adds a byte of code after the map's own, under Option when it is given. */
	std::string Addition(int32 Number, std::string_view Option = {})
	{
		std::string Text = "map " + std::to_string(Number) + "\n";
		if (!Option.empty())
		{
			Text += "option " + std::string(Option) + "\n";
		}
		return Text + "expect @0x0000 Return\nAdded:\n    End\n";
	}

	/** How many bytes the packs add to the map's code as map Number, with Options, in a game that plays the second disc expanded or not. */
	std::size_t Added(const FStoryPacks& Packs, int32 Number, const FEventOptions& Options, bool bExpandedStory = false)
	{
		FMap Target = Map();
		const std::size_t Before = Target.Scripts.CodeSize();
		REQUIRE_FALSE(Packs.ApplyToMap(Number, Target.Scripts, Target.Messages, Options, bExpandedStory));
		return Target.Scripts.CodeSize() - Before;
	}

	/** A fresh folder, removed when the test ends. */
	struct FStoryFolder
	{
		std::filesystem::path Path;

		explicit FStoryFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}

		~FStoryFolder()
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
}

TEST_CASE("A story document is added to the map it is written for, under the setting it names", "[Story]")
{
	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add("additions", "helper.txt", Addition(12, "fix.rps_helper")));
	REQUIRE_FALSE(Packs.Add("extras", "scene.txt", Addition(13)));
	REQUIRE(Packs.Documents().size() == 2);
	CHECK(Packs.Documents()[0].Name == "additions/helper.txt");
	CHECK(Packs.Documents()[1].Name == "extras/scene.txt");

	CHECK(Added(Packs, 12, FEventOptions()) == 1);
	CHECK(Added(Packs, 12, FEventOptions::Original()) == 0);
	// A document under no setting always is.
	CHECK(Added(Packs, 13, FEventOptions::Original()) == 1);
	CHECK(Added(Packs, 14, FEventOptions()) == 0);

	// A map whose instructions are not the document's is left as it is.
	REQUIRE_FALSE(Packs.Add("additions", "other.txt", "map 14\nexpect @0x0000 End\nAdded:\n    End\n"));
	CHECK(Added(Packs, 14, FEventOptions()) == 0);
}

TEST_CASE("A story document that cannot be read or does not fit is refused with its name", "[Story]")
{
	FStoryPacks Packs;
	const auto Unread = Packs.Add("additions", "broken.txt", "map 12\nmap 13\n");
	REQUIRE(Unread);
	CHECK(Unread->Message == "additions/broken.txt: line 2: 'map' takes the map's number, once");
	const auto Unknown = Packs.Add("additions", "unknown.txt", Addition(12, "fix.nothing"));
	REQUIRE(Unknown);
	CHECK(Unknown->Message == "additions/unknown.txt: no setting is named 'fix.nothing'");
	CHECK(Packs.Documents().empty());

	REQUIRE_FALSE(Packs.Add("additions", "unfit.txt", "map 12\nFly #1\n"));
	FMap Target = Map();
	const auto Unfit = Packs.ApplyToMap(12, Target.Scripts, Target.Messages, FEventOptions(), false);
	REQUIRE(Unfit);
	CHECK(Unfit->Message == "additions/unfit.txt: line 2: no instruction is named 'Fly'");
}

TEST_CASE("The story is read from a folder of packs, each a folder of documents, in the order of their names", "[Story]")
{
	const FStoryFolder Folder("WeltallStoryPacks");
	// A checkout may give a document Windows' line ends.
	Folder.Write("extras/b.txt", "map 13\r\nexpect @0x0000 Return\r\nAdded:\r\n    End\r\n");
	Folder.Write("extras/a.txt", Addition(12));
	// A document saved with a byte order mark reads the same.
	Folder.Write("additions/helper.txt", "\xEF\xBB\xBF" + Addition(12, "fix.rps_helper"));
	Folder.Write("additions/notes.md", "Not a document.");
	Folder.Write("loose.txt", Addition(15));
	const auto Packs = FStoryPacks::Read(Folder.Path);
	REQUIRE(Packs);
	REQUIRE(Packs->Documents().size() == 3);
	CHECK(Packs->Documents()[0].Name == "additions/helper.txt");
	CHECK(Packs->Documents()[1].Name == "extras/a.txt");
	CHECK(Packs->Documents()[2].Name == "extras/b.txt");
	CHECK(Added(*Packs, 12, FEventOptions()) == 2);
	CHECK(Added(*Packs, 13, FEventOptions()) == 1);

	Folder.Write("extras/c.txt", "map 1\nmap 2\n");
	const auto Broken = FStoryPacks::Read(Folder.Path);
	REQUIRE_FALSE(Broken);
	CHECK(Broken.Error().Message == "extras/c.txt: line 2: 'map' takes the map's number, once");

	const auto None = FStoryPacks::Read(Folder.Path / "missing");
	REQUIRE(None);
	CHECK(None->Documents().empty());
}

TEST_CASE("The story leaves out the files and folders a leading dot hides, such as macOS's companions of copied files", "[Story]")
{
	const FStoryFolder Folder("WeltallStoryHidden");
	Folder.Write("additions/castle.txt", Addition(12));
	Testing::WriteAppleDoubleCompanion(Folder.Path / "additions" / "castle.txt");
	Folder.Write(".backup/old.txt", Addition(13));
	const auto Packs = FStoryPacks::Read(Folder.Path);
	REQUIRE(Packs);
	REQUIRE(Packs->Documents().size() == 1);
	CHECK(Packs->Documents()[0].Name == "additions/castle.txt");
	CHECK(Added(*Packs, 13, FEventOptions()) == 0);
}

TEST_CASE("A map of its own is made by one document, and the patches of its number are added to it", "[Story]")
{
	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add("extras", "room.txt", "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\n"));
	REQUIRE_FALSE(Packs.Add("extras", "more.txt", Addition(1000)));
	const FStoryDocument* Room = Packs.MapOfItsOwn(1000, false);
	REQUIRE(Room != nullptr);
	CHECK(Room->Name == "extras/room.txt");
	CHECK(Packs.MapOfItsOwn(1001, false) == nullptr);
	CHECK(Packs.MapOfItsOwn(7, false) == nullptr);
	// The map built is a map like any other to the patches written for its number.
	CHECK(Added(Packs, 1000, FEventOptions()) == 1);

	const auto Twice = Packs.Add("extras", "again.txt", "map 1000 from 8\nactors 1\n");
	REQUIRE(Twice);
	CHECK(Twice->Message == "extras/again.txt: map 1000 is made by extras/room.txt already");

	// A map over its base is made by its document too; the field adds that to the base map's scripts, so it is no patch of its own number.
	REQUIRE_FALSE(Packs.Add("extras", "over.txt", "map 1001 over 7\nexpect @0x0000 Return\nAdded:\n    End\n"));
	const FStoryDocument* Over = Packs.MapOfItsOwn(1001, false);
	REQUIRE(Over != nullptr);
	CHECK(Over->Patch.KeepsBaseScripts());
	CHECK(Added(Packs, 1001, FEventOptions()) == 0);
	const auto Again = Packs.Add("extras", "over-again.txt", "map 1001 over 8\n");
	REQUIRE(Again);
	CHECK(Again->Message == "extras/over-again.txt: map 1001 is made by extras/over.txt already");
}

TEST_CASE("A document that begins with a battle event changes that event's text, the expansion's only in a game that plays it", "[Story]")
{
	const std::vector<FAddedMessage> Own = {*MesMessageOfText("Met him again.{page}\nHe stood\nin my way."), *MesMessageOfText("No choice.")};
	const std::vector<uint8> Table = *BuildMessageTable(Own);
	FStoryPacks Packs;
	CHECK_FALSE(Packs.BattleEventMessages(19, Table, true));
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "told.txt", "; Told elsewhere.\n\nbattle event 19\nmessage 0 from page 2\n"));
	CHECK(Packs.Documents().empty());
	REQUIRE(Packs.EventDocuments().size() == 1);
	CHECK(Packs.EventDocuments()[0].Name == "disc2/told.txt");
	CHECK(Packs.EventDocuments()[0].bExpansion);
	CHECK(Packs.DocumentCount() == 1);
	CHECK(Packs.HasExpansion());

	const auto Changed = Packs.BattleEventMessages(19, Table, true);
	REQUIRE(Changed);
	const auto Read = FMessageTable::Parse(*Changed);
	REQUIRE(Read);
	CHECK(MesTextOfCodes(Read->String(0)) == "He stood\nin my way.");
	// Another event's text, and the game on the original's second disc, are the disc's own.
	CHECK_FALSE(Packs.BattleEventMessages(18, Table, true));
	CHECK_FALSE(Packs.BattleEventMessages(19, Table, false));

	// A document of another pack is every game's; one the table does not fit is left out.
	REQUIRE_FALSE(Packs.Add("extras", "other.txt", "battle event 18\nmessage 1 from page 1\n"));
	CHECK(Packs.BattleEventMessages(18, Table, false));
	REQUIRE_FALSE(Packs.Add("extras", "unfit.txt", "battle event 17\nmessage 5 from page 1\n"));
	CHECK_FALSE(Packs.BattleEventMessages(17, Table, false));
	CHECK(Packs.DocumentCount() == 3);

	const auto Unread = Packs.Add("extras", "broken.txt", "battle event\n");
	REQUIRE(Unread);
	CHECK(Unread->Message == "extras/broken.txt: line 1: 'battle event N' takes the event's number, once");
}

TEST_CASE("The expanded second disc's documents are a game's only when it plays the second disc expanded", "[Story]")
{
	FStoryPacks Packs;
	REQUIRE_FALSE(Packs.Add("additions", "always.txt", Addition(12)));
	CHECK_FALSE(Packs.HasExpansion());
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "scene.txt", Addition(12)));
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "room.txt", "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\n"));
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "more.txt", Addition(1000)));
	CHECK(Packs.HasExpansion());
	CHECK_FALSE(Packs.Documents()[0].bExpansion);
	CHECK(Packs.Documents()[1].bExpansion);
	CHECK(Packs.Documents()[1].Name == "disc2/scene.txt");

	// A game with the original's second disc, or with none chosen, has the other packs' documents alone.
	CHECK(Added(Packs, 12, FEventOptions(), false) == 1);
	CHECK(Added(Packs, 12, FEventOptions(), true) == 2);
	CHECK(Packs.MapOfItsOwn(1000, false) == nullptr);
	CHECK(Added(Packs, 1000, FEventOptions(), false) == 0);
	const FStoryDocument* Room = Packs.MapOfItsOwn(1000, true);
	REQUIRE(Room != nullptr);
	CHECK(Room->Name == "disc2/room.txt");
	CHECK(Added(Packs, 1000, FEventOptions(), true) == 1);

	// No other document may make a map the expansion makes, whichever a game plays.
	const auto Twice = Packs.Add("additions", "again.txt", "map 1000 from 8\nactors 1\n");
	REQUIRE(Twice);
	CHECK(Twice->Message == "additions/again.txt: map 1000 is made by disc2/room.txt already");
}
