#include "Field/EventMachine.h"
#include "Foundation/LittleEndian.h"
#include "Game/ClearFile.h"
#include "Game/FieldProgram.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Menu/MenuGameMenus.h"
#include "Menu/MenuProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace Weltall;

// The clear file: offered on the ending's last picture, it saves the game put back aboard the Yggdrasil before Deus.

namespace
{
	constexpr uint16 EndingMap = 41;
	/** Where map 41's script goes on after the movies: the wait before the picture. */
	constexpr uint16 AfterTheMovies = 0x61;
	/** The field's ticks the screen takes to fade to black once the line is confirmed. */
	constexpr int32 FadeTicks = 30;
	constexpr FPadState Nothing{};
	constexpr FPadState ConfirmHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState CancelHeld{FPadState::Bit(EPadButton::Circle)};
	constexpr FPadState LeftHeld{FPadState::Bit(EPadButton::Left)};
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint16 EllyBit = 1 << Elly;
	/** The first two of the characters free at the ending's point, in the game's order. */
	constexpr uint8 Bart = 3;
	constexpr uint8 Billy = 4;
	/** Where a save's data keeps the place, the story, the party and the characters who have joined. */
	constexpr std::size_t PlaceInData = 0x1B1E;
	constexpr std::size_t StoryInData = 0x1134;
	constexpr std::size_t PartyInData = 0x1124 + 0x1D34 - 0x1920;
	constexpr std::size_t JoinedInData = 0x1124 + 0x1D30 - 0x1920;

	/** The game as the closing scenes leave it, at the ending entered with its first actor at the wait after the movies. */
	void EnterEnding(Testing::FGameLoopSession& Game)
	{
		FGameState& State = Game.Session->Game();
		State.SetSavedEventVariable(0, 350);
		State.SetParty({Fei, Elly, FGameState::EmptySlot});
		State.SetJoinedCharacters(0x0FB9);
		State.SetMenuCharacters(0xF7FD);
		Game.EnterField(EndingMap);
		REQUIRE(Game.Loop->Field() != nullptr);
		FEventThread& Thread = Game.Loop->Field()->Runtime().Context().States[0].Threads[0];
		Thread.Pc = AfterTheMovies;
		Thread.Script = FEventMachine::DefaultScript;
		Thread.SetPriority(FEventMachine::DefaultPriority);
	}

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

	/** The clear file offered, Confirm pressed afresh and the screen's fade to black run out: the clear file's save opens. */
	void ConfirmOffer(Testing::FGameLoopSession& Game, const FMenuGameMenus& Menus)
	{
		FFieldProgram& Field = *Game.Loop->Field();
		Game.RunUntil([&] { return Field.OffersClearFile(); }, 800);
		REQUIRE_FALSE(Game.Frame(ConfirmHeld));
		Game.RunUntil([&] { return Menus.Running() != nullptr; }, FadeTicks + 1);
		REQUIRE(Menus.Running() != nullptr);
	}

	/** The File screen's title picture (the details' chapter, its second line), 4 bits a texel at (320, 224) of the menu's VRAM. */
	uint8 TitleTexel(const FMenuProgram& Menu, int32 X, int32 Y)
	{
		constexpr int32 TitleX = 0x140;
		constexpr int32 TitleY = 0xE0;
		const uint16 Unit = Menu.Vram().ConsoleUnits()[static_cast<std::size_t>((TitleY + Y) * FVram::Width + TitleX + X / 4)];
		return static_cast<uint8>(Unit >> (X % 4 * 4) & 0xF);
	}

	/** The glyph the title picture holds in its cell at Place (16 pixels a cell, 16 to a line): its body's pixels. */
	std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight> TitleCell(const FMenuProgram& Menu, int32 Place)
	{
		constexpr int32 GlyphTop = 2;
		std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight> Body{};
		for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
		{
			for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
			{
				Body[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)] = TitleTexel(Menu, Place % 16 * 16 + Column, Place / 16 * 16 + GlyphTop + Row) != 0;
			}
		}
		return Body;
	}

	std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight> GlyphBody(const FMesFont& Font, FMesCode Code)
	{
		const FMesFont::FCell Cell = Font.Cell(Code);
		std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight> Body{};
		for (std::size_t Pixel = 0; Pixel < Body.size(); ++Pixel)
		{
			Body[Pixel] = (Cell[Pixel] & 1) != 0;
		}
		return Body;
	}
}

