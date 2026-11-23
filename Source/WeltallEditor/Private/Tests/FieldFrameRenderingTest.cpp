#include "Algo/Count.h"
#include "Field/FieldFrame.h"
#include "Field/FieldSceneFrame.h"
#include "Graphics/TexelKinds.h"
#include "Graphics/Vram.h"
#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Rendering/FieldPictureRendering.h"
#include "Rendering/FieldSceneRendering.h"
#include "Rendering/VramTexture.h"

#if WITH_DEV_AUTOMATION_TESTS

// The field frame's drawing beyond the static scenery and the characters, on synthetic frames:
// particles, models whose vertices or texels change, fog's modulation, the screen wave, the
// scripts' drawing area, and the picture of the screen drawn over the field.

namespace
{
	constexpr int32 ScreenWidth = 320;
	constexpr int32 ScreenHeight = 224;
	constexpr uint16 ClutAttribute = 256 << 6;
	constexpr uint8 RawQuad = 0x2D;
	constexpr uint8 FlatQuad = 0x28;

	/** A row of a 4-bit texture at VRAM (0, 0) and its CLUT at (0, 256): transparent, red, semi-transparent green, white, four times across. */
	Weltall::FVram MakeVram()
	{
		Weltall::FVram Vram;
		Vram.Upload({{0, 0, 4, 1}, std::vector<uint16>(4, 0x3210)});
		Vram.Upload({{0, 256, 4, 1}, {0x0000, 0x001F, 0x83E0, 0x7FFF}});
		return Vram;
	}

	/** A field frame of screen-aligned quads 1000 units ahead of the eye with H = 500, drawn by the field's frame drawing. */
	class FTestFrame
	{
	public:
		FTestFrame()
		{
			Scene.Models.emplace_back().Parts.emplace_back();
			View.View = Weltall::FGteMatrix::Identity();
			View.View.T = {0, 0, 1000};
			View.Scene = View.View;
			View.ScreenRotation = Weltall::FGteMatrix::Identity();
			View.Billboard = Weltall::FGteMatrix::Identity();
			View.ProjectionDistance = 500;
			Frame.bSceneDrawn = true;
			Frame.Background = {64, 64, 64};
			Frame.View = View;
		}

		static int16 Model(int32 Screen, int32 Center) { return static_cast<int16>((Screen - Center) * 2); }

		/** A quad of the scenery over the screen's pixels [X0, X1) x [Y0, Y1), its strip's 16 texels across. */
		void Quad(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 Code, Weltall::FMdlColor Color = {128, 128, 128})
		{
			Weltall::FMdlPart& Part = Scene.Models.front().Parts.front();
			const uint16 First = static_cast<uint16>(Part.Vertices.size());
			for (const int32 Y : {Y0, Y1})
			{
				for (const int32 X : {X0, X1})
				{
					Part.Vertices.push_back({Model(X, 160), Model(Y, 112), 0});
				}
			}
			Weltall::FMdlPrimitive Primitive;
			Primitive.Type = Code == FlatQuad ? Weltall::EMdlPrimitiveType::StaticFlatQuad : Weltall::EMdlPrimitiveType::StaticTexturedQuad;
			Primitive.Code = Code;
			Primitive.Color = Color;
			Primitive.Vertices = {First, static_cast<uint16>(First + 1), static_cast<uint16>(First + 2), static_cast<uint16>(First + 3)};
			Primitive.Uvs = {Weltall::FMdlUv{0, 0}, Weltall::FMdlUv{16, 0}, Weltall::FMdlUv{0, 0}, Weltall::FMdlUv{16, 0}};
			Part.Primitives.push_back(Primitive);
			Weltall::FFieldScenePolygon Polygon;
			Polygon.Primitive = static_cast<uint16>(Part.Primitives.size() - 1);
			Polygon.Packet.Code = Code;
			Polygon.Packet.Colors[0] = Color;
			Polygon.Clut = Weltall::FClutPosition::FromAttribute(ClutAttribute);
			Scene.Polygons.push_back(Polygon);
		}

		/** A particle facing the screen over [X, X + 16) x [Y, Y + 4), its picture the strip's 16 texels, at OtIndex. */
		void Particle(int32 X, int32 Y, uint32 Key, int16 Life, std::array<uint8, 3> Color, uint16 BlendBits, int32 OtIndex)
		{
			Weltall::FFieldParticleQuad& Quad = Frame.Particles.emplace_back();
			Quad.BlendKey = Key;
			Quad.Life = Life;
			Quad.Matrix = Weltall::FGteMatrix::Identity();
			Quad.Matrix.T = {Model(X, 160), Model(Y, 112), 1000};
			Quad.Corners = {{{0, 0, 0}, {32, 0, 0}, {0, 8, 0}, {32, 8, 0}}};
			Quad.Texels = {{{0, 0}, {16, 0}, {0, 1}, {16, 1}}};
			Quad.TexturePage = static_cast<uint16>(BlendBits << 5);
			Quad.Clut = ClutAttribute;
			Quad.Color = Color;
			Quad.OtIndex = OtIndex;
			Frame.Order.push_back({static_cast<uint32>(Frame.Particles.size() - 1), Weltall::EFieldDrawKind::Particle});
		}

