#include "FieldDrawBuilder.h"

#include "Field/FieldSpriteModels.h"
#include "Graphics/SpriteDrawing.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** A four-point polygon as two triangles, both wound as the first. */
	constexpr int32 SpriteModelQuadCorners[QuadCorners] = {0, 1, 2, 1, 3, 2};
	constexpr int32 SpriteModelTriangleCorners[3] = {0, 1, 2};
	constexpr uint8 SpriteModelRawCode = 1;

	uint32 PackSpriteModelColor(const Weltall::FMdlColor& Color)
	{
		return static_cast<uint32>(Color.R | Color.G << 8 | Color.B << 16);
	}

	FVector3f SpriteModelViewCorner(const Weltall::FFieldSpriteModelsFrame& Frame, const Weltall::FFieldSpriteModelPolygon& Polygon, int32 Corner)
	{
		const std::array<float, 3> At = Weltall::ViewPoint(Frame.Models[Polygon.Model].Matrix, Polygon.Corners[static_cast<std::size_t>(Corner)]);
		return FVector3f(At[0], At[1], At[2]);
	}

	/** The same polygon of the same child in the tick before, unless its sprite jumped. */
	const Weltall::FFieldSpriteModelPolygon* SpriteModelPolygonBefore(const Weltall::FFieldSpriteModelPolygon& Polygon, const Weltall::FFieldSpriteModelsFrame& Latest,
		const Weltall::FFieldFrame* Previous)
	{
		if (Previous == nullptr)
		{
			return nullptr;
		}
		for (const Weltall::FFieldSpriteModelPolygon& Before : Previous->SpriteModels.Polygons)
		{
			if (Before.BlendKey == Polygon.BlendKey)
			{
				const bool bNear = IsNearInWorld(Previous->SpriteModels.Models[Before.Model].World, Latest.Models[Polygon.Model].World, FieldJumpDistance);
				return bNear && Before.CornerCount == Polygon.CornerCount ? &Before : nullptr;
			}
		}
		return nullptr;
	}
}

void AddFieldSpriteModels(const Weltall::FFieldFrame& Latest, const Weltall::FFieldFrame* Previous, float Alpha, const FFieldSceneMesh& Mesh,
	FFieldSceneDraw& Draw)
{
	const uint32 Identity = Mesh.ViewSpaceObject();
	const Weltall::FFieldSpriteModelsFrame& Models = Latest.SpriteModels;
	for (const Weltall::FFieldSpriteModelPolygon& Polygon : Models.Polygons)
	{
		if (!Polygon.bDrawn)
		{
			Draw.SpriteModelCorners.Add(FFieldSceneDraw::NoRedirect);
			continue;
		}
		Draw.SpriteModelCorners.Add(static_cast<uint32>(Draw.DynamicCorners.Num()));
		const Weltall::FFieldSpriteModelPolygon* Before = SpriteModelPolygonBefore(Polygon, Models, Previous);
		const uint32 Dynamic = Mesh.Polygons() + static_cast<uint32>(Draw.DynamicPolygons.Num());
		const std::span<const int32> Corners = Polygon.CornerCount == 4 ? std::span<const int32>(SpriteModelQuadCorners) : std::span<const int32>(SpriteModelTriangleCorners);
		uint32 LowU = 255, LowV = 255, HighU = 0, HighV = 0;
		for (const int32 Corner : Corners)
		{
			const FVector3f Now = SpriteModelViewCorner(Models, Polygon, Corner);
			const Weltall::FMdlUv& Uv = Polygon.Uvs[static_cast<std::size_t>(Corner)];
			FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
			Out.Position = Before != nullptr ? FMath::Lerp(SpriteModelViewCorner(Previous->SpriteModels, *Before, Corner), Now, Alpha) : Now;
			Out.TexCoord = FVector2f(Uv.U, Uv.V);
			Out.Polygon = Dynamic;
			Out.Object = Identity;
			Out.Corner = Weltall::MdlPrimitiveTraits(Polygon.Type).bGouraud ? static_cast<uint32>(Corner) : 0;
			LowU = FMath::Min<uint32>(LowU, Uv.U);
			LowV = FMath::Min<uint32>(LowV, Uv.V);
			HighU = FMath::Max<uint32>(HighU, Uv.U);
			HighV = FMath::Max<uint32>(HighV, Uv.V);
		}
		FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
		State.Texture = FVramTexture::NoTexture;
		if (Weltall::MdlPrimitiveTraits(Polygon.Type).bTextured)
		{
			State.Texture = FVramTexture::Pack(Weltall::FTexturePage::FromAttribute(Polygon.TexturePage), Weltall::FClutPosition::FromAttribute(Polygon.Clut));
			State.Flags = (Polygon.Code & SpriteModelRawCode) != 0 ? FieldPolygonFlags::Raw : 0;
			State.TexelMin = LowU | LowV << 8;
			State.TexelMax = HighU | HighV << 8;
		}
		const uint32 Flags = Polygon.bFogged ? FieldColorFlags::Modulated : 0;
		Draw.Colors.Emplace(PackSpriteModelColor(Polygon.Colors[0]) | Flags, PackSpriteModelColor(Polygon.Colors[1]), PackSpriteModelColor(Polygon.Colors[2]),
			PackSpriteModelColor(Polygon.Colors[3]));
	}
}
