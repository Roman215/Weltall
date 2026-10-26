#pragma once

#include "Menu/MenuSettingsPages.h"

namespace Weltall::SoundPage
{
	/** The page's rows: its subtitles' source only where a disc or mod gives subtitles. */
	int32 RowCount(const FSoundSources& Sources);

	/**
	 * A row as Settings has it; a choice of voices or subtitles the sources leave no room for shows what there is, and
	 * why (bDiscsPicked: the discs that would give it are added on the Saves page, not put in a folder).
	 */
	FSettingsRow Row(int32 Row, const FPlayerSettings& Settings, const FSoundSources& Sources, bool bDiscsPicked = false);

	/** Left (-1) or Right (+1) on a row; false where nothing changed. */
	bool Step(int32 Row, int32 Direction, FPlayerSettings& Settings, const FSoundSources& Sources);
}
