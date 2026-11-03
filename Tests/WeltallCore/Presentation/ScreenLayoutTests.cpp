#include "Presentation/ScreenLayout.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	constexpr FScreenSize Screen{320, 240};
	constexpr FScreenRect WholeScreen{0, 0, 320, 240};
	constexpr FScreenRect Inset{16, 16, 288, 120};
}

TEST_CASE("The game screen fills a 4:3 display", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1600, 1200, Screen);
	CHECK(Layout.Scale() == 5.0);
	CHECK(Layout.ToDisplay(WholeScreen) == FDisplayRect{0, 0, 1600, 1200});
	CHECK(Layout.ToDisplay(Inset) == FDisplayRect{80, 80, 1440, 600});
}

TEST_CASE("A wide display shows the game screen between bars", "[Presentation]")
{
	const FScreenLayout FullHd = FScreenLayout::Fit(1920, 1080, Screen);
	CHECK(FullHd.Scale() == 4.5);
	CHECK(FullHd.ToDisplay(WholeScreen) == FDisplayRect{240, 0, 1440, 1080});
	CHECK(FullHd.ToDisplay(Inset) == FDisplayRect{312, 72, 1296, 540});

	// A handheld's 16:10 screen: a fractional scale, edges on the nearest pixel.
	const FScreenLayout Handheld = FScreenLayout::Fit(1280, 800, Screen);
	CHECK(Handheld.ToDisplay(WholeScreen) == FDisplayRect{107, 0, 1066, 800});
	CHECK(Handheld.ToDisplay(Inset) == FDisplayRect{160, 53, 960, 400});
}

TEST_CASE("A tall display shows the game screen between bars above and below", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(800, 1280, Screen);
	CHECK(Layout.Scale() == 2.5);
	CHECK(Layout.ToDisplay(WholeScreen) == FDisplayRect{0, 340, 800, 600});
}

TEST_CASE("Neighbouring rectangles share their edge whatever the scale", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1111, 777, Screen);
	for (int16 Split = 1; Split < 240; ++Split)
	{
		const FDisplayRect Upper = Layout.ToDisplay({0, 0, 320, Split});
		const FDisplayRect Lower = Layout.ToDisplay({0, Split, 320, static_cast<int16>(240 - Split)});
		REQUIRE(Upper.Y + Upper.Height == Lower.Y);
	}
}

TEST_CASE("An empty display shows nothing", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(0, 720, Screen);
	CHECK(Layout.Scale() == 0.0);
	CHECK(Layout.ToDisplay(Inset).Width == 0);
}

TEST_CASE("Any screen size fits, such as a taller picture than 240 lines", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Fit(1920, 1080, {320, 256});
	CHECK(Layout.Scale() == Catch::Approx(1080.0 / 256.0));
	CHECK(Layout.ToDisplay({0, 0, 320, 256}) == FDisplayRect{285, 0, 1350, 1080});
	CHECK(FScreenLayout::Fit(1920, 1080, {}).Scale() == 0.0);
}

TEST_CASE("The picture takes the window's shape, or the largest 16:9 or 4:3 in its middle", "[Presentation]")
{
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Window) == FDisplayRect{0, 0, 3840, 1080});
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Wide) == FDisplayRect{960, 0, 1920, 1080});
	CHECK(FitAspect(3840, 1080, EDisplayAspect::Standard) == FDisplayRect{1200, 0, 1440, 1080});
	CHECK(FitAspect(1920, 1080, EDisplayAspect::Wide) == FDisplayRect{0, 0, 1920, 1080});
	// A window taller than the shape has its bars above and below.
	CHECK(FitAspect(1280, 1024, EDisplayAspect::Wide) == FDisplayRect{0, 152, 1280, 720});
	CHECK(FitAspect(1080, 1920, EDisplayAspect::Standard) == FDisplayRect{0, 555, 1080, 810});
	CHECK(FitAspect(0, 720, EDisplayAspect::Wide) == FDisplayRect{0, 0, 0, 720});
}

TEST_CASE("A screen laid out whole takes as many display pixels a pixel as asked, centred", "[Presentation]")
{
	const FScreenLayout Layout = FScreenLayout::Whole(1592, 896, {320, 216}, 4);
	CHECK(Layout.Scale() == 4.0);
	CHECK(Layout.ToDisplay({0, 0, 320, 216}) == FDisplayRect{156, 16, 1280, 864});
	// Its 3D scene takes its rows across the whole width.
	CHECK(Layout.Rows(1592, {320, 216}) == FDisplayRect{0, 16, 1592, 864});
	CHECK(FScreenLayout::Fit(1440, 1080, PlayScreen).Rows(1440, PlayScreen) == FDisplayRect{0, 36, 1440, 1008});
	CHECK(FScreenLayout::Whole(1592, 896, PlayScreen, 0).Scale() == 0.0);
}

