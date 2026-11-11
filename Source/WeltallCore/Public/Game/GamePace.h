#pragma once

#include "Game/BattleSpeed.h"

namespace Weltall
{
	class FGameLoop;
	struct FPlayerSettings;

	/**
	 * Whether a battle is being fought: the Battle program from its opening transition to the last frame of
	 * its ending, before its result's screen.
	 */
	WELTALLCORE_API bool IsBattleFought(const FGameLoop& Loop);

	/** Whether Camera L/R control fast-forward in the field or world map right now. */
	WELTALLCORE_API bool CanFastForwardExploration(const FGameLoop& Loop, const FPlayerSettings& Settings);

	/** The speed of the current game program, or Normal where it runs at its own pace. */
	WELTALLCORE_API EBattleSpeed ActiveGameSpeed(const FGameLoop& Loop, const FPlayerSettings& Settings);

	/** How many times its own pace the game runs at this moment. */
	WELTALLCORE_API int32 GamePace(const FGameLoop& Loop, const FPlayerSettings& Settings);
}
