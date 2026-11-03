#include "Game/ClearFile.h"

#include "Field/EventContext.h"
#include "Field/EventScripts.h"
#include "Game/GameState.h"

#include <algorithm>
#include <array>

namespace Weltall::ClearFile
{
	namespace
	{
		constexpr uint8 Fei = 0;
		constexpr uint8 Elly = 1;
		constexpr uint16 EllyBit = 1 << Elly;
		constexpr uint8 Vierge = 2;
		constexpr uint8 Regurus = 10;

		// Saved event variables by index: the scripts address variable n as byte 2n.
		/** The map the field last ran, and the party's three characters (FFh for none), which the field writes every frame. */
		constexpr int32 MapVariable = 0x03C / 2;
		constexpr int32 PartyVariable = 0x03E / 2;
		/** Each map's own value for the world map's scene after a load (054h): the bridge's. */
		constexpr int32 SceneVariable = 0x054 / 2;
		constexpr uint16 BridgeScene = 4;
		/** The orbs at Deus' core, and how many have fallen (map 717). */
		constexpr int32 OrbsVariable = 0x0C8 / 2;
		constexpr int32 OrbsFallenVariable = 0x0CC / 2;
		/** The gates inside Deus (map 706). */
		constexpr int32 GatesVariable = 0x0E8 / 2;
		/** Deus' landing (map 702, bit 2) and its machinery (map 706, bits 3 to 7, 13 and 14); a disc 1 map keeps bit 0 here. */
		constexpr int32 DeusVariable = 0x0E6 / 2;
		constexpr uint16 DeusBits = 0x60FC;

		/** The song a save made on the bridge holds as the field's. */
		constexpr uint16 BridgeSong = 59;
		/** The ship's flags (1C8h): bit 14 while the party is aboard and the ship landed, which the bridge's launch asks for. */
		constexpr int32 ShipVariable = 0x1C8 / 2;
		constexpr uint16 AboardBit = 1 << 14;
		/** The characters who can be in the party, in the order the game numbers them. */
		constexpr uint8 CharacterCount = 11;

		/** The map 703 gives below story 350 (named Merkava Map): item 98 of the items' list. */
		constexpr int32 ItemsList = 0;
		constexpr uint8 DeusMap = 98;

		/** The battle scripts' globals the orbs' battles set as each orb falls and Deus' reads. */
		constexpr int32 FirstOrbGlobal = 5;
		constexpr int32 LastOrbGlobal = 8;

		/** The map of the ending, whose script waits for good on its last picture. */
		constexpr int32 EndingMap = 41;
		/** Wait #0 (its operand an immediate 0) and Jump, three bytes each. */
		constexpr std::array<uint8, 3> WaitNone{0x26, 0x00, 0x80};
		constexpr uint8 JumpOpcode = 0x01;
		constexpr std::size_t InstructionBytes = 3;

		/** Whether Code holds, at At, a wait that the next instruction jumps back to. */
		bool IsParkedAt(const FEventScripts& Scripts, std::size_t At)
		{
			if (At + 2 * InstructionBytes > Scripts.CodeSize())
			{
				return false;
			}
			const uint8* Code = Scripts.Code() + At;
			const std::size_t Target = static_cast<std::size_t>(Code[4] | Code[5] << 8);
			return std::equal(WaitNone.begin(), WaitNone.end(), Code) && Code[3] == JumpOpcode && Target == At;
		}

		/**
		 * The first character, in the game's order, who has joined and is in the menus, is not Excluded and not in Party: the
		 * one Member Change would offer first. EmptySlot when there is none.
		 */
		uint8 FirstFreeMember(const FGameState& State, const std::array<uint8, 3>& Party, uint8 Excluded)
		{
			const uint16 Available = State.JoinedCharacters() & State.MenuCharacters();
			for (uint8 Character = 0; Character < CharacterCount; ++Character)
			{
				if ((Available & 1 << Character) != 0 && Character != Excluded && std::find(Party.begin(), Party.end(), Character) == Party.end())
				{
					return Character;
				}
			}
			return FGameState::EmptySlot;
		}

		/** The party in its slots, each slot's riding byte with it, and the field's copy of it in the variables. */
		void PlaceParty(FGameState& State, const std::array<uint8, 3>& Party, const std::array<uint8, 3>& Riding)
		{
			State.SetParty(Party);
			for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
			{
				State.SetPartySlotInGear(static_cast<int32>(Slot), Riding[Slot]);
				State.SetSavedEventVariable(PartyVariable + static_cast<int32>(Slot), Party[Slot]);
			}
		}
	}