		/**
		 * A character's sprite, one part (a 4 texel quad, 16 x 32 pixels) at (X, Y) of the screen, Depth
		 * view units from the eye, linked two entries nearer, as the field links them; or its shadow,
		 * lying on its floor, drawn subtractive.
		 */
		void Sprite(int32 X, int32 Y, int32 Depth, bool bShadow = false)
		{
			Weltall::FFieldCharacterView& Character = Frame.Characters.emplace_back();
			Character.BlendKey = static_cast<uint32>(Frame.Characters.size());
			Character.bOnFloor = bShadow;
			Part(X, Y, Depth, Depth / 16 - 2);
		}

		/** Another part of the last sprite, at (X, Y) of the screen and Depth view units from the eye (a multiple of 250), linked into OtIndex. */
		void Part(int32 X, int32 Y, int32 Depth, int32 OtIndex)
		{
			const bool bShadow = Frame.Characters.back().bOnFloor;
			Weltall::FFieldCharacterQuad& Entry = Frame.Quads.emplace_back();
			Entry.Character = static_cast<uint16>(Frame.Characters.size() - 1);
			Weltall::FSpriteQuad& Quad = Entry.Quad;
			Quad.Matrix = Weltall::FGteMatrix::Identity();
			const auto Units = [Depth](int32 Pixels) { return Pixels * Depth / 500; };
			Quad.Matrix.T = {Units(X - 160), Units(Y - 112), Depth};
			Quad.Corners = {{{0, 0, 0}, {static_cast<int16>(Units(16)), 0, 0}, {0, static_cast<int16>(Units(32)), 0},
				{static_cast<int16>(Units(16)), static_cast<int16>(Units(32)), 0}}};
			Quad.U0 = 4;
			Quad.U1 = 8;
			Quad.V0 = 0;
			Quad.V1 = 1;
			Quad.TexturePage = bShadow ? static_cast<uint16>(2 << 5) : 0;
			Quad.Clut = ClutAttribute;
			Quad.ColorCode = (bShadow ? 0x2F000000u : 0x2D000000u) | 0x808080;
			Quad.OtIndex = OtIndex;
			Frame.Order.push_back({static_cast<uint32>(Frame.Quads.size() - 1), Weltall::EFieldDrawKind::Character});
		}

		/** Evaluates the scenery and puts its polygons into the order, after the particles already there. */
		void Evaluate()
		{
			Weltall::FFieldSceneObject& Object = Scene.Objects.emplace_back();
			Object.Placement = Weltall::FGteMatrix::Identity();
			Object.Bounds = {{0, 0, 0}, 5000};
			Object.PolygonCount = static_cast<uint32>(Scene.Polygons.size());
			Weltall::EvaluateFieldScene(Scene, View, Weltall::EFieldCulling::InRange, Frame.Scenery);
			Frame.SceneryWorld.assign(Scene.Objects.size(), {});
			for (const uint32 Polygon : Frame.Scenery.DrawOrder)
			{
				Frame.Order.push_back({Polygon, Weltall::EFieldDrawKind::Scenery});
			}
		}

		TArray<FColor> Render(const FVramTexture& Vram, const FTestFrame* Previous = nullptr, float Alpha = 1.0f,
			EFieldSorting Sorting = EFieldSorting::PaintersOrder)
		{
			const FFieldSceneMesh Mesh(Scene);
			FFieldRenderOptions Options;
			Options.AntiAliasing = EFieldAntiAliasing::None;
			Options.Sorting = Sorting;
			FFieldSceneDraw Draw;
			if (Previous != nullptr)
			{
				Frame.Tick = Previous->Frame.Tick + 1;
			}
			BuildFieldFrameDraw(Scene, Frame, Previous != nullptr ? &Previous->Frame : nullptr, Alpha, Mesh, Options, Draw);
			return RenderFieldSceneOffscreen(Mesh, Vram, Draw, FIntPoint(ScreenWidth, ScreenHeight));
		}

		Weltall::FFieldScene Scene;
		Weltall::FFieldView View;
		Weltall::FFieldFrame Frame;
	};

