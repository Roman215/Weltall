#include "BattleObjectsDraw.h"

#include "Battle/Show/BattleFrameBlend.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleShowObjects.h"
#include "FieldDrawBuilder.h"
#include "Gears/GearPackets.h"
#include "Graphics/SpriteDrawing.h"
#include "Rendering/VramTexture.h"

namespace
{
	/** The GPU code's bits: raw texels, semi-transparent. */
	constexpr uint8 BattleObjectRawCode = 1;
	constexpr uint8 BattleObjectSemiTransparentCode = 2;

	/**
	 * How a polygon meets a depth sorting: a 3D object's (a gear's) sorted by depth as a model, its floor shadow kept off what
	 * stands above its floor, the stage and the trails drawn in the order over the models (the stage sorted by depth beside
	 * them where the scenery is).
	 */
	enum class EBattleObjectRole : uint8
	{
		Model,
		Shadow,
		Stage,
		OverModels,
	};

	/** A polygon as its packet draws it: its corners in view space (three or four), its texels, its colours, its texture and code. */
	struct FBattleObjectPolygon
	{
		std::array<FVector3f, 4> Corners;
		int32 CornerCount = 3;
		std::array<Weltall::FMdlUv, 4> Uvs{};
		std::array<Weltall::FMdlColor, 4> Colors{};
		/** One colour a corner, else the first for all. */
		bool bGouraud = false;
		bool bTextured = false;
		uint16 TexturePage = 0;
		uint16 Clut = 0;
		/** The page and CLUT lie in the port's rows (a dress's kept performer pieces). */
		bool bPortRows = false;
		uint8 Code = 0;
		EBattleObjectRole Role = EBattleObjectRole::Model;
	};

	/** A point through Matrix into view space, then where the drawing between frames moves the latest frame's view space. */
	FVector3f BattleObjectPoint(const Weltall::FGteMatrix& Matrix, const Weltall::FGteVector& Local, const Weltall::FViewMove& Move)
	{
		const std::array<float, 3> View = Weltall::MoveViewPoint(Move, Weltall::ViewPoint(Matrix, Local));
		return FVector3f(View[0], View[1], View[2]);
	}

	const Weltall::FViewMove& AnchorMove(const Weltall::FBattleFrameBlend* Blend, std::size_t Anchor)
	{
		return Blend != nullptr && Anchor < Blend->Moves.size() ? Blend->Moves[Anchor] : Weltall::UnmovedView;
	}

	const Weltall::FViewMove& ViewMove(const Weltall::FBattleFrameBlend* Blend)
	{
		return Blend != nullptr ? Blend->View : Weltall::UnmovedView;
	}

	uint32 PackBattleObjectColor(const Weltall::FMdlColor& Color)
	{
		return static_cast<uint32>(Color.R | Color.G << 8 | Color.B << 16);
	}

	uint32 PackBattleObjectTexel(uint32 U, uint32 V)
	{
		return U | V << 8;
	}

	/**
	 * Whether a polygon faces up as the eye sees it, within some 45 degrees: the ground the sprites stand on. An eye-facing
	 * normal's share along the world's up (the view's FloorUp, zero for the painter's order).
	 */
	bool FacesUp(const FBattleObjectPolygon& Polygon, const FVector3f& Up)
	{
		constexpr float GroundFacing = 0.7f;
		if (Up.IsZero()) return false;
		FVector3f Normal = FVector3f::CrossProduct(Polygon.Corners[1] - Polygon.Corners[0], Polygon.Corners[2] - Polygon.Corners[0]).GetSafeNormal();
		if (FVector3f::DotProduct(Normal, Polygon.Corners[0]) > 0.0f) Normal = -Normal;
		return FVector3f::DotProduct(Normal, Up) >= GroundFacing;
	}

