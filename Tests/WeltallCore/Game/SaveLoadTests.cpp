#include "Game/SaveLoad.h"

#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Game/ClearFile.h"
#include "Game/GearPartTables.h"
#include "Game/KeepElly.h"
#include "Game/OriginalSave.h"
#include "Game/SaveWrite.h"
#include "Game/WayBackPoint.h"
#include "Mods/ExpansionMods.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MessageTable.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr std::size_t SaveGears = 0x99C;
	constexpr std::size_t SaveGearSize = 0x3C;
	constexpr std::size_t StateGears = 0x978;
	constexpr std::size_t StateGearSize = 0xA4;
	constexpr uint8 Stale = 0xEE;

	/** Glyph 1 is 'F', 2 'e', 3 'i', 4 a second-page code; glyph 0 cannot be stored, as it ends a name. */
	FMesNameTables MakeNameTables()
	{
		return Testing::MakeGlyphMapTables({{0x00, 0x61}, {0x00, 0x25}, {0x00, 0x41}, {0x00, 0x45}, {0xFE, 0x40}});
	}

	/**
	 * Record 0 of each table is empty; frame, armor and engine 1 and part 1 are a heavy build; part 2
	 * grants the first tier's gear Deathblow to a pilot who knows the tier's other two. Fei pilots
	 * every gear.
	 */
	FGearPartTables MakeTables()
	{
		std::vector<uint8> Frames(2 * FGearPartTables::FrameSize, 0);
		LittleEndian::WriteU32(Frames, 0x18 + 4, 1800);
		std::vector<uint8> Armors(2 * FGearPartTables::ArmorSize, 0);
		LittleEndian::WriteU16(Armors, 0x14 + 8, 300);
		std::vector<uint8> Engines(2 * FGearPartTables::EngineSize, 0);
		LittleEndian::WriteU16(Engines, 0x10 + 6, 1200);
		std::vector<uint8> Parts(3 * FGearPartTables::PartSize, 0);
		LittleEndian::WriteU16(Parts, 0x1C + 6, 3000);
		Parts[2 * FGearPartTables::PartSize + 0x15] = 6;
		auto Tables = FGearPartTables::Make(std::move(Frames), std::move(Armors), std::move(Engines), std::move(Parts), {});
		REQUIRE(Tables);
		return std::move(*Tables);
	}

	void StoreName(std::vector<uint8>& Data, std::size_t Offset, std::initializer_list<uint16> Glyphs)
	{
		for (const uint16 Glyph : Glyphs)
		{
			LittleEndian::WriteU16(Data, Offset, Glyph);
			Offset += 2;
		}
	}

	/**
	 * A save whose regions hold a byte pattern, with Fei, a name of ten two-byte glyphs and a gear
	 * called "Fe"; gear 0 is the heavy build, with 99 HP, 5000 fuel (over its engine's maximum), armor
	 * break 12, attack multiplier 10 and no weight tolerance.
	 */
	std::vector<uint8> MakeSaveData()
	{
		std::vector<uint8> Data(FOriginalSave::DataSize, 0);
		for (std::size_t Offset = 0x290; Offset < 0x1B5C; ++Offset)
		{
			Data[Offset] = static_cast<uint8>(Offset * 7 + 1);
		}
		for (std::size_t Gear = 0; Gear < 20; ++Gear)
		{
			std::fill_n(Data.begin() + static_cast<std::ptrdiff_t>(SaveGears + Gear * SaveGearSize) + 2, 10, uint8{0});
		}
		std::fill_n(Data.begin() + 0x24, 0x100 - 0x24, uint8{0});
		std::fill_n(Data.begin() + 0x100, 0x190, uint8{0});
		StoreName(Data, 0x24, {1, 2, 3});
		StoreName(Data, 0x24 + 0x14, {4, 4, 4, 4, 4, 4, 4, 4, 4, 4});
		StoreName(Data, 0x100, {1, 2});
		const std::size_t Gear = SaveGears;
		Data[Gear + 2] = 1;
		Data[Gear + 3] = 1;
		Data[Gear + 8] = 1;
		Data[Gear + 9] = 1;
		LittleEndian::WriteU32(Data, Gear + 0x2C, 99);
		LittleEndian::WriteU16(Data, Gear + 0x34, 5000);
		Data[Gear + 0x38] = 12;
		Data[Gear + 0x39] = 10;
		Data[Gear + 0x3A] = 0;
		LittleEndian::WriteU32(Data, 0, 0x123456);
		// The battle globals travel in the state's region from 1920h, at 2324h.
		for (std::size_t Global = 0; Global < 16; ++Global)
		{
			LittleEndian::WriteU16(Data, 0x1124 + 0x2324 - 0x1920 + Global * 2, static_cast<uint16>(0x100 + Global));
		}
		return Data;
	}

	/** The state the game held before: every byte stale, a play time and globals of its own. */
	FGameState MakeCurrent()
	{
		FGameState State;
		std::fill(State.Block().begin(), State.Block().end(), Stale);
		State.SetPlayTime(7);
		State.BattleGlobals().fill(0x5555);
		return State;
	}

	TResult<FGameState> Load(const std::vector<uint8>& Data, const FSaveLoadOptions& Options = {})
	{
		return LoadOriginalSave(FOriginalSave::WithChecksum(Data), MakeCurrent(), MakeTables(), MakeNameTables(), Options);
	}

	bool SameBytes(std::span<const uint8> State, std::size_t StateOffset, const std::vector<uint8>& Data, std::size_t SaveOffset, std::size_t Size)
	{
		return std::equal(State.begin() + static_cast<std::ptrdiff_t>(StateOffset), State.begin() + static_cast<std::ptrdiff_t>(StateOffset + Size),
			Data.begin() + static_cast<std::ptrdiff_t>(SaveOffset));
	}

	std::vector<uint8> Bytes(std::span<const uint8> Span)
	{
		return {Span.begin(), Span.end()};
	}
}