TEST_CASE("Without integer scaling a frame takes the aspect's whole area, each screen as large as fits", "[Presentation]")
{
	const FDisplayOptions Options;
	const FPictureArea Area = PictureArea(1920, 1080, Options, {320, 216});
	CHECK(Area == FPictureArea{{0, 0, 1920, 1080}, 0});
	CHECK(Area.Layout({320, 216}).Scale() == 5.0);
	CHECK(Area.Layout(PlayScreen).Scale() == Catch::Approx(1080.0 / 224.0));
}

TEST_CASE("Integer scaling draws every screen in play at one whole scale, the picture keeping its shape", "[Presentation]")
{
	FDisplayOptions Options;
	Options.bIntegerScaling = true;
	// 1080 rows hold 224 four times: the field, the menus, the world map's 216 rows and Battling's 218 at the same size.
	const FPictureArea Field = PictureArea(1920, 1080, Options, PlayScreen);
	CHECK(Field == FPictureArea{{164, 92, 1592, 896}, 4});
	CHECK(Field.Layout(PlayScreen).ToDisplay({0, 0, 320, 224}) == FDisplayRect{156, 0, 1280, 896});
	const FPictureArea WorldMap = PictureArea(1920, 1080, Options, {320, 216});
	CHECK(WorldMap == Field);
	CHECK(WorldMap.Layout({320, 216}).ToDisplay({0, 0, 320, 216}) == FDisplayRect{156, 16, 1280, 864});
	CHECK(WorldMap.Layout({320, 218}).ToDisplay({0, 0, 320, 218}) == FDisplayRect{156, 12, 1280, 872});
	// A movie's 240 rows at the same scale, in an area tall enough for them.
	const FPictureArea Movie = PictureArea(1920, 1080, Options, {320, 240});
	CHECK(Movie == FPictureArea{{107, 60, 1706, 960}, 4});

	// 4K holds 224 nine times, its picture's sides exact.
	CHECK(PictureArea(3840, 2160, Options, PlayScreen) == FPictureArea{{128, 72, 3584, 2016}, 9});
	CHECK(PictureArea(3840, 2160, Options, {320, 240}) == FPictureArea{{0, 0, 3840, 2160}, 9});
	CHECK(PictureArea(2560, 1440, Options, PlayScreen).WholeScale == 6);
	// A handheld's 800 rows hold 224 three times.
	CHECK(PictureArea(1280, 800, Options, PlayScreen) == FPictureArea{{103, 64, 1074, 672}, 3});
	// Where 240 rows fit fewer times than 224, a movie alone is smaller.
	CHECK(PictureArea(1600, 900, Options, PlayScreen).WholeScale == 4);
	CHECK(PictureArea(1600, 900, Options, {320, 240}) == FPictureArea{{160, 90, 1280, 720}, 3});
}

TEST_CASE("Integer scaling in 4:3 keeps the console's 240 lines, the screen's rows in their middle", "[Presentation]")
{
	FDisplayOptions Options;
	Options.bIntegerScaling = true;
	Options.Aspect = EDisplayAspect::Standard;
	const FPictureArea Field = PictureArea(1920, 1080, Options, PlayScreen);
	CHECK(Field == FPictureArea{{320, 60, 1280, 960}, 4});
	CHECK(Field.Layout(PlayScreen).Rows(1280, PlayScreen) == FDisplayRect{0, 32, 1280, 896});
	CHECK(PictureArea(1920, 1080, Options, {320, 240}) == Field);
}

TEST_CASE("Integer scaling leaves a display too small for one whole scale as it was", "[Presentation]")
{
	FDisplayOptions Options;
	Options.bIntegerScaling = true;
	CHECK(PictureArea(300, 200, Options, PlayScreen) == FPictureArea{{0, 0, 300, 200}, 0});
	CHECK(PictureArea(1920, 1080, Options, {}) == FPictureArea{{0, 0, 1920, 1080}, 0});
	CHECK(PictureArea(0, 0, Options, PlayScreen).Rect.Width == 0);
}
