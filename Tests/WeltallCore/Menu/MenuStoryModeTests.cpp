#include "Field/FieldRequests.h"
#include "Foundation/LittleEndian.h"
#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Game/WayBackPoint.h"
#include "Game/WorldMapProgram.h"
#include "Menu/MenuGameMenus.h"
#include "Menu/MenuProgram.h"
#include "Menu/OptionsText.h"
#include "Mods/ExpansionMods.h"
#include "Story/StoryLibrary.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace Weltall;

// How the second disc's story is told, asked as the second disc begins: after the disc change's "INSERT DISC 2", and
// after the load of a save made before the answer, where the port has the expanded second disc.
namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState CircleHeld{FPadState::Bit(EPadButton::Circle)};
	constexpr FPadState LeftHeld{FPadState::Bit(EPadButton::Left)};
	constexpr FPadState RightHeld{FPadState::Bit(EPadButton::Right)};
	constexpr FPadState SquareHeld{FPadState::Bit(EPadButton::Square)};
	constexpr FPadState UpHeld{FPadState::Bit(EPadButton::Up)};
	constexpr FPadState DownHeld{FPadState::Bit(EPadButton::Down)};
	constexpr FPadState R1Held{FPadState::Bit(EPadButton::R1)};
	/** Map 9: Lahan's tavern, whose scripts never move the story on; map 35: a village square; map 15: a forest, where saving is not allowed. */
	constexpr uint16 QuietMap = 9;
	constexpr uint16 VillageMap = 35;
	constexpr uint16 ForestMap = 15;
	/** A story well into the second disc. */
	constexpr uint16 LaterProgress = 300;
	constexpr uint8 FileCommand = 1;

	/** A story with the expanded second disc: one document of its pack. */
	FStoryPacks ExpandedStory()
	{
		FStoryPacks Story;
		REQUIRE_FALSE(Story.Add(FStoryPacks::ExpansionPack, "scene.txt", "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\n"));
		return Story;
	}

	/** Each button held for its frames, in order. */
	void Play(Testing::FGameLoopSession& Game, std::span<const std::pair<FPadState, int32>> Presses)
	{
		for (const auto& [Held, Frames] : Presses)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				REQUIRE_FALSE(Game.Frame(Held));
			}
		}
	}

	/**
	 * The game in the tavern with the story at Progress, both discs there, and the field opening the disc change:
	 * "Save?" cancelled, going on without saving, "INSERT DISC 2" up and waiting for its confirmation.
	 */
	struct FAtDiscPrompt
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		FMenuGameMenus Menus;

		/** Prepare, when given, sets the session and its menus up further before the field is entered. */
		FAtDiscPrompt(const FIso9660& First, const FIso9660& Second, const FStoryPacks* Story, uint16 Progress,
			const std::function<void(FGameSession&, FMenuGameMenus&)>& Prepare = {})
			: Game(Testing::FGameLoopSession::Start(First)), Menus(*Game->Executable)
		{
			Game->Session->SetMenus(&Menus);
			Game->Session->SetStory(Story);
			if (Prepare)
			{
				Prepare(*Game->Session, Menus);
			}
			Game->AddDisc(Second);
			Game->Session->Game().SetSavedEventVariable(0, Progress);
			Game->EnterField(QuietMap);
			FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
			Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 100);
			Runtime.Context().MenuRequest = static_cast<uint32>(EMenuMode::DiscChange);
			Game->RunUntil([&] { return Menus.Running() != nullptr; }, 100);
			REQUIRE(Menus.Running() != nullptr);
			const std::array<std::pair<FPadState, int32>, 7> Presses{
				{{NoButtons, 50}, {CircleHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 100}}};
			Play(*Game, Presses);
			REQUIRE(Menus.Running() != nullptr);
			REQUIRE(Game->Session->DiscNumber() == 0);
		}

		/** A button pressed, and the frames after it. */
		void Press(FPadState Button, int32 FramesAfter = 30)
		{
			const std::array<std::pair<FPadState, int32>, 2> Presses{{{Button, 2}, {NoButtons, FramesAfter}}};
			Play(*Game, Presses);
		}

		EStoryMode Mode() const { return Game->Session->Game().StoryMode(); }
	};
}