	void AddBattleObjectPolygon(const FBattleObjectPolygon& Polygon, bool bSceneryByDepth, FFieldSceneDraw& Draw, TArray<FScenePolygonItem>& Items)
	{
		FScenePolygonItem& Item = Items.AddDefaulted_GetRef();
		Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
		Item.CornerCount = Polygon.CornerCount == 4 ? QuadCorners : 3;
		Item.bTextured = Polygon.bTextured;
		Item.bSemiTransparent = (Polygon.Code & BattleObjectSemiTransparentCode) != 0;
		Item.bModel = Polygon.Role == EBattleObjectRole::Model;
		Item.bShadow = Polygon.Role == EBattleObjectRole::Shadow;
		Item.bOverModels = Polygon.Role == EBattleObjectRole::Stage || Polygon.Role == EBattleObjectRole::OverModels;
		Item.bScenery = bSceneryByDepth && Polygon.Role == EBattleObjectRole::Stage;
		Item.bGround = Polygon.Role == EBattleObjectRole::Stage && FacesUp(Polygon, Draw.FloorUp);
		const uint32 Number = static_cast<uint32>(Draw.DynamicPolygons.Num());
		for (uint32 At = 0; At < Item.CornerCount; ++At)
		{
			const std::size_t Corner = static_cast<std::size_t>(Polygon.CornerCount == 4 ? QuadTriangleCorners[At] : static_cast<int32>(At));
			FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
			Out.Position = Polygon.Corners[Corner];
			Out.TexCoord = FVector2f(Polygon.Uvs[Corner].U, Polygon.Uvs[Corner].V);
			Out.Polygon = Number;
			Out.Object = 0;
			Out.Corner = Polygon.bGouraud ? static_cast<uint32>(Corner) : 0;
		}
		FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
		State.Texture = FVramTexture::NoTexture;
		if (Polygon.bTextured)
		{
			const Weltall::FTexturePage Page = Polygon.bPortRows ? Weltall::FTexturePage::FromPortAttribute(Polygon.TexturePage)
																 : Weltall::FTexturePage::FromAttribute(Polygon.TexturePage);
			const Weltall::FClutPosition Clut = Polygon.bPortRows ? Weltall::FClutPosition::FromPortAttribute(Polygon.Clut)
																  : Weltall::FClutPosition::FromAttribute(Polygon.Clut);
			State.Texture = FVramTexture::Pack(Page, Clut);
			State.Flags = (Polygon.Code & BattleObjectRawCode) != 0 ? FieldPolygonFlags::Raw : 0;
			if (Polygon.Role == EBattleObjectRole::Shadow)
			{
				State.Flags |= FieldPolygonFlags::BroadShadow;
			}
			uint32 LowU = 255, LowV = 255, HighU = 0, HighV = 0;
			for (int32 Corner = 0; Corner < Polygon.CornerCount; ++Corner)
			{
				const Weltall::FMdlUv& Uv = Polygon.Uvs[static_cast<std::size_t>(Corner)];
				LowU = FMath::Min<uint32>(LowU, Uv.U);
				LowV = FMath::Min<uint32>(LowV, Uv.V);
				HighU = FMath::Max<uint32>(HighU, Uv.U);
				HighV = FMath::Max<uint32>(HighV, Uv.V);
			}
			State.TexelMin = PackBattleObjectTexel(LowU, LowV);
			State.TexelMax = PackBattleObjectTexel(HighU, HighV);
			Item.PageBlend = Page.Blend;
		}
		const auto ColorAt = [&Polygon](std::size_t Corner) { return PackBattleObjectColor(Polygon.Colors[Polygon.bGouraud ? Corner : 0]); };
		Draw.Colors.Emplace(ColorAt(0), ColorAt(1), ColorAt(2), ColorAt(3));
	}

