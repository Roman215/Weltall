#include "Battle/BattleEndingState.h"
#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattlePresentation.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameBalance.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Game/PlayerSettings.h"
#include "Game/WorldMapProgram.h"
#include "Sound/GameSoundSession.h"
#include "Sound/GameSoundSystem.h"
#include "Sound/ResidentSound.h"
#include "Sound/SoundDriver.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/StorySweepStrength.h"
#include "WorldMap/WorldMapContext.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

// The Battle program as the game's loop runs it: started from what the field or the world map hands the kernel, fought to
// its end, and handing the kernel what picks the program after it.

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	/** Map 15: a forest whose random encounters the program's oracle fights; its formation 1. */
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	/** Map 57's formation 0: four of Aveh's soldiers, whose records give 35 gold each, and a box that stays hidden. */
	constexpr uint16 SoldiersMap = 57;
	constexpr uint8 SoldiersFormation = 0;
	constexpr uint32 SoldiersGold = 4 * 35;
	constexpr uint16 TitleMap = 490;
	/** The game state's characters' records, their status word's knocked-out bit and their EXP to the next level. */
	constexpr std::size_t CharactersAt = 0x26C;
	constexpr std::size_t CharacterBytes = 0xA4;
	constexpr std::size_t StatusHighByte = 0x7D;
	constexpr uint8 KnockedOut = 0x80;
	constexpr std::size_t ExpToNextAt = 0x44;
	/** The gear battles' bank, which every battle gives back to the kernel where the boot put it, and the battle's common effects. */
	constexpr int32 GearBattleBankId = 2;
	constexpr uint32 GearBattleRoom = 0x68000;
	constexpr uint16 CommonSoundsId = 1;

	/** The EXP a character still needs for its next level, which a won battle's rewards lower. */
	uint32 ExpToNextLevel(const FGameState& State, std::size_t Character)
	{
		const std::size_t At = CharactersAt + Character * CharacterBytes + ExpToNextAt;
		const auto Block = State.Block();
		return static_cast<uint32>(Block[At] | Block[At + 1] << 8 | Block[At + 2] << 16 | Block[At + 3] << 24);
	}

	std::string Stopped(const Testing::FGameLoopSession& Game)
	{
		return Game.Loop->StopReason() ? Game.Loop->StopReason()->Message : std::string();
	}

	/** Enters Map as a new game with a party of three, past the map's entry into the field's loop. */
	void EnterMap(Testing::FGameLoopSession& Game, uint16 Map)
	{
		FGameState& State = Game.Session->Game();
		State.SetParty({0, 1, 2});
		State.SetJoinedCharacters(0x1FF);
		Game.EnterField(Map);
		FFieldProgram* Field = Game.Loop->Field();
		REQUIRE(Field != nullptr);
		Game.RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		Game.Frame();
	}

	void EnterForest(Testing::FGameLoopSession& Game)
	{
		EnterMap(Game, ForestMap);
	}

	/** Asks the field for a battle of the map's formation as a script asks for one, and runs the loop into the Battle program. */
	void StartBattle(Testing::FGameLoopSession& Game, uint8 Formation)
	{
		FEventContext& Context = Game.Loop->Field()->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = Formation;
		Game.RunUntil([&] { return Game.Loop->Program() == EGameProgram::Battle; }, 8);
		INFO(Stopped(Game));
		REQUIRE(Game.Loop->Stage() == EGameLoopStage::Program);
		REQUIRE(Game.Loop->Battle() != nullptr);
	}

	/** Fights the battle, pressing Cross now and then (the menus' confirm, the attacks' press) when asked, until the kernel runs the next program. */
	void Fight(Testing::FGameLoopSession& Game, bool bPressing)
	{
		const int32 Runs = Game.Loop->ProgramRuns();
		for (int32 Frame = 0; Frame < 3000 && Game.Loop->ProgramRuns() == Runs; ++Frame)
		{
			const bool bPressed = bPressing && Frame % 24 < 2;
			INFO("frame " << Frame);
			REQUIRE_FALSE(Game.Frame(bPressed ? CrossHeld : NoButtons));
		}
		INFO(Stopped(Game));
		REQUIRE(Game.Loop->ProgramRuns() != Runs);
		REQUIRE(Game.Loop->Stage() == EGameLoopStage::Program);
	}

	/** The sound as a battle leaves it: the gear battles' bank back as the kernel's, none of the battle's effect sets held. */
	void CheckSoundAfterBattle(const Testing::FGameLoopSession& Game)
	{
		const FResidentSound& Resident = Game.Session->Sound().Resident();
		const FLockedGameSound Locked = Game.Session->Sound().Sound()->Lock();
		REQUIRE(Resident.GearBattleBank != nullptr);
		CHECK(Resident.GearBattleBank == Locked->FindBank(GearBattleBankId));
		CHECK(FSoundDriver::BankAddress(*Resident.GearBattleBank) == GearBattleRoom);
		CHECK(Locked->FindEffects(CommonSoundsId) == nullptr);
	}
}