TEST_CASE("After INSERT DISC 2 the disc change asks how the second disc is told, the pointer on the expanded one", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const FStoryPacks Story = ExpandedStory();
	FAtDiscPrompt At(*First, *Second, &Story, DiscChangeStoryProgress);
	// The disc's prompt confirmed: the second disc is in the drive, and the question stays until it is answered.
	At.Press(CrossHeld, 200);
	REQUIRE(At.Menus.Running() != nullptr);
	CHECK(At.Game->Session->DiscNumber() == 1);
	CHECK(At.Mode() == EStoryMode::Unchosen);
	// Cancel answers nothing.
	At.Press(CircleHeld, 60);
	REQUIRE(At.Menus.Running() != nullptr);
	CHECK(At.Mode() == EStoryMode::Unchosen);
	// Confirmed where the pointer starts.
	At.Press(CrossHeld, 0);
	At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 200);
	REQUIRE(At.Menus.Running() == nullptr);
	CHECK(At.Mode() == EStoryMode::Expanded);
}

TEST_CASE("The second disc's question answered on its left option is the original's second disc, and Right goes back", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const FStoryPacks Story = ExpandedStory();
	{
		FAtDiscPrompt At(*First, *Second, &Story, DiscChangeStoryProgress);
		At.Press(CrossHeld, 100);
		At.Press(LeftHeld, 8);
		At.Press(CrossHeld, 0);
		At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 200);
		REQUIRE(At.Menus.Running() == nullptr);
		CHECK(At.Mode() == EStoryMode::Original);
	}
	{
		FAtDiscPrompt At(*First, *Second, &Story, DiscChangeStoryProgress);
		At.Press(CrossHeld, 100);
		At.Press(LeftHeld, 8);
		At.Press(RightHeld, 8);
		At.Press(CrossHeld, 0);
		At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 200);
		REQUIRE(At.Menus.Running() == nullptr);
		CHECK(At.Mode() == EStoryMode::Expanded);
	}
}

TEST_CASE("No game is asked without the expanded second disc, or with the story past the disc change", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const FStoryPacks Expanded = ExpandedStory();
	FStoryPacks Additions;
	REQUIRE_FALSE(Additions.Add("additions", "scene.txt", "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\n"));
	struct FCase
	{
		const FStoryPacks* Story;
		uint16 Progress;
	};
	const std::array<FCase, 3> Cases{{{nullptr, DiscChangeStoryProgress}, {&Additions, DiscChangeStoryProgress}, {&Expanded, LaterProgress}}};
	for (const FCase& Case : Cases)
	{
		FAtDiscPrompt At(*First, *Second, Case.Story, Case.Progress);
		At.Press(CrossHeld, 0);
		At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 200);
		CHECK(At.Menus.Running() == nullptr);
		CHECK(At.Game->Session->DiscNumber() == 1);
		CHECK(At.Mode() == EStoryMode::Unchosen);
	}
}

