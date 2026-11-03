#include "Game/GameState.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameNames.h"
#include "Game/GameStateLayout.h"
#include "Media/GameFileIndex.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 NewGameDirectory = 16;
		constexpr int32 NewGameFileNumber = 3;
		constexpr std::size_t GoldOffset = 0x1924;
		constexpr std::size_t SavedEventVariablesOffset = 0x1930;
		/** In the new-game file a name is up to ten glyph indices, ended by this one. */
		constexpr uint16 FileNameEnd = 0x000F;
		/** A new game's Normal Menu opens on Status. */
		constexpr uint8 NewGameMenuCommand = 6;

		static_assert(FGameState::CharacterCount == GameStateLayout::CharacterCount);
		static_assert(FGameState::InventoryListCount == static_cast<int32>(GameStateLayout::InventoryLists.size()));

		/** Where a character's gear byte is, for any number the scripts give: past the characters, a later record's place. */
		std::optional<std::size_t> CharacterGearAt(int32 Character)
		{
			if (Character < 0)
			{
				return std::nullopt;
			}
			const std::size_t At = GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::Gear;
			return At < FGameState::BlockSize ? std::optional<std::size_t>(At) : std::nullopt;
		}

		std::size_t CharacterPointsAt(int32 Character, FGameState::ECharacterPoints Which)
		{
			return GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::Hp + static_cast<std::size_t>(Which) * 2;
		}
	}

	TResult<FGameState> FGameState::NewGame(std::span<const uint8> NewGameFile, const FMesNameTables& NameTables)
	{
		if (NewGameFile.size() < BlockSize)
		{
			return MakeError("the new-game file is too short");
		}
		FGameState State;
		std::copy_n(NewGameFile.begin(), BlockSize, State.Bytes.begin());
		for (int32 Name = 0; Name < NameCount; ++Name)
		{
			const std::span<uint8> Slot = std::span<uint8>(State.Bytes).subspan(static_cast<std::size_t>(Name) * NameBytes, NameBytes);
			const TResult<GameNames::FNameSlot> Converted = GameNames::FromGlyphIndices(Slot, FileNameEnd, NameTables);
			if (!Converted)
			{
				return Converted.Error();
			}
			std::copy(Converted->begin(), Converted->end(), Slot.begin());
		}
		State.MenuLastCommand = NewGameMenuCommand;
		return State;
	}

	std::span<const uint8> FGameState::Name(int32 Index) const
	{
		const std::span<const uint8> Slot = std::span<const uint8>(Bytes).subspan(static_cast<std::size_t>(Index) * NameBytes, NameBytes);
		return Slot.first(static_cast<std::size_t>(std::find(Slot.begin(), Slot.end(), 0) - Slot.begin()));
	}

	uint16 FGameState::SavedEventVariable(int32 Index) const
	{
		return LittleEndian::ReadU16(Bytes, SavedEventVariablesOffset + static_cast<std::size_t>(Index) * 2);
	}

	void FGameState::SetSavedEventVariable(int32 Index, uint16 Value)
	{
		LittleEndian::WriteU16(Bytes, SavedEventVariablesOffset + static_cast<std::size_t>(Index) * 2, Value);
	}

	std::array<uint8, 3> FGameState::Party() const
	{
		const std::size_t Party = GameStateLayout::PartyOffset;
		return {Bytes[Party], Bytes[Party + 1], Bytes[Party + 2]};
	}

	void FGameState::SetParty(const std::array<uint8, 3>& Characters)
	{
		std::copy(Characters.begin(), Characters.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::PartyOffset));
	}

	uint32 FGameState::Gold() const
	{
		return LittleEndian::ReadU32(Bytes, GoldOffset);
	}

	void FGameState::SetGold(uint32 Value)
	{
		LittleEndian::WriteU32(Bytes, GoldOffset, Value);
	}

	std::span<uint8> FGameState::InventoryCounts(int32 List)
	{
		const GameStateLayout::FInventoryList& Layout = GameStateLayout::InventoryLists[static_cast<std::size_t>(List)];
		return std::span<uint8>(Bytes).subspan(Layout.Counts, Layout.Slots);
	}

	std::span<uint8> FGameState::InventoryItems(int32 List)
	{
		const GameStateLayout::FInventoryList& Layout = GameStateLayout::InventoryLists[static_cast<std::size_t>(List)];
		return std::span<uint8>(Bytes).subspan(Layout.Items, Layout.Slots);
	}

	bool FGameState::TakeItem(int32 List, uint8 Item)
	{
		constexpr uint8 NoItem = 0xFF;
		const std::span<uint8> Counts = InventoryCounts(List);
		const std::span<uint8> Items = InventoryItems(List);
		for (std::size_t Slot = 0; Slot < Counts.size(); ++Slot)
		{
			if (Items[Slot] == Item && Counts[Slot] != 0)
			{
				if (--Counts[Slot] == 0)
				{
					Items[Slot] = NoItem;
				}
				return true;
			}
		}
		return false;
	}

	uint16 FGameState::JoinedCharacters() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::JoinedCharactersOffset);
	}

	void FGameState::SetJoinedCharacters(uint16 Characters)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::JoinedCharactersOffset, Characters);
	}

	uint16 FGameState::MenuCharacters() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::CharacterMaskOffset);
	}

	void FGameState::SetMenuCharacters(uint16 Characters)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::CharacterMaskOffset, Characters);
	}

	uint16 FGameState::LockedCharacters() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::LockedCharactersOffset);
	}

	void FGameState::SetLockedCharacters(uint16 Characters)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::LockedCharactersOffset, Characters);
	}

	uint8 FGameState::PartySlotInGear(int32 Slot) const
	{
		return Bytes[GameStateLayout::PartyInGearOffset + static_cast<std::size_t>(Slot)];
	}

	void FGameState::SetPartySlotInGear(int32 Slot, uint8 InGear)
	{
		Bytes[GameStateLayout::PartyInGearOffset + static_cast<std::size_t>(Slot)] = InGear;
	}

	std::optional<uint8> FGameState::CharacterGear(int32 Character) const
	{
		const std::optional<std::size_t> At = CharacterGearAt(Character);
		return At ? std::optional<uint8>(Bytes[*At]) : std::nullopt;
	}

	bool FGameState::SetCharacterGear(int32 Character, uint8 Gear)
	{
		const std::optional<std::size_t> At = CharacterGearAt(Character);
		if (At)
		{
			Bytes[*At] = Gear;
		}
		return At.has_value();
	}

	uint8 FGameState::CharacterWeight(int32 Character) const
	{
		return Bytes[GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::Weight];
	}

	uint8 FGameState::CharacterWeightGain(int32 Character) const
	{
		return Bytes[GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::WeightGain];
	}

	void FGameState::SetCharacterWeightGain(int32 Character, uint8 Gain)
	{
		Bytes[GameStateLayout::CharacterOffset(static_cast<std::size_t>(Character)) + GameStateLayout::Character::WeightGain] = Gain;
	}

	uint16 FGameState::CharacterPoints(int32 Character, ECharacterPoints Which) const
	{
		return LittleEndian::ReadU16(Bytes, CharacterPointsAt(Character, Which));
	}

	void FGameState::SetCharacterPoints(int32 Character, ECharacterPoints Which, uint16 Value)
	{
		LittleEndian::WriteU16(Bytes, CharacterPointsAt(Character, Which), Value);
	}

	void FGameState::RestoreGears()
	{
		for (int32 Gear = 0; Gear < GameStateLayout::GearCount; ++Gear)
		{
			const std::size_t At = GameStateLayout::GearOffset(static_cast<std::size_t>(Gear));
			LittleEndian::WriteU32(Bytes, At + GameStateLayout::Gear::Hp, LittleEndian::ReadU32(Bytes, At + GameStateLayout::Gear::MaxHp));
			LittleEndian::WriteU16(Bytes, At + GameStateLayout::Gear::Fuel, LittleEndian::ReadU16(Bytes, At + GameStateLayout::Gear::MaxFuel));
		}
	}

	std::optional<uint32> FGameState::GearHp(int32 Gear) const
	{
		if (Gear < 0 || Gear >= GearCount)
		{
			return std::nullopt;
		}
		return LittleEndian::ReadU32(Bytes, GameStateLayout::GearOffset(static_cast<std::size_t>(Gear)) + GameStateLayout::Gear::Hp);
	}

	std::optional<uint32> FGameState::GearMaxHp(int32 Gear) const
	{
		if (Gear < 0 || Gear >= GearCount)
		{
			return std::nullopt;
		}
		return LittleEndian::ReadU32(Bytes, GameStateLayout::GearOffset(static_cast<std::size_t>(Gear)) + GameStateLayout::Gear::MaxHp);
	}

	bool FGameState::SetGearHp(int32 Gear, uint32 Hp)
	{
		if (Gear < 0 || Gear >= GearCount)
		{
			return false;
		}
		LittleEndian::WriteU32(Bytes, GameStateLayout::GearOffset(static_cast<std::size_t>(Gear)) + GameStateLayout::Gear::Hp, Hp);
		return true;
	}

	bool FGameState::CopyCharacter(int32 From, int32 To)
	{
		if (From < 0 || From >= CharacterCount || To < 0 || To >= CharacterCount)
		{
			return false;
		}
		const auto Source = static_cast<std::size_t>(From);
		const auto Target = static_cast<std::size_t>(To);
		std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::CharacterOffset(Source)), GameStateLayout::CharacterSize,
			Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::CharacterOffset(Target)));
		std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::LearnedSkillsOffsetOf(Source)), GameStateLayout::LearnedSkillsSize,
			Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::LearnedSkillsOffsetOf(Target)));
		return true;
	}

	bool FGameState::CopyGear(int32 From, int32 To)
	{
		if (From < 0 || From >= GearCount || To < 0 || To >= GearCount)
		{
			return false;
		}
		std::copy_n(Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::GearOffset(static_cast<std::size_t>(From))), GameStateLayout::GearSize,
			Bytes.begin() + static_cast<std::ptrdiff_t>(GameStateLayout::GearOffset(static_cast<std::size_t>(To))));
		return true;
	}

	std::optional<uint16> FGameState::CharacterSpecialSkills(int32 Character) const
	{
		if (Character < 0 || Character >= CharacterCount)
		{
			return std::nullopt;
		}
		return LittleEndian::ReadU16(Bytes, GameStateLayout::LearnedSkillsOffsetOf(static_cast<std::size_t>(Character)) + GameStateLayout::Skills::Special);
	}

	bool FGameState::SetCharacterSpecialSkills(int32 Character, uint16 Skills)
	{
		if (Character < 0 || Character >= CharacterCount)
		{
			return false;
		}
		LittleEndian::WriteU16(Bytes, GameStateLayout::LearnedSkillsOffsetOf(static_cast<std::size_t>(Character)) + GameStateLayout::Skills::Special, Skills);
		return true;
	}

	uint16 FGameState::StoryFlags() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::StoryFlagsOffset);
	}

	void FGameState::SetStoryFlags(uint16 Flags)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::StoryFlagsOffset, Flags);
	}

	std::array<uint8, 8> FGameState::ArenaDefeatedGears() const
	{
		std::array<uint8, 8> Gears{};
		std::copy_n(Bytes.begin() + GameStateLayout::ArenaDefeatedGearsOffset, Gears.size(), Gears.begin());
		return Gears;
	}

	void FGameState::SetArenaDefeatedGears(const std::array<uint8, 8>& Gears)
	{
		std::copy(Gears.begin(), Gears.end(), Bytes.begin() + GameStateLayout::ArenaDefeatedGearsOffset);
	}

	uint32 FGameState::ArenaSettings() const
	{
		return LittleEndian::ReadU32(Bytes, GameStateLayout::ArenaSettingsOffset);
	}

	void FGameState::SetArenaSettings(uint32 Settings)
	{
		LittleEndian::WriteU32(Bytes, GameStateLayout::ArenaSettingsOffset, Settings);
	}

	int32 FGameState::SavedDisc() const
	{
		return Bytes[GameStateLayout::SavedDiscOffset];
	}

	uint16 FGameState::Place() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::PlaceOffset);
	}

	void FGameState::SetPlace(uint16 Place)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::PlaceOffset, Place);
	}

	void FGameState::SetFieldPlace(uint16 Map, uint16 CameraYaw, uint16 Entrance, uint16 Song)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::PlaceOffset, Map);
		LittleEndian::WriteU16(Bytes, GameStateLayout::CameraYawOffset, CameraYaw);
		LittleEndian::WriteU16(Bytes, GameStateLayout::SceneOffset, Entrance);
		LittleEndian::WriteU16(Bytes, GameStateLayout::SongOffset, Song);
	}

	uint16 FGameState::CameraYaw() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::CameraYawOffset);
	}

	uint16 FGameState::Scene() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::SceneOffset);
	}

	uint16 FGameState::FieldSong() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::SongOffset);
	}

	void FGameState::SetFieldSong(uint16 Song)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::SongOffset, Song);
	}

	void FGameState::SetFieldEntrance(uint16 Entrance)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::SceneOffset, Entrance);
	}

	void FGameState::SetWorldMapPlace(uint16 Place, uint16 Entrance, uint16 CameraYaw, uint16 Scene)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::PlaceOffset, Place);
		LittleEndian::WriteU16(Bytes, GameStateLayout::WorldMapEntranceOffset, Entrance);
		LittleEndian::WriteU16(Bytes, GameStateLayout::CameraYawOffset, CameraYaw);
		LittleEndian::WriteU16(Bytes, GameStateLayout::SceneOffset, Scene);
	}

	std::array<uint16, 4> FGameState::WorldMapPosition() const
	{
		std::array<uint16, 4> Position{};
		for (std::size_t Word = 0; Word < Position.size(); ++Word)
		{
			Position[Word] = LittleEndian::ReadU16(Bytes, GameStateLayout::WorldMapPositionOffset + 2 * Word);
		}
		return Position;
	}

	void FGameState::SetWorldMapPosition(const std::array<uint16, 4>& Position)
	{
		for (std::size_t Word = 0; Word < Position.size(); ++Word)
		{
			LittleEndian::WriteU16(Bytes, GameStateLayout::WorldMapPositionOffset + 2 * Word, Position[Word]);
		}
	}

	uint16 FGameState::WorldMapFlags() const
	{
		return LittleEndian::ReadU16(Bytes, GameStateLayout::WorldMapFlagsOffset);
	}

	void FGameState::SetWorldMapFlags(uint16 Flags)
	{
		LittleEndian::WriteU16(Bytes, GameStateLayout::WorldMapFlagsOffset, Flags);
	}

	void FGameState::SetWorldMapVehicle(int16 X, int16 Y, int16 Z, int16 Heading)
	{
		const std::array<int16, 4> Words{X, Y, Z, Heading};
		for (std::size_t Word = 0; Word < Words.size(); ++Word)
		{
			LittleEndian::WriteU16(Bytes, GameStateLayout::WorldMapPositionOffset + 2 * Word, static_cast<uint16>(Words[Word]));
		}
	}

	std::array<uint16, 2> FGameState::SeaObjectPlace() const
	{
		return {LittleEndian::ReadU16(Bytes, GameStateLayout::SeaObjectPlaceOffset), LittleEndian::ReadU16(Bytes, GameStateLayout::SeaObjectPlaceOffset + 2)};
	}

	std::array<uint16, 2> FGameState::AirObjectPlace() const
	{
		return {LittleEndian::ReadU16(Bytes, GameStateLayout::AirObjectXOffset), LittleEndian::ReadU16(Bytes, GameStateLayout::AirObjectZOffset)};
	}

	TResult<FGameState> LoadNewGameState(const FCdImage& Image, const FGameFileIndex& Index, const FMesNameTables& NameTables)
	{
		const FIndexRecord* Record = Index.Find(NewGameDirectory, NewGameFileNumber);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the new-game file");
		}
		const TResult<std::vector<uint8>> File = ReadFileSectors(Image, *Record);
		if (!File)
		{
			return File.Error();
		}
		return FGameState::NewGame(*File, NameTables);
	}
}
