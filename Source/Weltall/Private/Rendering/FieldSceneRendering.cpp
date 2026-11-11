#include "Rendering/FieldSceneRendering.h"

#include "CommonRenderResources.h"
#include "Field/FieldScrolls.h"
#include "FieldSceneShaders.h"
#include "PipelineStateCache.h"
#include "PixelShaderUtils.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Rendering/FieldPictureRendering.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** The field's screen: 320 x 224 pixels, the view axis at its center. */
	constexpr float ScreenWidth = 320.0f;
	constexpr float ScreenHeight = 224.0f;
	/** View units in front of the eye where the drawing clips; the depth is this over the view depth. */
	constexpr float NearPlane = 1.0f;
	constexpr uint8 MsaaSamples = 4;
	constexpr int32 SupersamplingFactor = 2;

	/** The GPU's blend modes as blend states; the pixel shader scales the new pixel for B + F / 4 and outputs alpha 0.5 for averaging. */
	FRHIBlendState* BlendStateOf(const FFieldDrawRun& Run)
	{
		constexpr uint8 PlainTexels = 1;
		constexpr uint8 Opaque = 0;
		if (Run.Depth == EFieldRunDepth::Mask || Run.Depth == EFieldRunDepth::MaskBesideModels || Run.Depth == EFieldRunDepth::MaskBesideMarks ||
			Run.Depth == EFieldRunDepth::PushedBack)
		{
			return TStaticBlendState<CW_NONE>::GetRHI();
		}
		if (Run.Pass == Opaque || Run.Pass == PlainTexels)
		{
			return TStaticBlendState<CW_RGB>::GetRHI();
		}
		switch (Run.Blend)
		{
		case Weltall::EBlendMode::Average:
			return TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
		case Weltall::EBlendMode::Subtract:
			return TStaticBlendState<CW_RGB, BO_ReverseSubtract, BF_One, BF_One>::GetRHI();
		default:
			return TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
		}
	}

	/**
	 * The stencil's bits: the pixels the sprites mark, those the sprite being drawn drew, and those the 3D models mark
	 * (where a shadow is tested close to its floor). Inside the marks is inside either's.
	 */
	constexpr uint8 StencilMarked = 1;
	constexpr uint8 StencilDrawn = 2;
	constexpr uint8 StencilModel = 4;
	constexpr uint8 StencilMarks = StencilMarked | StencilModel;

	/**
	 * How far a sprite stands from its entry's far edge against the scenery: eight entries. The order
	 * decides between a sprite and the scenery within this depth of each other (a sprite's feet on the
	 * floor before them, a close pillar); beyond it, the depth.
	 */
	constexpr float SpriteMargin = 128.0f;

	/**
	 * How far behind the nearest of the 3D models (the gears, the children's models) one may stand and still be drawn
	 * in the order: within it the order decides (details laid on a surface), beyond it the depth. The thinnest models
	 * seen from behind (a save point, 12 units through) must stand farther than it.
	 */
	constexpr float ModelMargin = 8.0f;

	/**
	 * How far behind the nearest surface of the scenery, across its plane, a piece may lie and still be drawn in the
	 * order when the scenery is sorted by depth: the maps lay their decals up to 3 units under the ground they show
	 * on (map 59's), so the order keeps them there; the pieces that cross farther apart are sorted by depth.
	 */
	constexpr float SceneryMargin = 4.0f;

	/**
	 * How far above a shadow's floor (view units) what was drawn before it must stand to hide it: a character's, and a
	 * gear's broad one, whose ground rises and falls more across it. A ledge or a wall between it and the eye hides it.
	 */
	constexpr float CharacterShadowHeight = 64.0f;
	constexpr float GearShadowHeight = 256.0f;

	/**
	 * Over a model's polygons a shadow is tested this close to its floor: a gear stands on the ground the shadow lies on,
	 * so all of it but the soles of its feet stands in front of the floor behind it.
	 */
	constexpr float OverModelShadowHeight = 16.0f;

	/** Draws a run's pixels outside the marks as they are, keeping their depth when asked (the shadows test it). */
	FRHIDepthStencilState* OutsideMarks(bool bWrite)
	{
		return bWrite ? TStaticDepthStencilState<true, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, StencilMarks, 0>::GetRHI()
					  : TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, StencilMarks, 0>::GetRHI();
	}

	/** Draws a run's pixels inside any marks against the depth, writing its own when asked (the reference 0). */
	FRHIDepthStencilState* InsideMarks(bool bWrite)
	{
		return bWrite ? TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, StencilMarks,
							0>::GetRHI()
					  : TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, StencilMarks,
							0>::GetRHI();
	}

	/**
	 * The margin a run's sprites and particles stand at from their entries' far edges, or its models and scenery from
	 * their own depth (the scenery's across its polygons' planes). A model's or the scenery's prepass stands a margin
	 * nearer, their drawing two: what lies within a margin behind the nearest is drawn in the order, what lies farther
	 * fails the depth.
	 */
	float EntryMarginOf(const FFieldDrawRun& Run)
	{
		switch (Run.Rule)
		{
		case EFieldDepthRule::Model:
			return Run.Depth == EFieldRunDepth::Mask ? -ModelMargin : Run.Depth == EFieldRunDepth::InsideMask || Run.Depth == EFieldRunDepth::Masked ? -2.0f * ModelMargin : 0.0f;
		case EFieldDepthRule::Scenery:
			if (Run.Depth == EFieldRunDepth::Mask || Run.Depth == EFieldRunDepth::MaskBesideModels || Run.Depth == EFieldRunDepth::MaskBesideMarks)
			{
				return -SceneryMargin;
			}
			return Run.Depth == EFieldRunDepth::Masked || Run.Depth == EFieldRunDepth::OverModels || Run.Depth == EFieldRunDepth::GroundOverModels
				? -2.0f * SceneryMargin
				: 0.0f;
		case EFieldDepthRule::Entry:
			break;
		}
		switch (Run.Depth)
		{
		case EFieldRunDepth::Mask:
		case EFieldRunDepth::InsideMask:
			return -SpriteMargin;
		case EFieldRunDepth::PushedBack:
			return SpriteMargin;
		default:
			return 0.0f;
		}
	}

	/**
	 * The depth and stencil states a run is drawn with, one draw each, the stencil value each compares with, and how far
	 * above their floors each tests its shadows (a character's, a gear's: none, at their own depth).
	 */
	struct FRunDepthStates
	{
		FRHIDepthStencilState* States[2] = {};
		uint32 References[2] = {};
		FVector2f ShadowHeights[2] = {};
		int32 Count = 1;
	};

	FRunDepthStates DepthStatesOf(const FFieldDrawRun& Run, bool bShadowsOnFloors)
	{
		FRunDepthStates Result;
		switch (Run.Depth)
		{
		case EFieldRunDepth::Mask:
			// The sprites mark their pixels with one bit, the models with the other; the scenery leaves its depth alone.
			switch (Run.Rule)
			{
			case EFieldDepthRule::Model:
				Result.States[0] = TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace, true, CF_Always, SO_Keep, SO_Keep,
					SO_Replace, 0xFF, StencilModel>::GetRHI();
				Result.References[0] = StencilModel;
				break;
			case EFieldDepthRule::Scenery:
				Result.States[0] = TStaticDepthStencilState<true, CF_GreaterEqual>::GetRHI();
				break;
			case EFieldDepthRule::Entry:
				Result.States[0] = TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace, true, CF_Always, SO_Keep, SO_Keep,
					SO_Replace, 0xFF, StencilMarked>::GetRHI();
				Result.References[0] = StencilMarked;
				break;
			}
			break;
		case EFieldRunDepth::Masked:
			if (Run.Rule == EFieldDepthRule::Scenery)
			{
				// The scenery sorted by depth everywhere, against its prepass and the marks' depth.
				Result.States[0] = TStaticDepthStencilState<false, CF_GreaterEqual>::GetRHI();
				break;
			}
			Result.States[0] = OutsideMarks(Run.bDepthWrite);
			Result.States[1] = InsideMarks(Run.bDepthWrite);
			Result.Count = 2;
			break;
		case EFieldRunDepth::MaskBesideModels:
			Result.States[0] = TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				StencilModel, 0>::GetRHI();
			break;
		case EFieldRunDepth::MaskBesideMarks:
			Result.States[0] = TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				StencilMarks, 0>::GetRHI();
			break;
		case EFieldRunDepth::GroundOverModels:
			if (Run.Rule == EFieldDepthRule::Scenery)
			{
				// Beside every mark against its prepass and the marks' depth; over the models' (not the sprites') as the order draws it.
				Result.States[0] = TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep,
					SO_Keep, StencilMarks, 0>::GetRHI();
				Result.States[1] = TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					StencilMarks, 0>::GetRHI();
				Result.References[1] = StencilModel;
				Result.Count = 2;
				break;
			}
			// Outside the sprites' marks as the order draws it, over the models' too.
			Result.States[0] = TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				StencilMarked, 0>::GetRHI();
			break;
		case EFieldRunDepth::OverModels:
			if (Run.Rule == EFieldDepthRule::Scenery)
			{
				// Beside the models' marks against its prepass and the marks' depth; over them as the order draws it.
				Result.States[0] = TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep,
					SO_Keep, StencilModel, 0>::GetRHI();
				Result.States[1] = TStaticDepthStencilState<false, CF_Always, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep,
					SO_Keep, StencilModel, 0>::GetRHI();
				Result.Count = 2;
				break;
			}
			// Outside the sprites' marks as the order draws it, over the models' too; inside them against the depth.
			Result.States[0] = TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				StencilMarked, 0>::GetRHI();
			Result.States[1] = Run.bDepthWrite
				? TStaticDepthStencilState<true, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
					  StencilMarked, 0>::GetRHI()
				: TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
					  StencilMarked, 0>::GetRHI();
			Result.Count = 2;
			break;
		case EFieldRunDepth::InsideMask:
			// Inside the marks: the reference holds no mark's bit, so it is unequal there. A sprite notes where it drew.
			Result.States[0] = Run.Rule == EFieldDepthRule::Model
				? TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Keep,
					  StencilMarks, 0>::GetRHI()
				: TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Replace, true, CF_NotEqual, SO_Keep, SO_Keep, SO_Replace,
					  StencilMarks, StencilDrawn>::GetRHI();
			Result.References[0] = Run.Rule == EFieldDepthRule::Model ? 0 : StencilDrawn;
			break;
		case EFieldRunDepth::PushedBack:
			// Where it drew, its depth pushed back, the note cleared.
			Result.States[0] = TStaticDepthStencilState<true, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Zero, true, CF_Equal, SO_Keep, SO_Keep, SO_Zero,
				StencilDrawn, StencilDrawn>::GetRHI();
			Result.References[0] = StencilDrawn;
			break;
		case EFieldRunDepth::OutsideMask:
			if (!bShadowsOnFloors)
			{
				Result.States[0] = OutsideMarks(false);
				break;
			}
			// Outside the marks against what stands well above the floor; over a model's pixels (not a sprite's), close to it.
			Result.States[0] = TStaticDepthStencilState<false, CF_GreaterEqual, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				StencilMarks, 0>::GetRHI();
			Result.ShadowHeights[0] = FVector2f(CharacterShadowHeight, GearShadowHeight);
			Result.States[1] = Result.States[0];
			Result.References[1] = StencilModel;
			Result.ShadowHeights[1] = FVector2f(OverModelShadowHeight, OverModelShadowHeight);
			Result.Count = 2;
			break;
		default:
			Result.States[0] = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			break;
		}
		return Result;
	}

	bool IsBlended(const FFieldDrawRun& Run)
	{
		return Run.Pass >= 2;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FFieldScenePassParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldCorner>, Corners)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldCorner>, DynamicCorners)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, Order)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Objects)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Colors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, Polygons)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, DynamicPolygons)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, Upscaled4)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, Upscaled8)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	template <typename TElement>
	FRDGBufferSRVRef Upload(FRDGBuilder& GraphBuilder, const TCHAR* Name, const TArray<TElement>& Data)
	{
		static const TElement Empty{};
		const uint32 Count = static_cast<uint32>(Data.Num());
		return GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, Name, sizeof(TElement), FMath::Max(Count, 1u), Count > 0 ? Data.GetData() : &Empty,
			sizeof(TElement) * FMath::Max(Count, 1u)));
	}
}

