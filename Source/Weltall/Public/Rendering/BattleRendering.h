#pragma once

#include "CoreMinimal.h"
#include "Presentation/DrawList.h"
#include "Rendering/FieldSceneRendering.h"
#include "Rendering/ScreenArea.h"

class FVramTexture;

namespace Weltall
{
	struct FBattleFrameBlend;
	struct FBattleShowFrame;
}

/**
 * The drawing of a frame of the battle's show, in the order the GPU draws its ordering table: the scene (the stage's
 * model, its 3D objects' parts, shadows, cloths and trails, the sprites', shadows' and target markers' polygons), the
 * fade layer, what the frame links nearer than the fade, the HUD, and what it links nearer than the HUD (the damage
 * numbers, the running total).
 */
struct FBattleFrameDraw
{
	/** Drawn by the field scene's passes on the frame's clear colour, in the table's order or under the field's depth rule. */
	FFieldSceneDraw Scene;
	/** The fade layer, when the frame links one: a primitive over the whole 320 x 224 screen and past its edges. */
	Weltall::FDrawList Fade;
	/** Over the fade, in the table's order. */
	FFieldSceneDraw OverFade;
	/** The HUD's 2D list on the screen, which the frame's owner fills. */
	Weltall::FDrawList Hud;
	/** Over the HUD, in the table's order. */
	FFieldSceneDraw OverHud;
};

/**
 * Makes the drawing of a frame of the battle's show but for its HUD's list: its polygons through the matrices the GTE drew
 * them with into view space, with the battle's projection (its 320 x 224 screen, the centre below the middle), each in the
 * part the frame's order puts it in. With a Blend (Weltall::BlendBattleFrames' of this frame), drawn between the frame
 * before and this one: each anchor's points moved, the cloths, the backdrop, the projection and the colours as the blend
 * has them; without, as it is. TexelKinds, following the VRAM the frame is drawn from, lets a semi-transparent polygon's pass
 * with no texels to draw be left out. Reuses Draw's memory.
 */
WELTALL_API void BuildBattleFrameDraw(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, const FFieldRenderOptions& Options,
	FBattleFrameDraw& Draw, Weltall::FTexelKindCache* TexelKinds = nullptr);

/**
 * Adds the passes that draw a battle frame into Area of Output: the scene on the widened screen and the fade over it, on
 * the screen's rows across the area, the HUD on the menus' screen laid out in the area (its button pictures textured
 * from IconVram), each part in its turn.
 */
WELTALL_API void AddBattleFramePasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FVramTexture& IconVram,
	const FBattleFrameDraw& Draw, FRDGTextureRef Output, const FScreenArea& Area);
