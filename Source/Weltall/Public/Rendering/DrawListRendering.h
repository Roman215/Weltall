#pragma once

#include "CoreMinimal.h"
#include "Presentation/GameScreen.h"
#include "RenderGraphFwd.h"

#include <vector>

class FDrawListTextures;
class FVramTexture;

namespace Weltall
{
	struct FDrawList;
}

/**
 * One displayed frame of a screen's 2D layer: the latest tick's list, with the primitives that
 * change between ticks drawn Alpha of the way from their partners in the tick before (see
 * Weltall::MatchBlendKeys). Without a previous list everything is drawn as it is.
 */
struct FDrawListFrame
{
	const Weltall::FDrawList* Latest = nullptr;
	const Weltall::FDrawList* Previous = nullptr;
	/** For each primitive of Latest, its partner's index in Previous, or -1. */
	const std::vector<int32>* Matches = nullptr;
	float Alpha = 1.0f;
	/** The stretch of Latest's primitives drawn: the whole list unless told otherwise. */
	std::size_t First = 0;
	std::size_t End = static_cast<std::size_t>(-1);
	/** Shift for right-anchored primitives, in output pixels. */
	int32 RightShift = 0;
	bool bHideRightAnchored = false;
};

/**
 * Adds the passes that draw a frame's 2D layer over Output, with the game screen (Screen of its own
 * pixels) filling ScreenRect of it, nothing drawn above or below its rows. Textured primitives sample
 * Textures, which must hold every texture they name.
 */
WELTALL_API void AddDrawListPasses(FRDGBuilder& GraphBuilder, const FDrawListTextures& Textures, const FDrawListFrame& Frame, FRDGTextureRef Output,
	const FIntRect& ScreenRect, Weltall::FScreenSize Screen);

/** The same, with every textured primitive fetching its texels from VRAM through its page and CLUT. */
WELTALL_API void AddDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FDrawListFrame& Frame, FRDGTextureRef Output,
	const FIntRect& ScreenRect, Weltall::FScreenSize Screen);

/**
 * A list whose button pictures may be the player's own (Weltall::FMenuButtonIcons), drawn as it is: each
 * stretch of primitives from the VRAM its textures name, IconVram for the pictures' and Vram for the rest.
 */
WELTALL_API void AddIconDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FVramTexture& IconVram, const Weltall::FDrawList& List,
	FRDGTextureRef Output, const FIntRect& ScreenRect, Weltall::FScreenSize Screen);

/** The same for a displayed frame of a list, its stretch drawn between the tick before's and the latest. */
WELTALL_API void AddIconDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FVramTexture& IconVram, const FDrawListFrame& Frame,
	FRDGTextureRef Output, const FIntRect& ScreenRect, Weltall::FScreenSize Screen);
