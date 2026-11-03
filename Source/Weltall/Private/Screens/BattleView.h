#pragma once

#include "CoreMinimal.h"
#include "Battle/Show/BattleFrameBlend.h"
#include "Graphics/TexelKinds.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Rendering/BattleTransitionRendering.h"
#include "Rendering/FieldSceneRendering.h"
#include "Screens/GameScreen.h"

class FBattleScreenFrame;
class FBattleScreenWaveBuffers;
class FBattleScreenWaveFrame;
class FBattleTransitionFrame;
class FFieldCoverPicture;
class FVramTexture;

namespace Weltall
{
	class FBattleGameProgram;
	class IButtonGlyphs;
}

/**
 * The Battle program shown at the display's rate, from its show's latest frame: the stage's model, the 3D objects (their
 * parts, shadows, cloths and trails), the combatants' sprites, their shadows and the target markers over the frame's clear
 * colour, then the fade, the HUD and the numbers, textured from the show's VRAM; black until the show has begun. The
 * HUD's button pictures show the player's own buttons (Weltall::FMenuButtonIcons), as the glyph service shows them. A
 * battle first shows its transition: the picture of the screen before it broken into shards, or drawn as the mesh its
 * kind asks for. Between two of the show's frames the scene is drawn blended from the one before to the latest
 * (Weltall::BlendBattleFrames), and the HUD's moving parts likewise from their places in both (Weltall::BattleHudBlendKeys).
 * While a screen wave holds the battle, the display shows the picture of the screen before it in the wave's mesh.
 */
class FBattleView
{
public:
	explicit FBattleView(const FFieldRenderOptions& InOptions);
	~FBattleView();

	/** How the next frames are drawn (the player's settings changed). */
	void SetOptions(const FFieldRenderOptions& InOptions) { Options = InOptions; }

	/** How the player's buttons are shown, from the next frame the display shows; none keeps the game's pictures. It must outlive the view. */
	void SetButtonGlyphs(const Weltall::IButtonGlyphs* InGlyphs) { Glyphs = InGlyphs; }

	/** Whether the game has the Japanese commands (Weltall::FKernelPad::HasJapaneseCommands): its roles' buttons and its sheet's pictures. */
	void SetJapaneseCommands(bool bInJapaneseCommands)
	{
		bJapaneseCommands = bInJapaneseCommands;
		Icons.SetJapaneseCommands(bInJapaneseCommands);
	}

	/**
	 * After each of the program's frames: VRAM as the show's frame left it. bNewProgram: another battle, whose frames start
	 * afresh, after Shown, the last frame the display showed before it (none: black).
	 */
	void Follow(const Weltall::FBattleGameProgram& Program, bool bNewProgram, const TSharedPtr<const IScreenFrame>& Shown);

	/**
	 * What the display shows: the transition Alpha of the way from its frame before to its latest, then the show's
	 * latest frame Alpha of the way from the one before it, when that one was displayed in the program's frame before.
	 */
	TSharedPtr<const IScreenFrame> Capture(const Weltall::FBattleGameProgram& Program, float Alpha) const;

private:
	FFieldRenderOptions Options;
	TSharedRef<FVramTexture> Vram;
	/** What the blend needs of the show's latest frame and of the one before it, and whether that one came a program's frame before. */
	Weltall::FBattleFrameKeep LatestKept;
	Weltall::FBattleFrameKeep PreviousKept;
	bool bKept = false;
	bool bPreviousFollows = false;
	mutable Weltall::FBattleFrameBlend Blend;
	mutable Weltall::FBattleFrameBlendScratch BlendScratch;
	/** The field scene's passes draw a scene's mesh and the frame's own polygons; the battle's are all its own. */
	TSharedRef<const FFieldSceneMesh> NoMesh;
	/** The HUD's primitives as the menus' builder takes them, and the builder. */
	mutable Weltall::FMenuFrame HudFrame;
	mutable Weltall::FMenuDrawBuilder HudBuilder;
	/** The player's button icons: the glyphs, the port's pictures and their VRAM on the GPU. */
	const Weltall::IButtonGlyphs* Glyphs = nullptr;
	bool bJapaneseCommands = false;
	mutable Weltall::FMenuButtonIcons Icons;
	TSharedRef<FVramTexture> IconVram;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FBattleScreenFrame>> DisplayFrames;
	/** What the frames' drawing knows of the texels VRAM holds. */
	mutable Weltall::FTexelKindCache TexelKinds;
	/** The picture of the screen before the battle, kept while its transition runs, and the transition's drawing. */
	TSharedPtr<FFieldCoverPicture> ScreenBefore;
	mutable FBattleTransitionDrawBuilder Transition;
	mutable TArray<TSharedRef<FBattleTransitionFrame>> TransitionFrames;
	/** The picture of the screen before a screen wave and the wave's draw buffers, kept while it runs. */
	TSharedPtr<FFieldCoverPicture> ScreenWave;
	TSharedPtr<FBattleScreenWaveBuffers> WaveBuffers;
	mutable TArray<TSharedRef<FBattleScreenWaveFrame>> WaveFrames;
};
