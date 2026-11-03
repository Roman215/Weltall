#include "Presentation/ScreenLayout.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		int32 NearestBoundary(double Position)
		{
			return static_cast<int32>(std::lround(Position));
		}

		/** A side of a smaller picture: Old's Kept in Of, at least Filled, the margins beside Filled whole and equal. */
		int32 SmallerSide(int32 Old, int32 Filled, int64 Kept, int32 Of)
		{
			const int64 Margin = (Old * Kept - static_cast<int64>(Filled) * Of) / (2 * static_cast<int64>(Of));
			return Filled + 2 * static_cast<int32>(std::max<int64>(Margin, 0));
		}
	}

	FDisplayRect FitAspect(int32 DisplayWidth, int32 DisplayHeight, EDisplayAspect Aspect)
	{
		if (Aspect == EDisplayAspect::Window || DisplayWidth <= 0 || DisplayHeight <= 0)
		{
			return {0, 0, std::max(DisplayWidth, 0), std::max(DisplayHeight, 0)};
		}
		const int32 Across = Aspect == EDisplayAspect::Wide ? 16 : 4;
		const int32 Down = Aspect == EDisplayAspect::Wide ? 9 : 3;
		const int32 Width = std::min(DisplayWidth, static_cast<int32>(static_cast<int64>(DisplayHeight) * Across / Down));
		const int32 Height = std::min(DisplayHeight, static_cast<int32>(static_cast<int64>(DisplayWidth) * Down / Across));
		return {(DisplayWidth - Width) / 2, (DisplayHeight - Height) / 2, Width, Height};
	}

	FScreenLayout FScreenLayout::Fit(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen)
	{
		if (DisplayWidth <= 0 || DisplayHeight <= 0 || Screen.Width <= 0 || Screen.Height <= 0)
		{
			return {};
		}
		return Centred(DisplayWidth, DisplayHeight, Screen,
			std::min(static_cast<double>(DisplayWidth) / Screen.Width, static_cast<double>(DisplayHeight) / Screen.Height));
	}

	FScreenLayout FScreenLayout::Whole(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen, int32 Scale)
	{
		if (DisplayWidth <= 0 || DisplayHeight <= 0 || Screen.Width <= 0 || Screen.Height <= 0 || Scale <= 0)
		{
			return {};
		}
		return Centred(DisplayWidth, DisplayHeight, Screen, Scale);
	}

	FScreenLayout FScreenLayout::Centred(int32 DisplayWidth, int32 DisplayHeight, FScreenSize Screen, double Scale)
	{
		FScreenLayout Layout;
		Layout.PixelScale = Scale;
		Layout.OriginX = (DisplayWidth - Screen.Width * Scale) / 2.0;
		Layout.OriginY = (DisplayHeight - Screen.Height * Scale) / 2.0;
		return Layout;
	}

	FDisplayRect FScreenLayout::ToDisplay(const FScreenRect& Rect) const
	{
		const int32 Left = NearestBoundary(OriginX + Rect.X * PixelScale);
		const int32 Top = NearestBoundary(OriginY + Rect.Y * PixelScale);
		const int32 Right = NearestBoundary(OriginX + (Rect.X + Rect.Width) * PixelScale);
		const int32 Bottom = NearestBoundary(OriginY + (Rect.Y + Rect.Height) * PixelScale);
		return {Left, Top, Right - Left, Bottom - Top};
	}

	FDisplayRect FScreenLayout::Rows(int32 Width, FScreenSize Screen) const
	{
		const FDisplayRect Placed = ToDisplay({0, 0, static_cast<int16>(Screen.Width), static_cast<int16>(Screen.Height)});
		return {0, Placed.Y, std::max(Width, 0), Placed.Height};
	}

	FScreenLayout FPictureArea::Layout(FScreenSize Screen) const
	{
		return WholeScale > 0 ? FScreenLayout::Whole(Rect.Width, Rect.Height, Screen, WholeScale) : FScreenLayout::Fit(Rect.Width, Rect.Height, Screen);
	}

	FPictureArea PictureArea(int32 DisplayWidth, int32 DisplayHeight, const FDisplayOptions& Options, FScreenSize Screen)
	{
		const FDisplayRect Fitted = FitAspect(DisplayWidth, DisplayHeight, Options.Aspect);
		if (!Options.bIntegerScaling || Screen.Width <= 0 || Screen.Height <= 0)
		{
			return {Fitted};
		}
		const FScreenSize Held{std::max(Screen.Width, PlayScreen.Width), std::max(Screen.Height, PlayScreen.Height)};
		// The side Held fills first sets the scale; the picture keeps Kept of every Side pixels of each side, in whole numbers so
		// that side comes out exact.
		const bool bAcross = static_cast<int64>(Fitted.Width) * Held.Height <= static_cast<int64>(Fitted.Height) * Held.Width;
		const int32 Side = bAcross ? Fitted.Width : Fitted.Height;
		const int32 HeldSide = bAcross ? Held.Width : Held.Height;
		const int32 Scale = Side / HeldSide;
		if (Scale < 1)
		{
			return {Fitted};
		}
		const int64 Kept = static_cast<int64>(HeldSide) * Scale;
		const int32 Width = SmallerSide(Fitted.Width, Held.Width * Scale, Kept, Side);
		const int32 Height = SmallerSide(Fitted.Height, Held.Height * Scale, Kept, Side);
		return {{Fitted.X + (Fitted.Width - Width) / 2, Fitted.Y + (Fitted.Height - Height) / 2, Width, Height}, Scale};
	}
}
