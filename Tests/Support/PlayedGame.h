#pragma once

#include "Foundation/Result.h"
#include "Game/GameSession.h"
#include "Game/OriginalSave.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/Iso9660.h"
#include "Media/PsxExecutable.h"
#include "Sound/GameSoundSession.h"
#include "Sound/UnheardGameSound.h"

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

namespace Weltall::Testing
{
	/** A disc's program and files. */
	struct FPlayedDisc
	{
		const FIso9660* Iso = nullptr;
		FDiscIdentity Identity{EDiscRelease::UsaDisc1, 1, ""};
		std::unique_ptr<FPsxExecutable> Executable;
		std::unique_ptr<FGameFileIndex> Index;
	};

	/** Both discs, and the game's sound, which every session of a run shares. */
	struct FPlayedGame
	{
		std::array<FPlayedDisc, 2> Discs;
		std::unique_ptr<FGameSoundSession> Sound;
		std::unique_ptr<FUnheardGameSound> Unheard;

		/** The discs of the library's folder, their programs and the game's sound: an error when disc 1 is missing. */
		static TResult<FPlayedGame> Open(const FGameLibrary& Library);
		/** The same from the discs themselves, the second or both null where they are missing. */
		static TResult<FPlayedGame> Open(const FIso9660* First, const FIso9660* Second);

		/**
		 * A session powered on with disc Number in the drive and the other beside it, with the game's framing and past the
		 * cases no script reaches, as a player's build; with a save, the save loaded. It plays the run's sound unless given
		 * a sound of its own (a battle left mid-fight keeps its banks registered).
		 */
		TResult<std::unique_ptr<FGameSession>> Start(int32 Number, const FOriginalSave* Save, FGameSoundSession* OwnSound = nullptr) const;
	};

	/**
	 * Every save of every memory card image in the folder and the folders below it, and every save file of the port's
	 * own (a .sav: one save's game data), in the files' order.
	 */
	std::vector<FOriginalSave> ReadCardFolderSaves(const std::filesystem::path& Folder);
}