	FBattleObjectPolygon PartPolygon(const Weltall::FBattleObjectsFrame& Objects, const Weltall::FBattleFrameBlend* Blend, const Weltall::FBattleObjectLink& Link)
	{
		const Weltall::FBattleDrawnPolygon& Drawn = Objects.Polygons[Link.Index];
		const Weltall::FViewMove& Move = AnchorMove(Blend, Blend != nullptr ? Blend->FirstPart + Drawn.Part : 0);
		const Weltall::FBattleDrawnPart& Part = Objects.Parts[Drawn.Part];
		const Weltall::FMdlPrimitive& Primitive = Part.Part->Primitives[Drawn.Number];
		const Weltall::FMdlPrimitiveTraits& Traits = Primitive.Traits();
		FBattleObjectPolygon Polygon;
		Polygon.CornerCount = Traits.Corners;
		for (std::size_t Corner = 0; Corner < Traits.Corners; ++Corner)
		{
			Polygon.Corners[Corner] = BattleObjectPoint(Part.Matrix, Part.Part->Vertices[Primitive.Vertices[Corner]], Move);
		}
		if (Primitive.Type == Weltall::EMdlPrimitiveType::EnvironmentMappedTriangle)
		{
			std::copy(Drawn.Texels.begin(), Drawn.Texels.end(), Polygon.Uvs.begin());
		}
		else
		{
			Polygon.Uvs = Primitive.Uvs;
		}
		Polygon.Colors = Drawn.Colors;
		Polygon.bGouraud = Traits.bGouraud;
		Polygon.bTextured = Traits.bTextured;
		Polygon.TexturePage = Part.Packets->Textures[Drawn.Number].TexturePage;
		Polygon.Clut = Part.Packets->Textures[Drawn.Number].Clut;
		Polygon.bPortRows = Part.bPortRows;
		Polygon.Code = Part.Packets->Packets[Drawn.Number].Code;
		// The gears' soles stand under the stage's floor: by depth it would hide them.
		Polygon.Role = Part.Slot == Weltall::FBattleObjects::StageSlot ? EBattleObjectRole::Stage : EBattleObjectRole::Model;
		return Polygon;
	}

