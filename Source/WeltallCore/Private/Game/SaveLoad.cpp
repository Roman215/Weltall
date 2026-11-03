#include "Game/SaveLoad.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameNames.h"
#include "Game/GameStateLayout.h"
#include "Game/GearPartTables.h"
#include "Game/GearRecompute.h"
#include "Game/OriginalSave.h"
#include "Game/SaveLayout.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		bool RecomputeGear(FGameState& State, int32 Gear, const FGearPartTables& Tables)
		{
			return ApplyGearFrame(State, Gear, Tables) && ApplyGearArmor(State, Gear, Tables) && ApplyGearEngine(State, Gear, Tables)
				&& ApplyGearParts(State, Gear, Tables);
		}
	}

	TResult<FGameState> LoadOriginalSave(const FOriginalSave& Save, FGameState State, const FGearPartTables& Tables, const FMesNameTables& NameTables,
		const FSaveLoadOptions& Options, bool bExpansionPlayed)
	{
		using namespace SaveLayout;
		const std::vector<uint8> Played = Save.PlayedData(bExpansionPlayed);
		const std::span<const uint8> Data = Played;
		const std::span<uint8> Block = State.Block();
		CopyToState(RegionsBeforeGears, Data, Block);
		for (int32 Gear = 0; Gear < GameStateLayout::GearCount; ++Gear)
		{
			const std::span<const uint8> Saved = Data.subspan(SaveLayout::GearsOffset + static_cast<std::size_t>(Gear) * SaveLayout::GearSize, SaveLayout::GearSize);
			const std::span<uint8> Record = Block.subspan(GameStateLayout::GearOffset(static_cast<std::size_t>(Gear)), GameStateLayout::GearSize);
			CopyToState(GearFieldsBeforeRecompute, Saved, Record);
			if (Options.bPenaltyFromSavedTolerance)
			{
				CopyToState(std::span(&GearWeightTolerance, 1), Saved, Record);
			}
			if (!RecomputeGear(State, Gear, Tables))
			{
				return MakeError("the save's gear " + std::to_string(Gear) + " is built of a part the game lacks");
			}
			CopyToState(GearFieldsAfterRecompute, Saved, Record);
		}
		if (Options.bDeathblowsFromGearInUse)
		{
			// Each recompute gives the pilot what its gear's parts grant: the gear in use goes last.
			for (int32 Character = 0; Character < GameStateLayout::CharacterCount; ++Character)
			{
				const uint8 Gear = Block[GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::Gear];
				if (Gear < GameStateLayout::GearCount && Tables.Pilot(Gear) == Character)
				{
					// The loop above recomputed this gear already, so its parts are in the tables.
					static_cast<void>(ApplyGearParts(State, Gear, Tables));
				}
			}
		}
		CopyToState(RegionsAfterGears, Data, Block);

		State.SetPlayTime(LittleEndian::ReadU32(Data, Summary::PlayTime));
		const bool bExpanded = Save.StoryMode() == EStoryMode::Expanded && bExpansionPlayed;
		State.SetStoryMode(Save.StoryMode() == EStoryMode::Expanded && !bExpansionPlayed ? EStoryMode::Original : Save.StoryMode());
		State.SetExpansion(bExpanded ? Save.Expansion() : std::string());
		State.SetPartyOrder(Save.PartyOrder());
		State.SetCharacterLooks(Save.CharacterLooks());
		State.SetClearGame(Save.ClearGame());
		for (std::size_t Global = 0; Global < State.BattleGlobals().size(); ++Global)
		{
			State.BattleGlobals()[Global] = LittleEndian::ReadU16(Block, GameStateLayout::SavedBattleGlobalsOffset + Global * 2);
		}
		for (int32 Name = 0; Name < FGameState::NameCount; ++Name)
		{
			const std::span<uint8> Slot = Block.subspan(static_cast<std::size_t>(Name) * FGameState::NameBytes, FGameState::NameBytes);
			const TResult<GameNames::FNameSlot> Converted = GameNames::FromGlyphIndices(Slot, SavedNameEnd, NameTables);
			if (!Converted)
			{
				return Converted.Error();
			}
			std::copy(Converted->begin(), Converted->end(), Slot.begin());
		}
		return State;
	}
}
