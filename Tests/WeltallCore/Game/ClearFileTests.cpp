#include "Game/ClearFile.h"

#include "Game/GameState.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>

using namespace Weltall;

namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint16 EllyBit = 1 << Elly;
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	/** Saved variables by index (the scripts' byte address halved). */
	constexpr int32 Story = 0;
	constexpr int32 Entrance = 1;
	constexpr int32 MapVariable = 0x03C / 2;
	constexpr int32 PartyVariable = 0x03E / 2;
	constexpr int32 Scene = 0x054 / 2;
	constexpr int32 Orbs = 0x0C8 / 2;
	constexpr int32 OrbsFallen = 0x0CC / 2;
	constexpr int32 Deus = 0x0E6 / 2;
	constexpr int32 Gates = 0x0E8 / 2;
	constexpr int32 Treasure = 0x2CA / 2;
	/** The ship's flags: bit 14 while the party is aboard and the ship landed, which the bridge's launch asks for. */
	constexpr int32 Ship = 0x1C8 / 2;
	constexpr uint16 Aboard = 1 << 14;
	/** The first of the characters free at this point, in the game's order: Bart. */
	constexpr uint8 Bart = 3;
	constexpr uint8 Billy = 4;
	/** The items' list, and the map 703 gives (item 98). */
	constexpr int32 Items = 0;
	constexpr uint8 DeusMap = 98;

	/**
	 * A game as the ending leaves it: the story at 350 on map 41, Fei and Elly in the party riding as the last scenes
	 * left them, everyone unlocked, Elly out of the menus and in Vierge, Deus' flags and the orbs' globals set, the map of
	 * Deus held twice (once from elsewhere), a treasure taken.
	 */
	FGameState AtTheEnding()
	{
		FGameState State;
		State.SetSavedEventVariable(Story, 350);
		State.SetPlace(41);
		State.SetParty({Fei, Elly, FGameState::EmptySlot});
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			State.SetPartySlotInGear(Slot, 1);
		}
		State.SetJoinedCharacters(0x0FB9);
		State.SetMenuCharacters(0xF7FD);
		State.SetLockedCharacters(0);
		State.SetCharacterGear(Elly, Vierge);
		State.SetSavedEventVariable(Orbs, 0x0078);
		State.SetSavedEventVariable(OrbsFallen, 4);
		State.SetSavedEventVariable(Deus, 0x60FD);
		State.SetSavedEventVariable(Gates, 0xFBFF);
		State.SetSavedEventVariable(Scene, 2);
		State.SetSavedEventVariable(Treasure, 0x0203);
		State.SetSavedEventVariable(Ship, 0x8015);
		State.InventoryItems(Items)[4] = DeusMap;
		State.InventoryCounts(Items)[4] = 2;
		State.BattleGlobals().fill(0x00FF);
		State.BattleGlobals()[4] = 0x1237;
		return State;
	}
}

TEST_CASE("The clear file puts the ending's game back aboard the Yggdrasil before Deus, Fei, Elly and a third in the party", "[Game]")
{
	FGameState State = AtTheEnding();
	ClearFile::Make(State);
	CHECK(State.IsClearGame());

	// The bridge, entered as from the world map's way into the ship, in the last chapter's free roaming.
	CHECK(State.Place() == 290);
	CHECK(State.Scene() == 3);
	CHECK(State.SavedEventVariable(Entrance) == 3);
	CHECK(State.SavedEventVariable(Story) == 340);
	CHECK(State.SavedEventVariable(MapVariable) == 290);
	CHECK(State.SavedEventVariable(Scene) == 4);
	CHECK(State.FieldSong() == 59);
	// Boarded and landed, as a load does not enter the bridge from the world map: the launch is offered.
	CHECK(State.SavedEventVariable(Ship) == (0x8015 | Aboard));

	// What the final dungeon wrote is undone, what was taken stays taken, and one map of Deus goes back.
	CHECK(State.SavedEventVariable(Orbs) == 0);
	CHECK(State.SavedEventVariable(OrbsFallen) == 0);
	CHECK(State.SavedEventVariable(Gates) == 0);
	CHECK(State.SavedEventVariable(Deus) == 0x0001);
	CHECK(State.SavedEventVariable(Treasure) == 0x0203);
	CHECK(State.InventoryCounts(Items)[4] == 1);
	CHECK(State.InventoryItems(Items)[4] == DeusMap);
	for (int32 Global = 0; Global < FGameState::BattleGlobalCount; ++Global)
	{
		const uint16 Expected = Global >= 5 && Global <= 8 ? 0 : Global == 4 ? 0x1237 : 0x00FF;
		CHECK(State.BattleGlobals()[static_cast<std::size_t>(Global)] == Expected);
	}

	// Fei leads alone locked, nobody rides, Elly joined and in the menus again, in Regurus; the third member the first
	// Member Change offers, as the ship's member change refuses a party of two.
	CHECK(State.LockedCharacters() == 1);
	CHECK(State.Party() == std::array<uint8, 3>{Fei, Elly, Bart});
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		CHECK(State.PartySlotInGear(Slot) == 0);
	}
	CHECK(State.SavedEventVariable(PartyVariable) == Fei);
	CHECK(State.SavedEventVariable(PartyVariable + 1) == Elly);
	CHECK(State.SavedEventVariable(PartyVariable + 2) == Bart);
	CHECK(State.JoinedCharacters() == (0x0FB9 | EllyBit));
	CHECK(State.MenuCharacters() == 0xF7FF);
	CHECK(State.CharacterGear(Elly) == Regurus);
}

