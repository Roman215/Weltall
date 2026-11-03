#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	class FMesWindow;

	/** The player's auto-advancing text: off, or how long a reader takes over a page that waits for the button. */
	enum class EMesAutoAdvance : uint8
	{
		Off,
		Slow,
		Normal,
		Fast,
	};

	/** The vertical blanks a page of Glyphs glyphs waits at Pace before it goes on by itself: a pause, then a time for each glyph. */
	WELTALLCORE_API int32 MesReadingVblanks(EMesAutoAdvance Pace, int32 Glyphs);

	/** The glyphs a window shows on its rows, as the latest tick drew them. */
	WELTALLCORE_API int32 MesShownGlyphs(const FMesWindow& Window);

	/**
	 * Auto-advancing text: the time a reader has spent on the page that waits for the button, counted in vertical blanks
	 * so that it lasts as long in a mode of 30 ticks a second as in one of 60. The count starts again whenever no page
	 * waits, so only a page that waits the whole time goes on.
	 */
	class FMesReading
	{
	public:
		/**
		 * One tick of Vblanks: true on the tick the page that waits (bWaits) of Glyphs glyphs has been read at Pace, when
		 * the mode presses the button for the player; the count then starts again.
		 */
		WELTALLCORE_API bool Tick(EMesAutoAdvance Pace, bool bWaits, int32 Glyphs, int32 Vblanks);

		int32 Waited() const { return WaitedVblanks; }

	private:
		int32 WaitedVblanks = 0;
	};
}
