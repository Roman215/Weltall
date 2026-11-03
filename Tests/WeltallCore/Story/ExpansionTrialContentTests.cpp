#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Story/DiscIdentity.h"
#include "Story/ExpansionTrial.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;

// An expansion's documents tried on the player's second disc as the game puts them in its maps and battle events, so
// that the Modding page can tell which were written for another disc before anyone plays them.

namespace
{
	/** Text with its "disc" line naming no disc's map, and a comment that makes it an edit of the export's. */
	std::string ForNoDisc(std::string Text)
	{
		const std::size_t Identity = Text.find("\ndisc ") + 6;
		REQUIRE(Identity > 6);
		Text.replace(Identity, 40, std::string(40, '0'));
		return Text + "; edited\n";
	}
}

TEST_CASE("The example scenes tried on the first disc, which a game of the second reads its files from, are all written for it", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	REQUIRE(Testing::PortStory().HasExpansion());
	for (const FEventOptions& Options : {FEventOptions(), FEventOptions::Original()})
	{
		const TResult<FExpansionTrial> Trial = TryExpansion(Testing::PortStory(), Disc->Image(), *Index, Options);
		REQUIRE(Trial);
		CHECK(Trial->ForAnotherDisc == std::vector<std::string>{});
		CHECK(Trial->Faults.empty());
	}
}

TEST_CASE("On the second disc alone, the example scenes' maps of their own built on the first disc's maps are another disc's", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const TResult<FExpansionTrial> Trial = TryExpansion(Testing::PortStory(), Disc->Image(), *Index, FEventOptions());
	REQUIRE(Trial);
	// The Mass Driver's airlock, corridor and control room take maps 382, 384 and 369, which only the first disc holds.
	CHECK(Trial->ForAnotherDisc == std::vector<std::string>{"Disc 2 examples/disc2/mass-driver-airlock.txt", "Disc 2 examples/disc2/mass-driver-corridor.txt",
		"Disc 2 examples/disc2/mass-driver-control-room.txt"});
	CHECK(Trial->Faults.empty());
}

TEST_CASE("The second disc's maps and battle events are the first disc's, so what is written for the second fits where a game reads the first", "[Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const auto FirstExecutable = ReadBootExecutable(*First);
	const auto SecondExecutable = ReadBootExecutable(*Second);
	REQUIRE(FirstExecutable);
	REQUIRE(SecondExecutable);
	const auto FirstIndex = FGameFileIndex::Read(*FirstExecutable);
	const auto SecondIndex = FGameFileIndex::Read(*SecondExecutable);
	REQUIRE(FirstIndex);
	REQUIRE(SecondIndex);
	int32 Maps = 0;
	for (int32 Map = 0; Map < FieldMapCount; ++Map)
	{
		if (!HoldsFieldMap(Second->Image(), *SecondIndex, Map))
		{
			continue;
		}
		INFO("map " << Map);
		const auto Own = LoadFieldArchive(Second->Image(), *SecondIndex, Map);
		const auto Firsts = LoadFieldArchive(First->Image(), *FirstIndex, Map);
		REQUIRE(Own);
		REQUIRE(Firsts);
		CHECK(FieldMapIdentity(Own->Section(EFieldSection::EventScripts), Own->Section(EFieldSection::Messages))
			== FieldMapIdentity(Firsts->Section(EFieldSection::EventScripts), Firsts->Section(EFieldSection::Messages)));
		++Maps;
	}
	CHECK(Maps == 205);
	const auto OwnEvents = FBattleEventArchive::Load(Second->Image(), *SecondIndex);
	const auto FirstEvents = FBattleEventArchive::Load(First->Image(), *FirstIndex);
	REQUIRE(OwnEvents);
	REQUIRE(FirstEvents);
	REQUIRE(OwnEvents->Count() == FirstEvents->Count());
	for (int32 Event = 0; Event < OwnEvents->Count(); ++Event)
	{
		INFO("battle event " << Event);
		CHECK(MessageTableIdentity(OwnEvents->DiscMessages(Event)) == MessageTableIdentity(FirstEvents->DiscMessages(Event)));
	}
}

TEST_CASE("Tried on the second disc, an expansion's documents for another disc are named in their maps' and events' order, and a map that stops is told", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	constexpr int32 Tavern = 9;
	constexpr int32 Patched = 616;
	constexpr int32 Whole = 617;
	REQUIRE_FALSE(HoldsFieldMap(Disc->Image(), *Index, Tavern));
	REQUIRE(HoldsFieldMap(Disc->Image(), *Index, Patched));
	const auto Archive = LoadFieldArchive(Disc->Image(), *Index, Whole);
	REQUIRE(Archive);
	const auto WholeText = WriteWholeFieldMapText(Whole, Archive->Section(EFieldSection::EventScripts), Archive->Section(EFieldSection::Messages));
	REQUIRE(WholeText);

	FStoryPacks Story;
	// The port's own documents are not the expansion's to tell, even where they do not fit.
	REQUIRE_FALSE(Story.Add("additions", "own.txt", "map 616\nexpect @0xFFF0 Return\nOwn:\n    End\n"));
	// A disc 1 map the second disc does not hold; a patch that fits; one whose instruction is past the map's code.
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "a-tavern.txt", "map 9\nAdded:\n    End\n"));
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "b-fits.txt", "map 616\nAdded:\n    End\n"));
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "c-unfit.txt", "map 616\nexpect @0xFFF0 Return\nUnfit:\n    End\n"));
	// Disc 2's map given whole, written for no disc's; as the export wrote it, it would not even be read.
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "d-whole.txt", ForNoDisc(*WholeText)));
	// A map of its own over the disc's whose instructions are not the ones it expects, which cannot be played.
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "e-over.txt", "map 1001 over 616\nexpect @0xFFF0 Return\nOver:\n    End\n"));
	// A map of its own whose scripts do not assemble, which stops as it loads on any disc.
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "f-broken.txt", "map 1000 from 616\nactors 1\nactor 0 script 0:\n    NoSuchInstruction\n"));
	// A battle event's message the disc's table lacks, and an event past the disc's.
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "g-event.txt", "battle event 19\nmessage 99 from page 1\n"));
	REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "h-event.txt", "battle event 255\nmessage 0 from page 2\n"));

	const TResult<FExpansionTrial> Trial = TryExpansion(Story, Disc->Image(), *Index, FEventOptions());
	REQUIRE(Trial);
	CHECK(Trial->ForAnotherDisc == std::vector<std::string>{"disc2/a-tavern.txt", "disc2/c-unfit.txt", "disc2/d-whole.txt", "disc2/e-over.txt",
		"disc2/g-event.txt", "disc2/h-event.txt"});
	REQUIRE(Trial->Faults.size() == 1);
	CHECK(Trial->Faults[0].Message.starts_with("field map 1000: disc2/f-broken.txt: "));

	// The whole map written for the disc's own is not another disc's.
	FStoryPacks Own;
	REQUIRE_FALSE(Own.Add(FStoryPacks::ExpansionPack, "whole.txt", *WholeText + "; edited\n"));
	const TResult<FExpansionTrial> OwnTrial = TryExpansion(Own, Disc->Image(), *Index, FEventOptions());
	REQUIRE(OwnTrial);
	CHECK(OwnTrial->ForAnotherDisc.empty());
	CHECK(OwnTrial->Faults.empty());
}
