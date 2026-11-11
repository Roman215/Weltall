#include "Field/FieldSceneFrame.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Vram.h"
#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "RenderingThread.h"
#include "Rendering/FieldSceneRendering.h"
#include "Rendering/VramTexture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 ScreenWidth = 320;
	constexpr int32 ScreenHeight = 224;
	constexpr uint16 ClutAttribute = 256 << 6;
	constexpr uint8 RawQuad = 0x2D;
	constexpr uint8 RawSemiTransparentQuad = 0x2F;
	constexpr uint8 ModulatedQuad = 0x2C;
	constexpr uint8 FlatQuad = 0x28;

	/** One row of a 4-bit texture at VRAM (0, 0), its CLUT at (0, 256): transparent, red, semi-transparent green, white. */
	Weltall::FVram MakeVram()
	{
		Weltall::FVram Vram;
		Vram.Upload({{0, 0, 1, 1}, {0x3210}});
		Vram.Upload({{0, 256, 4, 1}, {0x0000, 0x001F, 0x83E0, 0x7FFF}});
		return Vram;
	}

	/**
	 * Screen-aligned quads of one model, seen 1000 units ahead of the eye with H = 500: a quad
	 * Depth units further back is scaled so it still covers the screen rectangle asked for.
	 */
	class FTestScene
	{
	public:
		FTestScene()
		{
			Scene.Models.emplace_back().Parts.emplace_back();
			Scene.State.BackgroundColor = {64, 64, 64};
			View.View = Weltall::FGteMatrix::Identity();
			View.View.T = {0, 0, 1000};
			View.Scene = View.View;
			View.ScreenRotation = Weltall::FGteMatrix::Identity();
			View.Billboard = Weltall::FGteMatrix::Identity();
			View.ProjectionDistance = 500;
		}

		/** A quad over the screen's pixels [X0, X1) x [Y0, Y1): textured with the strip's four texels across, or flat in Color. */
		void Quad(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Depth, uint8 Code, Weltall::FMdlColor Color = {128, 128, 128}, uint16 Page = 0)
		{
			Weltall::FMdlPart& Part = Scene.Models.front().Parts.front();
			const uint16 First = static_cast<uint16>(Part.Vertices.size());
			const auto Model = [Depth](int32 Screen, int32 Center) { return static_cast<int16>((Screen - Center) * (1000 + Depth) / 500); };
			for (const int32 Y : {Y0, Y1})
			{
				for (const int32 X : {X0, X1})
				{
					Part.Vertices.push_back({Model(X, 160), Model(Y, 112), static_cast<int16>(Depth)});
				}
			}
			Weltall::FMdlPrimitive Primitive;
			const bool bTextured = Code != FlatQuad;
			Primitive.Type = bTextured ? Weltall::EMdlPrimitiveType::StaticTexturedQuad : Weltall::EMdlPrimitiveType::StaticFlatQuad;
			Primitive.Code = Code;
			Primitive.Color = Color;
			Primitive.Vertices = {First, static_cast<uint16>(First + 1), static_cast<uint16>(First + 2), static_cast<uint16>(First + 3)};
			Primitive.Uvs = {Weltall::FMdlUv{0, 0}, Weltall::FMdlUv{4, 0}, Weltall::FMdlUv{0, 0}, Weltall::FMdlUv{4, 0}};
			Part.Primitives.push_back(Primitive);
			Weltall::FFieldScenePolygon Polygon;
			Polygon.Primitive = static_cast<uint16>(Part.Primitives.size() - 1);
			Polygon.Packet.Code = Code;
			Polygon.Packet.Colors[0] = Color;
			Polygon.Page = Weltall::FTexturePage::FromAttribute(Page);
			Polygon.Clut = Weltall::FClutPosition::FromAttribute(ClutAttribute);
			Scene.Polygons.push_back(Polygon);
		}

		TArray<FColor> Render(EFieldSorting Sorting, const FVramTexture& Vram)
		{
			Weltall::FFieldSceneObject& Object = Scene.Objects.emplace_back();
			Object.Placement = Weltall::FGteMatrix::Identity();
			Object.Bounds = {{0, 0, 0}, 5000};
			Object.PolygonCount = static_cast<uint32>(Scene.Polygons.size());
			Weltall::FFieldSceneFrame Frame;
			Weltall::EvaluateFieldScene(Scene, View, Weltall::EFieldCulling::Original, Frame);
			const FFieldSceneMesh Mesh(Scene);
			FFieldRenderOptions Options;
			Options.Sorting = Sorting;
			Options.AntiAliasing = EFieldAntiAliasing::None;
			Options.Culling = Weltall::EFieldCulling::Original;
			FFieldSceneDraw Draw;
			BuildFieldSceneDraw(Scene, Frame, View, Mesh, Options, Draw);
			TArray<FColor> Pixels = RenderFieldSceneOffscreen(Mesh, Vram, Draw, FIntPoint(ScreenWidth, ScreenHeight));
			Scene.Objects.clear();
			return Pixels;
		}

		Weltall::FFieldScene Scene;
		Weltall::FFieldView View;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldSceneAndroidSamplesTest, "Weltall.Rendering.FieldSceneAvoidsMsaaOnAdreno610",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldSceneAndroidSamplesTest::RunTest(const FString&)
{
	const FStringView Adreno610(TEXT("Adreno (TM) 610 Vulkan"));
	TestEqual(TEXT("Adreno 610 uses one sample even with MSAA selected"), FieldSceneSampleCount(EFieldAntiAliasing::Msaa, Adreno610), uint8{1});
	TestEqual(TEXT("Other Android adapters can use MSAA"), FieldSceneSampleCount(EFieldAntiAliasing::Msaa, TEXT("Adreno (TM) 618 Vulkan")), uint8{4});
	TestEqual(TEXT("Desktop can use MSAA"), FieldSceneSampleCount(EFieldAntiAliasing::Msaa, {}), uint8{4});
	TestEqual(TEXT("Supersampling still uses a single-sample target"), FieldSceneSampleCount(EFieldAntiAliasing::Supersampling, Adreno610), uint8{1});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldSceneRenderingTest, "Weltall.Rendering.FieldSceneFollowsTheGpusRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldSceneRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	FTestScene Test;
	// The strip raw and opaque, raw and semi-transparent (averaging), and modulated at half.
	Test.Quad(20, 20, 60, 30, 0, RawQuad);
	Test.Quad(20, 40, 60, 50, 0, RawSemiTransparentQuad);
	Test.Quad(20, 60, 60, 70, 0, ModulatedQuad, {64, 64, 64});
	// The other blend modes of the semi-transparent texel, from the pages' blend bits.
	Test.Quad(80, 20, 120, 30, 0, RawSemiTransparentQuad, {128, 128, 128}, 1 << 5);
	Test.Quad(80, 40, 120, 50, 0, RawSemiTransparentQuad, {128, 128, 128}, 2 << 5);
	Test.Quad(80, 60, 120, 70, 0, RawSemiTransparentQuad, {128, 128, 128}, 3 << 5);
	// Two overlapping quads in one ordering-table entry: the farther one linked first is drawn last.
	Test.Quad(150, 20, 200, 60, 4, FlatQuad, {255, 0, 0});
	Test.Quad(170, 40, 220, 80, 0, FlatQuad, {0, 0, 255});

	const TArray<FColor> Painted = Test.Render(EFieldSorting::PaintersOrder, Units);
	const TArray<FColor> Buffered = Test.Render(EFieldSorting::DepthBuffer, Units);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Painted.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Buffered.Num(), Painted.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Pixels, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Pixels[Y * ScreenWidth + X];
		const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
	};
	for (const TArray<FColor>* Pixels : {&Painted, &Buffered})
	{
		Near(*Pixels, TEXT("background"), 300, 200, 64, 64, 64);
		Near(*Pixels, TEXT("texel 0 is transparent"), 24, 25, 64, 64, 64);
		Near(*Pixels, TEXT("raw red"), 34, 25, 255, 0, 0);
		Near(*Pixels, TEXT("semi-transparent texel of an opaque polygon"), 44, 25, 0, 255, 0);
		Near(*Pixels, TEXT("raw white"), 54, 25, 255, 255, 255);
		Near(*Pixels, TEXT("plain texel of a semi-transparent polygon"), 34, 45, 255, 0, 0);
		Near(*Pixels, TEXT("averaged"), 44, 45, 32, 159, 32);
		Near(*Pixels, TEXT("modulated at half"), 34, 65, 127, 0, 0);
		Near(*Pixels, TEXT("added"), 104, 25, 64, 255, 64);
		Near(*Pixels, TEXT("subtracted"), 104, 45, 64, 0, 64);
		Near(*Pixels, TEXT("a quarter added"), 104, 65, 64, 128, 64);
	}
	// With no sprites to sort, the depth buffer leaves the scenery in the painter's order.
	Near(Painted, TEXT("painter's order: the farther quad drawn over the nearer"), 185, 50, 255, 0, 0);
	Near(Buffered, TEXT("depth buffer: the scenery still in the painter's order"), 185, 50, 255, 0, 0);
	return true;
}

#endif
