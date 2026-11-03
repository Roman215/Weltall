#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapFrameBlend.h"
#include "WorldMap/WorldMapPlayerCamera.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace Weltall;

// The player's view of the world map in the game: the right stick's turn and zoom change what the frames show and
// which way the d-pad walks, and nothing else the tick reads.

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState UpHeld{FPadState::Bit(EPadButton::Up)};
	constexpr FPadState R1Held{FPadState::Bit(EPadButton::R1)};
	/** Era 1 (the story's value 30), at the scene's first entrance, facing north-west. */
	constexpr uint16 Story = 30;
	constexpr uint16 Place = 0x401;
	constexpr uint16 Entrance = 1;
	constexpr uint16 CameraYaw = 0xC00;
	constexpr uint16 Scene = 1;
	/** The ticks of the scene's own fade-in. */
	constexpr int32 FadeInTicks = 62;
	constexpr int32 QuarterTurn = 0x400;
	/** The terrain's draw distance under the original's reach. */
	constexpr int32 TerrainReach = 0xF00;

	struct FPlayedMap
	{
		std::unique_ptr<Testing::FGameLoopSession> Game;
		std::unique_ptr<FWorldMapProgram> Program;

		FGameSession& Session() { return *Game->Session; }
		const FWorldMapContext& Context() const { return Program->Runtime().Context(); }

		void Run(FPadState Held, int32 Ticks = 1)
		{
			for (int32 Tick = 0; Tick < Ticks; ++Tick)
			{
				std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program->VblanksPerFrame()), Held);
				const std::optional<FError> Error = Program->Frame(Vblanks);
				INFO((Error ? Error->Message : std::string()));
				REQUIRE_FALSE(Error);
			}
		}
	};

	/** The world map entered and faded in, the party standing where it arrives. */
	FPlayedMap EnterMap(const FIso9660& Disc)
	{
		FPlayedMap Played;
		Played.Game = Testing::FGameLoopSession::Start(Disc);
		FGameState& State = Played.Session().Game();
		State.SetSavedEventVariable(0, Story);
		State.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
		auto Started = FWorldMapProgram::Start(Played.Session());
		INFO((Started ? std::string() : Started.Error().Message));
		REQUIRE(Started);
		Played.Program = std::move(*Started);
		Played.Run(NoButtons, FWorldMapProgram::EntryFadeFrames + FadeInTicks + 2);
		return Played;
	}

	FFieldViewInput Turned(float Degrees, float Zoom = 0.0f)
	{
		FFieldViewInput Input;
		Input.Turn = Degrees;
		Input.Zoom = Zoom;
		return Input;
	}

	int32 FarthestTerrain(const FWorldMapFrame& Frame)
	{
		int32 Farthest = 0;
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Terrain)
			{
				for (std::size_t Corner = 0; Corner < 3; ++Corner)
				{
					Farthest = std::max(Farthest, static_cast<int32>(Polygon.View[Corner][2]));
				}
			}
		}
		return Farthest;
	}
}