TEST_CASE("The ending's last picture offers the clear file once it has shown alone: Confirm fades to black, asks to save and saves it before the title", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	FGameSession& Session = *Game->Session;
	Session.SetMenus(&Menus);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallClearFile";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	Session.SetSaves(&Store);
	EnterEnding(*Game);
	FFieldProgram& Field = *Game->Loop->Field();

	// No offer while the picture fades in; its script parks for good once it has.
	Game->RunUntil([&] { return Field.Runtime().Context().Render.bHiResStill; }, 200);
	CHECK_FALSE(ClearFile::IsEndingWaiting(Field.Runtime().Context()));
	Game->RunUntil([&] { return ClearFile::IsEndingWaiting(Field.Runtime().Context()); }, 200);
	REQUIRE(ClearFile::IsEndingWaiting(Field.Runtime().Context()));
	CHECK_FALSE(Field.OffersClearFile());

	// Confirm while the picture shows alone does nothing, nor does it held on once the clear file is offered.
	const int32 Waited = Game->Vblanks;
	Game->RunUntil([&] { return Field.OffersClearFile(); }, 400, ConfirmHeld);
	// Three seconds, the frame the script parked on among them.
	CHECK(Game->Vblanks - Waited + Field.Runtime().VblanksPerTick() == 180);
	CHECK(Menus.Running() == nullptr);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(ConfirmHeld));
	}
	REQUIRE_FALSE(Game->Frame());
	CHECK(Menus.Running() == nullptr);
	CHECK(Field.OffersClearFile());
	CHECK_FALSE(Session.Game().IsClearGame());

	// Pressed afresh: the screen fades to black as a script's fade-out darkens it, and only then is the game the clear
	// file's, its save open.
	const FFieldFade& Fade = Field.Runtime().Context().Render.Fades[0];
	CHECK_FALSE(Fade.bActive);
	REQUIRE_FALSE(Game->Frame(ConfirmHeld));
	CHECK(Menus.Running() == nullptr);
	CHECK(Fade.bActive);
	CHECK(Fade.BlendMode == 2);
	CHECK_FALSE(Field.OffersClearFile());
	int32 Darkness = Fade.DrawnColor().R;
	int32 Ticks = 0;
	while (Menus.Running() == nullptr && Ticks <= FadeTicks)
	{
		REQUIRE_FALSE(Game->Frame(ConfirmHeld));
		++Ticks;
		CHECK(Fade.DrawnColor().R > Darkness);
		Darkness = Fade.DrawnColor().R;
		if (Menus.Running() == nullptr)
		{
			CHECK_FALSE(Session.Game().IsClearGame());
		}
	}
	REQUIRE(Menus.Running() != nullptr);
	CHECK(Ticks == FadeTicks);
	CHECK(Fade.DrawnColor() == FMdlColor{0xFF, 0xFF, 0xFF});
	const FGameState& State = Session.Game();
	CHECK(State.IsClearGame());
	CHECK(State.Place() == 290);

	// The disc change's "Save?", Yes; Save, the first cell, "This block is free. Save?" Yes; the files stay for another,
	// the save listed with the font's star a space after "First and Last"; Cancel leaves, and the console starts again.
	const std::array<std::pair<FPadState, int32>, 11> Presses{{{Nothing, 80}, {ConfirmHeld, 2}, {Nothing, 80}, {ConfirmHeld, 2}, {Nothing, 40},
		{ConfirmHeld, 2}, {Nothing, 30}, {LeftHeld, 2}, {Nothing, 6}, {ConfirmHeld, 2}, {Nothing, 200}}};
	Play(*Game, Presses);
	REQUIRE(Menus.Running() != nullptr);
	const FMesFont& Font = Session.Text().Font;
	REQUIRE(Font.Star());
	constexpr int32 AfterTheChapter = 31;
	CHECK(TitleCell(*Menus.Running(), AfterTheChapter) == GlyphBody(Font, *Font.Star()));
	const std::array<std::pair<FPadState, int32>, 1> Leave{{{CancelHeld, 2}}};
	Play(*Game, Leave);
	Game->RunUntil([&] { return Game->Loop->Stage() == EGameLoopStage::SoftReset; }, 200);
	CHECK(Game->Loop->Stage() == EGameLoopStage::SoftReset);

	const auto Read = Store.Read(0);
	REQUIRE(Read);
	REQUIRE(Read->has_value());
	const FOriginalSave& Save = **Read;
	CHECK(Save.ClearGame() == EClearGame::ClearFile);
	// The game reads the bridge at 340 with Billy, the next free, in Elly's place; the port plays Fei, Elly and Bart there.
	CHECK(LittleEndian::ReadU16(Save.Data(), PlaceInData) == 290);
	CHECK(LittleEndian::ReadU16(Save.Data(), StoryInData) == 340);
	CHECK(Save.Data()[PartyInData] == Fei);
	CHECK(Save.Data()[PartyInData + 1] == Billy);
	CHECK(Save.Data()[PartyInData + 2] == Bart);
	CHECK((LittleEndian::ReadU16(Save.Data(), JoinedInData) & EllyBit) == 0);
	CHECK(Save.Party() == std::array<uint8, 3>{Fei, Billy, Bart});
	CHECK(Save.PlayedData()[PartyInData + 1] == Elly);
	CHECK((LittleEndian::ReadU16(Save.PlayedData(), JoinedInData) & EllyBit) != 0);
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("Declined, the clear file's save asks whether to go on without, No going on to the files, which ask again when left", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallClearFileSkip";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	Game->Session->SetSaves(&Store);
	EnterEnding(*Game);
	ConfirmOffer(*Game, Menus);

	// "Save?" No; "Are you sure you don't need to save?" No: the files. Cancel there asks again: No goes back to them;
	// Cancel again, then Yes.
	const std::array<std::pair<FPadState, int32>, 13> Back{{{Nothing, 80}, {LeftHeld, 2}, {Nothing, 6}, {ConfirmHeld, 2}, {Nothing, 60},
		{ConfirmHeld, 2}, {Nothing, 80}, {CancelHeld, 2}, {Nothing, 60}, {ConfirmHeld, 2}, {Nothing, 80}, {CancelHeld, 2}, {Nothing, 60}}};
	Play(*Game, Back);
	CHECK(Menus.Running() != nullptr);
	CHECK(Game->Loop->Stage() == EGameLoopStage::Program);
	const std::array<std::pair<FPadState, int32>, 3> Leave{{{LeftHeld, 2}, {Nothing, 6}, {ConfirmHeld, 2}}};
	Play(*Game, Leave);
	Game->RunUntil([&] { return Game->Loop->Stage() == EGameLoopStage::SoftReset; }, 200);
	CHECK(Game->Loop->Stage() == EGameLoopStage::SoftReset);
	const auto Read = Store.Read(0);
	REQUIRE(Read);
	CHECK_FALSE(Read->has_value());
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("The clear file's \"Save?\" answered No, then sure not to save, starts the game again with nothing saved", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallClearFileDeclined";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	FSaveStore Store(Folder);
	Game->Session->SetSaves(&Store);
	EnterEnding(*Game);
	ConfirmOffer(*Game, Menus);

	const std::array<std::pair<FPadState, int32>, 8> Declined{{{Nothing, 80}, {CancelHeld, 2}, {Nothing, 60}, {LeftHeld, 2}, {Nothing, 6},
		{ConfirmHeld, 2}, {Nothing, 2}, {Nothing, 2}}};
	Play(*Game, Declined);
	Game->RunUntil([&] { return Game->Loop->Stage() == EGameLoopStage::SoftReset; }, 200);
	CHECK(Game->Loop->Stage() == EGameLoopStage::SoftReset);
	for (int32 Slot = 0; Slot < FSaveStore::SlotsPerStore; ++Slot)
	{
		const auto Read = Store.Read(Slot);
		REQUIRE(Read);
		CHECK_FALSE(Read->has_value());
	}
	std::filesystem::remove_all(Folder, Error);
}

namespace
{
	/**
	 * The title's Continue, then Load, with Saved alone in the first store's first slot: the 32 cells of the title picture
	 * the details show the save with.
	 */
	std::vector<std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight>> ListedTitle(const FIso9660& Disc, FGameState& Saved, std::string_view Name)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		FMenuGameMenus Menus(*Game->Executable);
		FGameSession& Session = *Game->Session;
		Session.SetMenus(&Menus);
		const std::filesystem::path Folder = std::filesystem::temp_directory_path() / Name;
		std::error_code Error;
		std::filesystem::remove_all(Folder, Error);
		FSaveStore Store(Folder);
		REQUIRE_FALSE(Store.Write(0, WriteOriginalSave(Saved, 0, 1, Session.Text().NameTables, Session.Text().Font.LeadByte())));
		Session.SetSaves(&Store);
		Game->PowerOn();
		Game->RunUntil([&] { return Menus.Running() != nullptr; }, 2000, ConfirmHeld);
		REQUIRE(Menus.Running() != nullptr);
		const std::array<std::pair<FPadState, int32>, 5> Open{{{Nothing, 40}, {ConfirmHeld, 2}, {Nothing, 70}, {ConfirmHeld, 2}, {Nothing, 40}}};
		Play(*Game, Open);
		REQUIRE(Menus.Running() != nullptr);
		std::vector<std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight>> Cells;
		for (int32 Place = 0; Place < 32; ++Place)
		{
			Cells.push_back(TitleCell(*Menus.Running(), Place));
		}
		std::filesystem::remove_all(Folder, Error);
		return Cells;
	}
}

