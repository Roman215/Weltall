#include "Battle/BattleContext.h"
#include "Battle/BattleScene.h"
#include "Game/GameState.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** A game with every look switched: Fei as Id, Elly's hair, the second Citan without his sword, the grown Emeralda as the child. */
	FGameState EveryLookSwitched()
	{
		FCharacterLooks Looks;
		for (const uint8 Character : {FCharacterLooks::Fei, FCharacterLooks::Elly, FCharacterLooks::SwordCitan, FCharacterLooks::GrownEmeralda})
		{
			REQUIRE(Looks.Switch(Character));
		}
		FGameState Game;
		Game.SetCharacterLooks(Looks);
		return Game;
	}
}

TEST_CASE("A battle draws the party as its player has it, every look kept", "[Battle]")
{
	FGameState Game = EveryLookSwitched();
	FBattleContext Context;
	Context.GameState = &Game;
	CHECK(Context.PartyLooks() == Game.CharacterLooks());
	Context.GameState = nullptr;
	CHECK_FALSE(Context.PartyLooks().IsSet());
}

TEST_CASE("A battle an event stages, or a special party's, keeps Fei as Id and Elly's hair, the other records' looks the story's", "[Battle]")
{
	FGameState Game = EveryLookSwitched();
	for (const uint8 Flag : {FormationFlags::EventBattle, FormationFlags::SpecialParty})
	{
		INFO(static_cast<int32>(Flag));
		FBattleContext Context;
		Context.GameState = &Game;
		Context.Formation.Flags = Flag;
		const FCharacterLooks Looks = Context.PartyLooks();
		CHECK(Looks.IsId(FCharacterLooks::Fei));
		CHECK(Looks.WearsIdsGear(FCharacterLooks::Xenogears));
		CHECK(Looks.HasPurpleHair(FCharacterLooks::Elly));
		// Their scripts play the sprites' animations by number, which the first Citan's and the child's records number otherwise.
		CHECK(Looks.Drawn(FCharacterLooks::SwordCitan) == FCharacterLooks::SwordCitan);
		CHECK(Looks.Drawn(FCharacterLooks::GrownEmeralda) == FCharacterLooks::GrownEmeralda);
	}
}