TEST_CASE("A save's regions, play time, battle globals and names load over the current state", "[Game]")
{
	const std::vector<uint8> Data = MakeSaveData();
	const auto State = Load(Data);
	REQUIRE(State);
	const std::span<const uint8> Block = State->Block();
	CHECK(SameBytes(Block, 0x26C, Data, 0x290, 11 * 0xA4));
	CHECK(SameBytes(Block, 0x16C0, Data, 0xEC4, 4));
	CHECK(SameBytes(Block, 0x16C6, Data, 0xECA, 0x160 - 6));
	// Fei pilots every gear here, and their parts grant none of the Deathblows parts can.
	CHECK(LittleEndian::ReadU16(Block, 0x16C4) == (LittleEndian::ReadU16(Data, 0xEC8) & 0xFB6F));
	CHECK(SameBytes(Block, 0x1648, Data, 0xE4C, 0x78));
	CHECK(SameBytes(Block, 0x1820, Data, 0x1024, 0x100));
	CHECK(SameBytes(Block, 0x1920, Data, 0x1124, 0xA38));
	CHECK(State->PlayTime() == 0x123456);
	for (std::size_t Global = 0; Global < 16; ++Global)
	{
		CHECK(State->BattleGlobals()[Global] == 0x100 + Global);
	}
	CHECK(Bytes(State->Name(0)) == std::vector<uint8>{0x25, 0x41, 0x45});
	CHECK(State->Name(1).size() == FGameState::NameBytes);
	CHECK(State->Name(1)[18] == 0xFE);
	CHECK(Bytes(State->Name(11)) == std::vector<uint8>{0x25, 0x41});
	CHECK(State->Name(2).empty());
	CHECK(Block[3] == 0);
}

TEST_CASE("A loaded gear is recomputed from its parts under its saved fuel, HP and fields", "[Game]")
{
	const auto State = Load(MakeSaveData());
	REQUIRE(State);
	const std::vector<uint8> Data = MakeSaveData();
	const std::span<const uint8> Gear = State->Block().subspan(StateGears, StateGearSize);
	CHECK(SameBytes(Gear, 0, Data, SaveGears, 0x28));
	CHECK(SameBytes(Gear, 0x5C, Data, SaveGears + 0x28, 4));
	// Saved: HP, fuel over its maximum, armor break, attack multiplier, weight tolerance.
	CHECK(LittleEndian::ReadU32(Gear, 0x60) == 99);
	CHECK(LittleEndian::ReadU16(Gear, 0x38) == 5000);
	CHECK(Gear[0x99] == 12);
	CHECK(Gear[0x74] == 10);
	CHECK(Gear[0x75] == 0);
	// Derived from the parts.
	CHECK(LittleEndian::ReadU32(Gear, 0x64) == 1800);
	CHECK(LittleEndian::ReadU16(Gear, 0x3A) == 1200);
	CHECK(LittleEndian::ReadU16(Gear, 0x70) == 300);
	CHECK(LittleEndian::ReadU16(Gear, 0x44) == 3000);
	// The penalty follows the saved tolerance: (3000 / 120 - 0) / 2.
	CHECK(Gear[0x4A] == 12);
	// Neither saved nor derived: as the state held them.
	CHECK(Gear[0x28] == Stale);
	CHECK(Gear[0x68] == Stale);
	CHECK(Gear[0xA3] == Stale);
	CHECK(LittleEndian::ReadU16(Gear, 0x86) == 0xE000);
}

