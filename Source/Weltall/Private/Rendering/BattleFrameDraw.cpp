#include "Rendering/BattleRendering.h"

#include "Battle/Show/BattleFrameBlend.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowFrame.h"
#include "BattleObjectsDraw.h"
#include "FieldDrawBuilder.h"
#include "Graphics/Texture.h"
#include "Menu/MenuDrawing.h"
#include "Presentation/DrawList.h"
#include "Rendering/DrawListRendering.h"
#include "Rendering/VramTexture.h"
#include "ScenePolygonOrder.h"

#include <optional>

namespace
{
	/** The field scene's passes project about the middle of the field's 320 x 224 screen, which is the battle's too. */
	constexpr float BattleScreenMiddleX = 160.0f;
	constexpr float BattleScreenMiddleY = 112.0f;
	/** The GPU code's bits: raw texels, semi-transparent. */
	constexpr uint32 BattleRawTexels = 1u << 24;
	constexpr uint32 BattleSemiTransparent = 1u << 25;
	constexpr uint8 BattleTriangleSemiTransparent = 0x02;
	/** The fade's tile, from (-32, -32) to (320, 240). */
	constexpr Weltall::FScreenRect BattleFadeTile{-32, -32, 352, 272};
	/** The backdrop lies far behind anything the battle draws, and its bands reach a screen's width past either side. */
	constexpr float BattleBackdropDepth = 65536.0f;
	constexpr int32 BattleBackdropBeyond = Weltall::FieldBackdropWidth;

	uint32 PackBattleTexel(int32 U, int32 V)
	{
		return static_cast<uint32>(FMath::Clamp(U, 0, 255)) | static_cast<uint32>(FMath::Clamp(V, 0, 255)) << 8;
	}

	uint32 PackBattleColor(uint32 ColorCode)
	{
		return ColorCode & 0xFFFFFF;
	}

	FVector3f BattleViewVector(const std::array<float, 3>& Point)
	{
		return FVector3f(Point[0], Point[1], Point[2]);
	}

	/** What the drawing between two of the show's frames moves, or nothing drawing the latest as it is. */
	const Weltall::FViewMove& BattleMove(const Weltall::FBattleFrameBlend* Blend, std::size_t Anchor)
	{
		return Blend != nullptr && Anchor < Blend->Moves.size() ? Blend->Moves[Anchor] : Weltall::UnmovedView;
	}

	/** The frame's polygons as the passes take them, one link of its drawing order at a time. */
	class FBattleDrawBuilder
	{
	public:
		FBattleDrawBuilder(const Weltall::FBattleShowFrame& InFrame, const Weltall::FBattleFrameBlend* InBlend, bool bInSceneryByDepth, FFieldSceneDraw& InDraw)
			: Frame(InFrame), Blend(InBlend), Draw(InDraw), Distance(InBlend != nullptr ? InBlend->ProjectionDistance : static_cast<float>(InFrame.ProjectionDistance)),
			  bSceneryByDepth(bInSceneryByDepth)
		{
			Distance = FMath::Max(Distance, 1.0f);
		}

		void AddQuad(const Weltall::FBattleShowQuad& Entry)
		{
			const Weltall::FSpriteQuad& Quad = Entry.Quad;
			const Weltall::EBattleShowDrawer Kind = Frame.Drawers[Entry.Drawer].Kind;
			std::array<FVector3f, 4> Corners;
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Corners[Corner] = BattleViewVector(Moved(Entry.Drawer, Quad.ViewCorner(Corner)));
			}
			if (Kind == Weltall::EBattleShowDrawer::Shadow)
			{
				FlattenShadow(Corners);
			}
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = QuadCorners;
			Item.bTextured = true;
			Item.bSemiTransparent = (Quad.ColorCode & BattleSemiTransparent) != 0;
			Item.bShadow = Kind == Weltall::EBattleShowDrawer::Shadow;
			Item.bSprite = !Item.bShadow;
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (const int32 Corner : QuadTriangleCorners)
			{
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = Corners[static_cast<std::size_t>(Corner)];
				Out.TexCoord = FVector2f((Corner & 1) != 0 ? Quad.U1 : Quad.U0, (Corner & 2) != 0 ? Quad.V1 : Quad.V0);
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = 0;
			}
			// A sprite's parts may lie in VRAM's port rows (Id's battle sprite).
			const Weltall::FTexturePage Page = Weltall::FTexturePage::FromPortAttribute(Quad.TexturePage);
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::Pack(Page, Weltall::FClutPosition::FromPortAttribute(Quad.Clut));
			State.Flags = FieldPolygonFlags::Sprite | ((Quad.ColorCode & BattleRawTexels) != 0 ? FieldPolygonFlags::Raw : 0);
			if (Item.bSprite)
			{
				State.Flags |= FieldPolygonFlags::AtEntry(Quad.OtIndex);
			}
			else
			{
				// A card at the feet's depth, partly under the floor drawn in front of it: what the stage raises there may stand
				// as high as under a gear's shadow before it hides the card.
				State.Flags |= FieldPolygonFlags::BroadShadow;
			}
			// The quads map their texels edge to edge: the last texel is one before the far edge.
			State.TexelMin = PackBattleTexel(FMath::Min(Quad.U0, Quad.U1), FMath::Min(Quad.V0, Quad.V1));
			State.TexelMax = PackBattleTexel(FMath::Max(Quad.U0, Quad.U1) - 1, FMath::Max(Quad.V0, Quad.V1) - 1);
			Item.PageBlend = Page.Blend;
			const uint32 Color = PackBattleColor(Quad.ColorCode);
			Draw.Colors.Emplace(Color, Color, Color, Color);
		}

