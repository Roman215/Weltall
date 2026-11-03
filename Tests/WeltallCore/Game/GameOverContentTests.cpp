#include "Battle/BattleContext.h"
#include "Field/FieldMap.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Game/PlayerSettings.h"
#include "Graphics/Vram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	/** Map 199, Nortune's D-block: its encounter (formation 0) is too much for a new game's party a point from death. */
	constexpr uint16 DBlock = 199;
	constexpr uint8 DBlockEncounter = 0;
	/** Where the title's map has its picture, and the defeat's picture goes after a lost battle. */
	constexpr FVramRect TitlePicture{0x200, 0, 0x140, 0xE0};

	/** The field's game over screen asked for the title's menu (Load Game). */
	bool AskedForTheTitle(const Testing::FGameLoopSession& Game)
	{
		const FFieldProgram* Field = Game.Loop->Field();
		return Field != nullptr && Field->Runtime().Context().MenusPending != 0;
	}

	/**
	 * A new game's party a point from death in Nortune's D-block, its encounter fought with the confirm button pressed
	 * turn after turn and held down as the party falls: the game over on the title's map.
	 */
	void LoseInTheDBlock(Testing::FGameLoopSession& Game)
	{
		FGameState& State = Game.Session->Game();
		State.SetParty({0, 1, 2});
		State.SetJoinedCharacters(0x1FF);
		Game.EnterField(DBlock);
		FFieldProgram* Field = Game.Loop->Field();
		REQUIRE(Field != nullptr);
		Game.RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		REQUIRE_FALSE(Game.Frame());
		for (const int32 Character : {0, 1, 2})
		{
			State.SetCharacterPoints(Character, FGameState::ECharacterPoints::Hp, 1);
		}
		FEventContext& Context = Field->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = false;
		Context.Encounters.bBattleDrawn = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = DBlockEncounter;
		Game.RunUntil([&] { return Game.Loop->Program() == EGameProgram::Battle; }, 8);
		REQUIRE(Game.Loop->Program() == EGameProgram::Battle);
		const int32 Runs = Game.Loop->ProgramRuns();
		for (int32 Frame = 0; Frame < 12000 && Game.Loop->ProgramRuns() == Runs; ++Frame)
		{
			REQUIRE_FALSE(Game.Frame(Frame % 24 < 2 ? CrossHeld : NoButtons));
		}
		REQUIRE(Game.Loop->Program() == EGameProgram::Field);
		REQUIRE(State.ProgramResult == BattleModeResult::Lost);
		REQUIRE(Game.Loop->Field()->Runtime().Context().RunningMap == FKernelState::TitleMap);
	}
}

TEST_CASE("After a lost battle the game over takes no button held from the fight, and a fresh press leaves it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	LoseInTheDBlock(*Game);
	// The game over's screen leaves on the confirm button held from its 32nd tick: held from the fight, it stays.
	for (int32 Frame = 0; Frame < 300; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(CrossHeld));
		REQUIRE_FALSE(AskedForTheTitle(*Game));
	}
	// Let go and pressed afresh, the title's menu is asked for.
	REQUIRE_FALSE(Game->Frame(NoButtons));
	REQUIRE_FALSE(Game->Frame(NoButtons));
	CHECK(Game->RunUntil([&] { return AskedForTheTitle(*Game); }, 30, CrossHeld) < 30);
}

TEST_CASE("The game over takes no press while the battle's music fades, and the confirm button leaves it after", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	LoseInTheDBlock(*Game);
	// The battle's music fades over 240 vertical blanks, the field's ticks two each: nothing pressed in them is taken.
	for (int32 Frame = 0; Frame < 110; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 6 == 0 ? CrossHeld : NoButtons));
		REQUIRE_FALSE(AskedForTheTitle(*Game));
	}
	// Its last ticks pass with nothing held, then the button leaves it, as the original's screen does.
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(NoButtons));
		REQUIRE_FALSE(AskedForTheTitle(*Game));
	}
	CHECK(Game->RunUntil([&] { return AskedForTheTitle(*Game); }, 30, CrossHeld) < 30);
}

