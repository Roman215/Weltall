#include "Rendering/FieldSceneRendering.h"

#include "FieldDrawBuilder.h"
#include "Field/FieldTextureRegions.h"
#include "Graphics/Texture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** A four-point polygon as two triangles, both wound as the first. */
	constexpr int32 QuadTriangles[2][3] = {{0, 1, 2}, {1, 3, 2}};

	uint32 PackTexel(uint32 U, uint32 V)
	{
		return U | V << 8;
	}

	uint32 PackColor(const Weltall::FMdlColor& Color)
	{
		return Color.R | Color.G << 8 | Color.B << 16;
	}

	FRDGBufferRef Register(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& Pooled, const TCHAR* Name, uint32 Stride, uint32 Count, const void* Data)
	{
		if (Pooled.IsValid())
		{
			return GraphBuilder.RegisterExternalBuffer(Pooled);
		}
		FRDGBufferRef Buffer = CreateStructuredBuffer(GraphBuilder, Name, Stride, FMath::Max(Count, 1u), Data, Stride * Count);
		Pooled = GraphBuilder.ConvertToExternalBuffer(Buffer);
		return Buffer;
	}
}

FFieldSceneMesh::FFieldSceneMesh(const Weltall::FFieldScene& Scene)
{
	AddScenery(Scene);
	PolygonCorners.Add(static_cast<uint32>(CornerData.Num()));
	Regions = Weltall::FieldSceneryTextureRegions(Scene);
}

FFieldSceneMesh::FFieldSceneMesh(const Weltall::FFieldScene& Scene, std::span<const Weltall::FFieldGearLayoutItem> Gears, uint32 GearLayout,
	std::span<const Weltall::FTextureRegion> SpriteSheets)
	: GearRevision(GearLayout)
{
	AddScenery(Scene);
	AddGears(Gears);
	PolygonCorners.Add(static_cast<uint32>(CornerData.Num()));
	Regions = Weltall::FieldSceneryTextureRegions(Scene);
	Regions.insert(Regions.end(), SpriteSheets.begin(), SpriteSheets.end());
	std::vector<Weltall::FTextureRegion> GearFootprints;
	for (int32 Index = static_cast<int32>(SceneryPolygons); Index < PolygonData.Num(); ++Index)
	{
		const FFieldScenePolygonState& State = PolygonData[Index];
		if (State.Texture == FVramTexture::NoTexture || (State.Flags & FieldPolygonFlags::Window) != 0) continue;
		GearFootprints.push_back(Weltall::TextureRegionBetween(State.Texture, static_cast<uint8>(State.TexelMin), static_cast<uint8>(State.TexelMin >> 8),
			static_cast<uint8>(State.TexelMax), static_cast<uint8>(State.TexelMax >> 8)));
	}
	const std::vector<Weltall::FTextureRegion> Gear = Weltall::MergeTextureRegions(std::move(GearFootprints));
	Regions.insert(Regions.end(), Gear.begin(), Gear.end());
}

void FFieldSceneMesh::AddScenery(const Weltall::FFieldScene& Scene)
{
	SceneryPolygons = static_cast<uint32>(Scene.Polygons.size());
	GearObjectRow = static_cast<uint32>(Scene.Objects.size()) + 1;
	PolygonData.SetNum(static_cast<int32>(Scene.Polygons.size()));
	PolygonCorners.Reserve(static_cast<int32>(Scene.Polygons.size()) + 1);
	for (uint32 Index = 0; Index < Scene.Polygons.size(); ++Index)
	{
		const Weltall::FFieldScenePolygon& Polygon = Scene.Polygons[Index];
		const Weltall::FMdlPrimitive& Primitive = Scene.PrimitiveOf(Polygon);
		const Weltall::FMdlPart& Part = Scene.PartOf(Scene.Objects[Polygon.Object]);
		const Weltall::FMdlPrimitiveTraits& Traits = Primitive.Traits();
		PolygonCorners.Add(static_cast<uint32>(CornerData.Num()));
		bool bJoined = false;
		for (int32 Triangle = 0; Triangle < (Traits.Corners == 4 ? 2 : 1); ++Triangle)
		{
			for (const int32 Corner : QuadTriangles[Triangle])
			{
				const uint16 Number = Primitive.Vertices[static_cast<std::size_t>(Corner)];
				const Weltall::FGteVector& Vertex = Part.Vertices[Number];
				const Weltall::FMdlUv& Uv = Primitive.Uvs[static_cast<std::size_t>(Corner)];
				FFieldSceneCorner& Out = CornerData.AddDefaulted_GetRef();
				Out.Position = FVector3f(Vertex[0], Vertex[1], Vertex[2]);
				Out.TexCoord = FVector2f(Uv.U, Uv.V);
				Out.Polygon = Index;
				Out.Object = Polygon.Object;
				Out.Corner = Traits.bGouraud ? static_cast<uint32>(Corner) : 0;
				if (const Weltall::FFieldSceneJoin* Join = Scene.JoinOf(Polygon.Object, Number))
				{
					Out.Position = FVector3f(Join->Point[0], Join->Point[1], Join->Point[2]);
					Out.Object = Join->Anchor;
					bJoined = true;
				}
			}
		}
		if (bJoined)
		{
			JoinedPolygonList.Add(Index);
		}
		FFieldScenePolygonState& State = PolygonData[static_cast<int32>(Index)];
		State.Texture = FVramTexture::NoTexture;
		if (Traits.bTextured)
		{
			State.Texture = FVramTexture::Pack(Polygon.Page, Polygon.Clut);
			State.Flags = Primitive.IsRawTexture() ? FieldPolygonFlags::Raw : 0;
			uint32 LowU = 255, LowV = 255, HighU = 0, HighV = 0;
			for (int32 Corner = 0; Corner < Traits.Corners; ++Corner)
			{
				const Weltall::FMdlUv& Uv = Primitive.Uvs[static_cast<std::size_t>(Corner)];
				LowU = FMath::Min<uint32>(LowU, Uv.U);
				LowV = FMath::Min<uint32>(LowV, Uv.V);
				HighU = FMath::Max<uint32>(HighU, Uv.U);
				HighV = FMath::Max<uint32>(HighV, Uv.V);
			}
			State.TexelMin = PackTexel(LowU, LowV);
			State.TexelMax = PackTexel(HighU, HighV);
		}
	}
}

