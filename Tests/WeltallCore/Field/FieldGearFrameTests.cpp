#include "Field/FieldGears.h"
#include "Field/FieldPlay.h"
#include "Field/FieldRenderState.h"
#include "Gears/GearFiles.h"
#include "Gears/GearModels.h"
#include "Graphics/GteVectors.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <cstdlib>

using namespace Weltall;

// A new game's field played on maps with gears: what each tick's frame holds of the gears' models
// for the renderer, as the presenter captures it.

namespace
{
	/** Map 16: the mountain path, where four gears stand (models 0, 8, 8 and 8). */
	constexpr int32 MountainPathMap = 16;
	constexpr int32 MountainPathGears = 4;
	/** Map 2: Lahan with a gear the scripts swap. */
	constexpr int32 LahanMap = 2;
	/** Map 28: its gears lit by a point light that follows actor 2 within 4000 of its place. */
	constexpr int32 PointLightMap = 28;
	constexpr int16 PointLightActor = 2;
	constexpr int32 PointLightRadius = 4000;
}

TEST_CASE("A played map with gears lays out each model's floor shadow and bone parts and draws them", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(40);
	const FFieldFrame& Frame = Session->Frame;
	const FFieldGearsFrame& Gears = Frame.Gears;
	CHECK(Gears.Layout == FieldGearLayout(Session->Play->Field().Context().Gears));
	REQUIRE(Gears.Objects.size() == Gears.Items.size());
	int32 Shadows = 0;
	uint32 Polygons = 0;
	for (const FFieldGearLayoutItem& Item : Gears.Items)
	{
		CHECK(Item.FirstPolygon == Polygons);
		Polygons += Item.PolygonCount;
		if (Item.Packets == nullptr)
		{
			++Shadows;
			CHECK(Item.PolygonCount == 1);
			continue;
		}
		CHECK(Item.PolygonCount == Item.Part->Primitives.size());
		CHECK(Item.Packets->Packets.size() == Item.PolygonCount);
	}
	CHECK(Shadows == MountainPathGears);
	CHECK(Gears.Polygons.size() == Polygons);
	// Everything linked is drawn, and the order holds each link once.
	REQUIRE_FALSE(Gears.Links.empty());
	std::size_t Linked = 0;
	for (const FFieldDrawItem& Item : Frame.Order)
	{
		if (Item.Kind == EFieldDrawKind::Gear && Item.Gear == EFieldGearDraw::Polygon)
		{
			++Linked;
			CHECK(Gears.Polygons[Item.Index].bDrawn);
		}
	}
	CHECK(Linked == Gears.Links.size());
}

TEST_CASE("A played map's frame orders its gears among the characters and the scenery far to near, the gears first within an entry", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(40);
	const FFieldFrame& Frame = Session->Frame;
	const FFieldScene& Scene = Session->Play->Drawing().Scene();
	std::size_t First = 0;
	while (First < Frame.Order.size()
		&& (Frame.Order[First].Kind == EFieldDrawKind::Backdrop
			|| (Frame.Order[First].Kind == EFieldDrawKind::Scenery && Scene.Objects[Scene.Polygons[Frame.Order[First].Index].Object].bBackground)))
	{
		++First;
	}
	for (std::size_t Item = First + 1; Item < Frame.Order.size(); ++Item)
	{
		const FFieldDrawItem& Before = Frame.Order[Item - 1];
		const FFieldDrawItem& After = Frame.Order[Item];
		INFO("order " << Item);
		REQUIRE(Testing::FieldDrawEntry(Frame, Before) >= Testing::FieldDrawEntry(Frame, After));
		if (Testing::FieldDrawEntry(Frame, Before) == Testing::FieldDrawEntry(Frame, After))
		{
			REQUIRE(Testing::FieldDrawRank(Before) <= Testing::FieldDrawRank(After));
		}
	}
}

