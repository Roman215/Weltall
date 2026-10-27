#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * The chair's third session (map 617 by entrance 3, the story at Nisan the refuge's value) tells of Ramsus' assault
	 * on Nisan in stills, then goes on to the Ministry's scene (map 273 by entrance 12) with Fei, Elly and the ninth
	 * character in the party. The expanded second disc leaves the chair for Nisan's town, a map of its own, with
	 * Elly alone and the story at the assault's own value; before the cathedral, another map of its own, the scene
	 * is played, and from it the game goes on to the same Ministry's scene with the same party and story.
	 */
	constexpr uint16 ChairMap = 617;
	constexpr uint16 SessionEntrance = 3;
	constexpr uint16 SessionProgress = 268;
	constexpr uint16 AssaultProgress = 269;
	constexpr uint16 TownMap = 740;
	constexpr uint16 CathedralMap = 741;
	constexpr uint16 SceneEntrance = 1;
	constexpr uint16 MinistryMap = 273;
	constexpr uint16 MinistryEntrance = 12;
	constexpr uint8 Elly = 1;
	constexpr uint8 NoOne = 0xFF;
	/** The saved variables of the field's place and time, not of what the story has done (as StoryContentTests has them). */
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr std::array<uint16, 15> PlaceAndTimeVariables = {MapLeftVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};

	/** A game of the second disc's story mode Mode at the chair's session. */
	std::unique_ptr<Testing::FGameLoopSession> StartAtSession(const FIso9660& Disc, EStoryMode Mode)
	{
		std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(Mode);
		State.SetSavedEventVariable(0, SessionProgress);
		State.SetSavedEventVariable(1, SessionEntrance);
		State.SetFieldEntrance(SessionEntrance);
		Game->EnterField(ChairMap);
		return Game;
	}

	bool PageWaits(const FEventContext& Context)
	{
		return std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bTextWaits; });
	}

	/** Plays the field on, confirming each message, until Done; the field maps played. */
	std::vector<int32> PlayFieldUntil(Testing::FGameLoopSession& Game, const std::function<bool(const FEventContext*)>& Done, int32 FrameLimit)
	{
		constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
		constexpr int32 FramesAPress = 8;
		std::vector<int32> Maps;
		for (int32 Frame = 0;; ++Frame)
		{
			REQUIRE(Frame < FrameLimit);
			const FFieldProgram* Field = Game.Loop->Field();
			const FEventContext* Context = Field != nullptr ? &Field->Runtime().Context() : nullptr;
			if (Context != nullptr && (Maps.empty() || Maps.back() != Context->RunningMap))
			{
				Maps.push_back(Context->RunningMap);
			}
			if (Done(Context))
			{
				return Maps;
			}
			const std::optional<FError> Error = Game.Frame((Frame / FramesAPress) % 2 == 0 ? CrossHeld : FPadState{});
			INFO((Error ? Error->Message : std::string()));
			REQUIRE_FALSE(Error);
		}
	}

	/** Until the player has the party in Map, no page waiting. */
	std::function<bool(const FEventContext*)> PlayerHasPartyIn(int32 Map)
	{
		return [Map](const FEventContext* Context)
		{
			return Context != nullptr && Context->RunningMap == Map && Context->PlayerLock == 0 && !Context->bMenuLocked && !PageWaits(*Context);
		};
	}

	/** Until the field runs Map. */
	std::function<bool(const FEventContext*)> Reached(int32 Map)
	{
		return [Map](const FEventContext* Context) { return Context != nullptr && Context->RunningMap == Map; };
	}

	std::vector<std::string> StoryDifferences(const FGameState& Ours, const FGameState& Theirs)
	{
		std::vector<std::string> Differing;
		for (int32 Variable = 0; Variable < FGameState::SavedEventVariableCount; ++Variable)
		{
			const uint16 Address = static_cast<uint16>(Variable * 2);
			const bool bPlaceOrTime = std::find(PlaceAndTimeVariables.begin(), PlaceAndTimeVariables.end(), Address) != PlaceAndTimeVariables.end();
			if (!bPlaceOrTime && Ours.SavedEventVariable(Variable) != Theirs.SavedEventVariable(Variable))
			{
				char Line[32];
				std::snprintf(Line, sizeof(Line), "%03X: %d %d", Address, Ours.SavedEventVariable(Variable), Theirs.SavedEventVariable(Variable));
				Differing.emplace_back(Line);
			}
		}
		return Differing;
	}
}

