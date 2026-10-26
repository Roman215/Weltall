#include "Input/SwitchGamepads.h"

#include <algorithm>
#include <cmath>

namespace Weltall
{
	namespace
	{
		/** Flash never written reads as all ones. */
		constexpr int32 Unwritten = 0xFFF;
		constexpr int32 FullMiddle = 0x800;
		constexpr int32 SimpleMiddle = 0x8000;
		/** The reach of an axis whose calibration is missing: seven tenths of half the range. */
		constexpr int32 DefaultFullReach = FullMiddle * 7 / 10;
		constexpr int32 SimpleReach = 0x4000;

		/** Where the player's calibration of each stick begins, after its two-byte mark. */
		constexpr std::size_t UserLeft = 2;
		constexpr std::size_t UserRight = 13;
		constexpr std::array<uint8, 2> UserMark = {0xB2, 0xA1};

		/** The motors at rest on one side: the high band at 320 Hz and the low band at 160 Hz, both silent. */
		constexpr std::array<uint8, 4> RumbleAtRest = {0x00, 0x01, 0x40, 0x40};

		constexpr uint32 Bit(EGamepadControl Control)
		{
			return GamepadControlBit(Control);
		}

		/** The two 12-bit values three bytes hold, the first in the low bits. */
		std::array<int32, 2> Pair(std::span<const uint8> Bytes)
		{
			return {Bytes[0] | ((Bytes[1] & 0x0F) << 8), (Bytes[1] >> 4) | (Bytes[2] << 4)};
		}

		/** An axis from its calibration, the middle of the range where the flash holds none or a centre far from the middle. */
		FSwitchAxis Calibrated(int32 Center, int32 Below, int32 Above)
		{
			const bool bCentered = Center != Unwritten && Center > FullMiddle / 2 && Center < FullMiddle * 3 / 2;
			const auto Reach = [](int32 Value) { return Value == Unwritten || Value <= 0 ? DefaultFullReach : Value; };
			return {bCentered ? Center : FullMiddle, std::max(Reach(Below) * 7 / 10, 1), std::max(Reach(Above) * 7 / 10, 1)};
		}

		/** The left stick's nine bytes: X and Y above the centre, the centres, X and Y below. */
		void ReadLeft(std::span<const uint8> Bytes, FSwitchSticks& Sticks)
		{
			const std::array<int32, 2> Above = Pair(Bytes.subspan(0, 3));
			const std::array<int32, 2> Center = Pair(Bytes.subspan(3, 3));
			const std::array<int32, 2> Below = Pair(Bytes.subspan(6, 3));
			Sticks.Full[0] = Calibrated(Center[0], Below[0], Above[0]);
			Sticks.Full[1] = Calibrated(Center[1], Below[1], Above[1]);
		}

		/** The right stick's, in another order: the centres, below, above. */
		void ReadRight(std::span<const uint8> Bytes, FSwitchSticks& Sticks)
		{
			const std::array<int32, 2> Center = Pair(Bytes.subspan(0, 3));
			const std::array<int32, 2> Below = Pair(Bytes.subspan(3, 3));
			const std::array<int32, 2> Above = Pair(Bytes.subspan(6, 3));
			Sticks.Full[2] = Calibrated(Center[0], Below[0], Above[0]);
			Sticks.Full[3] = Calibrated(Center[1], Below[1], Above[1]);
		}

		bool HasUserMark(std::span<const uint8> User, std::size_t Start)
		{
			return User.size() >= Start + 9 && User[Start - 2] == UserMark[0] && User[Start - 1] == UserMark[1];
		}

