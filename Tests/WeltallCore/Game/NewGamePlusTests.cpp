#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/NewGamePlus.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

// The New Game+: what each part the player chooses takes from the clear file into the new game.
namespace
{
	/** The game state's characters' and gears' records and learned skills, and the fields the tests look at. */
	constexpr std::size_t CharacterOffset(std::size_t Id) { return 0x26C + Id * 0xA4; }
	constexpr std::size_t GearOffset(std::size_t Gear) { return 0x978 + Gear * 0xA4; }
	constexpr std::size_t LearnedSkillsOffsetOf(std::size_t Id) { return 0x16C0 + Id * 0x20; }
	constexpr int32 CharacterCount = 11;
	namespace Character
	{
		constexpr std::size_t Exp = 0x3C;
		constexpr std::size_t Hp = 0x4C;
		constexpr std::size_t MaxHp = 0x4E;
		constexpr std::size_t Ep = 0x50;
		constexpr std::size_t MaxEp = 0x52;
		constexpr std::size_t Level = 0x62;
		constexpr std::size_t EtherLevel = 0x63;
		constexpr std::size_t Weapon = 0x6A;
		constexpr std::size_t DeathblowCounters = 0x90;
		constexpr std::size_t Gear = 0xA0;
	}
	namespace Skills
	{
		constexpr std::size_t Deathblows = 0x00;
		constexpr std::size_t Abilities = 0x02;
		constexpr std::size_t GearDeathblows = 0x04;
		constexpr std::size_t GearAbilities = 0x06;
		constexpr std::size_t ApPerTurn = 0x17;
	}

	constexpr int32 ItemList = 0;
	constexpr int32 WeaponList = 1;
	constexpr int32 AccessoryList = 2;
	constexpr int32 GearWeaponList = 3;
	constexpr int32 GearPartList = 4;
	constexpr std::size_t Fei = 0;
	constexpr std::size_t FirstCitan = 2;
	constexpr std::size_t Maria = 8;
	constexpr uint8 Xenogears = 15;
	constexpr uint8 Weltall2 = 1;
	/** Items 1 and 2: Aquasols; 98: the map of Deus, a key item. */
	constexpr uint8 Aquasol = 1;
	constexpr uint8 AquasolS = 2;
	constexpr uint8 MerkavaMap = 98;

	std::vector<uint8> ItemTable()
	{
		std::vector<uint8> Table(150 * 0x10, 0);
		Table[MerkavaMap * 0x10 + 6] = 0x10;
		Table[Aquasol * 0x10 + 6] = 0xC0;
		return Table;
	}

	/** The count of Item in a list, over its slots. */
	uint32 Held(FGameState& State, int32 List, uint8 Item)
	{
		uint32 Count = 0;
		for (std::size_t Slot = 0; Slot < State.InventoryItems(List).size(); ++Slot)
		{
			if (State.InventoryItems(List)[Slot] == Item)
			{
				Count += State.InventoryCounts(List)[Slot];
			}
		}
		return Count;
	}

	void Hold(FGameState& State, int32 List, std::size_t Slot, uint8 Item, uint8 Count)
	{
		State.InventoryItems(List)[Slot] = Item;
		State.InventoryCounts(List)[Slot] = Count;
	}

	uint16 Word(const FGameState& State, std::size_t Offset)
	{
		return LittleEndian::ReadU16(State.Block(), Offset);
	}

	void SetWord(FGameState& State, std::size_t Offset, uint16 Value)
	{
		LittleEndian::WriteU16(State.Block(), Offset, Value);
	}

	std::size_t Learned(std::size_t Id, std::size_t Field)
	{
		return LearnedSkillsOffsetOf(Id) + Field;
	}
}

TEST_CASE("A New Game+ that carries nothing is the new game, marked a New Game+", "[Game]")
{
	FGameState Clear;
	Clear.SetGold(5000000);
	Hold(Clear, ItemList, 0, AquasolS, 40);
	Clear.SetStoryFlags(0xC000);
	FGameState NewGame;
	NewGame.SetGold(100);
	NewGame.SetStoryFlags(0x8000);
	const FGameState Before = NewGame;
	NewGamePlus::Carry(Clear, NewGame, {}, ItemTable());
	CHECK(std::equal(NewGame.Block().begin(), NewGame.Block().end(), Before.Block().begin()));
	CHECK(NewGame.ClearGame() == EClearGame::NewGamePlus);
	CHECK(NewGame.IsClearGame());
}

TEST_CASE("A New Game+ with Keep Elly chosen is marked a Keep Elly game, and carries nothing more for it", "[Game]")
{
	FGameState Clear;
	Clear.SetGold(5000000);
	FGameState NewGame;
	NewGame.SetGold(100);
	const FGameState Before = NewGame;
	FNewGamePlusCarry Carry;
	Carry.bKeepElly = true;
	NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
	CHECK(std::equal(NewGame.Block().begin(), NewGame.Block().end(), Before.Block().begin()));
	CHECK(NewGame.ClearGame() == EClearGame::KeepElly);
	CHECK(NewGame.IsClearGame());
}

