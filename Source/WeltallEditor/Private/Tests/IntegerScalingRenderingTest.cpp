#include "Misc/AutomationTest.h"
#include "OffscreenRender.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Screens/GameScreen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A game screen whose rows are blue across the picture, as a 3D scene draws them, and one of its rows white. */
	class FRowsTestFrame final : public IScreenFrame
	{
	public:
		FRowsTestFrame(Weltall::FScreenSize InScreen, int32 InRow) : Screen(InScreen), Row(InRow) {}

		virtual void AddPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FScreenArea& Area) const override
		{
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Blue, Area.Rows(Screen));
			const Weltall::FDisplayRect Placed = Area.Layout(Screen).ToDisplay({0, static_cast<int16>(Row), static_cast<int16>(Screen.Width), 1});
			const FIntPoint Min = Area.Min + FIntPoint(Placed.X, Placed.Y);
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::White, FIntRect(Min, Min + FIntPoint(Placed.Width, Placed.Height)));
		}

		virtual Weltall::FScreenSize ScreenSize() const override { return Screen; }

	private:
		Weltall::FScreenSize Screen;
		int32 Row;
	};

	const FIntPoint FullHd(1920, 1080);

	enum class EShade : uint8
	{
		Black,
		Blue,
		White,
	};

	EShade ShadeAt(const TArray<FColor>& Pixels, int32 X, int32 Y)
	{
		const FColor& Pixel = Pixels[Y * FullHd.X + X];
		return Pixel.R > 200 && Pixel.B > 200 ? EShade::White : Pixel.B > 200 ? EShade::Blue : EShade::Black;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegerScalingRenderingTest, "Weltall.Rendering.IntegerScalingDrawsEveryScreenAtOneWholeScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FIntegerScalingRenderingTest::RunTest(const FString&)
{
	Weltall::FDisplayOptions Options;
	Options.bIntegerScaling = true;
	const TArray<FColor> Field = RenderScreenFrameOffscreen(FRowsTestFrame({320, 224}, 100), FullHd, Options);
	const TArray<FColor> WorldMap = RenderScreenFrameOffscreen(FRowsTestFrame({320, 216}, 100), FullHd, Options);
	Options.bIntegerScaling = false;
	Options.Aspect = Weltall::EDisplayAspect::Standard;
	const TArray<FColor> Standard = RenderScreenFrameOffscreen(FRowsTestFrame({320, 224}, 100), FullHd, Options);
	if (!TestEqual(TEXT("The frames were read back"), Field.Num() + WorldMap.Num() + Standard.Num(), 3 * FullHd.X * FullHd.Y))
	{
		return false;
	}
	const auto Expect = [this](const TCHAR* What, const TArray<FColor>& Pixels, int32 X, int32 Y, EShade Shade)
	{
		TestEqual(FString::Printf(TEXT("%s (%d, %d)"), What, X, Y), static_cast<int32>(ShadeAt(Pixels, X, Y)), static_cast<int32>(Shade));
	};

	// 1080 rows hold 224 four times: the picture 1592 x 896 in the display's middle, black around it.
	Expect(TEXT("Left of the picture"), Field, 163, 500, EShade::Black);
	Expect(TEXT("The picture's left edge"), Field, 164, 500, EShade::Blue);
	Expect(TEXT("The picture's right edge"), Field, 1755, 500, EShade::Blue);
	Expect(TEXT("Right of the picture"), Field, 1756, 500, EShade::Black);
	Expect(TEXT("Above the picture"), Field, 1000, 91, EShade::Black);
	Expect(TEXT("The picture's top"), Field, 1000, 92, EShade::Blue);
	Expect(TEXT("The picture's bottom"), Field, 1000, 987, EShade::Blue);
	Expect(TEXT("Below the picture"), Field, 1000, 988, EShade::Black);
	// Row 100 four display rows tall, across the screen's 1280 columns.
	Expect(TEXT("Above row 100"), Field, 1000, 491, EShade::Blue);
	Expect(TEXT("Row 100's first display row"), Field, 1000, 492, EShade::White);
	Expect(TEXT("Row 100's last display row"), Field, 1000, 495, EShade::White);
	Expect(TEXT("Below row 100"), Field, 1000, 496, EShade::Blue);
	Expect(TEXT("Left of the screen"), Field, 319, 493, EShade::Blue);
	Expect(TEXT("The screen's first column"), Field, 320, 493, EShade::White);
	Expect(TEXT("The screen's last column"), Field, 1599, 493, EShade::White);
	Expect(TEXT("Right of the screen"), Field, 1600, 493, EShade::Blue);

	// The world map's 216 rows at the same scale, in the middle of the same picture: black above and below them.
	Expect(TEXT("Above the world map's rows"), WorldMap, 1000, 107, EShade::Black);
	Expect(TEXT("The world map's first row"), WorldMap, 1000, 108, EShade::Blue);
	Expect(TEXT("The world map's row 100"), WorldMap, 1000, 508, EShade::White);
	Expect(TEXT("The world map's row 100's last display row"), WorldMap, 1000, 511, EShade::White);
	Expect(TEXT("Below the world map's row 100"), WorldMap, 1000, 512, EShade::Blue);
	Expect(TEXT("The world map's last row"), WorldMap, 1000, 971, EShade::Blue);
	Expect(TEXT("Below the world map's rows"), WorldMap, 1000, 972, EShade::Black);

	// Without it, a 4:3 picture's 240 lines show the screen's 224 rows between bars, 4.5 display rows each.
	Expect(TEXT("4:3's left bar"), Standard, 239, 500, EShade::Black);
	Expect(TEXT("4:3's left edge"), Standard, 240, 500, EShade::Blue);
	Expect(TEXT("4:3's right edge"), Standard, 1679, 500, EShade::Blue);
	Expect(TEXT("4:3's right bar"), Standard, 1680, 500, EShade::Black);
	Expect(TEXT("Above 4:3's rows"), Standard, 1000, 35, EShade::Black);
	Expect(TEXT("4:3's first row"), Standard, 1000, 36, EShade::Blue);
	Expect(TEXT("4:3's last row"), Standard, 1000, 1043, EShade::Blue);
	Expect(TEXT("Below 4:3's rows"), Standard, 1000, 1044, EShade::Black);
	return true;
}

#endif
