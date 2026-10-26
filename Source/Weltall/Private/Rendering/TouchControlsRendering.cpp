#include "Rendering/TouchControlsRendering.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"

namespace
{
	class FTouchControlsPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FTouchControlsPS);
		SHADER_USE_PARAMETER_STRUCT(FTouchControlsPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector2f, Centre)
			SHADER_PARAMETER(float, Radius)
			SHADER_PARAMETER(uint32, Kind)
			SHADER_PARAMETER(uint32, Held)
			SHADER_PARAMETER(FVector2f, Knob)
			SHADER_PARAMETER(float, KnobRadius)
			SHADER_PARAMETER(float, Opacity)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};
}

IMPLEMENT_GLOBAL_SHADER(FTouchControlsPS, "/Weltall/Private/TouchControls.usf", "MainPS", SF_Pixel);

void AddTouchControlsPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Output, const FTouchControlsDrawn& Drawn)
{
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const TShaderMapRef<FTouchControlsPS> PixelShader(ShaderMap);
	const FIntRect Target(FIntPoint::ZeroValue, Output->Desc.Extent);
	for (const FTouchControlsDrawn::FControl& Control : Drawn.Controls)
	{
		// The control's square, a pixel past its rim for the edge's smoothing (the stick's past its knob pushed to the rim).
		constexpr uint32 StickKind = 11;
		const float Reach = Control.Radius * (Control.Kind == StickKind ? 1.0f + FTouchControlsDrawn::KnobRadius : 1.0f) + 2.0f;
		FIntRect Area(FIntPoint(FMath::FloorToInt32(Control.Centre.X - Reach), FMath::FloorToInt32(Control.Centre.Y - Reach)),
			FIntPoint(FMath::CeilToInt32(Control.Centre.X + Reach), FMath::CeilToInt32(Control.Centre.Y + Reach)));
		Area.Clip(Target);
		if (Area.Area() <= 0)
		{
			continue;
		}
		FTouchControlsPS::FParameters* Parameters = GraphBuilder.AllocParameters<FTouchControlsPS::FParameters>();
		Parameters->Centre = Control.Centre;
		Parameters->Radius = Control.Radius;
		Parameters->Kind = Control.Kind;
		Parameters->Held = Control.Held;
		Parameters->Knob = Control.Knob;
		Parameters->KnobRadius = FTouchControlsDrawn::KnobRadius;
		Parameters->Opacity = Drawn.Opacity;
		Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("TouchControl"), PixelShader, Parameters, Area,
			TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI());
	}
}