namespace
{
	/** The buffers every pass of a frame's scene reads. */
	struct FSceneBuffers
	{
		FRDGBufferSRVRef Corners = nullptr;
		FRDGBufferSRVRef Polygons = nullptr;
		FRDGBufferSRVRef DynamicCorners = nullptr;
		FRDGBufferSRVRef DynamicPolygons = nullptr;
		FRDGBufferSRVRef Order = nullptr;
		FRDGBufferSRVRef Objects = nullptr;
		FRDGBufferSRVRef Colors = nullptr;
	};

	/** The frame's buffers for a target of Size: a scene staged before a picture stretched across its width (StretchFieldPictureScene). */
	FSceneBuffers UploadSceneBuffers(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FFieldSceneDraw& Draw, FIntPoint Size)
	{
		FSceneBuffers Buffers;
		Buffers.Corners = GraphBuilder.CreateSRV(Mesh.CornerBuffer(GraphBuilder));
		Buffers.Polygons = GraphBuilder.CreateSRV(Mesh.PolygonBuffer(GraphBuilder));
		TArray<FVector4f> StretchedObjects;
		TArray<FFieldSceneCorner> StretchedCorners;
		const bool bStretched = StretchFieldPictureScene(Draw, FieldPictureStretchFactor(Size), StretchedObjects, StretchedCorners);
		Buffers.DynamicCorners = Upload(GraphBuilder, TEXT("FieldSceneDynamicCorners"), bStretched ? StretchedCorners : Draw.DynamicCorners);
		Buffers.DynamicPolygons = Upload(GraphBuilder, TEXT("FieldSceneDynamicPolygons"), Draw.DynamicPolygons);
		Buffers.Order = Upload(GraphBuilder, TEXT("FieldSceneOrder"), Draw.Order);
		Buffers.Objects = Upload(GraphBuilder, TEXT("FieldSceneObjects"), bStretched ? StretchedObjects : Draw.Objects);
		Buffers.Colors = Upload(GraphBuilder, TEXT("FieldSceneColors"), Draw.Colors);
		return Buffers;
	}

