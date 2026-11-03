#include "Field/FieldRequests.h"
#include "Game/ClearFile.h"
#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Menu/MenuGameMenus.h"
#include "Menu/MenuNewGamePlusPage.h"
#include "Menu/MenuProgram.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>

using namespace Weltall;

// The New Game+: the title's New Game, with a save of the clear file's game in the stores, asks "Start a New Game+?";
// Yes picks a clear save on the load screen, whose page then sets what carries over.
namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState CircleHeld{FPadState::Bit(EPadButton::Circle)};
	constexpr FPadState LeftHeld{FPadState::Bit(EPadButton::Left)};
	constexpr FPadState RightHeld{FPadState::Bit(EPadButton::Right)};
	constexpr FPadState UpHeld{FPadState::Bit(EPadButton::Up)};
	constexpr FPadState DownHeld{FPadState::Bit(EPadButton::Down)};
	/** The opening's first map, the bridge the clear file starts on. */
	constexpr uint16 FirstMap = 4;
	constexpr uint16 Bridge = 290;
	constexpr int32 SecondOpeningMovie = 13;
	/** What the clear save holds that a new game does not: its money, Fei's level (character record +62h), the 7-AP flag. */
	constexpr uint32 ClearGold = 4321000;
	constexpr std::size_t FeiLevel = 0x26C + 0x62;
	constexpr uint8 ClearLevel = 77;
	constexpr uint16 Limiter = 0x4000;
	/** Who has joined and who is in the menus at the ending's point, Elly away; Regurus, her gear in the clear file. */
	constexpr uint16 JoinedAtTheEnd = 0x0FB9;
	constexpr uint16 InTheMenusAtTheEnd = 0xF7FD;
	constexpr int32 Regurus = 10;

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
	 * Both discs there; in the first store an ordinary save, and under it (unless bPlainOnly) a save of the clear file's
	 * game. The ordinary one is saved last, so the load screen's cursor starts on it.
	 */
	struct FWithClearSave
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		FMenuGameMenus Menus;
		std::filesystem::path Folder;
		std::unique_ptr<FSaveStore> Store;

		FWithClearSave(const FIso9660& First, const FIso9660& Second, const char* Name, bool bPlainOnly = false)
			: Game(Testing::FGameLoopSession::Start(First)), Menus(*Game->Executable), Folder(std::filesystem::temp_directory_path() / Name)
		{
			Game->Session->SetMenus(&Menus);
			Game->AddDisc(Second);
			std::error_code Error;
			std::filesystem::remove_all(Folder, Error);
			Store = std::make_unique<FSaveStore>(Folder);
			const FMesNameTables& Names = Game->Session->Text().NameTables;
			const uint8 Lead = Game->Session->Text().Font.LeadByte();
			if (!bPlainOnly)
			{
				FGameState Ended = Game->Session->Game();
				Ended.SetSavedEventVariable(0, 350);
				Ended.SetGold(ClearGold);
				Ended.Block()[FeiLevel] = ClearLevel;
				Ended.SetStoryFlags(static_cast<uint16>(Ended.StoryFlags() | Limiter));
				Ended.SetJoinedCharacters(JoinedAtTheEnd);
				Ended.SetMenuCharacters(InTheMenusAtTheEnd);
				REQUIRE(Ended.SetGearHp(Regurus, 1));
				ClearFile::Make(Ended);
				REQUIRE(Ended.ClearGame() == EClearGame::ClearFile);
				REQUIRE_FALSE(Store->Write(1, WriteOriginalSave(Ended, 1, 1, Names, Lead)));
			}
			FGameState Plain = Game->Session->Game();
			REQUIRE_FALSE(Store->Write(0, WriteOriginalSave(Plain, 0, 0, Names, Lead)));
			Game->Session->SetSaves(Store.get());
		}

		~FWithClearSave()
		{
			std::error_code Error;
			std::filesystem::remove_all(Folder, Error);
		}

		/** From power-on to the title's arc, its cursor on Continue. */
		void ToTitle()
		{
			Game->PowerOn();
			Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, CrossHeld);
			REQUIRE(Menus.Running() != nullptr);
			Play(*Game, std::array<std::pair<FPadState, int32>, 1>{{{NoButtons, 40}}});
		}

		/** New Game on the title (Up from Continue), and the frames for what it opens. */
		void NewGame()
		{
			Press(UpHeld, 4);
			Press(CrossHeld, 120);
		}

		/** A button pressed, and the frames after it. */
		void Press(FPadState Button, int32 FramesAfter = 12)
		{
			Play(*Game, std::array<std::pair<FPadState, int32>, 2>{{{Button, 2}, {NoButtons, FramesAfter}}});
		}

		/** Yes to the question, Load on the load screen, the ordinary save refused ("Load this file?" not asked), the clear save (the cell below) loaded. */
		void PickClearSave()
		{
			Press(CrossHeld, 100);
			Press(CrossHeld, 60);
			// The ordinary save refused: no window opens over the screen.
			const std::size_t Shown = Menus.Running()->Snapshot().Primitives.size();
			Press(CrossHeld, 40);
			REQUIRE(Menus.Running() != nullptr);
			CHECK(State().ClearGame() == EClearGame::None);
			CHECK(Menus.Running()->Snapshot().Primitives.size() == Shown);
			Press(DownHeld, 12);
			Press(CrossHeld, 40);
			Press(LeftHeld, 12);
			Press(CrossHeld, 80);
			Press(CrossHeld, 100);
		}

		FGameState& State() { return Game->Session->Game(); }
	};
}

