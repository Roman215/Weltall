#pragma once

#include "CoreMinimal.h"
#include "Field/FieldFrame.h"
#include "Field/FieldGearDrawing.h"
#include "Field/FieldScene.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Menu/MenuPrimitive.h"
#include "Rendering/FieldSceneRendering.h"
#include "Screens/GameScreen.h"

#include <array>
#include <vector>

class FMenuScreenFrame;
class FVramTexture;

namespace Weltall
{
	class FGearModels;
	class FGearShopModel;
	class FMenuProgram;
	class FMenuSpriteSheet;
	class IButtonGlyphs;
}

/**
 * The Menu program shown at the display's rate: its 2D layer and 3D layer drawn between its latest two
 * frames (FMenuDrawBuilder: the layer's turn and flight in floating point, what moves by its blend key),
 * over the picture it was opened over, the 320 x 224 screen fitted to the display, textured from the
 * menu's own VRAM. The gear shop's screen is black instead, its gear drawn under the menu as the field
 * draws its gears (under the field's options), between the same two frames. Its button pictures show
 * the player's own buttons (Weltall::FMenuButtonIcons), as the glyph service shows them.
 */
class FMenuView
{
public:
	explicit FMenuView(const FFieldRenderOptions& InOptions);
	~FMenuView();

	/** After each of the menu's frames: its latest frame and VRAM. A menu opened afresh starts a new pair of frames. */
	void Follow(const Weltall::FMenuProgram& Menu);

	/** No menu runs: the next one starts afresh. */
	void Reset();

	/** How the gear shop's gear is drawn from the next frame the display shows. */
	void SetOptions(const FFieldRenderOptions& InOptions) { Options = InOptions; }

	/** Whether the menus draw past the 320 x 224 screen's edges where the display shows them (Weltall::FMenuDrawBuilder::bWideDrawingArea). */
	void SetWideDrawingArea(bool bWide) { Builder.bWideDrawingArea = bWide; }

	/** How the player's buttons are shown, from the next frame the display shows; none keeps the game's pictures. It must outlive the view. */
	void SetButtonGlyphs(const Weltall::IButtonGlyphs* InGlyphs) { Glyphs = InGlyphs; }

	/** Whether the game has the Japanese commands (Weltall::FKernelPad::HasJapaneseCommands): its roles' buttons and its sheet's pictures. */
	void SetJapaneseCommands(bool bInJapaneseCommands)
	{
		bJapaneseCommands = bInJapaneseCommands;
		Icons.SetJapaneseCommands(bInJapaneseCommands);
	}

	/** What the display shows Alpha of the way from the menu's frame before its latest to the latest, over Background; nothing before its first. */
	TSharedPtr<const IScreenFrame> Capture(TSharedPtr<const IScreenFrame> Background, float Alpha) const;

private:
	/** The gear shop's gear in the frame kept at Next: its drawing evaluated, the mesh made again when its models change. */
	void FollowGear(const Weltall::FGearShopModel& Model, const Weltall::FMenuFrame& Snapshot, int32 Next);

	FFieldRenderOptions Options;
	TSharedRef<FVramTexture> Vram;
	const Weltall::FVram* SourceVram = nullptr;
	/** The last two frames: the latest, and the one before it the display draws from. */
	std::array<Weltall::FMenuFrame, 2> Frames;
	int32 Latest = -1;
	bool bPrevious = false;
	mutable Weltall::FMenuDrawBuilder Builder;
	/** The player's button icons: the glyphs, the menus' sheet they swap the game's own symbols from, the port's pictures and their VRAM on the GPU. */
	const Weltall::IButtonGlyphs* Glyphs = nullptr;
	bool bJapaneseCommands = false;
	const Weltall::FMenuSpriteSheet* Sheet = nullptr;
	mutable Weltall::FMenuButtonIcons Icons;
	TSharedRef<FVramTexture> IconVram;
	/** The gear shop's gear in the same two frames, and the mesh of its models (no scenery), which the display frames share. */
	bool bGearScene = false;
	std::array<Weltall::FFieldFrame, 2> GearFrames;
	Weltall::FFieldScene NoScenery;
	TSharedPtr<const FFieldSceneMesh> GearMesh;
	const Weltall::FGearModels* MeshModels = nullptr;
	std::vector<Weltall::FFieldGearLayoutItem> GearLayout;
	/** Frames handed out; one the rendering thread no longer holds is filled again. */
	mutable TArray<TSharedRef<FMenuScreenFrame>> DisplayFrames;
};
