#include "Field/EventMachine.h"
#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The French translation's credits, which its kernel shows as the game's end reads its last picture.

namespace
{
	/** Map 41 on disc 2: the ending's movies, then the picture "XENOGEARS Episode V FIN", held for good. */
	constexpr uint16 EndingMap = 41;
	/** Where its script goes on after the movies (4, 5 and 0: some 11,000 pictures): the wait before the picture. */
	constexpr uint16 AfterTheMovies = 0x61;
	constexpr FPadState Nothing{};
	constexpr FPadState StartHeld{FPadState::Bit(EPadButton::Start)};
	/** The US release's confirm, the game's Circle; its Circle is the game's Cross. */
	constexpr FPadState ConfirmHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState CancelHeld{FPadState::Bit(EPadButton::Circle)};

	/** The ending entered with its first actor's script at the wait after the movies. */
	void EnterEndingAfterMovies(Testing::FGameLoopSession& Game)
	{
		Game.EnterField(EndingMap);
		REQUIRE(Game.Loop->Field() != nullptr);
		FEventThread& Thread = Game.Loop->Field()->Runtime().Context().States[0].Threads[0];
		Thread.Pc = AfterTheMovies;
		Thread.Script = FEventMachine::DefaultScript;
		Thread.SetPriority(FEventMachine::DefaultPriority);
	}
}

TEST_CASE("The French translation's ending shows its credits with song 8 before its last picture, until Circle or Start", "[Game][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterEndingAfterMovies(*Game);
	FFieldProgram& Field = *Game->Loop->Field();
	Game->RunUntil([&] { return Field.Credits() != nullptr; }, 200);
	REQUIRE(Field.Credits() != nullptr);
	// The tick that read the picture stands still while they show; their song plays from its start.
	CHECK(Field.Runtime().Context().ScriptImage.has_value());
	CHECK_FALSE(Field.Runtime().Context().Render.bHiResStill);
	const FFieldMusic& Music = Game->Session->Sound().FieldMusic();
	CHECK(Music.Song() == FBootNotice::TranslationCreditsSong);
	CHECK(Music.Sequence() != nullptr);
	CHECK(Game->Loop->VblanksPerFrame() == 1);

	// Start held as they come, and the game's Cross, leave them up.
	const int32 Shown = Game->Vblanks;
	Game->RunUntil([&] { return Game->Vblanks - Shown == 300; }, 400, StartHeld);
	Game->Frame(CancelHeld);
	Game->Frame(Nothing);
	CHECK(Field.Credits() != nullptr);

	// The game's Circle: 17 blanks to black, then the field goes on to its picture.
	Game->Frame(ConfirmHeld);
	const int32 Pressed = Game->Vblanks;
	Game->RunUntil([&] { return Field.Credits() == nullptr; }, 40);
	CHECK(Game->Vblanks - Pressed == 17);
	Game->RunUntil([&] { return Field.Runtime().Context().Render.bHiResStill; }, 100);
	CHECK(Field.Runtime().Context().Render.bHiResStill);
	CHECK(Music.Song() == FBootNotice::TranslationCreditsSong);
}

TEST_CASE("The USA's ending goes from its movies to its last picture with no screen between", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterEndingAfterMovies(*Game);
	FFieldProgram& Field = *Game->Loop->Field();
	Game->RunUntil([&] { return Field.Credits() != nullptr || Field.Runtime().Context().Render.bHiResStill; }, 200);
	CHECK(Field.Credits() == nullptr);
	CHECK(Field.Runtime().Context().Render.bHiResStill);
	CHECK(Game->Session->Sound().FieldMusic().Song() == FFieldMusic::NoSong);
}