TEST_CASE("The world map's frames show the player's turn and zoom over a camera the game keeps as its own", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Game = EnterMap(*Disc);
	FPlayedMap Viewed = EnterMap(*Disc);
	REQUIRE(Game.Program->LatestFrame() != nullptr);
	CHECK(Game.Program->LatestFrame()->bPlayerView);
	CHECK(Game.Program->LatestFrame()->ShownOffsets == FWorldMapCameraOffsets{});
	const int32 GameFarthest = FarthestTerrain(*Game.Program->LatestFrame());
	REQUIRE(GameFarthest > 0);

	// A quarter turn the way R1 turns, and twice as far.
	Viewed.Program->AddViewInput(Turned(90.0f, -1.0f));
	CHECK(Viewed.Program->LiveViewOffsets() == FWorldMapCameraOffsets{2.0f, 0.0f, 90.0f});
	for (int32 Tick = 0; Tick < 40; ++Tick)
	{
		// The game's own camera turns too, half-way through: the view stays a quarter turn from it.
		const FPadState Held = Tick == 10 ? R1Held : NoButtons;
		Game.Run(Held);
		Viewed.Run(Held);
		const FWorldMapContext& Own = Game.Context();
		const FWorldMapContext& Under = Viewed.Context();
		REQUIRE(Under.CameraMatrix == Own.CameraMatrix);
		REQUIRE(Under.CameraYaw == Own.CameraYaw);
		REQUIRE(Under.CameraPitch == Own.CameraPitch);
		REQUIRE(Under.CameraDistance == Own.CameraDistance);
		REQUIRE(Under.Orbit.Eye == Own.Orbit.Eye);
		REQUIRE(Under.Controlled.X == Own.Controlled.X);
		REQUIRE(Under.Controlled.Z == Own.Controlled.Z);
		REQUIRE(Under.SpriteDepths == Own.SpriteDepths);
		REQUIRE(Under.ViewTurn == QuarterTurn);
		REQUIRE(Own.ViewTurn == 0);

		const FWorldMapFrame& Shown = *Viewed.Program->LatestFrame();
		const FWorldMapFrame& Plain = *Game.Program->LatestFrame();
		REQUIRE(Plain.Camera == Own.CameraMatrix);
		REQUIRE(Shown.bPlayerView);
		REQUIRE(Shown.Yaw == static_cast<int16>(Own.CameraYaw + QuarterTurn));
		REQUIRE(Shown.ShownOffsets == FWorldMapCameraOffsets{2.0f, 0.0f, 90.0f});
		const std::optional<FWorldMapCameraBasis> Basis = WorldMapCameraBasis(Under);
		REQUIRE(Basis);
		REQUIRE(Shown.CameraBasis == *Basis);
		REQUIRE(Shown.Camera == WorldMapPlayerView(*Basis, {2.0f, 0.0f, 90.0f}).Camera);
	}
	CHECK(Game.Context().CameraYaw != CameraYaw);

	// The world ends where the game ends it: the terrain is drawn as much farther as the eye stands back.
	const FWorldMapFrame& Shown = *Viewed.Program->LatestFrame();
	const int32 EyeBack = WorldMapPlayerView(Shown.CameraBasis, Shown.ShownOffsets).EyeBack;
	CHECK(EyeBack == Shown.CameraBasis.Distance >> 12);
	const int32 Farthest = FarthestTerrain(Shown);
	CHECK(Farthest > TerrainReach);
	CHECK(Farthest < TerrainReach + EyeBack);
	CHECK(FarthestTerrain(*Game.Program->LatestFrame()) < TerrainReach);

	// The reset puts the view back as the game frames it.
	FFieldViewInput Reset;
	Reset.bReset = true;
	Viewed.Program->AddViewInput(Reset);
	Viewed.Run(NoButtons);
	Game.Run(NoButtons);
	CHECK(Viewed.Session().Settings().WorldMapCamera == FWorldMapCameraOffsets{});
	CHECK(Viewed.Program->LatestFrame()->Camera == Game.Program->LatestFrame()->Camera);
	CHECK(Viewed.Context().ViewTurn == 0);
}

TEST_CASE("On the world map up walks into the view the player sees", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Game = EnterMap(*Disc);
	FPlayedMap Viewed = EnterMap(*Disc);
	Viewed.Program->AddViewInput(Turned(90.0f));
	Game.Run(UpHeld, 20);
	Viewed.Run(UpHeld, 20);
	const int32 Yaw = Game.Context().CameraYaw;
	CHECK(Viewed.Context().CameraYaw == Yaw);
	CHECK(Game.Context().ControlledHeading == Yaw);
	CHECK(Viewed.Context().ControlledHeading == ((Yaw + QuarterTurn) & 0xFFF));
	// The two walked the same way on the screen each sees, a quarter turn apart across the ground.
	CHECK(Viewed.Context().Controlled.X != Game.Context().Controlled.X);
	CHECK(Viewed.Context().Controlled.Z != Game.Context().Controlled.Z);
}