TEST_CASE("With only ordinary saves in the stores, New Game asks nothing and is the original's", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	FWithClearSave With(*First, *Second, "WeltallMenuNewGamePlusNone", true);
	With.ToTitle();
	With.NewGame();
	With.Game->RunUntil([&] { return With.Menus.Running() == nullptr; }, 200);
	REQUIRE(With.Menus.Running() == nullptr);
	CHECK(With.State().Place() == FirstMap);
	CHECK(With.State().ClearGame() == EClearGame::None);
}

TEST_CASE("With a clear save in the stores, New Game asks Start a New Game+?, and No is the original's New Game", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	FWithClearSave With(*First, *Second, "WeltallMenuNewGamePlusNo");
	With.ToTitle();
	With.NewGame();
	// The question waits; its pointer starts on Yes.
	Play(*With.Game, std::array<std::pair<FPadState, int32>, 1>{{{NoButtons, 60}}});
	REQUIRE(With.Menus.Running() != nullptr);
	With.Press(LeftHeld);
	With.Press(CrossHeld, 0);
	With.Game->RunUntil([&] { return With.Menus.Running() == nullptr; }, 200);
	REQUIRE(With.Menus.Running() == nullptr);
	CHECK(With.State().Place() == FirstMap);
	CHECK(With.State().ClearGame() == EClearGame::None);
	CHECK(With.State().Gold() != ClearGold);
}

TEST_CASE("Yes picks a clear save on the load screen, others refused; its page's Keep, Removed and Start begin the story with them", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	FWithClearSave With(*First, *Second, "WeltallMenuNewGamePlusStart");
	With.ToTitle();
	With.NewGame();
	With.PickClearSave();
	REQUIRE(With.Menus.Running() != nullptr);
	// The clear save is loaded under the page.
	CHECK(With.State().ClearGame() == EClearGame::ClearFile);
	// Money kept (its row first, Right), Levels kept (four rows down, Confirm), the Limiter removed (Right), Elly left as
	// she is, then Start, the list scrolled down a row to it.
	With.Press(RightHeld);
	for (int32 Row = 0; Row < 4; ++Row)
	{
		With.Press(DownHeld);
	}
	With.Press(CrossHeld);
	With.Press(DownHeld);
	With.Press(RightHeld);
	With.Press(DownHeld);
	With.Press(DownHeld);
	With.Press(CrossHeld, 0);
	With.Game->RunUntil([&] { return With.Menus.Running() == nullptr; }, 200);
	REQUIRE(With.Menus.Running() == nullptr);

	// A new game, as the title's New Game makes it, with the clear save's money, levels and flag: disc 1, the opening's first map.
	FGameState& State = With.State();
	CHECK(State.ClearGame() == EClearGame::NewGamePlus);
	CHECK(State.ProgramResult == 0);
	CHECK(State.Place() == FirstMap);
	CHECK(State.SavedEventVariable(0) == 0);
	CHECK(State.MenuLastCommand == 6);
	CHECK(State.Gold() == ClearGold);
	CHECK(State.Block()[FeiLevel] == ClearLevel);
	CHECK((State.StoryFlags() & Limiter) != 0);
	CHECK(With.Game->Session->DiscNumber() == 0);
	FFieldProgram* Field = With.Game->Loop->Field();
	REQUIRE(Field != nullptr);
	With.Game->RunUntil([&] { return Field->Movie() != nullptr; }, 600);
	REQUIRE(Field->Runtime().Request().Kind == EFieldRequest::Movie);
	CHECK(Field->Runtime().Request().Movie.Movie == SecondOpeningMovie);

	// Its saves keep it a New Game+, with Fei's and Elly's looks to switch.
	const FOriginalSave Save = WriteOriginalSave(State, 1, 0, With.Game->Session->Text().NameTables, With.Game->Session->Text().Font.LeadByte());
	CHECK(Save.ClearGame() == EClearGame::NewGamePlus);
	CHECK(FCharacterLooks::CanSwitch(0, State.IsClearGame()));
	CHECK(FCharacterLooks::CanSwitch(1, State.IsClearGame()));
}

