#include "Menu/MenuDrawing.h"
#include "Menu/MenuSpriteSheet.h"

#include <catch_amalgamated.hpp>

#include <cmath>

using namespace Weltall;

// The menu's frames as the display draws them: between two frames, over any size of screen.
namespace
{
	FMenuPrimitive ScreenQuad(int16 X, int16 Y, int16 Size, uint32 Key)
	{
		FMenuPrimitive Quad;
		Quad.Code = MenuGpu::FlatQuad;
		Quad.BlendKey = Key;
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Quad.Corners[Corner].X = static_cast<int16>(X + (Corner & 1) * Size);
			Quad.Corners[Corner].Y = static_cast<int16>(Y + (Corner >> 1) * Size);
			Quad.Corners[Corner].Color = {0x80, 0x40, 0x20};
		}
		return Quad;
	}
}

TEST_CASE("A menu primitive is drawn between its place in the frame before and its place now", "[Menu]")
{
	FMenuFrame Before;
	FMenuFrame Now;
	Before.Primitives.push_back(ScreenQuad(0, 0, 10, 7));
	Now.Primitives.push_back(ScreenQuad(40, 20, 10, 7));
	// Without a key there is no partner: drawn where it is now.
	Now.Primitives.push_back(ScreenQuad(100, 100, 4, 0));
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, &Before, 0.25f, List);
	REQUIRE(List.Primitives.size() == 2);
	CHECK(List.Primitives[0].Corners[0].X == Catch::Approx(10.0f));
	CHECK(List.Primitives[0].Corners[3].Y == Catch::Approx(15.0f));
	CHECK(List.Primitives[0].BlendKey == 7);
	CHECK(List.Primitives[1].Corners[0].X == Catch::Approx(100.0f));
	CHECK(List.Primitives[0].Clip.Width == MenuScreenSize.Width);
	Builder.Build(Now, nullptr, 0.25f, List);
	CHECK(List.Primitives[0].Corners[0].X == Catch::Approx(40.0f));
}

TEST_CASE("A keyed primitive that moved further than the builder's reach is drawn where it is now", "[Menu]")
{
	FMenuFrame Before;
	FMenuFrame Now;
	Before.Primitives.push_back(ScreenQuad(0, 0, 10, 7));
	Before.Primitives.push_back(ScreenQuad(0, 100, 10, 8));
	Before.Primitives.push_back(ScreenQuad(0, 200, 10, 9));
	// Within reach; across it; grown by one corner past it.
	Now.Primitives.push_back(ScreenQuad(64, 0, 10, 7));
	Now.Primitives.push_back(ScreenQuad(0, 165, 10, 8));
	FMenuPrimitive Grown = ScreenQuad(0, 200, 10, 9);
	Grown.Corners[3].X = 80;
	Now.Primitives.push_back(Grown);
	FMenuDrawBuilder Builder;
	Builder.BlendReach = 64.0f;
	FDrawList List;
	Builder.Build(Now, &Before, 0.5f, List);
	REQUIRE(List.Primitives.size() == 3);
	CHECK(List.Primitives[0].Corners[0].X == Catch::Approx(32.0f));
	CHECK(List.Primitives[1].Corners[0].Y == Catch::Approx(165.0f));
	CHECK(List.Primitives[2].Corners[3].X == Catch::Approx(80.0f));
	CHECK(List.Primitives[2].Corners[0].X == Catch::Approx(0.0f));
	// Without a reach, any distance blends.
	Builder.BlendReach = 0.0f;
	Builder.Build(Now, &Before, 0.5f, List);
	CHECK(List.Primitives[1].Corners[0].Y == Catch::Approx(132.5f));
}