TEST_CASE("The last map of Deus taken back empties its slot, and none held leaves the items as they were", "[Game]")
{
	FGameState State = AtTheEnding();
	State.InventoryCounts(Items)[4] = 1;
	ClearFile::Make(State);
	CHECK(State.InventoryCounts(Items)[4] == 0);
	CHECK(State.InventoryItems(Items)[4] == 0xFF);

	FGameState Without = AtTheEnding();
	Without.InventoryCounts(Items)[4] = 0;
	const FGameState Before = Without;
	ClearFile::Make(Without);
	const bool bItemsKept = std::equal(Before.Block().begin() + 0x1F90, Before.Block().begin() + 0x20BC, Without.Block().begin() + 0x1F90);
	CHECK(bItemsKept);
}

TEST_CASE("The original sees a clear game with Elly away, the first free member in her place with her gear's riding", "[Game]")
{
	FGameState State = AtTheEnding();
	ClearFile::Make(State);
	State.SetParty({Fei, Elly, Billy});
	State.SetPartySlotInGear(0, 0);
	State.SetPartySlotInGear(1, 1);
	State.SetPartySlotInGear(2, 0);
	ClearFile::HideElly(State);
	CHECK(State.Party() == std::array<uint8, 3>{Fei, Bart, Billy});
	CHECK(State.PartySlotInGear(1) == 1);
	CHECK(State.PartySlotInGear(2) == 0);
	CHECK(State.SavedEventVariable(PartyVariable + 1) == Bart);
	CHECK(State.SavedEventVariable(PartyVariable + 2) == Billy);
	CHECK((State.JoinedCharacters() & EllyBit) == 0);
	CHECK(State.CharacterGear(Elly) == Vierge);
	// Only her return is hidden: the rest of the clear game is the original's.
	CHECK(State.SavedEventVariable(Story) == 340);
	CHECK(State.MenuCharacters() == 0xF7FF);
}

TEST_CASE("With nobody free to take her place, the members after Elly close up with their gears", "[Game]")
{
	FGameState State = AtTheEnding();
	ClearFile::Make(State);
	constexpr uint8 Citan = 2;
	State.SetMenuCharacters(1 << Fei | EllyBit | 1 << Citan);
	State.SetParty({Elly, Citan, Fei});
	State.SetPartySlotInGear(0, 0);
	State.SetPartySlotInGear(1, 1);
	State.SetPartySlotInGear(2, 0);
	ClearFile::HideElly(State);
	CHECK(State.Party() == std::array<uint8, 3>{Citan, Fei, FGameState::EmptySlot});
	CHECK(State.PartySlotInGear(0) == 1);
	CHECK(State.PartySlotInGear(1) == 0);
	CHECK(State.SavedEventVariable(PartyVariable) == Citan);
	CHECK(State.SavedEventVariable(PartyVariable + 2) == FGameState::EmptySlot);
	CHECK((State.JoinedCharacters() & EllyBit) == 0);
}
