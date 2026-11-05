#pragma once

#include "Foundation/Result.h"

#include <optional>

namespace Weltall
{
	struct FEventContext;
	class FGameState;
	class FPartySpriteFiles;

	/** Refreshes a party changed in the player's menu without restarting the map or its scripts. */
	WELTALLCORE_API std::optional<FError> RefreshFieldParty(FEventContext& Context, FPartySpriteFiles& Files, const FGameState& Game);
}
