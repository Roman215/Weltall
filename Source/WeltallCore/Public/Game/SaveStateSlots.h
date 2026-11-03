#pragma once

#include "Foundation/Types.h"
#include "Game/SaveState.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Weltall
{
	class FGameLoop;
	class FGameSession;

	/** What a save state's key did: the line told over the game, and for the log why it failed, when it did. */
	struct FSaveStateTold
	{
		std::string Line;
		std::string Why;
	};

	/**
	 * The player's ten save state slots, a file each in their folder (beside the saves; every game's states share them,
	 * as the saves do: a load checks the state fits the game), and the slot chosen for the next save and load: the first
	 * as the game starts, stepped round from the last to the first. Each key's line says what it did.
	 */
	class FSaveStateSlots
	{
	public:
		static constexpr int32 SlotCount = 10;

		WELTALLCORE_API explicit FSaveStateSlots(std::filesystem::path InFolder);

		/** The slot chosen, 1 to SlotCount. */
		int32 Chosen() const { return Slot; }

		/** The next slot chosen (By 1) or the previous (By -1); the line telling it, with its state's play time or that it is empty. */
		WELTALLCORE_API FSaveStateTold Step(int32 By);

		/** The game saved to the chosen slot where the moment is quiet (CaptureSaveState), or nothing done elsewhere. */
		WELTALLCORE_API FSaveStateTold Save(FGameLoop& Loop, FGameSession& Session);

		/** The chosen slot's state; or none, with what to tell. */
		WELTALLCORE_API std::optional<FSaveState> Read(FSaveStateTold& Told) const;

		/** What a load of the chosen slot tells: done; or its disc (0 or 1) not in the game's folder, or its map's scripts not the game's. */
		WELTALLCORE_API std::string Loaded() const;
		WELTALLCORE_API std::string NeedsDisc(int32 Disc) const;
		WELTALLCORE_API std::string OtherFiles() const;

		/** Slot's file (1 to SlotCount). */
		WELTALLCORE_API std::filesystem::path File(int32 InSlot) const;

	private:
		std::filesystem::path Folder;
		int32 Slot = 1;
	};

	/** A play time as a slot's line shows it, hours and minutes as the menus show them: "32:05". */
	WELTALLCORE_API std::string StatePlayTime(uint32 Vblanks);
}
