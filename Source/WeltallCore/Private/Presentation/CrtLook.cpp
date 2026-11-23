#include "Presentation/CrtLook.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A step's share of the whole range, 0 to 1. */
		float Share(int32 Step)
		{
			return static_cast<float>(std::clamp(Step, 0, FCrtLook::Steps)) / FCrtLook::Steps;
		}

		float Steps(int32 Step)
		{
			return Share(Step) * FCrtLook::Steps;
		}
	}

	FCrtParameters CrtParameters(const FCrtLook& Look)
	{
		FCrtParameters Parameters;
		Parameters.LumaBlur = 0.50f - 0.047f * Steps(Look.Sharpness);
		Parameters.ChromaBlur = Parameters.LumaBlur + 0.10f * Steps(Look.ColorBleed);
		Parameters.ScanlineStrength = Share(Look.Scanlines);
		Parameters.MaskKind = Look.Mask == ECrtMask::ApertureGrille ? 1 : Look.Mask == ECrtMask::SlotMask ? 2 : 0;
		Parameters.TriadsPerHeight = Look.MaskSize == ECrtMaskSize::Fine ? 480.0f : Look.MaskSize == ECrtMaskSize::Medium ? 360.0f : 270.0f;
		Parameters.MaskStrength = Share(Look.MaskStrength);
		Parameters.GlowAmount = 0.01f * Steps(Look.Glow);
		Parameters.GlowRadius = 0.75f + 0.25f * Steps(Look.Glow);
		Parameters.CurveX = 0.01f * Steps(Look.Curvature);
		Parameters.CurveY = 0.015f * Steps(Look.Curvature);
		Parameters.CornerRadius = 0.01f * Steps(Look.Corners);
		Parameters.Vignette = 0.05f * Steps(Look.Vignette);
		return Parameters;
	}
}