	/**
	 * Draws Runs [First, End) of a frame's scene into Viewport of Color (resolved into Resolved when multisampled), cleared
	 * first or over what it holds.
	 */
	void AddSceneRuns(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FFieldSceneDraw& Draw, const FSceneBuffers& Buffers,
		FRDGTextureRef Color, FRDGTextureRef Resolved, const FIntRect& Viewport, bool bClear, int32 First, int32 End, const FIntRect* Scissor)
	{
		const FIntPoint Size = Viewport.Size();
		const uint8 Samples = static_cast<uint8>(Color->Desc.NumSamples);
		FFieldScenePassParameters* PassParameters = GraphBuilder.AllocParameters<FFieldScenePassParameters>();
		PassParameters->Corners = Buffers.Corners;
		PassParameters->Polygons = Buffers.Polygons;
		PassParameters->DynamicCorners = Buffers.DynamicCorners;
		PassParameters->DynamicPolygons = Buffers.DynamicPolygons;
		PassParameters->Order = Buffers.Order;
		PassParameters->Objects = Buffers.Objects;
		PassParameters->Colors = Buffers.Colors;
		const ERenderTargetLoadAction Load = bClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Color, Samples > 1 ? Resolved : nullptr, Load);
		if (Draw.bDepthBuffer)
		{
			FRDGTextureRef Depth = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Color->Desc.Extent, PF_DepthStencil, FClearValueBinding::DepthZero,
				TexCreate_DepthStencilTargetable, 1, Samples), TEXT("FieldSceneDepth"));
			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(Depth, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear,
				FExclusiveDepthStencil::DepthWrite_StencilWrite);
		}

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FFieldSceneVS> VertexShader(ShaderMap);
		FFieldScenePS::FPermutationDomain Permutation;
		Permutation.Set<FFieldScenePS::FPerSampleTexels>(Samples == MsaaSamples);
		TShaderMapRef<FFieldScenePS> PixelShader(ShaderMap, Permutation);
		// H times the clip units of one pixel of the original screen: its height fills the target's.
		const float Vertical = Draw.ProjectionDistance * 2.0f / ScreenHeight;
		const FVector2f ProjectionScale(Vertical * static_cast<float>(Size.Y) / static_cast<float>(Size.X), Vertical);
		FRHITexture* VramUnits = Vram.Texture();
		const FTextureReplacementResources Replacements = Vram.Replacements();
		TArray<FFieldDrawRun, SceneRenderingAllocator>* Runs = GraphBuilder.AllocObject<TArray<FFieldDrawRun, SceneRenderingAllocator>>();
		Runs->Append(Draw.Runs.GetData() + First, End - First);
		uint32 Filter = static_cast<uint32>(Draw.Filter);
		uint32 SpriteFilter = static_cast<uint32>(Draw.SpriteFilter);
		if (Draw.Filter == EFieldTextureFilter::Upscaled || Draw.SpriteFilter == EFieldTextureFilter::Upscaled)
		{
			const FVramTexture::FUpscaled Upscaled = Vram.Upscaled(GraphBuilder);
			PassParameters->Upscaled4 = Upscaled.Indexed4;
			PassParameters->Upscaled8 = Upscaled.Indexed8;
			if (Upscaled.Indexed4 == nullptr)
			{
				// A VRAM not set to upscale (FVramTexture::SetUpscaled) is drawn crisp.
				constexpr uint32 Crisp = static_cast<uint32>(EFieldTextureFilter::Crisp);
				Filter = Draw.Filter == EFieldTextureFilter::Upscaled ? Crisp : Filter;
				SpriteFilter = Draw.SpriteFilter == EFieldTextureFilter::Upscaled ? Crisp : SpriteFilter;
			}
		}
		const uint32 StaticCorners = Mesh.Corners();
		const uint32 StaticPolygons = Mesh.Polygons();
		const bool bScissor = Scissor != nullptr;
		const FIntRect ScissorRect = bScissor ? *Scissor : FIntRect();
		const bool bDepthTarget = Draw.bDepthBuffer;
		const FVector3f FloorUp = Draw.FloorUp;
		const bool bShadowsOnFloors = !FloorUp.IsZero();

		GraphBuilder.AddPass(RDG_EVENT_NAME("FieldScene"), PassParameters, ERDGPassFlags::Raster,
			[PassParameters, Runs, VertexShader, PixelShader, Viewport, ProjectionScale, VramUnits, Filter, SpriteFilter, StaticCorners, StaticPolygons, bScissor,
				ScissorRect, bDepthTarget, FloorUp, bShadowsOnFloors, Replacements](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(static_cast<float>(Viewport.Min.X), static_cast<float>(Viewport.Min.Y), 0.0f, static_cast<float>(Viewport.Max.X),
					static_cast<float>(Viewport.Max.Y), 1.0f);
				if (bScissor)
				{
					RHICmdList.SetScissorRect(true, ScissorRect.Min.X, ScissorRect.Min.Y, ScissorRect.Max.X, ScissorRect.Max.Y);
				}
				FFieldSceneVS::FParameters VertexParameters;
				VertexParameters.Corners = PassParameters->Corners;
				VertexParameters.DynamicCorners = PassParameters->DynamicCorners;
				VertexParameters.Order = PassParameters->Order;
				VertexParameters.Objects = PassParameters->Objects;
				VertexParameters.Colors = PassParameters->Colors;
				VertexParameters.Polygons = PassParameters->Polygons;
				VertexParameters.DynamicPolygons = PassParameters->DynamicPolygons;
				VertexParameters.StaticCorners = StaticCorners;
				VertexParameters.StaticPolygons = StaticPolygons;
				VertexParameters.ProjectionScale = ProjectionScale;
				VertexParameters.NearPlane = NearPlane;
				VertexParameters.FloorUp = FloorUp;
				for (const FFieldDrawRun& Run : *Runs)
				{
					FRunDepthStates Depth;
					if (bDepthTarget)
					{
						Depth = DepthStatesOf(Run, bShadowsOnFloors);
					}
					else
					{
						Depth.States[0] = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					}
					for (int32 Draw = 0; Draw < Depth.Count; ++Draw)
					{
						FGraphicsPipelineStateInitializer State;
						RHICmdList.ApplyCachedRenderTargets(State);
						State.BlendState = BlendStateOf(Run);
						State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
						State.DepthStencilState = Depth.States[Draw];
						State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
						State.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						State.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
						State.PrimitiveType = PT_TriangleList;
						SetGraphicsPipelineState(RHICmdList, State, Depth.References[Draw]);
						VertexParameters.FirstVertex = Run.FirstVertex;
						VertexParameters.EntryMargin = EntryMarginOf(Run);
						VertexParameters.SlopeMargin = Run.Rule == EFieldDepthRule::Scenery ? 1u : 0u;
						VertexParameters.ShadowHeights = Depth.ShadowHeights[Draw];
						SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);
						FFieldScenePS::FParameters PixelParameters;
						PixelParameters.Polygons = PassParameters->Polygons;
						PixelParameters.DynamicPolygons = PassParameters->DynamicPolygons;
						PixelParameters.Vram = VramUnits;
						PixelParameters.Replacements = TextureReplacementParameters(Replacements);
						PixelParameters.Upscaled4 = PassParameters->Upscaled4 != nullptr ? PassParameters->Upscaled4->GetRHI() : VramUnits;
						PixelParameters.Upscaled8 = PassParameters->Upscaled8 != nullptr ? PassParameters->Upscaled8->GetRHI() : VramUnits;
						PixelParameters.StaticPolygons = StaticPolygons;
						PixelParameters.DrawPass = Run.Pass;
						PixelParameters.Filter = Filter;
						PixelParameters.SpriteFilter = SpriteFilter;
						PixelParameters.ColorScale = IsBlended(Run) && Run.Blend == Weltall::EBlendMode::AddQuarter ? 0.25f : 1.0f;
						PixelParameters.OutputAlpha = IsBlended(Run) && Run.Blend == Weltall::EBlendMode::Average ? 0.5f : 1.0f;
						SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);
						RHICmdList.DrawPrimitive(0, Run.VertexCount / 3, 1);
					}
				}
			});
	}

	FRDGTextureRef CreateSceneColor(FRDGBuilder& GraphBuilder, const FFieldSceneDraw& Draw, FIntPoint Size, uint8 Samples, const TCHAR* Name)
	{
		return GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding(Draw.Background),
			TexCreate_RenderTargetable | TexCreate_ShaderResource, 1, Samples), Name);
	}

	FRDGTextureRef CreateResolved(FRDGBuilder& GraphBuilder, const FFieldSceneDraw& Draw, FIntPoint Size, const TCHAR* Name)
	{
		return GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding(Draw.Background),
			TexCreate_ResolveTargetable | TexCreate_ShaderResource), Name);
	}
}

