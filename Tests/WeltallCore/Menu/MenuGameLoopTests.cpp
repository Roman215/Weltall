#include "Field/FieldRequests.h"
#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Game/WorldMapProgram.h"
#include "Game/PlayerSettings.h"
#include "Menu/MenuDeviceFiles.h"
#include "Menu/MenuGameMenus.h"
#include "Menu/MenuProgram.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

using namespace Weltall;

// The Menu inside the game's loop: the title's map opens Load Game, whose New Game hands the field a
// new game.
namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState UpHeld{FPadState::Bit(EPadButton::Up)};
	constexpr FPadState LeftHeld{FPadState::Bit(EPadButton::Left)};
	constexpr FPadState CircleHeld{FPadState::Bit(EPadButton::Circle)};
	constexpr int32 FirstMap = 4;
	/** Map 35: a village square. */
	constexpr int32 VillageMap = 35;
	constexpr int32 SecondOpeningMovie = 13;

	/** The host's drive, which finds the disc the game asks for once the player has put it where the game looks. */
	class FLateDrive final : public IGameDiscDrive
	{
	public:
		FLateDrive(Testing::FGameLoopSession& InOwner, const FIso9660& InLateDisc) : Owner(InOwner), LateDisc(InLateDisc) {}
		void Seek(int32 Number) override
		{
			Asked.push_back(Number);
			if (bThere)
			{
				Owner.AddDisc(LateDisc);
			}
		}
		Testing::FGameLoopSession& Owner;
		const FIso9660& LateDisc;
		std::vector<int32> Asked;
		bool bThere = false;
	};
}

TEST_CASE("On the title's map Load Game runs in the loop, and its New Game plays the opening's second movie", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	// Cross skips the opening movie, then opens Load Game on the title's map.
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Loop.Program() == EGameProgram::Field);
	CHECK(Game->Session->Kernel().MenuMode == static_cast<uint8>(EMenuMode::LoadGame));

	// The layer flies in and the arc shows itself; Cross held from before is no new press.
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
	}
	REQUIRE(Menus.Running() != nullptr);
	CHECK_FALSE(Menus.Running()->Snapshot().Primitives.empty());
	// The title is no chapter's start.
	CHECK(Game->Session->ChapterCard().Shown() == 0);

	// Up to New Game, and Cross.
	const uint32 PlayTimeBefore = Game->Session->Game().PlayTime();
	REQUIRE(PlayTimeBefore > 300);
	REQUIRE_FALSE(Game->Frame(UpHeld));
	REQUIRE_FALSE(Game->Frame(NoButtons));
	REQUIRE_FALSE(Game->Frame(CrossHeld));
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	// The new game's clock started at 0 (the fix; the original's goes on from power-on).
	CHECK(Game->Session->Game().PlayTime() < 210);
	CHECK(Game->Session->Game().ProgramResult == 0);
	CHECK(Game->Session->Game().MenuLastCommand == 6);
	CHECK(Game->Session->Game().Place() == FirstMap);

	// The field lightens its picture back, then the title's scripts play the opening's second movie.
	FFieldProgram* Field = Loop.Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Movie() != nullptr; }, 600);
	const FFieldRequest& Request = Field->Runtime().Request();
	REQUIRE(Request.Kind == EFieldRequest::Movie);
	CHECK(Request.Movie.Movie == SecondOpeningMovie);
}

