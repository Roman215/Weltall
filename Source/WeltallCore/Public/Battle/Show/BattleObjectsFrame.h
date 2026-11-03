#pragma once

#include "Battle/Show/BattleObjectParts.h"
#include "Foundation/Types.h"
#include "Gears/GearCloth.h"
#include "Graphics/Gte.h"
#include "Graphics/MdlModel.h"
#include "Graphics/TexturePictures.h"

#include <array>
#include <vector>

namespace Weltall
{
	struct FGearBonePackets;

	/**
	 * An object's floor shadow as each object's making sets it up: a semi-transparent quad of the stage's
	 * shadow colour on a 4-bit page at (380h, 0) that subtracts, its CLUT at (30h, 1CCh), its corners (+x, +z),
	 * (-x, +z), (+x, -z), (-x, -z) of the object's half-sizes taking the texels' first column and row, the
	 * last column, the last row, and both.
	 */
	namespace BattleObjectShadow
	{
		constexpr uint16 TexturePage = 0x4E;
		constexpr uint16 Clut = 0x7303;
		constexpr uint8 Code = 0x2E;
		constexpr uint8 FirstTexel = 0xC0;
		constexpr uint8 LastTexel = 0xFE;
	}

	/**
	 * The trails' quads as their pool sets them up: a quad on a 4-bit page at (380h, 0) that adds, its CLUT at
	 * (0, 1CDh), its corners taking the texels (0, C1h), (0, C1h), (Fh, C1h), (Fh, C1h), semi-transparent as its
	 * emitter asks.
	 */
	namespace BattleTrailQuad
	{
		constexpr uint16 TexturePage = 0x2E;
		constexpr uint16 Clut = 0x7340;
		constexpr uint8 Code = 0x2C;
		constexpr uint8 SemiTransparentCode = 0x2E;
		constexpr uint8 Row = 0xC1;
		constexpr uint8 LastTexel = 0x0F;
	}

	/**
	 * A trail's quad as the frame draws it: its corners on the screen (kind 0, x and y) or in the world
	 * (kind 1, projected through the frame's view), its colour (the pool's 64ths shifted down) and whether
	 * it is added to what lies behind it.
	 */
	struct FBattleTrailQuad
	{
		/** Its quad of the trails' pool. */
		uint16 Segment = 0;
		int16 Kind = 0;
		std::array<FGteVector, 4> Corners{};
		std::array<uint8, 3> Colour{};
		bool bSemiTransparent = false;
	};

	/**
	 * A bone's part as a frame drew it (the stage's among them): model to view space as the GTE drew it, its
	 * model's part and the packets its object built, and the ambient colour it was lit with (none for the stage's);
	 * the slot and the bone name the same part from frame to frame. A dress's kept performer pieces number after the
	 * wearer's bones, and their packets' pages and CLUTs lie in the port's rows (FTexturePage::FromPortAttribute).
	 */
	struct FBattleDrawnPart
	{
		FGteMatrix Matrix;
		const FMdlPart* Part = nullptr;
		const FGearBonePackets* Packets = nullptr;
		uint8 Slot = 0;
		uint16 Bone = 0;
		std::array<uint8, 3> Ambient{};
		bool bPortRows = false;
	};

	/**
	 * A polygon of a drawn part as it was linked: its part (FBattleObjectsFrame::Parts) and its number there,
	 * its colours this frame (one per corner for the Gouraud kinds) and, for the environment-mapped kind, the
	 * texels its normals gave it.
	 */
	struct FBattleDrawnPolygon
	{
		uint32 Part = 0;
		uint16 Number = 0;
		std::array<FMdlColor, 4> Colors{};
		std::array<FMdlUv, 3> Texels{};
	};

	/** An object's floor shadow as it was linked: its quad's matrix into view space and corners, and its colour. */
	struct FBattleDrawnShadow
	{
		FGteMatrix Matrix;
		std::array<FGteVector, 4> Corners{};
		FMdlColor Colour;
		uint8 Slot = 0;
	};

	/** What a link of the objects draws. */
	enum class EBattleObjectDraw : uint8
	{
		/** A polygon of a drawn part (FBattleObjectsFrame::Polygons). */
		Polygon,
		/** An object's floor shadow (Shadows). */
		Shadow,
		/** A cloth's triangle (Cloths), its points in the world. */
		Cloth,
		/** A trail's quad (Trails). */
		Trail,
	};

	struct FBattleObjectLink
	{
		EBattleObjectDraw Kind = EBattleObjectDraw::Polygon;
		uint32 Index = 0;
		/** The ordering table's entry it was linked into. */
		int32 Entry = 0;
	};

	/**
	 * The stage's and the 3D objects' part of the show's frame (FBattleShowFrame::Objects): what they linked
	 * into the ordering table in the order they linked it (the stage's model first, then each object's shadow,
	 * shown parts and cloths in slot order, then the trails), with what each link draws, and the point lights.
	 * Rebuilt each frame into the same memory.
	 */
	struct FBattleObjectsFrame
	{
		std::vector<FBattleDrawnPart> Parts;
		std::vector<FBattleDrawnPolygon> Polygons;
		std::vector<FBattleDrawnShadow> Shadows;
		std::vector<FGearDrawnClothTriangle> Cloths;
		/** The trails' quads drawn this frame, in the pool's order. */
		std::vector<FBattleTrailQuad> Trails;
		std::vector<FBattleObjectLink> Links;
		/** The view the cloths' points and the trails' world corners go through. */
		FGteMatrix View;
		/** The texture regions of the objects' models with their loads' colours, each object's once (FBattleObject::TextureSeeds). */
		std::vector<FTexturePictureSeed> TextureSeeds;
		std::array<FBattlePointLight, 2> PointLights{};
	};
}