TEST_CASE("A menu drawn on a widened drawing area shows what lies past the screen's edges", "[Menu]")
{
	FMenuFrame Now;
	Now.Primitives.push_back(ScreenQuad(330, 20, 10, 0));
	FMenuPrimitive Line = ScreenQuad(-20, 5, 10, 0);
	Line.Code = MenuGpu::FlatLine;
	Now.Primitives.push_back(Line);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 2);
	CHECK(List.Primitives[0].Clip.X == 0);
	CHECK(List.Primitives[0].Clip.Width == MenuScreenSize.Width);
	Builder.bWideDrawingArea = true;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 2);
	for (const FDrawPrimitive& Drawn : List.Primitives)
	{
		CHECK(Drawn.Clip.X < -1000);
		CHECK(Drawn.Clip.X + Drawn.Clip.Width > 1320);
	}
}

TEST_CASE("The menu's background covers the screen however wide the display, and its drawing mode sets the blend", "[Menu]")
{
	FMenuFrame Now;
	Now.Primitives.push_back(FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 2, 0x180, 0)));
	FMenuPrimitive Grey = ScreenQuad(0, 0, 0, 0);
	Grey.Code = MenuGpu::FlatQuad | MenuGpu::SemiTransparent;
	Grey.bWholeScreen = true;
	Now.Primitives.push_back(Grey);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 1);
	const FDrawPrimitive& Drawn = List.Primitives[0];
	CHECK(Drawn.Blend == EDrawBlend::SemiTransparent);
	CHECK(Drawn.UntexturedBlend == EBlendMode::Subtract);
	CHECK(Drawn.Corners[0].X < -1000.0f);
	CHECK(Drawn.Corners[3].X > 1320.0f);
	CHECK(Drawn.Clip.X < -1000);
}

TEST_CASE("A textured quad shows the texels the GPU's half-texel stepping shows, mirrored or not", "[Menu]")
{
	// 24 pixels over texels A0h-B7h, then the same mirrored as the game writes it: from B7h at the left to 9Fh, the
	// column left of the picture, at the right. Sampled at the pixels' centres, both must show A0h-B7h.
	FMenuFrame Now;
	for (const bool bMirrored : {false, true})
	{
		FMenuPrimitive Quad = ScreenQuad(0, 0, 24, 0);
		Quad.Code = MenuGpu::TexturedQuad;
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			const bool bRight = (Corner & 1) != 0;
			Quad.Corners[Corner].U = static_cast<uint8>(bMirrored ? (bRight ? 0x9F : 0xB7) : (bRight ? 0xB8 : 0xA0));
			Quad.Corners[Corner].V = static_cast<uint8>((Corner >> 1) * 24);
		}
		Now.Primitives.push_back(Quad);
	}
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 2);
	const auto TexelAt = [&](std::size_t Index, float X) {
		const FDrawPrimitive& Drawn = List.Primitives[Index];
		const float Left = Drawn.Corners[0].U;
		const float Right = Drawn.Corners[1].U;
		return static_cast<int32>(std::floor(Left + (Right - Left) * X / 24.0f));
	};
	CHECK(TexelAt(0, 0.5f) == 0xA0);
	CHECK(TexelAt(0, 23.5f) == 0xB7);
	CHECK(TexelAt(1, 0.5f) == 0xB7);
	CHECK(TexelAt(1, 23.5f) == 0xA0);
	CHECK(List.Primitives[0].Corners[2].V == Catch::Approx(24.0f));
	// Either way the picture's own texels are named: A0h to B7h, rows 0 to 23.
	CHECK(List.Primitives[0].Texels == FDrawTexels{0xA0, 0, 0xB7, 23});
	CHECK(List.Primitives[1].Texels == FDrawTexels{0xA0, 0, 0xB7, 23});
}