TEST_CASE("On the title's map Continue loads a save from the player's stores, and the game goes on at its map", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	// A save of a new game moved to the village, in the first store's first slot.
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuGameLoopContinue";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	REQUIRE_FALSE(Game->Session->InitNewGame());
	FGameState Saved = Game->Session->Game();
	Saved.SetPlace(VillageMap);
	const FOriginalSave Save = WriteOriginalSave(Saved, 0, 0, Game->Session->Text().NameTables, Game->Session->Text().Font.LeadByte());
	REQUIRE_FALSE(Store.Write(0, Save));
	Game->Session->SetSaves(&Store);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
	REQUIRE(Menus.Running() != nullptr);

	// Continue (the title's first command), Load, the save, "Load this file?" Yes, "Finished".
	const std::array<std::pair<FPadState, int32>, 12> Presses{{{NoButtons, 40}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
		{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60}, {CrossHeld, 2}}};
	// The title's arc shows no stores; the File screen's cards the first pair of the eight.
	CHECK_FALSE(Menus.Running()->Snapshot().Stores.bShown);
	bool bStoresShown = false;
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
			if (Menus.Running() != nullptr && Menus.Running()->Snapshot().Stores.bShown)
			{
				bStoresShown = true;
				CHECK(Menus.Running()->Snapshot().Stores.FirstStore == 0);
				CHECK(Menus.Running()->Snapshot().Stores.StoreCount == FSaveStore::StoreCount);
			}
		}
	}
	CHECK(bStoresShown);
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Game->Session->Game().ProgramResult == 2);
	CHECK(Game->Session->Game().Place() == VillageMap);

	// The title's map starts the Field program again, on the save's map.
	const auto OnSavedMap = [&] {
		return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().MapNumber == VillageMap && Loop.Field()->Runtime().Context().bFieldRunning;
	};
	Game->RunUntil(OnSavedMap, 600);
	CHECK(OnSavedMap());
	CHECK(Loop.Program() == EGameProgram::Field);
	// The save's chapter is not announced again as the player walks on.
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
		REQUIRE(Game->Session->ChapterCard().Shown() == 0);
	}
	std::filesystem::remove_all(Folder, Error);
}

namespace
{
	constexpr FPadState SquareHeld{FPadState::Bit(EPadButton::Square)};
	/** Map 15: a forest, where saving is not allowed. */
	constexpr uint16 ForestMap = 15;
	/** Era 1 of the world map at its first scene's first entrance, where its menu saves. */
	constexpr uint16 WorldMapStory = 30;
	constexpr uint16 WorldMapPlace = 0x401;
	/** The Normal Menu's File command. */
	constexpr uint8 FileCommand = 1;

