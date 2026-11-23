#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "Presentation/ScreenLayout.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Screens/GameScreen.h"

#include <cmath>

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
	double RowLight(const TArray<FColor>& Pixels, int32 Y, int32 Width = CrtTestSize.X)
	{
		const int32 Left = Width / 2 - 240;
		const int32 Right = Left + 480;
		double Sum = 0.0;
		for (int32 X = Left; X < Right; ++X)
		{
			const FColor& Pixel = Pixels[Y * Width + X];
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
	// light is the picture's, made linear by the tube's gamma, with the glass's glow added: the beam and the
	// mask move light, they neither make nor lose it.
	for (int32 Line = 100; Line < 103; ++Line)
	{
		TestTrue(FString::Printf(TEXT("Line %d's middle is brighter than its edge"), Line), RowLight(Flat, 4 * Line + 1) > 1.25 * RowLight(Flat, 4 * Line));
	}
	double Sum = 0.0;
	for (int32 Y = 400; Y < 412; ++Y)
	{
		Sum += RowLight(Flat, Y);
	}
	const Weltall::FCrtParameters Monitor = Weltall::CrtParameters(Weltall::FCrtLook::Monitor());
	const double Expected = FMath::Pow(FCrtTestFrame::Grey / 255.0, Monitor.TubeGamma) * (1.0 + Monitor.GlowAmount);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrtFourByThreeTest, "Weltall.Rendering.CrtIntegratesScanlinesAtFourByThreeDisplayResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrtFourByThreeTest::RunTest(const FString&)
{
	constexpr Weltall::FScreenSize Screen{320, 224};
	const FCrtTestFrame Frame(Screen, -1);
	Weltall::FDisplayOptions Options;
	Options.Aspect = Weltall::EDisplayAspect::Standard;
	Options.Crt = Weltall::ECrtFilter::Custom;
	Options.CustomCrt.Mask = Weltall::ECrtMask::None;
	Options.CustomCrt.Glow = 0;
	Options.CustomCrt.Scanlines = Weltall::FCrtLook::Steps;
	const FIntPoint Display(1280, 960);
	const TArray<FColor> Pixels = RenderScreenFrameOffscreen(Frame, Display, Options);
	if (!TestEqual(TEXT("The 4:3 frame was read back"), Pixels.Num(), Display.X * Display.Y))
	{
		return false;
	}

	// Compare a complete cycle of fractional scanline phases to beams integrated directly over the final pixels.
	const double Signal = FMath::Pow(FCrtTestFrame::Grey / 255.0, 2.4);
	const double Pitch = static_cast<double>(Display.Y) / Screen.Height;
	const double Sigma = FMath::Lerp(0.25, 0.42, FMath::Sqrt(Signal));
	const double Scale = Pitch * Sigma * FMath::Sqrt(2.0);
	double MaxError = 0.0;
	for (int32 Y = 450; Y < 480; ++Y)
	{
		const int32 Nearest = static_cast<int32>((Y + 0.5) / Pitch);
		double Expected = 0.0;
		for (int32 Row = Nearest - 3; Row <= Nearest + 3; ++Row)
		{
			const double Centre = (Row + 0.5) * Pitch;
			Expected += 0.5 * Pitch * Signal * (std::erf((Y + 1 - Centre) / Scale) - std::erf((Y - Centre) / Scale));
		}
		MaxError = FMath::Max(MaxError, FMath::Abs(RowLight(Pixels, Y, Display.X) - Expected));
	}
	TestTrue(FString::Printf(TEXT("The largest beam integration error is %.4f"), MaxError), MaxError < 0.004);

	const FIntPoint Small(640, 480);
	const TArray<FColor> SmallPixels = RenderScreenFrameOffscreen(Frame, Small, Options);
	if (!TestEqual(TEXT("The 480p frame was read back"), SmallPixels.Num(), Small.X * Small.Y))
	{
		return false;
	}
	double Min = 1.0;
	double Max = 0.0;
	for (int32 Y = 210; Y < 240; ++Y)
	{
		const double Row = RowLight(SmallPixels, Y, Small.X);
		Min = FMath::Min(Min, Row);
		Max = FMath::Max(Max, Row);
	}
	TestTrue(TEXT("The beam's contrast falls off as its lines approach the display's resolution"), Max - Min < 0.12 * Signal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrtSlotMaskTest, "Weltall.Rendering.CrtSlotMaskKeepsLightAcrossEachTriad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCrtSlotMaskTest::RunTest(const FString&)
{
	Weltall::FDisplayOptions Options;
	Options.Crt = Weltall::ECrtFilter::Custom;
	Options.CustomCrt.Mask = Weltall::ECrtMask::SlotMask;
	Options.CustomCrt.Scanlines = 0;
	Options.CustomCrt.Glow = 0;
	const TArray<FColor> Pixels = RenderScreenFrameOffscreen(FCrtTestFrame({320, 224}, -1), CrtTestSize, Options);
	if (!TestEqual(TEXT("The slot mask frame was read back"), Pixels.Num(), CrtTestSize.X * CrtTestSize.Y))
	{
		return false;
	}
	const double Signal = FMath::Pow(FCrtTestFrame::Grey / 255.0, 2.4);
	double MaxError = 0.0;
	int32 MaxRedSpread = 0;
	for (int32 Y = 400; Y < 412; ++Y)
	{
		for (int32 X = 600; X < 606; X += 3)
		{
			const FColor* Triad = &Pixels[Y * CrtTestSize.X + X];
			const double Red = (Light(Triad[0].R) + Light(Triad[1].R) + Light(Triad[2].R)) / 3.0;
			MaxError = FMath::Max(MaxError, FMath::Abs(Red - Signal));
			MaxRedSpread = FMath::Max(MaxRedSpread, Triad[0].R - Triad[1].R);
		}
	}
	TestTrue(TEXT("The mask still separates the phosphor colours"), MaxRedSpread > 20);
	TestTrue(FString::Printf(TEXT("The largest slot mask light error is %.4f"), MaxError), MaxError < 0.004);
	return true;
}

#endif
