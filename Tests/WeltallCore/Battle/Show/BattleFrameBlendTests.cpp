#include "Battle/Show/BattleFrameBlend.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Graphics/GteVectors.h"
#include "Graphics/SpriteDrawing.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

// A battle frame drawn between two of the show's frames: each drawer, part and shadow moved from where the latest frame
// put it toward where the frame before did, paired by key, and the camera, the cloths, the backdrop and the colours with
// them.
namespace
{
	using FPoint = std::array<float, 3>;

	FBattleShowFrame MakeFrame(uint32 Step)
	{
		FBattleShowFrame Frame;
		Frame.Step = Step;
		Frame.View = FGteMatrix::Identity();
		Frame.View.T = {0, 0, 2000};
		Frame.Eye = {0, -500, -2000};
		Frame.Target = {0, 0, 0};
		Frame.CentreX = 160;
		Frame.CentreY = 164;
		Frame.ProjectionDistance = 512;
		return Frame;
	}

	FGteMatrix At(int32 X, int32 Y, int32 Z)
	{
		FGteMatrix Matrix = FGteMatrix::Identity();
		Matrix.T = {X, Y, Z};
		return Matrix;
	}

	/** A sprite's quad drawn through Matrix by a drawer of the key given, standing at World. */
	void AddSprite(FBattleShowFrame& Frame, uint32 Key, const FGteMatrix& Matrix, const std::array<int16, 3>& World = {})
	{
		FBattleShowQuad& Entry = Frame.Quads.emplace_back();
		Entry.Drawer = static_cast<uint16>(Frame.Drawers.size());
		Entry.Quad.Matrix = Matrix;
		Frame.Drawers.push_back({Key, EBattleShowDrawer::Sprite, World});
	}

	FBattleFrameBlend Blend(const FBattleShowFrame& Latest, const FBattleShowFrame& Previous, float Alpha)
	{
		FBattleFrameKeep LatestKept;
		FBattleFrameKeep PreviousKept;
		KeepBattleFrame(Latest, LatestKept);
		KeepBattleFrame(Previous, PreviousKept);
		FBattleFrameBlend Result;
		FBattleFrameBlendScratch Scratch;
		BlendBattleFrames(LatestKept, &PreviousKept, Alpha, Result, Scratch);
		return Result;
	}

	void CheckNear(const FPoint& Actual, const FPoint& Expected)
	{
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			CHECK(Actual[Axis] == Catch::Approx(Expected[Axis]).margin(0.05));
		}
	}

	bool IsUnmoved(const FViewMove& Move)
	{
		return Move == UnmovedView;
	}
}

TEST_CASE("A battle sprite is drawn between two of the show's frames, paired by its key", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(10);
	FBattleShowFrame Latest = MakeFrame(11);
	AddSprite(Previous, 7, At(100, 0, 1500));
	AddSprite(Latest, 3, At(0, 0, 1500));
	AddSprite(Latest, 7, At(140, -20, 1500));
	const FBattleFrameBlend Result = Blend(Latest, Previous, 0.25f);
	REQUIRE(Result.Moves.size() == 2);
	CHECK(IsUnmoved(Result.Moves[0]));
	// A corner 10 units right of the sprite's origin: a quarter of the way from the frame before's place.
	const FPoint Corner = ViewPoint(Latest.Quads[1].Quad.Matrix, {10, 0, 0});
	CheckNear(MoveViewPoint(Result.Moves[1], Corner), {120.0f, -5.0f, 1500.0f});
	CheckNear(MoveViewPoint(Blend(Latest, Previous, 1.0f).Moves[1], Corner), Corner);
}

TEST_CASE("Battle drawers sharing a key pair in their order", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(1);
	FBattleShowFrame Latest = MakeFrame(2);
	AddSprite(Previous, 5, At(0, 0, 1000));
	AddSprite(Previous, 5, At(200, 0, 1000));
	AddSprite(Latest, 5, At(20, 0, 1000));
	AddSprite(Latest, 5, At(220, 0, 1000));
	const FBattleFrameBlend Result = Blend(Latest, Previous, 0.5f);
	CheckNear(MoveViewPoint(Result.Moves[0], {20.0f, 0.0f, 1000.0f}), {10.0f, 0.0f, 1000.0f});
	CheckNear(MoveViewPoint(Result.Moves[1], {220.0f, 0.0f, 1000.0f}), {210.0f, 0.0f, 1000.0f});
}

