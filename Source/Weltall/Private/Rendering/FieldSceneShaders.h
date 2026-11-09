#pragma once

#include "GlobalShader.h"
#include "Rendering/FieldSceneRendering.h"
#include "RenderGraphResources.h"
#include "ShaderParameterStruct.h"
#include "ShaderPermutation.h"
#include "TextureReplacementParameters.h"

class FFieldSceneVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldSceneVS);
	SHADER_USE_PARAMETER_STRUCT(FFieldSceneVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldCorner>, Corners)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldCorner>, DynamicCorners)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, Order)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, Objects)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Colors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, Polygons)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, DynamicPolygons)
		SHADER_PARAMETER(uint32, StaticCorners)
		SHADER_PARAMETER(uint32, StaticPolygons)
		SHADER_PARAMETER(uint32, FirstVertex)
		SHADER_PARAMETER(FVector2f, ProjectionScale)
		SHADER_PARAMETER(float, NearPlane)
		SHADER_PARAMETER(float, EntryMargin)
		SHADER_PARAMETER(uint32, SlopeMargin)
		SHADER_PARAMETER(FVector3f, FloorUp)
		SHADER_PARAMETER(FVector2f, ShadowHeights)
	END_SHADER_PARAMETER_STRUCT()
};

class FFieldScenePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldScenePS);
	SHADER_USE_PARAMETER_STRUCT(FFieldScenePS, FGlobalShader);

	class FPerSampleTexels : SHADER_PERMUTATION_BOOL("PER_SAMPLE_TEXELS");
	using FPermutationDomain = TShaderPermutationDomain<FPerSampleTexels>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, Polygons)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FFieldPolygon>, DynamicPolygons)
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, Vram)
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, Upscaled4)
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, Upscaled8)
		SHADER_PARAMETER_STRUCT_INCLUDE(FTextureReplacementParameters, Replacements)
		SHADER_PARAMETER(uint32, StaticPolygons)
		SHADER_PARAMETER(uint32, DrawPass)
		SHADER_PARAMETER(uint32, Filter)
		SHADER_PARAMETER(uint32, SpriteFilter)
		SHADER_PARAMETER(float, ColorScale)
		SHADER_PARAMETER(float, OutputAlpha)
	END_SHADER_PARAMETER_STRUCT()
};

class FFieldSceneResolvePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFieldSceneResolvePS);
	SHADER_USE_PARAMETER_STRUCT(FFieldSceneResolvePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Scene)
		SHADER_PARAMETER(FIntPoint, AreaOrigin)
		SHADER_PARAMETER(int32, Factor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