	void Make(FGameState& State)
	{
		Start.Apply(State);
		State.SetSavedEventVariable(MapVariable, Start.Map);
		State.SetFieldSong(BridgeSong);
		State.SetSavedEventVariable(SceneVariable, BridgeScene);
		State.SetSavedEventVariable(OrbsVariable, 0);
		State.SetSavedEventVariable(OrbsFallenVariable, 0);
		State.SetSavedEventVariable(GatesVariable, 0);
		State.SetSavedEventVariable(DeusVariable, static_cast<uint16>(State.SavedEventVariable(DeusVariable) & ~DeusBits));
		State.TakeItem(ItemsList, DeusMap);
		for (int32 Global = FirstOrbGlobal; Global <= LastOrbGlobal; ++Global)
		{
			State.BattleGlobals()[static_cast<std::size_t>(Global)] = 0;
		}
		// Aboard the ship before Deus, Fei leads and nobody rides; the core's member change had unlocked everyone, and the
		// last of the closing scenes took Elly out of the menus. A third member as every party at this point has: the
		// ship's member change refuses a party of two.
		State.SetLockedCharacters(1 << Fei);
		State.SetMenuCharacters(State.MenuCharacters() | EllyBit);
		State.SetJoinedCharacters(State.JoinedCharacters() | EllyBit);
		State.SetCharacterGear(Elly, Regurus);
		PlaceParty(State, {Fei, Elly, FirstFreeMember(State, {Fei, Elly, FGameState::EmptySlot}, Elly)}, {});
		// Boarded, as the bridge entered from the world map leaves the game: a load enters it by the scene's entrance
		// instead, which neither restores the gears nor marks the ship landed.
		State.RestoreGears();
		State.SetSavedEventVariable(ShipVariable, static_cast<uint16>(State.SavedEventVariable(ShipVariable) | AboardBit));
		State.SetClearGame(EClearGame::ClearFile);
	}

	void HideElly(FGameState& State)
	{
		// Her place goes to the member Member Change would offer first, so the party keeps its size (the ship's member
		// change refuses a party of two); with nobody to take it, the members after her close up, each with its riding
		// byte, as the game's PartyRemove moves them.
		const std::array<uint8, 3> Party = State.Party();
		const auto Place = std::find(Party.begin(), Party.end(), Elly);
		const uint8 Stand = FirstFreeMember(State, Party, Elly);
		if (Place != Party.end() && Stand != FGameState::EmptySlot)
		{
			std::array<uint8, 3> Taken = Party;
			std::array<uint8, 3> Riding{};
			for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
			{
				Riding[Slot] = State.PartySlotInGear(static_cast<int32>(Slot));
			}
			Taken[static_cast<std::size_t>(Place - Party.begin())] = Stand;
			PlaceParty(State, Taken, Riding);
			State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() & ~EllyBit));
			State.SetCharacterGear(Elly, Vierge);
			return;
		}
		std::array<uint8, 3> Kept{FGameState::EmptySlot, FGameState::EmptySlot, FGameState::EmptySlot};
		std::array<uint8, 3> Riding{};
		std::size_t To = 0;
		for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
		{
			if (Party[Slot] == Elly)
			{
				continue;
			}
			Kept[To] = Party[Slot];
			Riding[To] = State.PartySlotInGear(static_cast<int32>(Slot));
			++To;
		}
		PlaceParty(State, Kept, Riding);
		State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() & ~EllyBit));
		State.SetCharacterGear(Elly, Vierge);
	}

	bool IsEndingWaiting(const FEventContext& Context)
	{
		if (Context.MapNumber != EndingMap || Context.Scripts == nullptr || Context.States.empty())
		{
			return false;
		}
		// Between ticks its thread stands at the wait, or at the jump once the wait has run.
		for (const FEventThread& Thread : Context.States.front().Threads)
		{
			if (!Thread.IsFree() && (IsParkedAt(*Context.Scripts, Thread.Pc) || (Thread.Pc >= InstructionBytes && IsParkedAt(*Context.Scripts, Thread.Pc - InstructionBytes))))
			{
				return true;
			}
		}
		return false;
	}
}
