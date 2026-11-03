#pragma once

#include "CoreMinimal.h"
#include "Presentation/CrtLook.h"
#include "Presentation/GameScreen.h"
#include "RenderGraphFwd.h"
#include "Rendering/ScreenArea.h"

/**
 * Adds the passes that draw Picture, a frame drawn whole into a texture of Area's size, into Area of Output as a CRT of
 * the look given shows it (Crt.usf): Screen's rows, laid out as Area lays out screens, are the beam's lines.
 */
void AddCrtPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Picture, FRDGTextureRef Output, const FScreenArea& Area, const Weltall::FCrtParameters& Look,
	Weltall::FScreenSize Screen);
