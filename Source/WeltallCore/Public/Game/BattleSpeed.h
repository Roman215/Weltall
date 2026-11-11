#pragma once

#include "Foundation/Types.h"
#include "Input/PadState.h"

#include <string_view>

namespace Weltall
{
	/** A program's simulation speed: normal, twice or four times the game's pace. */
	enum class EBattleSpeed : uint8
	{
		Normal,
		Double,
		Quadruple,
	};

	inline constexpr int32 BattleSpeedCount = 3;

	/** How many times the game's own pace a speed is. */
	WELTALLCORE_API int32 BattleSpeedRate(EBattleSpeed Speed);

	/** A speed as the screen and the settings' text name it: "2x", "4x"; nothing for the game's own pace. */
	WELTALLCORE_API std::string_view BattleSpeedName(EBattleSpeed Speed);

	/**
	 * The speed after the buttons newly pressed: R1 a step faster, L1 a
	 * step back toward the game's own pace, no further than either end.
	 */
	WELTALLCORE_API EBattleSpeed StepBattleSpeed(EBattleSpeed Speed, FPadState Pressed);
}
