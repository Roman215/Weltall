#include "Game/FieldProgram.h"
#include "Game/KeepElly.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

// Keep Elly played on the second disc's own scripts: she is back the first time the player has the party after Golgoda.

namespace
{
	constexpr uint16 MerkavasCore = 709;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint16 EllyBit = 1 << Elly;
	constexpr uint8 Bart = 3;
	constexpr uint8 Citan = 9;
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	/** Who Member Change offers: joined, in the menus' other mask, one of the eleven. */
	constexpr uint16 Playable = 0x7FF;
	constexpr FPadState ConfirmHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr int32 ArrivalFrames = 6000;
	/** The field's map number under the flags it carries above it. */
	constexpr int32 MapBits = 0x3FFF;

	/**
	 * The game as the chair's narration leaves it on its way into Merkava (story 281): Fei locked, Citan and Bart beside
	 * him, Elly cleared from the joined set and in Vierge, everyone else free.
	 */
	void EnterMerkava(Testing::FGameLoopSession& Game, EClearGame Kind)
	{
		FGameState& State = Game.Session->Game();
		State.SetClearGame(Kind);
		State.SetSavedEventVariable(0, 281);
		State.SetJoinedCharacters(static_cast<uint16>(0x07B9));
		State.SetMenuCharacters(0xFFFF);
		State.SetLockedCharacters(1 << Fei);
		State.SetCharacterGear(Elly, Vierge);
		State.SetParty({Fei, Citan, Bart});
		Game.EnterField(MerkavasCore);
		REQUIRE(Game.Loop->Field() != nullptr);
		// Its arrival scene, Confirm tapped through its lines, until the player has the party.
		const FEventContext& Context = Game.Loop->Field()->Runtime().Context();
		for (int32 Frame = 0; Frame < ArrivalFrames && !(Context.bFieldRunning && Context.bPlayerControlled); ++Frame)
		{
			REQUIRE_FALSE(Game.Frame(Frame % 24 < 2 ? ConfirmHeld : FPadState{}));
		}
		REQUIRE(Context.bPlayerControlled);
		CHECK((Context.MapNumber & MapBits) == MerkavasCore);
		CHECK(State.SavedEventVariable(0) == KeepElly::ReturnStory);
	}

	bool Offered(const FGameState& State, uint8 Character)
	{
		return (State.JoinedCharacters() & State.MenuCharacters() & Playable & 1 << Character) != 0;
	}
}

TEST_CASE("Keep Elly has Elly back in Merkava's core, joined and in Regurus for its member change, the party as the scripts built it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterMerkava(*Game, EClearGame::KeepElly);
	const FGameState& State = Game->Session->Game();
	CHECK(Offered(State, Elly));
	CHECK(State.CharacterGear(Elly) == Regurus);
	CHECK(Game->Loop->Field()->Runtime().Context().PartyCharacters == std::array<uint8, 3>{Fei, Citan, Bart});
	CHECK(State.LockedCharacters() == 1 << Fei);
}

TEST_CASE("A New Game+ without Keep Elly reaches Merkava's core with Elly away, as the original", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterMerkava(*Game, EClearGame::NewGamePlus);
	const FGameState& State = Game->Session->Game();
	CHECK_FALSE(Offered(State, Elly));
	CHECK((State.JoinedCharacters() & EllyBit) == 0);
	CHECK(State.CharacterGear(Elly) == Vierge);
}