TEST_CASE("The expanded second disc plays Ramsus' assault on Nisan as Elly, from the chair's session to the Ministry's scene the session goes on to", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 20000;

	// The original: the session's stills, then the Ministry's scene.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAtSession(*Disc, EStoryMode::Original);
	CHECK(PlayFieldUntil(*Original, Reached(MinistryMap), FrameLimit) == std::vector<int32>{ChairMap, MinistryMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(1) == MinistryEntrance);

	// The expanded game: from the chair to Nisan's town, Elly alone, the story at the assault's value, the player given her.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAtSession(*Disc, EStoryMode::Expanded);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(TownMap), FrameLimit) == std::vector<int32>{ChairMap, TownMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == AssaultProgress);
	CHECK(Ours.Party() == std::array<uint8, 3>{Elly, NoOne, NoOne});

	// Before the cathedral by the steps, where the scene begins at once; from it to the Ministry's scene, as the original.
	Ours.SetSavedEventVariable(1, SceneEntrance);
	Ours.SetFieldEntrance(SceneEntrance);
	Expanded->EnterField(CathedralMap);
	CHECK(PlayFieldUntil(*Expanded, Reached(MinistryMap), FrameLimit) == std::vector<int32>{CathedralMap, MinistryMap});
	CHECK(Ours.SavedEventVariable(1) == MinistryEntrance);
	CHECK(Ours.SavedEventVariable(0) == SessionProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Mods/PerfectWorks.h"
#include "Story/DiscText.h"
#include "Text/MesText.h"
#include "Text/MessageTable.h"

#include <filesystem>

TEST_CASE("The chair's lines the Nisan assault names keep their numbers under Perfect Works Build's script, and read as its words", "[Story][Content][Mods]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FPerfectWorksRelease> Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
	if (!Release)
	{
		SKIP("Perfect Works Build not found in Mods/PerfectWorks");
	}
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	constexpr int32 Chair = 617;
	constexpr int32 WhereIsHe = 23;
	constexpr int32 DontFearLove = 48;

	// The original's words.
	std::string Original;
	std::string OriginalLast;
	{
		const FGameDiscText Text(Disc->Image(), *Index);
		const auto Table = FMessageTable::Parse(Text.FieldMapMessages(Chair));
		REQUIRE(Table);
		REQUIRE(Table->Count() > DontFearLove);
		Original = MesTextOfCodes(Table->String(WhereIsHe));
		OriginalLast = MesTextOfCodes(Table->String(DontFearLove));
	}
	CHECK(Original.find("Ramsus") != std::string::npos);
	CHECK(Original.find("Where is he") != std::string::npos);
	CHECK(OriginalLast.find("Elly") != std::string::npos);

	// The mod's: the same numbers, Ramsus still asking after Fei and Elly still answering, in other words.
	FPerfectWorksOptions Options;
	Options.bEnabled = true;
	const std::vector<FModFile> Files = Release->Files(Options, 1);
	const Testing::FModFilesGiven Given(*Disc, FDiscModFiles::Make(*Index, 1, Files, Release->Changes(Options, Files)));
	const FGameDiscText Text(Disc->Image(), *Index);
	const auto Table = FMessageTable::Parse(Text.FieldMapMessages(Chair));
	REQUIRE(Table);
	REQUIRE(Table->Count() > DontFearLove);
	const std::string Retold = MesTextOfCodes(Table->String(WhereIsHe));
	const std::string RetoldLast = MesTextOfCodes(Table->String(DontFearLove));
	INFO(Retold);
	INFO(RetoldLast);
	CHECK(Retold.find("Ramsus") != std::string::npos);
	CHECK(Retold.find("Where is he") != std::string::npos);
	CHECK(Retold != Original);
	CHECK(RetoldLast.find("Elly") != std::string::npos);
}