TEST_CASE("The question offers the Mods folder's expansion the player chose, which the game answered Extended plays; none with expansions off", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const std::filesystem::path Mods = std::filesystem::temp_directory_path() / "WeltallMenuStoryModeMods";
	std::error_code Error;
	std::filesystem::remove_all(Mods, Error);
	for (const char* Folder : {"Abel", "Zeboim"})
	{
		std::filesystem::create_directories(Mods / Folder / "disc2");
		std::ofstream(Mods / Folder / "disc2" / "scene.txt", std::ios::binary) << "map 1000 from 7\nactors 1\nactor 0 script 0:\n    Return\n";
	}
	const FStoryLibrary Library(FStoryPacks(), FindExpansionMods(Mods));
	std::filesystem::remove_all(Mods, Error);
	REQUIRE(Library.Expansion("Zeboim") != nullptr);

	FPlayerSettings Chosen;
	Chosen.Expansions.Folder = "Zeboim";
	FAtDiscPrompt At(*First, *Second, nullptr, DiscChangeStoryProgress, [&](FGameSession& Session, FMenuGameMenus&)
		{
			Session.SetStoryLibrary(&Library);
			Session.SetSettings(Chosen);
		});
	FGameSession& Session = *At.Game->Session;
	CHECK(Session.Story() == &Library.For("Zeboim"));
	REQUIRE(Session.OfferedExpansion() != nullptr);
	CHECK(Session.OfferedExpansion()->Folder == "Zeboim");
	At.Press(CrossHeld, 200);
	At.Press(CrossHeld, 0);
	At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 200);
	REQUIRE(At.Menus.Running() == nullptr);
	CHECK(At.Mode() == EStoryMode::Expanded);
	CHECK(Session.Game().Expansion() == "Zeboim");
	// The game plays its own expansion whichever the player chooses next, and with expansions turned off: they apply to the
	// games offered or loaded after.
	FPlayerSettings Other = Chosen;
	Other.Expansions.Folder = "Abel";
	Session.SetSettings(Other);
	CHECK(Session.Story() == &Library.For("Zeboim"));
	CHECK(Session.PlaysExpansion("Abel"));
	CHECK_FALSE(Session.PlaysExpansion("Missing"));
	Other.Expansions.bEnabled = false;
	Session.SetSettings(Other);
	CHECK(Session.Story() == &Library.For("Zeboim"));
	CHECK_FALSE(Session.PlaysExpansion("Zeboim"));
	CHECK(Session.OfferedExpansion() == nullptr);

	FPlayerSettings Off;
	Off.Expansions.bEnabled = false;
	FAtDiscPrompt Unasked(*First, *Second, nullptr, DiscChangeStoryProgress, [&](FGameSession& Game, FMenuGameMenus&)
		{
			Game.SetStoryLibrary(&Library);
			Game.SetSettings(Off);
		});
	Unasked.Press(CrossHeld, 0);
	Unasked.Game->RunUntil([&] { return Unasked.Menus.Running() == nullptr; }, 200);
	CHECK(Unasked.Menus.Running() == nullptr);
	CHECK(Unasked.Game->Session->DiscNumber() == 1);
	CHECK(Unasked.Mode() == EStoryMode::Unchosen);

	// Menu (Square, as the USA's buttons are) at the question opens the Modding page on the expansion offered: Right
	// chooses the next, and back at the question Extended plays it.
	std::vector<FPlayerSettings> Kept;
	FAtDiscPrompt Chooses(*First, *Second, nullptr, DiscChangeStoryProgress, [&](FGameSession& Game, FMenuGameMenus& Menus)
		{
			Game.SetStoryLibrary(&Library);
			Menus.SetSettingsKept([&Kept](const FPlayerSettings& Settings) { Kept.push_back(Settings); });
		});
	Chooses.Press(CrossHeld, 200);
	Chooses.Press(SquareHeld, 80);
	Chooses.Press(RightHeld, 30);
	CHECK(Chooses.Game->Session->Settings().Expansions.Folder == "Zeboim");
	CHECK(Chooses.Mode() == EStoryMode::Unchosen);
	Chooses.Press(CircleHeld, 80);
	REQUIRE(Chooses.Menus.Running() != nullptr);
	REQUIRE(Kept.size() == 1);
	CHECK(Kept.front().Expansions.Folder == "Zeboim");
	Chooses.Press(CrossHeld, 0);
	Chooses.Game->RunUntil([&] { return Chooses.Menus.Running() == nullptr; }, 200);
	REQUIRE(Chooses.Menus.Running() == nullptr);
	CHECK(Chooses.Mode() == EStoryMode::Expanded);
	CHECK(Chooses.Game->Session->Game().Expansion() == "Zeboim");
}

