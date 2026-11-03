#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Mods/ExpansionMods.h"
#include "Story/Disc2Export.h"
#include "Story/ExpansionTrial.h"
#include "Story/ShareableExpansion.h"
#include "Story/StoryLibrary.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using namespace Weltall;

// Make shareable copies an expansion made from Export Disc 2 with only what its author changed or added.

namespace
{
	/** A Mods folder of its own for each test, which the tests run beside one another. */
	struct FModsFolder
	{
		std::filesystem::path Path;

		explicit FModsFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}

		~FModsFolder()
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}
	};

	std::string ReadText(const std::filesystem::path& File)
	{
		std::ifstream Stream(File, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
	}

	void WriteText(const std::filesystem::path& File, std::string_view Text)
	{
		std::filesystem::create_directories(File.parent_path());
		std::ofstream(File, std::ios::binary) << Text;
	}

	/** Text with the words of its message K (a whole document's "msgK" or "message K") given as Line instead. */
	std::string WithMessage(std::string Text, std::string_view Named, std::string_view Line)
	{
		const std::size_t At = Text.find(Named);
		REQUIRE(At != std::string::npos);
		const std::size_t Begin = Text.find("{\n", At) + 2;
		return Text.replace(Begin, Text.find("\n}\n", Begin) - Begin, Line);
	}

	/** The job stepped to its end; its error, if any. */
	std::optional<FError> Make(const FIso9660& Disc, const FGameFileIndex& Index, const std::filesystem::path& Expansion)
	{
		auto Job = FShareableExpansion::Open(Disc, Index, Testing::PortStory(), Expansion);
		REQUIRE(Job);
		for (int32 Step = 0; Step < 1000; ++Step)
		{
			const TResult<bool> Done = (*Job)->Step();
			if (!Done)
			{
				return Done.Error();
			}
			if (*Done)
			{
				CHECK((*Job)->CompletedFiles() == (*Job)->TotalFiles());
				return std::nullopt;
			}
		}
		FAIL("the copy never ends");
		return std::nullopt;
	}

	void ExportInto(const FIso9660& Disc, const FGameFileIndex& Index, const std::filesystem::path& Folder)
	{
		auto Export = FDisc2Export::Open(Disc, Index, Folder);
		REQUIRE(Export);
		for (bool bDone = false; !bDone;)
		{
			const TResult<bool> Done = (*Export)->Step();
			REQUIRE(Done);
			bDone = *Done;
		}
	}

	/** Text with a line added before the reached instruction in the middle of its listing's. */
	std::string WithLineAdded(std::string Text, std::string_view Line)
	{
		std::vector<std::size_t> Lines;
		for (std::size_t At = Text.find("\n    ", Text.find("\nactors ")); At != std::string::npos; At = Text.find("\n    ", At + 1))
		{
			const std::string_view Listed(Text.data() + At + 1, Text.find('\n', At + 1) - At - 1);
			if (Listed.find("; ") != std::string_view::npos && Listed.find("unreached") == std::string_view::npos && !Listed.starts_with("    data"))
			{
				Lines.push_back(At + 1);
			}
		}
		REQUIRE_FALSE(Lines.empty());
		return Text.insert(Lines[Lines.size() / 2], Line);
	}

	std::vector<std::string> FilesIn(const std::filesystem::path& Folder)
	{
		std::vector<std::string> Files;
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(Folder))
		{
			if (Entry.is_regular_file())
			{
				Files.push_back(std::filesystem::relative(Entry.path(), Folder).generic_string());
			}
		}
		std::sort(Files.begin(), Files.end());
		return Files;
	}
}

