#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>

using namespace Weltall;

// The pause after a door: the party leaves Lahan's tavern walking down and left into its door, which opens as it is
// walked into, and comes out in the village with the directions kept from it for the pause, held or pressed.

namespace
{
	constexpr uint16 TavernMap = 9;
	constexpr int32 VillageMap = 1;
	constexpr int32 SettleFrames = 200;
	constexpr int32 FramesToDoor = 60;
	constexpr int32 FramesAfter = 40;
	/** The default pause, half a second, in the console's vertical blanks; the stretch looked at stays inside it. */
	constexpr int32 PauseVblanks = DefaultDoorPause * 6;
	constexpr int32 InsideThePause = PauseVblanks - 6;
	constexpr FPadState Nothing{};
	constexpr FPadState TowardDoor{static_cast<uint16>(FPadState::Bit(EPadButton::Down) | FPadState::Bit(EPadButton::Left))};

	struct FWalk
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;

		const FEventContext& Context() const { return Game->Loop->Field()->Runtime().Context(); }

		std::array<int32, 3> Leader() const
		{
			const FEventActorState& State = Context().States[Context().LeaderActor];
			return {State.Position[0], State.Position[1], State.Position[2]};
		}

		void Run(FPadState Held, int32 Frames)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				const std::optional<FError> Stopped = Game->Frame(Held);
				INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
				REQUIRE_FALSE(Stopped.has_value());
				REQUIRE(Game->Loop->Field() != nullptr);
			}
		}

		/** Frames until Vblanks have gone since the map loaded, a frame's pad from Pad; how many it took. */
		template <typename TPad>
		int32 RunFor(int32 Vblanks, TPad Pad)
		{
			const int32 Loaded = Game->Vblanks;
			int32 Frames = 0;
			while (Game->Vblanks - Loaded < Vblanks)
			{
				Run(Pad(Frames), 1);
				++Frames;
			}
			return Frames;
		}
	};

	/** The party walked out of the tavern into the village with a pause of Tenths after doors, the frame its map loads. */
	FWalk WalkOut(const FIso9660& Disc, int32 Tenths)
	{
		FWalk Walk;
		Walk.Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Walk.Game->Session->Settings();
		Settings.Controls.bWalkIntoDoors = true;
		Settings.Controls.DoorPause = Tenths;
		Walk.Game->Session->SetSettings(Settings);
		Walk.Game->EnterField(TavernMap);
		Walk.Run(Nothing, SettleFrames);
		const int32 Loads = Walk.Game->Loop->Field()->Runtime().MapLoads();
		for (int32 Frame = 0; Frame < FramesToDoor && Walk.Game->Loop->Field()->Runtime().MapLoads() == Loads; ++Frame)
		{
			Walk.Run(TowardDoor, 1);
		}
		REQUIRE(Walk.Context().MapNumber == VillageMap);
		return Walk;
	}
}

TEST_CASE("After a door, a direction held through the map change waits out the pause, then walks on unpressed", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FWalk LetGo = WalkOut(*Disc, DefaultDoorPause);
	const int32 Frames = LetGo.RunFor(InsideThePause, [](int32) { return Nothing; });
	FWalk HeldOn = WalkOut(*Disc, DefaultDoorPause);
	HeldOn.Run(TowardDoor, Frames);
	CHECK(HeldOn.Leader() == LetGo.Leader());

	// Held on past the pause, the same press walks.
	const std::array<int32, 3> Waited = HeldOn.Leader();
	HeldOn.Run(TowardDoor, FramesAfter);
	CHECK(HeldOn.Leader() != Waited);
}

TEST_CASE("After a door, directions let go and pressed again during the pause move nothing either", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FWalk LetGo = WalkOut(*Disc, DefaultDoorPause);
	LetGo.RunFor(InsideThePause, [](int32) { return Nothing; });
	FWalk Fidgeted = WalkOut(*Disc, DefaultDoorPause);
	Fidgeted.RunFor(InsideThePause, [](int32 Frame) { return Frame % 2 == 0 ? TowardDoor : Nothing; });
	CHECK(Fidgeted.Leader() == LetGo.Leader());
}

TEST_CASE("With no pause after doors, a direction held through a map change walks on as in the original", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FWalk LetGo = WalkOut(*Disc, NoDoorPause);
	LetGo.Run(Nothing, FramesAfter);
	FWalk HeldOn = WalkOut(*Disc, NoDoorPause);
	HeldOn.Run(TowardDoor, FramesAfter);
	CHECK(HeldOn.Leader() != LetGo.Leader());
}
