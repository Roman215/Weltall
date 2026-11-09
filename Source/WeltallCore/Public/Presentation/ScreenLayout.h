#pragma once

#include "Foundation/Types.h"
#include "Presentation/DisplayOptions.h"
#include "Presentation/GameScreen.h"

namespace Weltall
{
	enum class ECompassPlacement : uint8;

	/** A rectangle of whole display pixels. */
	struct FDisplayRect
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;

		bool operator==(const FDisplayRect&) const = default;
	};

	/** The part of a display the picture takes: the whole of it, or the largest of the aspect's shape in its middle. */
	WELTALLCORE_API FDisplayRect FitAspect(int32 DisplayWidth, int32 DisplayHeight, EDisplayAspect Aspect);

	/** The right edge of the compass's centered area within the picture, in pixels from the picture's left. */
	WELTALLCORE_API int32 CompassRightEdge(int32 Width, int32 Height, ECompassPlacement Placement);

	/**
	 * Where a game screen appears on a display: centred, with square pixels, as large as fits or at a whole
	 * number of display pixels a pixel, so a wider or taller display shows black bars beside or above it.
	 */
	class FScreenLayout
	{
	public:
		WELTALLCORE_API static FScreenLayout Fit(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen);

		/** The screen at Scale display pixels a pixel, whether or not it fits. */
		WELTALLCORE_API static FScreenLayout Whole(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen, int32 Scale);

		/** Display pixels per game-screen pixel. */
		double Scale() const { return PixelScale; }

		/** The display pixels a rectangle of the game screen covers, each edge on its nearest pixel boundary. */
		WELTALLCORE_API FDisplayRect ToDisplay(const FScreenRect& Rect) const;

		/** The display rows the whole screen covers, across a display Width wide: where a 3D scene of the screen is drawn. */
		WELTALLCORE_API FDisplayRect Rows(int32 Width, FScreenSize Screen) const;

	private:
		static FScreenLayout Centred(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen, double Scale);

		double PixelScale = 0.0;
		double OriginX = 0.0;
		double OriginY = 0.0;
	};

	/** The screen of the field, the menus and the battles: integer scaling sizes every screen by its pixels. */
	inline constexpr FScreenSize PlayScreen{320, 224};

	/** Where a display frame is drawn: the part of the display its picture takes, and how its game screens lie there. */
	struct FPictureArea
	{
		FDisplayRect Rect;
		/** Display pixels a game pixel, the same for every screen in the frame; 0 fits each screen as large as it goes. */
		int32 WholeScale = 0;

		/** Where Screen lies, relative to Rect's corner. */
		WELTALLCORE_API FScreenLayout Layout(FScreenSize Screen) const;

		bool operator==(const FPictureArea&) const = default;
	};

	/**
	 * The area a frame of Screen is drawn in: FitAspect's. With integer scaling, that shape made smaller about its
	 * middle until it holds Screen, and the play screen the menus draw over any screen, at the most whole display
	 * pixels a pixel at which the play screen fits (fewer where Screen, as a movie's 240 rows, would not fit at as
	 * many); FitAspect's where not one fits.
	 */
	WELTALLCORE_API FPictureArea PictureArea(int32 DisplayWidth, int32 DisplayHeight, const FDisplayOptions& Options, FScreenSize Screen);
}