TEST_CASE("A quad on the menu's layer is stepped from half a texel as the GPU steps its projected corners", "[Menu]")
{
	// A window's top-left corner as the menu builds it: the sheet's corner picture (texels 132-147 down) drawn upside down,
	// its rows written one lower as for a mirrored part (131 at the quad's bottom, 147 at its top), on the layer at rest.
	FMenuFrame Now;
	Now.Layer = MenuFlatTransform;
	FMenuPrimitive Corner;
	Corner.Code = MenuGpu::TexturedQuad;
	Corner.Space = EMenuSpace::Layer;
	// Its corners from the screen's centre (160, 112): left 32, right 48, its first row's corners at y 58, the others at 42.
	Corner.Points = {FGteVector{-128, -54, 0}, FGteVector{-112, -54, 0}, FGteVector{-128, -70, 0}, FGteVector{-112, -70, 0}};
	for (std::size_t At = 0; At < 4; ++At)
	{
		Corner.Corners[At].U = static_cast<uint8>(32 + (At & 1) * 16);
		Corner.Corners[At].V = static_cast<uint8>(131 + (At >> 1) * 16);
	}
	Now.Primitives.push_back(Corner);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 1);
	const FDrawPrimitive& Drawn = List.Primitives[0];
	const auto TexelAt = [&](float Y) {
		const float Top = Drawn.Corners[2].Y;
		const float Bottom = Drawn.Corners[0].Y;
		return static_cast<int32>(std::floor(Drawn.Corners[2].V + (Drawn.Corners[0].V - Drawn.Corners[2].V) * (Y - Top) / (Bottom - Top)));
	};
	// Its top pixel row shows the picture's last row and its bottom one the first, as on the console: the same rows as the
	// window's edges beside it, which are drawn the right way up.
	CHECK(TexelAt(42.5f) == 147);
	CHECK(TexelAt(57.5f) == 132);
	CHECK(Drawn.Texels == FDrawTexels{32, 132, 47, 147});
}

TEST_CASE("A picture drawn at another size keeps to its own texels, though its stepping starts outside them", "[Menu]")
{
	// The menus' pointer over a save's cell: 32 texels over 16 pixels, from C0h. The GPU steps them from half a texel
	// in at a pixel's corner, so at half size the first pixel's centre shows C0h and its left edge lies at BFh and a
	// half: a display with several pixels to the console's one would show the picture beside this one there.
	FMenuFrame Now;
	FMenuPrimitive Half = ScreenQuad(100, 50, 16, 0);
	Half.Code = MenuGpu::TexturedQuad;
	// The same texels over twice their size: the stepping then ends a quarter of a texel past the last.
	FMenuPrimitive Doubled = ScreenQuad(10, 10, 64, 0);
	Doubled.Code = MenuGpu::TexturedQuad;
	for (std::size_t Corner = 0; Corner < 4; ++Corner)
	{
		Half.Corners[Corner].U = static_cast<uint8>(0xC0 + (Corner & 1) * 32);
		Half.Corners[Corner].V = static_cast<uint8>(0x10 + (Corner >> 1) * 32);
		Doubled.Corners[Corner].U = Half.Corners[Corner].U;
		Doubled.Corners[Corner].V = Half.Corners[Corner].V;
	}
	Now.Primitives.push_back(Half);
	Now.Primitives.push_back(Doubled);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 2);
	CHECK(List.Primitives[0].Corners[0].U == Catch::Approx(0xC0 - 0.5f));
	CHECK(List.Primitives[0].Corners[0].V == Catch::Approx(0x10 - 0.5f));
	CHECK(List.Primitives[0].Texels == FDrawTexels{0xC0, 0x10, 0xDF, 0x2F});
	CHECK(List.Primitives[1].Corners[1].U == Catch::Approx(0xE0 + 0.25f));
	CHECK(List.Primitives[1].Texels == FDrawTexels{0xC0, 0x10, 0xDF, 0x2F});
}

