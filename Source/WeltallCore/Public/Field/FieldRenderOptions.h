#pragma once

#include "Field/FieldSceneFrame.h"
#include "Foundation/Types.h"

namespace Weltall
{
	/** How the scenery's polygons are ordered on screen. */
	enum class EFieldSorting : uint8
	{
		/** The original's painter's order by its ordering-table buckets, background first. */
		PaintersOrder,
		/**
		 * The characters' sprites, each at the depth of the entry the field sorts it in, and the 3D models'
		 * polygons sorted against everything by a depth buffer; the scenery among itself in the painter's order,
		 * which keeps its coplanar decals where the original draws them.
		 */
		DepthBuffer,
		/**
		 * As DepthBuffer, and the field's scenery sorted by depth too: the painter's order decides only between
		 * what lies within a decal's margin of a surface, so a farther piece no longer covers a nearer one. The default.
		 */
		DepthBufferWithScenery,
	};

	enum class EFieldTextureFilter : uint8
	{
		/** Each pixel takes the texel under it, as the GPU does. */
		Nearest,
		/**
		 * The texel under it but where a texel's edge crosses the pixel, which takes both sides by their share: as sharp,
		 * without rows and columns doubling and dropping as the picture moves by less than a pixel.
		 */
		Crisp,
		/** Bilinear among the texels a pass draws, so transparent texels leave clean edges. */
		Bilinear,
		/**
		 * The texels upscaled by the port's own rule, each new texel one of the indices about it (TextureUpscale.h),
		 * taken as Crisp takes texels. A 15-bit page's texels are drawn as Crisp draws them.
		 */
		Upscaled,
	};

	/** Whether a scene with these filters draws upscaled texels, which its VRAM must then keep. */
	constexpr bool UpscalesTextures(EFieldTextureFilter Filter, EFieldTextureFilter SpriteFilter)
	{
		return Filter == EFieldTextureFilter::Upscaled || SpriteFilter == EFieldTextureFilter::Upscaled;
	}

	/** How a movie's picture is brought to the display. */
	enum class EMovieFilter : uint8
	{
		/** A Catmull-Rom of the luma and a bilinear chroma, as decoded. */
		Smooth,
		/** The compression's blocking and ringing taken out, then upscaled along the picture's edges (MovieUpscale.h). */
		Upscaled,
	};

	enum class EFieldAntiAliasing : uint8
	{
		None,
		/** Four samples per pixel for polygon edges. */
		Msaa,
		/** Drawn at twice the size each way and averaged down: polygon and texel edges alike. */
		Supersampling,
	};

	enum class EFieldGearModels : uint8
	{
		/** Their sprites, as the original draws them. */
		Off,
		/** Their models while the player plays; their sprites in the story's cutscenes (FFieldCutscenes). */
		OutsideCutscenes,
		/** Their models throughout. */
		Everywhere,
	};

	/** How the battle effects' flashes, their adding screen fades, are drawn: as made, at a quarter of their brightness, or not at all. */
	enum class EBattleFlashes : uint8
	{
		Original,
		Softer,
		Off,
	};

	enum class ECompassPlacement : uint8
	{
		Standard,
		Wide,
		Ultrawide,
		Screen,
		Off,
	};

	/**
	 * The ways the field can be presented, each the player's choice. The defaults sort everything by depth (the
	 * scenery's decals in the original's order), keep the texels sharp, smooth only polygon edges, draw
	 * everything a wider view shows, and draw the party's pilots with the members' own sprites.
	 */
	struct FFieldRenderOptions
	{
		EFieldSorting Sorting = EFieldSorting::DepthBufferWithScenery;
		EFieldTextureFilter Filter = EFieldTextureFilter::Nearest;
		/** How the characters' sprites are filtered: their pixel art stays sharp by default. */
		EFieldTextureFilter SpriteFilter = EFieldTextureFilter::Nearest;
		EFieldAntiAliasing AntiAliasing = EFieldAntiAliasing::Msaa;
		EFieldCulling Culling = EFieldCulling::InRange;
		EMovieFilter MovieFilter = EMovieFilter::Smooth;
		/**
		 * While the party is in gears, its members on foot drawn with their own sprites at the scale of the map's people
		 * (FFieldPilotStandIns), rather than with the map's small pilot sprites.
		 */
		bool bDetailedPilots = true;
		/**
		 * Where the gears the field draws as sprites are drawn with their battle models instead (FFieldGearStandIns), and the
		 * party's gears on the world map with them (FWorldMapPresenter::SetGearStandIns).
		 */
		EFieldGearModels GearModels = EFieldGearModels::OutsideCutscenes;
		EBattleFlashes BattleFlashes = EBattleFlashes::Original;
		ECompassPlacement Compass = ECompassPlacement::Wide;

		bool operator==(const FFieldRenderOptions&) const = default;
	};
}
