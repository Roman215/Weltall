#include "Game/FieldProgram.h"
#include "Game/SaveState.h"
#include "Game/WorldMapProgram.h"
#include "Menu/MenuGameMenus.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>
#include <optional>
#include <vector>

using namespace Weltall;

// States saved where the player has the party, written and read back, then loaded into a freshly powered-on game.

namespace
{
	constexpr uint16 TavernMap = 9;
	constexpr uint16 OpeningMap = 4;
	constexpr int32 VillageMap = 35;
	constexpr uint16 OutsideLahan = 0x405;
	constexpr int32 SettleFrames = 200;
	constexpr int32 PlayOnFrames = 300;
	constexpr FPadState Nothing{};
	constexpr FPadState Left{FPadState::Bit(EPadButton::Left)};
	constexpr FPadState Up{FPadState::Bit(EPadButton::Up)};
	/** The USA kernel's menu button (the field's Triangle once it has swapped the face buttons). */
	constexpr FPadState MenuButton{FPadState::Bit(EPadButton::Square)};

	void Run(Testing::FGameLoopSession& Game, FPadState Held, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const std::optional<FError> Stopped = Game.Frame(Held);
			INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
			REQUIRE_FALSE(Stopped.has_value());
		}
	}

	std::optional<FSaveState> Capture(Testing::FGameLoopSession& Game)
	{
		TResult<std::optional<FSaveState>> Captured = CaptureSaveState(*Game.Loop, *Game.Session);
		if (!Captured)
		{
			FAIL(Captured.Error().Message);
		}
		return std::move(*Captured);
	}

	/** The state through its file, loaded into a fresh game on the same disc. */
	std::unique_ptr<Testing::FGameLoopSession> LoadFresh(const FIso9660& Disc, const FSaveState& State)
	{
		ESaveStateFault Fault = ESaveStateFault::NotAState;
		const std::optional<FSaveState> Read = ReadSaveState(WriteSaveState(State), Fault);
		REQUIRE(Read.has_value());
		auto Fresh = Testing::FGameLoopSession::Start(Disc);
		TResult<std::unique_ptr<FGameLoop>> Loaded = LoadSaveState(*Fresh->Session, *Read);
		if (!Loaded)
		{
			FAIL(Loaded.Error().Message);
		}
		Fresh->Loop = std::move(*Loaded);
		return Fresh;
	}

	std::array<int32, 3> LeaderOnField(const Testing::FGameLoopSession& Game)
	{
		const FEventContext& Context = Game.Loop->Field()->Runtime().Context();
		const FEventActorState& Leader = Context.States[Context.LeaderActor];
		return {Leader.Position[0], Leader.Position[1], Leader.Position[2]};
	}

	std::array<uint16, FGameState::SavedEventVariableCount> SavedVariables(const FGameState& Game)
	{
		std::array<uint16, FGameState::SavedEventVariableCount> Variables{};
		for (int32 Index = 0; Index < FGameState::SavedEventVariableCount; ++Index)
		{
			Variables[static_cast<std::size_t>(Index)] = Game.SavedEventVariable(Index);
		}
		return Variables;
	}
}

TEST_CASE("A state saved in Lahan's tavern loads into a fresh game where the party stood, and plays on", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(TavernMap);
	Run(*Game, Nothing, SettleFrames);
	Run(*Game, Left, 8);
	Run(*Game, Nothing, 4);
	const std::optional<FSaveState> State = Capture(*Game);
	REQUIRE(State.has_value());
	CHECK(State->Kernel.NextProgram == EGameProgram::Field);
	REQUIRE(State->Kernel.Field.Saved.has_value());
	const std::array<int32, 3> Stood = LeaderOnField(*Game);
	const FGameState& Was = Game->Session->Game();

	auto Fresh = LoadFresh(*Disc, *State);
	REQUIRE(Fresh->Loop->Field() != nullptr);
	CHECK(Fresh->Loop->Field()->Runtime().Context().MapNumber == TavernMap);
	CHECK(LeaderOnField(*Fresh) == Stood);
	const FGameState& Loaded = Fresh->Session->Game();
	CHECK(Loaded.Place() == Was.Place());
	CHECK(Loaded.Party() == Was.Party());
	CHECK(Loaded.Gold() == Was.Gold());
	CHECK(Loaded.PlayTime() == Was.PlayTime());
	CHECK(SavedVariables(Loaded) == SavedVariables(State->Game));
	CHECK(Fresh->Session->Clock().Seconds == Game->Session->Clock().Seconds);

	// Nobody moves the party while the pad rests; the game plays on, and a state can be saved there again.
	Run(*Fresh, Nothing, PlayOnFrames);
	REQUIRE(Fresh->Loop->Field() != nullptr);
	CHECK(Fresh->Loop->Field()->Runtime().Context().MapNumber == TavernMap);
	CHECK(LeaderOnField(*Fresh) == Stood);
	const std::optional<FSaveState> Again = Capture(*Fresh);
	REQUIRE(Again.has_value());
	CHECK(Again->Game.PlayTime() > State->Game.PlayTime());
}

