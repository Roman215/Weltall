#include "Game/SaveWrite.h"

#include "Foundation/LittleEndian.h"
#include "Game/ClearFile.h"
#include "Game/GameNames.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"
#include "Game/KeepElly.h"
#include "Game/SaveLayout.h"
#include "Game/WayBackPoint.h"
#include "Mods/ExpansionMods.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	static_assert(SaveLayout::Port::PartyOrder - SaveLayout::Port::Expansion == ExpansionFolderBytes);

	namespace
	{
		using LittleEndian::ReadU16;
		using LittleEndian::WriteU16;

		/** The characters the joined mask can name. */
		constexpr uint16 CharacterBits = 0x7FF;
		constexpr int32 CharacterBitCount = 16;
		constexpr int32 CharacterNames = 11;
		constexpr std::size_t CharacterNamesOffset = 0x024;
		constexpr std::size_t GearNamesOffset = 0x100;

		/** The party as the menu shows it: members who have not joined, by both masks, are left out. */
		std::array<uint8, 3> MenuParty(std::span<const uint8> Block)
		{
			using namespace GameStateLayout;
			const uint16 Joined = static_cast<uint16>(ReadU16(Block, JoinedCharactersOffset) & ReadU16(Block, CharacterMaskOffset) & CharacterBits);
			std::array<uint8, 3> Party;
			for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
			{
				const uint8 Id = Block[PartyOffset + Slot];
				const bool bShown = Id < CharacterBitCount && (Joined >> Id & 1) != 0;
				Party[Slot] = bShown ? Id : FGameState::EmptySlot;
			}
			return Party;
		}

		/** The summary the File screen shows: the party in the order its player keeps it, each face as Looks draws it, as the menus show them. */
		void WriteSummary(std::span<const uint8> Block, const FPartyOrder& Order, const FCharacterLooks& Looks, uint32 PlayTime, int32 CardSlot, std::span<uint8> Data)
		{
			namespace Summary = SaveLayout::Summary;
			namespace Character = GameStateLayout::Character;
			// Placed as the whole party is, as the menus place it, then each member the menus leave out left out.
			constexpr std::size_t PartyOffset = GameStateLayout::PartyOffset;
			const FPartyOrder::FPlaces Places = Order.PlacesOf({Block[PartyOffset], Block[PartyOffset + 1], Block[PartyOffset + 2]});
			const std::array<uint8, 3> Members = MenuParty(Block);
			std::array<uint8, 3> Party;
			for (std::size_t Place = 0; Place < Party.size(); ++Place)
			{
				Party[Place] = Places[Place] == FPartyOrder::Nobody ? FGameState::EmptySlot : Members[static_cast<std::size_t>(Places[Place])];
			}
			for (std::size_t Slot = 0; Slot < Party.size(); ++Slot)
			{
				if (Party[Slot] == FGameState::EmptySlot)
				{
					Data[Summary::Party + Slot] = FGameState::EmptySlot;
					continue;
				}
				Data[Summary::Party + Slot] = Looks.Drawn(Party[Slot]);
				const std::span<const uint8> Record = Block.subspan(GameStateLayout::CharacterOffset(Party[Slot]), GameStateLayout::CharacterSize);
				WriteU16(Data, Summary::Hp + Slot * 2, ReadU16(Record, Character::Hp));
				WriteU16(Data, Summary::MaxHp + Slot * 2, ReadU16(Record, Character::MaxHp));
				Data[Summary::Ep + Slot] = Record[Character::Ep];
				Data[Summary::MaxEp + Slot] = Record[Character::MaxEp];
				Data[Summary::Levels + Slot] = Record[Character::Level];
				Data[Summary::EtherLevels + Slot] = Record[Character::EtherLevel];
			}
			Data[Summary::CardSlot] = static_cast<uint8>(CardSlot);
			LittleEndian::WriteU32(Data, Summary::PlayTime, PlayTime);
		}

		/**
		 * Keeps in Data's unread bytes where Played differs from it, as runs (SaveLayout::Port::KeptRuns); false, with
		 * nothing written, where they do not fit.
		 */
		bool KeepRuns(std::span<const uint8> Played, std::span<uint8> Data)
		{
			namespace Port = SaveLayout::Port;
			// Bytes alike between two differing ones are kept with them while that costs less than a run's head.
			constexpr std::size_t RunHead = 3;
			constexpr std::size_t LongestRun = 0xFF;
			std::vector<uint8> Runs;
			int32 Count = 0;
			for (std::size_t At = 0; At < Port::Mark;)
			{
				if (Played[At] == Data[At])
				{
					++At;
					continue;
				}
				std::size_t End = At + 1;
				for (std::size_t Next = End; Next < Port::Mark && Next - At < LongestRun && Next - End < RunHead; ++Next)
				{
					if (Played[Next] != Data[Next])
					{
						End = Next + 1;
					}
				}
				Runs.push_back(static_cast<uint8>(At));
				Runs.push_back(static_cast<uint8>(At >> 8));
				Runs.push_back(static_cast<uint8>(End - At));
				Runs.insert(Runs.end(), Played.begin() + static_cast<std::ptrdiff_t>(At), Played.begin() + static_cast<std::ptrdiff_t>(End));
				++Count;
				At = End;
			}
			if (Count > 0xFF || Port::KeptRuns + 1 + Runs.size() > Port::RunsEnd)
			{
				return false;
			}
			Data[Port::KeptRuns] = static_cast<uint8>(Count);
			std::copy(Runs.begin(), Runs.end(), Data.begin() + static_cast<std::ptrdiff_t>(Port::KeptRuns + 1));
			return true;
		}

		/** The game data of a save of State, without the port's own part or the checksum. */
		std::vector<uint8> WriteData(FGameState& State, int32 CardSlot, int32 Disc, const FMesNameTables& NameTables, uint8 LeadByte)
		{
			const std::span<uint8> Block = State.Block();
			for (std::size_t Global = 0; Global < State.BattleGlobals().size(); ++Global)
			{
				WriteU16(Block, GameStateLayout::SavedBattleGlobalsOffset + Global * 2, State.BattleGlobals()[Global]);
			}
			WriteU16(Block, GameStateLayout::SavedDiscOffset, static_cast<uint16>(Disc));

			std::vector<uint8> Data(FOriginalSave::DataSize, 0);
			WriteSummary(Block, State.PartyOrder(), State.CharacterLooks(), State.PlayTime(), CardSlot, Data);
			SaveLayout::CopyToSave(SaveLayout::RegionsBeforeGears, Block, Data);
			SaveLayout::CopyToSave(SaveLayout::RegionsAfterGears, Block, Data);
			for (int32 Name = 0; Name < FGameState::NameCount; ++Name)
			{
				const std::span<const uint8> Slot = Block.subspan(static_cast<std::size_t>(Name) * FGameState::NameBytes, FGameState::NameBytes);
				const GameNames::FNameSlot Stored = GameNames::ToGlyphIndices(Slot, LeadByte, NameTables);
				const std::size_t Offset = Name < CharacterNames ? CharacterNamesOffset + static_cast<std::size_t>(Name) * FGameState::NameBytes
																 : GearNamesOffset + static_cast<std::size_t>(Name - CharacterNames) * FGameState::NameBytes;
				std::copy(Stored.begin(), Stored.end(), Data.begin() + static_cast<std::ptrdiff_t>(Offset));
				// The Menu stores the names in the state itself and reads them back, so a name the save cannot
				// keep (one starting with glyph 0 reads back empty) changes in the state too. One holding a code
				// the glyph map lacks, which the game reads back from memory past the map, stays as it was.
				const TResult<GameNames::FNameSlot> ReadBack = GameNames::FromGlyphIndices(Stored, SaveLayout::SavedNameEnd, NameTables);
				if (ReadBack)
				{
					std::copy(ReadBack->begin(), ReadBack->end(), Block.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(Name) * FGameState::NameBytes));
				}
			}
			for (int32 Gear = 0; Gear < GameStateLayout::GearCount; ++Gear)
			{
				const std::span<const uint8> Record = Block.subspan(GameStateLayout::GearOffset(static_cast<std::size_t>(Gear)), GameStateLayout::GearSize);
				const std::span<uint8> Saved = std::span<uint8>(Data).subspan(SaveLayout::GearsOffset + static_cast<std::size_t>(Gear) * SaveLayout::GearSize, SaveLayout::GearSize);
				SaveLayout::CopyToSave(SaveLayout::GearFieldsBeforeRecompute, Record, Saved);
				SaveLayout::CopyToSave(SaveLayout::GearFieldsAfterRecompute, Record, Saved);
			}
			return Data;
		}
	}

	FOriginalSave WriteOriginalSave(FGameState& State, int32 CardSlot, int32 Disc, const FMesNameTables& NameTables, uint8 LeadByte, const FWayBackPoint* WayBack)
	{
		std::vector<uint8> Data = WriteData(State, CardSlot, Disc, NameTables, LeadByte);
		const bool bHidesElly = State.ClearGame() == EClearGame::ClearFile || KeepElly::IsBack(State);
		if (WayBack != nullptr || bHidesElly)
		{
			// The original's save at the point, and with Elly away where the port has her back, the game's own kept beside it.
			FGameState Back = State;
			if (WayBack != nullptr)
			{
				WayBack->Apply(Back);
			}
			if (bHidesElly)
			{
				ClearFile::HideElly(Back);
			}
			std::vector<uint8> Original = WriteData(Back, CardSlot, Disc, NameTables, LeadByte);
			if (KeepRuns(Data, Original))
			{
				Data = std::move(Original);
			}
		}
		// The port's part only once it has something to keep: until then the save is the game's own to the byte.
		if (State.StoryMode() != EStoryMode::Unchosen || State.PartyOrder().IsSet() || State.CharacterLooks().IsSet() || State.IsClearGame())
		{
			Data[SaveLayout::Port::ClearGame] = static_cast<uint8>(State.ClearGame());
			std::copy(SaveLayout::Port::MarkBytes.begin(), SaveLayout::Port::MarkBytes.end(), Data.begin() + static_cast<std::ptrdiff_t>(SaveLayout::Port::Mark));
			Data[SaveLayout::Port::StoryMode] = static_cast<uint8>(State.StoryMode());
			const std::string& Expansion = State.StoryMode() == EStoryMode::Expanded ? State.Expansion() : std::string();
			std::copy_n(Expansion.begin(), std::min(Expansion.size(), ExpansionFolderBytes), Data.begin() + static_cast<std::ptrdiff_t>(SaveLayout::Port::Expansion));
			const std::array<uint8, FPartyOrder::SavedBytes> Order = State.PartyOrder().ToSaved();
			std::copy(Order.begin(), Order.end(), Data.begin() + static_cast<std::ptrdiff_t>(SaveLayout::Port::PartyOrder));
			const std::array<uint8, FCharacterLooks::SavedBytes> Looks = State.CharacterLooks().ToSaved();
			std::copy(Looks.begin(), Looks.end(), Data.begin() + static_cast<std::ptrdiff_t>(SaveLayout::Port::CharacterLooks));
		}
		return FOriginalSave::WithChecksum(std::move(Data));
	}
}