TEST_CASE("A battle drawer that jumped or turned about is drawn where the latest frame has it", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(1);
	FBattleShowFrame Latest = MakeFrame(2);
	AddSprite(Previous, 5, At(0, 0, 1000), {0, 0, 0});
	AddSprite(Latest, 5, At(20, 0, 1000), {600, 0, 0});
	CHECK(IsUnmoved(Blend(Latest, Previous, 0.5f).Moves[0]));

	Latest.Drawers[0].World = {20, 0, 0};
	CHECK_FALSE(IsUnmoved(Blend(Latest, Previous, 0.5f).Moves[0]));
	Latest.Quads[0].Quad.Matrix.M[0][0] = -FGteMatrix::One;
	CHECK(IsUnmoved(Blend(Latest, Previous, 0.5f).Moves[0]));
}

TEST_CASE("Nothing of a battle frame blends across a cut", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(4);
	AddSprite(Previous, 1, At(0, 0, 1000));
	Previous.ProjectionDistance = 400;
	const auto Unmoved = [&Previous](const FBattleShowFrame& Latest)
	{
		const FBattleFrameBlend Result = Blend(Latest, Previous, 0.0f);
		return IsUnmoved(Result.Moves[0]) && IsUnmoved(Result.View) && Result.ProjectionDistance == static_cast<float>(Latest.ProjectionDistance);
	};
	const auto Moved = [](uint32 Step)
	{
		FBattleShowFrame Frame = MakeFrame(Step);
		AddSprite(Frame, 1, At(30, 0, 1000));
		return Frame;
	};
	CHECK_FALSE(Unmoved(Moved(5)));
	CHECK(Unmoved(Moved(6)));
	FBattleShowFrame Jumped = Moved(5);
	Jumped.Eye[0] = 1500;
	CHECK(Unmoved(Jumped));
	FBattleShowFrame Turned = Moved(5);
	Turned.View = RotationYxz({0, 0x200, 0});
	CHECK(Unmoved(Turned));
}

TEST_CASE("The battle's camera moves the world's points between frames", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(1);
	FBattleShowFrame Latest = MakeFrame(2);
	Latest.View = RotationYxz({0, 0x40, 0});
	Latest.View.T = {0, 0, 2100};
	const FGteVector World{300, -40, 100};
	const FPoint Then = ViewPoint(Previous.View, World);
	const FPoint Now = ViewPoint(Latest.View, World);
	const FBattleFrameBlend Result = Blend(Latest, Previous, 0.0f);
	CheckNear(MoveViewPoint(Result.View, Now), Then);
	// A drawer without a matrix of its own (a spark) moves with the view about its place.
	FBattleShowFrame PreviousSpark = Previous;
	FBattleShowFrame LatestSpark = Latest;
	PreviousSpark.Drawers.push_back({9, EBattleShowDrawer::Tile, World});
	LatestSpark.Drawers.push_back({9, EBattleShowDrawer::Tile, World});
	CheckNear(MoveViewPoint(Blend(LatestSpark, PreviousSpark, 0.0f).Moves[0], Now), Then);
}

