#include "Rendering/DrawListRendering.h"

#include "CommonRenderResources.h"
#include "DrawListShaders.h"
#include "Menu/MenuButtonIcons.h"
#include "PipelineStateCache.h"
#include "Presentation/DrawList.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Rendering/DrawListTextures.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** Which pixels a draw keeps; the values match ScreenDrawList.usf. */
	enum class EDrawPass : uint32
	{
		Opaque = 0,
		PlainTexels = 1,
		SemiTransparentTexels = 2,
		BlendAll = 3,
	};

	constexpr uint32 NoTexture = 0xFFFF;
	constexpr uint32 FlagModulate = 1;

	/** Consecutive triangles drawn with one state. */
	struct FDrawRun
	{
		uint32 FirstVertex = 0;
		uint32 VertexCount = 0;
		EDrawPass Pass = EDrawPass::Opaque;
		Weltall::EBlendMode Mode = Weltall::EBlendMode::Average;
		/**
		 * The two draws of a semi-transparent textured primitive stay on their own: joining them to
		 * their neighbors' would change which pixels land over which.
		 */
		bool bJoinable = true;
	};

	using FDrawRuns = TArray<FDrawRun, SceneRenderingAllocator>;

	float Lerp(float From, float To, float Alpha)
	{
		return From + (To - From) * Alpha;
	}

	/** Where a draw list's textured primitives take their texels: decoded layers, or VRAM itself. */
	struct FTextureSource
	{
		const FDrawListTextures* Layers = nullptr;
		const FVramTexture* Vram = nullptr;
	};

	/** Bounds of a primitive whose texels wrap round its page's edge: none. */
	constexpr uint32 NoTexelBounds = 0x000000FF;

	/** The texels a primitive names as its own, else those between its corners: its coordinates only ever lie between their least and greatest. */
	uint32 TexelBoundsOf(const Weltall::FDrawPrimitive& Primitive)
	{
		if (Primitive.Texels)
		{
			const Weltall::FDrawTexels& Own = *Primitive.Texels;
			return static_cast<uint32>(Own.FirstU) | static_cast<uint32>(Own.FirstV) << 8 | static_cast<uint32>(Own.LastU) << 16 | static_cast<uint32>(Own.LastV) << 24;
		}
		float LowU = Primitive.Corners[0].U, LowV = Primitive.Corners[0].V, HighU = LowU, HighV = LowV;
		for (int32 Corner = 1; Corner < Primitive.CornerCount; ++Corner)
		{
			LowU = FMath::Min(LowU, Primitive.Corners[Corner].U);
			LowV = FMath::Min(LowV, Primitive.Corners[Corner].V);
			HighU = FMath::Max(HighU, Primitive.Corners[Corner].U);
			HighV = FMath::Max(HighV, Primitive.Corners[Corner].V);
		}
		if (LowU < 0.0f || LowV < 0.0f || HighU > 256.0f || HighV > 256.0f)
		{
			return NoTexelBounds;
		}
		const auto Byte = [](float Value) { return static_cast<uint32>(FMath::Clamp(Value, 0.0f, 255.0f)); };
		const uint32 FirstU = Byte(FMath::FloorToFloat(LowU));
		const uint32 FirstV = Byte(FMath::FloorToFloat(LowV));
		const uint32 LastU = FMath::Max(FirstU, Byte(FMath::CeilToFloat(HighU) - 1.0f));
		const uint32 LastV = FMath::Max(FirstV, Byte(FMath::CeilToFloat(HighV) - 1.0f));
		return FirstU | FirstV << 8 | LastU << 16 | LastV << 24;
	}

	FScreenDrawVertex MakeVertex(const Weltall::FDrawPrimitive& Primitive, const Weltall::FDrawPrimitive* Before, int32 Corner, float Alpha,
		uint32 Layer, uint32 VramTexture, const FVector4f& Display)
	{
		// A flat primitive takes its first corner's color everywhere.
		const auto ColorOf = [Corner](const Weltall::FDrawPrimitive& Shape) { return Shape.Corners[Shape.bGouraud ? Corner : 0].Color; };
		const Weltall::FDrawVertex& Now = Primitive.Corners[Corner];
		const Weltall::FDrawVertex& Then = Before != nullptr ? Before->Corners[Corner] : Now;
		const Weltall::FDrawColor Color = ColorOf(Primitive);
		const Weltall::FDrawColor ThenColor = Before != nullptr ? ColorOf(*Before) : Color;
		FScreenDrawVertex Vertex;
		Vertex.Position = FVector2f(Lerp(Then.X, Now.X, Alpha), Lerp(Then.Y, Now.Y, Alpha));
		if (Primitive.bSpansDisplay)
		{
			Vertex.Position = FVector2f((Corner & 1) != 0 ? Display.Z : Display.X, (Corner & 2) != 0 ? Display.W : Display.Y);
		}
		Vertex.TexCoord = FVector2f(Lerp(Then.U, Now.U, Alpha), Lerp(Then.V, Now.V, Alpha));
		Vertex.Color = FVector4f(Lerp(ThenColor.R, Color.R, Alpha), Lerp(ThenColor.G, Color.G, Alpha), Lerp(ThenColor.B, Color.B, Alpha), 255.0f);
		const Weltall::FScreenRect& Clip = Primitive.Clip;
		Vertex.Clip = FVector4f(Clip.X, Clip.Y, Clip.X + Clip.Width, Clip.Y + Clip.Height);
		Vertex.Texture = Layer;
		Vertex.Flags = Primitive.bModulate ? FlagModulate : 0;
		Vertex.VramTexture = VramTexture;
		Vertex.TexelBounds = TexelBoundsOf(Primitive);
		return Vertex;
	}

	void AddRun(FDrawRuns& Runs, uint32 FirstVertex, uint32 VertexCount, EDrawPass Pass, Weltall::EBlendMode Mode, bool bJoinable)
	{
		if (bJoinable && !Runs.IsEmpty())
		{
			FDrawRun& Last = Runs.Last();
			if (Last.bJoinable && Last.Pass == Pass && Last.Mode == Mode && Last.FirstVertex + Last.VertexCount == FirstVertex)
			{
				Last.VertexCount += VertexCount;
				return;
			}
		}
		Runs.Add({FirstVertex, VertexCount, Pass, Mode, bJoinable});
	}

	/** The GPU's blend modes as blend states; the pixel shader scales the new pixel for B + F / 4 and outputs alpha 0.5 for averaging. */
	FRHIBlendState* BlendStateOf(const FDrawRun& Run)
	{
		if (Run.Pass == EDrawPass::Opaque || Run.Pass == EDrawPass::PlainTexels)
		{
			return TStaticBlendState<CW_RGB>::GetRHI();
		}
		switch (Run.Mode)
		{
		case Weltall::EBlendMode::Average:
			return TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
		case Weltall::EBlendMode::Subtract:
			return TStaticBlendState<CW_RGB, BO_ReverseSubtract, BF_One, BF_One>::GetRHI();
		default:
			return TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
		}
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FScreenDrawPassParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScreenVertex>, Vertices)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	void AddPasses(FRDGBuilder& GraphBuilder, const FTextureSource& Textures, const FDrawListFrame& Frame, FRDGTextureRef Output, const FIntRect& ScreenRect,
		Weltall::FScreenSize Screen)
	{
		if (Frame.Latest == nullptr || Frame.Latest->Primitives.empty())
		{
			return;
		}
		const std::vector<Weltall::FDrawPrimitive>& Primitives = Frame.Latest->Primitives;
		const std::size_t First = FMath::Min(Frame.First, Primitives.size());
		const std::size_t End = FMath::Min(Frame.End, Primitives.size());
		uint32 Capacity = 0;
		for (std::size_t Index = First; Index < End; ++Index)
		{
			Capacity += Primitives[Index].CornerCount == 4 ? 6 : 3;
		}
		if (Capacity == 0)
		{
			return;
		}
		FScreenDrawVertex* Vertices = GraphBuilder.AllocPODArray<FScreenDrawVertex>(Capacity);
		// The whole target on the screen's scale: where a primitive spanning the display reaches.
		const FIntPoint Extent = Output->Desc.Extent;
		const auto ToScreen = [](int32 Pixel, int32 Min, int32 Size, int32 ScreenSize) {
			return static_cast<float>(Pixel - Min) * static_cast<float>(ScreenSize) / static_cast<float>(FMath::Max(Size, 1));
		};
		const FVector4f Display(ToScreen(0, ScreenRect.Min.X, ScreenRect.Width(), Screen.Width), ToScreen(0, ScreenRect.Min.Y, ScreenRect.Height(), Screen.Height),
			ToScreen(Extent.X, ScreenRect.Min.X, ScreenRect.Width(), Screen.Width), ToScreen(Extent.Y, ScreenRect.Min.Y, ScreenRect.Height(), Screen.Height));
		FDrawRuns& Runs = *GraphBuilder.AllocObject<FDrawRuns>();
		uint32 Written = 0;
		for (std::size_t Index = First; Index < End; ++Index)
		{
			const Weltall::FDrawPrimitive& Primitive = Primitives[Index];
			uint32 Layer = NoTexture;
			uint32 VramTexture = 0;
			if (Primitive.Texture && Textures.Vram != nullptr)
			{
				Layer = 0;
				VramTexture = FVramTexture::Pack(Primitive.Texture->Page, Primitive.Texture->Clut);
			}
			else if (Primitive.Texture)
			{
				Layer = Textures.Layers->LayerOf(*Primitive.Texture);
				if (Layer == FDrawListTextures::NoLayer)
				{
					continue;
				}
			}
			const Weltall::FDrawPrimitive* Before = nullptr;
			if (Frame.Previous != nullptr && Frame.Matches != nullptr && Index < Frame.Matches->size() && (*Frame.Matches)[Index] >= 0)
			{
				Before = &Frame.Previous->Primitives[static_cast<std::size_t>((*Frame.Matches)[Index])];
			}
			// The GPU draws a four-point polygon as the triangles (0, 1, 2) and (1, 2, 3).
			constexpr int32 Triangles[2][3] = {{0, 1, 2}, {1, 2, 3}};
			const uint32 FirstVertex = Written;
			for (int32 Triangle = 0; Triangle < (Primitive.CornerCount == 4 ? 2 : 1); ++Triangle)
			{
				for (const int32 Corner : Triangles[Triangle])
				{
					Vertices[Written++] = MakeVertex(Primitive, Before, Corner, Frame.Alpha, Layer, VramTexture, Display);
				}
			}
			const uint32 Count = Written - FirstVertex;
			if (Primitive.Blend == Weltall::EDrawBlend::Opaque)
			{
				AddRun(Runs, FirstVertex, Count, EDrawPass::Opaque, Weltall::EBlendMode::Average, true);
			}
			else if (Primitive.Texture && Primitive.Blend == Weltall::EDrawBlend::SemiTransparentAll)
			{
				AddRun(Runs, FirstVertex, Count, EDrawPass::BlendAll, Primitive.Texture->Page.Blend, true);
			}
			else if (Primitive.Texture)
			{
				AddRun(Runs, FirstVertex, Count, EDrawPass::PlainTexels, Primitive.Texture->Page.Blend, false);
				AddRun(Runs, FirstVertex, Count, EDrawPass::SemiTransparentTexels, Primitive.Texture->Page.Blend, false);
			}
			else
			{
				AddRun(Runs, FirstVertex, Count, EDrawPass::BlendAll, Primitive.UntexturedBlend, true);
			}
		}
		if (Written == 0)
		{
			return;
		}

		FRDGBufferRef VertexBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("ScreenDrawVertices"), sizeof(FScreenDrawVertex), Written, Vertices,
			Written * sizeof(FScreenDrawVertex), ERDGInitialDataFlags::NoCopy);
		FScreenDrawPassParameters* PassParameters = GraphBuilder.AllocParameters<FScreenDrawPassParameters>();
		PassParameters->Vertices = GraphBuilder.CreateSRV(VertexBuffer);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenDrawListVS> VertexShader(ShaderMap);
		FScreenDrawListPS::FPermutationDomain Permutation;
		Permutation.Set<FScreenDrawListPS::FRawVram>(Textures.Vram != nullptr);
		TShaderMapRef<FScreenDrawListPS> PixelShader(ShaderMap, Permutation);
		const FIntPoint TargetSize = Output->Desc.Extent;
		const FVector4f Placement(ScreenRect.Min.X, ScreenRect.Min.Y, ScreenRect.Width(), ScreenRect.Height());
		// As on a television, nothing shows above or below the screen's rows; a fill wider than the screen still reaches across.
		const FIntRect Rows(0, FMath::Clamp(ScreenRect.Min.Y, 0, TargetSize.Y), TargetSize.X, FMath::Clamp(ScreenRect.Max.Y, 0, TargetSize.Y));
		const FVector2f ScreenSize(Screen.Width, Screen.Height);
		FRHITexture* TextureArray = Textures.Layers != nullptr ? Textures.Layers->TextureArray() : nullptr;
		FRHITexture* VramUnits = Textures.Vram != nullptr ? Textures.Vram->Texture() : nullptr;
		const FTextureReplacementResources Replacements = Textures.Vram != nullptr ? Textures.Vram->Replacements() : FTextureReplacementResources{};

		GraphBuilder.AddPass(RDG_EVENT_NAME("ScreenDrawList"), PassParameters, ERDGPassFlags::Raster,
			[PassParameters, &Runs, VertexShader, PixelShader, TargetSize, Rows, Placement, ScreenSize, TextureArray, VramUnits, Replacements](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, static_cast<float>(TargetSize.X), static_cast<float>(TargetSize.Y), 1.0f);
				RHICmdList.SetScissorRect(true, Rows.Min.X, Rows.Min.Y, Rows.Max.X, Rows.Max.Y);
				FScreenDrawListVS::FParameters VertexParameters;
				VertexParameters.Vertices = PassParameters->Vertices;
				VertexParameters.ScreenRect = Placement;
				VertexParameters.ScreenSize = ScreenSize;
				VertexParameters.TargetSize = FVector2f(TargetSize.X, TargetSize.Y);
				for (const FDrawRun& Run : Runs)
				{
					FGraphicsPipelineStateInitializer State;
					RHICmdList.ApplyCachedRenderTargets(State);
					State.BlendState = BlendStateOf(Run);
					State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					State.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					State.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					State.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, State, 0);
					VertexParameters.FirstVertex = Run.FirstVertex;
					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
					FScreenDrawListPS::FParameters PixelParameters;
					PixelParameters.Textures = TextureArray;
					PixelParameters.Vram = VramUnits;
					PixelParameters.Replacements = TextureReplacementParameters(Replacements);
					PixelParameters.ScreenRect = Placement;
					PixelParameters.ScreenSize = ScreenSize;
					PixelParameters.DrawPass = static_cast<uint32>(Run.Pass);
					const bool bBlends = Run.Pass == EDrawPass::SemiTransparentTexels || Run.Pass == EDrawPass::BlendAll;
					PixelParameters.ColorScale = bBlends && Run.Mode == Weltall::EBlendMode::AddQuarter ? 0.25f : 1.0f;
					PixelParameters.OutputAlpha = bBlends && Run.Mode == Weltall::EBlendMode::Average ? 0.5f : 1.0f;
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);
					RHICmdList.DrawPrimitive(0, Run.VertexCount / 3, 1);
				}
			});
	}
}

