#pragma once

#include "Foundation/Types.h"
#include "Graphics/Texture.h"
#include "Presentation/GameScreen.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace Weltall
{
	/** A color as the GPU takes it, 0-255 per channel; 128 leaves a texel's color unchanged. */
	struct FDrawColor
	{
		uint8 R = 128;
		uint8 G = 128;
		uint8 B = 128;

		bool operator==(const FDrawColor&) const = default;
	};

	/** How a primitive's pixels meet the ones already drawn. */
	enum class EDrawBlend : uint8
	{
		Opaque,
		/** Semi-transparent with one of the GPU's four modes: textured, only texels with bit 15 set blend. */
		SemiTransparent,
		/** Semi-transparent, every texel blending as if VRAM had set its bit 15 (texel 0 stays transparent). */
		SemiTransparentAll,
	};

	/** The GPU's whole drawing range: a primitive clipped to it keeps every pixel a screen can show. */
	inline constexpr FScreenRect WholeDrawingArea{0, 0, 1024, 512};

	/** The texels of a textured primitive: a texture page and CLUT of one of the frame's VRAM states. */
	struct FDrawTexture
	{
		/** Which of the screen's VRAM states. */
		uint16 Vram = 0;
		/** The page's blend mode is the one semi-transparent pixels use. */
		FTexturePage Page;
		FClutPosition Clut;

		bool operator==(const FDrawTexture&) const = default;
	};

	/** A corner of a primitive, in pixels of the game screen. */
	struct FDrawVertex
	{
		float X = 0.0f;
		float Y = 0.0f;
		FDrawColor Color;
		/**
		 * Texel coordinates in the texture page, interpolated across the primitive as the GPU does and
		 * rounded down per pixel: a polygon's corners carry the game's own values, a sprite's right and
		 * bottom edges lie one past its last texel.
		 */
		float U = 0.0f;
		float V = 0.0f;
	};

	/**
	 * One primitive the GPU draws: a triangle, or a four-point polygon drawn as the triangles
	 * (0, 1, 2) and (1, 2, 3), with its corners in the order the game gives them.
	 */
	/** The texels of its page a primitive may show: the first and the last along u and along v. */
	struct FDrawTexels
	{
		uint8 FirstU = 0;
		uint8 FirstV = 0;
		uint8 LastU = 0;
		uint8 LastV = 0;

		bool operator==(const FDrawTexels&) const = default;
	};

	struct FDrawPrimitive
	{
		std::array<FDrawVertex, 4> Corners{};
		uint8 CornerCount = 4;
		/** Colors blend across the primitive; otherwise the first corner's color covers it all. */
		bool bGouraud = false;
		std::optional<FDrawTexture> Texture;
		/**
		 * The texels the primitive shows and no others, where its corners' coordinates reach past them: a polygon
		 * drawn smaller or larger than its picture has them stepped from half a texel in, which on a display finer
		 * than the console's would show a sliver of the picture beside its own. None: whatever lies between its corners.
		 */
		std::optional<FDrawTexels> Texels;
		/** Texels are multiplied by the color (128 = unchanged); otherwise drawn as they are. */
		bool bModulate = true;
		EDrawBlend Blend = EDrawBlend::Opaque;
		/** The blend mode of an untextured semi-transparent primitive; a textured one takes its page's. */
		EBlendMode UntexturedBlend = EBlendMode::Average;
		/** Only the pixels inside this rectangle are drawn, as the GPU's drawing area limits them. */
		FScreenRect Clip = WholeDrawingArea;
		/**
		 * Spread over the whole display it is drawn on, however wide, its corners there (top left, top right, bottom left,
		 * bottom right) whatever their places here, its texels across it as they lie: a fill or a picture covering the screen.
		 */
		bool bSpansDisplay = false;
		/** Follow the right edge when the picture extends past the game screen. */
		bool bAnchorRight = false;
		/**
		 * Non-zero for a primitive that moves or changes size between ticks: the one with the same key
		 * in the tick before is drawn in between at the display's rate. Keys are unique in a list.
		 */
		uint32 BlendKey = 0;
	};

	/** A screen's 2D layer for one tick: primitives in drawing order, later ones over earlier ones. */
	struct FDrawList
	{
		std::vector<FDrawPrimitive> Primitives;
	};

	/**
	 * For each primitive of Later, the index of the primitive of Earlier with the same blend key and
	 * as many corners, or -1: the pairs a display draws in between.
	 */
	WELTALLCORE_API std::vector<int32> MatchBlendKeys(const FDrawList& Earlier, const FDrawList& Later);

	/** MatchBlendKeys into Matches, sorting in Scratch: neither allocates once both have grown to the lists' sizes. */
	WELTALLCORE_API void MatchBlendKeys(const FDrawList& Earlier, const FDrawList& Later, std::vector<int32>& Matches,
		std::vector<std::pair<uint32, int32>>& Scratch);

	/** An axis-aligned rectangle of one color, or with a color at each corner (top left, top right, bottom left, bottom right). */
	WELTALLCORE_API FDrawPrimitive MakeGouraudRect(const FScreenRect& Rect, const std::array<FDrawColor, 4>& Colors);

	/** A texture rectangle drawn at its own size, texel for pixel, as the GPU's sprites are. */
	WELTALLCORE_API FDrawPrimitive MakeSprite(int32 X, int32 Y, int32 Width, int32 Height, const FDrawTexture& Texture, uint8 U, uint8 V,
		FDrawColor Color = {});
}
