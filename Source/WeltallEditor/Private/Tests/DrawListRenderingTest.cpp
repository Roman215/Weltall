#include "Field/FieldRenderOptions.h"
#include "Graphics/Vram.h"
#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "Presentation/DrawList.h"
#include "RenderingThread.h"
#include "Rendering/DrawListTextures.h"
#include "Rendering/ScreenArea.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** One row of a 4-bit texture at VRAM (0, 0) and its CLUT at (0, 256): transparent, red, semi-transparent green, white. */
	Weltall::FVram MakeVram()
	{
		Weltall::FVram Vram;
		Vram.Upload({{0, 0, 1, 1}, {0x3210}});
		Vram.Upload({{0, 256, 4, 1}, {0x0000, 0x001F, 0x83E0, 0x7FFF}});
		return Vram;
	}

	Weltall::FDrawTexture Texture(Weltall::EBlendMode Blend = Weltall::EBlendMode::Average)
	{
		return {0, {0, 0, Weltall::ETextureDepth::Indexed4, Blend}, {0, 256}};
	}

	Weltall::FDrawPrimitive SolidRect(int16 X, int16 Y, int16 Width, int16 Height, Weltall::FDrawColor Color)
	{
		return Weltall::MakeGouraudRect({X, Y, Width, Height}, {Color, Color, Color, Color});
	}

	Weltall::FDrawPrimitive SemiTransparentRect(int16 X, Weltall::EBlendMode Mode, uint8 Level)
	{
		Weltall::FDrawPrimitive Primitive = SolidRect(X, 40, 4, 4, {Level, Level, Level});
		Primitive.Blend = Weltall::EDrawBlend::SemiTransparent;
		Primitive.UntexturedBlend = Mode;
		return Primitive;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrawListRenderingTest, "Weltall.Rendering.DrawListFollowsTheGpusRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDrawListRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	const Weltall::FDrawTexture Textures[] = {Texture(), Texture(Weltall::EBlendMode::Add)};
	FDrawListTextures Resources(std::span<const Weltall::FVram>(&Vram, 1), Textures);
	BeginInitResource(&Resources);

	const Weltall::FDrawColor Grey{64, 64, 64};
	Weltall::FDrawList List;
	List.Primitives.push_back(SolidRect(0, 0, 320, 240, Grey));
	// Gouraud corners.
	List.Primitives.push_back(Weltall::MakeGouraudRect({10, 10, 20, 20}, {Weltall::FDrawColor{255, 0, 0}, Weltall::FDrawColor{0, 255, 0}, Weltall::FDrawColor{0, 0, 255}, Weltall::FDrawColor{255, 255, 255}}));
	// An opaque sprite: texel 0 lets the background through, and a set bit 15 does not matter.
	List.Primitives.push_back(Weltall::MakeSprite(40, 10, 4, 1, Texture(), 0, 0));
	// A semi-transparent sprite: only the texel with bit 15 set blends, here by averaging.
	Weltall::FDrawPrimitive Blended = Weltall::MakeSprite(40, 12, 4, 1, Texture(), 0, 0);
	Blended.Blend = Weltall::EDrawBlend::SemiTransparent;
	List.Primitives.push_back(Blended);
	// A sprite modulated by half brightness.
	List.Primitives.push_back(Weltall::MakeSprite(40, 14, 4, 1, Texture(), 0, 0, {64, 64, 64}));
	// A drawing area that cuts a rectangle in half.
	Weltall::FDrawPrimitive Clipped = SolidRect(60, 10, 10, 10, {255, 255, 255});
	Clipped.Clip = {60, 10, 5, 10};
	List.Primitives.push_back(Clipped);
	// The four blend modes of untextured semi-transparent rectangles over the grey.
	List.Primitives.push_back(SemiTransparentRect(80, Weltall::EBlendMode::Average, 200));
	List.Primitives.push_back(SemiTransparentRect(90, Weltall::EBlendMode::Add, 100));
	List.Primitives.push_back(SemiTransparentRect(100, Weltall::EBlendMode::Subtract, 32));
	List.Primitives.push_back(SemiTransparentRect(110, Weltall::EBlendMode::AddQuarter, 200));
	// Every texel blending, as VRAM marked with bit 15 blends: added here, texel 0 still transparent.
	Weltall::FDrawPrimitive AllBlended = Weltall::MakeSprite(40, 16, 4, 1, Texture(Weltall::EBlendMode::Add), 0, 0);
	AllBlended.Blend = Weltall::EDrawBlend::SemiTransparentAll;
	List.Primitives.push_back(AllBlended);

	const FIntPoint Size(320, 240);
	const TArray<FColor> Pixels = RenderDrawListOffscreen(Resources, FDrawListFrame{&List}, Size, {Size.X, Size.Y});
	BeginReleaseResource(&Resources);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), Size.X * Size.Y))
	{
		return false;
	}
	const auto Near = [this, &Pixels, Size](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Pixels[Y * Size.X + X];
		const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
	};
	Near(TEXT("background"), 300, 200, 64, 64, 64);
	Near(TEXT("Gouraud top left"), 10, 10, 242, 6, 6);
	Near(TEXT("Gouraud bottom right"), 29, 29, 242, 249, 249);
	Near(TEXT("opaque sprite, texel 0"), 40, 10, 64, 64, 64);
	Near(TEXT("opaque sprite, red"), 41, 10, 255, 0, 0);
	Near(TEXT("opaque sprite, green with bit 15"), 42, 10, 0, 255, 0);
	Near(TEXT("opaque sprite, white"), 43, 10, 255, 255, 255);
	Near(TEXT("semi-transparent sprite, texel 0"), 40, 12, 64, 64, 64);
	Near(TEXT("semi-transparent sprite, red drawn opaque"), 41, 12, 255, 0, 0);
	Near(TEXT("semi-transparent sprite, green averaged"), 42, 12, 32, 159, 32);
	Near(TEXT("modulated sprite, red at half"), 41, 14, 127, 0, 0);
	Near(TEXT("inside the drawing area"), 64, 15, 255, 255, 255);
	Near(TEXT("outside the drawing area"), 65, 15, 64, 64, 64);
	Near(TEXT("average"), 81, 41, 132, 132, 132);
	Near(TEXT("add"), 91, 41, 164, 164, 164);
	Near(TEXT("subtract"), 101, 41, 32, 32, 32);
	Near(TEXT("add a quarter"), 111, 41, 114, 114, 114);
	Near(TEXT("every texel blending, texel 0"), 40, 16, 64, 64, 64);
	Near(TEXT("every texel blending, red added"), 41, 16, 255, 64, 64);
	Near(TEXT("every texel blending, green added"), 42, 16, 64, 255, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrawListSpanTest, "Weltall.Rendering.DrawListSpansTheDisplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDrawListSpanTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	const Weltall::FDrawTexture Textures[] = {Texture()};
	FDrawListTextures Resources(std::span<const Weltall::FVram>(&Vram, 1), Textures);
	BeginInitResource(&Resources);

	// The texture's row spread over a display wider than the screen placed in its middle, whatever the corners say.
	Weltall::FDrawList List;
	Weltall::FDrawPrimitive Picture = Weltall::MakeSprite(100, 100, 4, 1, Texture(), 0, 0);
	Picture.bSpansDisplay = true;
	Picture.Clip = {-1000, -1000, 2320, 2240};
	List.Primitives.push_back(Picture);
	const FIntPoint Size(480, 240);
	const TArray<FColor> Pixels = RenderPassesOffscreen(Size, [&Resources, &List](FRDGBuilder& GraphBuilder, FRDGTextureRef Color)
	{
		AddDrawListPasses(GraphBuilder, Resources, FDrawListFrame{&List}, Color, FIntRect(80, 0, 400, 240), {320, 240});
	});
	BeginReleaseResource(&Resources);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), Size.X * Size.Y))
	{
		return false;
	}
	const auto Is = [this, &Pixels, Size](const TCHAR* What, int32 X, int32 Y, const FColor& Expected)
	{
		const FColor& Pixel = Pixels[Y * Size.X + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B), Pixel.R == Expected.R && Pixel.G == Expected.G && Pixel.B == Expected.B);
	};
	// A texel a quarter of the width: clear, red, green, white.
	Is(TEXT("the first texel, clear, from the display's left edge"), 30, 5, FColor::Black);
	Is(TEXT("the second, red"), 170, 120, FColor(255, 0, 0));
	Is(TEXT("the third, green"), 300, 230, FColor(0, 255, 0));
	Is(TEXT("the last, white, out to the display's right edge, past the screen's"), 470, 120, FColor(255, 255, 255));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrawListAnchorRightTest, "Weltall.Rendering.DrawListCanAnchorToThePicturesRightEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDrawListAnchorRightTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	const Weltall::FDrawTexture Textures[] = {Texture()};
	FDrawListTextures Resources(std::span<const Weltall::FVram>(&Vram, 1), Textures);
	BeginInitResource(&Resources);

	Weltall::FDrawList List;
	List.Primitives.push_back(SolidRect(240, 130, 20, 20, {255, 0, 0}));
	Weltall::FDrawPrimitive& Anchored = List.Primitives.emplace_back(SolidRect(240, 160, 20, 20, {0, 255, 0}));
	Anchored.bAnchorRight = true;
	Anchored.Clip = {240, 160, 10, 20};

	// The picture has side bars of its own, so its right edge need not be the output's.
	const FIntPoint Size(960, 240);
	using Weltall::ECompassPlacement;
	struct FCase { int32 Width; ECompassPlacement Placement; };
	for (const FCase& Case : {FCase{320, ECompassPlacement::Ultrawide}, FCase{480, ECompassPlacement::Wide},
		FCase{800, ECompassPlacement::Standard}, FCase{800, ECompassPlacement::Screen}, FCase{800, ECompassPlacement::Off}})
	{
		const int32 Width = Case.Width;
		const int32 Left = (Size.X - Width) / 2;
		const FScreenArea Area(FIntRect(Left, 0, Left + Width, 240));
		const FIntRect ScreenRect = Area.ScreenRect({320, 224});
		FDrawListFrame Frame{&List};
		Frame.RightShift = Left + Weltall::CompassRightEdge(Width, Size.Y, Case.Placement) - ScreenRect.Max.X;
		Frame.bHideRightAnchored = Case.Placement == ECompassPlacement::Off;
		const TArray<FColor> Pixels = RenderPassesOffscreen(Size, [&Resources, &Frame, &ScreenRect](FRDGBuilder& GraphBuilder, FRDGTextureRef Color)
		{
			AddDrawListPasses(GraphBuilder, Resources, Frame, Color, ScreenRect, {320, 224});
		});
		if (TestEqual(TEXT("The frame was read back"), Pixels.Num(), Size.X * Size.Y))
		{
			const int32 Fixed = ScreenRect.Min.X + FMath::FloorToInt(245.0f * ScreenRect.Width() / 320.0f);
			const int32 Moved = Fixed + Frame.RightShift;
			const int32 FixedRow = ScreenRect.Min.Y + FMath::FloorToInt(135.0f * ScreenRect.Height() / 224.0f);
			const int32 MovedRow = ScreenRect.Min.Y + FMath::FloorToInt(165.0f * ScreenRect.Height() / 224.0f);
			TestEqual(TEXT("The rest of the layer stays on the centered screen"), Pixels[FixedRow * Size.X + Fixed], FColor::Red);
			TestEqual(TEXT("The anchored piece keeps its right margin or is hidden"), Pixels[MovedRow * Size.X + Moved],
				Frame.bHideRightAnchored ? FColor::Black : FColor::Green);
			TestEqual(TEXT("The piece's clip moves with it"), Pixels[MovedRow * Size.X + Moved + 10], FColor::Black);
			if (Frame.RightShift != 0)
			{
				TestEqual(TEXT("The anchored piece is absent from its old position"), Pixels[MovedRow * Size.X + Fixed], FColor::Black);
			}
		}
	}
	BeginReleaseResource(&Resources);
	FlushRenderingCommands();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrawListOwnTexelsTest, "Weltall.Rendering.AShrunkPictureKeepsToItsOwnTexels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDrawListOwnTexelsTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	const Weltall::FDrawTexture Textures[] = {Texture()};
	FDrawListTextures Resources(std::span<const Weltall::FVram>(&Vram, 1), Textures);
	BeginInitResource(&Resources);

	// The row's last two texels (green, white) drawn at half size over one of the screen's pixels, as the menus draw
	// their pointer over a save: the texels stepped from half a texel in, so the pixel's left edge lies in the red
	// texel before them. On a display eight times as fine the pixel's first quarter would show that red.
	Weltall::FDrawList List;
	List.Primitives.push_back(SolidRect(0, 0, 320, 240, {64, 64, 64}));
	Weltall::FDrawPrimitive Shrunk = Weltall::MakeSprite(20, 10, 1, 1, Texture(), 2, 0);
	const float Stepped[] = {1.5f, 3.5f, 1.5f, 3.5f};
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		Shrunk.Corners[Corner].U = Stepped[Corner];
	}
	Weltall::FDrawPrimitive Kept = Shrunk;
	for (Weltall::FDrawVertex& Corner : Kept.Corners)
	{
		Corner.X += 10.0f;
	}
	Kept.Texels = Weltall::FDrawTexels{2, 0, 3, 0};
	List.Primitives.push_back(Shrunk);
	List.Primitives.push_back(Kept);
	const int32 Fine = 8;
	const FIntPoint Size(320 * Fine, 240 * Fine);
	const TArray<FColor> Pixels = RenderDrawListOffscreen(Resources, FDrawListFrame{&List}, Size, {320, 240});
	BeginReleaseResource(&Resources);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), Size.X * Size.Y))
	{
		return false;
	}
	const auto Is = [this, &Pixels, Size](const TCHAR* What, int32 X, int32 Y, const FColor& Expected)
	{
		const FColor& Pixel = Pixels[Y * Size.X + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B), Pixel.R == Expected.R && Pixel.G == Expected.G && Pixel.B == Expected.B);
	};
	const int32 Row = 10 * Fine + Fine / 2;
	// Left to itself the stepping reaches the texel before the picture's.
	Is(TEXT("a picture naming no texels of its own, at its left edge: the red before it"), 20 * Fine, Row, FColor(255, 0, 0));
	Is(TEXT("the same, half way: its green"), 20 * Fine + 3, Row, FColor(0, 255, 0));
	// Naming its own, it shows them alone.
	Is(TEXT("a picture naming its texels, at its left edge: its green"), 30 * Fine, Row, FColor(0, 255, 0));
	Is(TEXT("the same, its second pixel: its green"), 30 * Fine + 1, Row, FColor(0, 255, 0));
	Is(TEXT("the same, its right edge: its white"), 30 * Fine + Fine - 1, Row, FColor(255, 255, 255));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrawListTexturesRefreshTest, "Weltall.Rendering.DrawListTexturesFollowVramUploads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDrawListTexturesRefreshTest::RunTest(const FString&)
{
	Weltall::FVram Vrams[] = {MakeVram(), MakeVram()};
	const Weltall::FDrawTexture Textures[] = {Texture(), {1, {0, 0, Weltall::ETextureDepth::Indexed4, Weltall::EBlendMode::Average}, {0, 256}}};
	const FDrawListTextures First(Vrams, Textures);
	TestTrue(TEXT("a new set is current"), First.IsCurrent(Vrams, Textures));
	Vrams[1].Upload({{0, 0, 1, 1}, {0x0123}});
	TestFalse(TEXT("an upload into one of its VRAM states puts it out of date"), First.IsCurrent(Vrams, Textures));
	const FDrawListTextures Second(Vrams, Textures, &First);
	TestTrue(TEXT("the set made after the upload is current"), Second.IsCurrent(Vrams, Textures));
	TestEqual(TEXT("its textures keep their order"), Second.LayerOf(Textures[1]), 1u);
	TestFalse(TEXT("another list of textures is not the one it was made for"), Second.IsCurrent(Vrams, std::span(Textures, 1)));
	return true;
}

#endif
