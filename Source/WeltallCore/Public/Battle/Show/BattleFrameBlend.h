#pragma once

#include "Battle/Show/BattleShowFrame.h"
#include "Foundation/Types.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/GteMatrix.h"

#include <array>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * Something of a battle frame that moves as one between two of the show's frames: a drawer (FBattleShowFrame::Drawers),
	 * a 3D object's part or an object's shadow. Its matrix takes its own points to view space; its place in the world tells
	 * a jump from a move.
	 */
	struct FBattleFrameAnchor
	{
		/** What it is, its own key and its count among those of that key before it: the same thing from frame to frame. */
		uint64 Key = 0;
		FAffine Matrix;
		std::array<float, 3> World{};
	};

	/** A cloth's triangle as a frame left it: its points in the world, each with its key. */
	struct FBattleKeptCloth
	{
		std::array<FGteVector, 3> Points{};
		std::array<uint32, 3> PointKeys{};
	};

	/**
	 * What the blending between frames needs of one of the show's frames, kept apart from it: the show reuses a frame's memory
	 * once the next is displayed, and the objects' part is rebuilt in place.
	 */
	struct FBattleFrameKeep
	{
		uint32 Step = 0;
		FGteMatrix View;
		std::array<int16, 3> Eye{};
		std::array<int16, 3> Target{};
		int32 CentreX = 0;
		int32 CentreY = 0;
		int32 ProjectionDistance = 0;
		std::array<uint8, 3> ClearColor{};
		/** The fade layer, when the frame links one. */
		bool bFade = false;
		FBattleShowFade Fade;
		/** The drawers in the frame's order, then the objects' parts, then their shadows. */
		std::vector<FBattleFrameAnchor> Anchors;
		uint32 FirstPart = 0;
		uint32 FirstShadow = 0;
		std::vector<FBattleKeptCloth> Cloths;
		FBattleBackdropFrame Backdrop;
		/** The HUD's parts that move between frames (those with a blend key), as the menus' builder takes a frame before. */
		FMenuFrame Hud;
	};

	/**
	 * How a display draws the latest of the show's frames Alpha of the way from the one before it: where each anchor's
	 * points go (its view-space points through its move), the world's points under the view, the cloths' points, the
	 * projection, the backdrop's packets and the screen's colours.
	 */
	struct FBattleFrameBlend
	{
		/** For each anchor of the latest frame, in its order (the drawers, then the parts from FirstPart, the shadows from FirstShadow): the unmoved view where it does not blend. */
		std::vector<FViewMove> Moves;
		uint32 FirstPart = 0;
		uint32 FirstShadow = 0;
		/** For points the latest frame put in view space through its view. */
		FViewMove View = UnmovedView;
		/** Each cloth triangle's points in the world, in the latest frame's order. */
		std::vector<std::array<std::array<float, 3>, 3>> Cloths;
		float CentreX = 0.0f;
		float CentreY = 0.0f;
		float ProjectionDistance = 0.0f;
		/** The latest backdrop, its panoramas placed between the two frames' places. */
		FBattleBackdropFrame Backdrop;
		/** Each of the dome's packets' corners on the screen, in the latest frame's order. */
		std::vector<std::array<std::array<float, 2>, 4>> DomeCorners;
		/** The clear colour as the GPU fills the screen with it (FilledChannel), between the two frames'. */
		std::array<float, 3> ClearColor{};
		std::array<float, 3> FadeColor{};
	};

	/** The blend's working lists, kept from call to call so that it allocates only while they grow. */
	struct FBattleFrameBlendScratch
	{
		std::vector<std::pair<uint64, uint32>> Keys;
		std::vector<std::pair<uint32, FGteVector>> ClothPoints;
	};

	/** Keeps what the blend needs of Frame. Reuses Kept's memory. */
	WELTALLCORE_API void KeepBattleFrame(const FBattleShowFrame& Frame, FBattleFrameKeep& Kept);

	/**
	 * Latest drawn Alpha of the way from Previous, the frame displayed before it: each anchor's matrix blended column by
	 * column (direction and length) and in its translation with the previous frame's of the same key, the view, the
	 * projection, the cloths' points by their keys, the dome's corners and the panoramas' places, the clear colour and the
	 * fade's. Nothing blends across a cut (frames not consecutive, the camera's eye or target jumping or its view turning
	 * sharply), and an anchor or a cloth's point does not where it jumped or turned about. Blend keeps its memory from call
	 * to call.
	 */
	WELTALLCORE_API void BlendBattleFrames(const FBattleFrameKeep& Latest, const FBattleFrameKeep* Previous, float Alpha, FBattleFrameBlend& Blend,
		FBattleFrameBlendScratch& Scratch);
}
