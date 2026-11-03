#include "Text/MesReading.h"

#include "Text/MesWindow.h"

namespace Weltall
{
	namespace
	{
		/** A pace's pause before any glyph and its time for each glyph, in vertical blanks (60 a second). */
		struct FReadingPace
		{
			int32 Pause = 0;
			int32 PerGlyph = 0;
		};

		FReadingPace PaceOf(EMesAutoAdvance Pace)
		{
			switch (Pace)
			{
			case EMesAutoAdvance::Slow:
				return {120, 5};
			case EMesAutoAdvance::Normal:
				return {90, 4};
			case EMesAutoAdvance::Fast:
				return {60, 3};
			case EMesAutoAdvance::Off:
				break;
			}
			return {};
		}
	}

	int32 MesReadingVblanks(EMesAutoAdvance Pace, int32 Glyphs)
	{
		const FReadingPace Reading = PaceOf(Pace);
		return Reading.Pause + Reading.PerGlyph * (Glyphs > 0 ? Glyphs : 0);
	}

	int32 MesShownGlyphs(const FMesWindow& Window)
	{
		int32 Glyphs = 0;
		for (int32 Row = 0; Row < Window.Lines(); ++Row)
		{
			Glyphs += static_cast<int32>(Window.Row(Row).Glyphs.size());
		}
		return Glyphs;
	}

	bool FMesReading::Tick(EMesAutoAdvance Pace, bool bWaits, int32 Glyphs, int32 Vblanks)
	{
		if (Pace == EMesAutoAdvance::Off || !bWaits)
		{
			WaitedVblanks = 0;
			return false;
		}
		WaitedVblanks += Vblanks > 0 ? Vblanks : 1;
		if (WaitedVblanks < MesReadingVblanks(Pace, Glyphs))
		{
			return false;
		}
		WaitedVblanks = 0;
		return true;
	}
}