TEST_CASE("Make shareable copies an edited export with only what its author changed or added", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FModsFolder Mods("WeltallShareableCopy");
	const std::filesystem::path Expansion = Mods.Path / "My Disc 2";
	ExportInto(*Disc, *Index, Expansion);

	// A message of the chair's map and of event 19's table in the author's words, a line of the chair's script added,
	// a document and a note of the author's own.
	const std::filesystem::path Chair = Expansion / "disc2" / "map-617.txt";
	WriteText(Chair, WithLineAdded(WithMessage(ReadText(Chair), "message msg0 size", "My own first words."), "    Yield\n"));
	const std::filesystem::path Event = Expansion / "disc2" / "battle-event-019.txt";
	WriteText(Event, WithMessage(ReadText(Event), "message 0 size", "My own battle words."));
	WriteText(Expansion / "disc2" / "my-scene.txt", "map 1000 from 617\nmessage Hello\n{\nHello.\n}\nactors 1\nactor 0 script 0:\n    Return\n");
	WriteText(Expansion / "README.txt", "Played after the chair.\n");
	WriteText(Expansion / "mod.txt", "name My Disc 2\nauthor A tester\n");

	if (const std::optional<FError> Error = Make(*Disc, *Index, Expansion))
	{
		FAIL(Error->Message);
	}
	const std::filesystem::path Copy = Mods.Path / "My Disc 2 (shareable)";
	CHECK(FilesIn(Copy) == std::vector<std::string>{"README.txt", "disc2/battle-event-019.txt", "disc2/map-617.txt", "disc2/my-scene.txt", "mod.txt"});
	CHECK(ReadText(Copy / "mod.txt") == "name My Disc 2\nauthor A tester\n");
	CHECK(ReadText(Copy / "README.txt") == "Played after the chair.\n");
	CHECK(ReadText(Copy / "disc2" / "my-scene.txt") == ReadText(Expansion / "disc2" / "my-scene.txt"));
	const std::string SharedMap = ReadText(Copy / "disc2" / "map-617.txt");
	CHECK(SharedMap.starts_with("map 617\ndisc "));
	CHECK(SharedMap.find("message msg0 anew size ") != std::string::npos);
	CHECK(SharedMap.find("My own first words.") != std::string::npos);
	CHECK(std::count(SharedMap.begin(), SharedMap.end(), '{') == 1);
	CHECK(SharedMap.size() < ReadText(Chair).size() / 20);
	const std::string SharedEvent = ReadText(Copy / "disc2" / "battle-event-019.txt");
	CHECK(SharedEvent.starts_with("battle event 19\ndisc "));
	CHECK(SharedEvent.find("message 0 anew size ") != std::string::npos);
	CHECK(std::count(SharedEvent.begin(), SharedEvent.end(), '{') == 1);

	// The copy is an expansion that plays, every document fitting the disc, as the edited export does.
	const FStoryLibrary Library(FStoryPacks(), FindExpansionMods(Mods.Path));
	const FStoryLibrary::FExpansion* Shared = Library.Expansion("My Disc 2 (shareable)");
	REQUIRE(Shared != nullptr);
	REQUIRE(Shared->Packs);
	CHECK(Shared->Packs->DocumentCount() == 3);
	const auto Tried = TryExpansion(*Shared->Packs, Disc->Image(), *Index, FEventOptions());
	REQUIRE(Tried);
	CHECK(Tried->ForAnotherDisc.empty());
	CHECK(Tried->Faults.empty());

	// Made again, the copy is replaced whole: a file of the earlier copy's goes with it.
	WriteText(Copy / "disc2" / "stale.txt", "map 5\n");
	WriteText(Event, ReadText(Event) + "; a comment of the author's\n");
	REQUIRE_FALSE(Make(*Disc, *Index, Expansion));
	CHECK_FALSE(std::filesystem::exists(Copy / "disc2" / "stale.txt"));
	CHECK(ReadText(Copy / "disc2" / "battle-event-019.txt") == SharedEvent);
}

TEST_CASE("Make shareable refuses a patch of a map the expansion gives whole, naming both", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FModsFolder Mods("WeltallShareableMixed");
	const std::filesystem::path Expansion = Mods.Path / "Mixed";
	ExportInto(*Disc, *Index, Expansion);
	const std::filesystem::path Chair = Expansion / "disc2" / "map-617.txt";
	WriteText(Chair, WithMessage(ReadText(Chair), "message msg0 size", "My own first words."));
	WriteText(Expansion / "disc2" / "chair-patch.txt", "map 617\nmessage Extra\n{\nMore.\n}\n");
	const std::optional<FError> Error = Make(*Disc, *Index, Expansion);
	REQUIRE(Error);
	CHECK(Error->Message == "Mixed/disc2/chair-patch.txt: it patches map 617, which Mixed/disc2/map-617.txt gives whole, so it cannot be shared: make its change in that one");
	CHECK_FALSE(std::filesystem::exists(Mods.Path / "Mixed (shareable)"));
}

TEST_CASE("Make shareable refuses an expansion whose copy's folder a save could not keep", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FModsFolder Mods("WeltallShareableLong");
	// A save keeps 64 bytes of the folder's name: 52 letters and " (shareable)" fit, 53 do not.
	const std::string Long(53, 'x');
	const std::string Fits(52, 'y');
	std::filesystem::create_directories(Mods.Path / Long / "disc2");
	std::filesystem::create_directories(Mods.Path / Fits / "disc2");
	const auto Refused = FShareableExpansion::Open(*Disc, *Index, Testing::PortStory(), Mods.Path / Long);
	REQUIRE_FALSE(Refused);
	CHECK(Refused.Error().Message == "the copy's folder, " + Long + " (shareable), is longer than a save can keep (64 bytes): shorten the expansion's folder name");
	CHECK(FShareableExpansion::Open(*Disc, *Index, Testing::PortStory(), Mods.Path / Fits));
}