TEST_CASE("A save at the disc change's story holds no answer: loaded, it is asked, the disc change's own after its INSERT DISC 2, and the answer is saved", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const FStoryPacks Story = ExpandedStory();
	// The save the disc change makes, on the forest where it opens, and one made on the second disc before its story
	// moves on: both to go on with the second disc, nothing chosen.
	constexpr uint16 DiscChangeMap = 612;
	for (const uint16 Place : {DiscChangeMap, VillageMap})
	{
		INFO("saved on map " << Place);
		const bool bDiscChange = Place == DiscChangeMap;
		auto Game = Testing::FGameLoopSession::Start(*First);
		FMenuGameMenus Menus(*Game->Executable);
		FGameSession& Session = *Game->Session;
		Session.SetMenus(&Menus);
		Session.SetStory(&Story);
		Game->AddDisc(*Second);
		const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuStoryModeLoad";
		std::error_code Error;
		std::filesystem::remove_all(Folder, Error);
		FSaveStore Store(Folder);
		FGameState Saved = Session.Game();
		Saved.SetPlace(Place);
		Saved.SetSavedEventVariable(0, DiscChangeStoryProgress);
		const FOriginalSave AtDiscChange = WriteOriginalSave(Saved, 0, 1, Session.Text().NameTables, Session.Text().Font.LeadByte());
		CHECK(AtDiscChange.StoryMode() == EStoryMode::Unchosen);
		REQUIRE_FALSE(Store.Write(0, AtDiscChange));
		Session.SetSaves(&Store);
		FGameState& State = Session.Game();
		State.SetParty({0, 1, 2});
		State.SetJoinedCharacters(0x1FF);
		State.MenuLastCommand = FileCommand;
		Game->EnterField(ForestMap);
		FGameLoop& Loop = *Game->Loop;
		Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning; }, 200);
		Game->RunUntil([&] { return Menus.Running() != nullptr; }, 200, SquareHeld);
		REQUIRE(Menus.Running() != nullptr);

		// File, Load, the save, "Load this file?" Yes, "Finished": then the disc change's save asks for its disc, the
		// other goes on with it at once.
		const std::array<std::pair<FPadState, int32>, 13> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
			{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60}, {CrossHeld, 2}, {NoButtons, 200}}};
		Play(*Game, Presses);
		REQUIRE(Menus.Running() != nullptr);
		CHECK(Session.DiscNumber() == (bDiscChange ? 0 : 1));
		CHECK(State.StoryMode() == EStoryMode::Unchosen);
		if (bDiscChange)
		{
			// "INSERT DISC 2" confirmed.
			const std::array<std::pair<FPadState, int32>, 2> Confirmed{{{CrossHeld, 2}, {NoButtons, 100}}};
			Play(*Game, Confirmed);
		}
		// The question: the original's second disc.
		const std::array<std::pair<FPadState, int32>, 3> Answer{{{LeftHeld, 2}, {NoButtons, 8}, {CrossHeld, 2}}};
		Play(*Game, Answer);
		Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
		REQUIRE(Menus.Running() == nullptr);
		CHECK(Session.DiscNumber() == 1);
		CHECK(State.ProgramResult == 2);
		CHECK(State.StoryMode() == EStoryMode::Original);

		// A save made from here on keeps the answer, and loads with it and no question.
		const FOriginalSave Answered = WriteOriginalSave(State, 1, 1, Session.Text().NameTables, Session.Text().Font.LeadByte());
		CHECK(Answered.StoryMode() == EStoryMode::Original);
		std::filesystem::remove_all(Folder, Error);
	}
}