	FBattleObjectPolygon ShadowPolygon(const Weltall::FBattleObjectsFrame& Objects, const Weltall::FBattleFrameBlend* Blend, const Weltall::FBattleObjectLink& Link)
	{
		using namespace Weltall::BattleObjectShadow;
		const Weltall::FBattleDrawnShadow& Shadow = Objects.Shadows[Link.Index];
		const Weltall::FViewMove& Move = AnchorMove(Blend, Blend != nullptr ? Blend->FirstShadow + Link.Index : 0);
		FBattleObjectPolygon Polygon;
		Polygon.CornerCount = 4;
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			Polygon.Corners[Corner] = BattleObjectPoint(Shadow.Matrix, Shadow.Corners[Corner], Move);
		}
		Polygon.Uvs = {{{FirstTexel, FirstTexel}, {LastTexel, FirstTexel}, {FirstTexel, LastTexel}, {LastTexel, LastTexel}}};
		Polygon.Colors[0] = Shadow.Colour;
		Polygon.bTextured = true;
		Polygon.TexturePage = TexturePage;
		Polygon.Clut = Clut;
		Polygon.Code = Code;
		Polygon.Role = EBattleObjectRole::Shadow;
		return Polygon;
	}

	/** A cloth's triangle: its points in the world (where the blend puts them between frames) through the view. */
	FBattleObjectPolygon ClothPolygon(const Weltall::FBattleObjectsFrame& Objects, const Weltall::FBattleFrameBlend* Blend, const Weltall::FBattleObjectLink& Link)
	{
		const Weltall::FGearDrawnClothTriangle& Triangle = Objects.Cloths[Link.Index];
		const bool bBlended = Blend != nullptr && Link.Index < Blend->Cloths.size();
		const Weltall::FAffine View = Weltall::FAffine::From(Objects.View);
		FBattleObjectPolygon Polygon;
		for (std::size_t Corner = 0; Corner < 3; ++Corner)
		{
			const Weltall::FGteVector& Point = Triangle.Points[Corner];
			const std::array<float, 3> World = bBlended
				? Blend->Cloths[Link.Index][Corner]
				: std::array<float, 3>{static_cast<float>(Point[0]), static_cast<float>(Point[1]), static_cast<float>(Point[2])};
			const std::array<float, 3> Turned = View.Turn(World);
			const std::array<float, 3> Moved =
				Weltall::MoveViewPoint(ViewMove(Blend), {Turned[0] + View.T[0], Turned[1] + View.T[1], Turned[2] + View.T[2]});
			Polygon.Corners[Corner] = FVector3f(Moved[0], Moved[1], Moved[2]);
			Polygon.Uvs[Corner] = Triangle.Uvs[Corner];
			Polygon.Colors[Corner] = Triangle.Colors[Corner];
		}
		Polygon.bGouraud = true;
		Polygon.bTextured = true;
		Polygon.TexturePage = Triangle.TexturePage;
		Polygon.Clut = Triangle.Clut;
		Polygon.Code = Weltall::GearClothCode;
		return Polygon;
	}

	/** A trail's quad: on the screen, a point at the projection's distance across and down from the centre; in the world, through the view. */
	FBattleObjectPolygon TrailPolygon(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleObjectsFrame& Objects, const Weltall::FBattleFrameBlend* Blend,
		const Weltall::FBattleObjectLink& Link)
	{
		using namespace Weltall::BattleTrailQuad;
		const Weltall::FBattleTrailQuad& Quad = Objects.Trails[Link.Index];
		FBattleObjectPolygon Polygon;
		Polygon.CornerCount = 4;
		for (std::size_t Corner = 0; Corner < 4; ++Corner)
		{
			const Weltall::FGteVector& At = Quad.Corners[Corner];
			if (Quad.Kind == 0)
			{
				const std::array<float, 3> Screen = Weltall::MoveViewPoint(ViewMove(Blend),
					{static_cast<float>(At[0] - Frame.CentreX), static_cast<float>(At[1] - Frame.CentreY), static_cast<float>(Frame.ProjectionDistance)});
				Polygon.Corners[Corner] = FVector3f(Screen[0], Screen[1], Screen[2]);
			}
			else
			{
				Polygon.Corners[Corner] = BattleObjectPoint(Objects.View, At, ViewMove(Blend));
			}
		}
		Polygon.Uvs = {{{0, Row}, {0, Row}, {LastTexel, Row}, {LastTexel, Row}}};
		Polygon.Colors[0] = {Quad.Colour[0], Quad.Colour[1], Quad.Colour[2]};
		Polygon.bTextured = true;
		Polygon.TexturePage = TexturePage;
		Polygon.Clut = Clut;
		Polygon.Code = Quad.bSemiTransparent ? SemiTransparentCode : Code;
		Polygon.Role = EBattleObjectRole::OverModels;
		return Polygon;
	}
}

void AddBattleObjectLink(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, const Weltall::FBattleObjectLink& Link, bool bSceneryByDepth,
	FFieldSceneDraw& Draw, TArray<FScenePolygonItem>& Items)
{
	const Weltall::FBattleObjectsFrame& Objects = *Frame.Objects;
	switch (Link.Kind)
	{
	case Weltall::EBattleObjectDraw::Polygon:
		AddBattleObjectPolygon(PartPolygon(Objects, Blend, Link), bSceneryByDepth, Draw, Items);
		break;
	case Weltall::EBattleObjectDraw::Shadow:
		AddBattleObjectPolygon(ShadowPolygon(Objects, Blend, Link), bSceneryByDepth, Draw, Items);
		break;
	case Weltall::EBattleObjectDraw::Cloth:
		AddBattleObjectPolygon(ClothPolygon(Objects, Blend, Link), bSceneryByDepth, Draw, Items);
		break;
	case Weltall::EBattleObjectDraw::Trail:
		AddBattleObjectPolygon(TrailPolygon(Frame, Objects, Blend, Link), bSceneryByDepth, Draw, Items);
		break;
	}
}