TEST_CASE("A battle object's parts and shadow blend by slot and bone, its cloth's points by their keys", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(1);
	FBattleShowFrame Latest = MakeFrame(2);
	FBattleObjectsFrame PreviousObjects;
	FBattleObjectsFrame LatestObjects;
	PreviousObjects.Parts.push_back({At(0, 0, 1000), nullptr, nullptr, 2, 4, {}});
	PreviousObjects.Parts.push_back({At(0, 100, 1000), nullptr, nullptr, 2, 5, {}});
	LatestObjects.Parts.push_back({At(0, 140, 1000), nullptr, nullptr, 2, 5, {}});
	LatestObjects.Parts.push_back({At(40, 0, 1000), nullptr, nullptr, 2, 4, {}});
	FBattleDrawnShadow Shadow;
	Shadow.Slot = 2;
	Shadow.Matrix = At(0, 200, 1000);
	PreviousObjects.Shadows.push_back(Shadow);
	Shadow.Matrix = At(60, 200, 1000);
	LatestObjects.Shadows.push_back(Shadow);
	FGearDrawnClothTriangle Cloth;
	Cloth.Points = {FGteVector{0, 0, 0}, FGteVector{100, 0, 0}, FGteVector{0, 100, 0}};
	Cloth.PointKeys = {1, 2, 3};
	PreviousObjects.Cloths.push_back(Cloth);
	Cloth.Points = {FGteVector{0, 40, 0}, FGteVector{900, 0, 0}, FGteVector{20, 100, 0}};
	Cloth.PointKeys = {1, 2, 4};
	LatestObjects.Cloths.push_back(Cloth);
	Previous.Objects = &PreviousObjects;
	Latest.Objects = &LatestObjects;

	const FBattleFrameBlend Result = Blend(Latest, Previous, 0.5f);
	REQUIRE(Result.Moves.size() == 3);
	CHECK(Result.FirstPart == 0);
	CHECK(Result.FirstShadow == 2);
	CheckNear(MoveViewPoint(Result.Moves[0], {0.0f, 140.0f, 1000.0f}), {0.0f, 120.0f, 1000.0f});
	CheckNear(MoveViewPoint(Result.Moves[1], {40.0f, 0.0f, 1000.0f}), {20.0f, 0.0f, 1000.0f});
	CheckNear(MoveViewPoint(Result.Moves[2], {60.0f, 200.0f, 1000.0f}), {30.0f, 200.0f, 1000.0f});
	REQUIRE(Result.Cloths.size() == 1);
	CheckNear(Result.Cloths[0][0], {0.0f, 20.0f, 0.0f});
	// The second point jumped, the third is a new one: each where the latest frame has it.
	CheckNear(Result.Cloths[0][1], {900.0f, 0.0f, 0.0f});
	CheckNear(Result.Cloths[0][2], {20.0f, 100.0f, 0.0f});
}

TEST_CASE("The battle's projection, colours and dome blend between frames", "[battle][blend]")
{
	FBattleShowFrame Previous = MakeFrame(1);
	FBattleShowFrame Latest = MakeFrame(2);
	Previous.ProjectionDistance = 400;
	Previous.CentreY = 150;
	Previous.bCleared = Latest.bCleared = true;
	Previous.ClearColor = {0, 0, 20};
	Latest.ClearColor = {0, 0, 60};
	Previous.Fade = {1, {100, 0, 0}};
	Latest.Fade = {1, {50, 0, 0}};
	Previous.Order.push_back({EBattleShowLink::Fade, 0, 2});
	Latest.Order.push_back({EBattleShowLink::Fade, 0, 2});
	FBattleDomePacket Cell;
	Cell.Quad.Corners = {{{0, 100}, {20, 100}, {0, 120}, {20, 120}}};
	Previous.Backdrop.Dome.push_back(Cell);
	Cell.Quad.Corners = {{{10, 100}, {30, 100}, {10, 120}, {30, 120}}};
	Latest.Backdrop.Dome.push_back(Cell);

	FBattleFrameBlend Result = Blend(Latest, Previous, 0.5f);
	CHECK(Result.ProjectionDistance == Catch::Approx(456.0f));
	CHECK(Result.CentreY == Catch::Approx(157.0f));
	// The clear colours as the GPU fills with them, 20 as 16 and 60 as 57, blended.
	CHECK(Result.ClearColor[2] == Catch::Approx(36.5f));
	CHECK(Result.FadeColor[0] == Catch::Approx(75.0f));
	REQUIRE(Result.DomeCorners.size() == 1);
	CHECK(Result.DomeCorners[0][0][0] == Catch::Approx(5.0f));
	CHECK(Result.DomeCorners[0][3][0] == Catch::Approx(25.0f));

	// A fade that changed its blend mode is the latest's.
	Latest.Fade.Mode = 2;
	Result = Blend(Latest, Previous, 0.5f);
	CHECK(Result.FadeColor[0] == Catch::Approx(50.0f));
}

TEST_CASE("A kept battle frame holds the HUD's parts that move between frames, and only those", "[battle][blend]")
{
	FBattleShowFrame Frame = MakeFrame(1);
	FMenuPrimitive Moving;
	Moving.Code = MenuGpu::FlatQuad;
	Moving.BlendKey = 0x10000002;
	FMenuPrimitive Text = Moving;
	Text.BlendKey = 0;
	Frame.Hud.Primitives = {Text, Moving, Text};
	FBattleFrameKeep Kept;
	KeepBattleFrame(Frame, Kept);
	REQUIRE(Kept.Hud.Primitives.size() == 1);
	CHECK(Kept.Hud.Primitives[0].BlendKey == 0x10000002);
	Frame.Hud.Primitives.clear();
	KeepBattleFrame(Frame, Kept);
	CHECK(Kept.Hud.Primitives.empty());
}