TEST_CASE("A save of an expansion that cannot be played loads as the original second disc's game at its point, and says so", "[Menu][Story][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*First);
	FMenuGameMenus Menus(*Game->Executable);
	FGameSession& Session = *Game->Session;
	Session.SetMenus(&Menus);
	// The Mods folder holds no expansion.
	const FStoryLibrary Library(FStoryPacks(), {});
	Session.SetStoryLibrary(&Library);
	Game->AddDisc(*Second);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuStoryModeMissing";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	// A game in a map of its expansion's own, saved as the original's at the chair where the expansion rejoins it.
	FGameState Saved = Session.Game();
	Saved.SetStoryMode(EStoryMode::Expanded);
	Saved.SetExpansion("Abel");
	Saved.SetPlace(735);
	Saved.SetSavedEventVariable(0, 253);
	const FWayBackPoint Chair{617, 2, 252};
	const FOriginalSave Save = WriteOriginalSave(Saved, 0, 1, Session.Text().NameTables, Session.Text().Font.LeadByte(), &Chair);
	REQUIRE(Save.Expansion() == "Abel");
	REQUIRE_FALSE(Store.Write(0, Save));
	Session.SetSaves(&Store);
	FGameState& State = Session.Game();
	State.SetParty({0, 1, 2});
	State.SetJoinedCharacters(0x1FF);
	State.MenuLastCommand = FileCommand;
	Game->EnterField(ForestMap);
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning; }, 200);
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 200, SquareHeld);
	REQUIRE(Menus.Running() != nullptr);

	// File, Load, the save, "Load this file?" Yes, "Finished".
	const std::array<std::pair<FPadState, int32>, 13> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
		{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60}, {CrossHeld, 2}, {NoButtons, 200}}};
	Play(*Game, Presses);
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(State.ProgramResult == 2);
	CHECK(State.StoryMode() == EStoryMode::Original);
	CHECK(State.Expansion().empty());
	CHECK(State.Place() == 617);
	CHECK(State.SavedEventVariable(0) == 252);
	const FToldLine Told = Session.TakeTold();
	CHECK(Told.Line == OptionsText::SecondDisc::ExpansionMissing);
	CHECK(Told.Why.find("Mods/Abel") != std::string::npos);
	CHECK(Session.TakeTold().Line.empty());
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("The menu saves a game of the expanded second disc where the original stands elsewhere as the original's save there", "[Menu][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// A world map of the first disc stands in for the expanded play: its story at a value the way back names.
	constexpr uint16 Story = 30;
	constexpr uint16 WorldMapPlace = 0x401;
	FStoryPacks Packs = ExpandedStory();
	REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "way-back.txt", "way back\nstory 30 to map 617 entrance 2 story 252\n"));
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	FGameSession& Session = *Game->Session;
	Session.SetMenus(&Menus);
	Session.SetStory(&Packs);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuStoryModeWayBack";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	Session.SetSaves(&Store);
	FGameState& State = Session.Game();
	State.SetStoryMode(EStoryMode::Expanded);
	State.SetSavedEventVariable(0, Story);
	State.SetWorldMapPlace(WorldMapPlace, 1, 0xC00, 1);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	const auto Frame = [&Program](FPadState Held)
	{
		const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program.VblanksPerFrame()), Held);
		REQUIRE_FALSE(Program.Frame(Vblanks));
	};
	for (int32 Tick = 0; Tick < 200 && Menus.Running() == nullptr; ++Tick)
	{
		Frame(Tick % 4 == 0 ? SquareHeld : NoButtons);
	}
	REQUIRE(Menus.Running() != nullptr);

	// File, where the menu opens where saving is allowed; Save, the first slot, "Save?" Yes.
	const std::array<std::pair<FPadState, int32>, 10> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
		{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Each = 0; Each < Frames; ++Each)
		{
			Frame(Held);
		}
	}
	for (int32 Each = 0; Each < 200; ++Each)
	{
		Frame(NoButtons);
	}
	const auto Read = Store.Read(0);
	REQUIRE(Read);
	REQUIRE(Read->has_value());
	const FOriginalSave& Save = **Read;
	CHECK(Save.StoryMode() == EStoryMode::Expanded);
	// The game reads the chair, the story at 252; the port, the world map where it was made.
	constexpr std::size_t PlaceInData = 0x1B1E;
	constexpr std::size_t StoryInData = 0x1134;
	CHECK(LittleEndian::ReadU16(Save.Data(), PlaceInData) == 617);
	CHECK(LittleEndian::ReadU16(Save.Data(), StoryInData) == 252);
	CHECK(LittleEndian::ReadU16(Save.PlayedData(), PlaceInData) == WorldMapPlace);
	CHECK(Save.StoryProgress() == Story);
	// And the game goes on where it was.
	CHECK(State.Place() == WorldMapPlace);
	CHECK(State.SavedEventVariable(0) == Story);
	std::filesystem::remove_all(Folder, Error);
}

