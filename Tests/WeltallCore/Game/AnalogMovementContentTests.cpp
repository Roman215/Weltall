#include "Game/FieldProgram.h"
#include "Graphics/Sprite.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <optional>
#include <string>

using namespace Weltall;

namespace
{
	constexpr int32 ForestMap = 22;
	constexpr int32 Frames = 60;
	/** Long enough to be walking, short enough to meet no wall on the way. */
	constexpr int32 HeadingFrames = 20;
	constexpr uint8 Centre = FPadState::StickCentre;
	constexpr uint16 AngleMask = 0xFFF;

	/** The stick at (X, Y) holding the directions its bindings would: a gamepad's, with nothing else held. */
	FPadState Stick(uint8 X, uint8 Y, uint16 Directions, uint16 Others = 0)
	{
		return FPadState{static_cast<uint16>(Directions | Others), Directions, X, Y, true};
	}

	constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
	constexpr uint16 Right = FPadState::Bit(EPadButton::Right);
	constexpr uint16 RunButton = FPadState::Bit(EPadButton::Circle);

	/** The leader's walk up the forest, and what the field kept of it. */
	struct FWalk
	{
		FFieldPoint From{};
		FFieldPoint To{};
		uint16 Facing = 0;
		/** The heading the camera gives a direction of the pad's table (4096 a turn). */
		uint16 CameraYaw = 0;
		uint32 StepsRun = 0;
		uint32 TimeScale = 0;
		int32 EncounterCountdown = 0;
		int32 EncounterPeriod = 0;
		uint16 NewestSlow = 0;

		double Distance() const
		{
			return std::hypot(static_cast<double>((To[0] >> 16) - (From[0] >> 16)), static_cast<double>((To[2] >> 16) - (From[2] >> 16)));
		}
	};

	FWalk Walk(const FIso9660& Disc, bool bAnalog, bool bAlwaysRun, FPadState Held, int32 FrameCount = Frames)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Controls.bAnalogMovement = bAnalog;
		Settings.Controls.bAlwaysRun = bAlwaysRun;
		Game->Session->SetSettings(Settings);
		Game->EnterField(ForestMap);
		FWalk Walked;
		for (int32 Frame = 0; Frame < FrameCount; ++Frame)
		{
			const std::optional<FError> Stopped = Game->Frame(Held);
			INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
			REQUIRE_FALSE(Stopped.has_value());
			if (Frame == 0)
			{
				REQUIRE(Game->Loop->Field() != nullptr);
				const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
				Walked.From = Context.States[Context.LeaderActor].Position;
			}
		}
		REQUIRE(Game->Loop->Field() != nullptr);
		FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		const FEventActorState& Leader = Context.States[Context.LeaderActor];
		Walked.To = Leader.Position;
		Walked.Facing = Leader.Facing;
		Walked.CameraYaw = static_cast<uint16>(Context.Camera.GoalViewYaw + Context.ViewTurn);
		const FSprite* Sprite = Context.Models->ActorSprite(Context.LeaderActor);
		REQUIRE(Sprite != nullptr);
		Walked.StepsRun = Sprite->StepsRun();
		Walked.TimeScale = Sprite->TimeScale();
		Walked.EncounterCountdown = Context.Encounters.Countdown;
		Walked.EncounterPeriod = Context.Encounters.Period;
		Walked.NewestSlow = Context.Controls.Trail[(Context.Controls.TrailIndices[0] + 1) % FFieldControls::TrailLength].Slow;
		return Walked;
	}
}

TEST_CASE("Analog movement walks where the stick points, where the original's eight directions snap it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Up and to the right, nearer the right: its bindings hold both directions, the d-pad's table's 0xE00.
	const FPadState Held = Stick(0xC0, 0x60, Up | Right);
	constexpr uint16 StickHeading = 0xED2;
	const FWalk Analog = Walk(*Disc, true, true, Held, HeadingFrames);
	CHECK(Analog.Facing == ((StickHeading - Analog.CameraYaw) & AngleMask));
	const FWalk Original = Walk(*Disc, false, true, Held, HeadingFrames);
	CHECK(Original.Facing == ((0xE00 - Original.CameraYaw) & AngleMask));
	// Off, the stick plays as the d-pad does.
	CHECK(Original.To == Walk(*Disc, false, true, FPadState{static_cast<uint16>(Up | Right)}, HeadingFrames).To);
}

TEST_CASE("A firm push of the stick plays as the d-pad: Always run runs, else it walks, and the run button swaps them", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	for (const bool bAlwaysRun : {true, false})
	{
		for (const bool bRunButton : {false, true})
		{
			INFO("always run " << bAlwaysRun << ", run button " << bRunButton);
			const uint16 Others = bRunButton ? RunButton : uint16{0};
			const FWalk Pushed = Walk(*Disc, true, bAlwaysRun, Stick(Centre, 0x00, Up, Others));
			CHECK(Pushed.To == Walk(*Disc, true, bAlwaysRun, FPadState{static_cast<uint16>(Up | Others)}).To);
			CHECK(Pushed.NewestSlow == FieldUnslowed);
		}
	}
	// A direction the tick's frames held while the stick has come back to rest walks where the d-pad's table says.
	CHECK(Walk(*Disc, true, true, Stick(Centre, Centre, Up)).To == Walk(*Disc, true, true, FPadState{Up}).To);
}

TEST_CASE("A light push walks slowly, its steps and its way slowed alike, and meets battles as often", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Half of the stick's travel: 0.43 of the way from its dead zone to a full push.
	const FPadState Light = Stick(Centre, 0x40, Up);
	const FWalk Full = Walk(*Disc, true, false, Stick(Centre, 0x00, Up));
	const FWalk Slow = Walk(*Disc, true, false, Light);
	const double Share = Slow.Distance() / Full.Distance();
	INFO("light " << Slow.Distance() << ", full " << Full.Distance() << ", steps " << Slow.StepsRun << " of " << Full.StepsRun);
	CHECK(Share > 0.35);
	CHECK(Share < 0.5);
	CHECK(Slow.StepsRun < Full.StepsRun);
	CHECK(Slow.NewestSlow > FieldUnslowed);
	// The slowing lasts a tick at a time: between ticks the sprite has its own time scale.
	CHECK(Slow.TimeScale == Full.TimeScale);
	REQUIRE(Full.EncounterPeriod != 0);
	CHECK(Slow.EncounterCountdown == Full.EncounterCountdown);

	// Under Always run a light push walks (a firm one runs), and the run button runs at any push.
	const FWalk AlwaysRunLight = Walk(*Disc, true, true, Light);
	CHECK(AlwaysRunLight.Distance() < Full.Distance());
	CHECK(AlwaysRunLight.Distance() > Slow.Distance());
	const FWalk Ran = Walk(*Disc, true, true, FPadState{Up});
	CHECK(Walk(*Disc, true, true, Stick(Centre, 0x40, Up, RunButton)).To == Ran.To);
}