TEST_CASE("A state saved on the world map loads into a fresh game where the party stood and faced, and plays on", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	Game->Frame();
	FEventContext& FieldContext = Field->Runtime().Context();
	FieldContext.Requests.bWorldMap = true;
	FieldContext.bFrameVariablesFrozen = true;
	Game->Session->Game().SetWorldMapPlace(OutsideLahan, 1, 0x800, 2);
	const auto Ready = [](const Testing::FGameLoopSession& Of)
	{
		const FWorldMapProgram* Map = Of.Loop->WorldMap();
		return Map != nullptr && Map->TicksMap() && Map->Runtime().CouldOpenMenu();
	};
	REQUIRE(Game->RunUntil([&] { return Ready(*Game); }, 600) < 600);
	Run(*Game, Up, 8);
	Run(*Game, Nothing, 1);
	const std::optional<FSaveState> State = Capture(*Game);
	REQUIRE(State.has_value());
	CHECK(State->Kernel.NextProgram == EGameProgram::WorldMap);
	CHECK_FALSE(State->Kernel.Field.Saved.has_value());
	const FWorldMapTask& Leader = Game->Loop->WorldMap()->Runtime().Context().Tasks[FWorldMapContext::LeaderSlot];
	const int32 StoodX = Leader.Position.X >> 12;
	const int32 StoodZ = Leader.Position.Z >> 12;
	const int16 Faced = Leader.Heading;

	auto Fresh = LoadFresh(*Disc, *State);
	REQUIRE(Fresh->RunUntil([&] { return Ready(*Fresh); }, 600) < 600);
	const FWorldMapTask& Placed = Fresh->Loop->WorldMap()->Runtime().Context().Tasks[FWorldMapContext::LeaderSlot];
	CHECK(Placed.Position.X >> 12 == StoodX);
	CHECK(Placed.Position.Z >> 12 == StoodZ);
	CHECK(Placed.Heading == Faced);
	CHECK(Fresh->Session->Game().Place() == Game->Session->Game().Place());

	Run(*Fresh, Nothing, PlayOnFrames);
	REQUIRE(Fresh->Loop->WorldMap() != nullptr);
	CHECK(Capture(*Fresh).has_value());
}

TEST_CASE("No state is saved through the opening's narration, nor while the menu is open", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opening = Testing::FGameLoopSession::Start(*Disc);
	Opening->EnterField(OpeningMap);
	for (int32 Frame = 0; Frame < SettleFrames; ++Frame)
	{
		INFO("frame " << Frame);
		CHECK_FALSE(Capture(*Opening).has_value());
		Run(*Opening, Nothing, 1);
	}

	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FMenuGameMenus Menus(*Game->Executable);
	Game->Session->SetMenus(&Menus);
	Game->EnterField(TavernMap);
	Run(*Game, Nothing, SettleFrames);
	REQUIRE(Capture(*Game).has_value());
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 60, MenuButton);
	CHECK_FALSE(Capture(*Game).has_value());
	Run(*Game, Nothing, 30);
	REQUIRE(Menus.Running() != nullptr);
	CHECK_FALSE(Capture(*Game).has_value());
}