		void ReadFull(std::span<const uint8> Report, FSwitchSticks& Sticks, FGamepadReading& Reading)
		{
			const uint8 Right = Report[3];
			const uint8 Shared = Report[4];
			const uint8 Left = Report[5];
			uint32 Buttons = 0;
			const auto Add = [&Buttons](uint8 Byte, uint8 Mask, EGamepadControl Control) { Buttons |= (Byte & Mask) != 0 ? Bit(Control) : 0; };
			Add(Right, 0x01, EGamepadControl::FaceLeft);
			Add(Right, 0x02, EGamepadControl::FaceTop);
			Add(Right, 0x04, EGamepadControl::FaceBottom);
			Add(Right, 0x08, EGamepadControl::FaceRight);
			Add(Right, 0x40, EGamepadControl::RightShoulder);
			Add(Shared, 0x01, EGamepadControl::SpecialLeft);
			Add(Shared, 0x02, EGamepadControl::SpecialRight);
			Add(Shared, 0x04, EGamepadControl::RightThumb);
			Add(Shared, 0x08, EGamepadControl::LeftThumb);
			Add(Left, 0x01, EGamepadControl::DPadDown);
			Add(Left, 0x02, EGamepadControl::DPadUp);
			Add(Left, 0x04, EGamepadControl::DPadRight);
			Add(Left, 0x08, EGamepadControl::DPadLeft);
			Add(Left, 0x40, EGamepadControl::LeftShoulder);
			Reading.Buttons = Buttons;
			Reading.LeftTrigger = (Left & 0x80) != 0 ? 1.0f : 0.0f;
			Reading.RightTrigger = (Right & 0x80) != 0 ? 1.0f : 0.0f;
			// The full report counts its sticks up and right.
			const std::array<int32, 2> LeftStick = Pair(Report.subspan(6, 3));
			const std::array<int32, 2> RightStick = Pair(Report.subspan(9, 3));
			Reading.LeftX = Sticks.Full[0].Read(LeftStick[0]);
			Reading.LeftY = Sticks.Full[1].Read(LeftStick[1]);
			Reading.RightX = Sticks.Full[2].Read(RightStick[0]);
			Reading.RightY = Sticks.Full[3].Read(RightStick[1]);
		}

		void ReadSimple(std::span<const uint8> Report, FSwitchSticks& Sticks, FGamepadReading& Reading)
		{
			const uint8 Faces = Report[1];
			const uint8 Others = Report[2];
			uint32 Buttons = GamepadHatButtons(Report[3]);
			const auto Add = [&Buttons](uint8 Byte, uint8 Mask, EGamepadControl Control) { Buttons |= (Byte & Mask) != 0 ? Bit(Control) : 0; };
			Add(Faces, 0x01, EGamepadControl::FaceBottom);
			Add(Faces, 0x02, EGamepadControl::FaceRight);
			Add(Faces, 0x04, EGamepadControl::FaceLeft);
			Add(Faces, 0x08, EGamepadControl::FaceTop);
			Add(Faces, 0x10, EGamepadControl::LeftShoulder);
			Add(Faces, 0x20, EGamepadControl::RightShoulder);
			Add(Others, 0x01, EGamepadControl::SpecialLeft);
			Add(Others, 0x02, EGamepadControl::SpecialRight);
			Add(Others, 0x04, EGamepadControl::LeftThumb);
			Add(Others, 0x08, EGamepadControl::RightThumb);
			Reading.Buttons = Buttons;
			Reading.LeftTrigger = (Faces & 0x40) != 0 ? 1.0f : 0.0f;
			Reading.RightTrigger = (Faces & 0x80) != 0 ? 1.0f : 0.0f;
			// The simple report counts its sticks down and right, as HID does.
			const auto Axis = [Report](std::size_t At) { return static_cast<int32>(Report[At] | (Report[At + 1] << 8)); };
			Reading.LeftX = Sticks.Simple[0].Read(Axis(4));
			Reading.LeftY = -Sticks.Simple[1].Read(Axis(6));
			Reading.RightX = Sticks.Simple[2].Read(Axis(8));
			Reading.RightY = -Sticks.Simple[3].Read(Axis(10));
		}

		/**
		 * The amplitude step (0-100) of a motor's speed. Above an amplitude of 0.23 a step is a 32nd of an octave, above 0.12 a
		 * 16th, and below a quarter, step 15 being 0.1125; step 100 is the strongest that is safe for the motors.
		 */
		int32 AmplitudeStep(uint8 Speed)
		{
			if (Speed == 0)
			{
				return 0;
			}
			const double Amplitude = Speed / 255.0;
			const double Step = Amplitude > 0.23 ? std::log2(Amplitude * 8.7) * 32.0
				: Amplitude > 0.12				 ? std::log2(Amplitude * 17.0) * 16.0
												 : 15.0 + std::log2(Amplitude / 0.112491) * 4.0;
			return std::clamp(static_cast<int32>(std::lround(Step)), 1, 100);
		}
	}

	float FSwitchAxis::Read(int32 Raw)
	{
		const int32 Offset = Raw - Center;
		if (Offset < 0)
		{
			Below = std::max(Below, -Offset);
			return static_cast<float>(Offset) / static_cast<float>(Below);
		}
		Above = std::max(Above, Offset);
		return static_cast<float>(Offset) / static_cast<float>(Above);
	}

