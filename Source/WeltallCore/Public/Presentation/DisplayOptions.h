#pragma once

#include "Foundation/Types.h"
#include "Presentation/CrtLook.h"

#include <compare>
#include <vector>

namespace Weltall
{
	/** A size in display pixels, ordered by width, then height. */
	struct FDisplaySize
	{
		int32 Width = 0;
		int32 Height = 0;

		bool IsSet() const { return Width > 0 && Height > 0; }
		auto operator<=>(const FDisplaySize&) const = default;
	};

	/** The shape of the picture in the window: the window's own, or the widest of a fixed shape that fits, bars around it. */
	enum class EDisplayAspect : uint8
	{
		Window,
		Wide,
		Standard,
	};

	/** Whether the picture is drawn as a CRT shows it: a studio monitor's look, a television's, or the player's own. */
	enum class ECrtFilter : uint8
	{
		Off,
		Monitor,
		Television,
		Custom,
	};

	/** How the player has the game's window. */
	struct FDisplayOptions
	{
		/** The window's size the player chose; unset, the window is as the platform opens it. */
		FDisplaySize WindowSize;
		EDisplayAspect Aspect = EDisplayAspect::Window;
		/** The game stops, its play time with it, while its window is not the one in use. */
		bool bPauseInBackground = true;
		/** The sound is heard while the window is not the one in use (and the game goes on). */
		bool bSoundInBackground = false;
		/** The game fills the display without a border (never the display's own full-screen mode); off, a window of WindowSize. */
		bool bFullScreen = false;
		ECrtFilter Crt = ECrtFilter::Off;
		/** The player's own look, which Custom draws with. */
		FCrtLook CustomCrt = FCrtLook::Monitor();
		/** Every game screen is drawn at a whole number of display pixels a pixel, the picture smaller for it (PictureArea). */
		bool bIntegerScaling = false;

		bool operator==(const FDisplayOptions&) const = default;
	};

	/** The CRT's look in use: the chosen look's, or the player's own while Custom or off. */
	constexpr FCrtLook ShownCrtLook(const FDisplayOptions& Options)
	{
		return Options.Crt == ECrtFilter::Monitor ? FCrtLook::Monitor() : Options.Crt == ECrtFilter::Television ? FCrtLook::Television() : Options.CustomCrt;
	}

	/** What the display offers the options screen: the sizes a window can take, smallest first, and the window's now. None where the game has no window. */
	struct FDisplayModes
	{
		std::vector<FDisplaySize> WindowSizes;
		FDisplaySize Window;
	};
}
