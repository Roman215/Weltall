#include "Game/SaveWrite.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Support/GameText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint8 LeadByte = 0xFE;

	/** Glyph 0 is a full-width code, 1 'F', 2 'e', 3 'i', 4 the second page's first code, 5 'F' again. */
	FMesNameTables MakeNameTables()
	{
		return Testing::MakeGlyphMapTables({{0x00, 0x61}, {0x00, 0x25}, {0x00, 0x41}, {0x00, 0x45}, {0xFE, 0x00}, {0x00, 0x25}});
	}

	void SetName(FGameState& State, int32 Name, std::initializer_list<uint8> Codes)
	{
		const std::span<uint8> Slot = State.Block().subspan(static_cast<std::size_t>(Name) * FGameState::NameBytes, FGameState::NameBytes);
		std::fill(Slot.begin(), Slot.end(), uint8{0});
		std::copy(Codes.begin(), Codes.end(), Slot.begin());
	}

	/**
	 * Fei (level 12), Bart and Rico in the party; Fei and Bart have joined by both masks, Rico by one
	 * only. Every byte of the state has a value of its own.
	 */
	FGameState MakeState()
	{
		FGameState State;
		const std::span<uint8> Block = State.Block();
		for (std::size_t Offset = 0; Offset < Block.size(); ++Offset)
		{
			Block[Offset] = static_cast<uint8>(Offset * 5 + 3);
		}
		for (int32 Name = 0; Name < FGameState::NameCount; ++Name)
		{
			SetName(State, Name, {});
		}
		SetName(State, 0, {0x25, 0x41, 0x45});
		SetName(State, 3, {0xFE, 0x00, 0x25});
		SetName(State, 4, {0x61});
		SetName(State, 5, {0x25, 0x99, 0x41});
		LittleEndian::WriteU16(Block, 0x1D30, 0x0029);
		LittleEndian::WriteU16(Block, 0x1D32, 0x0009);
		Block[0x1D34] = 0;
		Block[0x1D35] = 3;
		Block[0x1D36] = 5;
		const std::size_t Fei = 0x26C;
		LittleEndian::WriteU16(Block, Fei + 0x4C, 321);
		LittleEndian::WriteU16(Block, Fei + 0x4E, 400);
		LittleEndian::WriteU16(Block, Fei + 0x50, 0x0133);
		LittleEndian::WriteU16(Block, Fei + 0x52, 0x0144);
		Block[Fei + 0x62] = 12;
		Block[Fei + 0x63] = 9;
		State.SetPlayTime(0x01020304);
		for (std::size_t Global = 0; Global < 16; ++Global)
		{
			State.BattleGlobals()[Global] = static_cast<uint16>(0xA00 + Global);
		}
		return State;
	}
}

TEST_CASE("A save's summary shows the party the menu shows, with their HP, EP and levels", "[Game]")
{
	FGameState State = MakeState();
	const FOriginalSave Save = WriteOriginalSave(State, 11, 0, MakeNameTables(), LeadByte);
	const std::span<const uint8> Data = Save.Data();
	CHECK(Save.PlayTime() == 0x01020304);
	CHECK(Save.CardSlot() == 11);
	// Rico has joined by only one of the two masks: the menu leaves him out.
	CHECK(Save.Party() == std::array<uint8, 3>{0, 3, FOriginalSave::EmptySlot});
	CHECK(LittleEndian::ReadU16(Data, 0x04) == 321);
	CHECK(LittleEndian::ReadU16(Data, 0x0A) == 400);
	CHECK(Data[0x10] == 0x33);
	CHECK(Data[0x13] == 0x44);
	CHECK(Save.Levels()[0] == 12);
	CHECK(Data[0x19] == 9);
	CHECK(LittleEndian::ReadU16(Data, 0x08) == 0);
	CHECK(Data[0x18] == 0);
	CHECK(std::all_of(Data.begin() + 0x1F, Data.begin() + 0x23, [](uint8 Byte) { return Byte == 0; }));
	CHECK(std::all_of(Data.begin() + 0x1B5C, Data.end() - 1, [](uint8 Byte) { return Byte == 0; }));
	CHECK(FOriginalSave::FromData(Data));
}

TEST_CASE("A save keeps the state's regions, each gear in part and the names as glyph indices", "[Game]")
{
	const FGameState Before = MakeState();
	FGameState State = Before;
	const FOriginalSave Save = WriteOriginalSave(State, 0, 1, MakeNameTables(), LeadByte);
	const std::span<const uint8> Data = Save.Data();
	const std::span<const uint8> Block = State.Block();
	CHECK(std::equal(Block.begin() + 0x26C, Block.begin() + 0x978, Data.begin() + 0x290));
	CHECK(std::equal(Block.begin() + 0x1648, Block.end(), Data.begin() + 0xE4C));
	const std::span<const uint8> Gear = Block.subspan(0x978 + 5 * 0xA4, 0xA4);
	const std::span<const uint8> SavedGear = Data.subspan(0x99C + 5 * 0x3C, 0x3C);
	CHECK(std::equal(Gear.begin(), Gear.begin() + 0x28, SavedGear.begin()));
	CHECK(LittleEndian::ReadU32(SavedGear, 0x28) == LittleEndian::ReadU32(Gear, 0x5C));
	CHECK(LittleEndian::ReadU32(SavedGear, 0x2C) == LittleEndian::ReadU32(Gear, 0x60));
	CHECK(LittleEndian::ReadU16(SavedGear, 0x34) == LittleEndian::ReadU16(Gear, 0x38));
	CHECK(SavedGear[0x38] == Gear[0x99]);
	CHECK(SavedGear[0x39] == Gear[0x74]);
	CHECK(SavedGear[0x3A] == Gear[0x75]);
	CHECK(LittleEndian::ReadU32(SavedGear, 0x30) == 0);
	// Fei; a second-page code; a code the map lacks ends the name as 8000h.
	CHECK(LittleEndian::ReadU16(Data, 0x24) == 1);
	CHECK(LittleEndian::ReadU16(Data, 0x28) == 3);
	CHECK(LittleEndian::ReadU16(Data, 0x2A) == 0);
	CHECK(LittleEndian::ReadU16(Data, 0x24 + 3 * 0x14) == 4);
	CHECK(LittleEndian::ReadU16(Data, 0x24 + 3 * 0x14 + 2) == 1);
	CHECK(LittleEndian::ReadU16(Data, 0x24 + 5 * 0x14 + 2) == 0x8000);
	CHECK(LittleEndian::ReadU16(Data, 0x24 + 5 * 0x14 + 4) == 0);
}

