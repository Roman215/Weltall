#pragma once

#include "CoreMinimal.h"
#include "Presentation/CrtLook.h"
#include "Presentation/GameScreen.h"
#include "RenderGraphFwd.h"
#include "Rendering/ScreenArea.h"

/**
 * Draws Picture from DrawArea into Area of Output. The two areas share a width. Source rows are read in
 * DrawArea's layout, then the beam and phosphors are drawn at the output's pixel positions.
 */
void AddCrtPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Picture, FRDGTextureRef Output, const FScreenArea& DrawArea, const FScreenArea& Area,
	const Weltall::FCrtParameters& Look, Weltall::FScreenSize Screen);