FFieldSceneMesh::~FFieldSceneMesh() = default;

FRDGBufferRef FFieldSceneMesh::CornerBuffer(FRDGBuilder& GraphBuilder) const
{
	return Register(GraphBuilder, PooledCorners, TEXT("FieldSceneCorners"), sizeof(FFieldSceneCorner), static_cast<uint32>(CornerData.Num()), CornerData.GetData());
}

FRDGBufferRef FFieldSceneMesh::PolygonBuffer(FRDGBuilder& GraphBuilder) const
{
	return Register(GraphBuilder, PooledPolygons, TEXT("FieldScenePolygons"), sizeof(FFieldScenePolygonState), static_cast<uint32>(PolygonData.Num()),
		PolygonData.GetData());
}

void BuildFieldSceneDraw(const Weltall::FFieldScene& Scene, const Weltall::FFieldSceneFrame& Frame, const Weltall::FFieldView& View,
	const FFieldSceneMesh& Mesh, const FFieldRenderOptions& Options, FFieldSceneDraw& Draw)
{
	Draw.Objects.Reset();
	Draw.FirstSpriteRow = MAX_int32;
	Draw.PictureStretch.bStaged = false;
	for (const Weltall::FFieldObjectView& Object : Frame.Objects)
	{
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			const auto& Entries = Object.Exact.M[Row];
			Draw.Objects.Emplace(Entries[0], Entries[1], Entries[2], Object.Exact.T[Row]);
		}
	}
	Draw.Colors.Reset();
	for (const Weltall::FFieldPolygonView& Polygon : Frame.Polygons)
	{
		Draw.Colors.Emplace(PackColor(Polygon.Colors[0]), PackColor(Polygon.Colors[1]), PackColor(Polygon.Colors[2]), PackColor(Polygon.Colors[3]));
	}
	Draw.DynamicCorners.Reset();
	Draw.DynamicPolygons.Reset();
	Draw.SpriteCells.Reset();
	Draw.Order.Reset();
	Draw.Runs.Reset();
	FFieldDrawBuilder Builder(Scene, Frame, {}, {}, {}, Mesh, Draw);
	if (Options.Sorting != EFieldSorting::PaintersOrder)
	{
		Builder.DepthBuffer(Options.Sorting == EFieldSorting::DepthBufferWithScenery);
	}
	else
	{
		Builder.PaintersOrder();
	}
	const Weltall::FMdlColor& Background = Scene.State.BackgroundColor;
	Draw.Background = FColor(Weltall::FilledChannel(Background.R), Weltall::FilledChannel(Background.G), Weltall::FilledChannel(Background.B)).ReinterpretAsLinear();
	Draw.ProjectionDistance = View.ProjectionDistance;
	Draw.Filter = Options.Filter;
	Draw.SpriteFilter = Options.SpriteFilter;
	Draw.AntiAliasing = Options.AntiAliasing;
	Draw.bDepthBuffer = Options.Sorting != EFieldSorting::PaintersOrder;
}
