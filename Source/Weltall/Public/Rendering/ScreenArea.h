#pragma once

#include "CoreMinimal.h"
#include "Presentation/ScreenLayout.h"

/**
 * Where a display frame is drawn in its output: the part its picture takes, and how its game screens lie there,
 * each as large as fits or all at one whole number of pixels a pixel (Weltall::FPictureArea).
 */
struct FScreenArea : public FIntRect
{
	FScreenArea() = default;
	explicit FScreenArea(const FIntRect& Rect, int32 InWholeScale = 0, bool bInTouchOverlay = false)
		: FIntRect(Rect), WholeScale(InWholeScale), bTouchOverlay(bInTouchOverlay) {}

	/** Pixels a game pixel for every screen of the frame; 0 fits each screen as large as it goes. */
	int32 WholeScale = 0;
	/** On-screen controls are drawn over the picture's sides. */
	bool bTouchOverlay = false;

	/** Where Screen lies, relative to the area's corner. */
	Weltall::FScreenLayout Layout(Weltall::FScreenSize Screen) const
	{
		return Weltall::FPictureArea{{0, 0, Width(), Height()}, WholeScale}.Layout(Screen);
	}

	/** The output's pixels Screen covers. */
	FIntRect ScreenRect(Weltall::FScreenSize Screen) const
	{
		return InOutput(Layout(Screen).ToDisplay({0, 0, static_cast<int16>(Screen.Width), static_cast<int16>(Screen.Height)}));
	}

	/** Screen's rows across the area's whole width, laid out as this area lays out screens: where a 3D scene of it is drawn. */
	FScreenArea Rows(Weltall::FScreenSize Screen) const
	{
		return FScreenArea(InOutput(Layout(Screen).Rows(Width(), Screen)), WholeScale, bTouchOverlay);
	}

	/** The same area with its corner at Corner: a picture of the frame in a texture of its own. */
	FScreenArea At(FIntPoint Corner) const
	{
		return FScreenArea(FIntRect(Corner, Corner + Size()), WholeScale, bTouchOverlay);
	}

	bool operator==(const FScreenArea& Other) const
	{
		return FIntRect::operator==(Other) && WholeScale == Other.WholeScale && bTouchOverlay == Other.bTouchOverlay;
	}

private:
	FIntRect InOutput(const Weltall::FDisplayRect& Rect) const
	{
		const FIntPoint Corner = Min + FIntPoint(Rect.X, Rect.Y);
		return FIntRect(Corner, Corner + FIntPoint(Rect.Width, Rect.Height));
	}
};