TEST_CASE("The File screen marks a clear game's save with the font's star after its chapter, another save of the point not", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMesFont& Font = Resident.Font;
	REQUIRE(Font.Star());

	// The bridge before Deus: the clear file made from the game at the ending, and the game as it was there.
	FGameState Clear;
	Clear.SetSavedEventVariable(0, 350);
	Clear.SetParty({Fei, Elly, FGameState::EmptySlot});
	Clear.SetJoinedCharacters(0x0FB9);
	Clear.SetMenuCharacters(0xF7FD);
	ClearFile::Make(Clear);
	FGameState Plain = Clear;
	Plain.SetClearGame(EClearGame::None);
	ClearFile::HideElly(Plain);

	// "First and Last" fills the second line's cells 16 to 29; the star stands a space after it.
	constexpr int32 LastLetter = 29;
	const std::array<bool, FMesFont::CellWidth * FMesFont::CellHeight> Blank{};
	const auto Listed = ListedTitle(*Disc, Plain, "WeltallClearFileListedPlain");
	CHECK(Listed[LastLetter] == GlyphBody(Font, *MesCodeOfCharacter('t')));
	CHECK(Listed[LastLetter + 1] == Blank);
	CHECK(Listed[LastLetter + 2] == Blank);
	const auto Marked = ListedTitle(*Disc, Clear, "WeltallClearFileListedClear");
	for (int32 Place = 0; Place <= LastLetter; ++Place)
	{
		INFO(Place);
		CHECK(Marked[static_cast<std::size_t>(Place)] == Listed[static_cast<std::size_t>(Place)]);
	}
	CHECK(Marked[LastLetter + 1] == Blank);
	CHECK(Marked[LastLetter + 2] == GlyphBody(Font, *Font.Star()));
}

