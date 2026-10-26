#include "Game/FieldProgram.h"
#include "Game/WorldMapProgram.h"
#include "Graphics/Sprite.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <memory>

using namespace Weltall;

namespace
{
	constexpr int32 VillageMap = 35;
	/** Shorter than the walk to the encounter that the same random draws give every run (20 frames from the start). */
	constexpr int32 Frames = 16;
	constexpr uint8 Centre = FPadState::StickCentre;
	constexpr uint16 AngleMask = 0xFFF;
	constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
	constexpr uint16 Right = FPadState::Bit(EPadButton::Right);

	FPadState Stick(uint8 X, uint8 Y, uint16 Directions)
	{
		return FPadState{Directions, Directions, X, Y, true};
	}

	/** The leader's walk on the world map outside Lahan, on foot, and what the map kept of it. */
	struct FWalk
	{
		FWorldMapVector From{};
		FWorldMapVector To{};
		int16 Heading = 0;
		/** The view's yaw, from which the pad's directions count. */
		int32 Yaw = 0;
		int32 EncounterLeft = 0;
		uint32 WalkingTimeScale = 0;
		uint32 RestingTimeScale = 0;

		double Distance() const
		{
			return std::hypot(static_cast<double>((To.X - From.X) >> 12), static_cast<double>((To.Z - From.Z) >> 12));
		}
	};

	FWalk Walk(const FIso9660& Disc, bool bAnalog, FPadState Held)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAnalogMovement = bAnalog;
		Game->Session->SetSettings(Settings);
		Game->EnterField(VillageMap);
		FFieldProgram* Field = Game->Loop->Field();
		REQUIRE(Field != nullptr);
		Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		Game->Frame();
		FEventContext& FieldContext = Field->Runtime().Context();
		FieldContext.Requests.bWorldMap = true;
		FieldContext.bFrameVariablesFrozen = true;
		Game->Session->Game().SetWorldMapPlace(0x405, 1, 0x800, 2);
		const auto Ready = [&]
		{
			const FWorldMapProgram* Map = Game->Loop->WorldMap();
			return Map != nullptr && Map->Runtime().Context().Running != 0 && Map->Runtime().Context().Fading == 0;
		};
		REQUIRE(Game->RunUntil(Ready, 600) < 600);
		const FWorldMapContext& Context = Game->Loop->WorldMap()->Runtime().Context();
		const FWorldMapTask& Leader = Context.Tasks[FWorldMapContext::LeaderSlot];
		const FSprite& Sprite = *Context.SpriteSlots[static_cast<std::size_t>(Leader.Sprite)];
		FWalk Walked;
		Walked.From = Leader.Position;
		const FWorldMapProgram* Map = Game->Loop->WorldMap();
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame(Held).has_value());
		}
		REQUIRE(Game->Loop->WorldMap() == Map);
		Walked.To = Leader.Position;
		Walked.Heading = Leader.Heading;
		Walked.Yaw = (Context.CameraYaw + Context.ViewTurn) & AngleMask;
		Walked.EncounterLeft = Context.Encounters.Left;
		Walked.WalkingTimeScale = Sprite.TimeScale();
		Game->Frame();
		Game->Frame();
		REQUIRE(Game->Loop->WorldMap() == Map);
		Walked.RestingTimeScale = Sprite.TimeScale();
		return Walked;
	}
}

TEST_CASE("On the world map the stick walks the party where it points, where the original's eight directions snap it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Up and to the right, nearer the right: 0xED2 in the stick's own way, a quarter turn on in the world map's.
	const FPadState Held = Stick(0xC0, 0x60, Up | Right);
	const FWalk Analog = Walk(*Disc, true, Held);
	CHECK(Analog.Heading == ((Analog.Yaw + 0xED2 + 0x400) & AngleMask));
	const FWalk Original = Walk(*Disc, false, Held);
	CHECK(Original.Heading == ((Original.Yaw + 0x200) & AngleMask));
	CHECK(Original.To == Walk(*Disc, false, FPadState{static_cast<uint16>(Up | Right)}).To);
	// A firm push straight up walks as the d-pad's up does.
	CHECK(Walk(*Disc, true, Stick(Centre, 0x00, Up)).To == Walk(*Disc, true, FPadState{Up}).To);
}

TEST_CASE("On the world map a light push walks slowly, the party's steps slowed alike, and meets battles as often", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FWalk Full = Walk(*Disc, true, Stick(Centre, 0x00, Up));
	// Half of the stick's travel: 0.43 of the way past its dead zone, 0.64 of the walk's band (full from two thirds).
	const FWalk Slow = Walk(*Disc, true, Stick(Centre, 0x40, Up));
	const double Share = Slow.Distance() / Full.Distance();
	INFO("light " << Slow.Distance() << ", full " << Full.Distance());
	CHECK(Share > 0.55);
	CHECK(Share < 0.72);
	CHECK(Slow.WalkingTimeScale > Full.WalkingTimeScale);
	CHECK(Slow.RestingTimeScale == Full.WalkingTimeScale);
	CHECK(Slow.EncounterLeft == Full.EncounterLeft);
}
