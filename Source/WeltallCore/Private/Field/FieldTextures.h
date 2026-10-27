#pragma once

#include "Field/FieldArchive.h"
#include "Foundation/Result.h"
#include "Graphics/Vram.h"

#include <vector>

namespace Weltall
{
	/**
	 * The uploads the field makes from the archive while the map loads: section 0's TIMs, then the
	 * images of section 4 for each sprite slot that takes them from the archive, the slot's images
	 * side by side 64 units apart.
	 */
	TResult<std::vector<FVramImage>> ReadFieldArchiveImages(const FFieldArchive& Archive);

	/**
	 * The images of section 4 for sprite slot Slot of Archive, placed as for slot At: what another map's slot
	 * takes from it. An error where the slot's images are not in the archive, or the section lacks the slot.
	 */
	TResult<std::vector<FVramImage>> ReadFieldSpriteSlotImages(const FFieldArchive& Archive, int32 Slot, const FFieldSpriteSlot& At);
}