		void AddTriangle(const Weltall::FBattleShowTriangle& Triangle)
		{
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = 3;
			Item.bSemiTransparent = ((Triangle.ColorCodes[0] >> 24) & BattleTriangleSemiTransparent) != 0;
			Item.bOverModels = true;
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = BattleViewVector(Moved(Triangle.Drawer, Weltall::ViewPoint(Triangle.Matrix, Triangle.Corners[Corner])));
				Out.TexCoord = FVector2f::ZeroVector;
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = Corner;
			}
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::NoTexture;
			Draw.Colors.Emplace(PackBattleColor(Triangle.ColorCodes[0]), PackBattleColor(Triangle.ColorCodes[1]), PackBattleColor(Triangle.ColorCodes[2]), 0u);
		}

		/**
		 * A spawned sprite's dot or square: flat on the screen about its place, at its place's depth. The original's is
		 * whole pixels of its screen, a dot one of them; here its side is the one its size projects to, a pixel of that
		 * screen at least.
		 */
		void AddTile(const Weltall::FBattleShowTile& Tile)
		{
			const std::array<float, 3> Centre = Moved(Tile.Drawer, Weltall::ViewPoint(Frame.View, Tile.Place));
			if (Centre[2] <= 0.0f)
			{
				return;
			}
			// A pixel of the original's screen at the tile's depth, in the view's units.
			const float Pixel = Centre[2] / Distance;
			float Side = Pixel;
			if (!Tile.bDot)
			{
				const Weltall::FGteVector Along{static_cast<int16>(Tile.Place[0] + Tile.Size), Tile.Place[1], Tile.Place[2]};
				const std::array<float, 3> Edge = Moved(Tile.Drawer, Weltall::ViewPoint(Frame.View, Along));
				if (Edge[2] > 0.0f)
				{
					Side = FMath::Max(FMath::Abs(Edge[0] / Edge[2] - Centre[0] / Centre[2]) * Distance, 1.0f) * Pixel;
				}
			}
			// The pixel the place falls in is the middle of the square.
			const float Left = Centre[0] + Pixel / 2.0f - Side / 2.0f;
			const float Top = Centre[1] + Pixel / 2.0f - Side / 2.0f;
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = QuadCorners;
			Item.bSemiTransparent = (Tile.ColorCode & BattleSemiTransparent) != 0;
			Item.bOverModels = true;
			Item.bSetsBlend = true;
			Item.PageBlend = static_cast<Weltall::EBlendMode>(Tile.Blend & 3);
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (const int32 Corner : QuadTriangleCorners)
			{
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = FVector3f(Left + ((Corner & 1) != 0 ? Side : 0.0f), Top + ((Corner & 2) != 0 ? Side : 0.0f), Centre[2]);
				Out.TexCoord = FVector2f::ZeroVector;
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = 0;
			}
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::NoTexture;
			const uint32 Color = PackBattleColor(Tile.ColorCode);
			Draw.Colors.Emplace(Color, Color, Color, Color);
		}

		/** A spawned sprite's line: a pixel of the original's screen wide, from one end to the other, each at its depth. */
		void AddLine(const Weltall::FBattleShowLine& Line)
		{
			const std::array<float, 3> From = Moved(Line.Drawer, Weltall::ViewPoint(Line.Matrix, Line.Ends[0]));
			const std::array<float, 3> To = Moved(Line.Drawer, Weltall::ViewPoint(Line.Matrix, Line.Ends[1]));
			if (From[2] <= 0.0f || To[2] <= 0.0f)
			{
				return;
			}
			// On the screen: the line's ends at their pixels' middles, half a pixel to either side and past each end.
			const FVector2f Start(From[0] / From[2] * Distance + 0.5f, From[1] / From[2] * Distance + 0.5f);
			const FVector2f End(To[0] / To[2] * Distance + 0.5f, To[1] / To[2] * Distance + 0.5f);
			FVector2f Along = End - Start;
			if (!Along.Normalize())
			{
				Along = FVector2f(1.0f, 0.0f);
			}
			const FVector2f Side(-Along.Y * 0.5f, Along.X * 0.5f);
			const std::array<FVector2f, 4> Screen{Start - Along * 0.5f - Side, Start - Along * 0.5f + Side, End + Along * 0.5f - Side, End + Along * 0.5f + Side};
			const std::array<float, 4> Depths{From[2], From[2], To[2], To[2]};
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = QuadCorners;
			Item.bSemiTransparent = (Line.ColorCode & BattleSemiTransparent) != 0;
			Item.bOverModels = true;
			Item.bSetsBlend = Line.Blend >= 0;
			Item.PageBlend = static_cast<Weltall::EBlendMode>(Line.Blend & 3);
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (const int32 Corner : QuadTriangleCorners)
			{
				const std::size_t At = static_cast<std::size_t>(Corner);
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = FVector3f(Screen[At].X * Depths[At] / Distance, Screen[At].Y * Depths[At] / Distance, Depths[At]);
				Out.TexCoord = FVector2f::ZeroVector;
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = 0;
			}
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::NoTexture;
			const uint32 Color = PackBattleColor(Line.ColorCode);
			Draw.Colors.Emplace(Color, Color, Color, Color);
		}

		/** A quad placed on the original's screen (a span of a script's trail): each corner where it lies there, at its end's depth. */
		void AddScreenQuad(const Weltall::FBattleShowScreenQuad& Quad)
		{
			if (Quad.Depths[0] <= 0 || Quad.Depths[1] <= 0)
			{
				return;
			}
			// Placed on the latest frame's screen: back into its view space, then moved with its drawer.
			const float LatestDistance = static_cast<float>(FMath::Max(Frame.ProjectionDistance, 1));
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = QuadCorners;
			Item.bTextured = true;
			Item.bSemiTransparent = (Quad.ColorCode & BattleSemiTransparent) != 0;
			Item.bOverModels = true;
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (const int32 Corner : QuadTriangleCorners)
			{
				const std::size_t At = static_cast<std::size_t>(Corner);
				const float Depth = static_cast<float>(Quad.Depths[At >> 1]);
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = BattleViewVector(Moved(Quad.Drawer, {static_cast<float>(Quad.Corners[At][0] - Frame.CentreX) * Depth / LatestDistance,
					static_cast<float>(Quad.Corners[At][1] - Frame.CentreY) * Depth / LatestDistance, Depth}));
				Out.TexCoord = FVector2f((Corner & 1) != 0 ? Quad.U1 : Quad.U0, (Corner & 2) != 0 ? Quad.V1 : Quad.V0);
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = 0;
			}
			const Weltall::FTexturePage Page = Weltall::FTexturePage::FromPortAttribute(Quad.TexturePage);
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::Pack(Page, Weltall::FClutPosition::FromPortAttribute(Quad.Clut));
			State.Flags = (Quad.ColorCode & BattleRawTexels) != 0 ? FieldPolygonFlags::Raw : 0;
			State.TexelMin = PackBattleTexel(FMath::Min(Quad.U0, Quad.U1), FMath::Min(Quad.V0, Quad.V1));
			State.TexelMax = PackBattleTexel(FMath::Max(Quad.U0, Quad.U1) - 1, FMath::Max(Quad.V0, Quad.V1) - 1);
			Item.PageBlend = Page.Blend;
			const uint32 Color = PackBattleColor(Quad.ColorCode);
			Draw.Colors.Emplace(Color, Color, Color, Color);
		}

		/**
		 * A polygon of a model a sprite draws: its corners through its model's matrix, about the model's own centre on the
		 * screen when that is not the frame's; an untextured one blends in the mode its drawing mode sets.
		 */
		void AddModelPolygon(const Weltall::FBattleShowModelPolygon& Polygon)
		{
			const Weltall::FBattleShowModel& Model = Frame.Models[Polygon.Model];
			const float Across = static_cast<float>(Model.CentreX - Frame.CentreX) / Distance;
			const float Lower = static_cast<float>(Model.CentreY - Frame.CentreY) / Distance;
			const int32 CornerCount = Polygon.CornerCount;
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = CornerCount == 4 ? QuadCorners : 3;
			Item.bTextured = Polygon.bTextured;
			Item.bSemiTransparent = (Polygon.ColorCodes[0] & BattleSemiTransparent) != 0;
			Item.bOverModels = true;
			const uint32 Number = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (uint32 At = 0; At < Item.CornerCount; ++At)
			{
				const std::size_t Corner = static_cast<std::size_t>(CornerCount == 4 ? QuadTriangleCorners[At] : static_cast<int32>(At));
				const std::array<float, 3> Point = Moved(Model.Drawer, Weltall::ViewPoint(Model.Matrix, Polygon.Corners[Corner]));
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = FVector3f(Point[0] + Across * Point[2], Point[1] + Lower * Point[2], Point[2]);
				Out.TexCoord = FVector2f(Polygon.Uvs[Corner].U, Polygon.Uvs[Corner].V);
				Out.Polygon = Number;
				Out.Object = 0;
				Out.Corner = Polygon.bGouraud ? static_cast<uint32>(Corner) : 0;
			}
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::NoTexture;
			if (Polygon.bTextured)
			{
				const Weltall::FTexturePage Page = Weltall::FTexturePage::FromAttribute(Polygon.TexturePage);
				State.Texture = FVramTexture::Pack(Page, Weltall::FClutPosition::FromAttribute(Polygon.Clut));
				State.Flags = (Polygon.ColorCodes[0] & BattleRawTexels) != 0 ? FieldPolygonFlags::Raw : 0;
				int32 LowU = 255, LowV = 255, HighU = 0, HighV = 0;
				for (int32 Corner = 0; Corner < CornerCount; ++Corner)
				{
					const Weltall::FMdlUv& Uv = Polygon.Uvs[static_cast<std::size_t>(Corner)];
					LowU = FMath::Min<int32>(LowU, Uv.U);
					LowV = FMath::Min<int32>(LowV, Uv.V);
					HighU = FMath::Max<int32>(HighU, Uv.U);
					HighV = FMath::Max<int32>(HighV, Uv.V);
				}
				State.TexelMin = PackBattleTexel(LowU, LowV);
				State.TexelMax = PackBattleTexel(HighU, HighV);
				Item.PageBlend = Page.Blend;
			}
			else
			{
				Item.bSetsBlend = Polygon.Blend >= 0;
				Item.PageBlend = static_cast<Weltall::EBlendMode>(Polygon.Blend & 3);
			}
			const auto ColorAt = [&Polygon](std::size_t Corner) { return PackBattleColor(Polygon.ColorCodes[Polygon.bGouraud ? Corner : 0]); };
			Draw.Colors.Emplace(ColorAt(0), ColorAt(1), ColorAt(2), ColorAt(3));
		}

		void AddObject(const Weltall::FBattleObjectLink& Link)
		{
			AddBattleObjectLink(Frame, Blend, Link, bSceneryByDepth, Draw, AddedItems);
		}

		/** The stage's backdrop at its place in the order: the dome's packets, then the panoramas' (the second first), as the GPU draws them. */
		void AddBackdrop()
		{
			std::optional<std::array<int16, 4>> Window;
			// An untextured semi-transparent packet blends by the page the last draw mode or textured packet set.
			Weltall::EBlendMode PageBlend = Weltall::EBlendMode::Average;
			for (std::size_t Index = 0; Index < Frame.Backdrop.Dome.size(); ++Index)
			{
				const Weltall::FBattleDomePacket& Packet = Frame.Backdrop.Dome[Index];
				if (Packet.bDrawMode)
				{
					Window = Packet.Window[2] != 0 ? std::optional<std::array<int16, 4>>(Packet.Window) : std::nullopt;
					PageBlend = Weltall::FTexturePage::FromAttribute(Packet.DrawPage).Blend;
					continue;
				}
				const Weltall::FFieldBackdropQuad& Quad = Packet.Quad;
				std::array<FVector2f, 4> Screen = ScreenCorners(Quad);
				if (Blend != nullptr && Index < Blend->DomeCorners.size())
				{
					for (std::size_t Corner = 0; Corner < 4; ++Corner)
					{
						Screen[Corner] = FVector2f(Blend->DomeCorners[Index][Corner][0], Blend->DomeCorners[Index][Corner][1]);
					}
				}
				if (Quad.Kind == Weltall::FFieldBackdropQuad::EKind::Textured)
				{
					PageBlend = Weltall::FTexturePage::FromAttribute(Quad.TexturePage).Blend;
				}
				else
				{
					// The bands span the screen: in a wider view they go on to either side.
					for (std::size_t Corner = 0; Corner < 4; ++Corner)
					{
						Screen[Corner].X = Quad.Corners[Corner][0] == 0 ? -BattleBackdropBeyond : Screen[Corner].X + BattleBackdropBeyond;
					}
				}
				AddBackdropQuad(Quad, Screen, Packet.bSemiTransparent, Window, PageBlend);
			}
			const Weltall::FBattleBackdropFrame& Placed = Blend != nullptr ? Blend->Backdrop : Frame.Backdrop;
			for (std::size_t Slot = Placed.Panoramas.size(); Slot-- > 0;)
			{
				Weltall::DrawFieldBackdrop(Placed.Panoramas[Slot], -BattleBackdropBeyond, Weltall::FieldBackdropWidth + BattleBackdropBeyond, BackdropQuads);
				for (const Weltall::FFieldBackdropQuad& Quad : BackdropQuads)
				{
					AddBackdropQuad(Quad, ScreenCorners(Quad), false, std::nullopt, PageBlend);
				}
			}
		}

		TArray<FScenePolygonItem>& Items() { return AddedItems; }

	private:
		/**
		 * A sprite's shadow as the GPU draws it: once projected, its first two corners take their average height on the
		 * screen, and the last two theirs; each corner keeps its depth.
		 */
		void FlattenShadow(std::array<FVector3f, 4>& Corners) const
		{
			for (const std::array<std::size_t, 2>& Edge : {std::array<std::size_t, 2>{0, 1}, std::array<std::size_t, 2>{2, 3}})
			{
				FVector3f& A = Corners[Edge[0]];
				FVector3f& B = Corners[Edge[1]];
				if (A.Z <= 0.0f || B.Z <= 0.0f)
				{
					continue;
				}
				const float Screen = (A.Y * Distance / A.Z + B.Y * Distance / B.Z) / 2.0f;
				A.Y = Screen * A.Z / Distance;
				B.Y = Screen * B.Z / Distance;
			}
		}

		/** A point of the latest frame's view space where the drawing between frames puts it. */
		std::array<float, 3> Moved(std::size_t Drawer, const std::array<float, 3>& Point) const
		{
			return Weltall::MoveViewPoint(BattleMove(Blend, Drawer), Point);
		}

		static std::array<FVector2f, 4> ScreenCorners(const Weltall::FFieldBackdropQuad& Quad)
		{
			std::array<FVector2f, 4> Screen;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				Screen[Corner] = FVector2f(Quad.Corners[Corner][0], Quad.Corners[Corner][1]);
			}
			return Screen;
		}

		/** A backdrop packet placed far behind where Screen puts it on the screen, raw textured in its window or its own texels. */
		void AddBackdropQuad(const Weltall::FFieldBackdropQuad& Quad, const std::array<FVector2f, 4>& Screen, bool bSemiTransparent,
			const std::optional<std::array<int16, 4>>& Window, Weltall::EBlendMode PageBlend)
		{
			const bool bTextured = Quad.Kind == Weltall::FFieldBackdropQuad::EKind::Textured;
			FScenePolygonItem& Item = AddedItems.AddDefaulted_GetRef();
			Item.FirstCorner = static_cast<uint32>(Draw.DynamicCorners.Num());
			Item.CornerCount = QuadCorners;
			Item.bTextured = bTextured;
			Item.bSemiTransparent = bSemiTransparent;
			Item.bBackground = true;
			Item.PageBlend = PageBlend;
			const float Scale = BattleBackdropDepth / Distance;
			const FVector2f Centre = Blend != nullptr ? FVector2f(Blend->CentreX, Blend->CentreY) : FVector2f(Frame.CentreX, Frame.CentreY);
			const uint32 Polygon = static_cast<uint32>(Draw.DynamicPolygons.Num());
			for (const int32 Corner : QuadTriangleCorners)
			{
				const std::size_t At = static_cast<std::size_t>(Corner);
				FFieldSceneCorner& Out = Draw.DynamicCorners.AddDefaulted_GetRef();
				Out.Position = FVector3f((Screen[At].X - Centre.X) * Scale, (Screen[At].Y - Centre.Y) * Scale, BattleBackdropDepth);
				Out.TexCoord = FVector2f(Quad.Texels[At][0], Quad.Texels[At][1]);
				Out.Polygon = Polygon;
				Out.Object = 0;
				Out.Corner = Quad.Kind == Weltall::FFieldBackdropQuad::EKind::Graded ? static_cast<uint32>(Corner) : 0;
			}
			FFieldScenePolygonState& State = Draw.DynamicPolygons.AddDefaulted_GetRef();
			State.Texture = FVramTexture::NoTexture;
			if (bTextured)
			{
				State.Texture = FVramTexture::Pack(Weltall::FTexturePage::FromAttribute(Quad.TexturePage), Weltall::FClutPosition::FromAttribute(Quad.Clut));
				State.Flags = FieldPolygonFlags::Raw;
				if (Window)
				{
					const std::array<int16, 4>& Rectangle = *Window;
					State.Flags |= FieldPolygonFlags::Window;
					State.TexelMin = PackBattleTexel(Rectangle[0], Rectangle[1]);
					State.TexelMax = PackBattleTexel(~(Rectangle[2] - 1) & 0xFF, ~(Rectangle[3] - 1) & 0xFF);
				}
				else
				{
					// The packet's texels run to the far column and row themselves.
					State.TexelMin = PackBattleTexel(FMath::Min(Quad.Texels[0][0], Quad.Texels[1][0]), FMath::Min(Quad.Texels[0][1], Quad.Texels[2][1]));
					State.TexelMax = PackBattleTexel(FMath::Max(Quad.Texels[0][0], Quad.Texels[1][0]), FMath::Max(Quad.Texels[0][1], Quad.Texels[2][1]));
				}
			}
			const auto Color = [&Quad](std::size_t Corner) { return PackBattleColor(Quad.Colors[Corner][0] | Quad.Colors[Corner][1] << 8 | Quad.Colors[Corner][2] << 16); };
			Draw.Colors.Emplace(Color(0), Color(1), Color(2), Color(3));
		}

		const Weltall::FBattleShowFrame& Frame;
		const Weltall::FBattleFrameBlend* Blend;
		FFieldSceneDraw& Draw;
		/** The projection's distance this drawing projects with. */
		float Distance;
		/** The stage is sorted by depth beside the 3D objects (EFieldSorting::DepthBufferWithScenery). */
		bool bSceneryByDepth = false;
		TArray<FScenePolygonItem> AddedItems;
		std::vector<Weltall::FFieldBackdropQuad> BackdropQuads;
	};

	void ResetBattleDraw(FFieldSceneDraw& Draw)
	{
		Draw.Objects.Reset();
		Draw.Colors.Reset();
		Draw.DynamicCorners.Reset();
		Draw.DynamicPolygons.Reset();
		Draw.SpriteCells.Reset();
		Draw.SpriteModelCorners.Reset();
		Draw.Order.Reset();
		Draw.Runs.Reset();
		Draw.WaveGrid.Reset();
		Draw.bWave = false;
		Draw.WaveRuns = 0;
		Draw.ParticleCorners = 0;
		Draw.BackdropCorners = 0;
		Draw.BackdropCornerCount = 0;
		Draw.BackdropQuads.clear();
		Draw.Redirects.Reset();
		Draw.FirstClothCorner = 0;
		Draw.bGearsDrawn = false;
		Draw.bClipped = false;
	}

	using FBattleLinks = TConstArrayView<Weltall::FBattleShowLink>;

	bool IsBattleHudLink(const Weltall::FBattleShowLink& Link)
	{
		return Link.Kind == Weltall::EBattleShowLink::Hud;
	}

	/** The frame's layers drawn apart from its polygons: the fade and the HUD. */
	bool IsBattleLayerLink(const Weltall::FBattleShowLink& Link)
	{
		return Link.Kind == Weltall::EBattleShowLink::Fade || IsBattleHudLink(Link);
	}

	/** The links the frame's scene takes: those the GPU draws before the first layer. */
	FBattleLinks BattleSceneLinks(const Weltall::FBattleShowFrame& Frame)
	{
		const FBattleLinks Order(Frame.Order.data(), static_cast<int32>(Frame.Order.size()));
		const int32 Layer = Order.IndexOfByPredicate(IsBattleLayerLink);
		return Layer == INDEX_NONE ? Order : Order.Left(Layer);
	}

	/** The links drawn over a layer: from the fade to the HUD, or all those after the HUD. */
	FBattleLinks BattleOverlayLinks(const Weltall::FBattleShowFrame& Frame, bool bOverHud)
	{
		const FBattleLinks Order(Frame.Order.data(), static_cast<int32>(Frame.Order.size()));
		const int32 Hud = Order.IndexOfByPredicate(IsBattleHudLink);
		if (bOverHud)
		{
			return Hud == INDEX_NONE ? FBattleLinks() : Order.RightChop(Hud + 1);
		}
		const int32 Layer = Order.IndexOfByPredicate(IsBattleLayerLink);
		const int32 End = Hud == INDEX_NONE ? Order.Num() : Hud;
		return Layer == INDEX_NONE ? FBattleLinks() : Order.Slice(Layer, End - Layer);
	}

	/** The drawing of the links given of the frame's order, with the frame's projection. */
	void BuildBattleDraw(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, const FFieldRenderOptions& Options, FBattleLinks Links,
		bool bDepthBuffer, FFieldSceneDraw& Draw, Weltall::FTexelKindCache* TexelKinds)
	{
		ResetBattleDraw(Draw);
		Draw.Background = FLinearColor::Black;
		if (Frame.bCleared)
		{
			const std::array<float, 3> Clear = Blend != nullptr ? Blend->ClearColor
				: std::array<float, 3>{static_cast<float>(Weltall::FilledChannel(Frame.ClearColor[0])), static_cast<float>(Weltall::FilledChannel(Frame.ClearColor[1])),
					static_cast<float>(Weltall::FilledChannel(Frame.ClearColor[2]))};
			Draw.Background = FLinearColor(Clear[0] / 255.0f, Clear[1] / 255.0f, Clear[2] / 255.0f);
		}
		Draw.Filter = Options.Filter;
		Draw.SpriteFilter = Options.SpriteFilter;
		Draw.AntiAliasing = Options.AntiAliasing;
		Draw.bDepthBuffer = bDepthBuffer;
		// The world's up in view space (its y runs down: the view's second column turned the other way), which the shadows'
		// floors face: a gear's shadow is not drawn over its own legs.
		const Weltall::FGteMatrix& View = Frame.View;
		Draw.FloorUp = bDepthBuffer ? -FVector3f(View.M[0][1], View.M[1][1], View.M[2][1]).GetSafeNormal() : FVector3f::ZeroVector;
		const float Distance = FMath::Max(Blend != nullptr ? Blend->ProjectionDistance : static_cast<float>(Frame.ProjectionDistance), 1.0f);
		Draw.ProjectionDistance = Distance;
		// The passes project about the screen's middle: the battle's centre, below it, is reached by moving each point across and
		// down in proportion to its depth.
		const float CentreX = Blend != nullptr ? Blend->CentreX : static_cast<float>(Frame.CentreX);
		const float CentreY = Blend != nullptr ? Blend->CentreY : static_cast<float>(Frame.CentreY);
		const float Across = (CentreX - BattleScreenMiddleX) / Distance;
		const float Lower = (CentreY - BattleScreenMiddleY) / Distance;
		Draw.Objects.Emplace(1.0f, 0.0f, Across, 0.0f);
		Draw.Objects.Emplace(0.0f, 1.0f, Lower, 0.0f);
		Draw.Objects.Emplace(0.0f, 0.0f, 1.0f, 0.0f);
		FBattleDrawBuilder Builder(Frame, Blend, bDepthBuffer && Options.Sorting == EFieldSorting::DepthBufferWithScenery, Draw);
		for (const Weltall::FBattleShowLink& Link : Links)
		{
			if (Link.Kind == Weltall::EBattleShowLink::Quad)
			{
				Builder.AddQuad(Frame.Quads[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::Triangle)
			{
				Builder.AddTriangle(Frame.Triangles[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::Object && Frame.Objects != nullptr)
			{
				Builder.AddObject(Frame.Objects->Links[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::Backdrop)
			{
				Builder.AddBackdrop();
			}
			else if (Link.Kind == Weltall::EBattleShowLink::Tile)
			{
				Builder.AddTile(Frame.Tiles[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::Line)
			{
				Builder.AddLine(Frame.Lines[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::ScreenQuad)
			{
				Builder.AddScreenQuad(Frame.ScreenQuads[Link.Index]);
			}
			else if (Link.Kind == Weltall::EBattleShowLink::ModelPolygon)
			{
				Builder.AddModelPolygon(Frame.ModelPolygons[Link.Index]);
			}
		}
		if (TexelKinds != nullptr)
		{
			FindScenePolygonTexelKinds(Draw, Builder.Items(), *TexelKinds);
		}
		if (Draw.bDepthBuffer)
		{
			AddScenePolygonsUnderDepthRule(Draw, Builder.Items());
		}
		else
		{
			AddScenePolygonsInPaintersOrder(Draw, Builder.Items());
		}
	}

	/** The frame's fade layer, when it links one, as a primitive over the whole screen and past its edges; an effect's flash as the player has them. */
	void BuildBattleFade(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, Weltall::EBattleFlashes Flashes, Weltall::FDrawList& Fade)
	{
		Fade.Primitives.clear();
		const float Share = Weltall::DrawnFadeShare(Frame.Fade, Flashes);
		for (const Weltall::FBattleShowLink& Link : Frame.Order)
		{
			if (Link.Kind != Weltall::EBattleShowLink::Fade)
			{
				continue;
			}
			const auto Channel = [&Frame, Blend, Share](std::size_t At)
			{
				const float Value = Blend != nullptr ? Blend->FadeColor[At] : static_cast<float>(Frame.Fade.Color[At]);
				return static_cast<uint8>(FMath::RoundToInt(Value * Share));
			};
			const Weltall::FDrawColor Color{Channel(0), Channel(1), Channel(2)};
			Weltall::FDrawPrimitive Tile = Weltall::MakeGouraudRect(BattleFadeTile, {Color, Color, Color, Color});
			Tile.bGouraud = false;
			Tile.Blend = Weltall::EDrawBlend::SemiTransparent;
			Tile.UntexturedBlend = static_cast<Weltall::EBlendMode>(Frame.Fade.Mode & 3);
			Fade.Primitives.push_back(Tile);
		}
	}
}

void BuildBattleFrameDraw(const Weltall::FBattleShowFrame& Frame, const Weltall::FBattleFrameBlend* Blend, const FFieldRenderOptions& Options, FBattleFrameDraw& Draw,
	Weltall::FTexelKindCache* TexelKinds)
{
	BuildBattleDraw(Frame, Blend, Options, BattleSceneLinks(Frame), Options.Sorting != EFieldSorting::PaintersOrder, Draw.Scene, TexelKinds);
	BuildBattleFade(Frame, Blend, Options.BattleFlashes, Draw.Fade);
	// A layer has no depth: over it, the table's order alone.
	BuildBattleDraw(Frame, Blend, Options, BattleOverlayLinks(Frame, false), false, Draw.OverFade, TexelKinds);
	BuildBattleDraw(Frame, Blend, Options, BattleOverlayLinks(Frame, true), false, Draw.OverHud, TexelKinds);
}

void AddBattleFramePasses(FRDGBuilder& GraphBuilder, const FFieldSceneMesh& Mesh, const FVramTexture& Vram, const FVramTexture& IconVram,
	const FBattleFrameDraw& Draw, FRDGTextureRef Output, const FScreenArea& Area)
{
	const FScreenArea Rows = Area.Rows(Weltall::MenuScreenSize);
	AddFieldScenePasses(GraphBuilder, Mesh, Vram, Draw.Scene, Output, Rows);
	if (!Draw.Fade.Primitives.empty())
	{
		AddDrawListPasses(GraphBuilder, Vram, FDrawListFrame{&Draw.Fade, nullptr, nullptr, 1.0f}, Output, Rows, Weltall::MenuScreenSize);
	}
	AddFieldSceneOverlayPasses(GraphBuilder, Mesh, Vram, Draw.OverFade, Output, Rows);
	if (!Draw.Hud.Primitives.empty())
	{
		AddIconDrawListPasses(GraphBuilder, Vram, IconVram, Draw.Hud, Output, Area.ScreenRect(Weltall::MenuScreenSize), Weltall::MenuScreenSize);
	}
	AddFieldSceneOverlayPasses(GraphBuilder, Mesh, Vram, Draw.OverHud, Output, Rows);
}
