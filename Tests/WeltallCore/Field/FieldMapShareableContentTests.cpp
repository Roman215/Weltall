#include "Field/EventScripts.h"
#include "Field/FieldArchive.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Field/FieldMapShareable.h"
#include "Field/FieldScriptFixes.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

// Make shareable writes an edited whole map as the patch of the disc's map that plays as it does, carrying only what
// changed: every map of the second disc, as exported and as a test edits it.

namespace
{
	constexpr std::string_view Edit = "Edited by a test.";

	/** Where the export's listing has the reached instruction Part / Whole of the way through its reached ones; npos where it has none. */
	std::size_t ReachedInstruction(const std::string& Text, std::size_t Part, std::size_t Whole)
	{
		std::vector<std::size_t> Lines;
		for (std::size_t At = Text.find("\n    ", Text.find("\nactors ")); At != std::string::npos; At = Text.find("\n    ", At + 1))
		{
			const std::string_view Line(Text.data() + At + 1, Text.find('\n', At + 1) - At - 1);
			if (Line.find("; ") != std::string_view::npos && Line.find("unreached") == std::string_view::npos && !Line.starts_with("    data"))
			{
				Lines.push_back(At + 1);
			}
		}
		return Lines.empty() ? std::string::npos : Lines[Lines.size() * Part / Whole];
	}

	/** Text with its message 0's words given as the test's. */
	std::string WithFirstMessageEdited(std::string Text)
	{
		const std::size_t Named = Text.find("message msg0 size");
		if (Named == std::string::npos)
		{
			return Text;
		}
		const std::size_t Begin = Text.find("{\n", Named) + 2;
		return Text.replace(Begin, Text.find("\n}\n", Begin) - Begin, Edit);
	}

	int32 Count(std::string_view Text, std::string_view Word)
	{
		int32 Found = 0;
		for (std::size_t At = Text.find(Word); At != std::string_view::npos; At = Text.find(Word, At + 1))
		{
			++Found;
		}
		return Found;
	}

	/**
	 * Every map Disc holds, as exported and as edited: message 0 in words of the test's, an instruction added before the
	 * middle of the reached ones, the one a third of the way taken away. Each is written as a patch that names the disc's
	 * map, gives the edited message alone and carries a few instructions; the writer checks that each plays as edited.
	 */
	void CheckEveryMap(const FIso9660& Disc, int32 Held)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const FGameDiscText DiscText(Disc.Image(), *Index);
		int32 Maps = 0;
		int32 MostCarried = 0;
		int32 Carried = 0;
		for (int32 Map = 0; Map < FieldMapCount; ++Map)
		{
			const auto Archive = LoadFieldArchive(Disc.Image(), *Index, Map);
			if (!Archive)
			{
				continue;
			}
			INFO("map " << Map);
			++Maps;
			const std::span<const uint8> Scripts = Archive->Section(EFieldSection::EventScripts);
			const std::span<const uint8> Messages = Archive->Section(EFieldSection::Messages);
			const auto Identity = FieldMapIdentity(Scripts, Messages);
			REQUIRE(Identity);

			// The map as the port leaves it: its fixes, and the port's own documents.
			auto Fixed = FEventScripts::Parse(Scripts);
			REQUIRE(Fixed);
			ApplyFieldScriptFixes(Map, *Fixed, FEventOptions());
			std::vector<uint8> FixedMessages(Messages.begin(), Messages.end());
			REQUIRE_FALSE(Testing::PortStory().ApplyToMap(Map, *Fixed, FixedMessages, FEventOptions(), false, &DiscText, &*Identity));

			const auto Exported = WriteWholeFieldMapText(Map, Scripts, Messages);
			REQUIRE(Exported);
			const auto Unedited = FFieldMapPatch::Parse(*Exported);
			REQUIRE(Unedited);
			const auto Nothing = WriteShareableFieldMapText(*Unedited, Scripts, Messages, DiscText, *Fixed);
			if (!Nothing)
			{
				FAIL(Nothing.Error().Message);
			}
			CHECK(Nothing->empty());

			std::string Edited = WithFirstMessageEdited(*Exported);
			if (const std::size_t Middle = ReachedInstruction(Edited, 1, 2); Middle != std::string::npos)
			{
				Edited.insert(Middle, "    Yield\n");
			}
			if (const std::size_t Third = ReachedInstruction(Edited, 1, 3); Third != std::string::npos && Third != ReachedInstruction(Edited, 1, 2))
			{
				Edited.erase(Third, Edited.find('\n', Third) + 1 - Third);
			}
			const auto Whole = FFieldMapPatch::Parse(Edited);
			REQUIRE(Whole);
			const auto Text = WriteShareableFieldMapText(*Whole, Scripts, Messages, DiscText, *Fixed);
			if (!Text)
			{
				FAIL(Text.Error().Message);
			}
			CHECK(Text->starts_with("map " + std::to_string(Map) + "\ndisc " + FSha1::ToHex(*Identity) + "\n"));
			// The edited message alone is written out; none of the disc's other words.
			const bool bMessages = Edited.find("message msg0 size") != std::string::npos;
			CHECK(Count(*Text, "\nmessage ") == (bMessages ? 1 : 0));
			CHECK(Count(*Text, "{\n") == (bMessages ? 1 : 0));
			CHECK((Text->find("\nmessage msg0 anew size ") != std::string::npos) == bMessages);
			CHECK((Text->find(Edit) != std::string::npos) == bMessages);

			const int32 Lines = Count(*Text, "\n    ");
			MostCarried = std::max(MostCarried, Lines);
			Carried += Lines;
		}
		CHECK(Maps == Held);
		// Most edits are reached by a detour next to them; a script that has no room for one is carried from its start.
		CHECK(Carried <= 6 * Maps);
		CHECK(MostCarried <= 48);
	}
}

TEST_CASE("Every map of the second disc, edited, is written as a patch that carries only what changed", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	CheckEveryMap(*Disc, 205);
}

TEST_CASE("Every map of the first disc, edited, is written as a patch that carries only what changed", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMap(*Disc, FieldMapCount);
}