TEST_CASE("The original load takes a gear's penalty from the tolerance the state held", "[Game]")
{
	const auto State = Load(MakeSaveData(), FSaveLoadOptions::Original());
	REQUIRE(State);
	const std::span<const uint8> Gear = State->Block().subspan(StateGears, StateGearSize);
	// Tolerance EEh when the penalty was computed, 0 once restored: the penalty stays 0.
	CHECK(Gear[0x75] == 0);
	CHECK(Gear[0x4A] == 0);
}

TEST_CASE("A pilot keeps the gear Deathblows of the gear in use", "[Game]")
{
	constexpr std::size_t FeiGearInUse = 0x290 + 0xA0;
	constexpr std::size_t FeiGearDeathblows = 0xEC4 + 4;
	std::vector<uint8> Data = MakeSaveData();
	// Fei uses gear 0, whose first part grants the tier's Deathblow; gears 1-19 have none of it.
	Data[FeiGearInUse] = 0;
	Data[SaveGears + 9] = 2;
	LittleEndian::WriteU16(Data, FeiGearDeathblows, 0x1800);
	const auto Fixed = Load(Data);
	const auto Original = Load(Data, FSaveLoadOptions::Original());
	REQUIRE(Fixed);
	REQUIRE(Original);
	CHECK(LittleEndian::ReadU16(Fixed->Block(), 0x16C4) == 0x1C00);
	// The original ends on gear 19, which grants nothing.
	CHECK(LittleEndian::ReadU16(Original->Block(), 0x16C4) == 0x1800);
}

TEST_CASE("A save naming a part or a glyph the game lacks is refused", "[Game]")
{
	std::vector<uint8> Data = MakeSaveData();
	Data[SaveGears + 3 * SaveGearSize + 0xA] = 3;
	CHECK_FALSE(Load(Data));
	Data = MakeSaveData();
	LittleEndian::WriteU16(Data, 0x24 + 0x14 * 3, 5);
	CHECK_FALSE(Load(Data));
}

TEST_CASE("A state saved and loaded back is the same where the save keeps it", "[Game]")
{
	const FMesNameTables NameTables = MakeNameTables();
	auto First = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(First);
	FGameState Original = *First;
	CHECK(Original.Block()[StateGears + 0x4A] == 12);
	Original.SetPlayTime(0xABCDEF);
	Original.BattleGlobals()[3] = 0x7777;
	const FOriginalSave Save = WriteOriginalSave(Original, 4, 1, NameTables, 0xFE);
	const auto Loaded = LoadOriginalSave(Save, Original, MakeTables(), NameTables);
	REQUIRE(Loaded);
	CHECK(Bytes(Loaded->Block()) == Bytes(Original.Block()));
	CHECK(Loaded->PlayTime() == 0xABCDEF);
	CHECK(Loaded->BattleGlobals() == Original.BattleGlobals());
	CHECK(Loaded->SavedDisc() == 1);
}

