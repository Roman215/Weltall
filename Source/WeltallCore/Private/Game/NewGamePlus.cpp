#include "Game/NewGamePlus.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Game/ItemUse.h"

#include <algorithm>
#include <array>
#include <set>

namespace Weltall
{
	namespace
	{
		using namespace GameStateLayout;

		/** The inventory's lists, numbered as the scripts number them. */
		constexpr int32 ItemList = 0;
		constexpr int32 WeaponList = 1;
		constexpr int32 AccessoryList = 2;
		constexpr int32 GearWeaponList = 3;
		constexpr int32 GearPartList = 4;
		constexpr uint8 MostHeld = 99;
		/** An item number no slot holds: 0 never filled, FFh emptied. */
		constexpr uint8 NoItem = 0;
		constexpr uint8 EmptiedItem = 0xFF;
		/** The item table's use flag of the story's key items. */
		constexpr uint8 KeyItem = 0x10;

		/** What a character has on: its weapon and three accessories; a gear's weapon and three parts. */
		constexpr std::size_t CharacterWeapon = Character::Weapon;
		constexpr std::size_t CharacterAccessories = 0x74;
		constexpr std::size_t GearWeapon = 0x0C;
		constexpr std::size_t GearParts = 0x09;
		constexpr std::size_t SlotsOfAccessories = 3;

		/** The record's figures the levels carry: experience to HP and EP, the base stats, the two levels. */
		constexpr std::size_t LevelFiguresFrom = Character::Exp;
		constexpr std::size_t LevelFiguresTo = Character::MaxEp + 2;
		constexpr std::size_t StatsFrom = 0x58;
		constexpr std::size_t StatsTo = 0x60;
		constexpr std::size_t DeathblowCountersSize = 7 * 2;

		/** The learned skills' fourth word: the gear Deathblows, and below them the Infinity attacks of levels 50, 60 and 70. */
		constexpr uint16 GearDeathblowBits = 0x1FF0;
		constexpr uint16 InfinityAttackBits = 0x000E;
		constexpr std::size_t Maria = 8;

		/** The story flags: the 7-AP tier and Infinity; the second Citan and the grown Emeralda made from the first. */
		constexpr uint16 Limiter = 0x4000;
		constexpr uint16 SecondCitanMade = 0x2000;
		constexpr uint16 GrownEmeraldaMade = 0x1000;
		constexpr std::size_t FirstCitan = 2;
		constexpr std::size_t ChildEmeralda = 6;
		constexpr uint8 HighestAp = 7;
		constexpr uint8 HighestApWithoutLimiter = 6;
		constexpr uint8 NoGear = 0xFF;

		/** Count more of Item into a list, as the scripts give one: its slot, else the first free one; nothing when full. */
		void Add(FGameState& State, int32 List, uint8 Item, uint32 Count)
		{
			const std::span<uint8> Counts = State.InventoryCounts(List);
			const std::span<uint8> Items = State.InventoryItems(List);
			for (std::size_t Slot = 0; Slot < Counts.size(); ++Slot)
			{
				if (Items[Slot] == Item && Counts[Slot] != 0)
				{
					Counts[Slot] = static_cast<uint8>(std::min<uint32>(Counts[Slot] + Count, MostHeld));
					return;
				}
			}
			for (std::size_t Slot = 0; Slot < Counts.size(); ++Slot)
			{
				if (Counts[Slot] == 0 || Items[Slot] == NoItem)
				{
					Items[Slot] = Item;
					Counts[Slot] = static_cast<uint8>(std::min<uint32>(Count, MostHeld));
					return;
				}
			}
		}

		bool IsHeld(uint8 Item)
		{
			return Item != NoItem && Item != EmptiedItem;
		}

		/** Every item of one of the clear file's lists added to the new game's, those Keeps takes. */
		template <typename FKeeps>
		void AddList(std::span<const uint8> From, FGameState& NewGame, int32 List, FKeeps Keeps)
		{
			const FInventoryList& Layout = InventoryLists[static_cast<std::size_t>(List)];
			for (std::size_t Slot = 0; Slot < Layout.Slots; ++Slot)
			{
				const uint8 Count = From[Layout.Counts + Slot];
				const uint8 Item = From[Layout.Items + Slot];
				if (Count != 0 && IsHeld(Item) && Keeps(Item))
				{
					Add(NewGame, List, Item, Count);
				}
			}
		}

		bool IsKeyItem(uint8 Item, std::span<const uint8> ItemTable)
		{
			const std::size_t At = static_cast<std::size_t>(Item) * ItemRecord::Size + ItemRecord::Use;
			return At < ItemTable.size() && (ItemTable[At] & KeyItem) != 0;
		}

		/**
		 * The characters whose records hold what they have on: all but the first Citan and the child Emeralda once the
		 * story has made the second Citan and the grown Emeralda from them (the copies took their equipment with them).
		 */
		bool WearsOwn(std::size_t Id, uint16 StoryFlags)
		{
			return !(Id == FirstCitan && (StoryFlags & SecondCitanMade) != 0) && !(Id == ChildEmeralda && (StoryFlags & GrownEmeraldaMade) != 0);
		}

