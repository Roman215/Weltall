#include "Field/EventContext.h"
#include "Field/FieldCompass.h"
#include "Field/FieldDrawingTables.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScreenLayer.h"
#include "Presentation/DrawList.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The field's 2D layer beside the fades and the windows: the cockpit's instruments, the screen
// markers, the picture of the screen drawn from VRAM, Select's picture, and the scripts' drawing area.

namespace
{
	constexpr FClutPosition MarkerClut{256, 247};
	constexpr FClutPosition HudClut{0, 232};

	/** The instruments' layout: every piece a 4 x 4 rectangle of the image at its own place, added. */
	FFieldDrawingTables Tables()
	{
		FFieldDrawingTables Result;
		Result.CockpitHud.Images = {{16, 32, 4, 4}};
		for (int32 Piece = 0; Piece < FFieldCockpitHudLayout::PieceCount; ++Piece)
		{
			Result.CockpitHud.Pieces[static_cast<std::size_t>(Piece)] = {static_cast<int16>(Piece), 10, 0, 0};
		}
		return Result;
	}

	bool IsMarker(const FDrawPrimitive& Primitive)
	{
		return Primitive.Texture && Primitive.Texture->Clut == MarkerClut;
	}
}

TEST_CASE("The cockpit's instruments are drawn before the fades, and the markers after everything, the last placed first", "[Field]")
{
	FEventContext Context;
	Context.Render.CockpitHud.bOn = true;
	Context.Render.Fades[1].bActive = true;
	Context.Render.Fades[0].bActive = true;
	FFieldScreenMarkers& Markers = Context.Render.Markers;
	Markers.bMade = true;
	Markers.Colors[0] = {10, 20, 30};
	Markers.Colors[5] = {40, 50, 60};
	Markers.Placed = {{0, 100, 50}, {5, 30, 40}};
	FDrawList Layer;
	const uint32 CoverAt = BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() == FFieldCockpitHudLayout::PieceCount + 2 + 2);
	// Piece 108 first, piece 0 last: each at its own x, from the 4-bit page at (896, 0).
	const FDrawPrimitive& First = Layer.Primitives.front();
	CHECK(First.Corners[0].X == 108.0f);
	CHECK(First.Texture->Page == FTexturePage{896, 0, ETextureDepth::Indexed4, EBlendMode::Add});
	CHECK(First.Texture->Clut == HudClut);
	CHECK(First.Blend == EDrawBlend::SemiTransparent);
	CHECK(Layer.Primitives[FFieldCockpitHudLayout::PieceCount - 1].Corners[0].X == 0.0f);
	CHECK_FALSE(Layer.Primitives[FFieldCockpitHudLayout::PieceCount].Texture);
	// Then the scripts' fade and the map's, then marker 5 (placed last) and marker 0, the big one.
	const FDrawPrimitive& Small = Layer.Primitives[Layer.Primitives.size() - 2];
	const FDrawPrimitive& Big = Layer.Primitives.back();
	REQUIRE(IsMarker(Small));
	REQUIRE(IsMarker(Big));
	CHECK(Small.Corners[0].X == 30.0f);
	CHECK(Small.Corners[3].X - Small.Corners[0].X == 8.0f);
	CHECK(Small.Corners[0].U == 0xE0);
	CHECK(Small.Corners[0].V == 0x60);
	CHECK(Small.Corners[0].Color == FDrawColor{40, 50, 60});
	CHECK(Big.Corners[3].X - Big.Corners[0].X == 16.0f);
	CHECK(Big.Corners[0].V == 0x70);
	CHECK(Big.Blend == EDrawBlend::Opaque);
	// The picture of the screen covers it all.
	CHECK(CoverAt == Layer.Primitives.size());
}

TEST_CASE("A picture of the screen goes over the tick's layer, and in a fade frame between the scripts' fade and the map's", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[1].bActive = true;
	Context.Render.Fades[0].bActive = true;
	Context.Render.Cover.bShown = true;
	FDrawList Layer;
	CHECK(BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer) == 2);
	CHECK(BuildFieldScreenLayer(Context, Tables(), EFieldFrame::MapFade, Layer) == 1);
	CHECK(Layer.Primitives.size() == 2);
}