TEST_CASE("Cancel steps back from the page to the saves, the question and the title, whose game is as it was", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	FWithClearSave With(*First, *Second, "WeltallMenuNewGamePlusBack");
	With.ToTitle();
	const FGameState Title = With.State();
	With.NewGame();
	With.PickClearSave();
	REQUIRE(With.State().ClearGame() == EClearGame::ClearFile);
	// The page, the load screen (its title's game back), the question: each Cancel a step back.
	With.Press(CircleHeld, 100);
	REQUIRE(With.Menus.Running() != nullptr);
	CHECK(With.State().ClearGame() == EClearGame::None);
	With.Press(CircleHeld, 100);
	REQUIRE(With.Menus.Running() != nullptr);
	With.Press(CircleHeld, 60);
	REQUIRE(With.Menus.Running() != nullptr);
	CHECK(std::equal(Title.Block().begin(), Title.Block().end(), With.State().Block().begin()));
	// On the title's arc again, New Game where it was: asked again, No.
	With.Press(CrossHeld, 100);
	With.Press(LeftHeld);
	With.Press(CrossHeld, 0);
	With.Game->RunUntil([&] { return With.Menus.Running() == nullptr; }, 200);
	REQUIRE(With.Menus.Running() == nullptr);
	CHECK(With.State().Place() == FirstMap);
	CHECK(With.State().ClearGame() == EClearGame::None);
}

TEST_CASE("A clear save loaded from Continue goes on aboard as the clear file's game, with no question, the ship ready", "[Menu][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	FWithClearSave With(*First, *Second, "WeltallMenuNewGamePlusContinue");
	With.ToTitle();
	// Continue, Load, the clear save (the cell below the first), "Load this file?" Yes, "Finished".
	With.Press(CrossHeld, 70);
	With.Press(CrossHeld, 40);
	With.Press(DownHeld, 8);
	With.Press(CrossHeld, 30);
	With.Press(LeftHeld, 6);
	With.Press(CrossHeld, 60);
	With.Press(CrossHeld, 0);
	With.Game->RunUntil([&] { return With.Menus.Running() == nullptr; }, 200);
	REQUIRE(With.Menus.Running() == nullptr);
	CHECK(With.State().ProgramResult == 2);
	CHECK(With.State().ClearGame() == EClearGame::ClearFile);
	CHECK(With.State().Place() == Bridge);
	CHECK(With.State().Gold() == ClearGold);
	CHECK(With.Game->Session->DiscNumber() == 1);

	// The bridge as a load enters it (by the scene's entrance, not the world map's): the gears restored, the launch
	// offered (1C8h bit 14) and a party of three, which the ship's member change needs.
	With.Game->RunUntil([&] {
		const FFieldProgram* Field = With.Game->Loop->Field();
		return Field != nullptr && Field->Runtime().Context().bFieldRunning && Field->Runtime().Context().bPlayerControlled;
	}, 600);
	const FEventContext& Context = With.Game->Loop->Field()->Runtime().Context();
	CHECK(Context.MapNumber == Bridge);
	CHECK(Context.Variables.Raw(0x002) == 4);
	CHECK((Context.Variables.Raw(0x1C8) & 0x4000) != 0);
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Bart = 3;
	CHECK(Context.PartyCharacters == std::array<uint8, 3>{Fei, Elly, Bart});
	CHECK(Context.Variables.Raw(0x042) == Bart);
	CHECK(With.State().GearHp(Regurus) == With.State().GearMaxHp(Regurus));
}

