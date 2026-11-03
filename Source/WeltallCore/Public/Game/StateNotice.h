#pragma once

#include "Foundation/Types.h"
#include "Game/ScreenLineDrawing.h"

#include <string>

namespace Weltall
{
	class FMesFont;

	/**
	 * What a save state's key told, drawn over the game in its message font at the screen's top left: shown at once,
	 * held a while, then faded, counted in real time (a battle fought fast or a pause changes nothing). A new line
	 * replaces the one shown.
	 */
	class FStateNotice
	{
	public:
		static constexpr double HoldSeconds = 2.0;
		static constexpr double FadeSeconds = 0.5;

		void Show(std::string InLine)
		{
			Line = std::move(InLine);
			Age = 0.0;
		}

		void Advance(double Seconds) { Age += Seconds; }

		/** Whether a line is still to be seen. */
		bool IsShown() const { return !Line.empty() && Age < HoldSeconds + FadeSeconds; }

		/** How strongly the line is drawn, 255 while held, down to 0 as it fades (white until half gone, as the chapter cards' lines). */
		WELTALLCORE_API int32 Strength() const;

		/** The line at its strength as a list over the 320 x 224 game screen, its textures in Pictures(); nothing once it is gone. */
		WELTALLCORE_API void Build(const FMesFont& Font, FDrawList& Out);

		const FVram& Pictures() const { return Words.Pictures(); }

	private:
		std::string Line;
		double Age = HoldSeconds + FadeSeconds;
		FScreenLineDrawing Words;
	};
}