TEST_CASE("The battle's end hands the kernel what its event asked for, then the program after it", "[Game]")
{
	FGameState Game;
	Game.SetWorldMapPlace(0x0010, 0, 0, 0);

	FKernelState Kernel;
	FBattleOutcome Won;
	Won.Result = FKernelState::BattleWon;
	EndBattleProgram(Won, Kernel, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Field);
	CHECK(Kernel.bBattleDebug);
	CHECK_FALSE(Kernel.bEventFadeIn);

	// The event's chained battle and its kind: the Battle program again, without the battle debug set.
	Kernel = FKernelState{};
	FBattleOutcome Chained = Won;
	Chained.Chain = FBattleChain{0x0B, 3};
	EndBattleProgram(Chained, Kernel, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Battle);
	CHECK(Kernel.ChainedBattle == 0x0B);
	CHECK(Kernel.BattleKind == 3);
	CHECK_FALSE(Kernel.bBattleDebug);

	// The event's movie, then the field; the movie comes before a chained battle.
	Kernel = FKernelState{};
	FBattleOutcome Movie = Chained;
	Movie.Movie = FBattleMovieRequest{0x81, 12, 1, 700};
	EndBattleProgram(Movie, Kernel, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Movie);
	CHECK(Kernel.Movie.Type == 0x81);
	CHECK(Kernel.Movie.Number == 12);
	CHECK(Kernel.Movie.NextProgram == EGameProgram::Field);
	CHECK(Kernel.Movie.NoSkip == 1);
	CHECK(Kernel.Movie.EndFrame == 700);

	// The field starting afresh, and its return fade.
	Kernel = FKernelState{};
	Kernel.FieldTrips = 1;
	FBattleOutcome Fresh = Won;
	Fresh.bFieldStartsFresh = true;
	Fresh.FieldReturn = 1;
	EndBattleProgram(Fresh, Kernel, Game);
	CHECK(Kernel.FieldTrips == 0);
	CHECK(Kernel.bEventFadeIn);
	Fresh.FieldReturn = 2;
	EndBattleProgram(Fresh, Kernel, Game);
	CHECK_FALSE(Kernel.bEventFadeIn);

	// The result screen's fanfare leaves its mark for the next chained battle, however many battles later.
	Kernel = FKernelState{};
	FBattleOutcome Fanfare = Won;
	Fanfare.bFanfarePlayed = true;
	EndBattleProgram(Fanfare, Kernel, Game);
	CHECK(Kernel.bResultFanfare);
	EndBattleProgram(Won, Kernel, Game);
	CHECK(Kernel.bResultFanfare);

	// Lost: the title's map, the field starting afresh.
	Kernel = FKernelState{};
	Kernel.FieldTrips = 1;
	FBattleOutcome Lost;
	Lost.Result = FKernelState::BattleLost;
	EndBattleProgram(Lost, Kernel, Game);
	CHECK(Kernel.NextProgram == EGameProgram::Field);
	CHECK(Kernel.FieldTrips == 0);
	CHECK(Game.Place() == FKernelState::TitleMap);
}

TEST_CASE("The field's battle is fought in the Battle program and the field comes back after it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterForest(*Game);
	// The gears' lock the field last worked out goes to the battle as the kernel keeps it.
	Game->Loop->Field()->Runtime().Context().bGearsLocked = true;
	// The light matrix the field's drawing left in the GTE lights the battle's models that load none: the forest's
	// frames load none either, so the one an earlier program left stays.
	FGteMatrix Light;
	Light.M = {{{0x123, -0x456, 0x789}, {1, 2, 3}, {-4, -5, -6}}};
	Game->Loop->Field()->Runtime().SetLightLeft(Light);
	FGameState& State = Game->Session->Game();
	const uint32 ExpToNext = ExpToNextLevel(State, 0);
	StartBattle(*Game, ForestFormation);
	const FBattleGameProgram& Battle = *Game->Loop->Battle();
	CHECK(Game->Session->Kernel().LightMatrix == Light);
	CHECK(Battle.Presentation().Show.Gte.LightMatrix == Light);
	const auto Record = Game->Session->Kernel().Formation(ForestFormation);
	REQUIRE(Record);
	CHECK(Battle.Runtime().Context().Formation.Scene == FBattleFormation::FromRecord(*Record).Scene);
	CHECK(Battle.Runtime().Context().bGearsLocked);
	CHECK(Battle.Runtime().IsInTransition());
	CHECK(Game->Session->Kernel().Music == EKernelMusic::Field);

	Fight(*Game, true);
	REQUIRE(Game->Loop->Program() == EGameProgram::Field);
	const FKernelState& Kernel = Game->Session->Kernel();
	CHECK(State.ProgramResult == 0);
	CHECK(ExpToNextLevel(State, 0) < ExpToNext);
	CHECK(Kernel.bBattleDebug);
	CHECK(Kernel.FieldTrips == 0);
	CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == ForestMap);
	// The map's load has left the GTE its lights under the reset camera: none.
	CHECK(Game->Loop->Field()->Runtime().LightLeft() == FGteMatrix{});
	CheckSoundAfterBattle(*Game);
	Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 200);
}