TEST_CASE("A still in VRAM's picture area is drawn as its five columns, every texel added, at the tick's brightness and scale", "[Field]")
{
	FEventContext Context;
	FFieldCover& Cover = Context.Render.Cover;
	FVramImage Still;
	Still.Rect = {0x2C0, 0x100, 0x140, 0xE0};
	Still.Units.assign(0x140 * 0xE0, 0x1234);
	Context.Render.Upload(Still);
	CHECK_FALSE(Cover.bScreen);
	Cover.Take(true, false);
	Cover.bShown = true;
	Cover.Brightness = {0x40, 0x20};
	Context.DrawBuffer = 1;
	FDrawList Layer;
	const uint32 CoverAt = BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() == CoverAt + 5);
	// The last column first, from the 15-bit page at (960, 256), 64 texels across 64 pixels, 223 lines.
	const FDrawPrimitive& Last = Layer.Primitives[CoverAt];
	CHECK(Last.Texture->Page == FTexturePage{0x3C0, 0x100, ETextureDepth::Direct15, EBlendMode::Add});
	CHECK(Last.Blend == EDrawBlend::SemiTransparentAll);
	CHECK(Last.Corners[0].X == 256.0f);
	CHECK(Last.Corners[3].X == 320.0f);
	CHECK(Last.Corners[3].Y == 223.0f);
	CHECK(Last.Corners[3].U == 64.0f);
	CHECK(Last.Corners[0].Color == FDrawColor{0x20, 0x20, 0x20});
	CHECK(Layer.Primitives.back().Texture->Page.X == 0x2C0);
	// Zoomed a quarter bigger: 400 wide about the centre, the GTE's quotient leaving the far edges a pixel in.
	Cover.Scale = 0x1400;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	CHECK(Layer.Primitives.back().Corners[0].X == -40.0f);
	CHECK(Layer.Primitives.back().Corners[0].Y == -28.0f);
	CHECK(Layer.Primitives[CoverAt].Corners[3].X == 359.0f);
	CHECK(Layer.Primitives[CoverAt].Corners[3].Y == 251.0f);
}

TEST_CASE("The picture of the screen holds the screen until the scripts write VRAM into its area", "[Field]")
{
	FFieldRenderState Render;
	CHECK(Render.Cover.bScreen);
	Render.Cover.Take(true, true);
	CHECK(Render.Cover.Taken == 1);
	Render.WroteVram({0x200, 0x100, 0xC0, 0x10});
	CHECK(Render.Cover.bScreen);
	Render.WroteVram({0x3FF, 0x1DF, 1, 1});
	CHECK_FALSE(Render.Cover.bScreen);
	// Set up again without the screen copied, it stays the scripts' still; copied, it is the screen again.
	Render.Cover.Take(true, false);
	CHECK_FALSE(Render.Cover.bScreen);
	CHECK(Render.Cover.Taken == 1);
	Render.Cover.Take(false, true);
	CHECK(Render.Cover.bScreen);
	CHECK(Render.Cover.Taken == 2);
}

TEST_CASE("Select's picture is drawn alone: three columns of the 8-bit picture on the screen, the leader's marker over them", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[0].bActive = true;
	FFieldMapPicture& Picture = Context.Render.MapPicture;
	Picture.bShown = true;
	Picture.MarkerX = 150;
	Picture.MarkerY = 90;
	Picture.Brightness = 0x40;
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::MapPicture, Layer);
	REQUIRE(Layer.Primitives.size() == 4);
	const FDrawPrimitive& Column = Layer.Primitives.front();
	CHECK(Column.Texture->Page == FTexturePage{0x380, 0x100, ETextureDepth::Indexed8, EBlendMode::Average});
	CHECK(Column.Texture->Clut == FClutPosition{0, 0xF6});
	CHECK(Column.Corners[0].X == 256.0f);
	CHECK(Column.Corners[3].X == 384.0f);
	CHECK(Column.Corners[3].Y == 223.0f);
	CHECK(Column.Blend == EDrawBlend::SemiTransparent);
	// The third column runs past the picture: the screen's edge clips it.
	CHECK(Column.Clip == FScreenRect{0, 0, 320, 224});
	const FDrawPrimitive& Marker = Layer.Primitives.back();
	CHECK(Marker.Corners[0].X == 150.0f);
	CHECK(Marker.Corners[0].U == 0xE0);
	CHECK(Marker.Corners[0].Color == FDrawColor{0x40, 0x40, 0x40});
}

TEST_CASE("The compass is drawn after the cockpit's instruments and before the scripts' fade and the windows", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[1].bActive = true;
	FFieldCompassFrame Compass;
	Compass.Count = 2;
	Compass.Quads[0].Corners = {{{200, 150}, {220, 150}, {200, 170}, {220, 170}}};
	Compass.Quads[0].Texels = {{{0, 192}, {31, 192}, {0, 223}, {31, 223}}};
	Compass.Quads[0].Texture = {0, 640, 256, 256, 242};
	Compass.Quads[0].bShadow = true;
	Compass.Quads[1].Texture = {1, 640, 256, 0, 251};
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer, &Compass);
	REQUIRE(Layer.Primitives.size() == 3);
	const FDrawPrimitive& Shadow = Layer.Primitives[0];
	CHECK(Shadow.Corners[3].X == 220.0f);
	CHECK(Shadow.Corners[3].V == 223.0f);
	CHECK(Shadow.Texture->Page == FTexturePage{640, 256, ETextureDepth::Indexed4, EBlendMode::Subtract});
	CHECK(Shadow.Texture->Clut == FClutPosition{256, 242});
	CHECK(Shadow.Blend == EDrawBlend::SemiTransparent);
	CHECK(Shadow.bAnchorRight);
	const FDrawPrimitive& Dial = Layer.Primitives[1];
	CHECK(Dial.Texture->Page.Depth == ETextureDepth::Indexed8);
	CHECK(Dial.Blend == EDrawBlend::Opaque);
	CHECK(Dial.bAnchorRight);
	CHECK(Shadow.BlendKey != Dial.BlendKey);
	CHECK_FALSE(Layer.Primitives[2].Texture);
	CHECK_FALSE(Layer.Primitives[2].bAnchorRight);
}