TEST_CASE("A game over left alone plays the opening's movie, and the title's own picture streams back in where the defeat's was", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	LoseInTheDBlock(*Game);
	FFieldProgram* Field = Game->Loop->Field();
	CHECK(Field->Runtime().Context().Render.bDefeatPicture);
	// The title's picture, which the map's texture stream places where the defeat's picture is copied.
	FVram Streamed;
	UploadFieldTextureStream(Field->Runtime().Map(), Streamed, false);
	const auto TitleShown = [&] {
		const FVram& Vram = Field->Drawing().Vram();
		for (int16 Y = 0; Y < TitlePicture.Height; ++Y)
		{
			for (int16 X = 0; X < TitlePicture.Width; ++X)
			{
				if (Vram.At(TitlePicture.X + X, TitlePicture.Y + Y) != Streamed.At(TitlePicture.X + X, TitlePicture.Y + Y))
				{
					return false;
				}
			}
		}
		return true;
	};
	CHECK_FALSE(TitleShown());

	// 960 ticks without the confirm button, a fade, then the opening's movie, which the button skips.
	const auto MoviePlays = [&] { return Field->Runtime().Request().Kind == EFieldRequest::Movie; };
	REQUIRE(Game->RunUntil(MoviePlays, 1200) < 1200);
	REQUIRE(Game->RunUntil([&] { return !MoviePlays(); }, 30, CrossHeld) < 30);
	CHECK(TitleShown());
	CHECK_FALSE(Field->Runtime().Context().Render.bDefeatPicture);
}

TEST_CASE("A scripted battle lost where its script plans no defeat ends in the game over, or as the original does with the fix off", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Bart against Ramsus in Aveh's castle (map 152's formation 13, a battle of kind 1): a battle event whose script plans
	// no defeat. The original keeps the result the field left (none) and enters the title's map as any map.
	constexpr uint16 CastleHall = 152;
	constexpr uint8 RamsusFight = 13;
	constexpr uint8 EventKind = 1;
	constexpr uint16 InTheCastle = 70;
	for (const bool bFixed : {true, false})
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Fixes.BattleEvents.bUnscriptedDefeatIsLost = bFixed;
		Game->Session->SetSettings(Settings);
		FGameState& State = Game->Session->Game();
		State.SetParty({3, 0xFF, 0xFF});
		State.SetJoinedCharacters(0x1FF);
		State.SetSavedEventVariable(0, InTheCastle);
		Game->EnterField(CastleHall);
		FFieldProgram* Field = Game->Loop->Field();
		REQUIRE(Field != nullptr);
		Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		REQUIRE_FALSE(Game->Frame());
		State.SetCharacterPoints(3, FGameState::ECharacterPoints::Hp, 1);
		FEventContext& Context = Field->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = EventKind;
		Context.Encounters.Formation = RamsusFight;
		Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 8);
		REQUIRE(Game->Loop->Program() == EGameProgram::Battle);
		const int32 Runs = Game->Loop->ProgramRuns();
		for (int32 Frame = 0; Frame < 20000 && Game->Loop->ProgramRuns() == Runs; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Frame % 24 < 2 ? CrossHeld : NoButtons));
		}
		INFO("fixed " << bFixed);
		REQUIRE(Game->Loop->Program() == EGameProgram::Field);
		REQUIRE(Game->Loop->Field()->Runtime().Context().RunningMap == FKernelState::TitleMap);
		if (bFixed)
		{
			// The game over: a press held from the fight is not taken.
			CHECK(State.ProgramResult == BattleModeResult::Lost);
			for (int32 Frame = 0; Frame < 100; ++Frame)
			{
				REQUIRE_FALSE(Game->Frame(CrossHeld));
				REQUIRE_FALSE(AskedForTheTitle(*Game));
			}
		}
		else
		{
			CHECK(State.ProgramResult != BattleModeResult::Lost);
		}
	}
}