TEST_CASE("A sprite rectangle takes its texels from the page of the drawing mode before it", "[Menu]")
{
	FMenuFrame Now;
	FMenuPrimitive Sprite;
	Sprite.Code = MenuGpu::Sprite | MenuGpu::RawTexture;
	Sprite.Corners[0].X = 40;
	Sprite.Corners[0].Y = 60;
	Sprite.Corners[0].U = 8;
	Sprite.Corners[0].V = 13;
	Sprite.Clut = MenuGpu::Clut(0x100, 0x1F0);
	Sprite.Width = 0x100;
	Sprite.Height = 13;
	// Before any drawing mode the page is the first; after one, the mode's.
	Now.Primitives.push_back(Sprite);
	Now.Primitives.push_back(FMenuPrimitive::MakeDrawMode(MenuGpu::TexturePage(0, 0, 0x380, 0x100)));
	Now.Primitives.push_back(Sprite);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, nullptr, 1.0f, List);
	REQUIRE(List.Primitives.size() == 2);
	const FDrawPrimitive& Drawn = List.Primitives[1];
	REQUIRE(Drawn.Texture);
	CHECK(Drawn.Texture->Page == FTexturePage::FromAttribute(MenuGpu::TexturePage(0, 0, 0x380, 0x100)));
	CHECK(Drawn.Texture->Clut == FClutPosition::FromAttribute(Sprite.Clut));
	CHECK(List.Primitives[0].Texture->Page == FTexturePage::FromAttribute(0));
	CHECK(Drawn.CornerCount == 4);
	CHECK_FALSE(Drawn.bModulate);
	CHECK(Drawn.Blend == EDrawBlend::Opaque);
	// One texel a pixel from its corner: 256 across, 13 down.
	CHECK(Drawn.Corners[0].X == Catch::Approx(40.0f));
	CHECK(Drawn.Corners[0].Y == Catch::Approx(60.0f));
	CHECK(Drawn.Corners[3].X == Catch::Approx(296.0f));
	CHECK(Drawn.Corners[3].Y == Catch::Approx(73.0f));
	CHECK(Drawn.Corners[0].U == Catch::Approx(8.0f));
	CHECK(Drawn.Corners[1].U == Catch::Approx(264.0f));
	CHECK(Drawn.Corners[2].V == Catch::Approx(26.0f));
}

TEST_CASE("The menu's layer at rest shows its points at the screen's scale, and turns smoothly between frames", "[Menu]")
{
	const auto [X, Y] = ProjectMenuPoint(MenuFlatTransform, 10.0f, -20.0f, 0.0f);
	CHECK(X == Catch::Approx(170.0f));
	CHECK(Y == Catch::Approx(92.0f));

	// A quad on the layer, a quarter turn about the vertical axis apart: half way it is turned an eighth.
	FMenuPrimitive Quad = ScreenQuad(0, 0, 0, 0);
	Quad.Space = EMenuSpace::Layer;
	Quad.Points[0] = {100, 0, 0};
	FMenuFrame Before;
	FMenuFrame Now;
	Before.Layer = {{0, 0, 0}, {0, 0, 0x400}};
	Now.Layer = {{0, 0x400, 0}, {0, 0, 0x400}};
	Before.Primitives.push_back(Quad);
	Now.Primitives.push_back(Quad);
	FMenuDrawBuilder Builder;
	FDrawList List;
	Builder.Build(Now, &Before, 0.5f, List);
	REQUIRE(List.Primitives.size() == 1);
	const auto [Expected, Unused] = ProjectMenuPoint({{0, 0x200, 0}, {0, 0, 0x400}}, 100.0f, 0.0f, 0.0f);
	CHECK(List.Primitives[0].Corners[0].X == Catch::Approx(Expected));
	CHECK(Expected < 160.0f + 50.0f);
	CHECK(Expected > 160.0f);
}

TEST_CASE("The menus' button pictures name the logical buttons behind them, for the player's glyphs", "[Menu]")
{
	// × Fierce and Accept (logical Circle), △ Weak (logical Square), □ Strong (logical Triangle), ○ (logical Cross).
	CHECK(MenuButtonRole(0xF9) == 0x20);
	CHECK(MenuButtonRole(0xFA) == 0x80);
	CHECK(MenuButtonRole(0xFB) == 0x10);
	CHECK(MenuButtonRole(0xFC) == 0x40);
	CHECK(MenuButtonRole(0x164) == 0x04);
	CHECK(MenuButtonRole(0x165) == 0x08);
	CHECK(MenuButtonRole(0x108) == 0);
	CHECK(MenuButtonRole(0) == 0);
}