TEST_CASE("A New Game+ carries the clear file's money, and its items but the story's key items, added to the new game's", "[Game]")
{
	FGameState Clear;
	Clear.SetGold(5000000);
	Hold(Clear, ItemList, 0, Aquasol, 90);
	Hold(Clear, ItemList, 1, MerkavaMap, 1);
	Hold(Clear, ItemList, 2, AquasolS, 7);
	// A slot an item was taken from, emptied (FFh with no count), holds nothing.
	Hold(Clear, ItemList, 3, 0xFF, 0);
	FGameState NewGame;
	NewGame.SetGold(100);
	Hold(NewGame, ItemList, 0, Aquasol, 20);
	FNewGamePlusCarry Carry;
	Carry.bMoney = true;
	Carry.bItems = true;
	NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
	CHECK(NewGame.Gold() == 5000000);
	// Stacked on the new game's own, up to 99.
	CHECK(Held(NewGame, ItemList, Aquasol) == 99);
	CHECK(Held(NewGame, ItemList, AquasolS) == 7);
	CHECK(Held(NewGame, ItemList, MerkavaMap) == 0);
	CHECK(Held(NewGame, ItemList, 0xFF) == 0);
	// Equipment stays behind.
	CHECK(Held(NewGame, WeaponList, 0) == 0);
}

TEST_CASE("A New Game+ carries every piece of equipment held or worn into the inventory, each gear's once", "[Game]")
{
	FGameState Clear;
	Hold(Clear, WeaponList, 0, 10, 2);
	Hold(Clear, AccessoryList, 0, 30, 1);
	Hold(Clear, GearWeaponList, 0, 5, 1);
	Hold(Clear, GearPartList, 0, 40, 3);
	// Fei wears weapon 12 and accessories 31 and 32 and pilots Xenogears with parts 41 and 42 and gear weapon 6;
	// Weltall-2, which no one pilots any more, holds copies of parts it had.
	const std::size_t FeiRecord = CharacterOffset(Fei);
	Clear.Block()[FeiRecord + Character::Weapon] = 12;
	Clear.Block()[FeiRecord + 0x74] = 31;
	Clear.Block()[FeiRecord + 0x75] = 32;
	Clear.Block()[FeiRecord + Character::Gear] = Xenogears;
	Clear.Block()[GearOffset(Xenogears) + 0x0C] = 6;
	Clear.Block()[GearOffset(Xenogears) + 0x09] = 41;
	Clear.Block()[GearOffset(Xenogears) + 0x0B] = 42;
	Clear.Block()[GearOffset(Weltall2) + 0x09] = 41;
	// The first Citan, whose record the second Citan was made from, still holds the sword he wore then.
	for (std::size_t Id = 1; Id < static_cast<std::size_t>(CharacterCount); ++Id)
	{
		Clear.Block()[CharacterOffset(Id) + Character::Gear] = 0xFF;
	}
	Clear.Block()[CharacterOffset(FirstCitan) + Character::Weapon] = 20;
	Clear.Block()[CharacterOffset(9) + Character::Weapon] = 20;
	Clear.SetStoryFlags(0x2000);
	FGameState NewGame;
	Hold(NewGame, WeaponList, 0, 10, 1);
	NewGame.Block()[FeiRecord + Character::Weapon] = 1;
	FNewGamePlusCarry Carry;
	Carry.bEquipment = true;
	NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
	CHECK(Held(NewGame, WeaponList, 10) == 3);
	CHECK(Held(NewGame, WeaponList, 12) == 1);
	CHECK(Held(NewGame, WeaponList, 20) == 1);
	CHECK(Held(NewGame, AccessoryList, 30) == 1);
	CHECK(Held(NewGame, AccessoryList, 31) == 1);
	CHECK(Held(NewGame, AccessoryList, 32) == 1);
	CHECK(Held(NewGame, GearWeaponList, 5) == 1);
	CHECK(Held(NewGame, GearWeaponList, 6) == 1);
	CHECK(Held(NewGame, GearPartList, 40) == 3);
	CHECK(Held(NewGame, GearPartList, 41) == 1);
	CHECK(Held(NewGame, GearPartList, 42) == 1);
	// The new game's Fei keeps what he has on.
	CHECK(NewGame.Block()[FeiRecord + Character::Weapon] == 1);
}

