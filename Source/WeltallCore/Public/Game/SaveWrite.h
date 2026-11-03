#pragma once

#include "Foundation/Types.h"
#include "Game/OriginalSave.h"

namespace Weltall
{
	class FGameState;
	class FMesNameTables;
	struct FWayBackPoint;

	/**
	 * A save made from the state as the Menu's Save makes it: the summary the File screen shows (the
	 * party the menu shows with their HP, EP and levels, the play time, the card slot), the state with
	 * its names stored as glyph indices and each gear's record in part, and the checksum. As in the
	 * game, the state first takes the battle scripts' globals and the disc to continue on (Disc, 0 for
	 * the first), which the save then keeps, and its names become the ones the save reads back.
	 * LeadByte is the message font's (FMesFont::LeadByte). A game whose second disc's story mode is chosen
	 * keeps it in bytes the game leaves zero and never reads, so the game itself loads the save as any.
	 * With WayBack (a game of the expanded second disc where the original stands elsewhere: FStoryWayBack), the save
	 * is the original's at that point, and the bytes of the game's own place and story kept in the same unread bytes
	 * go back over it when the port loads it (FOriginalSave::PlayedData). A clear game's save is likewise the original's
	 * with Elly away (ClearFile::HideElly), her return kept beside it.
	 */
	WELTALLCORE_API FOriginalSave WriteOriginalSave(FGameState& State, int32 CardSlot, int32 Disc, const FMesNameTables& NameTables, uint8 LeadByte,
		const FWayBackPoint* WayBack = nullptr);
}