TEST_CASE("FE DF's still is drawn after the windows as five columns across the wide screen, shown at half their width", "[Field]")
{
	FEventContext Context;
	Context.Render.bHiResDisplay = true;
	Context.Render.bHiResStill = true;
	Context.Render.DrawClip = {0, 0, 0x280, 0xE0};
	Context.Render.Fades[0].bActive = true;
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() == 6);
	// The last column first, from the 8-bit page at (640 + 4 * 64, 0), 128 texels over 64 of the screen's pixels.
	const FDrawPrimitive& Last = Layer.Primitives[0];
	CHECK(Last.Texture->Page == FTexturePage{896, 0, ETextureDepth::Indexed8, EBlendMode::Average});
	CHECK(Last.Texture->Clut == FClutPosition{0, 0xE8});
	CHECK(Last.Corners[0].X == 256.0f);
	CHECK(Last.Corners[1].X == 320.0f);
	CHECK(Last.Corners[1].U == 128.0f);
	CHECK(Last.Corners[3].Y == 224.0f);
	CHECK(Layer.Primitives[4].Corners[0].X == 0.0f);
	// The wide drawing area is the whole screen: nothing is clipped short of it.
	CHECK(Last.Clip.Width >= 320);
	// The map's fade over it.
	CHECK_FALSE(Layer.Primitives[5].Texture);
}

TEST_CASE("The pause's picture is drawn over everything from the buffer on display", "[Field]")
{
	FEventContext Context;
	Context.DrawBuffer = 1;
	FFieldDrawingTables Pausing = Tables();
	Pausing.PauseImage.Rect = {0, 0, 40, 8};
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Pausing, EFieldFrame::Paused, Layer);
	REQUIRE(Layer.Primitives.size() == 1);
	const FDrawPrimitive& Picture = Layer.Primitives[0];
	// Loaded at (88h, 100) of the buffer the last tick did not draw into: the 15-bit page around it.
	CHECK(Picture.Texture->Page == FTexturePage{128, 0, ETextureDepth::Direct15, EBlendMode::Average});
	CHECK(Picture.Corners[0].X == 136.0f);
	CHECK(Picture.Corners[0].Y == 100.0f);
	CHECK(Picture.Corners[0].U == 8.0f);
	CHECK(Picture.Corners[0].V == 100.0f);
	CHECK(Picture.Corners[3].X == 176.0f);
	CHECK_FALSE(Picture.bModulate);
	Context.DrawBuffer = 0;
	BuildFieldScreenLayer(Context, Pausing, EFieldFrame::Paused, Layer);
	CHECK(Layer.Primitives[0].Texture->Page.Y == 256);
}

TEST_CASE("A window is drawn as far from where the game placed it as its shift says, the rest of the layer where it was", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[0].bActive = true;
	FFieldMessageWindow& Window = Context.Messages.Windows[1];
	Window.bOpen = true;
	Window.Age = 0;
	Window.Flags = FieldMessageWindowFlags::NoBackground;
	Window.bPortrait = true;
	Window.Portrait = 3;
	Window.Frame.PortraitX0 = 20;
	Window.Frame.PortraitX1 = 84;
	Window.Frame.PortraitY0 = 30;
	Window.Frame.PortraitY1 = 94;
	FDrawList Plain;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Plain);
	std::array<std::array<int16, 2>, 4> Shifts{};
	Shifts[1] = {12, -5};
	FDrawList Shifted;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Shifted, nullptr, &Shifts);
	REQUIRE(Plain.Primitives.size() == 2);
	REQUIRE(Shifted.Primitives.size() == 2);
	CHECK(Shifted.Primitives[0].Corners[0].X == Plain.Primitives[0].Corners[0].X + 12);
	CHECK(Shifted.Primitives[0].Corners[3].Y == Plain.Primitives[0].Corners[3].Y - 5);
	// The map's fade stays over the whole screen.
	CHECK(Shifted.Primitives[1].Corners[0].X == Plain.Primitives[1].Corners[0].X);
}

TEST_CASE("The scripts' drawing area clips the whole layer when it narrows the screen", "[Field]")
{
	FEventContext Context;
	Context.Render.Fades[0].bActive = true;
	FDrawList Layer;
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	REQUIRE(Layer.Primitives.size() == 1);
	CHECK(Layer.Primitives.front().Clip.Width > 320);
	Context.Render.DrawClip = {0, 16, 320, 192};
	BuildFieldScreenLayer(Context, Tables(), EFieldFrame::Tick, Layer);
	CHECK(Layer.Primitives.front().Clip == FScreenRect{0, 16, 320, 192});
}