void AddDrawListPasses(FRDGBuilder& GraphBuilder, const FDrawListTextures& Textures, const FDrawListFrame& Frame, FRDGTextureRef Output,
	const FIntRect& ScreenRect, Weltall::FScreenSize Screen)
{
	AddPasses(GraphBuilder, {&Textures, nullptr}, Frame, Output, ScreenRect, Screen);
}

void AddDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FDrawListFrame& Frame, FRDGTextureRef Output, const FIntRect& ScreenRect,
	Weltall::FScreenSize Screen)
{
	AddPasses(GraphBuilder, {nullptr, &Vram}, Frame, Output, ScreenRect, Screen);
}

void AddIconDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FVramTexture& IconVram, const Weltall::FDrawList& List,
	FRDGTextureRef Output, const FIntRect& ScreenRect, Weltall::FScreenSize Screen)
{
	AddIconDrawListPasses(GraphBuilder, Vram, IconVram, FDrawListFrame{&List, nullptr, nullptr, 1.0f}, Output, ScreenRect, Screen);
}

void AddIconDrawListPasses(FRDGBuilder& GraphBuilder, const FVramTexture& Vram, const FVramTexture& IconVram, const FDrawListFrame& Frame,
	FRDGTextureRef Output, const FIntRect& ScreenRect, Weltall::FScreenSize Screen)
{
	const Weltall::FDrawList& List = *Frame.Latest;
	const auto VramOf = [](const Weltall::FDrawPrimitive& Primitive) { return Primitive.Texture ? Primitive.Texture->Vram : uint16{0}; };
	const std::size_t Count = std::min(Frame.End, List.Primitives.size());
	for (std::size_t First = Frame.First; First < Count;)
	{
		const uint16 Source = VramOf(List.Primitives[First]);
		std::size_t End = First + 1;
		while (End < Count && VramOf(List.Primitives[End]) == Source)
		{
			++End;
		}
		FDrawListFrame Stretch = Frame;
		Stretch.First = First;
		Stretch.End = End;
		AddDrawListPasses(GraphBuilder, Source == Weltall::FMenuButtonIcons::PicturesVram ? IconVram : Vram, Stretch, Output, ScreenRect, Screen);
		First = End;
	}
}
