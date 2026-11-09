#include "FieldScreenPieces.h"

#include "Field/FieldCompass.h"
#include "Field/FieldDrawingTables.h"
#include "Field/FieldRenderState.h"
#include "Field/FieldScreenOverlays.h"
#include "Presentation/DrawList.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr FDrawColor Neutral{128, 128, 128};
		/** The original's 320 x 224 screen, the drawing area its full-screen pieces are clipped to. */
		constexpr FScreenRect Screen{0, 0, 320, 224};

		/** The effects' textures' 4-bit page, which the markers share with the particles, and their CLUT. */
		constexpr FTexturePage EffectsPage{0x3C0, 0x100, ETextureDepth::Indexed4, EBlendMode::Average};
		constexpr FClutPosition MarkerClut{0x100, 0xF7};
		constexpr int32 BigMarker = 16;
		constexpr int32 SmallMarker = 8;

		/** The cockpit's image: a 4-bit page at (896, 0), its CLUT at (0, 232). */
		constexpr FClutPosition HudClut{0, FFieldCockpitHud::ClutY};

		/**
		 * The picture's columns: 64 units of a 15-bit page each, 223 lines, which the field sets up as
		 * quads of half the screen's size about its centre (x -80 to 80 and y -56 to 56 in 32-unit
		 * strips) and projects, when scaled, with a depth of 0: the division overflows and doubles them.
		 */
		constexpr int32 CoverColumns = 5;
		constexpr int32 CoverColumnUnits = 0x40;
		constexpr int32 CoverLines = 0xDF;
		constexpr int32 CoverHalfWidth = 80;
		constexpr int32 CoverHalfHeight = 56;
		constexpr int32 CoverStrip = 32;
		constexpr int32 OverflowedQuotient = 0x1FFFF;
		constexpr int32 ScreenCentreX = 160;
		constexpr int32 ScreenCentreY = 112;
		constexpr int32 ScreenLimit = 1024;

		/** Select's picture: three 128-texel columns of an 8-bit page each from (768, 256), its CLUT at (0, 246). */
		constexpr int32 PictureColumns = 3;
		constexpr int32 PictureColumnTexels = 0x80;
		constexpr int32 PictureLines = 0xDF;
		constexpr FClutPosition PictureClut{0, 0xF6};
		constexpr uint8 PictureMarkerU = 0xE0;
		constexpr uint8 PictureMarkerV = 0x70;

		/** A screen coordinate of the picture's projection: the GTE's, with its overflowed quotient and its saturation. */
		float ProjectCover(int32 Scaled, int32 Centre)
		{
			const int32 Projected = ((Centre << 16) + Scaled * OverflowedQuotient) >> 16;
			return static_cast<float>(std::clamp(Projected, -ScreenLimit, ScreenLimit - 1));
		}

		FDrawColor Grey(uint8 Level)
		{
			return {Level, Level, Level};
		}
	}

	void FieldScreenPieces::AddCockpitHud(const FFieldCockpitHudLayout& Layout, const FFieldCockpitHud& Hud, FDrawList& Layer)
	{
		std::array<FFieldHudQuad, FFieldCockpitHudLayout::PieceCount> Quads;
		BuildCockpitHudQuads(Layout, Hud, Quads);
		for (auto Quad = Quads.rbegin(); Quad != Quads.rend(); ++Quad)
		{
			if (!Quad->bDrawn)
			{
				continue;
			}
			FDrawPrimitive Primitive;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				FDrawVertex& Vertex = Primitive.Corners[Corner];
				Vertex.X = Quad->Corners[Corner][0];
				Vertex.Y = Quad->Corners[Corner][1];
				Vertex.U = Quad->Texels[Corner][0];
				Vertex.V = Quad->Texels[Corner][1];
				Vertex.Color = Neutral;
			}
			const FTexturePage Page{FFieldCockpitHud::ImageX, FFieldCockpitHud::ImageY, ETextureDepth::Indexed4, static_cast<EBlendMode>(Quad->BlendMode & 3)};
			Primitive.Texture = FDrawTexture{0, Page, HudClut};
			Primitive.Blend = EDrawBlend::SemiTransparent;
			Layer.Primitives.push_back(Primitive);
		}
	}

	void FieldScreenPieces::AddCompass(const FFieldCompassFrame& Compass, FDrawList& Layer)
	{
		// The compass's own keys: its quads turn and tip with the camera between ticks.
		constexpr uint32 CompassKeys = 0xC0000;
		for (int32 Index = 0; Index < Compass.Count; ++Index)
		{
			const FFieldCompassQuad& Quad = Compass.Quads[static_cast<std::size_t>(Index)];
			FDrawPrimitive Primitive;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				FDrawVertex& Vertex = Primitive.Corners[Corner];
				Vertex.X = Quad.Corners[Corner][0];
				Vertex.Y = Quad.Corners[Corner][1];
				Vertex.U = Quad.Texels[Corner][0];
				Vertex.V = Quad.Texels[Corner][1];
				Vertex.Color = Neutral;
			}
			const FFieldCompassLayout::FTexture& Texture = Quad.Texture;
			const FTexturePage Page{Texture.PageX, Texture.PageY, Texture.Depth == 0 ? ETextureDepth::Indexed4 : ETextureDepth::Indexed8,
				Quad.bShadow ? EBlendMode::Subtract : EBlendMode::Average};
			Primitive.Texture = FDrawTexture{0, Page, FClutPosition{Texture.ClutX, Texture.ClutY}};
			Primitive.Blend = Quad.bShadow ? EDrawBlend::SemiTransparent : EDrawBlend::Opaque;
			Primitive.bAnchorRight = true;
			Primitive.BlendKey = CompassKeys | static_cast<uint32>(Index);
			Layer.Primitives.push_back(Primitive);
		}
	}

	void FieldScreenPieces::AddHiResStill(FDrawList& Layer)
	{
		constexpr int32 Columns = 5;
		constexpr int32 ColumnPixels = 0x80;
		constexpr int32 ColumnUnits = 0x40;
		constexpr int32 Lines = 0xE0;
		// Each column's sprite is linked after its draw mode, the first first: the last is drawn first.
		for (int32 Column = Columns; Column-- > 0;)
		{
			const FTexturePage Page{static_cast<int16>(FFieldRenderState::HiResStillX + Column * ColumnUnits), 0, ETextureDepth::Indexed8, EBlendMode::Average};
			Layer.Primitives.push_back(MakeSprite(Column * ColumnPixels, 0, ColumnPixels, Lines, {0, Page, FClutPosition{0, FFieldRenderState::HiResStillClutY}}, 0, 0,
				Neutral));
		}
	}

	void FieldScreenPieces::AddPausePicture(const FFieldDrawingTables& Tables, int32 DrawBuffer, FDrawList& Layer)
	{
		// Written into VRAM's buffer on display: the direct 15-bit page around it, drawn as it is.
		constexpr int16 BufferHeight = 0x100;
		constexpr int16 PageUnits = 0x40;
		const FVramRect& Offset = Tables.PauseImage.Rect;
		const int16 X = static_cast<int16>(FFieldDrawingTables::PauseX + Offset.X);
		const int16 Y = static_cast<int16>(FFieldDrawingTables::PauseY + Offset.Y);
		const int16 BufferY = static_cast<int16>(BufferHeight * ((DrawBuffer + 1) & 1));
		const FTexturePage Page{static_cast<int16>(X & ~(PageUnits - 1)), BufferY, ETextureDepth::Direct15, EBlendMode::Average};
		FDrawPrimitive Picture = MakeSprite(X, Y, Offset.Width, Offset.Height, {0, Page, FClutPosition{}}, static_cast<uint8>(X & (PageUnits - 1)),
			static_cast<uint8>(Y), Neutral);
		Picture.bModulate = false;
		Layer.Primitives.push_back(Picture);
	}

	void FieldScreenPieces::AddScreenMarkers(const FFieldScreenMarkers& Markers, FDrawList& Layer)
	{
		for (auto Placed = Markers.Placed.rbegin(); Placed != Markers.Placed.rend(); ++Placed)
		{
			const bool bBig = Placed->Marker == 0;
			const std::array<uint8, 2>& Texels = bBig ? FFieldScreenMarkers::BigTexels : FFieldScreenMarkers::SmallTexels;
			const std::array<uint8, 3>& Color = Markers.Colors[Placed->Marker];
			const int32 Size = bBig ? BigMarker : SmallMarker;
			Layer.Primitives.push_back(MakeSprite(Placed->X, Placed->Y, Size, Size, {0, EffectsPage, MarkerClut}, Texels[0], Texels[1], {Color[0], Color[1], Color[2]}));
		}
	}

	void FieldScreenPieces::AddCoverColumns(const FFieldCover& Cover, uint8 Brightness, FDrawList& Layer)
	{
		const EBlendMode Blend = Cover.bAdditive ? EBlendMode::Add : EBlendMode::Average;
		// Linked one after another at the head of the layer table's first entry: the last column is drawn first.
		for (int32 Column = CoverColumns; Column-- > 0;)
		{
			std::array<float, 2> Left{static_cast<float>(Column * CoverColumnUnits), 0.0f};
			std::array<float, 2> Right{static_cast<float>((Column + 1) * CoverColumnUnits), static_cast<float>(CoverLines)};
			if (Cover.Scale != FFieldCover::UnitScale)
			{
				const auto Scaled = [&Cover](int32 Value) { return Cover.Scale * Value >> 12; };
				Left = {ProjectCover(Scaled(-CoverHalfWidth + CoverStrip * Column), ScreenCentreX), ProjectCover(Scaled(-CoverHalfHeight), ScreenCentreY)};
				Right = {ProjectCover(Scaled(-CoverHalfWidth + CoverStrip * (Column + 1)), ScreenCentreX), ProjectCover(Scaled(CoverHalfHeight), ScreenCentreY)};
			}
			FDrawPrimitive Primitive;
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				const bool bRight = (Corner & 1) != 0;
				const bool bBottom = (Corner & 2) != 0;
				FDrawVertex& Vertex = Primitive.Corners[Corner];
				Vertex.X = bRight ? Right[0] : Left[0];
				Vertex.Y = bBottom ? Right[1] : Left[1];
				Vertex.U = bRight ? static_cast<float>(CoverColumnUnits) : 0.0f;
				Vertex.V = bBottom ? static_cast<float>(CoverLines) : 0.0f;
				Vertex.Color = Grey(Brightness);
			}
			const FTexturePage Page{FFieldCover::PictureArea.X + Column * CoverColumnUnits, FFieldCover::PictureArea.Y, ETextureDepth::Direct15, Blend};
			Primitive.Texture = FDrawTexture{0, Page, {}};
			Primitive.Blend = Cover.bAdditive ? EDrawBlend::SemiTransparentAll : EDrawBlend::Opaque;
			Layer.Primitives.push_back(Primitive);
		}
	}

	void FieldScreenPieces::AddMapPicture(const FFieldMapPicture& Picture, FDrawList& Layer)
	{
		for (int32 Column = PictureColumns; Column-- > 0;)
		{
			const FTexturePage Page{0x300 + Column * 0x40, 0x100, ETextureDepth::Indexed8, EBlendMode::Average};
			FDrawPrimitive Primitive = MakeSprite(Column * PictureColumnTexels, 0, PictureColumnTexels, PictureLines, {0, Page, PictureClut}, 0, 0, Grey(Picture.Brightness));
			Primitive.Blend = EDrawBlend::SemiTransparent;
			Primitive.Clip = Screen;
			Layer.Primitives.push_back(Primitive);
		}
		FDrawPrimitive Marker = MakeSprite(Picture.MarkerX, Picture.MarkerY, BigMarker, BigMarker, {0, EffectsPage, MarkerClut}, PictureMarkerU, PictureMarkerV,
			Grey(Picture.Brightness));
		Marker.Clip = Screen;
		Layer.Primitives.push_back(Marker);
	}
}
