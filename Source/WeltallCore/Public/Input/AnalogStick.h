#pragma once

#include "Foundation/Types.h"

#include <optional>

namespace Weltall
{
	/**
	 * The left stick as analog movement reads it, from a DualShock's report (0 left or up, 0xFF right or down, 0x80 its
	 * centre): its heading, 4096 a turn with right 0 and down 400h (the field's d-pad table's way), and how far it is
	 * pushed past its dead zone, 0 to FullTilt.
	 */
	struct FAnalogStick
	{
		static constexpr int32 FullTilt = 0x1000;
		/** Under Always run, a push past this runs and a lighter one walks; a walk is full from it on the world map. */
		static constexpr int32 RunTilt = FullTilt * 2 / 3;
		/** The slowest walk a light push gives, as a share of the walk's own pace. */
		static constexpr int32 SlowestPace = FullTilt / 3;

		uint16 Heading = 0;
		int32 Tilt = 0;

		/** A walk's pace from this push (FullTilt the walk's own), its band reaching full at Top. */
		WELTALLCORE_API int32 Pace(int32 Top) const;
	};

	/**
	 * The stick, when it alone holds a direction (the d-pad's own held directions win) and lies past its dead zone: the
	 * directions among Held (bits 12-15, logical) that only the stick holds are StickHeld, its report StickX and StickY.
	 * A direction the frames held while the stick has come back to rest is none.
	 */
	WELTALLCORE_API std::optional<FAnalogStick> ReadAnalogStick(uint16 Held, uint16 StickHeld, uint8 StickX, uint8 StickY);
}