TEST_CASE("The world map's view keeps still through the pause", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Viewed = EnterMap(*Disc);
	Viewed.Program->AddViewInput(Turned(30.0f));
	Viewed.Run(NoButtons);
	const FWorldMapCameraOffsets Before = Viewed.Session().Settings().WorldMapCamera;
	CHECK(Before.Turn == 30.0f);

	// With no controller the program waits in the pause's frames, which draw nothing new.
	Viewed.Session().Pad().SetConnected(false);
	Viewed.Run(NoButtons, 2);
	CHECK(Viewed.Program->VblanksPerFrame() == 1);
	Viewed.Program->AddViewInput(Turned(30.0f));
	CHECK(Viewed.Program->LiveViewOffsets() == Viewed.Program->LatestFrame()->ShownOffsets);
	Viewed.Run(NoButtons, 2);
	Viewed.Program->AddViewInput(Turned(30.0f));
	Viewed.Run(NoButtons, 2);
	CHECK(Viewed.Session().Settings().WorldMapCamera == Before);
	CHECK(Viewed.Program->LiveViewOffsets() == Viewed.Program->LatestFrame()->ShownOffsets);

	Viewed.Session().Pad().SetConnected(true);
	Viewed.Run(NoButtons, 3);
	Viewed.Program->AddViewInput(Turned(30.0f));
	Viewed.Run(NoButtons);
	CHECK(Viewed.Session().Settings().WorldMapCamera.Turn == 60.0f);
}

TEST_CASE("The world map's zoom leaves everything in the order the game's camera sorts it", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Facing Blackmoon Forest: the trees stand close, many overlapping, as the ordering table sorts them.
	FPlayedMap Viewed = EnterMap(*Disc);
	Viewed.Program->SetCulling(EWorldMapCulling::InRange);
	// The game's camera settles on its distance after the scene's entry: the order is the game's own once it rests.
	int32 Resting = 0;
	for (int32 Tick = 0; Tick < 600 && Resting < 10; ++Tick)
	{
		const FGteMatrix Before = Viewed.Context().CameraMatrix;
		Viewed.Run(NoButtons);
		Resting = Viewed.Context().CameraMatrix == Before ? Resting + 1 : 0;
	}
	REQUIRE(Resting == 10);
	const auto Entries = [&Viewed]()
	{
		std::map<uint32, uint16> Entry;
		const FWorldMapFrame& Frame = *Viewed.Program->LatestFrame();
		for (const FWorldMapPolygon& Polygon : Frame.Polygons)
		{
			if ((Polygon.Layer == EWorldMapLayer::Billboard || Polygon.Layer == EWorldMapLayer::Sprite) && Polygon.Anchor != FWorldMapPolygon::NoAnchor)
			{
				Entry.emplace(Frame.Anchors[Polygon.Anchor].Key, Polygon.Bucket);
			}
		}
		return Entry;
	};
	const std::map<uint32, uint16> AsTheGameSortsThem = Entries();
	REQUIRE(AsTheGameSortsThem.size() > 100);
	// Pulled back to twice the distance and in to half, a step a tick: a tree or a sprite the view still has stays in
	// its entry (cut into entries as the zoomed depths are, trees side by side change places, and seem to hop).
	int32 Compared = 0;
	for (int32 Tick = 0; Tick < 60; ++Tick)
	{
		Viewed.Program->AddViewInput(Turned(0.0f, Tick < 20 ? -0.05f : 0.05f));
		Viewed.Run(NoButtons);
		for (const auto& [Key, Entry] : Entries())
		{
			if (const auto Found = AsTheGameSortsThem.find(Key); Found != AsTheGameSortsThem.end())
			{
				INFO("tick " << Tick << ", zoom " << Viewed.Program->LatestFrame()->ShownOffsets.Zoom << ", key " << Key);
				CHECK(Entry == Found->second);
				++Compared;
			}
		}
	}
	CHECK(Viewed.Program->LatestFrame()->ShownOffsets.Zoom == FWorldMapCameraOffsets::MinZoom);
	CHECK(Compared > 1000);
}

