#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	class FSpriteFile;
	struct FTim;

	/**
	 * Elly's purple hair (FCharacterLooks): her own pictures with their orange (her hair, and her trousers, which share its
	 * shades) turned to the purple of Merkava's purple-haired Elly, her paler skin kept. The game's purple-haired Elly is a
	 * cutscene sprite in another uniform; her shades stand to Elly's orange as 0.62 of its saturation and value at the
	 * purple's hue.
	 */
	namespace EllysHair
	{
		/**
		 * A 15-bit colour with her orange, and the dark red-brown that shades her trousers, turned purple (its
		 * semi-transparency bit kept); any other colour as it is.
		 */
		WELTALLCORE_API uint16 Purple(uint16 Colour);

		/** A sprite file's palettes (every set, in the file's bytes) recoloured. */
		WELTALLCORE_API void RecolourSpriteFile(std::span<uint8> File);
		WELTALLCORE_API void RecolourSpriteFile(FSpriteFile& File);

		/**
		 * An indexed picture (Depth 4 or 8 bits a pixel, rows of Width pixels) and its CLUT recoloured as a painted portrait
		 * needs: her hair's pixels purple, with the paler orange along its edge, but not the shading of her skin below the
		 * cheeks. That is a light orange with skin, or pale shading hanging from skin, one or two rows above it. The hair's
		 * strands beside the cheek are darker, and its pale edge against the panel behind her hangs from hair, not skin.
		 * Each CLUT entry takes the colour most of its pixels want; a pixel that wants another is given the entry whose
		 * colour is nearest.
		 */
		WELTALLCORE_API void RecolourPicture(std::span<uint8> Pixels, int32 Width, int32 Height, int32 Depth, std::span<uint16> Clut);

		/** A portrait's TIM (4- or 8-bit, with its CLUT) recoloured so; any other as it is. */
		WELTALLCORE_API void RecolourTim(FTim& Tim);
	}
}