	/** A save of a new game moved to the village, in the first store's first slot of a fresh folder. */
	FSaveStore SaveInVillage(Testing::FGameLoopSession& Game, const std::filesystem::path& Folder)
	{
		std::error_code Error;
		std::filesystem::remove_all(Folder, Error);
		FSaveStore Store(Folder);
		FGameState Saved = Game.Session->Game();
		Saved.SetPlace(VillageMap);
		const FOriginalSave Save = WriteOriginalSave(Saved, 0, 0, Game.Session->Text().NameTables, Game.Session->Text().Font.LeadByte());
		REQUIRE_FALSE(Store.Write(0, Save));
		return Store;
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
}

TEST_CASE("In a field the Normal Menu's File loads a save over Copy and Delete, and the game goes on at the save's map", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuGameLoopLoadInField";
	FSaveStore Store = SaveInVillage(*Game, Folder);
	Game->Session->SetSaves(&Store);
	FGameState& State = Game->Session->Game();
	State.SetParty({0, 1, 2});
	State.SetJoinedCharacters(0x1FF);
	State.MenuLastCommand = FileCommand;
	Game->EnterField(ForestMap);
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning; }, 200);
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 200, SquareHeld);
	REQUIRE(Menus.Running() != nullptr);
	const int32 Runs = Loop.ProgramRuns();

	// File, where the menu opened; Load (the top of Load, Copy and Delete, where the cursor starts), the save,
	// "Load this file?" Yes, "Finished".
	const std::array<std::pair<FPadState, int32>, 12> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
		{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60}, {CrossHeld, 2}}};
	Play(*Game, Presses);
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(State.ProgramResult == 2);
	CHECK(State.Place() == VillageMap);
	// The console keeps the command the menu closed on, which no save holds.
	CHECK(State.MenuLastCommand == FileCommand);

	// The forest's Field program ends, and the kernel starts the field again on the save's map.
	const auto OnSavedMap = [&] {
		return Loop.ProgramRuns() != Runs && Loop.Field() != nullptr && Loop.Field()->Runtime().Context().MapNumber == VillageMap
			&& Loop.Field()->Runtime().Context().bFieldRunning;
	};
	Game->RunUntil(OnSavedMap, 600);
	CHECK(OnSavedMap());
	CHECK(State.Place() == VillageMap);
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("On the world map the menu's File loads a save from over Save, and the world map hands the game to the save's map", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuGameLoopLoadOnWorldMap";
	FSaveStore Store = SaveInVillage(*Game, Folder);
	Game->Session->SetSaves(&Store);
	FGameSession& Session = *Game->Session;
	Session.Game().SetSavedEventVariable(0, WorldMapStory);
	Session.Game().SetWorldMapPlace(WorldMapPlace, 1, 0xC00, 1);
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

	// File, where the menu opens where saving is allowed; Up from Save to Load, the save, Yes, "Finished".
	const std::array<std::pair<FPadState, int32>, 14> Presses{{{NoButtons, 50}, {CrossHeld, 2}, {NoButtons, 70}, {UpHeld, 2}, {NoButtons, 8},
		{CrossHeld, 2}, {NoButtons, 40}, {CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60},
		{CrossHeld, 2}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Each = 0; Each < Frames; ++Each)
		{
			Frame(Held);
		}
	}
	for (int32 Each = 0; Each < 200 && Program.State() == EGameProgramState::Running; ++Each)
	{
		Frame(NoButtons);
	}
	CHECK(Program.State() == EGameProgramState::Ended);
	CHECK(Session.Kernel().NextProgram == EGameProgram::Field);
	CHECK(Session.Game().ProgramResult == 2);
	CHECK(Session.Game().Place() == VillageMap);
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("The disc change leaves disc 2 in the drive: the field's scripts read it and its movies are disc 2's", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*First);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	Game->AddDisc(*Second);
	Game->EnterField(VillageMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	FFieldRuntime& Runtime = Field->Runtime();
	Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 100);
	CHECK(Game->Session->DiscNumber() == 0);
	CHECK(Runtime.Context().Disc == 1);

	// The field opens the disc change; "Save?" cancelled asks whether to go on without saving, answered Yes.
	Runtime.Context().MenuRequest = static_cast<uint32>(EMenuMode::DiscChange);
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 100);
	REQUIRE(Menus.Running() != nullptr);
	const std::array<std::pair<FPadState, int32>, 6> Presses{{{NoButtons, 50}, {CircleHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
		}
	}
	// "INSERT DISC 2" stays until it is confirmed; disc 2 is in the drive from then on.
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Game->Session->DiscNumber() == 0);
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
	}
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Game->Session->DiscNumber() == 1);
	CHECK(Runtime.Context().Disc == 2);
	const TResult<FGameDisc> Drive = Game->Session->DriveDisc();
	REQUIRE(Drive);
	CHECK(Drive->Image == &Second->Image());
}

TEST_CASE("A disc 2 not yet found keeps INSERT DISC 2 up at each confirmation until the host's drive finds it", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*First);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);

	// The player puts disc 2 where the game looks only after the first confirmation.
	FLateDrive Drive(*Game, *Second);
	Game->Session->SetDiscDrive(&Drive);
	Game->EnterField(VillageMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	FFieldRuntime& Runtime = Field->Runtime();
	Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 100);

	Runtime.Context().MenuRequest = static_cast<uint32>(EMenuMode::DiscChange);
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 100);
	REQUIRE(Menus.Running() != nullptr);
	const std::array<std::pair<FPadState, int32>, 7> Presses{
		{{NoButtons, 50}, {CircleHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 200}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
		}
	}
	CHECK(Drive.Asked.empty());

	// Confirmed with disc 2 still missing: it was looked for, and the message stays.
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
	}
	for (int32 Frame = 0; Frame < 100; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK(Drive.Asked == std::vector<int32>{1});
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Game->Session->DiscNumber() == 0);

	// There now: the next confirmation finds it and the game goes on on disc 2.
	Drive.bThere = true;
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
	}
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Drive.Asked == std::vector<int32>{1, 1});
	CHECK(Game->Session->DiscNumber() == 1);
	CHECK(Runtime.Context().Disc == 2);
}

