#include "Game/KeepElly.h"

#include "Game/GameState.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>

using namespace Weltall;

namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint16 FeiBit = 1 << Fei;
	constexpr uint16 EllyBit = 1 << Elly;
	constexpr uint8 Bart = 3;
	constexpr uint8 Billy = 4;
	constexpr uint8 Citan = 9;
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	constexpr int32 Story = 0;

	/**
	 * A Keep Elly game at the story's value as the scripts leave it after Golgoda: Elly not joined and in Vierge, still
	 * in the menus' other mask as everyone is, Fei, Citan and Bart in the party.
	 */
	FGameState AfterGolgoda(uint16 Progress)
	{
		FGameState State;
		State.SetClearGame(EClearGame::KeepElly);
		State.SetSavedEventVariable(Story, Progress);
		State.SetJoinedCharacters(static_cast<uint16>(0x7FF & ~EllyBit));
		State.SetMenuCharacters(static_cast<uint16>(0x7FF & ~EllyBit));
		State.SetCharacterGear(Elly, Vierge);
		State.SetParty({Fei, Citan, Bart});
		return State;
	}

	bool Untouched(const FGameState& State, const FGameState& Before)
	{
		return std::equal(State.Block().begin(), State.Block().end(), Before.Block().begin());
	}
}

TEST_CASE("Keep Elly holds Elly a member from Merkava on: joined, in the menus, in Regurus, the party left as it is", "[Game]")
{
	FGameState State = AfterGolgoda(KeepElly::ReturnStory);
	CHECK(KeepElly::IsBack(State));
	KeepElly::Hold(State);
	CHECK((State.JoinedCharacters() & EllyBit) != 0);
	CHECK((State.MenuCharacters() & EllyBit) != 0);
	CHECK(State.CharacterGear(Elly) == Regurus);
	CHECK(State.Party() == std::array<uint8, 3>{Fei, Citan, Bart});

	// Whatever a scene writes of her next, the hold puts back; it takes nothing from anyone else.
	State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() & ~EllyBit));
	State.SetCharacterGear(Elly, Vierge);
	const uint16 Others = static_cast<uint16>(State.JoinedCharacters());
	KeepElly::Hold(State);
	CHECK(State.JoinedCharacters() == (Others | EllyBit));
	CHECK(State.CharacterGear(Elly) == Regurus);

	// Later in the story too: the hideout at 340 and on.
	FGameState Later = AfterGolgoda(340);
	KeepElly::Hold(Later);
	CHECK((Later.JoinedCharacters() & EllyBit) != 0);
}

TEST_CASE("Keep Elly leaves Elly away before Merkava, and with Fei while the story takes the party from him", "[Game]")
{
	// Golgoda and the Ministry's narration, before Merkava's core: as the story has it.
	const FGameState Golgoda = AfterGolgoda(KeepElly::ReturnStory - 1);
	FGameState State = Golgoda;
	CHECK_FALSE(KeepElly::IsBack(State));
	KeepElly::Hold(State);
	CHECK(Untouched(State, Golgoda));

	// Before Bethlehem the scripts take Fei and Elly out together: she stays out with him.
	FGameState Bethlehem = AfterGolgoda(320);
	Bethlehem.SetJoinedCharacters(static_cast<uint16>(Bethlehem.JoinedCharacters() & ~FeiBit));
	Bethlehem.SetParty({Citan, Bart, Billy});
	const FGameState Chosen = Bethlehem;
	CHECK(KeepElly::IsBack(Bethlehem));
	KeepElly::Hold(Bethlehem);
	CHECK(Untouched(Bethlehem, Chosen));

	// Once he is back, so is she.
	Bethlehem.SetJoinedCharacters(static_cast<uint16>(Bethlehem.JoinedCharacters() | FeiBit));
	KeepElly::Hold(Bethlehem);
	CHECK((Bethlehem.JoinedCharacters() & EllyBit) != 0);
}

TEST_CASE("Only a Keep Elly game keeps Elly: a New Game+ without it, the clear file's game and a plain game are as the scripts leave them", "[Game]")
{
	for (const EClearGame Kind : {EClearGame::None, EClearGame::NewGamePlus, EClearGame::ClearFile})
	{
		INFO(static_cast<int32>(Kind));
		FGameState State = AfterGolgoda(KeepElly::ReturnStory);
		State.SetClearGame(Kind);
		const FGameState Before = State;
		CHECK_FALSE(KeepElly::IsBack(State));
		KeepElly::Hold(State);
		CHECK(Untouched(State, Before));
	}
}