TEST_CASE("A won battle's gold goes into the purse at the player's rate and stays there as the field comes back", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint32 Purse = 1000;
	const auto GoldWon = [&](ERewardRate Rate)
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Balance.Gold = Rate;
		Game->Session->SetSettings(Settings);
		FGameState& State = Game->Session->Game();
		EnterMap(*Game, SoldiersMap);
		Testing::StrengthenParty(State);
		State.SetGold(Purse);
		StartBattle(*Game, SoldiersFormation);
		Fight(*Game, true);
		REQUIRE(Game->Loop->Program() == EGameProgram::Field);
		CHECK(State.ProgramResult == 0);
		Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 200);
		return State.Gold() - Purse;
	};
	CHECK(GoldWon(ERewardRate::Original) == SoldiersGold);
	CHECK(GoldWon(ERewardRate::Quadruple) == 4 * SoldiersGold);
}

TEST_CASE("A party knocked out before the battle loses it at once, and the field starts again on the title's map", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterForest(*Game);
	FGameState& State = Game->Session->Game();
	for (uint8 Character = 0; Character < 3; ++Character)
	{
		State.SetCharacterPoints(Character, FGameState::ECharacterPoints::Hp, 0);
		State.Block()[CharactersAt + Character * CharacterBytes + StatusHighByte] |= KnockedOut;
	}
	StartBattle(*Game, ForestFormation);
	Fight(*Game, false);
	REQUIRE(Game->Loop->Program() == EGameProgram::Field);
	CHECK(State.ProgramResult == 1);
	CHECK(State.Place() == TitleMap);
	CHECK(Game->Session->Kernel().FieldTrips == 0);
	CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == TitleMap);
	CheckSoundAfterBattle(*Game);
}

TEST_CASE("The world map's battle is fought in the Battle program and the world map comes back from what it kept", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	EnterForest(*Game);
	FGameState& State = Game->Session->Game();
	// Era 1 at the first scene's first entrance, as the field's scripts leave it for the world map.
	State.SetSavedEventVariable(0, 30);
	State.SetWorldMapPlace(0x401, 1, 0xC00, 1);
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bWorldMap = true;
	Context.bFrameVariablesFrozen = true;
	// A lock the field left before the world map, which the world map works out again for the battle.
	Context.bGearsLocked = true;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::WorldMap; }, 8);
	FWorldMapProgram* WorldMap = Game->Loop->WorldMap();
	REQUIRE(WorldMap != nullptr);
	CHECK(Game->Session->Kernel().Field.bGearsLocked);
	// The world map's start loads its place models' light from above, which a battle begun there is left with.
	CHECK(Game->Session->Kernel().LightMatrix.M == std::array<std::array<int16, 3>, 3>{{{0, -4096, 0}, {0, 0, 0}, {0, 0, 0}}});
	// Past the entry's fade and the scene's own fade-in, the period's encounter falls on the next tick the party moves.
	Game->RunUntil([&] { return !WorldMap->IsFadingOut(); }, 100);
	for (int32 Tick = 0; Tick < 64; ++Tick)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	FWorldMapEncounters& Encounters = WorldMap->Runtime().Context().Encounters;
	Encounters.Timers[0] = 1;
	Encounters.Left = FWorldMapEncounters::FootPeriod;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 16, FPadState{FPadState::Bit(EPadButton::Down)});
	REQUIRE(Game->Loop->Battle() != nullptr);
	CHECK(Game->Session->Kernel().Music == EKernelMusic::WorldMap);
	CHECK_FALSE(Game->Loop->Battle()->Runtime().Context().bGearsLocked);

	Fight(*Game, true);
	REQUIRE(Game->Loop->Program() == EGameProgram::WorldMap);
	CHECK(State.ProgramResult == 0);
	CHECK(Game->Loop->WorldMap()->Runtime().Context().ReturnWord == 2);
	CheckSoundAfterBattle(*Game);
}