TEST_CASE("Where discs come through the device's picker, INSERT DISC 2's Confirm picks the missing disc there and then", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*First);
	FMenuGameMenus Menus(*Game->Executable);
	Menus.SetSettingsKept([](const FPlayerSettings&) {});
	/** The device's picker as the test plays it: the player picks, then the copy runs and ends as the test says. */
	class FPicker final : public IMenuDeviceFiles
	{
	public:
		FTransferStatus Status() const override { return Shown; }
		bool Start(EDeviceTransfer Kind, const std::filesystem::path& = {}) override
		{
			Started.push_back(Kind);
			Set(ETransferState::Picking);
			return true;
		}
		void Set(ETransferState State, uint64 Copied = 0)
		{
			Shown.State = State;
			Shown.Copied = Copied;
			Shown.Total = 100;
			++Shown.Revision;
		}
		FTransferStatus Shown;
		std::vector<EDeviceTransfer> Started;
	} Picker;
	Menus.SetDeviceFiles(&Picker);
	Game->Session->SetMenus(&Menus);
	FLateDrive Drive(*Game, *Second);
	Game->Session->SetDiscDrive(&Drive);
	Game->EnterField(VillageMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	FFieldRuntime& Runtime = Field->Runtime();
	Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 100);
	Runtime.Context().MenuRequest = static_cast<uint32>(EMenuMode::DiscChange);
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 100);
	REQUIRE(Menus.Running() != nullptr);
	const std::array<std::pair<FPadState, int32>, 7> Presses{
		{{NoButtons, 50}, {CircleHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 200}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
		}
	}
	const auto Run = [&](FPadState Held, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
		}
	};

	// Confirmed with disc 2 missing: looked for, then picked; the message stays while the copy runs.
	Run(CrossHeld, 2);
	Run(NoButtons, 30);
	CHECK(Drive.Asked == std::vector<int32>{1});
	REQUIRE(Picker.Started == std::vector<EDeviceTransfer>{EDeviceTransfer::Discs});
	Picker.Set(ETransferState::Copying, 50);
	Run(NoButtons, 30);
	REQUIRE(Menus.Running() != nullptr);
	// The files picked held no disc 2: looked for again as the copy ended, the message still up.
	Picker.Set(ETransferState::Done, 100);
	Run(NoButtons, 30);
	CHECK(Drive.Asked == std::vector<int32>{1, 1});
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Game->Session->DiscNumber() == 0);

	// Confirm again picks again; this copy brings disc 2, and the game goes on on it.
	Run(CrossHeld, 2);
	Run(NoButtons, 10);
	CHECK(Picker.Started.size() == 2);
	Drive.bThere = true;
	Picker.Set(ETransferState::Done, 100);
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Game->Session->DiscNumber() == 1);
	CHECK(Runtime.Context().Disc == 2);
}

TEST_CASE("The disc prompts' own lines fit under the message", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
	// The message window's inside, from its lines' start to its frame, in pixels.
	constexpr int32 LinesRoom = 0x7A + 0xBC - 0x84 - 8;
	namespace Discs = OptionsText::Discs;
	for (const std::string& Line : {std::string(Discs::NotFound), OptionsText::Fill(Discs::PromptCopying, {"100"}), std::string(Discs::PromptNotPicked),
			 std::string(Discs::PromptNotCopied), std::string(Discs::PromptPickAgain)})
	{
		INFO(Line);
		int32 Units = 0;
		for (const char Character : Line)
		{
			const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
			REQUIRE(Code);
			Units += Text.Font.WidthUnits(*Code);
		}
		CHECK(Units * 4 <= LinesRoom);
	}
}

