#include "Rendering/CrtRendering.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "Presentation/ScreenLayout.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"

namespace
{
	class FCrtLinesPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCrtLinesPS);
		SHADER_USE_PARAMETER_STRUCT(FCrtLinesPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Picture)
			SHADER_PARAMETER(FIntPoint, PictureSize)
			SHADER_PARAMETER(float, LinePitch)
			SHADER_PARAMETER(float, FirstLineTop)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FCrtSignalPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCrtSignalPS);
		SHADER_USE_PARAMETER_STRUCT(FCrtSignalPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Lines)
			SHADER_PARAMETER_SAMPLER(SamplerState, LinearBorder)
			SHADER_PARAMETER(FIntPoint, LinesSize)
			SHADER_PARAMETER(float, LumaSigma)
			SHADER_PARAMETER(float, ChromaSigma)
			SHADER_PARAMETER(float, TubeGamma)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FCrtGlowAcrossPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCrtGlowAcrossPS);
		SHADER_USE_PARAMETER_STRUCT(FCrtGlowAcrossPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Signal)
			SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
			SHADER_PARAMETER(FIntPoint, SignalSize)
			SHADER_PARAMETER(float, LinePitch)
			SHADER_PARAMETER(float, GlowSigma)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FCrtGlowDownPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCrtGlowDownPS);
		SHADER_USE_PARAMETER_STRUCT(FCrtGlowDownPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlowAcross)
			SHADER_PARAMETER(FIntPoint, GlowSize)
			SHADER_PARAMETER(float, GlowSigma)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	class FCrtScreenPS final : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCrtScreenPS);
		SHADER_USE_PARAMETER_STRUCT(FCrtScreenPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Signal)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Glow)
			SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
			SHADER_PARAMETER(FIntPoint, PictureSize)
			SHADER_PARAMETER(FIntPoint, GlowSize)
			SHADER_PARAMETER(FVector2f, AreaMin)
			SHADER_PARAMETER(float, LinePitch)
			SHADER_PARAMETER(float, FirstLineTop)
			SHADER_PARAMETER(int32, LineCount)
			SHADER_PARAMETER(float, BeamMin)
			SHADER_PARAMETER(float, BeamMax)
			SHADER_PARAMETER(int32, MaskKind)
			SHADER_PARAMETER(int32, MaskPitch)
			SHADER_PARAMETER(float, MaskStrength)
			SHADER_PARAMETER(int32, SlotPitch)
			SHADER_PARAMETER(int32, SlotGap)
			SHADER_PARAMETER(float, GlowAmount)
			SHADER_PARAMETER(FVector2f, Curve)
			SHADER_PARAMETER(float, CornerRadius)
			SHADER_PARAMETER(float, Vignette)
			SHADER_PARAMETER(float, DisplayGamma)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()
	};

	/** Draws Rect of Target; a target drawn only in part keeps the rest (Load). */
	template <typename TShader>
	void AddCrtPass(FRDGBuilder& GraphBuilder, FRDGEventName&& Name, typename TShader::FParameters* Parameters, FRDGTextureRef Target, const FIntRect& Rect,
		ERenderTargetLoadAction Load = ERenderTargetLoadAction::ENoAction)
	{
		Parameters->RenderTargets[0] = FRenderTargetBinding(Target, Load);
		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const TShaderMapRef<TShader> PixelShader(ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, MoveTemp(Name), PixelShader, Parameters, Rect);
	}

	FRDGTextureRef CreateLightTexture(FRDGBuilder& GraphBuilder, FIntPoint Size, const TCHAR* Name)
	{
		return GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(Size, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), Name);
	}
}

IMPLEMENT_GLOBAL_SHADER(FCrtLinesPS, "/Weltall/Private/Crt.usf", "LinesPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCrtSignalPS, "/Weltall/Private/Crt.usf", "SignalPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCrtGlowAcrossPS, "/Weltall/Private/Crt.usf", "GlowAcrossPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCrtGlowDownPS, "/Weltall/Private/Crt.usf", "GlowDownPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCrtScreenPS, "/Weltall/Private/Crt.usf", "ScreenPS", SF_Pixel);

void AddCrtPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Picture, FRDGTextureRef Output, const FScreenArea& Area, const Weltall::FCrtParameters& Look,
	Weltall::FScreenSize Screen)
{
	const FIntPoint Size = Area.Size();
	if (Size.X <= 0 || Size.Y <= 0 || Screen.Height <= 0)
	{
		return;
	}
	// The rows as the game screen's 2D layer is placed; the lines go on above and below it to the picture's edges.
	const Weltall::FDisplayRect Placed = Area.Layout(Screen).ToDisplay({0, 0, static_cast<int16>(Screen.Width), static_cast<int16>(Screen.Height)});
	const float LinePitch = static_cast<float>(Placed.Height) / Screen.Height;
	if (LinePitch <= 0.0f)
	{
		return;
	}
	const float FirstLineTop = Placed.Y - FMath::CeilToFloat(Placed.Y / LinePitch) * LinePitch;
	const int32 LineCount = FMath::CeilToInt32((Size.Y - FirstLineTop) / LinePitch);
	const FIntPoint LinesSize(Size.X, LineCount);
	const FIntPoint GlowSize(FMath::CeilToInt32(Size.X / LinePitch), LineCount);

	FRDGTextureRef Lines = CreateLightTexture(GraphBuilder, LinesSize, TEXT("CrtLines"));
	{
		FCrtLinesPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtLinesPS::FParameters>();
		Parameters->Picture = Picture;
		Parameters->PictureSize = Size;
		Parameters->LinePitch = LinePitch;
		Parameters->FirstLineTop = FirstLineTop;
		AddCrtPass<FCrtLinesPS>(GraphBuilder, RDG_EVENT_NAME("CrtLines"), Parameters, Lines, FIntRect(FIntPoint::ZeroValue, LinesSize));
	}
	FRDGTextureRef Signal = CreateLightTexture(GraphBuilder, LinesSize, TEXT("CrtSignal"));
	{
		FCrtSignalPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtSignalPS::FParameters>();
		Parameters->Lines = Lines;
		Parameters->LinearBorder = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Clamp>::GetRHI();
		Parameters->LinesSize = LinesSize;
		Parameters->LumaSigma = FMath::Max(Look.LumaBlur * LinePitch, 0.35f);
		Parameters->ChromaSigma = FMath::Max(Look.ChromaBlur * LinePitch, 0.35f);
		Parameters->TubeGamma = Look.TubeGamma;
		AddCrtPass<FCrtSignalPS>(GraphBuilder, RDG_EVENT_NAME("CrtSignal"), Parameters, Signal, FIntRect(FIntPoint::ZeroValue, LinesSize));
	}
	FRDGTextureRef GlowAcross = CreateLightTexture(GraphBuilder, GlowSize, TEXT("CrtGlowAcross"));
	{
		FCrtGlowAcrossPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtGlowAcrossPS::FParameters>();
		Parameters->Signal = Signal;
		Parameters->LinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->SignalSize = LinesSize;
		Parameters->LinePitch = LinePitch;
		Parameters->GlowSigma = Look.GlowRadius;
		AddCrtPass<FCrtGlowAcrossPS>(GraphBuilder, RDG_EVENT_NAME("CrtGlowAcross"), Parameters, GlowAcross, FIntRect(FIntPoint::ZeroValue, GlowSize));
	}
	FRDGTextureRef Glow = CreateLightTexture(GraphBuilder, GlowSize, TEXT("CrtGlow"));
	{
		FCrtGlowDownPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtGlowDownPS::FParameters>();
		Parameters->GlowAcross = GlowAcross;
		Parameters->GlowSize = GlowSize;
		Parameters->GlowSigma = Look.GlowRadius;
		AddCrtPass<FCrtGlowDownPS>(GraphBuilder, RDG_EVENT_NAME("CrtGlowDown"), Parameters, Glow, FIntRect(FIntPoint::ZeroValue, GlowSize));
	}
	{
		// The mask's triads keep their share of the picture at any size, three display pixels at the least.
		const int32 MaskPitch = FMath::Max(3, FMath::RoundToInt32(Size.Y / Look.TriadsPerHeight));
		FCrtScreenPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtScreenPS::FParameters>();
		Parameters->Signal = Signal;
		Parameters->Glow = Glow;
		Parameters->LinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->PictureSize = Size;
		Parameters->GlowSize = GlowSize;
		Parameters->AreaMin = FVector2f(Area.Min);
		Parameters->LinePitch = LinePitch;
		Parameters->FirstLineTop = FirstLineTop;
		Parameters->LineCount = LineCount;
		Parameters->BeamMin = Look.BeamMin;
		Parameters->BeamMax = Look.BeamMax;
		Parameters->MaskKind = Look.MaskKind;
		Parameters->MaskPitch = MaskPitch;
		Parameters->MaskStrength = Look.MaskStrength;
		Parameters->SlotPitch = 2 * MaskPitch;
		Parameters->SlotGap = FMath::Max(1, MaskPitch / 4);
		Parameters->GlowAmount = Look.GlowAmount;
		Parameters->Curve = FVector2f(Look.CurveX, Look.CurveY);
		Parameters->CornerRadius = Look.CornerRadius * Size.Y;
		Parameters->Vignette = Look.Vignette;
		Parameters->DisplayGamma = Look.DisplayGamma;
		AddCrtPass<FCrtScreenPS>(GraphBuilder, RDG_EVENT_NAME("CrtScreen"), Parameters, Output, Area, ERenderTargetLoadAction::ELoad);
	}
}