TEST_CASE("The New Game+ page sets each part Reset or Keep, the Limiter Intact or Removed, Elly Leaves or Returns, Start last", "[Menu]")
{
	namespace Text = OptionsText::NewGamePlus;
	FMenuNewGamePlusPage Page;
	std::vector<FSettingsRow> Rows = Page.Rows();
	REQUIRE(Rows.size() == FMenuNewGamePlusPage::StartRow + 1);
	for (int32 Row = 0; Row < FMenuNewGamePlusPage::LimiterRow; ++Row)
	{
		CHECK(Rows[static_cast<std::size_t>(Row)].Value == Text::Reset);
	}
	const FSettingsRow& LimiterRow = Rows[FMenuNewGamePlusPage::LimiterRow];
	CHECK(LimiterRow.Label == Text::Limiter);
	CHECK(LimiterRow.Value == Text::Intact);
	CHECK(LimiterRow.Help[0] == Text::IntactHelp[0]);
	const FSettingsRow& EllyRow = Rows[FMenuNewGamePlusPage::EllyRow];
	CHECK(EllyRow.Label == Text::Elly);
	CHECK(EllyRow.Value == Text::Leaves);
	CHECK(EllyRow.Help[0] == Text::LeavesHelp[0]);
	CHECK(Page.Carry() == FNewGamePlusCarry{});
	CHECK(Rows.back().Label == Text::Start.Label);
	CHECK_FALSE(Page.Switch(FMenuNewGamePlusPage::StartRow));
	for (int32 Row = 0; Row < FMenuNewGamePlusPage::StartRow; ++Row)
	{
		CHECK(Page.Switch(Row));
	}
	CHECK(Page.Carry() == FNewGamePlusCarry{true, true, true, true, true, true, true});
	Rows = Page.Rows();
	CHECK(Rows[4].Label == Text::Levels.Label);
	CHECK(Rows[4].Value == Text::Keep);
	CHECK(Rows[FMenuNewGamePlusPage::LimiterRow].Value == Text::Removed);
	CHECK(Rows[FMenuNewGamePlusPage::LimiterRow].Help[0] == Text::RemovedHelp[0]);
	CHECK(Rows[FMenuNewGamePlusPage::EllyRow].Value == Text::Returns);
	CHECK(Rows[FMenuNewGamePlusPage::EllyRow].Help[0] == Text::ReturnsHelp[0]);
	CHECK(Page.Switch(4));
	CHECK_FALSE(Page.Carry().bLevels);
}

TEST_CASE("The New Game+ question's and page's words are in the game's font and fit their windows", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
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
	// The question's window as the second disc's (MenuStoryModeTests.cpp).
	constexpr int32 WindowWidth = 0x110;
	constexpr int32 Margin = 0x10;
	constexpr int32 TypedWidth = 0x3D * 4;
	constexpr int32 LeftAnswerX = 0x24;
	constexpr int32 RightAnswerX = 0xAC;
	constexpr int32 PointerRoom = 3 + 17;
	const OptionsText::FOffer& Offer = OptionsText::NewGamePlus::Offer;
	std::vector<std::string_view> Lines{Offer.Question};
	Lines.insert(Lines.end(), Offer.LeftHelp.begin(), Offer.LeftHelp.end());
	Lines.insert(Lines.end(), Offer.RightHelp.begin(), Offer.RightHelp.end());
	for (const std::string_view Line : Lines)
	{
		INFO(Line);
		CHECK(Width(Line) <= WindowWidth - 2 * Margin);
		CHECK(Width(Line) <= TypedWidth);
	}
	CHECK(LeftAnswerX + Width(Offer.Left) + PointerRoom <= RightAnswerX);
	CHECK(RightAnswerX + Width(Offer.Right) + PointerRoom <= WindowWidth - Margin);

	// The page: the title, each row's label and its longest value side by side, each help line (the Limiter's and Elly's
	// both) in its strip.
	constexpr int32 LabelX = 0x28;
	constexpr int32 LabelGap = 8;
	constexpr int32 ValueEnd = 0x128;
	namespace Words = OptionsText::NewGamePlus;
	CHECK(Width(FMenuNewGamePlusPage::Title()) <= TypedWidth);
	FMenuNewGamePlusPage Page;
	Page.Switch(FMenuNewGamePlusPage::LimiterRow);
	Page.Switch(FMenuNewGamePlusPage::EllyRow);
	for (const std::vector<FSettingsRow>& Rows : {FMenuNewGamePlusPage().Rows(), Page.Rows()})
	{
		for (const FSettingsRow& Row : Rows)
		{
			INFO(Row.Label);
			for (const std::string_view Value : {Words::Reset, Words::Keep, Words::Intact, Words::Removed, Words::Leaves, Words::Returns})
			{
				CHECK(LabelX + Width(Row.Label) + LabelGap <= ValueEnd - Width(Value));
			}
			for (const std::string& Line : Row.Help)
			{
				INFO(Line);
				CHECK(Width(Line) <= TypedWidth);
			}
		}
	}
}
