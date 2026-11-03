#include "Field/FieldPlayerCamera.h"
#include "Game/FieldProgram.h"
#include "Support/FieldTickDigest.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

using namespace Weltall;

// The player's camera changes what the field shows, not what it does, but for who counts as seen with the wide view's
// fix (FEventOptions::bActorsRunInWideView), which these runs leave off.

namespace
{
	/** Map 22: the forest after Lahan, which the player walks at once with the camera following. */
	constexpr int32 ForestMap = 22;
	constexpr int32 Frames = 150;

	/** What a frame of the field's logic leaves: the camera's words, the random numbers, every actor's place, facing and flags, the variables. */
	std::string LogicDigest(FGameSession& Session, const FFieldProgram& Field)
	{
		const FEventContext& Context = Field.Runtime().Context();
		Testing::FOracleDigest Digest;
		Testing::DigestFieldCamera(Digest, Context.Camera);
		Digest.I32(Session.Random().Seed());
		for (const FEventActorState& State : Context.States)
		{
			Digest.I32(State.Position[0], State.Position[1], State.Position[2], State.Flags0, State.Flags1);
			Digest.I16(State.Facing, State.Yaw, State.MoveDirection);
		}
		for (int32 Address = 0; Address < 0x800; Address += 2)
		{
			Digest.I16(Context.Variables.Raw(static_cast<uint32>(Address)));
		}
		return Digest.Hex();
	}

	struct FPlayed
	{
		std::vector<std::string> Logic;
		/** Whether each frame was shown with the game's own view. */
		std::vector<bool> GameViews;
		/** The leader's facing after the frames. */
		uint16 LeaderFacing = 0;
	};

	FPadState Holding(std::initializer_list<EPadButton> Buttons)
	{
		FPadState Pad;
		for (const EPadButton Button : Buttons)
		{
			Pad.Buttons |= FPadState::Bit(Button);
		}
		return Pad;
	}

	/** The player walking about after the map's entry: up, turning the camera with R1, left running, L1, down, right. */
	FPadState Walking(int32 Frame)
	{
		const int32 Step = Frame / 15;
		switch (Step)
		{
		case 2:
			return Holding({EPadButton::Up});
		case 3:
			return Holding({EPadButton::Up, EPadButton::R1});
		case 4:
			return Holding({EPadButton::Left, EPadButton::Circle});
		case 5:
			return Holding({EPadButton::L1});
		case 6:
		case 7:
			return Holding({EPadButton::Down});
		case 8:
		case 9:
			return Holding({EPadButton::Right, EPadButton::Circle});
		default:
			return {};
		}
	}

	/** The forest played for Frames frames with the player walking about (or Held throughout), the player's view at Offsets. */
	FPlayed Play(const FIso9660& Disc, const FFieldCameraOffsets& Offsets, std::optional<FPadState> Held = std::nullopt)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.Fixes.Field.bActorsRunInWideView = false;
		Game->Session->SetSettings(Settings);
		Game->EnterField(ForestMap);
		Game->Session->CameraOffsets() = Offsets;
		FPlayed Played;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const std::optional<FError> Stopped = Game->Frame(Held ? *Held : Walking(Frame));
			INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
			REQUIRE_FALSE(Stopped.has_value());
			const FFieldProgram* Field = Game->Loop->Field();
			REQUIRE(Field != nullptr);
			Played.Logic.push_back(LogicDigest(*Game->Session, *Field));
			const FFieldFrame* Latest = Field->LatestFrame();
			const FGteMatrix& GameView = Field->Runtime().Context().Camera.View.View;
			Played.GameViews.push_back(Latest != nullptr && Latest->View.View.M == GameView.M && Latest->View.View.T == GameView.T);
		}
		const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
		Played.LeaderFacing = Context.States[Context.LeaderActor].Facing;
		return Played;
	}
}

TEST_CASE("The player's zoom and tilt change the field's view and, but for who is seen, nothing its logic does", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FPlayed Game = Play(*Disc, {});
	const FPlayed Zoomed = Play(*Disc, {2.0f, 20.0f, 0.0f});
	CHECK(Zoomed.Logic == Game.Logic);
	CHECK(std::count(Game.GameViews.begin(), Game.GameViews.end(), false) == 0);
	// Once the map has settled every frame is shown from the player's view.
	CHECK(std::count(Zoomed.GameViews.begin() + 8, Zoomed.GameViews.end(), true) == 0);
}

TEST_CASE("The player's turn turns the leader's walk with the view, up into the screen", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr FPadState Up{FPadState::Bit(EPadButton::Up)};
	const FPlayed Game = Play(*Disc, {}, Up);
	const FPlayed Turned = Play(*Disc, {1.0f, 0.0f, 45.0f}, Up);
	// The view turned as R1 turns the camera: its yaws a step back, the walk into the screen a step on.
	CHECK(((Turned.LeaderFacing - Game.LeaderFacing) & 0xFFF) == 0x200);
}

TEST_CASE("Camera buttons held to zoom the view leave the field's camera unturned", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr FPadState R1{FPadState::Bit(EPadButton::R1)};
	FPadState Zooming = R1;
	Zooming.ZoomButtons = R1.Buttons;
	const FPlayed Still = Play(*Disc, {}, FPadState{});
	CHECK(Play(*Disc, {}, Zooming).Logic == Still.Logic);
	CHECK(Play(*Disc, {}, R1).Logic != Still.Logic);
}