	FSwitchSticks SwitchSticksOf(std::span<const uint8> Factory, std::span<const uint8> User)
	{
		FSwitchSticks Sticks;
		Sticks.Full.fill(Calibrated(Unwritten, Unwritten, Unwritten));
		Sticks.Simple.fill({SimpleMiddle, SimpleReach, SimpleReach});
		if (Factory.size() >= SwitchFactorySticksLength)
		{
			ReadLeft(Factory.subspan(0, 9), Sticks);
			ReadRight(Factory.subspan(9, 9), Sticks);
		}
		if (HasUserMark(User, UserLeft))
		{
			ReadLeft(User.subspan(UserLeft, 9), Sticks);
		}
		if (HasUserMark(User, UserRight))
		{
			ReadRight(User.subspan(UserRight, 9), Sticks);
		}
		return Sticks;
	}

	bool ReadSwitchReport(std::span<const uint8> Report, FSwitchSticks& Sticks, FGamepadReading& Reading)
	{
		constexpr std::size_t ReportLength = 12;
		if (Report.size() < ReportLength)
		{
			return false;
		}
		switch (Report[0])
		{
		case SwitchReport::Full:
		case SwitchReport::FullWithMcu:
			ReadFull(Report, Sticks, Reading);
			return true;
		case SwitchReport::Simple:
			ReadSimple(Report, Sticks, Reading);
			return true;
		default:
			return false;
		}
	}

	std::vector<uint8> SwitchUsbCommand(ESwitchUsbCommand Command)
	{
		return {SwitchReport::UsbCommand, static_cast<uint8>(Command)};
	}

	std::vector<uint8> SwitchSubcommand(uint8 Packet, ESwitchSubcommand Subcommand, std::span<const uint8> Data)
	{
		std::vector<uint8> Report = {SwitchReport::Subcommand, static_cast<uint8>(Packet & 0x0F)};
		Report.insert(Report.end(), RumbleAtRest.begin(), RumbleAtRest.end());
		Report.insert(Report.end(), RumbleAtRest.begin(), RumbleAtRest.end());
		Report.push_back(static_cast<uint8>(Subcommand));
		Report.insert(Report.end(), Data.begin(), Data.end());
		return Report;
	}

	bool IsSwitchUsbReply(std::span<const uint8> Report, ESwitchUsbCommand Command)
	{
		return Report.size() >= 2 && Report[0] == SwitchReport::UsbReply && Report[1] == static_cast<uint8>(Command);
	}

	std::span<const uint8> SwitchSubcommandReply(std::span<const uint8> Report, ESwitchSubcommand Subcommand)
	{
		// The controls (bytes 1-12), then the acknowledgement (its top bit set) and the subcommand it answers.
		constexpr std::size_t Acknowledgement = 13;
		constexpr std::size_t Answered = 14;
		constexpr std::size_t Data = 15;
		const bool bReply = Report.size() >= Data && Report[0] == SwitchReport::SubcommandReply && (Report[Acknowledgement] & 0x80) != 0
			&& Report[Answered] == static_cast<uint8>(Subcommand);
		return bReply ? Report.subspan(Data) : std::span<const uint8>();
	}

	std::vector<uint8> SwitchRumble(uint8 Packet, uint8 Small, uint8 Large)
	{
		// Each band's frequency takes a bit beside the other band's amplitude: the high band's 320 Hz is its byte 0 and the low
		// bit of byte 1, the low band's amplitude its byte 3 and the top bit of byte 2.
		const int32 High = AmplitudeStep(Small);
		const int32 Low = AmplitudeStep(Large);
		const std::array<uint8, 4> Side = {RumbleAtRest[0], static_cast<uint8>(RumbleAtRest[1] | (High * 2)), static_cast<uint8>(RumbleAtRest[2] | ((Low & 1) << 7)),
			static_cast<uint8>(RumbleAtRest[3] + Low / 2)};
		std::vector<uint8> Report(10, 0);
		Report[0] = SwitchReport::Rumble;
		Report[1] = static_cast<uint8>(Packet & 0x0F);
		std::copy(Side.begin(), Side.end(), Report.begin() + 2);
		std::copy(Side.begin(), Side.end(), Report.begin() + 6);
		return Report;
	}

	bool FSwitchRumbleTimer::IsDue(uint64 Milliseconds, std::array<uint8, 2> Motors) const
	{
		constexpr uint64 Spacing = 30;
		constexpr uint64 Refresh = 50;
		if (bWritten && Milliseconds - LastWritten < Spacing)
		{
			return false;
		}
		const bool bRunning = Running != std::array<uint8, 2>{};
		return Motors != Running || (bRunning && Milliseconds - LastWritten >= Refresh);
	}

	void FSwitchRumbleTimer::Written(uint64 Milliseconds, std::array<uint8, 2> Motors)
	{
		bWritten = true;
		LastWritten = Milliseconds;
		Running = Motors;
	}
}