namespace
{
	/**
	 * A game of the expanded second disc in the forest, its story at Story, with Packs, its menu opened on Options and
	 * the options turned to the Fixes page, the cursor on the second disc's row.
	 */
	struct FAtSecondDiscRow
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		FMenuGameMenus Menus;

		FAtSecondDiscRow(const FIso9660& Disc, const FStoryPacks& Packs, uint16 Story)
			: Game(Testing::FGameLoopSession::Start(Disc)), Menus(*Game->Executable)
		{
			constexpr uint8 OptionsCommand = 7;
			FGameSession& Session = *Game->Session;
			Session.SetMenus(&Menus);
			Session.SetStory(&Packs);
			// Settings kept make the options screen the menu's eighth orb.
			Menus.SetSettingsKept([](const FPlayerSettings&) {});
			FGameState& State = Session.Game();
			State.SetParty({0, 1, 2});
			State.SetJoinedCharacters(0x1FF);
			State.SetStoryMode(EStoryMode::Expanded);
			State.SetSavedEventVariable(0, Story);
			State.MenuLastCommand = OptionsCommand;
			Game->EnterField(ForestMap);
			FGameLoop& Loop = *Game->Loop;
			Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning; }, 200);
			Game->RunUntil([&] { return Menus.Running() != nullptr; }, 200, SquareHeld);
			REQUIRE(Menus.Running() != nullptr);
			// Options; R1 past Display, Sound and Camera to Fixes; Down from Bug fixes to Disc 2.
			const std::array<std::pair<FPadState, int32>, 12> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 40}, {R1Held, 2}, {NoButtons, 8},
				{R1Held, 2}, {NoButtons, 8}, {R1Held, 2}, {NoButtons, 8}, {DownHeld, 2}, {NoButtons, 8}, {NoButtons, 8}}};
			Play(*Game, Presses);
			REQUIRE(Menus.Running() != nullptr);
		}

		void Press(FPadState Button, int32 FramesAfter = 20)
		{
			const std::array<std::pair<FPadState, int32>, 2> Presses{{{Button, 2}, {NoButtons, FramesAfter}}};
			Play(*Game, Presses);
		}
	};

	/** The expansion's pack with a way back from story 30 to the village square's entrance 0, the story at 31. */
	FStoryPacks StoryWithWayBack()
	{
		FStoryPacks Packs = ExpandedStory();
		REQUIRE_FALSE(Packs.Add(FStoryPacks::ExpansionPack, "way-back.txt", "way back\nstory 30 to map 35 entrance 0 story 31\n"));
		return Packs;
	}
}

TEST_CASE("The Fixes page's Disc 2 row asks, then tells the rest of the second disc as the original from where the original stands", "[Menu][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FStoryPacks Packs = StoryWithWayBack();
	FAtSecondDiscRow At(*Disc, Packs, 30);
	FGameSession& Session = *At.Game->Session;
	FGameState& State = Session.Game();
	// The first Confirm only asks.
	At.Press(CrossHeld);
	CHECK(State.StoryMode() == EStoryMode::Expanded);
	REQUIRE(At.Menus.Running() != nullptr);
	// The second switches: the menu closes on the game put where the original stands, and the field takes it there.
	At.Press(CrossHeld, 0);
	At.Game->RunUntil([&] { return At.Menus.Running() == nullptr; }, 300);
	REQUIRE(At.Menus.Running() == nullptr);
	CHECK(State.StoryMode() == EStoryMode::Original);
	CHECK(State.SavedEventVariable(0) == 31);
	CHECK(State.ProgramResult == 2);
	FGameLoop& Loop = *At.Game->Loop;
	At.Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().RunningMap == VillageMap; }, 600);
	REQUIRE(Loop.Field() != nullptr);
	CHECK(Loop.Field()->Runtime().Context().RunningMap == VillageMap);
}