TEST_CASE("A New Game+ carries each character's Deathblows: learned, the gear's, and the counters toward the rest", "[Game]")
{
	FGameState Clear;
	SetWord(Clear, Learned(Fei, Skills::Deathblows), 0xFFF8);
	SetWord(Clear, Learned(Fei, Skills::GearDeathblows), 0x1FFE);
	SetWord(Clear, Learned(Maria, Skills::GearDeathblows), 0x1FF0);
	SetWord(Clear, CharacterOffset(Fei) + Character::DeathblowCounters + 12, 4321);
	SetWord(Clear, Learned(Fei, Skills::Abilities), 0x8000);
	Clear.Block()[CharacterOffset(Fei) + Character::Level] = 80;
	FGameState NewGame;
	SetWord(NewGame, Learned(Fei, Skills::GearDeathblows), 0x0001);
	FNewGamePlusCarry Carry;
	Carry.bDeathblows = true;
	NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
	CHECK(Word(NewGame, Learned(Fei, Skills::Deathblows)) == 0xFFF8);
	// The gear Deathblows, not the Infinity attacks the levels give.
	CHECK(Word(NewGame, Learned(Fei, Skills::GearDeathblows)) == 0x1FF1);
	CHECK(Word(NewGame, CharacterOffset(Fei) + Character::DeathblowCounters + 12) == 4321);
	// Maria's gear Deathblows come by her level.
	CHECK(Word(NewGame, Learned(Maria, Skills::GearDeathblows)) == 0);
	CHECK(Word(NewGame, Learned(Fei, Skills::Abilities)) == 0);
	CHECK(NewGame.Block()[CharacterOffset(Fei) + Character::Level] == 0);
}

TEST_CASE("A New Game+ carries each character's levels, stats and abilities, full, with the AP per turn the level gives", "[Game]")
{
	FGameState Clear;
	const std::size_t Record = CharacterOffset(Fei);
	Clear.Block()[Record + Character::Level] = 72;
	Clear.Block()[Record + Character::EtherLevel] = 60;
	LittleEndian::WriteU32(Clear.Block(), Record + Character::Exp, 1234567);
	SetWord(Clear, Record + Character::Hp, 12);
	SetWord(Clear, Record + Character::MaxHp, 999);
	SetWord(Clear, Record + Character::Ep, 3);
	SetWord(Clear, Record + Character::MaxEp, 99);
	Clear.Block()[Record + 0x58] = 180;
	Clear.Block()[Record + 0x5F] = 40;
	SetWord(Clear, Learned(Fei, Skills::Abilities), 0xFF00);
	SetWord(Clear, Learned(Fei, Skills::GearAbilities), 0xF000);
	SetWord(Clear, Learned(Fei, Skills::GearDeathblows), 0x1FFE);
	SetWord(Clear, Learned(Maria, Skills::GearDeathblows), 0x1F00);
	Clear.Block()[Learned(Fei, Skills::ApPerTurn)] = 7;
	Clear.Block()[Learned(FirstCitan, Skills::ApPerTurn)] = 5;
	SetWord(Clear, Learned(Fei, Skills::Deathblows), 0xFFF8);
	Clear.SetStoryFlags(0xC000);

	for (const bool bLimiter : {false, true})
	{
		INFO("the 7-AP flag carried: " << bLimiter);
		FGameState NewGame;
		NewGame.Block()[Learned(Fei, Skills::ApPerTurn)] = 3;
		NewGame.SetStoryFlags(0x8000);
		FNewGamePlusCarry Carry;
		Carry.bLevels = true;
		Carry.bLimiter = bLimiter;
		NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
		CHECK(NewGame.Block()[Record + Character::Level] == 72);
		CHECK(NewGame.Block()[Record + Character::EtherLevel] == 60);
		CHECK(LittleEndian::ReadU32(NewGame.Block(), Record + Character::Exp) == 1234567);
		CHECK(Word(NewGame, Record + Character::Hp) == 999);
		CHECK(Word(NewGame, Record + Character::Ep) == 99);
		CHECK(NewGame.Block()[Record + 0x58] == 180);
		CHECK(NewGame.Block()[Record + 0x5F] == 40);
		CHECK(Word(NewGame, Learned(Fei, Skills::Abilities)) == 0xFF00);
		CHECK(Word(NewGame, Learned(Fei, Skills::GearAbilities)) == 0xF000);
		// The Infinity attacks of levels 50, 60 and 70, not the gear Deathblows; Maria's gear Deathblows by her level.
		CHECK(Word(NewGame, Learned(Fei, Skills::GearDeathblows)) == 0x000E);
		CHECK(Word(NewGame, Learned(Maria, Skills::GearDeathblows)) == 0x1F00);
		CHECK(NewGame.Block()[Learned(Fei, Skills::ApPerTurn)] == (bLimiter ? 7 : 6));
		CHECK(NewGame.Block()[Learned(FirstCitan, Skills::ApPerTurn)] == 5);
		CHECK(Word(NewGame, Learned(Fei, Skills::Deathblows)) == 0);
		CHECK(NewGame.StoryFlags() == (bLimiter ? 0xC000 : 0x8000));
	}
}

TEST_CASE("A New Game+ carries the 7-AP flag alone, not the story's flags of the second Citan and the grown Emeralda", "[Game]")
{
	FGameState Clear;
	Clear.SetStoryFlags(0xF000);
	FGameState NewGame;
	NewGame.SetStoryFlags(0x8000);
	NewGame.Block()[Learned(Fei, Skills::ApPerTurn)] = 3;
	FNewGamePlusCarry Carry;
	Carry.bLimiter = true;
	NewGamePlus::Carry(Clear, NewGame, Carry, ItemTable());
	CHECK(NewGame.StoryFlags() == 0xC000);
	// The AP per turn grows from the new game's levels.
	CHECK(NewGame.Block()[Learned(Fei, Skills::ApPerTurn)] == 3);
}