TEST_CASE("A save keeps the second disc's story mode in bytes the game leaves zero, and any other save has none chosen", "[Game]")
{
	constexpr std::size_t PortBytes = 0x1B5C;
	constexpr std::size_t StoryModeByte = 0x1B60;
	constexpr std::size_t Checksum = FOriginalSave::DataSize - 1;
	const FMesNameTables NameTables = MakeNameTables();
	// A save the game made has none, whatever the game held before the load.
	FGameState Current = MakeCurrent();
	Current.SetStoryMode(EStoryMode::Expanded);
	const auto Plain = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), Current, MakeTables(), NameTables);
	REQUIRE(Plain);
	CHECK(Plain->StoryMode() == EStoryMode::Unchosen);

	// Saved with none chosen, the bytes are zeros, as the game writes them.
	FGameState State = *Plain;
	const FOriginalSave Unchosen = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	const bool bZeros = std::all_of(Unchosen.Data().begin() + PortBytes, Unchosen.Data().begin() + Checksum, [](uint8 Byte) { return Byte == 0; });
	CHECK(bZeros);
	CHECK(Unchosen.StoryMode() == EStoryMode::Unchosen);

	for (const EStoryMode Mode : {EStoryMode::Original, EStoryMode::Expanded})
	{
		State.SetStoryMode(Mode);
		const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
		CHECK(Save.StoryMode() == Mode);
		// Still a save the game reads: nothing before those bytes differs, and the checksum holds over them.
		const bool bSameBefore = std::equal(Save.Data().begin(), Save.Data().begin() + PortBytes, Unchosen.Data().begin());
		CHECK(bSameBefore);
		CHECK(FOriginalSave::FromData(Save.Data()));
		const auto Loaded = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables);
		REQUIRE(Loaded);
		CHECK(Loaded->StoryMode() == Mode);
		const bool bSameState = Bytes(Loaded->Block()) == Bytes(State.Block());
		CHECK(bSameState);
	}

	// A mode's byte without the port's mark before it, and a marked value that is no mode, are none chosen.
	std::vector<uint8> Unmarked = MakeSaveData();
	Unmarked[StoryModeByte] = static_cast<uint8>(EStoryMode::Expanded);
	CHECK(FOriginalSave::WithChecksum(Unmarked).StoryMode() == EStoryMode::Unchosen);
	std::vector<uint8> NoMode(Unchosen.Data().begin(), Unchosen.Data().end());
	State.SetStoryMode(EStoryMode::Expanded);
	const FOriginalSave Marked = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	NoMode.assign(Marked.Data().begin(), Marked.Data().end());
	NoMode[StoryModeByte] = 7;
	CHECK(FOriginalSave::WithChecksum(NoMode).StoryMode() == EStoryMode::Unchosen);
}

TEST_CASE("A save of the expanded second disc where the original stands elsewhere is the original's save there, and the game's own in the port", "[Game]")
{
	constexpr std::size_t PortBytes = 0x1B5C;
	constexpr std::size_t KeptRuns = 0x1B61;
	constexpr std::size_t Checksum = FOriginalSave::DataSize - 1;
	const FMesNameTables NameTables = MakeNameTables();
	auto First = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(First);
	// A game on a map of the expansion's own, the story at a value of its own; the original goes on at the chair.
	FGameState State = *First;
	State.SetStoryMode(EStoryMode::Expanded);
	State.SetPlace(735);
	State.SetFieldEntrance(3);
	State.SetSavedEventVariable(1, 3);
	State.SetSavedEventVariable(0, 253);
	const FWayBackPoint Chair{617, 2, 252};
	FGameState Plainly = State;
	const FOriginalSave Plain = WriteOriginalSave(Plainly, 4, 1, NameTables, 0xFE);
	const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE, &Chair);
	CHECK(FOriginalSave::FromData(Save.Data()));
	CHECK(Save.StoryMode() == EStoryMode::Expanded);
	CHECK(Plain.Data()[KeptRuns] == 0);

	// The game reads it as a save made at the chair: the bytes it reads are those of the same game put there.
	FGameState AtChair = State;
	Chair.Apply(AtChair);
	std::vector<uint8> AsTheGameReads = Bytes(Save.Data());
	std::fill(AsTheGameReads.begin() + PortBytes, AsTheGameReads.begin() + Checksum, uint8{0});
	const auto Original = LoadOriginalSave(FOriginalSave::WithChecksum(AsTheGameReads), AtChair, MakeTables(), NameTables);
	REQUIRE(Original);
	CHECK(Bytes(Original->Block()) == Bytes(AtChair.Block()));
	CHECK(Original->Place() == 617);
	CHECK(Original->SavedEventVariable(0) == 252);

	// The port plays it where it was made.
	const bool bPlayedIsTheGames = std::equal(Plain.Data().begin(), Plain.Data().begin() + PortBytes, Save.PlayedData().begin());
	CHECK(bPlayedIsTheGames);
	CHECK(Save.StoryProgress() == 253);
	const auto Played = LoadOriginalSave(Save, State, MakeTables(), NameTables);
	REQUIRE(Played);
	CHECK(Bytes(Played->Block()) == Bytes(State.Block()));
	CHECK(Played->Place() == 735);
	CHECK(Played->StoryMode() == EStoryMode::Expanded);

	// Runs that reach past the game's bytes are none the port wrote: the save plays as the game reads it.
	std::vector<uint8> Reaching = Bytes(Save.Data());
	Reaching[KeptRuns + 1] = 0x50;
	Reaching[KeptRuns + 2] = 0x1B;
	Reaching[KeptRuns + 3] = 0x10;
	const FOriginalSave Foreign = FOriginalSave::WithChecksum(Reaching);
	CHECK(Foreign.PlayedData() == Bytes(Foreign.Data()));
}