TEST_CASE("The Disc 2 row switches a game the original stands with where it is, and asks again once the cursor has moved", "[Menu][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FStoryPacks Packs = StoryWithWayBack();
	FAtSecondDiscRow At(*Disc, Packs, 29);
	FGameState& State = At.Game->Session->Game();
	// Asked, then the cursor away and back: the question is asked again.
	At.Press(CrossHeld);
	At.Press(UpHeld);
	At.Press(DownHeld);
	At.Press(CrossHeld);
	CHECK(State.StoryMode() == EStoryMode::Expanded);
	At.Press(CrossHeld);
	CHECK(State.StoryMode() == EStoryMode::Original);
	CHECK(State.SavedEventVariable(0) == 29);
	// No point of the way back: the game stays where it is, the menu with it.
	CHECK(At.Menus.Running() != nullptr);
	CHECK(State.ProgramResult != 2);
}

TEST_CASE("The second disc's question's words are in the game's font and fit its window", "[Menu][Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
	// In pixels: a line from the window's margin to its other one, within a strip's typing; an answer and the pointer
	// beside it (its gap and its width at half its size) before the next answer, and before the margin.
	constexpr int32 WindowWidth = 0x110;
	constexpr int32 Margin = 0x10;
	constexpr int32 TypedWidth = 0x3D * 4;
	constexpr int32 LeftAnswerX = 0x24;
	constexpr int32 RightAnswerX = 0xAC;
	constexpr int32 PointerRoom = 3 + 17;
	const auto Width = [&Text](std::string_view Words)
	{
		int32 Units = 0;
		for (const char Character : Words)
		{
			const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
			CHECK(Code.has_value());
			Units += Code ? Text.Font.WidthUnits(*Code) : 0;
		}
		return Units * 4;
	};
	namespace Words = OptionsText::SecondDisc;
	for (const std::string_view Line : {Words::Question, Words::OriginalHelp[0], Words::OriginalHelp[1], Words::OriginalHelp[2], Words::ExtendedHelp[0],
			 Words::ExtendedHelp[1], Words::ExtendedHelp[2]})
	{
		INFO(Line);
		CHECK(Width(Line) <= WindowWidth - 2 * Margin);
		CHECK(Width(Line) <= TypedWidth);
	}
	CHECK(LeftAnswerX + Width(Words::Original) + PointerRoom <= RightAnswerX);
	CHECK(RightAnswerX + Width(Words::Extended) + PointerRoom <= WindowWidth - Margin);
	// A mod's expansion named under Extended, as long as the question shows a name, in the font's widest letters.
	for (const std::string& Line : OptionsText::Fill(Words::ExtendedNamed, {std::string(20, 'W') + ".."}))
	{
		INFO(Line);
		CHECK(Width(Line) <= WindowWidth - 2 * Margin);
	}

	// The Fixes page's row: its label and either value side by side in the list, each line of its help in its strip.
	constexpr int32 LabelX = 0x28;
	constexpr int32 LabelGap = 8;
	constexpr int32 ValueEnd = 0x128;
	for (const std::string_view Value : {Words::Extended, Words::Original})
	{
		CHECK(LabelX + Width(Words::Row.Label) + LabelGap <= ValueEnd - Width(Value));
	}
	for (const std::array<std::string_view, 3>& Help : {Words::Row.Help, Words::SwitchAsked, Words::Switched})
	{
		for (const std::string_view Line : Help)
		{
			INFO(Line);
			CHECK(Width(Line) <= TypedWidth);
		}
	}
}