TEST_CASE("Saving stores the battle globals and the disc in the state, and reads the names back", "[Game]")
{
	FGameState State = MakeState();
	const FOriginalSave Save = WriteOriginalSave(State, 0, 1, MakeNameTables(), LeadByte);
	CHECK(LittleEndian::ReadU16(State.Block(), 0x2324) == 0xA00);
	CHECK(LittleEndian::ReadU16(State.Block(), 0x2342) == 0xA0F);
	CHECK(LittleEndian::ReadU16(Save.Data(), 0x1124 + 0x2342 - 0x1920) == 0xA0F);
	CHECK(State.SavedDisc() == 1);
	CHECK(LittleEndian::ReadU16(State.Block(), 0x19D4) == 1);
	// Glyph 0 ends a saved name, so a name starting with it comes back empty.
	CHECK(State.Name(4).empty());
	CHECK(State.Name(0).size() == 3);
}

TEST_CASE("A save keeps the player's party order in the port's part and shows its summary in that order", "[Game]")
{
	FGameState State = MakeState();
	FPartyOrder Order;
	const std::array<uint8, 2> BartFirst{3, 0};
	Order.Arrange(State.Party(), BartFirst);
	State.SetPartyOrder(Order);
	const FOriginalSave Save = WriteOriginalSave(State, 11, 0, MakeNameTables(), LeadByte);
	const std::span<const uint8> Data = Save.Data();
	CHECK(Save.Party() == std::array<uint8, 3>{3, 0, FOriginalSave::EmptySlot});
	// Fei's HP and level now in the summary's second place; the game's own party keeps the story's order.
	CHECK(LittleEndian::ReadU16(Data, 0x06) == 321);
	CHECK(Save.Levels()[1] == 12);
	CHECK(Data[0x1124 + 0x1D34 - 0x1920] == 0);
	CHECK(Data[0x1124 + 0x1D35 - 0x1920] == 3);
	CHECK(Save.StoryMode() == EStoryMode::Unchosen);
	CHECK(Save.PartyOrder() == Order);
	CHECK(FOriginalSave::FromData(Data));
}

TEST_CASE("A save without an order keeps none, and one the port wrote without it reads as none set", "[Game]")
{
	FGameState State = MakeState();
	State.SetStoryMode(EStoryMode::Original);
	const FOriginalSave Save = WriteOriginalSave(State, 0, 0, MakeNameTables(), LeadByte);
	CHECK(Save.StoryMode() == EStoryMode::Original);
	CHECK_FALSE(Save.PartyOrder().IsSet());
	FGameState Plain = MakeState();
	CHECK_FALSE(WriteOriginalSave(Plain, 0, 0, MakeNameTables(), LeadByte).PartyOrder().IsSet());
}

TEST_CASE("A save keeps how the player has Citan and Emeralda drawn, and shows its faces so", "[Game]")
{
	constexpr uint8 Bart = 3;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 GrownEmeralda = 10;
	FGameState State = MakeState();
	State.SetParty({0, GrownEmeralda, Bart});
	LittleEndian::WriteU16(State.Block(), 0x1D30, static_cast<uint16>(0x0009 | 1u << GrownEmeralda));
	LittleEndian::WriteU16(State.Block(), 0x1D32, static_cast<uint16>(0x0009 | 1u << GrownEmeralda));
	const std::size_t Grown = 0x26C + GrownEmeralda * 0xA4;
	LittleEndian::WriteU16(State.Block(), Grown + 0x4C, 555);
	FCharacterLooks Looks;
	Looks.Switch(GrownEmeralda);
	State.SetCharacterLooks(Looks);
	const FOriginalSave Save = WriteOriginalSave(State, 2, 0, MakeNameTables(), LeadByte);
	// The summary's face is the child's, her HP the grown Emeralda's own; the game's party keeps the story's record.
	CHECK(Save.Party() == std::array<uint8, 3>{0, Emeralda, Bart});
	CHECK(LittleEndian::ReadU16(Save.Data(), 0x06) == 555);
	CHECK(Save.Data()[0x1124 + 0x1D35 - 0x1920] == GrownEmeralda);
	CHECK(Save.CharacterLooks() == Looks);
	CHECK_FALSE(Save.PartyOrder().IsSet());
	CHECK(FOriginalSave::FromData(Save.Data()));
}

TEST_CASE("A save with no look switched keeps none, as the game's own saves keep none", "[Game]")
{
	FGameState State = MakeState();
	const FOriginalSave Save = WriteOriginalSave(State, 0, 0, MakeNameTables(), LeadByte);
	CHECK_FALSE(Save.CharacterLooks().IsSet());
	CHECK(Save.Party() == std::array<uint8, 3>{0, 3, FOriginalSave::EmptySlot});
}
