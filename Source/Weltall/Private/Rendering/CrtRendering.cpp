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
			SHADER_PARAMETER(FIntPoint, SourceSize)
			SHADER_PARAMETER(float, SourceLinePitch)
			SHADER_PARAMETER(float, FirstSourceLineTop)
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
			SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
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
			SHADER_PARAMETER(float, SourceLinePitch)
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
			SHADER_PARAMETER(FIntPoint, OutputSize)
			SHADER_PARAMETER(FIntPoint, GlowSize)
			SHADER_PARAMETER(FVector2f, AreaMin)
			SHADER_PARAMETER(float, SourceLinePitch)
			SHADER_PARAMETER(float, LinePitch)
			SHADER_PARAMETER(float, FirstLineTop)
			SHADER_PARAMETER(int32, LineCount)
			SHADER_PARAMETER(float, ScanlineStrength)
			SHADER_PARAMETER(int32, MaskKind)
			SHADER_PARAMETER(int32, MaskPitch)
			SHADER_PARAMETER(float, MaskStrength)
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

void AddCrtPasses(FRDGBuilder& GraphBuilder, FRDGTextureRef Picture, FRDGTextureRef Output, const FScreenArea& DrawArea, const FScreenArea& Area,
	const Weltall::FCrtParameters& Look, Weltall::FScreenSize Screen)
{
	const FIntPoint SourceSize = DrawArea.Size();
	const FIntPoint Size = Area.Size();
	const double SourcePitch = DrawArea.Layout(Screen).Scale();
	if (SourcePitch <= 0.0)
	{
		return;
	}
	// Extend the centred raster through any margins. Map the same lines into the final display area.
	const double InactiveRows = 0.5 * (SourceSize.Y / SourcePitch - Screen.Height);
	const int32 MarginLines = FMath::CeilToInt32(FMath::Max(0.0, InactiveRows - UE_DOUBLE_SMALL_NUMBER));
	const int32 LineCount = Screen.Height + 2 * MarginLines;
	const float FirstSourceLineTop = static_cast<float>(0.5 * (SourceSize.Y - LineCount * SourcePitch));
	const float VerticalScale = static_cast<float>(Size.Y) / SourceSize.Y;
	const float SourceLinePitch = static_cast<float>(SourcePitch);
	const FIntPoint LinesSize(SourceSize.X, LineCount);
	const FIntPoint GlowSize(FMath::CeilToInt32(SourceSize.X / SourcePitch), LineCount);
	FRHISamplerState* LinearClamp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FRDGTextureRef Lines = CreateLightTexture(GraphBuilder, LinesSize, TEXT("CrtLines"));
	{
		FCrtLinesPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtLinesPS::FParameters>();
		Parameters->Picture = Picture;
		Parameters->SourceSize = SourceSize;
		Parameters->SourceLinePitch = SourceLinePitch;
		Parameters->FirstSourceLineTop = FirstSourceLineTop;
		AddCrtPass<FCrtLinesPS>(GraphBuilder, RDG_EVENT_NAME("CrtLines"), Parameters, Lines, FIntRect(FIntPoint::ZeroValue, LinesSize));
	}
	FRDGTextureRef Signal = CreateLightTexture(GraphBuilder, LinesSize, TEXT("CrtSignal"));
	{
		FCrtSignalPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtSignalPS::FParameters>();
		Parameters->Lines = Lines;
		Parameters->LinearClamp = LinearClamp;
		Parameters->LinesSize = LinesSize;
		Parameters->LumaSigma = Look.LumaBlur * SourceLinePitch;
		Parameters->ChromaSigma = Look.ChromaBlur * SourceLinePitch;
		Parameters->TubeGamma = Look.TubeGamma;
		AddCrtPass<FCrtSignalPS>(GraphBuilder, RDG_EVENT_NAME("CrtSignal"), Parameters, Signal, FIntRect(FIntPoint::ZeroValue, LinesSize));
	}
	FRDGTextureRef Glow = Signal;
	if (Look.GlowAmount > 0.0f)
	{
		const FIntRect GlowRect(FIntPoint::ZeroValue, GlowSize);
		FRDGTextureRef GlowAcross = CreateLightTexture(GraphBuilder, GlowSize, TEXT("CrtGlowAcross"));
		FCrtGlowAcrossPS::FParameters* Across = GraphBuilder.AllocParameters<FCrtGlowAcrossPS::FParameters>();
		Across->Signal = Signal;
		Across->LinearClamp = LinearClamp;
		Across->SignalSize = LinesSize;
		Across->SourceLinePitch = SourceLinePitch;
		Across->GlowSigma = Look.GlowRadius;
		AddCrtPass<FCrtGlowAcrossPS>(GraphBuilder, RDG_EVENT_NAME("CrtGlowAcross"), Across, GlowAcross, GlowRect);

		Glow = CreateLightTexture(GraphBuilder, GlowSize, TEXT("CrtGlow"));
		FCrtGlowDownPS::FParameters* Down = GraphBuilder.AllocParameters<FCrtGlowDownPS::FParameters>();
		Down->GlowAcross = GlowAcross;
		Down->GlowSize = GlowSize;
		Down->GlowSigma = Look.GlowRadius;
		AddCrtPass<FCrtGlowDownPS>(GraphBuilder, RDG_EVENT_NAME("CrtGlowDown"), Down, Glow, GlowRect);
	}
	{
		// The mask's triads keep their share of the picture at any size, three display pixels at the least.
		const int32 MaskPitch = FMath::Max(3, FMath::RoundToInt32(Size.Y / Look.TriadsPerHeight));
		FCrtScreenPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCrtScreenPS::FParameters>();
		Parameters->Signal = Signal;
		Parameters->Glow = Glow;
		Parameters->LinearClamp = LinearClamp;
		Parameters->OutputSize = Size;
		Parameters->GlowSize = GlowSize;
		Parameters->AreaMin = FVector2f(Area.Min);
		Parameters->SourceLinePitch = SourceLinePitch;
		Parameters->LinePitch = SourceLinePitch * VerticalScale;
		Parameters->FirstLineTop = FirstSourceLineTop * VerticalScale;
		Parameters->LineCount = LineCount;
		Parameters->ScanlineStrength = Look.ScanlineStrength;
		Parameters->MaskKind = Look.MaskKind;
		Parameters->MaskPitch = MaskPitch;
		Parameters->MaskStrength = Look.MaskStrength;
		Parameters->GlowAmount = Look.GlowAmount;
		Parameters->Curve = FVector2f(Look.CurveX, Look.CurveY);
		Parameters->CornerRadius = Look.CornerRadius * Size.Y;
		Parameters->Vignette = Look.Vignette;
		Parameters->DisplayGamma = Look.DisplayGamma;
		AddCrtPass<FCrtScreenPS>(GraphBuilder, RDG_EVENT_NAME("CrtScreen"), Parameters, Output, Area, ERenderTargetLoadAction::ELoad);
	}
}