TEST_CASE("Nothing is offered on the ending's map before its last picture, nor on another map", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(290);
	FFieldProgram& Field = *Game->Loop->Field();
	for (int32 Frame = 0; Frame < 300; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
		REQUIRE_FALSE(ClearFile::IsEndingWaiting(Field.Runtime().Context()));
	}
	CHECK_FALSE(Field.OffersClearFile());
}

TEST_CASE("A clear file starts on the Yggdrasil's bridge with Fei, Elly and Bart, every gear restored", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetSavedEventVariable(0, 350);
	State.SetParty({Fei, Elly, FGameState::EmptySlot});
	State.SetJoinedCharacters(0x0FB9);
	State.SetMenuCharacters(0xF7FD);
	constexpr int32 Regurus = 10;
	const std::optional<uint32> Full = State.GearMaxHp(Regurus);
	REQUIRE(Full);
	REQUIRE(State.SetGearHp(Regurus, 1));
	ClearFile::Make(State);
	Game->EnterField(ClearFile::Start.Map);
	FFieldProgram& Field = *Game->Loop->Field();
	Game->RunUntil([&] { return Field.Runtime().Context().bFieldRunning && Field.Runtime().Context().bPlayerControlled; }, 600);
	const FEventContext& Context = Field.Runtime().Context();
	CHECK(Context.MapNumber == 290);
	CHECK(Context.bPlayerControlled);
	CHECK(Context.PartyCharacters == std::array<uint8, 3>{Fei, Elly, Bart});
	CHECK(State.GearHp(Regurus) == Full);
	CHECK(State.SavedEventVariable(0) == 340);
}
