#pragma once

#include "Presentation/DisplayOptions.h"

#include <SDL3/SDL_video.h>

/** SDL can report the desktop size even on displays that offer no exclusive fullscreen modes. */
inline std::vector<Weltall::FDisplaySize> LinuxDisplaySizes()
{
	std::vector<Weltall::FDisplaySize> Sizes;
	const SDL_DisplayID Display = SDL_GetPrimaryDisplay();
	if (Display == 0)
	{
		return Sizes;
	}
	if (const SDL_DisplayMode* Desktop = SDL_GetDesktopDisplayMode(Display))
	{
		Sizes.push_back({Desktop->w, Desktop->h});
	}
	int Count = 0;
	if (SDL_DisplayMode** Modes = SDL_GetFullscreenDisplayModes(Display, &Count))
	{
		for (int Index = 0; Index < Count; ++Index)
		{
			Sizes.push_back({Modes[Index]->w, Modes[Index]->h});
		}
		SDL_free(Modes);
	}
	return Sizes;
}
