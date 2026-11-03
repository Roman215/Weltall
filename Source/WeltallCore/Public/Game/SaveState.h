#pragma once

#include "Foundation/Result.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Timing/PlayClock.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FGameLoop;
	class FGameSession;

	/**
	 * The game at a quiet moment, as a load brings it back: the game state, the kernel's globals, the game's random
	 * numbers and the play clock. On the field the kernel holds the field's state as the field saves itself for a battle,
	 * and the state's load enters the map as the field comes back from one: the map's song started again, its first
	 * actor's return script run, the entry's fade. On the world map the game state keeps the party's place as the menu's
	 * save keeps it, and the load starts the world map as a save made in its menu starts it. Nothing else differs from
	 * going on unbroken.
	 */
	struct FSaveState
	{
		/** The version of the file's format (WriteSaveState): a file of another is refused, never half read. */
		static constexpr uint32 FormatVersion = 2;

		FGameState Game;
		FKernelState Kernel;
		FPsxRandom Random;
		FPlayClock Clock;
	};

	/**
	 * The game at this moment, when it is a quiet one: on the field where the menu's button would open the Normal Menu
	 * (the party the player's, nothing on screen asked of them, the leader on the ground), on the world map where it would
	 * open the menu (on foot or in gears); none at any other moment. An error when the field cannot save itself.
	 */
	[[nodiscard]] WELTALLCORE_API TResult<std::optional<FSaveState>> CaptureSaveState(FGameLoop& Loop, FGameSession& Session);

	/** The state as a file keeps it: a mark, the format's version, the CRC-32 of the rest, then every member (SaveStateMembers.h). */
	WELTALLCORE_API std::vector<uint8> WriteSaveState(const FSaveState& State);

	/** Why a file holds no state the game can load. */
	enum class ESaveStateFault : uint8
	{
		/** Not a state's file. */
		NotAState,
		/** A state of another format's version. */
		OtherVersion,
		/** A state's file that is cut short or changed since it was written. */
		Damaged,
	};

	/** The state a file holds, or why it holds none. */
	WELTALLCORE_API std::optional<FSaveState> ReadSaveState(std::span<const uint8> Bytes, ESaveStateFault& Fault);

	/**
	 * A freshly powered-on session taking State, and the loop going on with the program its kernel names next: the disc
	 * it was saved on in the drive, its chapter's card not shown again. An error when the session lacks that disc, or
	 * when its map cannot be entered over it (a map whose scripts are not the ones it was saved with: another release
	 * of the game, a translation or a mod); the session then holds the state, and nothing runs it.
	 */
	[[nodiscard]] WELTALLCORE_API TResult<std::unique_ptr<FGameLoop>> LoadSaveState(FGameSession& Session, const FSaveState& State);
}