TEST_CASE("Powered on with disc 2 alone, New Game asks for disc 1 and the game goes on from disc 1's files", "[Menu][Content]")
{
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Second);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	FLateDrive Drive(*Game, *First);
	Game->Session->SetDiscDrive(&Drive);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	// Disc 2's opening and title, read from disc 2.
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Game->Session->DiscNumber() == 1);
	CHECK(&Game->Session->Image() == &Second->Image());
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
	}

	// New Game: "INSERT DISC 1" stays up, disc 1 looked for at each confirmation until it is there.
	REQUIRE_FALSE(Game->Frame(UpHeld));
	REQUIRE_FALSE(Game->Frame(NoButtons));
	REQUIRE_FALSE(Game->Frame(CrossHeld));
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Drive.Asked.empty());
	for (const bool bThere : {false, true})
	{
		Drive.bThere = bThere;
		for (int32 Frame = 0; Frame < 2; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(CrossHeld));
		}
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame());
		}
	}
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Drive.Asked == std::vector<int32>{0, 0});
	CHECK(Game->Session->DiscNumber() == 0);
	CHECK(&Game->Session->Image() == &First->Image());
	CHECK(Game->Session->Game().Place() == FirstMap);

	// The opening's second movie is disc 1's, and the first map, whose file disc 2 holds only a stand-in for, is read from disc 1.
	FFieldProgram* Field = Loop.Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Movie() != nullptr; }, 600);
	REQUIRE(Field->Runtime().Request().Kind == EFieldRequest::Movie);
	CHECK(Field->Runtime().Request().Movie.Movie == SecondOpeningMovie);
	Game->RunUntil([&] { return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().RunningMap == FirstMap; }, 3000, CrossHeld);
	REQUIRE(Loop.Field() != nullptr);
	CHECK(Loop.Field()->Runtime().Context().RunningMap == FirstMap);
}

TEST_CASE("Powered on with disc 2 alone, Continue with a disc 1 save asks for disc 1 and goes on at the save's map from its files", "[Menu][Content]")
{
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Second);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	FLateDrive Drive(*Game, *First);
	Drive.bThere = true;
	Game->Session->SetDiscDrive(&Drive);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallMenuGameLoopSecondDiscContinue";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	FGameState Saved = Game->Session->Game();
	Saved.SetPlace(VillageMap);
	REQUIRE_FALSE(Store.Write(0, WriteOriginalSave(Saved, 0, 0, Game->Session->Text().NameTables, Game->Session->Text().Font.LeadByte())));
	Game->Session->SetSaves(&Store);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
	REQUIRE(Menus.Running() != nullptr);

	// Continue, Load, the save, "Load this file?" Yes, "Finished", then "INSERT DISC 1" confirmed with it there.
	const std::array<std::pair<FPadState, int32>, 14> Presses{{{NoButtons, 40}, {CrossHeld, 2}, {NoButtons, 70}, {CrossHeld, 2}, {NoButtons, 40},
		{CrossHeld, 2}, {NoButtons, 30}, {LeftHeld, 2}, {NoButtons, 6}, {CrossHeld, 2}, {NoButtons, 60}, {CrossHeld, 2}, {NoButtons, 120},
		{CrossHeld, 2}}};
	for (const auto& [Held, Frames] : Presses)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held));
		}
	}
	Game->RunUntil([&] { return Menus.Running() == nullptr; }, 200);
	REQUIRE(Menus.Running() == nullptr);
	CHECK(Drive.Asked == std::vector<int32>{0});
	CHECK(Game->Session->DiscNumber() == 0);
	CHECK(&Game->Session->Image() == &First->Image());
	const auto OnSavedMap = [&] {
		return Loop.Field() != nullptr && Loop.Field()->Runtime().Context().MapNumber == VillageMap && Loop.Field()->Runtime().Context().bFieldRunning;
	};
	Game->RunUntil(OnSavedMap, 600);
	CHECK(OnSavedMap());
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("With no controller the Menu holds its frame with the sound paused, and the wait is no play time", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	Game->PowerOn();
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
	REQUIRE(Menus.Running() != nullptr);
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	const uint32 Number = Menus.Running()->Snapshot().Number;
	const uint32 PlayTime = Game->Session->Game().PlayTime();

	// The frame's input read finds no controller: the frame is held, however many vertical blanks pass.
	Game->Session->Pad().SetConnected(false);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
		CHECK(Menus.Running()->Snapshot().Number == Number);
	}

	// Back, the held frame is drawn and only its own vertical blank counts.
	Game->Session->Pad().SetConnected(true);
	REQUIRE_FALSE(Game->Frame());
	CHECK(Menus.Running()->Snapshot().Number == Number + 1);
	CHECK(Game->Session->Game().PlayTime() == PlayTime + 1);
	REQUIRE_FALSE(Game->Frame());
	CHECK(Menus.Running()->Snapshot().Number == Number + 2);
}