FVector4f FieldScreenPlacement(FIntPoint Size)
{
	// The 320 x 224 screen fills the target's height and lies at the centre of its width.
	const float Scale = static_cast<float>(Size.Y) / ScreenHeight;
	return FVector4f(Scale, Scale, static_cast<float>(Size.X) * 0.5f - ScreenWidth * 0.5f * Scale, 0.0f);
}

uint8 FieldSceneSampleCount(EFieldAntiAliasing AntiAliasing, FStringView AndroidAdapterName)
{
	// Android Vulkan reports the adapter name with this suffix and does not set GRHIDeviceId.
	const bool bAdreno610 = AndroidAdapterName.Equals(TEXT("Adreno (TM) 610 Vulkan"), ESearchCase::IgnoreCase);
	return AntiAliasing == EFieldAntiAliasing::Msaa && !bAdreno610 ? MsaaSamples : 1;
}

void AddFieldScenePasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FFieldSceneDraw& Draw,
	FRDGTextureRef Output, const FIntRect& Area)
{
	if (Area.Width() <= 0 || Area.Height() <= 0)
	{
		return;
	}
	const int32 Factor = Draw.AntiAliasing == EFieldAntiAliasing::Supersampling ? SupersamplingFactor : 1;
	const uint8 Samples = FieldSceneSampleCount(Draw.AntiAliasing, PLATFORM_ANDROID ? FStringView(GRHIAdapterName) : FStringView());
	const FIntPoint Size = Area.Size() * Factor;
	const FSceneBuffers Buffers = UploadSceneBuffers(GraphBuilder, Mesh, Draw, Size);
	const FIntRect Target(FIntPoint::ZeroValue, Size);

	// The scripts' drawing area, on the 320 x 224 screen, in the target's pixels; a scene before a picture within 21:9.
	const FVector4f Placement = FieldScreenPlacement(Size);
	const auto ToTarget = [&Placement, Size](FIntPoint Point)
	{
		return FIntPoint(FMath::Clamp(FMath::RoundToInt(Point.X * Placement.X + Placement.Z), 0, Size.X),
			FMath::Clamp(FMath::RoundToInt(Point.Y * Placement.Y + Placement.W), 0, Size.Y));
	};
	FIntRect Scissor = Draw.bClipped ? FIntRect(ToTarget(Draw.Clip.Min), ToTarget(Draw.Clip.Max)) : Target;
	if (Draw.PictureStretch.bStaged)
	{
		Scissor.Clip(FieldPictureArea(Size));
	}
	const FIntRect* Clip = Scissor != Target ? &Scissor : nullptr;

	FRDGTextureRef Color = CreateSceneColor(GraphBuilder, Draw, Size, Samples, TEXT("FieldSceneColor"));
	FRDGTextureRef Resolved = Samples > 1 ? CreateResolved(GraphBuilder, Draw, Size, TEXT("FieldSceneResolved")) : Color;
	if (!Draw.bWave)
	{
		AddSceneRuns(GraphBuilder, Mesh, Vram, Draw, Buffers, Color, Resolved, Target, true, 0, Draw.Runs.Num(), Clip);
	}
	else
	{
		// The scene drawn so far, then drawn again through the wave's grid (and as it is beside it), then the nearer entries over it.
		FRDGTextureRef Waved = Samples > 1 ? CreateResolved(GraphBuilder, Draw, Size, TEXT("FieldSceneWaved"))
										   : CreateSceneColor(GraphBuilder, Draw, Size, 1, TEXT("FieldSceneWaved"));
		FRDGTextureRef BeforeWave = Samples > 1 ? CreateSceneColor(GraphBuilder, Draw, Size, Samples, TEXT("FieldSceneBeforeWave")) : Waved;
		AddSceneRuns(GraphBuilder, Mesh, Vram, Draw, Buffers, BeforeWave, Waved, Target, true, 0, Draw.WaveRuns, Clip);
		TArray<FVector4f, SceneRenderingAllocator>& Grid = *GraphBuilder.AllocObject<TArray<FVector4f, SceneRenderingAllocator>>();
		const float Left = -Placement.Z / Placement.X;
		const float Right = (static_cast<float>(Size.X) - Placement.Z) / Placement.X;
		const FVector4f Whole[4] = {FVector4f(Left, 0.0f, Left, 0.0f), FVector4f(Right, 0.0f, Right, 0.0f), FVector4f(Left, ScreenHeight, Left, ScreenHeight),
			FVector4f(Right, ScreenHeight, Right, ScreenHeight)};
		for (const int32 Corner : {0, 1, 2, 1, 3, 2})
		{
			Grid.Add(Whole[Corner]);
		}
		// A view that shows more than the screen is waved across its whole width.
		const bool bWide = Left < -0.5f / Placement.X && !Draw.WideWaveGrid.IsEmpty();
		for (const FFieldWaveCorner& Corner : bWide ? Draw.WideWaveGrid : Draw.WaveGrid)
		{
			// Across a wide view the sway eases to nothing over the last two cells before each side edge, which stays put as the screen's do.
			const float Eased = bWide ? FMath::Clamp(FMath::Min(Corner.Source.X - Left, Right - Corner.Source.X) / (2.0f * Weltall::FFieldScreenWave::CellSize), 0.0f, 1.0f) : 1.0f;
			Grid.Emplace(FMath::Lerp(Corner.Source.X, Corner.Position.X, Eased), Corner.Position.Y, Corner.Source.X, Corner.Source.Y);
		}
		FFieldPictureDraw Wave;
		Wave.Vertices = Grid;
		Wave.Placement = Placement;
		Wave.SourcePlacement = FVector4f(Placement.X / Size.X, Placement.Y / Size.Y, Placement.Z / Size.X, Placement.W / Size.Y);
		Wave.bClearFirst = true;
		Wave.bScissor = Clip != nullptr;
		Wave.Scissor = Scissor;
		AddFieldPicturePass(GraphBuilder, Waved, Color, Wave);
		AddSceneRuns(GraphBuilder, Mesh, Vram, Draw, Buffers, Color, Resolved, Target, false, Draw.WaveRuns, Draw.Runs.Num(), Clip);
	}

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	FFieldSceneResolvePS::FParameters* ResolveParameters = GraphBuilder.AllocParameters<FFieldSceneResolvePS::FParameters>();
	ResolveParameters->Scene = Resolved;
	ResolveParameters->AreaOrigin = Area.Min;
	ResolveParameters->Factor = Factor;
	ResolveParameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ELoad);
	TShaderMapRef<FFieldSceneResolvePS> ResolveShader(ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("FieldSceneResolve"), ResolveShader, ResolveParameters, Area);
}

void AddFieldSceneOverlayPasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FFieldSceneDraw& Draw,
	FRDGTextureRef Output, const FIntRect& Area)
{
	if (Area.Width() <= 0 || Area.Height() <= 0 || Draw.Runs.IsEmpty())
	{
		return;
	}
	check(!Draw.bDepthBuffer && !Draw.bWave);
	AddSceneRuns(GraphBuilder, Mesh, Vram, Draw, UploadSceneBuffers(GraphBuilder, Mesh, Draw, Area.Size()), Output, Output, Area, false, 0, Draw.Runs.Num(), nullptr);
}
