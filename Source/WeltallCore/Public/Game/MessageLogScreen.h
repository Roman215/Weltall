#pragma once

#include "Foundation/Types.h"
#include "Graphics/Vram.h"
#include "Input/ButtonGlyphs.h"
#include "Input/PadState.h"
#include "Presentation/DrawList.h"
#include "Text/MesButtonCells.h"
#include "Text/MessageLog.h"

#include <array>
#include <deque>
#include <span>
#include <vector>

namespace Weltall
{
	class FMesFont;

	/**
	 * The message log open over the game (the port's own screen): the messages kept, the newest at the bottom, in the
	 * game's message font and colours over the picture darkened as the game's windows darken it, a speaker's name tinted
	 * and a choice's line in its window's colour; a line saying so while nothing is kept. Up and Down scroll a line
	 * (repeating while held), L1 and R1 or Left and Right a page; Confirm or Cancel closes it. The text glides to where it
	 * is scrolled.
	 */
	class FMessageLogScreen
	{
	public:
		static constexpr int32 LineSpacing = 14;
		/** The space between two messages. */
		static constexpr int32 MessageGap = 7;
		/** The text's area on the 320 x 224 screen. */
		static constexpr FScreenRect TextArea{16, 12, 288, 200};
		static constexpr double RepeatDelaySeconds = 0.35;
		static constexpr double RepeatSeconds = 0.05;

		/**
		 * Opened at Log's newest message (which must outlive it, unchanged while it is open), its text in Font, read by the
		 * kernel's two palettes for even and odd lines (TextClut, 16 colours each), over the picture less WindowColour as
		 * the game's windows take it from what lies behind them.
		 */
		WELTALLCORE_API FMessageLogScreen(const FMessageLog& Log, const FMesFont& Font, std::span<const uint16, 32> TextClut, std::array<uint8, 3> WindowColour);

		/** A display frame of Seconds with Held down (the buttons held as it opened count only once let go); false once the player has closed it. */
		WELTALLCORE_API bool Update(double Seconds, FPadState Held);

		/** Scrolled by Lines toward the older messages (the newer when negative), as a mouse wheel turns it. */
		WELTALLCORE_API void Scroll(int32 Lines);

		/** Whether the newest message's last line is at the bottom: where it opens, and where nothing is left to scroll down to. */
		WELTALLCORE_API bool IsAtNewest() const;

		/** As it shows now over the 320 x 224 game screen: its primitives in Out, the buttons its lines show in Cells, its textures in Pictures(). */
		WELTALLCORE_API void Build(FDrawList& Out, std::vector<FTextButtonCell>& Cells);

		const FVram& Pictures() const { return PictureVram; }

	private:
		/** A row of the log from its top: a message's line, or the gap after a message (Line -1). */
		struct FRow
		{
			int32 Message = 0;
			int32 Line = -1;
			int32 Top = 0;
			int32 Height = 0;
		};

		/** How far the log is scrolled up from its end, in pixels, at most ContentHeight less the area's. */
		int32 MostScroll() const;
		/** The scroll that shows a whole line more above, or below, the view the scroll Scrolled shows. */
		int32 LineUp(int32 Scrolled) const;
		int32 LineDown(int32 Scrolled) const;
		/** The rows' y on the screen at the shown scroll, the first row's top. */
		float ContentTop() const;
		/** Each row on the screen typed into its slot of the pictures, when other rows than the last typed are shown. */
		void TypeRows(std::span<const int32> Shown);
		bool IsSpeaker(int32 Message, int32 Line) const;

		const FMesFont& Font;
		/** The log's messages, or the line telling that it has none. */
		std::deque<FLoggedMessage> Placeholder;
		const std::deque<FLoggedMessage>* Messages = nullptr;
		std::array<uint8, 3> WindowColour;
		std::vector<FRow> Rows;
		int32 ContentHeight = 0;
		int32 Target = 0;
		float ShownScroll = 0.0f;
		uint16 Previous = 0;
		bool bFirstFrame = true;
		/** The direction held that repeats (-1 up, 1 down, 0 none) and how long until it next does. */
		int32 Repeating = 0;
		double RepeatIn = 0.0;
		FVram PictureVram;
		std::vector<uint16> Image;
		std::vector<int32> Typed;
		FMesButtonCells ButtonFinder;
	};
}