		void CarryEquipment(const FGameState& ClearFile, FGameState& NewGame)
		{
			const std::span<const uint8> From = ClearFile.Block();
			const auto All = [](uint8) { return true; };
			for (const int32 List : {WeaponList, AccessoryList, GearWeaponList, GearPartList})
			{
				AddList(From, NewGame, List, All);
			}
			// What the characters have on, and their gears: each gear once, its twins (Weltall's later records,
			// Regurus beside Vierge...) holding copies of the same parts.
			const uint16 StoryFlags = ClearFile.StoryFlags();
			std::set<uint8> Gears;
			for (std::size_t Id = 0; Id < static_cast<std::size_t>(CharacterCount); ++Id)
			{
				if (!WearsOwn(Id, StoryFlags))
				{
					continue;
				}
				const std::size_t Record = CharacterOffset(Id);
				if (IsHeld(From[Record + CharacterWeapon]))
				{
					Add(NewGame, WeaponList, From[Record + CharacterWeapon], 1);
				}
				for (std::size_t Slot = 0; Slot < SlotsOfAccessories; ++Slot)
				{
					if (IsHeld(From[Record + CharacterAccessories + Slot]))
					{
						Add(NewGame, AccessoryList, From[Record + CharacterAccessories + Slot], 1);
					}
				}
				const uint8 Gear = From[Record + Character::Gear];
				if (Gear != NoGear && Gear < GearCount)
				{
					Gears.insert(Gear);
				}
			}
			for (const uint8 Gear : Gears)
			{
				const std::size_t Record = GearOffset(Gear);
				if (IsHeld(From[Record + GearWeapon]))
				{
					Add(NewGame, GearWeaponList, From[Record + GearWeapon], 1);
				}
				for (std::size_t Slot = 0; Slot < SlotsOfAccessories; ++Slot)
				{
					if (IsHeld(From[Record + GearParts + Slot]))
					{
						Add(NewGame, GearPartList, From[Record + GearParts + Slot], 1);
					}
				}
			}
		}

		void CopyBytes(std::span<const uint8> From, std::span<uint8> To, std::size_t Offset, std::size_t Size)
		{
			std::copy_n(From.begin() + static_cast<std::ptrdiff_t>(Offset), Size, To.begin() + static_cast<std::ptrdiff_t>(Offset));
		}

		/** Bits Mask of a learned-skills word taken from the clear file's. */
		void CopyBits(std::span<const uint8> From, std::span<uint8> To, std::size_t Offset, uint16 Mask)
		{
			const uint16 Kept = static_cast<uint16>(LittleEndian::ReadU16(To, Offset) & ~Mask);
			LittleEndian::WriteU16(To, Offset, static_cast<uint16>(Kept | (LittleEndian::ReadU16(From, Offset) & Mask)));
		}

		void CarryDeathblows(const FGameState& ClearFile, FGameState& NewGame)
		{
			const std::span<const uint8> From = ClearFile.Block();
			const std::span<uint8> To = NewGame.Block();
			for (std::size_t Id = 0; Id < static_cast<std::size_t>(CharacterCount); ++Id)
			{
				const std::size_t Learned = LearnedSkillsOffsetOf(Id);
				CopyBits(From, To, Learned + Skills::Deathblows, 0xFFFF);
				if (Id != Maria)
				{
					CopyBits(From, To, Learned + Skills::GearDeathblows, GearDeathblowBits);
				}
				CopyBytes(From, To, CharacterOffset(Id) + Character::DeathblowCounters, DeathblowCountersSize);
			}
		}

		void CarryLevels(const FGameState& ClearFile, FGameState& NewGame, bool bLimiter)
		{
			const std::span<const uint8> From = ClearFile.Block();
			const std::span<uint8> To = NewGame.Block();
			for (std::size_t Id = 0; Id < static_cast<std::size_t>(CharacterCount); ++Id)
			{
				const std::size_t Record = CharacterOffset(Id);
				CopyBytes(From, To, Record + LevelFiguresFrom, LevelFiguresTo - LevelFiguresFrom);
				CopyBytes(From, To, Record + StatsFrom, StatsTo - StatsFrom);
				CopyBytes(From, To, Record + Character::Level, 2);
				LittleEndian::WriteU16(To, Record + Character::Hp, LittleEndian::ReadU16(To, Record + Character::MaxHp));
				LittleEndian::WriteU16(To, Record + Character::Ep, LittleEndian::ReadU16(To, Record + Character::MaxEp));

				const std::size_t Learned = LearnedSkillsOffsetOf(Id);
				CopyBits(From, To, Learned + Skills::Abilities, 0xFFFF);
				CopyBits(From, To, Learned + Skills::GearAbilities, 0xFFFF);
				CopyBits(From, To, Learned + Skills::GearDeathblows, Id == Maria ? static_cast<uint16>(InfinityAttackBits | GearDeathblowBits) : InfinityAttackBits);
				const uint8 Ap = From[Learned + Skills::ApPerTurn];
				To[Learned + Skills::ApPerTurn] = std::min(Ap, bLimiter ? HighestAp : HighestApWithoutLimiter);
			}
		}
	}

	void NewGamePlus::Carry(const FGameState& ClearFile, FGameState& NewGame, const FNewGamePlusCarry& Carry, std::span<const uint8> ItemTable)
	{
		if (Carry.bMoney)
		{
			NewGame.SetGold(ClearFile.Gold());
		}
		if (Carry.bEquipment)
		{
			CarryEquipment(ClearFile, NewGame);
		}
		if (Carry.bItems)
		{
			AddList(ClearFile.Block(), NewGame, ItemList, [ItemTable](uint8 Item) { return !IsKeyItem(Item, ItemTable); });
		}
		if (Carry.bDeathblows)
		{
			CarryDeathblows(ClearFile, NewGame);
		}
		if (Carry.bLevels)
		{
			CarryLevels(ClearFile, NewGame, Carry.bLimiter);
		}
		if (Carry.bLimiter)
		{
			NewGame.SetStoryFlags(static_cast<uint16>(NewGame.StoryFlags() | (ClearFile.StoryFlags() & Limiter)));
		}
		NewGame.SetClearGame(Carry.bKeepElly ? EClearGame::KeepElly : EClearGame::NewGamePlus);
	}
}