	bool IsNear(const FColor& Pixel, int32 R, int32 G, int32 B)
	{
		return FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldParticlesRenderingTest, "Weltall.Rendering.FieldParticlesFollowTheGpusRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldParticlesRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// Additive (the particles' blend bits 0), averaging (3), and modulated at half.
	FTestFrame Test;
	Test.Particle(20, 20, 1, 5, {128, 128, 128}, 1, 10);
	Test.Particle(20, 40, 2, 5, {128, 128, 128}, 0, 10);
	Test.Particle(20, 60, 3, 5, {64, 64, 64}, 1, 10);
	Test.Evaluate();
	const TArray<FColor> Pixels = Test.Render(Units);
	// A particle 32 view units (16 pixels) further right in the latest tick, one tick older: drawn half way.
	FTestFrame Before;
	Before.Particle(4, 100, 9, 6, {128, 128, 128}, 1, 10);
	Before.Evaluate();
	FTestFrame After;
	After.Particle(20, 100, 9, 5, {128, 128, 128}, 1, 10);
	After.Evaluate();
	const TArray<FColor> Between = After.Render(Units, &Before, 0.5f);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Pixels.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Between.Num(), Pixels.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	// Each texel is a pixel across; rows 20 to 23 hold the first particle.
	Near(Pixels, TEXT("texel 0 is transparent"), 20, 21, 64, 64, 64);
	Near(Pixels, TEXT("a texel without bit 15 is drawn opaque"), 21, 21, 255, 0, 0);
	Near(Pixels, TEXT("a texel with bit 15 is added"), 22, 21, 64, 255, 64);
	Near(Pixels, TEXT("its page's averaging"), 22, 41, 32, 159, 32);
	Near(Pixels, TEXT("modulated at half"), 21, 61, 127, 0, 0);
	Near(Pixels, TEXT("beside it, the background"), 40, 21, 64, 64, 64);
	Near(Between, TEXT("between two ticks: half way"), 13, 101, 255, 0, 0);
	Near(Between, TEXT("between two ticks: not yet where the latest put it"), 33, 101, 64, 64, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldTexelKindsRenderingTest, "Weltall.Rendering.FieldLeavesOutPassesWithNoTexelsToDraw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldTexelKindsRenderingTest::RunTest(const FString&)
{
	// Beside the strip's mixed CLUT, one of blended colours only (rain's) and one of plain colours only.
	Weltall::FVram Vram = MakeVram();
	Vram.Upload({{16, 256, 4, 1}, {0x0000, 0x801F, 0x83E0, 0xFFFF}});
	Vram.Upload({{32, 256, 4, 1}, {0x0000, 0x001F, 0x03E0, 0x7FFF}});
	constexpr uint16 BlendedClut = ClutAttribute | 1;
	constexpr uint16 PlainClut = ClutAttribute | 2;
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// Two overlapping particles of blended texels, one of both kinds, one of plain texels: added, as rain is.
	FTestFrame Test;
	Test.Sprite(28, 18, 2000);
	Test.Particle(20, 20, 1, 5, {128, 128, 128}, 1, 10);
	Test.Frame.Particles.back().Clut = BlendedClut;
	Test.Particle(26, 22, 2, 5, {128, 128, 128}, 1, 10);
	Test.Frame.Particles.back().Clut = BlendedClut;
	Test.Particle(20, 60, 3, 5, {128, 128, 128}, 1, 10);
	Test.Particle(20, 100, 4, 5, {128, 128, 128}, 1, 10);
	Test.Frame.Particles.back().Clut = PlainClut;
	Test.Evaluate();
	const FFieldSceneMesh Mesh(Test.Scene);
	const auto Passes = [](const FFieldSceneDraw& Draw, uint8 Pass)
	{
		return Algo::CountIf(Draw.Runs, [Pass](const FFieldDrawRun& Run) { return Run.Pass == Pass; });
	};
	for (const EFieldSorting Sorting : {EFieldSorting::PaintersOrder, EFieldSorting::DepthBufferWithScenery})
	{
		for (const EFieldAntiAliasing AntiAliasing : {EFieldAntiAliasing::None, EFieldAntiAliasing::Msaa})
		{
			const FString Case = FString::Printf(TEXT("sorting %d, anti-aliasing %d"), static_cast<int32>(Sorting), static_cast<int32>(AntiAliasing));
			FFieldRenderOptions Options;
			Options.Sorting = Sorting;
			Options.AntiAliasing = AntiAliasing;
			FFieldSceneDraw Both;
			BuildFieldFrameDraw(Test.Scene, Test.Frame, nullptr, 1.0f, Mesh, Options, Both);
			Weltall::FTexelKindCache Kinds;
			Kinds.Follow(Vram);
			FFieldSceneDraw Needed;
			BuildFieldFrameDraw(Test.Scene, Test.Frame, nullptr, 1.0f, Mesh, Options, Needed, &Kinds);
			// Every particle drawn twice; then the mixed one alone, the blended pair as one run, the plain one opaque.
			TestEqual(Case + TEXT(": plain passes without kinds"), Passes(Both, 1), 4);
			TestEqual(Case + TEXT(": plain passes"), Passes(Needed, 1), 1);
			TestEqual(Case + TEXT(": blended runs"), Passes(Needed, 2), Passes(Both, 2) - 2);
			TestEqual(Case + TEXT(": opaque runs"), Passes(Needed, 0), Passes(Both, 0) + 1);
			const TArray<FColor> Expected = RenderFieldSceneOffscreen(Mesh, Units, Both, FIntPoint(ScreenWidth, ScreenHeight) * 2);
			const TArray<FColor> Drawn = RenderFieldSceneOffscreen(Mesh, Units, Needed, FIntPoint(ScreenWidth, ScreenHeight) * 2);
			TestTrue(Case + TEXT(": drawn"), Expected.Num() == ScreenWidth * ScreenHeight * 4 && Drawn.Num() == Expected.Num());
			TestTrue(Case + TEXT(": the same pixels"), Drawn == Expected);
			// The pair's overlap, beside the sprite: the second's texels added over the first's.
			const int32 Overlap = 44 * ScreenWidth * 2 + 54;
			TestTrue(Case + TEXT(": the blended pair drawn"), Expected.IsValidIndex(Overlap) && Expected[Overlap] != FColor(64, 64, 64));
		}
	}
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldModelChangesRenderingTest, "Weltall.Rendering.FieldModelChangesFollowTheGpusRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldModelChangesRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// A raw quad whose texels the scripts scroll one on, and a raw quad in fog, modulated by its colour.
	FTestFrame Scrolled;
	Scrolled.Quad(20, 20, 36, 24, RawQuad);
	Scrolled.Quad(20, 40, 36, 44, RawQuad);
	Scrolled.Evaluate();
	Scrolled.Frame.ScrolledModels.push_back({0, 1, 0});
	Scrolled.Frame.Scenery.Polygons[1].bFogged = true;
	Scrolled.Frame.Scenery.Polygons[1].Colors.fill({64, 64, 64});
	const TArray<FColor> Pixels = Scrolled.Render(Units);
	// A MIMe animation moving the quad's vertices 16 pixels right from one tick to the next: drawn half way.
	FTestFrame Rest;
	Rest.Quad(20, 60, 36, 64, RawQuad);
	Rest.Evaluate();
	FTestFrame Moved;
	Moved.Quad(20, 60, 36, 64, RawQuad);
	Moved.Evaluate();
	for (FTestFrame* Frame : {&Rest, &Moved})
	{
		const std::vector<Weltall::FGteVector>& Vertices = Frame->Scene.Models.front().Parts.front().Vertices;
		Frame->Frame.MovedModels.push_back({0, 0, static_cast<uint32>(Vertices.size())});
		Frame->Frame.Vertices = Vertices;
	}
	for (Weltall::FGteVector& Vertex : Moved.Frame.Vertices)
	{
		Vertex[0] = static_cast<int16>(Vertex[0] + 32);
	}
	const TArray<FColor> Between = Moved.Render(Units, &Rest, 0.5f);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Pixels.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Between.Num(), Pixels.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	Near(Pixels, TEXT("scrolled a texel on: the first pixel shows red"), 20, 21, 255, 0, 0);
	Near(Pixels, TEXT("scrolled: the fourth wraps to texel 0 of the next four"), 23, 21, 64, 64, 64);
	Near(Pixels, TEXT("in fog, a raw texel modulated by the fog's colour"), 20, 41, 127, 0, 0);
	Near(Between, TEXT("moved vertices half way: its first texel's place"), 28, 61, 64, 64, 64);
	Near(Between, TEXT("moved vertices half way: red"), 29, 61, 255, 0, 0);
	Near(Between, TEXT("moved vertices half way: left behind"), 21, 61, 64, 64, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldScreenWaveRenderingTest, "Weltall.Rendering.FieldScreenWaveRedrawsTheScene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldScreenWaveRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// A red square in the far entries, the wave moving its cell 4 pixels right; and a blue square in entry 0, drawn over the wave.
	FTestFrame Waved;
	Waved.Quad(32, 32, 48, 48, FlatQuad, {255, 0, 0});
	Waved.Quad(96, 32, 112, 48, FlatQuad, {0, 0, 255});
	Waved.Evaluate();
	Waved.Frame.Scenery.Polygons[1].Bucket = 0;
	std::stable_partition(Waved.Frame.Order.begin(), Waved.Frame.Order.end(), [&Waved](const Weltall::FFieldDrawItem& Item) { return Waved.Frame.Scenery.Polygons[Item.Index].Bucket > 1; });
	Waved.Frame.bScreenWave = true;
	for (int32 Row = 0; Row < Weltall::FFieldScreenWave::Rows; ++Row)
	{
		for (int32 Column = 0; Column < Weltall::FFieldScreenWave::Columns; ++Column)
		{
			Weltall::FFieldScreenWaveCell& Cell = Waved.Frame.Wave[static_cast<std::size_t>(Row * Weltall::FFieldScreenWave::Columns + Column)];
			const int32 Shift = Row == 2 && Column == 2 ? 4 : Row == 2 && Column == 3 ? 4 : 0;
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const int32 Right = (Corner & 1) != 0 ? 1 : 0;
				const int32 Down = (Corner & 2) != 0 ? 1 : 0;
				const bool bMoved = (Column == 2 && Right == 1) || (Column == 3 && Right == 0);
				Cell.Corners[static_cast<std::size_t>(Corner)] = {16 * (Column + Right) + (bMoved ? Shift : 0), 16 * (Row + Down)};
			}
		}
	}
	const TArray<FColor> Pixels = Waved.Render(Units);
	// The scripts' drawing area narrowed to the screen's left half: the rest only the clear colour.
	FTestFrame Clipped;
	Clipped.Quad(0, 0, 320, 224, FlatQuad, {255, 255, 255});
	Clipped.Evaluate();
	Clipped.Frame.DrawClip = {0, 0, 160, 224};
	const TArray<FColor> Clip = Clipped.Render(Units);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Pixels.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Clip.Num(), Pixels.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	// Cell (2, 2) stretched 4 pixels wider and cell (2, 3) squeezed: the square's right edge moved from 48 to about 51.
	Near(Pixels, TEXT("the wave stretches the square's cell"), 49, 40, 255, 0, 0);
	Near(Pixels, TEXT("the square's other rows stay put"), 49, 50, 64, 64, 64);
	Near(Pixels, TEXT("a cell the wave leaves still"), 34, 50, 64, 64, 64);
	Near(Pixels, TEXT("the nearest entries are drawn over the wave, undisturbed"), 111, 40, 0, 0, 255);
	Near(Clip, TEXT("inside the drawing area"), 100, 100, 255, 255, 255);
	Near(Clip, TEXT("outside it, the clear colour"), 200, 100, 64, 64, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldSceneryDepthRenderingTest, "Weltall.Rendering.FieldSceneryByDepthKeepsItsDecals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldSceneryDepthRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// The grey ground 1000 away over the whole screen; a green decal 2 units behind it, as the maps lay theirs a hair
	// under the ground they show on; a blue wall 500 behind it and a red one 300 before it, the order linking the red
	// first, then the ground, the decal and the blue.
	FTestFrame Test;
	Test.Quad(0, 0, 320, 224, FlatQuad, {128, 128, 128});
	Test.Quad(20, 20, 60, 60, FlatQuad, {0, 255, 0});
	Test.Quad(90, 50, 140, 80, FlatQuad, {0, 0, 255});
	Test.Quad(200, 50, 240, 80, FlatQuad, {255, 0, 0});
	std::vector<Weltall::FGteVector>& Vertices = Test.Scene.Models.front().Parts.front().Vertices;
	constexpr int32 Depths[] = {0, 2, 500, -300};
	for (std::size_t Vertex = 0; Vertex < Vertices.size(); ++Vertex)
	{
		const int32 Depth = Depths[Vertex / 4];
		Vertices[Vertex] = {static_cast<int16>(Vertices[Vertex][0] * (1000 + Depth) / 1000), static_cast<int16>(Vertices[Vertex][1] * (1000 + Depth) / 1000),
			static_cast<int16>(Depth)};
	}
	Test.Evaluate();
	Test.Frame.Order = {{3, Weltall::EFieldDrawKind::Scenery}, {0, Weltall::EFieldDrawKind::Scenery}, {1, Weltall::EFieldDrawKind::Scenery},
		{2, Weltall::EFieldDrawKind::Scenery}};
	const TArray<FColor> Buffered = Test.Render(Units, nullptr, 1.0f, EFieldSorting::DepthBuffer);
	const TArray<FColor> Sorted = Test.Render(Units, nullptr, 1.0f, EFieldSorting::DepthBufferWithScenery);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Buffered.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Sorted.Num(), Buffered.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	Near(Buffered, TEXT("the scenery in its order: the far wall linked last over the ground"), 115, 65, 0, 0, 255);
	Near(Buffered, TEXT("the scenery in its order: the ground over the near wall linked first"), 220, 65, 128, 128, 128);
	Near(Sorted, TEXT("by depth: the ground in front of the far wall"), 115, 65, 128, 128, 128);
	Near(Sorted, TEXT("by depth: the near wall in front of the ground"), 220, 65, 255, 0, 0);
	Near(Sorted, TEXT("by depth: within the decals' margin the order decides, the decal over its ground"), 40, 40, 0, 255, 0);
	Near(Sorted, TEXT("by depth: the ground elsewhere"), 160, 150, 128, 128, 128);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldDepthSortingRenderingTest, "Weltall.Rendering.FieldDepthBufferSortsTheSpritesAndKeepsTheScenery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldDepthSortingRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// A green decal lying just behind the grey ground it is drawn over, as the maps' stones lie in theirs;
	// a sprite 1000 units away and a blue wall 1500 away that the ordering table draws after it; the
	// sprite's shadow, linked after it, overlapping its lower rows; a sprite of two parts, the first
	// nearer than the second, which is drawn over it; and a third sprite drawn after a yellow pillar
	// 100 units before it and a magenta wall 300 units before it.
	FTestFrame Test;
	Test.Sprite(100, 60, 1000);
	Test.Sprite(100, 84, 1000, true);
	Test.Sprite(150, 100, 750);
	Test.Part(154, 100, 1000, 750 / 16 - 2);
	Test.Sprite(200, 60, 1000);
	Test.Quad(0, 0, 320, 224, FlatQuad, {128, 128, 128});
	Test.Quad(20, 20, 60, 60, FlatQuad, {0, 255, 0});
	Test.Quad(90, 50, 140, 80, FlatQuad, {0, 0, 255});
	Test.Quad(202, 55, 210, 95, FlatQuad, {255, 255, 0});
	Test.Quad(210, 55, 220, 95, FlatQuad, {255, 0, 255});
	std::vector<Weltall::FGteVector>& Vertices = Test.Scene.Models.front().Parts.front().Vertices;
	// The ground 1000 away, the decal a hair behind it, the wall at 1500, the pillar at 900 and the
	// magenta wall at 700, each where it covers the same pixels.
	constexpr int32 Depths[] = {0, 2, 500, -100, -300};
	for (std::size_t Vertex = 0; Vertex < Vertices.size(); ++Vertex)
	{
		const int32 Depth = Depths[Vertex / 4];
		Vertices[Vertex] = {static_cast<int16>(Vertices[Vertex][0] * (1000 + Depth) / 1000), static_cast<int16>(Vertices[Vertex][1] * (1000 + Depth) / 1000),
			static_cast<int16>(Depth)};
	}
	Test.Evaluate();
	// The order the tables link them in: the ground, the decal, the pillar and the magenta wall, the
	// sprite, its shadow, the parts, the third sprite, then the blue wall.
	Test.Frame.Order = {{0, Weltall::EFieldDrawKind::Scenery}, {1, Weltall::EFieldDrawKind::Scenery}, {3, Weltall::EFieldDrawKind::Scenery},
		{4, Weltall::EFieldDrawKind::Scenery}, {0, Weltall::EFieldDrawKind::Character}, {1, Weltall::EFieldDrawKind::Character},
		{2, Weltall::EFieldDrawKind::Character}, {3, Weltall::EFieldDrawKind::Character}, {4, Weltall::EFieldDrawKind::Character},
		{2, Weltall::EFieldDrawKind::Scenery}};
	const TArray<FColor> Painted = Test.Render(Units, nullptr, 1.0f, EFieldSorting::PaintersOrder);
	const TArray<FColor> Buffered = Test.Render(Units, nullptr, 1.0f, EFieldSorting::DepthBuffer);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Painted.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Buffered.Num(), Painted.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	// The sprite's 4 texels, 4 pixels each from x 100: transparent, red, green, white.
	Near(Painted, TEXT("painter's order: the decal over its ground"), 40, 40, 0, 255, 0);
	Near(Buffered, TEXT("depth buffer: the decal still over its ground"), 40, 40, 0, 255, 0);
	Near(Painted, TEXT("painter's order: the wall linked later over the sprite"), 105, 70, 0, 0, 255);
	Near(Buffered, TEXT("depth buffer: the sprite in front of the farther wall"), 105, 70, 255, 0, 0);
	Near(Buffered, TEXT("depth buffer: the wall through the sprite's texel 0"), 101, 70, 0, 0, 255);
	Near(Buffered, TEXT("depth buffer: the wall beside the sprite"), 130, 70, 0, 0, 255);
	// The shadow's semi-transparent texel (green) is subtracted; its others are opaque.
	Near(Painted, TEXT("painter's order: the shadow linked later, subtracted from the sprite"), 109, 88, 0, 0, 0);
	Near(Buffered, TEXT("depth buffer: the shadow never over its sprite"), 109, 88, 0, 255, 0);
	Near(Buffered, TEXT("depth buffer: the shadow below the sprite, subtracted from the ground"), 109, 100, 128, 0, 128);
	// Scenery drawn before a sprite hides it only when clearly nearer: not the pillar, 100 units before
	// it, but the wall 300 units before it.
	Near(Painted, TEXT("painter's order: the sprite over the pillar drawn before it"), 205, 70, 255, 0, 0);
	Near(Buffered, TEXT("depth buffer: the sprite still over the pillar close before it"), 205, 70, 255, 0, 0);
	Near(Painted, TEXT("painter's order: the sprite over the wall drawn before it"), 213, 70, 255, 255, 255);
	Near(Buffered, TEXT("depth buffer: the wall well before the sprite hides it"), 213, 70, 255, 0, 255);
	// Where the parts overlap, the first's green lies under the second's red.
	Near(Painted, TEXT("painter's order: the later part over the nearer"), 159, 110, 255, 0, 0);
	Near(Buffered, TEXT("depth buffer: a sprite's parts still in their order"), 159, 110, 255, 0, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldShadowRenderingTest, "Weltall.Rendering.FieldShadowsHideBehindWhatStandsAboveTheirFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldShadowRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// A shadow on a floor below the eye (the view's down is the world's), 1000 units ahead; the ordering table draws
	// before it the grey ground behind, a blue wall over its left half 400 units ahead, standing well above its floor
	// along the eye's ray, and a yellow bump over its right half 800 units ahead, a little above it.
	FTestFrame Test;
	Test.Sprite(100, 180, 1000, true);
	Test.Quad(0, 0, 320, 224, FlatQuad, {128, 128, 128});
	Test.Quad(96, 170, 108, 220, FlatQuad, {0, 0, 255});
	Test.Quad(108, 170, 120, 220, FlatQuad, {255, 255, 0});
	std::vector<Weltall::FGteVector>& Vertices = Test.Scene.Models.front().Parts.front().Vertices;
	constexpr int32 Depths[] = {0, -600, -200};
	for (std::size_t Vertex = 0; Vertex < Vertices.size(); ++Vertex)
	{
		const int32 Depth = Depths[Vertex / 4];
		Vertices[Vertex] = {static_cast<int16>(Vertices[Vertex][0] * (1000 + Depth) / 1000), static_cast<int16>(Vertices[Vertex][1] * (1000 + Depth) / 1000),
			static_cast<int16>(Depth)};
	}
	Test.Evaluate();
	Test.Frame.Order = {{0, Weltall::EFieldDrawKind::Scenery}, {1, Weltall::EFieldDrawKind::Scenery}, {2, Weltall::EFieldDrawKind::Scenery},
		{0, Weltall::EFieldDrawKind::Character}};
	const TArray<FColor> Painted = Test.Render(Units, nullptr, 1.0f, EFieldSorting::PaintersOrder);
	const TArray<FColor> Buffered = Test.Render(Units, nullptr, 1.0f, EFieldSorting::DepthBuffer);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Painted.Num(), ScreenWidth * ScreenHeight) || !TestEqual(TEXT("both"), Buffered.Num(), Painted.Num()))
	{
		return false;
	}
	const auto Near = [this](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * ScreenWidth + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	// The shadow's 4 texels, 4 pixels each from x 100: transparent, red, green, white.
	Near(Painted, TEXT("painter's order: the shadow over the wall drawn before it"), 105, 195, 255, 0, 0);
	Near(Buffered, TEXT("depth buffer: the wall standing above the shadow's floor hides it"), 105, 195, 0, 0, 255);
	Near(Painted, TEXT("painter's order: the shadow over the bump drawn before it"), 113, 195, 255, 255, 255);
	Near(Buffered, TEXT("depth buffer: the shadow still over the bump close above its floor"), 113, 195, 255, 255, 255);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldPictureRenderingTest, "Weltall.Rendering.FieldPictureOfTheScreenIsDrawnAsTheFieldDrawsIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldPictureRenderingTest::RunTest(const FString&)
{
	// A picture whose left half is red and right half black, drawn over a green target: added, opaque
	// (its black letting the green through), at half brightness, and scaled twice about the centre.
	const FIntPoint Size(64, 32);
	const auto Draw = [Size](EFieldPictureBlend Blend, float ColorScale, float Scale)
	{
		return RenderPassesOffscreen(Size, [Size, Blend, ColorScale, Scale](FRDGBuilder& GraphBuilder, FRDGTextureRef Output)
		{
			FRDGTextureRef Picture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("TestPicture"));
			AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Black, FIntRect(FIntPoint::ZeroValue, Size));
			AddClearRenderTargetPass(GraphBuilder, Picture, FLinearColor::Red, FIntRect(0, 0, Size.X / 2, Size.Y));
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor(0.0f, 0.25f, 0.0f), FIntRect(FIntPoint::ZeroValue, Size));
			TArray<FVector4f, TInlineAllocator<6>> Quad;
			const FVector4f Corners[4] = {FVector4f(-1.0f, -1.0f, 0.0f, 0.0f), FVector4f(1.0f, -1.0f, 1.0f, 0.0f), FVector4f(-1.0f, 1.0f, 0.0f, 1.0f),
				FVector4f(1.0f, 1.0f, 1.0f, 1.0f)};
			for (const int32 Corner : {0, 1, 2, 1, 3, 2})
			{
				Quad.Add(Corners[Corner]);
			}
			FFieldPictureDraw Drawn;
			Drawn.Vertices = Quad;
			Drawn.Placement = FVector4f(Size.X * 0.5f * Scale, Size.Y * 0.5f * Scale, Size.X * 0.5f, Size.Y * 0.5f);
			Drawn.ColorScale = ColorScale;
			Drawn.Blend = Blend;
			AddFieldPicturePass(GraphBuilder, Picture, Output, Drawn);
		});
	};
	const TArray<FColor> Added = Draw(EFieldPictureBlend::Add, 1.0f, 1.0f);
	const TArray<FColor> Opaque = Draw(EFieldPictureBlend::OpaqueBlackTransparent, 1.0f, 1.0f);
	const TArray<FColor> Dimmed = Draw(EFieldPictureBlend::Opaque, 0.5f, 1.0f);
	const TArray<FColor> Zoomed = Draw(EFieldPictureBlend::Opaque, 1.0f, 2.0f);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frames were read back"), Added.Num(), Size.X * Size.Y))
	{
		return false;
	}
	const auto Near = [this, Size](const TArray<FColor>& Frame, const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Frame[Y * Size.X + X];
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), IsNear(Pixel, R, G, B));
	};
	Near(Added, TEXT("added: red over green"), 8, 16, 255, 64, 0);
	Near(Added, TEXT("added: black adds nothing"), 56, 16, 0, 64, 0);
	Near(Opaque, TEXT("opaque: red"), 8, 16, 255, 0, 0);
	Near(Opaque, TEXT("opaque: its black lets the green through"), 56, 16, 0, 64, 0);
	Near(Dimmed, TEXT("half brightness"), 8, 16, 127, 0, 0);
	Near(Zoomed, TEXT("scaled twice: the red half covers the left"), 2, 16, 255, 0, 0);
	Near(Zoomed, TEXT("scaled twice: the black half from the centre on"), 40, 16, 0, 0, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFieldViewMoveTest, "Weltall.Rendering.FieldFrameMovesToAnotherView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFieldViewMoveTest::RunTest(const FString&)
{
	// From turns the world a quarter about the vertical and scales it by 3; To only moves it. A point the frame placed
	// in From's view space is drawn where To sees the same point of the world.
	Weltall::FGteMatrix From;
	From.M = {{{0, 0, 3 * Weltall::FGteMatrix::One}, {0, 3 * Weltall::FGteMatrix::One, 0}, {-3 * Weltall::FGteMatrix::One, 0, 0}}};
	From.T = {10, -20, 1500};
	Weltall::FGteMatrix To = Weltall::FGteMatrix::Identity();
	To.T = {-40, 5, 900};
	const auto Through = [](const Weltall::FGteMatrix& Matrix, const FVector3f& Point)
	{
		FVector3f Out;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			Out[Row] = (Matrix.M[Row][0] * Point.X + Matrix.M[Row][1] * Point.Y + Matrix.M[Row][2] * Point.Z) / Weltall::FGteMatrix::One + Matrix.T[Row];
		}
		return Out;
	};
	const FVector3f World(100.0f, 50.0f, -30.0f);
	const FVector3f InFrom = Through(From, World);
	FFieldSceneDraw Draw;
	Draw.Objects.Emplace(1.0f, 0.0f, 0.0f, 0.0f);
	Draw.Objects.Emplace(0.0f, 1.0f, 0.0f, 0.0f);
	Draw.Objects.Emplace(0.0f, 0.0f, 1.0f, 0.0f);
	MoveFieldSceneView(Draw, Weltall::FAffine::From(From), Weltall::FAffine::From(To));
	const FVector3f Expected = Through(To, World);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const FVector4f& Rows = Draw.Objects[Row];
		const float Drawn = Rows.X * InFrom.X + Rows.Y * InFrom.Y + Rows.Z * InFrom.Z + Rows.W;
		TestTrue(FString::Printf(TEXT("axis %d at %f, not %f"), Row, Drawn, Expected[Row]), FMath::IsNearlyEqual(Drawn, Expected[Row], 0.01f));
	}