TEST_CASE("A save of the expanded second disc keeps its expansion's folder, and loads without it as the original's at its point", "[Game]")
{
	constexpr std::size_t ExpansionName = 0x1EB5;
	const FMesNameTables NameTables = MakeNameTables();
	auto First = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(First);
	FGameState State = *First;
	State.SetStoryMode(EStoryMode::Expanded);
	State.SetExpansion("Elly's errand");
	State.SetPlace(735);
	State.SetSavedEventVariable(0, 253);
	const FWayBackPoint Chair{617, 2, 252};
	const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE, &Chair);
	CHECK(FOriginalSave::FromData(Save.Data()));
	CHECK(Save.Expansion() == "Elly's errand");
	const auto Played = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(Played);
	CHECK(Played->Expansion() == "Elly's errand");
	CHECK(Played->Place() == 735);

	// Without its expansion the port plays what the original reads: the chair, the original's second disc.
	const auto Without = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables, {}, false);
	REQUIRE(Without);
	CHECK(Without->StoryMode() == EStoryMode::Original);
	CHECK(Without->Expansion().empty());
	CHECK(Without->Place() == 617);
	CHECK(Without->SavedEventVariable(0) == 252);
	// And a game of either other kind loads as it does with it.
	FGameState Original = State;
	Original.SetStoryMode(EStoryMode::Original);
	const FOriginalSave OriginalSave = WriteOriginalSave(Original, 4, 1, NameTables, 0xFE);
	CHECK(OriginalSave.Expansion().empty());
	const auto OriginalWithout = LoadOriginalSave(OriginalSave, MakeCurrent(), MakeTables(), NameTables, {}, false);
	REQUIRE(OriginalWithout);
	CHECK(OriginalWithout->StoryMode() == EStoryMode::Original);
	CHECK(OriginalWithout->Place() == 735);

	// A save made before saves kept the expansion's folder has zeros there: the examples'.
	std::vector<uint8> Unnamed = Bytes(Save.Data());
	std::fill(Unnamed.begin() + ExpansionName, Unnamed.begin() + ExpansionName + ExpansionFolderBytes, uint8{0});
	CHECK(FOriginalSave::WithChecksum(Unnamed).Expansion() == ExamplesFolder);
	// A folder's name may fill the bytes kept for it, with no zero after it.
	State.SetExpansion(std::string(ExpansionFolderBytes, 'L'));
	const FOriginalSave Long = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	CHECK(Long.Expansion() == std::string(ExpansionFolderBytes, 'L'));
}

