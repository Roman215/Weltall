#pragma once

#include "Foundation/Types.h"
#include "Graphics/TextureRegion.h"
#include "Graphics/Vram.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * What Beetle PSX HW's texture replacements name a picture by: the CRC-32 of one upload's units (FVramUpload::Crc) and
	 * of the palette it is drawn through, its file "<upload>-<palette>" in lower-case hex without leading zeros.
	 */
	struct FBeetleTextureName
	{
		uint32 Upload = 0;
		uint32 Palette = 0;

		auto operator<=>(const FBeetleTextureName&) const = default;
	};

	WELTALLCORE_API std::string BeetleTextureFileName(const FBeetleTextureName& Name);

	/** The name a file's base name gives, in either case: two groups of 1 to 8 hex digits joined by '-'. */
	WELTALLCORE_API std::optional<FBeetleTextureName> BeetleTextureNameOf(std::string_view BaseName);

	/** Part of an upload under the picture a region shows, with what Beetle PSX HW's packs name it by and where it lies. */
	struct FBeetleTexturePart
	{
		uint32 Upload = 0;
		/** The CRC-32 of the view's CLUT as VRAM holds it, 16 or 256 colours; 0 for a direct page. */
		uint32 Palette = 0;
		/**
		 * The CRC-32 of the CLUT from the least index the upload's units name to the greatest, at the view's depth: what a
		 * pack made with Beetle's Reduce Palette Range option names it by. Palette for a direct page.
		 */
		uint32 ReducedPalette = 0;
		/** The upload's size in VRAM units: Beetle scales its picture by the picture's size over these. */
		int32 UploadUnitsWide = 0;
		int32 UploadUnitsHigh = 0;
		/** Where the part begins in its upload, in the view's texels. */
		int32 FromU = 0;
		int32 FromV = 0;
		/** Where it lies in the region's picture, counted from the picture's first texel, and its size, in texels. */
		int32 U = 0;
		int32 V = 0;
		int32 Width = 0;
		int32 Height = 0;
		/** What its upload holds. */
		EVramUploadKind Kind = EVramUploadKind::Other;

		bool operator==(const FBeetleTexturePart&) const = default;
	};

	/** The parts of uploads under the picture a region shows (FTextureRegion::PictureFirstU and on), as VRAM keeps them. */
	WELTALLCORE_API std::vector<FBeetleTexturePart> BeetleTextureParts(const FVram& Vram, const FTextureRegion& Region);
}
