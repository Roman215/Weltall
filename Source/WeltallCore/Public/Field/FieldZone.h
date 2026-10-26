#pragma once

#include "Graphics/Gte.h"

#include <array>

namespace Weltall
{
	/**
	 * A quadrilateral on the ground that scripts test actors against (exits, doors, area events): a
	 * point is inside when it lies on the inner side of all four edges, seen from above. The corners'
	 * heights are not tested.
	 */
	struct FFieldZone
	{
		std::array<FGteVector, 4> Corners{};
	};
}
