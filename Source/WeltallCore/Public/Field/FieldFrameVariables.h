#pragma once

#include "Field/EventContext.h"
#include "Foundation/Result.h"
#include "Timing/PlayClock.h"

#include <optional>

namespace Weltall
{
	/**
	 * What the field writes every tick before its scripts run: the latched buttons take the held ones;
	 * the game state takes the party and the field's place (the map, the camera's yaw and the entrance
	 * from the variables it saved the tick before, the song); the variables that report the last
	 * program (0x044 the battle's acting turns, 0x046 its result), the leader's and the camera's
	 * directions (0x006, 0x008), the camera's pitch (0x024), the map (0x03C) and the party (0x03E-0x042)
	 * are refreshed, and the game state takes the story's variables; the field clock steps, each party
	 * slot riding its gear records where its pilot is (map, layer, x, z), and the variables that report
	 * the play clock (0x00C minutes and seconds, 0x00E hours) and the leader's whole position (0x01E x,
	 * 0x020 z, 0x022 y) are refreshed. After a game's load, or once the scripts leave for the world map,
	 * it does nothing. An error when the leader, or a riding slot's actor, has no script state.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> UpdateFieldFrameVariables(FEventContext& Context, const FPlayClock& Clock);

	/**
	 * The part of it that saves the field: the game state takes the field's place and song, the
	 * variables that report the game around the scripts are refreshed, and the game state takes the
	 * story's variables. Leader is the leader's script state.
	 */
	WELTALLCORE_API void SaveFieldVariables(FEventContext& Context, const FEventActorState& Leader);

	/** The same into Variables and Game, the context's copies, leaving the field as it is. */
	WELTALLCORE_API void SaveFieldVariables(const FEventContext& Context, const FEventActorState& Leader, FEventVariables& Variables, FGameState& Game);
}
