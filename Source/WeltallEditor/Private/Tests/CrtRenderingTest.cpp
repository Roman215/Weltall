#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "Presentation/ScreenLayout.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Screens/GameScreen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A game screen of flat grey, one of its rows white (none for -1). */
	class FCrtTestFrame final : public IScreenFrame
	{
	public:
		FCrtTestFrame(Weltall::FScreenSize InScreen, int32 InRow) : Screen(InScreen), Row(InRow) {}

		virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
		{
			const float Level = Grey / 255.0f;
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor(Level, Level, Level), Area);
			if (Row >= 0)
			{
				const Weltall::FDisplayRect Placed = Area.Layout(Screen).ToDisplay({0, static_cast<int16>(Row), static_cast<int16>(Screen.Width), 1});
				const FIntPoint Min = Area.Min + FIntPoint(Placed.X, Placed.Y);
				AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::White, FIntRect(Min, Min + FIntPoint(Placed.Width, Placed.Height)));
			}
		}

		virtual Weltall::FScreenSize ScreenSize() const override { return Screen; }

		static constexpr int32 Grey = 128;

	private:
		Weltall::FScreenSize Screen;
		int32 Row;
	};

	/** 224 rows four display pixels apart; a 216-row screen is placed 16 pixels down. */
	const FIntPoint CrtTestSize(1280, 896);

	TArray<FColor> RenderCrt(Weltall::FScreenSize Screen, int32 Row, Weltall::ECrtFilter Filter)
	{
		Weltall::FDisplayOptions Options;
		Options.Crt = Filter;
		return RenderScreenFrameOffscreen(FCrtTestFrame(Screen, Row), CrtTestSize, Options);
	}

	double Light(uint8 Level)
	{
		return FMath::Pow(Level / 255.0, 2.2);
	}

	/** The mean light of the display's row Y between columns 400 and 880, its three channels together. */
	double RowLight(const TArray<FColor>& Pixels, int32 Y)
	{
		double Sum = 0.0;
		for (int32 X = 400; X < 880; ++X)
		{
			const FColor& Pixel = Pixels[Y * CrtTestSize.X + X];
			Sum += Light(Pixel.R) + Light(Pixel.G) + Light(Pixel.B);
		}
		return Sum / (3 * 480);
	}

	/** The display row, among those within twelve of Near, with the most light. */
	int32 BrightestRow(const TArray<FColor>& Pixels, int32 Near)
	{
		int32 Brightest = Near - 12;
		for (int32 Y = Near - 12; Y <= Near + 12; ++Y)
		{
			Brightest = RowLight(Pixels, Y) > RowLight(Pixels, Brightest) ? Y : Brightest;
		}
		return Brightest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrtRenderingTest, "Weltall.Rendering.CrtDrawsTheScreensRowsAsLinesAndKeepsTheirLight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrtRenderingTest::RunTest(const FString&)
{
	const Weltall::FScreenSize Standard{320, 224};
	const TArray<FColor> Off = RenderCrt(Standard, -1, Weltall::ECrtFilter::Off);
	const TArray<FColor> Flat = RenderCrt(Standard, -1, Weltall::ECrtFilter::Monitor);
	if (!TestEqual(TEXT("The frames were read back"), Off.Num() + Flat.Num(), 2 * CrtTestSize.X * CrtTestSize.Y))
	{
		return false;
	}
	TestEqual(TEXT("Off draws the frame as it is"), static_cast<int32>(Off[448 * CrtTestSize.X + 640].G), FCrtTestFrame::Grey);

	// Lines of light four pixels apart: each line's middle two rows lit, the rows between its lines darker. Over whole lines the
	// light is the picture's, made linear by the tube's gamma (2.4), with the glass's glow (3%) added: the beam and the
	// mask move light, they neither make nor lose it.
	for (int32 Line = 100; Line < 103; ++Line)
	{
		TestTrue(FString::Printf(TEXT("Line %d's middle is brighter than its edge"), Line), RowLight(Flat, 4 * Line + 1) > 1.5 * RowLight(Flat, 4 * Line));
	}
	double Sum = 0.0;
	for (int32 Y = 400; Y < 412; ++Y)
	{
		Sum += RowLight(Flat, Y);
	}
	const double Expected = FMath::Pow(FCrtTestFrame::Grey / 255.0, 2.4) * 1.03;
	TestTrue(FString::Printf(TEXT("Three lines' light %.4f is the picture's %.4f"), Sum / 12, Expected), FMath::Abs(Sum / 12 / Expected - 1.0) < 0.01);

	// The aperture grille, three pixels a triad at this height: red, green and blue stripes.
	const FColor& Red = Flat[402 * CrtTestSize.X + 600];
	const FColor& Green = Flat[402 * CrtTestSize.X + 601];
	const FColor& Blue = Flat[402 * CrtTestSize.X + 602];
	TestTrue(TEXT("The grille's red stripe"), Red.R > Red.G && Red.R > Red.B);
	TestTrue(TEXT("The grille's green stripe"), Green.G > Green.R && Green.G > Green.B);
	TestTrue(TEXT("The grille's blue stripe"), Blue.B > Blue.R && Blue.B > Blue.G);

	// One white row lights its own line: 224 rows fill the height, row 100's line in the middle of display rows 400 to 404;
	// the world map's 216 rows are placed 16 rows down, and so is its line.
	const TArray<FColor> Row = RenderCrt(Standard, 100, Weltall::ECrtFilter::Monitor);
	const TArray<FColor> WorldMapRow = RenderCrt({320, 216}, 100, Weltall::ECrtFilter::Monitor);
	const int32 Brightest = BrightestRow(Row, 402);
	TestTrue(FString::Printf(TEXT("Row 100's line is lit at display row %d"), Brightest), Brightest == 401 || Brightest == 402);
	const int32 WorldMapBrightest = BrightestRow(WorldMapRow, 418);
	TestTrue(FString::Printf(TEXT("A 216-row screen's row 100 is lit at display row %d"), WorldMapBrightest), WorldMapBrightest == 417 || WorldMapBrightest == 418);

	// A television's glass bulges, its corners rounded off into black.
	const TArray<FColor> Television = RenderCrt(Standard, -1, Weltall::ECrtFilter::Television);
	TestEqual(TEXT("The television's corner is black"), static_cast<int32>(Television[0].G), 0);
	TestTrue(TEXT("The television's middle is lit"), Television[449 * CrtTestSize.X + 640].G > 64);
	return true;
}

#endif