TEST_CASE("The new game saved and loaded back matches the Menu's own", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FGearPartTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	const auto NewGame = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(NewGame);
	FGameState Saved = *NewGame;
	const FOriginalSave Save = WriteOriginalSave(Saved, 0, 0, Resident.NameTables, Resident.Font.LeadByte());
	// The Menu's own writer and loader under the oracle harness, from the kernel's new game.
	CHECK(FSha1::ToHex(FSha1::Hash(Save.Data())) == "e4a8bd451111f4c7d135ebd4c6e6f3e34946b205");
	const auto Loaded = LoadOriginalSave(Save, *NewGame, *Tables, Resident.NameTables, FSaveLoadOptions::Original());
	REQUIRE(Loaded);
	FSha1 Digest;
	Digest.Update(Loaded->Block());
	std::array<uint8, 4 + 32> Kernel{};
	LittleEndian::WriteU32(Kernel, 0, Loaded->PlayTime());
	for (std::size_t Global = 0; Global < 16; ++Global)
	{
		LittleEndian::WriteU16(Kernel, 4 + Global * 2, Loaded->BattleGlobals()[Global]);
	}
	Digest.Update(Kernel);
	CHECK(FSha1::ToHex(Digest.Finish()) == "acfedd4d41e8cf8d288eee6edddff6db93d48b76");
	CHECK(Loaded->Party() == NewGame->Party());
	CHECK(Loaded->SavedDisc() == 0);
	// The new-game file's place is the title screen's map; choosing New Game there moves it to map 4.
	CHECK(Loaded->Place() == 0x1EA);
	// The unnamed gear's name is glyph 0, which a save cannot keep.
	CHECK(Loaded->Name(30).empty());
	CHECK(Saved.Name(30).empty());
}

TEST_CASE("A save loads back how its player had Citan and Emeralda drawn, and a game's own save loads them as the story has them", "[Game]")
{
	const FMesNameTables NameTables = MakeNameTables();
	FGameState Current = MakeCurrent();
	FCharacterLooks Switched;
	Switched.Switch(9);
	Current.SetCharacterLooks(Switched);
	const auto Plain = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), Current, MakeTables(), NameTables);
	REQUIRE(Plain);
	CHECK_FALSE(Plain->CharacterLooks().IsSet());

	FGameState State = *Plain;
	State.SetCharacterLooks(Switched);
	const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	const auto Loaded = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(Loaded);
	CHECK(Loaded->CharacterLooks() == Switched);
	CHECK(Loaded->CharacterLooks().Drawn(9) == 2);
	const bool bSameState = Bytes(Loaded->Block()) == Bytes(State.Block());
	CHECK(bSameState);
}

TEST_CASE("A clear game's save is the original's with Elly away, and the port loads her back in the party in Regurus", "[Game]")
{
	constexpr std::size_t PortBytes = 0x1B5C;
	constexpr std::size_t Checksum = FOriginalSave::DataSize - 1;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 Bart = 3;
	constexpr uint16 EllyBit = 1 << Elly;
	const FMesNameTables NameTables = MakeNameTables();
	auto First = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(First);
	FGameState State = *First;
	State.SetJoinedCharacters(static_cast<uint16>(0x7FF & ~EllyBit));
	State.SetMenuCharacters(static_cast<uint16>(0x7FF & ~EllyBit));
	ClearFile::Make(State);
	// The player took Bart along, riding, Elly between them.
	State.SetParty({Fei, Elly, Bart});
	State.SetPartySlotInGear(2, 1);
	const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	CHECK(FOriginalSave::FromData(Save.Data()));
	CHECK(Save.ClearGame() == EClearGame::ClearFile);

	// The game reads the same game with Elly away: Citan, the first free, in her place, Bart riding after him, Elly not
	// joined and in Vierge.
	FGameState Away = State;
	ClearFile::HideElly(Away);
	CHECK(Away.Party() == std::array<uint8, 3>{Fei, Citan, Bart});
	CHECK(Away.PartySlotInGear(1) == 0);
	CHECK(Away.PartySlotInGear(2) == 1);
	CHECK((Away.JoinedCharacters() & EllyBit) == 0);
	CHECK(Away.CharacterGear(Elly) == 2);
	std::vector<uint8> AsTheGameReads = Bytes(Save.Data());
	std::fill(AsTheGameReads.begin() + PortBytes, AsTheGameReads.begin() + Checksum, uint8{0});
	const auto Original = LoadOriginalSave(FOriginalSave::WithChecksum(AsTheGameReads), Away, MakeTables(), NameTables);
	REQUIRE(Original);
	CHECK(Bytes(Original->Block()) == Bytes(Away.Block()));
	CHECK(Save.Party() == std::array<uint8, 3>{Fei, Citan, Bart});

	// The port plays the game as it was saved, a clear game again.
	const auto Played = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(Played);
	CHECK(Bytes(Played->Block()) == Bytes(State.Block()));
	CHECK(Played->ClearGame() == EClearGame::ClearFile);
	CHECK(Played->Party() == std::array<uint8, 3>{Fei, Elly, Bart});
	CHECK((Played->JoinedCharacters() & EllyBit) != 0);
	CHECK(Played->CharacterGear(Elly) == 10);

	// Saved again, the same; and a game that is not a clear game keeps no mark.
	FGameState Again = *Played;
	CHECK(WriteOriginalSave(Again, 4, 1, NameTables, 0xFE).ClearGame() == EClearGame::ClearFile);
	FGameState Plain = *First;
	CHECK(WriteOriginalSave(Plain, 4, 1, NameTables, 0xFE).ClearGame() == EClearGame::None);
}

