#include "EventOps.h"

#include "Game/GameState.h"

namespace Weltall::EventOps
{
	namespace
	{
		constexpr uint8 MostOfAnItem = 99;
		constexpr int32 MostGold = 9999999;

		/** An item code: the inventory list in its high byte, the item's number in the list in its low byte. */
		struct FItemCode
		{
			int32 List;
			uint8 Item;
		};

		std::optional<FItemCode> ReadItemCode(FEventContext& Context, int32 Offset)
		{
			const int32 Code = ReadValue(Context, Offset);
			const int32 List = Code >> 8;
			if (List < 0 || List >= FGameState::InventoryListCount)
			{
				// The original has no list there and reads and writes memory address 0.
				Fail(Context, EEventFault::TableIndexOutOfRange, static_cast<uint32>(List));
				return std::nullopt;
			}
			return FItemCode{List, static_cast<uint8>(Code)};
		}

		/** The first slot holding some of the item, or -1. */
		int32 FindItem(FGameState& State, const FItemCode& Code)
		{
			const std::span<uint8> Counts = State.InventoryCounts(Code.List);
			const std::span<uint8> Items = State.InventoryItems(Code.List);
			for (std::size_t Slot = 0; Slot < Counts.size(); ++Slot)
			{
				if (Items[Slot] == Code.Item && Counts[Slot] != 0)
				{
					return static_cast<int32>(Slot);
				}
			}
			return -1;
		}

		/** The first slot that holds nothing (or item 0), or -1 when the list is full. */
		int32 FindFreeSlot(FGameState& State, int32 List)
		{
			const std::span<uint8> Counts = State.InventoryCounts(List);
			const std::span<uint8> Items = State.InventoryItems(List);
			for (std::size_t Slot = 0; Slot < Counts.size(); ++Slot)
			{
				if (Counts[Slot] == 0 || Items[Slot] == 0)
				{
					return static_cast<int32>(Slot);
				}
			}
			return -1;
		}

		void GetItemCount(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			const std::optional<FItemCode> Code = ReadItemCode(Context, 1);
			if (State == nullptr || !Code)
			{
				return;
			}
			const int32 Slot = FindItem(*State, *Code);
			SetVar(Context, ReadU16(Context, 3), Slot == -1 ? 0 : State->InventoryCounts(Code->List)[static_cast<std::size_t>(Slot)]);
			Advance(Context, 5);
		}

		void IfHasItem(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			const std::optional<FItemCode> Code = ReadItemCode(Context, 1);
			if (State == nullptr || !Code)
			{
				return;
			}
			Branch(Context, FindItem(*State, *Code) != -1, 5, 3);
		}

		/** 8C: one more of the item, up to 99 in a slot; lost when its list is full. */
		void GiveItem(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			const std::optional<FItemCode> Code = ReadItemCode(Context, 1);
			if (State == nullptr || !Code)
			{
				return;
			}
			const std::span<uint8> Counts = State->InventoryCounts(Code->List);
			int32 Slot = FindItem(*State, *Code);
			if (Slot == -1)
			{
				Slot = FindFreeSlot(*State, Code->List);
				if (Slot != -1)
				{
					State->InventoryItems(Code->List)[static_cast<std::size_t>(Slot)] = Code->Item;
					Counts[static_cast<std::size_t>(Slot)] = 1;
				}
			}
			else if (Counts[static_cast<std::size_t>(Slot)] < MostOfAnItem)
			{
				++Counts[static_cast<std::size_t>(Slot)];
			}
			Advance(Context, 3);
		}

		/** 8D: one fewer of the item; an emptied slot's item becomes FF. */
		void TakeItem(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			const std::optional<FItemCode> Code = ReadItemCode(Context, 1);
			if (State == nullptr || !Code)
			{
				return;
			}
			State->TakeItem(Code->List, Code->Item);
			Advance(Context, 3);
		}

		/** 8E: goes on when the party has at least the operand's 32-bit amount of gold, else jumps. */
		void IfGold(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			const uint32 Amount = static_cast<uint32>(ReadU16(Context, 1)) | static_cast<uint32>(ReadU16(Context, 3)) << 16;
			Branch(Context, State->Gold() >= Amount, 7, 5);
		}

		/** 8F: adds gold up to 9,999,999; a negative variable takes gold away with no floor. */
		void AddGold(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			int32 Gold = static_cast<int32>(State->Gold() + static_cast<uint32>(ReadValue(Context, 1)));
			if (Gold > MostGold)
			{
				Gold = MostGold;
			}
			State->SetGold(static_cast<uint32>(Gold));
			Advance(Context, 3);
		}

		/** 90: takes gold down to 0; a negative variable adds gold with no ceiling. */
		void SubtractGold(FEventContext& Context)
		{
			FGameState* State = GameStateOf(Context);
			if (State == nullptr)
			{
				return;
			}
			int32 Gold = static_cast<int32>(State->Gold() - static_cast<uint32>(ReadValue(Context, 1)));
			if (Gold < 0)
			{
				Gold = 0;
			}
			State->SetGold(static_cast<uint32>(Gold));
			Advance(Context, 3);
		}
	}

	void RegisterItems(FBaseTable& Base, FExtendedTable&)
	{
		Base[0x34] = &GetItemCount;
		Base[0x8B] = &IfHasItem;
		Base[0x8C] = &GiveItem;
		Base[0x8D] = &TakeItem;
		Base[0x8E] = &IfGold;
		Base[0x8F] = &AddGold;
		Base[0x90] = &SubtractGold;
	}
}
