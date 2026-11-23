#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The phosphors behind a CRT's glass. */
	enum class ECrtMask : uint8
	{
		None,
		ApertureGrille,
		SlotMask,
	};

	/** How fine a mask's triads are. */
	enum class ECrtMaskSize : uint8
	{
		Fine,
		Medium,
		Coarse,
	};

	/** A CRT's look as the player sets it: each part a step from 0 to Steps, or a choice. Its defaults are the studio monitor's. */
	struct FCrtLook
	{
		static constexpr int32 Steps = 10;

		/** How dark the gaps between the lines are; at 0 the lines merge. */
		int32 Scanlines = 9;
		ECrtMask Mask = ECrtMask::ApertureGrille;
		int32 MaskStrength = 4;
		ECrtMaskSize MaskSize = ECrtMaskSize::Fine;
		/** How sharp each line is along itself. */
		int32 Sharpness = 10;
		/** How much further the colour spreads along a line than the brightness. */
		int32 ColorBleed = 0;
		int32 Glow = 1;
		/** How much the glass bulges, its corners' roundness and its darkening towards them. */
		int32 Curvature = 0;
		int32 Corners = 0;
		int32 Vignette = 0;

		/** A studio monitor fed RGB: sharp, a fine aperture grille, a flat face. */
		static constexpr FCrtLook Monitor() { return {}; }
		/** A television fed S-Video: softer colour, a slot mask, curved glass. */
		static constexpr FCrtLook Television() { return {7, ECrtMask::SlotMask, 4, ECrtMaskSize::Medium, 7, 3, 3, 3, 4, 4}; }

		bool operator==(const FCrtLook&) const = default;
	};

	/** What the CRT's passes draw with, from a look (Shaders/Private/Crt.usf names them). */
	struct FCrtParameters
	{
		/** The signal's Gaussian along a line, its brightness's and its colour's, in the game screen's pixels. */
		float LumaBlur = 0.0f;
		float ChromaBlur = 0.0f;
		/** The blend between a continuous picture and individual scanline beams. */
		float ScanlineStrength = 0.0f;
		/** Crt.usf's mask kind (0 none, 1 an aperture grille, 2 a slot mask), its triads across a picture's height, and its strength. */
		int32 MaskKind = 0;
		float TriadsPerHeight = 0.0f;
		float MaskStrength = 0.0f;
		/** The light diffused in the glass: its Gaussian in lines, and the share of it added. */
		float GlowRadius = 0.0f;
		float GlowAmount = 0.0f;
		/** The glass's bulge (Crt.usf's Curve), its corners' radius in picture heights, and its darkening at the corners. */
		float CurveX = 0.0f;
		float CurveY = 0.0f;
		float CornerRadius = 0.0f;
		float Vignette = 0.0f;
		/** The tube's gamma, which its signal was made for, and the display's. */
		float TubeGamma = 2.4f;
		float DisplayGamma = 2.2f;
	};

	/** The parameters a look draws with; each step moves its part evenly, a step past or short of the range clamped. */
	WELTALLCORE_API FCrtParameters CrtParameters(const FCrtLook& Look);
}
