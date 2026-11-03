#pragma once

#include "Foundation/Types.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuPrimitive.h"
#include "Presentation/DrawList.h"

#include <utility>
#include <vector>

namespace Weltall
{
	/** The screen the menus draw on: 320 x 224 pixels, the whole of it their drawing area. */
	inline constexpr FScreenSize MenuScreenSize{320, 224};

	/**
	 * A frame of a menu as the display draws it Alpha of the way from Previous to Latest (0 Previous,
	 * 1 Latest): Latest's primitives in its order, each with a partner in Previous (the same blend key)
	 * moved that far from it, and the 3D ones projected in floating point through the layer's
	 * transform as far between the two frames' (the angles the short way round), so the layer turns
	 * and flies smoothly at any size. A 2D draw list over the menu's VRAM, with the GPU's rules.
	 * Without Previous, Latest as it is. Out and Scratch keep their memory from call to call.
	 */
	struct FMenuDrawBuilder
	{
		WELTALLCORE_API void Build(const FMenuFrame& Latest, const FMenuFrame* Previous, float Alpha, FDrawList& Out);

		/**
		 * The drawing area widened with the display: what the menus place beyond the 320 x 224 screen's
		 * edges (the panels sliding in, pieces parked off screen) is drawn where a wider display shows
		 * it. Off, as the original's GPU clips it.
		 */
		bool bWideDrawingArea = false;

		/** The screen the list is drawn on, the whole of it the drawing area: the menus', or another program's own. */
		FScreenSize Screen = MenuScreenSize;

		/**
		 * How far, in pixels along either axis, a corner of a primitive on the screen may have moved from its partner's
		 * for the two to blend: one that moved further is drawn where it is now, as a part laid out afresh rather than
		 * moved. 0: any distance.
		 */
		float BlendReach = 0.0f;

		/** The button pictures of the last list built (the primitives with a ButtonRole), for the player's icons (FMenuButtonIcons). */
		std::vector<FMenuDrawnIcon> Icons;
		/** The frame's text buttons (FMenuFrame::TextButtons) as cells of the last list built, for FMenuButtonIcons::ApplyToText. */
		std::vector<FTextButtonCell> TextCells;

	private:
		/** Whether Now, a primitive on the screen, moved no further than BlendReach from Before; one in 3D always is. */
		bool WithinReach(const FMenuPrimitive& Before, const FMenuPrimitive& Now) const;

		std::vector<std::pair<uint32, int32>> PreviousKeys;
		/** Where the list stood as each of the frame's primitives was reached, and at its end. */
		std::vector<uint32> ListPlaces;
	};

	/** A point of the layer projected in floating point through a transform, as the GTE would to the pixel. */
	WELTALLCORE_API std::pair<float, float> ProjectMenuPoint(const FMenuLayerTransform& Transform, float X, float Y, float Z);
}
