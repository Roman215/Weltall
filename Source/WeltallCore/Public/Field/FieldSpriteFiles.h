#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/SpriteFile.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * A map's sprite files (its archive's section 3): a u32 count and u32 offsets from the section's
	 * start. The field reads each file from its offset on; its bytes run to the next file's start or
	 * the section's end.
	 */
	WELTALLCORE_API TResult<std::vector<FSpriteFile>> ParseFieldSpriteFiles(std::span<const uint8> Section);

	/** Sprite file File's bytes within its section, as ParseFieldSpriteFiles reads them. */
	WELTALLCORE_API TResult<std::span<const uint8>> FieldSpriteFileBytes(std::span<const uint8> Section, uint32 File);

	/** The section again with file File's bytes replaced by Bytes, the files in their order and each at its offset. */
	WELTALLCORE_API TResult<std::vector<uint8>> ReplaceFieldSpriteFile(std::span<const uint8> Section, uint32 File, std::span<const uint8> Bytes);

	/**
	 * A sprite file the field streams from the disc, as the game names it (directory, number): the
	 * party's field and gear sprites (directory 4, files 5 on, LZSS-packed) and the extra animation
	 * sets (files 0x77A on, stored as they are).
	 */
	WELTALLCORE_API TResult<FSpriteFile> ReadSpriteFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number);
}