TEST_CASE("As the game's camera settles the trees move on between ticks, where its rounding turned them back and forth", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPlayedMap Viewed = EnterMap(*Disc);
	// Each tree's foot on the original's screen at a display's two frames a tick, drawn as the frames have it and unrounded.
	std::array<std::map<uint32, std::vector<std::array<float, 2>>>, 2> Feet;
	std::vector<int32> Matches;
	std::vector<std::pair<uint32, int32>> Scratch;
	FWorldMapFrame Before = *Viewed.Program->LatestFrame();
	for (int32 Tick = 0; Tick < 120; ++Tick)
	{
		Viewed.Run(NoButtons);
		const FWorldMapFrame& Latest = *Viewed.Program->LatestFrame();
		MatchWorldMapAnchors(Before, Latest, Matches, Scratch);
		for (const float Alpha : {0.5f, 1.0f})
		{
			for (std::size_t Way = 0; Way < Feet.size(); ++Way)
			{
				FWorldMapFrameBlend Blend;
				BlendWorldMapFrames(Latest, &Before, Matches, Alpha, Blend);
				if (Way == 1)
				{
					UnroundWorldMapBlend(Latest, &Before, Alpha, Blend);
				}
				for (const FWorldMapPolygon& Polygon : Latest.Polygons)
				{
					if (Polygon.Layer == EWorldMapLayer::Billboard)
					{
						const std::array<float, 3> Foot = MoveViewPoint(Blend.Moves[Polygon.Anchor], {(Polygon.View[2][0] + Polygon.View[3][0]) / 2.0f,
							(Polygon.View[2][1] + Polygon.View[3][1]) / 2.0f, (Polygon.View[2][2] + Polygon.View[3][2]) / 2.0f});
						Feet[Way][Latest.Anchors[Polygon.Anchor].Key].push_back({Foot[0] / Foot[2], Foot[1] / Foot[2]});
					}
				}
			}
		}
		Before = Latest;
	}
	std::array<int32, 2> TurnsBack{};
	for (std::size_t Way = 0; Way < Feet.size(); ++Way)
	{
		for (const auto& [Key, Track] : Feet[Way])
		{
			for (std::size_t At = 2; At < Track.size(); ++At)
			{
				for (std::size_t Axis = 0; Axis < 2; ++Axis)
				{
					const float First = Track[At - 1][Axis] - Track[At - 2][Axis];
					const float Second = Track[At][Axis] - Track[At - 1][Axis];
					TurnsBack[Way] += First * Second < 0.0f && std::abs(First) > 1e-4f && std::abs(Second) > 1e-4f ? 1 : 0;
				}
			}
		}
	}
	INFO("drawn " << TurnsBack[0] << ", unrounded " << TurnsBack[1]);
	REQUIRE(Feet[0].size() > 100);
	CHECK(TurnsBack[1] * 2 < TurnsBack[0]);
}

TEST_CASE("On the world map camera buttons held to zoom the view leave the game's camera unturned", "[Game][WorldMap][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FPadState Zooming = R1Held;
	Zooming.ZoomButtons = R1Held.Buttons;
	FPlayedMap Zoomed = EnterMap(*Disc);
	FPlayedMap Turned = EnterMap(*Disc);
	const int16 Yaw = Zoomed.Context().CameraYaw;
	Zoomed.Run(Zooming, 20);
	Turned.Run(R1Held, 20);
	CHECK(Zoomed.Context().CameraYaw == Yaw);
	CHECK(Turned.Context().CameraYaw != Yaw);
}
