#pragma once

#include "Input/PadState.h"

namespace Weltall
{
	/**
	 * The buttons each tick sees when the controller is read at every display frame, more often
	 * than the game ticks: those held at the latest frame, and any pressed since the tick before.
	 * A press shorter than a tick still reaches the game, and a release is seen as soon as the
	 * game would see it. The stick is where the latest frame found it.
	 */
	class FPadLatch
	{
	public:
		/** Records the buttons held at a display frame. */
		void Sample(FPadState Held)
		{
			const uint16 New = static_cast<uint16>(Held.Buttons & ~Latest.Buttons);
			Pressed.Buttons |= New;
			Pressed.StickButtons |= New & Held.StickButtons;
			Pressed.ZoomButtons |= New & Held.ZoomButtons;
			Latest = Held;
		}

		/** The buttons for the next tick. */
		FPadState TakeForTick()
		{
			FPadState Taken = Latest;
			Taken.Buttons = static_cast<uint16>(Latest.Buttons | Pressed.Buttons);
			// A direction stays the stick's alone, and a shoulder the zoom's, unless something else held or pressed it.
			Taken.StickButtons = Only(&FPadState::StickButtons);
			Taken.ZoomButtons = Only(&FPadState::ZoomButtons);
			Pressed = {};
			return Taken;
		}

	private:
		/** The buttons the latest frame or a press since held only as Part says. */
		uint16 Only(uint16 FPadState::*Part) const
		{
			const uint16 Other = static_cast<uint16>((Latest.Buttons & ~(Latest.*Part)) | (Pressed.Buttons & ~(Pressed.*Part)));
			return static_cast<uint16>((Latest.*Part | Pressed.*Part) & ~Other);
		}

		FPadState Latest;
		FPadState Pressed;
	};
}