TEST_CASE("A Keep Elly game's save is the original's with Elly away from her return on, and the port loads her back", "[Game]")
{
	constexpr std::size_t PortBytes = 0x1B5C;
	constexpr std::size_t Checksum = FOriginalSave::DataSize - 1;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Bart = 3;
	constexpr uint8 Citan = 9;
	constexpr uint16 EllyBit = 1 << Elly;
	constexpr uint8 Regurus = 10;
	const FMesNameTables NameTables = MakeNameTables();
	auto First = LoadOriginalSave(FOriginalSave::WithChecksum(MakeSaveData()), MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(First);

	// Before her return the save is the game's own, marked.
	FGameState Before = *First;
	Before.SetClearGame(EClearGame::KeepElly);
	Before.SetSavedEventVariable(0, KeepElly::ReturnStory - 1);
	const FOriginalSave Early = WriteOriginalSave(Before, 4, 1, NameTables, 0xFE);
	CHECK(Early.ClearGame() == EClearGame::KeepElly);
	std::vector<uint8> EarlyAsTheGameReads = Bytes(Early.Data());
	std::fill(EarlyAsTheGameReads.begin() + PortBytes, EarlyAsTheGameReads.begin() + Checksum, uint8{0});
	const auto EarlyOriginal = LoadOriginalSave(FOriginalSave::WithChecksum(EarlyAsTheGameReads), Before, MakeTables(), NameTables);
	REQUIRE(EarlyOriginal);
	CHECK(Bytes(EarlyOriginal->Block()) == Bytes(Before.Block()));

	// In Merkava, the player brought her in beside Fei and Citan.
	FGameState State = *First;
	State.SetClearGame(EClearGame::KeepElly);
	State.SetSavedEventVariable(0, KeepElly::ReturnStory);
	State.SetJoinedCharacters(static_cast<uint16>((1 << Fei) | (1 << Bart) | (1 << Citan)));
	State.SetMenuCharacters(0x7FF);
	State.SetParty({Fei, Citan, Bart});
	KeepElly::Hold(State);
	State.SetParty({Fei, Elly, Citan});
	const FOriginalSave Save = WriteOriginalSave(State, 4, 1, NameTables, 0xFE);
	CHECK(Save.ClearGame() == EClearGame::KeepElly);

	// The game reads it with Elly away: Bart, the first free, in her place, she not joined and in Vierge.
	FGameState Away = State;
	ClearFile::HideElly(Away);
	CHECK(Away.Party() == std::array<uint8, 3>{Fei, Bart, Citan});
	CHECK((Away.JoinedCharacters() & EllyBit) == 0);
	CHECK(Away.CharacterGear(Elly) == 2);
	std::vector<uint8> AsTheGameReads = Bytes(Save.Data());
	std::fill(AsTheGameReads.begin() + PortBytes, AsTheGameReads.begin() + Checksum, uint8{0});
	const auto Original = LoadOriginalSave(FOriginalSave::WithChecksum(AsTheGameReads), Away, MakeTables(), NameTables);
	REQUIRE(Original);
	CHECK(Bytes(Original->Block()) == Bytes(Away.Block()));

	// The port plays it as it was saved: Elly in the party, joined, in Regurus, the game Keep Elly's.
	const auto Played = LoadOriginalSave(Save, MakeCurrent(), MakeTables(), NameTables);
	REQUIRE(Played);
	CHECK(Bytes(Played->Block()) == Bytes(State.Block()));
	CHECK(Played->ClearGame() == EClearGame::KeepElly);
	CHECK(Played->Party() == std::array<uint8, 3>{Fei, Elly, Citan});
	CHECK(Played->CharacterGear(Elly) == Regurus);
}
