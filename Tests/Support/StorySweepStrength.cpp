#include "Support/StorySweepStrength.h"

#include "Game/GameState.h"
#include "Game/KeepElly.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace Weltall::Testing
{
	namespace
	{
		// The characters' and the gears' records in the state's block.
		constexpr std::size_t Characters = 0x26C;
		constexpr std::size_t CharacterCount = 11;
		constexpr std::size_t Gears = 0x978;
		constexpr std::size_t GearCount = 20;
		constexpr std::size_t RecordSize = 0xA4;

		// A character's points and their caps, and its base values: below a byte's end, which a level gained adds to.
		constexpr std::size_t CharacterHp = 0x4C;
		constexpr std::size_t CharacterMaxHp = 0x4E;
		constexpr std::size_t CharacterEp = 0x50;
		constexpr std::size_t CharacterMaxEp = 0x52;
		constexpr uint16 MostHp = 999;
		constexpr uint16 MostEp = 99;
		struct FBaseValue
		{
			std::size_t Offset = 0;
			uint8 Value = 0;
		};
		constexpr std::array<FBaseValue, 7> CharacterValues = {{
			{0x58, 150}, // attack
			{0x59, 150}, // defense
			{0x5A, 16}, // agility, as far as a battle counts it
			{0x5B, 150}, // ether
			{0x5C, 150}, // ether defense
			{0x5E, 99}, // hit
			{0x5F, 90}, // evade
		}};

		// A gear's fuel and frame points with their most, its engine's output and its armor.
		constexpr std::size_t GearFuel = 0x38;
		constexpr std::size_t GearMaxFuel = 0x3A;
		constexpr std::size_t GearOutput = 0x3C;
		constexpr std::size_t GearHp = 0x60;
		constexpr std::size_t GearMaxHp = 0x64;
		constexpr std::size_t GearDefense = 0x70;
		constexpr std::size_t GearEtherDefense = 0x72;
		constexpr uint16 StrongFuel = 30000;
		constexpr uint8 StrongOutput = 250;
		constexpr uint32 StrongFrame = 60000;
		constexpr uint16 StrongArmor = 20000;

		template <typename TValue>
		void Put(std::span<uint8> Record, std::size_t Offset, TValue Value)
		{
			std::memcpy(Record.data() + Offset, &Value, sizeof(Value));
		}
	}

	void StrengthenParty(FGameState& State)
	{
		const std::span<uint8, FGameState::BlockSize> Block = State.Block();
		for (std::size_t Character = 0; Character < CharacterCount; ++Character)
		{
			const std::span<uint8> Record = Block.subspan(Characters + Character * RecordSize, RecordSize);
			Put(Record, CharacterHp, MostHp);
			Put(Record, CharacterMaxHp, MostHp);
			Put(Record, CharacterEp, MostEp);
			Put(Record, CharacterMaxEp, MostEp);
			for (const FBaseValue& Base : CharacterValues)
			{
				Record[Base.Offset] = Base.Value;
			}
		}
		for (std::size_t Gear = 0; Gear < GearCount; ++Gear)
		{
			const std::span<uint8> Record = Block.subspan(Gears + Gear * RecordSize, RecordSize);
			Put(Record, GearFuel, StrongFuel);
			Put(Record, GearMaxFuel, StrongFuel);
			Record[GearOutput] = StrongOutput;
			Put(Record, GearHp, StrongFrame);
			Put(Record, GearMaxHp, StrongFrame);
			Put(Record, GearDefense, StrongArmor);
			Put(Record, GearEtherDefense, StrongArmor);
		}
	}

	void BringEllyBack(FGameState& State)
	{
		constexpr uint8 Elly = 1;
		constexpr uint16 EllyBit = 1 << Elly;
		constexpr uint8 Regurus = 10;
		State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() | EllyBit));
		State.SetMenuCharacters(static_cast<uint16>(State.MenuCharacters() | EllyBit));
		State.SetCharacterGear(Elly, Regurus);
		std::array<uint8, 3> Party = State.Party();
		if (std::find(Party.begin(), Party.end(), Elly) == Party.end())
		{
			Party[1] = Elly;
			State.SetParty(Party);
		}
		State.SetClearGame(EClearGame::ClearFile);
	}

	void DrawFeiAsId(FGameState& State)
	{
		constexpr uint8 Fei = 0;
		if (!State.IsClearGame())
		{
			State.SetClearGame(EClearGame::NewGamePlus);
		}
		FCharacterLooks Looks = State.CharacterLooks();
		if (!Looks.IsId(Fei))
		{
			Looks.Switch(Fei);
			State.SetCharacterLooks(Looks);
		}
	}

	void KeepEllyInParty(FGameState& State)
	{
		constexpr uint8 Elly = 1;
		State.SetClearGame(EClearGame::KeepElly);
		KeepElly::Hold(State);
		std::array<uint8, 3> Party = State.Party();
		if ((State.JoinedCharacters() & 1 << Elly) != 0 && KeepElly::IsBack(State) && std::find(Party.begin(), Party.end(), Elly) == Party.end())
		{
			Party[1] = Elly;
			State.SetParty(Party);
		}
	}
}
