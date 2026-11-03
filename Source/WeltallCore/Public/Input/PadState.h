#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** The buttons of a PlayStation controller, numbered by their bit in the pad's report. */
	enum class EPadButton : uint8
	{
		Select = 0,
		L3 = 1,
		R3 = 2,
		Start = 3,
		Up = 4,
		Right = 5,
		Down = 6,
		Left = 7,
		L2 = 8,
		R2 = 9,
		L1 = 10,
		R1 = 11,
		Triangle = 12,
		Circle = 13,
		Cross = 14,
		Square = 15,
	};

	/**
	 * The buttons held on one tick. The game reads the original controller, so the port's input
	 * layer turns whatever the player uses into this.
	 */
	struct FPadState
	{
		/** The stick's report at rest. */
		static constexpr uint8 StickCentre = 0x80;

		uint16 Buttons = 0;
		/** The directions among Buttons that only the stick holds: the port's bindings let a stick press them, which a DualShock's never does. */
		uint16 StickButtons = 0;
		/** The stick as a DualShock in analog mode reports it: 0 at its left (top) to 0xFF at its right (bottom). */
		uint8 StickX = StickCentre;
		uint8 StickY = StickCentre;
		/** A gamepad, which the game is told is a DualShock in analog mode. */
		bool bAnalog = false;
		/** L1 and R1 among Buttons where the player holds them to zoom the view, which the camera's turns then pass by. */
		uint16 ZoomButtons = 0;

		static constexpr uint16 Bit(EPadButton Button) { return static_cast<uint16>(1u << static_cast<uint32>(Button)); }

		constexpr bool IsHeld(EPadButton Button) const { return (Buttons & Bit(Button)) != 0; }

		bool operator==(const FPadState&) const = default;
	};
}
