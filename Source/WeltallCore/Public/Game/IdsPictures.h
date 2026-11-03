#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FTim;

	/**
	 * Fei drawn as Id (FCharacterLooks). The game has no menu portrait, small face or battle panel portrait of Id, only his
	 * dialogue face, so his are made from it as the party's look: mirrored, since the party's portraits face the other way
	 * from their dialogue faces, resampled and given back the face's own colours, each with the semi-transparency bit the
	 * party's have. The large portrait shows the whole face; the small face his head, inside the opaque black line of the
	 * party's small faces.
	 */
	namespace IdsPictures
	{
		/** His dialogue face: directory 4's file 99 on both discs, a 64 × 64 8-bit TIM. */
		WELTALLCORE_API TResult<std::vector<uint8>> ReadFace(const FCdImage& Image, const FGameFileIndex& Index);

		/** The menus' large portrait (48 × 48 at 8 bits) made from the face; an error when Face is no 64 × 64 8-bit TIM. */
		WELTALLCORE_API TResult<FTim> Portrait(std::span<const uint8> Face);

		/** The small face of the menus, the battle's panels and the results screen (24 × 24 at 8 bits), made so. */
		WELTALLCORE_API TResult<FTim> SmallFace(std::span<const uint8> Face);

		/** "Id" in the message encoding, ended by 0: his name wherever the menus type the party's. */
		WELTALLCORE_API std::span<const uint8> Name();
	}
}
