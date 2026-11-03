#include "Battle/Show/BattleFrameBlend.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Battle/Show/BattleShowTransition.h"
#include "Field/FieldScene.h"
#include "Gears/GearPackets.h"
#include "Graphics/GteMath.h"
#include "Graphics/MdlPackets.h"
#include "Graphics/Vram.h"
#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "RenderingThread.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Rendering/BattleRendering.h"
#include "Rendering/BattleTransitionRendering.h"
#include "Rendering/FieldSceneRendering.h"
#include "Rendering/VramTexture.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 BattleScreenWidth = 320;
	constexpr int32 BattleScreenHeight = 224;
	/** The battle's projection: its distance, and its centre below the screen's middle. */
	constexpr int32 BattleDistance = 512;
	constexpr int32 BattleCentreX = 160;
	constexpr int32 BattleCentreY = 164;
	/** Where the quads stand ahead of the eye: every view unit two pixels' worth of the screen's. */
	constexpr int32 BattleDepth = 1024;
	/** A 4-bit texture page at VRAM (0, 0) and its CLUT at (0, 256). */
	constexpr uint16 BattlePage = 0;
	constexpr uint16 BattleClut = 256 << 6;
	/** A sprite's GPU code with raw texels. */
	constexpr uint32 BattleRawSprite = 0x2D000000;
	constexpr uint32 BattleOpaqueGouraudTriangle = 0x30000000;
	/** A tile's and a dot's GPU codes: a rectangle of any size, opaque or semi-transparent, and one of a pixel. */
	constexpr uint32 BattleOpaqueTile = 0x60000000;
	constexpr uint32 BattleBlendedTile = 0x62000000;
	constexpr uint32 BattleOpaqueDot = 0x68000000;
	/** A model's GPU codes: a triangle of raw texels, a flat quad that blends. */
	constexpr uint32 BattleRawModelTriangle = 0x25000000;
	constexpr uint32 BattleBlendedModelQuad = 0x2A000000;

	/** Two rows of a 4-bit texture: 16 texels of colour 1 then 16 of colour 2; the CLUT: transparent, red, green. */
	Weltall::FVram MakeBattleVram()
	{
		Weltall::FVram Vram;
		for (int16 Row = 0; Row < 32; ++Row)
		{
			Vram.Upload({{0, Row, 8, 1}, {0x1111, 0x1111, 0x1111, 0x1111, 0x2222, 0x2222, 0x2222, 0x2222}});
		}
		Vram.Upload({{0, 256, 3, 1}, {0x0000, 0x001F, 0x03E0}});
		return Vram;
	}

	/** A point of the original's screen as the battle's GTE put it there from the depth ahead: its view offset from the eye. */
	Weltall::FGteVector BattleViewAt(int32 X, int32 Y)
	{
		return {static_cast<int16>((X - BattleCentreX) * BattleDepth / BattleDistance), static_cast<int16>((Y - BattleCentreY) * BattleDepth / BattleDistance), 0};
	}

	/** A quad of the frame's list over the screen's corners given (top left, top right, bottom left, bottom right), its texels U0-U1 of the page. */
	void AddBattleQuad(Weltall::FBattleShowFrame& Frame, Weltall::EBattleShowDrawer Kind, const std::array<std::array<int32, 2>, 4>& Screen, int16 U0, int16 U1)
	{
		Weltall::FBattleShowQuad& Entry = Frame.Quads.emplace_back();
		Entry.Drawer = static_cast<uint16>(Frame.Drawers.size());
		Frame.Drawers.push_back({static_cast<uint32>(Frame.Drawers.size() + 1), Kind, {}});
		Weltall::FSpriteQuad& Quad = Entry.Quad;
		Quad.Matrix = Weltall::FGteMatrix::Identity();
		Quad.Matrix.T = {0, 0, BattleDepth};
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Quad.Corners[Corner] = BattleViewAt(Screen[Corner][0], Screen[Corner][1]);
		}
		Quad.U0 = U0;
		Quad.U1 = U1;
		Quad.V0 = 0;
		Quad.V1 = 16;
		Quad.TexturePage = BattlePage;
		Quad.Clut = BattleClut;
		Quad.ColorCode = BattleRawSprite | 0x808080;
		Quad.OtIndex = 100;
		Frame.Order.push_back({Weltall::EBattleShowLink::Quad, static_cast<uint32>(Frame.Quads.size() - 1), Quad.OtIndex});
	}

	/** A packet of the stage's dome: a band across the screen, or a textured cell over the corners given. */
	Weltall::FBattleDomePacket BattleDomeBand(int16 Top, int16 Bottom, const std::array<uint8, 3>& Colour)
	{
		Weltall::FBattleDomePacket Packet;
		Packet.Quad.Kind = Weltall::FFieldBackdropQuad::EKind::Graded;
		Packet.Quad.Corners = {{{0, Top}, {320, Top}, {0, Bottom}, {320, Bottom}}};
		Packet.Quad.Colors = {Colour, Colour, Colour, Colour};
		return Packet;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleRenderingTest, "Weltall.Rendering.BattleFollowsTheOriginalsScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeBattleVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	Weltall::FBattleShowFrame Frame;
	Frame.CentreX = BattleCentreX;
	Frame.CentreY = BattleCentreY;
	Frame.ProjectionDistance = BattleDistance;
	Frame.bCleared = true;
	Frame.ClearColor = {0, 0, 64};
	// A sprite of red texels where the original's GTE put it, about its centre below the screen's middle.
	AddBattleQuad(Frame, Weltall::EBattleShowDrawer::Sprite, {{{40, 120}, {80, 120}, {40, 180}, {80, 180}}}, 0, 16);
	// A shadow of green texels whose top corners stand at 90 and 100 lines: once projected both take 95, and the bottom ones 120.
	AddBattleQuad(Frame, Weltall::EBattleShowDrawer::Shadow, {{{200, 90}, {260, 100}, {200, 120}, {260, 120}}}, 16, 32);
	// A target marker's triangle, its corners' colours blue.
	Weltall::FBattleShowTriangle& Marker = Frame.Triangles.emplace_back();
	Marker.Matrix = Weltall::FGteMatrix::Identity();
	Marker.Matrix.T = {0, 0, BattleDepth};
	Marker.Corners = {BattleViewAt(120, 20), BattleViewAt(180, 20), BattleViewAt(150, 60)};
	Marker.ColorCodes = {BattleOpaqueGouraudTriangle | 0xFF0000, 0xFF0000, 0xFF0000};
	Marker.Drawer = static_cast<uint16>(Frame.Drawers.size());
	Frame.Drawers.push_back({99, Weltall::EBattleShowDrawer::Marker, {}});
	Frame.Order.push_back({Weltall::EBattleShowLink::Triangle, 0, 10});
	// A spawned sprite's square, eight pixels across about its place, green; another added to what lies behind it; a dot.
	Frame.View = Weltall::FGteMatrix::Identity();
	Frame.View.T = {0, 0, BattleDepth};
	const auto AddTile = [&Frame](int32 X, int32 Y, int16 Size, bool bDot, uint32 ColorCode, uint8 Blend)
	{
		Weltall::FBattleShowTile& Tile = Frame.Tiles.emplace_back();
		Tile.Place = BattleViewAt(X, Y);
		Tile.Size = Size;
		Tile.bDot = bDot;
		Tile.ColorCode = ColorCode;
		Tile.Blend = Blend;
		Frame.Order.push_back({Weltall::EBattleShowLink::Tile, static_cast<uint32>(Frame.Tiles.size() - 1), 20});
	};
	AddTile(20, 60, 8 * BattleDepth / BattleDistance, false, BattleOpaqueTile | 0x00FF00, 0);
	AddTile(40, 60, 8 * BattleDepth / BattleDistance, false, BattleBlendedTile | 0x000064, 1);
	AddTile(60, 60, 0, true, BattleOpaqueDot | 0xFFFFFF, 0);
	// A sprite's model projected about the screen's middle, not the battle's centre: a triangle of the page's green texels,
	// and a flat quad added to what lies behind it by the drawing mode before it.
	const auto ModelAt = [](int32 X, int32 Y)
	{
		return Weltall::FGteVector{static_cast<int16>((X - BattleScreenWidth / 2) * BattleDepth / BattleDistance),
			static_cast<int16>((Y - BattleScreenHeight / 2) * BattleDepth / BattleDistance), 0};
	};
	Weltall::FBattleShowModel& Model = Frame.Models.emplace_back();
	Model.Matrix = Weltall::FGteMatrix::Identity();
	Model.Matrix.T = {0, 0, BattleDepth};
	Model.CentreX = BattleScreenWidth / 2;
	Model.CentreY = BattleScreenHeight / 2;
	Weltall::FBattleShowModelPolygon& Textured = Frame.ModelPolygons.emplace_back();
	Textured.bTextured = true;
	Textured.Corners = {ModelAt(100, 185), ModelAt(140, 185), ModelAt(100, 200)};
	Textured.Uvs = {{{16, 0}, {31, 0}, {16, 15}}};
	Textured.ColorCodes[0] = BattleRawModelTriangle | 0x808080;
	Textured.TexturePage = BattlePage;
	Textured.Clut = BattleClut;
	Frame.Order.push_back({Weltall::EBattleShowLink::ModelPolygon, 0, 30});
	Weltall::FBattleShowModelPolygon& Added = Frame.ModelPolygons.emplace_back();
	Added.CornerCount = 4;
	Added.Corners = {ModelAt(150, 185), ModelAt(170, 185), ModelAt(150, 200), ModelAt(170, 200)};
	Added.ColorCodes[0] = BattleBlendedModelQuad | 0x000064;
	Added.Blend = 1;
	Frame.Order.push_back({Weltall::EBattleShowLink::ModelPolygon, 1, 30});
	// The stage's backdrop behind everything: a sky band; a window of the page's first 16 texels, in which a cell's texels
	// 16-31 (green on their own) wrap to the red ones; a band added by the page the cell left (its blend mode 1).
	Weltall::FBattleDomePacket Window;
	Window.bDrawMode = true;
	Window.DrawPage = 1 << 5;
	Window.Window = {0, 0, 16, 16};
	Frame.Backdrop.Dome.push_back(Window);
	Frame.Backdrop.Dome.push_back(BattleDomeBand(0, 6, {0, 128, 0}));
	Weltall::FBattleDomePacket& Cell = Frame.Backdrop.Dome.emplace_back();
	Cell.Quad.Kind = Weltall::FFieldBackdropQuad::EKind::Textured;
	Cell.Quad.Corners = {{{240, 140}, {300, 140}, {240, 200}, {300, 200}}};
	Cell.Quad.Texels = {{{16, 0}, {31, 0}, {16, 15}, {31, 15}}};
	Cell.Quad.TexturePage = BattlePage | 1 << 5;
	Cell.Quad.Clut = BattleClut;
	Weltall::FBattleDomePacket Glow = BattleDomeBand(204, 224, {0, 0, 100});
	Glow.bSemiTransparent = true;
	Frame.Backdrop.Dome.push_back(Glow);
	Frame.Order.insert(Frame.Order.begin(), {Weltall::EBattleShowLink::Backdrop, 0, 0xFFF});

	const FFieldSceneMesh NoMesh{Weltall::FFieldScene{}};
	FFieldRenderOptions Options;
	Options.Sorting = EFieldSorting::PaintersOrder;
	Options.AntiAliasing = EFieldAntiAliasing::None;
	FBattleFrameDraw Draw;
	BuildBattleFrameDraw(Frame, nullptr, Options, Draw);
	const TArray<FColor> Pixels = RenderFieldSceneOffscreen(NoMesh, Units, Draw.Scene, FIntPoint(BattleScreenWidth, BattleScreenHeight));
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), BattleScreenWidth * BattleScreenHeight))
	{
		return false;
	}
	const auto Near = [this, &Pixels](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Pixels[Y * BattleScreenWidth + X];
		const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
	};
	const int32 Clear = 64;
	Near(TEXT("the clear colour"), 10, 10, 0, 0, Clear);
	Near(TEXT("inside the sprite"), 42, 122, 255, 0, 0);
	Near(TEXT("inside the sprite's far corner"), 77, 177, 255, 0, 0);
	Near(TEXT("above the sprite"), 60, 117, 0, 0, Clear);
	Near(TEXT("below the sprite"), 60, 183, 0, 0, Clear);
	Near(TEXT("the shadow's flattened top, by its first corner"), 204, 97, 0, 255, 0);
	Near(TEXT("and above it there"), 204, 92, 0, 0, Clear);
	Near(TEXT("by its second corner"), 256, 97, 0, 255, 0);
	Near(TEXT("the marker"), 150, 30, 0, 0, 255);
	Near(TEXT("the backdrop's sky band"), 10, 3, 0, 128, 0);
	Near(TEXT("its cell's texels wrapped in the window"), 270, 170, 255, 0, 0);
	Near(TEXT("its glow added to the clear colour"), 10, 212, 0, 0, Clear + 100);
	Near(TEXT("inside the square"), 20, 60, 0, 255, 0);
	Near(TEXT("beside the square"), 27, 60, 0, 0, Clear);
	Near(TEXT("above the square"), 20, 54, 0, 0, Clear);
	Near(TEXT("the square added to the clear colour"), 40, 60, 100, 0, Clear);
	Near(TEXT("the dot"), 60, 60, 255, 255, 255);
	Near(TEXT("beside the dot"), 62, 60, 0, 0, Clear);
	Near(TEXT("the model's textured triangle about the screen's middle"), 105, 188, 0, 255, 0);
	Near(TEXT("past the triangle's slanted edge"), 135, 197, 0, 0, Clear);
	Near(TEXT("the model's quad added to the clear colour"), 160, 192, 100, 0, Clear);
	Near(TEXT("below the model's quad"), 160, 202, 0, 0, Clear);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleLayersRenderingTest, "Weltall.Rendering.BattleDrawsItsLayersInTheOrderingTablesOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleLayersRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeBattleVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	Weltall::FBattleShowFrame Frame;
	Frame.CentreX = BattleCentreX;
	Frame.CentreY = BattleCentreY;
	Frame.ProjectionDistance = BattleDistance;
	Frame.bCleared = true;
	Frame.ClearColor = {0, 0, 64};
	const auto AddSprite = [&Frame](int32 Left, int32 Top, int16 U0, int32 Entry)
	{
		AddBattleQuad(Frame, Weltall::EBattleShowDrawer::Sprite, {{{Left, Top}, {Left + 40, Top}, {Left, Top + 40}, {Left + 40, Top + 40}}}, U0,
			static_cast<int16>(U0 + 16));
		Frame.Order.back().Entry = Entry;
	};
	// As the GPU draws the table: a green sprite far away, the fade adding red, a green sprite in the fade's entry linked
	// before it, the HUD, then a red number in the nearest entry.
	AddSprite(20, 20, 16, 100);
	Frame.Fade = {1, {100, 0, 0}};
	Frame.Order.push_back({Weltall::EBattleShowLink::Fade, 0, 2});
	AddSprite(90, 90, 16, 2);
	Frame.Order.push_back({Weltall::EBattleShowLink::Hud, 0, 1});
	AddSprite(180, 130, 0, 0);

	const FFieldSceneMesh NoMesh{Weltall::FFieldScene{}};
	FFieldRenderOptions Options;
	Options.AntiAliasing = EFieldAntiAliasing::None;
	bool bPassed = true;
	// Under either sorting: what lies over a layer follows the table.
	for (const EFieldSorting Sorting : {EFieldSorting::PaintersOrder, EFieldSorting::DepthBuffer})
	{
		Options.Sorting = Sorting;
		FBattleFrameDraw Draw;
		BuildBattleFrameDraw(Frame, nullptr, Options, Draw);
		// The HUD: an opaque blue panel under the number and over the second sprite's corner.
		const Weltall::FDrawColor Blue{0, 0, 255};
		Weltall::FDrawPrimitive Panel = Weltall::MakeGouraudRect({100, 100, 100, 50}, {Blue, Blue, Blue, Blue});
		Panel.bGouraud = false;
		Draw.Hud.Primitives.push_back(Panel);
		const FIntPoint Size(BattleScreenWidth, BattleScreenHeight);
		const TArray<FColor> Pixels = RenderPassesOffscreen(Size, [&NoMesh, &Units, &Draw, Size](FRDGBuilder& GraphBuilder, FRDGTextureRef Output)
		{
			AddBattleFramePasses(GraphBuilder, NoMesh, Units, Units, Draw, Output, FScreenArea(FIntRect(FIntPoint::ZeroValue, Size)));
		});
		if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), BattleScreenWidth * BattleScreenHeight))
		{
			bPassed = false;
			break;
		}
		const auto Near = [this, &Pixels](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
		{
			const FColor& Pixel = Pixels[Y * BattleScreenWidth + X];
			const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
			TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
		};
		Near(TEXT("the fade over the clear colour"), 300, 200, 100, 0, 64);
		Near(TEXT("the fade over the far sprite"), 40, 40, 100, 255, 0);
		Near(TEXT("the sprite linked before the fade, over it"), 95, 95, 0, 255, 0);
		Near(TEXT("the HUD over that sprite"), 110, 110, 0, 0, 255);
		Near(TEXT("the HUD over the fade"), 150, 110, 0, 0, 255);
		Near(TEXT("the number over the HUD"), 190, 140, 255, 0, 0);
		Near(TEXT("the number over the fade"), 210, 160, 255, 0, 0);

		// In a 4:3 picture's 240 lines, the scene, its fade and the HUD on the battle's 224 rows alike, eight lines down,
		// black above and below them.
		const FIntPoint Tall(BattleScreenWidth, 240);
		const TArray<FColor> InTall = RenderPassesOffscreen(Tall, [&NoMesh, &Units, &Draw, Tall](FRDGBuilder& GraphBuilder, FRDGTextureRef Output)
		{
			AddBattleFramePasses(GraphBuilder, NoMesh, Units, Units, Draw, Output, FScreenArea(FIntRect(FIntPoint::ZeroValue, Tall)));
		});
		if (!TestEqual(TEXT("The tall frame was read back"), InTall.Num(), Tall.X * Tall.Y))
		{
			bPassed = false;
			break;
		}
		const auto NearInTall = [this, &InTall, Tall](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
		{
			const FColor& Pixel = InTall[Y * Tall.X + X];
			const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
			TestTrue(FString::Printf(TEXT("In 240 lines, %s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
		};
		NearInTall(TEXT("above the rows"), 300, 7, 0, 0, 0);
		NearInTall(TEXT("the fade over the clear colour"), 300, 8, 100, 0, 64);
		NearInTall(TEXT("the sprite linked before the fade"), 95, 103, 0, 255, 0);
		NearInTall(TEXT("the HUD over that sprite"), 110, 118, 0, 0, 255);
		NearInTall(TEXT("the number over the HUD"), 190, 148, 255, 0, 0);
		NearInTall(TEXT("the fade's last row"), 300, 231, 100, 0, 64);
		NearInTall(TEXT("below the rows"), 300, 232, 0, 0, 0);
	}
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleFlashesRenderingTest, "Weltall.Rendering.BattleFlashesDrawAnEffectsFlashesAsChosen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleFlashesRenderingTest::RunTest(const FString&)
{
	// Fei's flash: an effect's fade adding (255, 204, 136); the battle's own subtracting fade stays as it is.
	Weltall::FBattleShowFrame Frame;
	Frame.Order.push_back({Weltall::EBattleShowLink::Fade, 0, 2});
	const auto Drawn = [&Frame](uint8 Mode, bool bFromEffect, Weltall::EBattleFlashes Flashes)
	{
		Frame.Fade = {Mode, {255, 204, 136}, bFromEffect};
		FFieldRenderOptions Options;
		Options.BattleFlashes = Flashes;
		FBattleFrameDraw Draw;
		BuildBattleFrameDraw(Frame, nullptr, Options, Draw);
		return Draw.Fade.Primitives.size() == 1 ? Draw.Fade.Primitives[0].Corners[0].Color : Weltall::FDrawColor{};
	};
	const auto Same = [this](const TCHAR* What, const Weltall::FDrawColor& Color, uint8 R, uint8 G, uint8 B)
	{
		TestEqual(What, FIntVector(Color.R, Color.G, Color.B), FIntVector(R, G, B));
	};
	Same(TEXT("an effect's flash, original"), Drawn(1, true, Weltall::EBattleFlashes::Original), 255, 204, 136);
	Same(TEXT("an effect's flash, softer"), Drawn(1, true, Weltall::EBattleFlashes::Softer), 64, 51, 34);
	Same(TEXT("an effect's flash, off"), Drawn(1, true, Weltall::EBattleFlashes::Off), 0, 0, 0);
	Same(TEXT("an effect's darkening, off"), Drawn(2, true, Weltall::EBattleFlashes::Off), 255, 204, 136);
	Same(TEXT("the battle's own fade, off"), Drawn(1, false, Weltall::EBattleFlashes::Off), 255, 204, 136);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleShatterRenderingTest, "Weltall.Rendering.BattleShatterBreaksThePictureOfTheScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleShatterRenderingTest::RunTest(const FString&)
{
	// A display twice as wide as the screen: 160 pixels of the picture to either side of it.
	const FIntPoint Size(2 * BattleScreenWidth, BattleScreenHeight);
	const FIntRect Area(FIntPoint::ZeroValue, Size);
	const FIntRect Patch(400, 64, 480, 128);
	const FLinearColor Ground = FColor(100, 50, 25).ReinterpretAsLinear();
	const FLinearColor Mark = FColor(20, 40, 60).ReinterpretAsLinear();

	Weltall::FPsxRandom Random(1);
	const Weltall::FGteArctangents Arctangents{};
	const TUniquePtr<Weltall::FBattleDefaultTransition> Transition = MakeUnique<Weltall::FBattleDefaultTransition>();
	Transition->Start(Random, Arctangents);
	FBattleTransitionDrawBuilder Builder;
	FBattleTransitionDraw Draw;
	Builder.Build(*Transition, 0, 1.0f, Draw);
	const int32 Shards = Weltall::FBattleShatter::Sides * Weltall::FBattleShatter::Rows
		* (Weltall::FBattleShatterWings::EndColumn - Weltall::FBattleShatterWings::FirstColumn);
	TestEqual(TEXT("Every shard is drawn before the picture breaks"), Draw.Vertices.Num(), 3 * Shards);

	const TArray<FColor> Pixels = RenderPassesOffscreen(Size, [&](FRDGBuilder& GraphBuilder, FRDGTextureRef Output)
	{
		FRDGTextureRef Picture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("ShatterTestPicture"));
		AddClearRenderTargetPass(GraphBuilder, Picture, Ground);
		AddClearRenderTargetPass(GraphBuilder, Picture, Mark, Patch);
		AddBattleTransitionPasses(GraphBuilder, Picture, Draw, Output, Area);
	});
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
	// Whole, the picture lies where it was, each shard on its own piece, at the shards' first colour: FFh, twice as bright less a part in 128.
	Near(TEXT("the picture in the left wing"), 10, 10, 199, 100, 50);
	Near(TEXT("the picture on the screen"), 320, 112, 199, 100, 50);
	Near(TEXT("the picture in the right wing"), 630, 200, 199, 100, 50);
	Near(TEXT("the mark where the picture has it"), 440, 100, 40, 80, 120);
	Near(TEXT("left of the mark"), 396, 100, 199, 100, 50);
	Near(TEXT("below the mark"), 440, 132, 199, 100, 50);

	// Half way to the first frame, the background has lost half of the frame's twelve.
	Transition->Frame();
	Builder.Build(*Transition, 1, 0.5f, Draw);
	TestTrue(TEXT("The background fades between the frames"), FMath::IsNearlyEqual(Draw.Background.R, (255.0f + 243.0f) / 2.0f / 255.0f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleSetupTransitionRenderingTest, "Weltall.Rendering.BattleSetupTransitionsDrawThePictureAsTheirMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleSetupTransitionRenderingTest::RunTest(const FString&)
{
	const FIntPoint Size(2 * BattleScreenWidth, BattleScreenHeight);
	const FIntRect Area(FIntPoint::ZeroValue, Size);
	const FIntRect Patch(400, 64, 480, 128);
	const FLinearColor Ground = FColor(100, 50, 24).ReinterpretAsLinear();
	const FLinearColor Mark = FColor(20, 40, 60).ReinterpretAsLinear();
	// Before its first step each mesh lies flat on the picture: the triangles add it to the black as it is, the wave
	// averages it with the black.
	struct FCase
	{
		Weltall::EBattleScreenMesh Kind;
		int32 Divisor;
	};
	for (const FCase& Case : {FCase{Weltall::EBattleScreenMesh::TrianglesStill, 1}, FCase{Weltall::EBattleScreenMesh::Wave, 2}})
	{
		Weltall::FBattleScreenMesh Mesh;
		Mesh.Create(Case.Kind);
		FBattleTransitionDrawBuilder Builder;
		FBattleTransitionDraw Draw;
		Builder.Build(Mesh, 0, 0, 1.0f, Draw);
		const TArray<FColor> Pixels = RenderPassesOffscreen(Size, [&](FRDGBuilder& GraphBuilder, FRDGTextureRef Output)
		{
			FRDGTextureRef Picture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("TransitionTestPicture"));
			AddClearRenderTargetPass(GraphBuilder, Picture, Ground);
			AddClearRenderTargetPass(GraphBuilder, Picture, Mark, Patch);
			AddBattleTransitionPasses(GraphBuilder, Picture, Draw, Output, Area);
		});
		if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), Size.X * Size.Y))
		{
			return false;
		}
		const auto Near = [this, &Pixels, Size, &Case](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
		{
			const FColor& Pixel = Pixels[Y * Size.X + X];
			const bool bNear = FMath::Abs(Pixel.R - R / Case.Divisor) <= 2 && FMath::Abs(Pixel.G - G / Case.Divisor) <= 2 && FMath::Abs(Pixel.B - B / Case.Divisor) <= 2;
			TestTrue(FString::Printf(TEXT("%s, a part in %d: (%d, %d, %d) near (%d, %d, %d)"), What, Case.Divisor, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
		};
		Near(TEXT("the picture in the left wing"), 10, 10, 100, 50, 24);
		Near(TEXT("the picture on the screen"), 320, 112, 100, 50, 24);
		Near(TEXT("the picture in the right wing"), 630, 200, 100, 50, 24);
		Near(TEXT("the mark where the picture has it"), 440, 100, 20, 40, 60);
		Near(TEXT("left of the mark"), 396, 100, 100, 50, 24);
	}

	// The triangles that come nearer: a step on, the picture is larger about the screen's middle and dimmer in the buffer drawn.
	Weltall::FBattleScreenMesh Mesh;
	Mesh.Create(Weltall::EBattleScreenMesh::Triangles);
	FBattleTransitionDrawBuilder Builder;
	FBattleTransitionDraw Draw;
	Builder.Build(Mesh, 0, 0, 1.0f, Draw);
	Mesh.Step(1);
	Builder.Build(Mesh, 1, 1, 1.0f, Draw);
	TestTrue(TEXT("The triangles' colour dims by three"), FMath::IsNearlyEqual(Draw.Shades[0], 125.0f / 128.0f, 0.0001f));
	float Left = TNumericLimits<float>::Max();
	for (const FVector4f& Vertex : Draw.Vertices)
	{
		Left = FMath::Min(Left, Vertex.X);
	}
	// The left edge of the left wing, 256 pixels past the screen's, from the screen's middle: larger by 2000h over 1FC0h.
	TestTrue(TEXT("The mesh comes nearer"), FMath::IsNearlyEqual(Left, 160.0f - (160.0f + 256.0f) * 8192.0f / 8128.0f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleBlendRenderingTest, "Weltall.Rendering.BattleIsDrawnBetweenTwoOfTheShowsFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleBlendRenderingTest::RunTest(const FString&)
{
	const Weltall::FVram Vram = MakeBattleVram();
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// A red sprite 80 pixels further right a frame later, over a clear colour that brightens.
	const auto MakeFrame = [](uint32 Step, int32 ShiftX, uint8 Blue)
	{
		Weltall::FBattleShowFrame Frame;
		Frame.Step = Step;
		Frame.View = Weltall::FGteMatrix::Identity();
		Frame.CentreX = BattleCentreX;
		Frame.CentreY = BattleCentreY;
		Frame.ProjectionDistance = BattleDistance;
		Frame.bCleared = true;
		Frame.ClearColor = {0, 0, Blue};
		AddBattleQuad(Frame, Weltall::EBattleShowDrawer::Sprite, {{{40, 120}, {80, 120}, {40, 180}, {80, 180}}}, 0, 16);
		Frame.Quads[0].Quad.Matrix.T[0] += ShiftX * BattleDepth / BattleDistance;
		return Frame;
	};
	const Weltall::FBattleShowFrame Before = MakeFrame(7, 0, 32);
	const Weltall::FBattleShowFrame Latest = MakeFrame(8, 80, 96);
	Weltall::FBattleFrameKeep BeforeKept;
	Weltall::FBattleFrameKeep LatestKept;
	Weltall::KeepBattleFrame(Before, BeforeKept);
	Weltall::KeepBattleFrame(Latest, LatestKept);
	Weltall::FBattleFrameBlend Blend;
	Weltall::FBattleFrameBlendScratch Scratch;
	Weltall::BlendBattleFrames(LatestKept, &BeforeKept, 0.5f, Blend, Scratch);

	const FFieldSceneMesh NoMesh{Weltall::FFieldScene{}};
	FFieldRenderOptions Options;
	Options.Sorting = EFieldSorting::PaintersOrder;
	Options.AntiAliasing = EFieldAntiAliasing::None;
	FBattleFrameDraw Draw;
	BuildBattleFrameDraw(Latest, &Blend, Options, Draw);
	const TArray<FColor> Pixels = RenderFieldSceneOffscreen(NoMesh, Units, Draw.Scene, FIntPoint(BattleScreenWidth, BattleScreenHeight));
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	if (!TestEqual(TEXT("The frame was read back"), Pixels.Num(), BattleScreenWidth * BattleScreenHeight))
	{
		return false;
	}
	const auto Near = [this, &Pixels](const TCHAR* What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Pixels[Y * BattleScreenWidth + X];
		const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
	};
	const int32 Clear = 64;
	Near(TEXT("the clear colour halfway"), 10, 10, 0, 0, Clear);
	Near(TEXT("the sprite halfway, by its left edge"), 82, 150, 255, 0, 0);
	Near(TEXT("by its right edge"), 118, 150, 255, 0, 0);
	Near(TEXT("not where the frame before had it"), 60, 150, 0, 0, Clear);
	Near(TEXT("nor where the latest has it"), 150, 150, 0, 0, Clear);
	return true;
}

namespace
{
	/** A flat untextured quad's GPU code, opaque. */
	constexpr uint8 BattleFlatQuadCode = 0x28;

	/** The 3D objects' part of a battle frame made of flat quads, each a part of its own, given in view space. */
	struct FBattleTestObjects
	{
		/** Room for every part made, so that the frame's pointers to them stay put. */
		static constexpr std::size_t Room = 8;

		FBattleTestObjects()
		{
			Parts.reserve(Room);
			Packets.reserve(Room);
		}

		/** A quad (its corners in the GPU's order: the last two across from the first two) of the object in Slot, linked next. */
		void AddQuad(Weltall::FBattleShowFrame& Frame, uint8 Slot, const std::array<Weltall::FGteVector, 4>& Corners, Weltall::FMdlColor Colour)
		{
			check(Parts.size() < Room);
			Weltall::FMdlPart& Part = Parts.emplace_back();
			Part.Vertices.assign(Corners.begin(), Corners.end());
			Weltall::FMdlPrimitive& Primitive = Part.Primitives.emplace_back();
			Primitive.Type = Weltall::EMdlPrimitiveType::FlatQuad;
			Primitive.Vertices = {0, 1, 2, 3};
			Weltall::FGearBonePackets& Built = Packets.emplace_back();
			Built.Packets.push_back({BattleFlatQuadCode, {Colour, Colour, Colour, Colour}});
			Built.Textures.emplace_back();
			Weltall::FBattleDrawnPart& Drawn = Objects.Parts.emplace_back();
			Drawn.Matrix = Weltall::FGteMatrix::Identity();
			Drawn.Part = &Part;
			Drawn.Packets = &Built;
			Drawn.Slot = Slot;
			Weltall::FBattleDrawnPolygon& Polygon = Objects.Polygons.emplace_back();
			Polygon.Part = static_cast<uint32>(Objects.Parts.size() - 1);
			Polygon.Colors[0] = Colour;
			Link(Frame, Weltall::EBattleObjectDraw::Polygon, Objects.Polygons.size() - 1);
		}

		/** Slot's floor shadow over the corners (+x, +z), (-x, +z), (+x, -z), (-x, -z), linked next. */
		void AddShadow(Weltall::FBattleShowFrame& Frame, uint8 Slot, const std::array<Weltall::FGteVector, 4>& Corners)
		{
			Weltall::FBattleDrawnShadow& Shadow = Objects.Shadows.emplace_back();
			Shadow.Matrix = Weltall::FGteMatrix::Identity();
			Shadow.Corners = Corners;
			Shadow.Colour = {128, 128, 128};
			Shadow.Slot = Slot;
			Link(Frame, Weltall::EBattleObjectDraw::Shadow, Objects.Shadows.size() - 1);
		}

		Weltall::FBattleObjectsFrame Objects;
		std::vector<Weltall::FMdlPart> Parts;
		std::vector<Weltall::FGearBonePackets> Packets;

	private:
		void Link(Weltall::FBattleShowFrame& Frame, Weltall::EBattleObjectDraw Kind, std::size_t Index)
		{
			Objects.Links.push_back({Kind, static_cast<uint32>(Index), 0});
			Frame.Order.push_back({Weltall::EBattleShowLink::Object, static_cast<uint32>(Objects.Links.size() - 1), 0});
			Frame.Objects = &Objects;
		}
	};

	/** A quad standing up to the eye at Depth, from Left to Right across and from Top to Bottom. */
	std::array<Weltall::FGteVector, 4> BattleStanding(int16 Left, int16 Right, int16 Top, int16 Bottom, int16 Depth)
	{
		return {{{Left, Top, Depth}, {Right, Top, Depth}, {Left, Bottom, Depth}, {Right, Bottom, Depth}}};
	}

	/** A quad lying at Height below the eye from Near to Far ahead, Left to Right across: its far corners first. */
	std::array<Weltall::FGteVector, 4> BattleLying(int16 Left, int16 Right, int16 Height, int16 Near, int16 Far)
	{
		return {{{Left, Height, Far}, {Right, Height, Far}, {Left, Height, Near}, {Right, Height, Near}}};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleDepthRenderingTest, "Weltall.Rendering.BattleGearsAreSortedAndStandInFrontOfTheirShadows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleDepthRenderingTest::RunTest(const FString&)
{
	// The sprite's red texels, and the objects' shadow: a 4-bit page at (896, 0) whose texels C0h-FEh take CLUT entry 1 at
	// (48, 460), a semi-transparent 64 of each channel, which the page subtracts.
	Weltall::FVram Vram = MakeBattleVram();
	for (int16 Row = 0xC0; Row <= 0xFE; ++Row)
	{
		Vram.Upload({{944, Row, 16, 1}, std::vector<uint16>(16, 0x1111)});
	}
	Vram.Upload({{48, 460, 2, 1}, {0x0000, 0xA108}});
	FVramTexture Units;
	BeginInitResource(&Units);
	Units.Follow(Vram);

	// The eye 100 units above the stage's floor, looking ahead; behind the battle's centre (160, 164) at 512.
	Weltall::FBattleShowFrame Frame;
	Frame.CentreX = BattleCentreX;
	Frame.CentreY = BattleCentreY;
	Frame.ProjectionDistance = BattleDistance;
	Frame.View = Weltall::FGteMatrix::Identity();
	Frame.bCleared = true;
	Frame.ClearColor = {0, 0, 64};
	FBattleTestObjects Objects;
	const uint8 Stage = static_cast<uint8>(Weltall::FBattleObjects::StageSlot);
	// The stage's grey floor, then a sprite 2000 away standing at its entry, then the stage's brown wall 2500 away over it on
	// the screen, linked after it.
	Objects.AddQuad(Frame, Stage, BattleLying(-4000, 4000, 100, 500, 6000), {128, 128, 128});
	Weltall::FBattleShowQuad& Sprite = Frame.Quads.emplace_back();
	Sprite.Drawer = static_cast<uint16>(Frame.Drawers.size());
	Frame.Drawers.push_back({1, Weltall::EBattleShowDrawer::Sprite, {}});
	Sprite.Quad.Matrix = Weltall::FGteMatrix::Identity();
	Sprite.Quad.Matrix.T = {0, 0, 2000};
	const auto At2000 = [](int32 X, int32 Y) { return Weltall::FGteVector{static_cast<int16>((X - 160) * 2000 / 512), static_cast<int16>((Y - 164) * 2000 / 512), 0}; };
	Sprite.Quad.Corners = {At2000(20, 100), At2000(50, 100), At2000(20, 150), At2000(50, 150)};
	Sprite.Quad.U1 = 16;
	Sprite.Quad.V1 = 16;
	Sprite.Quad.TexturePage = BattlePage;
	Sprite.Quad.Clut = BattleClut;
	Sprite.Quad.ColorCode = BattleRawSprite | 0x808080;
	Sprite.Quad.OtIndex = 2000 / 16;
	Frame.Order.push_back({Weltall::EBattleShowLink::Quad, 0, Sprite.Quad.OtIndex});
	// A sprite standing 1600 away, its picture reaching below its feet on the screen (a long robe, a hovering boss): over floor
	// far nearer than its entry, which the original draws before it.
	const Weltall::FSpriteQuad Standing = Sprite.Quad;
	Weltall::FBattleShowQuad& Reaching = Frame.Quads.emplace_back();
	Reaching.Drawer = static_cast<uint16>(Frame.Drawers.size());
	Frame.Drawers.push_back({2, Weltall::EBattleShowDrawer::Sprite, {}});
	Reaching.Quad = Standing;
	Reaching.Quad.Matrix.T = {0, 0, 1600};
	const auto At1600 = [](int32 X, int32 Y) { return Weltall::FGteVector{static_cast<int16>((X - 160) * 1600 / 512), static_cast<int16>((Y - 164) * 1600 / 512), 0}; };
	Reaching.Quad.Corners = {At1600(70, 150), At1600(100, 150), At1600(70, 214), At1600(100, 214)};
	Reaching.Quad.OtIndex = 1600 / 16;
	Frame.Order.push_back({Weltall::EBattleShowLink::Quad, 1, Reaching.Quad.OtIndex});
	Objects.AddQuad(Frame, Stage, BattleStanding(-1500, -500, -400, 0, 2500), {128, 64, 0});
	// A purple pillar of the stage 3000 away, linked after the wall in front of it.
	Objects.AddQuad(Frame, Stage, BattleStanding(-1200, -900, -600, 0, 3000), {128, 0, 128});
	// A blue gear 1000 away, its soles 10 units under the floor, then a green one 1500 away linked after it, then the blue
	// one's shadow on the floor from 800 to 1400 ahead, right of the middle; a red spark inside the blue gear, 1100 away.
	Objects.AddQuad(Frame, 0, BattleStanding(-100, 100, -200, 110, 1000), {0, 0, 255});
	Objects.AddQuad(Frame, 1, BattleStanding(-60, 60, -150, -50, 1500), {0, 255, 0});
	Objects.AddShadow(Frame, 0, {{{300, 100, 1400}, {0, 100, 1400}, {300, 100, 800}, {0, 100, 800}}});
	Weltall::FBattleShowTile& Spark = Frame.Tiles.emplace_back();
	Spark.Place = {-64, -159, 1100};
	Spark.Size = 17;
	Spark.ColorCode = BattleOpaqueTile | 0x0000FF;
	Frame.Order.push_back({Weltall::EBattleShowLink::Tile, 0, 0});

	const auto Render = [&](EFieldSorting Sorting)
	{
		const FFieldSceneMesh NoMesh{Weltall::FFieldScene{}};
		FFieldRenderOptions Options;
		Options.Sorting = Sorting;
		Options.AntiAliasing = EFieldAntiAliasing::None;
		FBattleFrameDraw Draw;
		BuildBattleFrameDraw(Frame, nullptr, Options, Draw);
		return RenderFieldSceneOffscreen(NoMesh, Units, Draw.Scene, FIntPoint(BattleScreenWidth, BattleScreenHeight));
	};
	const TArray<FColor> Painted = Render(EFieldSorting::PaintersOrder);
	const TArray<FColor> Buffered = Render(EFieldSorting::DepthBuffer);
	const TArray<FColor> Sorted = Render(EFieldSorting::DepthBufferWithScenery);
	BeginReleaseResource(&Units);
	FlushRenderingCommands();
	for (const TArray<FColor>* Pixels : {&Painted, &Buffered, &Sorted})
	{
		if (!TestEqual(TEXT("The frames were read back"), Pixels->Num(), BattleScreenWidth * BattleScreenHeight))
		{
			return false;
		}
	}
	const auto Near = [this](const TArray<FColor>& Pixels, const FString& What, int32 X, int32 Y, int32 R, int32 G, int32 B)
	{
		const FColor& Pixel = Pixels[Y * BattleScreenWidth + X];
		const bool bNear = FMath::Abs(Pixel.R - R) <= 2 && FMath::Abs(Pixel.G - G) <= 2 && FMath::Abs(Pixel.B - B) <= 2;
		TestTrue(FString::Printf(TEXT("%s: (%d, %d, %d) near (%d, %d, %d)"), *What, Pixel.R, Pixel.G, Pixel.B, R, G, B), bNear);
	};
	Near(Painted, TEXT("in the order: the far gear linked last over the near one"), 160, 130, 0, 255, 0);
	Near(Painted, TEXT("in the order: the shadow over the gear's legs"), 180, 202, 0, 0, 191);
	Near(Painted, TEXT("in the order: the wall over the sprite before it"), 35, 125, 128, 64, 0);
	Near(Painted, TEXT("in the order: the far pillar over the wall"), 0, 120, 128, 0, 128);
	Near(Buffered, TEXT("per pixel: the stage keeps its order, the far pillar over the wall"), 0, 120, 128, 0, 128);
	Near(Sorted, TEXT("scenery too: the wall in front of the far pillar"), 0, 120, 128, 64, 0);
	for (const auto& [Pixels, Name] : {std::pair(&Painted, TEXT("in the order")), std::pair(&Buffered, TEXT("per pixel")), std::pair(&Sorted, TEXT("scenery too"))})
	{
		Near(*Pixels, FString::Printf(TEXT("%s: the gear's sole, 2 units above the floor, in its shadow"), Name), 180, 214, 0, 0, 191);
		Near(*Pixels, FString::Printf(TEXT("%s: the gear's soles under the stage's floor, drawn after it"), Name), 140, 218, 0, 0, 255);
		Near(*Pixels, FString::Printf(TEXT("%s: the spark inside the gear, linked after it"), Name), 130, 90, 255, 0, 0);
		Near(*Pixels, FString::Printf(TEXT("%s: the shadow on the floor beside the gear"), Name), 240, 210, 64, 64, 64);
		Near(*Pixels, FString::Printf(TEXT("%s: the floor never hides a sprite standing on it"), Name), 85, 210, 255, 0, 0);
	}
	for (const auto& [Pixels, Name] : {std::pair(&Buffered, TEXT("per pixel")), std::pair(&Sorted, TEXT("scenery too"))})
	{
		Near(*Pixels, FString::Printf(TEXT("%s: the near gear in front of the far one"), Name), 160, 130, 0, 0, 255);
		Near(*Pixels, FString::Printf(TEXT("%s: the gear's legs, 26 units above the floor, in front of its shadow"), Name), 180, 202, 0, 0, 255);
		Near(*Pixels, FString::Printf(TEXT("%s: the sprite in front of the wall linked after it"), Name), 35, 125, 255, 0, 0);
	}
	return true;
}

#endif