	FTestFrame Field;
	Field.Sprite(140, 120, 1000);
	Field.Part(156, 120, 1000, 60);
	Field.Sprite(140, 120, 1000, true);
	Field.Frame.Characters.front().Origin = {-40.0f, 16.0f, 1000.0f};
	Field.Evaluate();
	const FFieldSceneMesh Mesh(Field.Scene);
	BuildFieldFrameDraw(Field.Scene, Field.Frame, nullptr, 1.0f, Mesh, {}, Draw);
	const auto CornerPosition = [&Draw](int32 Index)
	{
		const FFieldSceneCorner& Corner = Draw.DynamicCorners[Index];
		FVector3f Position;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const FVector4f& Row = Draw.Objects[3 * Corner.Object + Axis];
			Position[Axis] = FVector3f::DotProduct(FVector3f(Row), Corner.Position) + Row.W;
		}
		return Position;
	};
	const FVector3f Origin = CornerPosition(0);
	const FVector3f FarCorner = CornerPosition(11);
	const FVector3f ShadowCorner = CornerPosition(17);
	const Weltall::FGteMatrix Identity = Weltall::FGteMatrix::Identity();
	MoveFieldSceneView(Draw, Weltall::FAffine::From(Identity), Weltall::FAffine::From(From));
	TestTrue(TEXT("The sprite follows its base into the new view"), CornerPosition(0).Equals(Through(From, Origin)));
	TestTrue(TEXT("Both sprite parts keep their screen orientation"), (CornerPosition(11) - CornerPosition(0)).Equals(FarCorner - Origin));
	TestTrue(TEXT("The shadow turns with the floor"), CornerPosition(17).Equals(Through(From, ShadowCorner)));
	return true;
}

#endif
