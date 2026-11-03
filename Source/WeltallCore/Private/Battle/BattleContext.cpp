#include "Battle/BattleContext.h"

#include "Game/GameState.h"

namespace Weltall
{
	bool FBattleContext::IsLoadedCommand(int32 Index)
	{
		if (Index < 0 || Index >= CommandRecordCount)
		{
			return false;
		}
		if (Index < FirstGearCommand)
		{
			return true;
		}
		if (Index < FirstEnemyCommand)
		{
			return (Index - FirstGearCommand) % GearTableSize < FBattleTables::GearCommandCount;
		}
		return Index - FirstEnemyCommand < FBattleTables::EnemyCommandCount;
	}

	FCharacterLooks FBattleContext::PartyLooks() const
	{
		if (GameState == nullptr)
		{
			return {};
		}
		const bool bStaged = (Formation.Flags & (FormationFlags::EventBattle | FormationFlags::SpecialParty)) != 0;
		return bStaged ? GameState->CharacterLooks().WithOwnNumbering() : GameState->CharacterLooks();
	}

	bool FBattleContext::IsTrueId(int32 Slot, int32 Special) const
	{
		constexpr int32 Infinity = 1;
		return Slot >= 0 && IsParty(Slot) && Special == Infinity && Combatants[static_cast<std::size_t>(Slot)].Gear == FCharacterLooks::Xenogears
			&& PartyLooks().WearsIdsGear(FCharacterLooks::Xenogears);
	}

	uint8 FBattleContext::DrawnOccupant(int32 Slot) const
	{
		const uint8 Occupant = Slots[static_cast<std::size_t>(Slot)].Occupant;
		return IsParty(Slot) ? PartyLooks().Drawn(Occupant) : Occupant;
	}
}
