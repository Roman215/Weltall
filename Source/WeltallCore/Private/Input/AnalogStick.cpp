#include "Input/AnalogStick.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Weltall
{
	namespace
	{
		constexpr uint16 DpadBits = 0xF000;
		constexpr uint16 AngleMask = 0xFFF;
		constexpr double Centre = 0x80;
		/** The stick's travel from its centre, in report units, that its dead zone leaves out, and the push that counts as full. */
		constexpr double DeadZone = 0.2 * 128.0;
		constexpr double FullPush = 0.9 * 128.0;
	}

	int32 FAnalogStick::Pace(int32 Top) const
	{
		return std::clamp(Tilt * FullTilt / Top, SlowestPace, FullTilt);
	}

	std::optional<FAnalogStick> ReadAnalogStick(uint16 Held, uint16 StickHeld, uint8 StickX, uint8 StickY)
	{
		if ((StickHeld & DpadBits) == 0 || (Held & ~StickHeld & DpadBits) != 0)
		{
			return std::nullopt;
		}
		const double X = static_cast<double>(StickX) - Centre;
		const double Y = static_cast<double>(StickY) - Centre;
		const double Reach = std::hypot(X, Y);
		if (Reach < DeadZone)
		{
			return std::nullopt;
		}
		FAnalogStick Stick;
		const double Turns = std::atan2(Y, X) / (2.0 * std::numbers::pi);
		Stick.Heading = static_cast<uint16>(std::lround(Turns * 4096.0) & AngleMask);
		const double Push = (Reach - DeadZone) / (FullPush - DeadZone);
		Stick.Tilt = static_cast<int32>(std::lround(std::clamp(Push, 0.0, 1.0) * FAnalogStick::FullTilt));
		return Stick;
	}
}
