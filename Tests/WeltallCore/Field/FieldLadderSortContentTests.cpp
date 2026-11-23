#include "Field/EventPlacement.h"
#include "Field/FieldFrame.h"
#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/Sprite.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

using namespace Weltall;

namespace
{
	/** Nortune's streets with the party of the story's step 111: Fei, Citan and Rico. */
	constexpr uint16 Streets = 196;
	constexpr uint16 Story = 111;
	constexpr std::array<uint8, 3> Party{0, 2, 5};
	/** Halfway up the ladder against the house by the lit door: a floor of layer 1 that faces the camera's direction 0. */
	constexpr int32 LadderX = -470;
	constexpr int32 LadderZ = -45;
	constexpr uint16 LadderLayer = 1;
	/** The view's yaws face that direction about 0 (4096 a turn); an eighth is 512. */
	constexpr int32 Eighth = 0x200;
	constexpr int32 TurnUnits = 0x1000;
	constexpr float YawUnitsPerDegree = 4096.0f / 360.0f;
	/** A drawn character's blend key holds its actor above these bits. */
	constexpr uint32 ActorShift = 3;

	FFieldProgram& FieldOf(Testing::FGameLoopSession& Game)
	{
		FFieldProgram* Field = Game.Loop->Field();
		REQUIRE(Field != nullptr);
		return *Field;
	}

	/** How far the yaw is from facing the ladder head-on, either way. */
	int32 OffHeadOn(int32 Yaw)
	{
		const int32 Wrapped = ((Yaw % TurnUnits) + TurnUnits) % TurnUnits;
		return std::min(Wrapped, TurnUnits - Wrapped);
	}

	/** The nearest entry the leader's sprite is drawn at in the latest frame shown. */
	std::optional<int32> LeaderEntry(const FFieldProgram& Field)
	{
		const FFieldFrame* Frame = Field.LatestFrame();
		REQUIRE(Frame != nullptr);
		const uint32 Leader = Field.Runtime().Context().LeaderActor;
		std::optional<int32> Nearest;
		for (const FFieldCharacterQuad& Quad : Frame->Quads)
		{
			const FFieldCharacterView& Character = Frame->Characters[Quad.Character];
			if (!Character.bOnFloor && Character.BlendKey >> ActorShift == Leader)
			{
				Nearest = std::min(Nearest.value_or(Quad.Quad.OtIndex), Quad.Quad.OtIndex);
			}
		}
		return Nearest;
	}

	void Wait(Testing::FGameLoopSession& Game, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game.Frame());
		}
	}

	/** Frames until the game's camera has turned, and the view and the drawing have caught up. */
	void Settle(Testing::FGameLoopSession& Game)
	{
		Game.RunUntil([&Game] { return FieldOf(Game).Runtime().Context().Camera.TurnTicks == 0; }, 240);
		Wait(Game, 30);
	}

	/** R1 turns the game's camera an eighth. */
	void TurnCamera(Testing::FGameLoopSession& Game)
	{
		REQUIRE_FALSE(Game.Frame(FPadState{FPadState::Bit(EPadButton::R1)}));
		REQUIRE_FALSE(Game.Frame());
		Settle(Game);
	}

	/** The player's view turned as a stick turns it, until the view shown is turned Turn yaw units from the camera's. */
	void TurnView(Testing::FGameLoopSession& Game, int32 Turn)
	{
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			const int32 Left = Turn - FieldOf(Game).PlayerView().ViewTurn();
			if (std::abs(Left) < 4)
			{
				break;
			}
			// The view's yaws turn the other way from the player's turn.
			const float Degrees = std::clamp(static_cast<float>(-Left) / YawUnitsPerDegree, -1.0f, 1.0f);
			FFieldProgram& Field = FieldOf(Game);
			Field.AddViewInput(FFieldViewInput{Degrees});
			const float LiveTurn = Field.PlayerView().LiveOffsets().Turn;
			REQUIRE_FALSE(Game.Frame());
			REQUIRE(Field.Runtime().Context().ViewTurn == -std::lround(LiveTurn * YawUnitsPerDegree));
		}
		Wait(Game, 8);
	}
}

TEST_CASE("A climber seen head-on sorts as one seen head-on, whichever eighth the game's camera stands at under the player's turn", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameState& State = Game->Session->Game();
	State.SetSavedEventVariable(0, Story);
	State.SetParty(Party);
	Game->EnterField(Streets);
	Game->RunUntil([&Game] { return Game->Loop->Field() != nullptr && Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 600);
	FFieldProgram& Field = FieldOf(*Game);
	FEventContext& Context = Field.Runtime().Context();

	// The leader stood halfway up the ladder.
	FEventActorState& Leader = Context.States[Context.LeaderActor];
	FEventActorState Placed = Leader;
	FFieldActor Record = Context.Actors[Context.LeaderActor];
	Placed.Layer = LadderLayer;
	REQUIRE(PlaceActor(Placed, Record, *Context.Walkmesh, LadderX, LadderZ));
	Leader = Placed;
	Context.Actors[Context.LeaderActor] = Record;
	FSprite* Sprite = Context.Models->ActorSprite(Context.LeaderActor);
	REQUIRE(Sprite != nullptr);
	Sprite->FloorY = Leader.FloorHeight;
	Sprite->Position = Leader.Position;
	Settle(*Game);
	REQUIRE((Leader.FloorAttributes & FieldFloorAttribute::Ladder) != 0);

	// The game's camera turned to face the ladder head-on: the climber's own entry.
	for (int32 Press = 0; Press < 8 && OffHeadOn(Context.Camera.GoalViewYaw) > Eighth / 2; ++Press)
	{
		TurnCamera(*Game);
	}
	REQUIRE(OffHeadOn(Context.Camera.GoalViewYaw) <= Eighth / 2);
	const int32 HeadOnYaw = Context.Camera.GoalViewYaw;
	const std::optional<int32> HeadOn = LeaderEntry(Field);
	REQUIRE(HeadOn);

	// An eighth past it, which the game sorts the climber for as seen from the side; the player turns the view back.
	TurnCamera(*Game);
	REQUIRE(OffHeadOn(Context.Camera.GoalViewYaw) > Eighth / 2);
	TurnView(*Game, static_cast<int16>(HeadOnYaw - Context.Camera.GoalViewYaw));
	REQUIRE(OffHeadOn(Context.Camera.GoalViewYaw + Field.PlayerView().ViewTurn()) <= Eighth / 2);
	const std::optional<int32> TurnedBack = LeaderEntry(Field);
	REQUIRE(TurnedBack);
	CHECK(std::abs(*TurnedBack - *HeadOn) <= 1);
}