TEST_CASE("A played map's gears keep their layout from tick to tick, and move a little between them", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(50);
	const FFieldGearsFrame Before = Session->Frame.Gears;
	Session->Run(1);
	const FFieldGearsFrame& After = Session->Frame.Gears;
	CHECK(After.Layout == Before.Layout);
	REQUIRE(After.Objects.size() == Before.Objects.size());
	for (std::size_t Object = 0; Object < After.Objects.size(); ++Object)
	{
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			CHECK(std::abs(After.Objects[Object].World[Axis] - Before.Objects[Object].World[Axis]) < 64);
		}
	}
}

TEST_CASE("A played map's gears' textures reach the field's VRAM at their slots' places", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, LahanMap);
	Session->Run(1);
	const FEventContext& Context = Session->Play->Field().Context();
	REQUIRE(Context.Gears.Count > 0);
	auto Files = ReadGearFiles(Disc->Image(), *Session->Index, Context.Gears.FileNumbers[0] / 2);
	REQUIRE(Files);
	REQUIRE_FALSE(Files->Model.Textures.empty());
	const FGearTextureBlock& Block = Files->Model.Textures.front();
	const FGearTexturePlace Place = FieldGearTexturePlace(Context.Gears, 0);
	const bool bAbsolute = (Files->Model.Header.Flags & GearModelFlags::AbsoluteTextures) != 0;
	const int32 X = (bAbsolute ? Block.X : Block.bClut ? Place.ClutX : Place.X) + Block.OffsetX;
	const int32 Y = (bAbsolute ? Block.Y : Block.bClut ? Place.ClutY : Place.Y) + Block.OffsetY;
	const FVram& Vram = Session->Play->Drawing().Vram();
	CHECK(Vram.At(X, Y) == Block.Pixels.front());
	CHECK(Vram.At(X + Block.Width - 1, Y + Block.Height - 1) == Block.Pixels.back());
}

TEST_CASE("A played map that sets no gear lights of its own lights its gears with the load's one white light", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, MountainPathMap);
	Session->Run(40);
	const FFieldGearLights& Lights = Session->Play->Field().Context().Render.GearLights;
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		INFO("channel " << Channel);
		CHECK(Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(0, Channel))] == 0x800);
		CHECK(Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(1, Channel))] == 0);
		CHECK(Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(2, Channel))] == 0);
	}
	// The mountain path's gears are grey, so a white light leaves every drawn corner grey.
	const FFieldGearsFrame& Gears = Session->Frame.Gears;
	int32 Corners = 0;
	for (const FFieldGearLayoutItem& Item : Gears.Items)
	{
		for (uint32 Polygon = Item.FirstPolygon; Item.Packets != nullptr && Polygon < Item.FirstPolygon + Item.PolygonCount; ++Polygon)
		{
			if (!Gears.Polygons[Polygon].bDrawn)
			{
				continue;
			}
			for (const FMdlColor& Color : Gears.Polygons[Polygon].Colors)
			{
				CHECK(Color.R == Color.G);
				CHECK(Color.G == Color.B);
				++Corners;
			}
		}
	}
	CHECK(Corners > 0);
}

TEST_CASE("A played map's point light fades its gears' light by how far its actor stands from its place", "[Field][Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Session = Testing::EnterFieldMap(*Disc, PointLightMap);
	Session->Run(40);
	const FEventContext& Context = Session->Play->Field().Context();
	const FFieldGearLights& Lights = Context.Render.GearLights;
	const FFieldGearLights::FPointLight& Point = Lights.Points[0];
	REQUIRE(Point.Actor == PointLightActor);
	CHECK(Point.Radius == PointLightRadius);
	const FEventActorState& State = Context.States[static_cast<std::size_t>(Point.Actor)];
	int32 Squares = 0;
	for (std::size_t Axis = 0; Axis < 3; ++Axis)
	{
		const int32 Away = Point.Position[Axis] - static_cast<int16>(State.Position[Axis] >> 16);
		Squares += Away * Away;
	}
	const int32 Left = Point.Radius - SquareRoot0(Squares);
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		const std::size_t At = static_cast<std::size_t>(Channel);
		const int32 Difference = (Point.Near[At] - Point.Far[At]) * 0x10000;
		CHECK(Lights.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(0, Channel))] == static_cast<int16>(Difference / Point.Radius * Left >> 16));
	}
}
