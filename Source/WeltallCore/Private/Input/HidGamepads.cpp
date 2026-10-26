#include "Input/HidGamepads.h"

#include "Foundation/Crc32.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	namespace
	{
		constexpr uint16 SonyVendor = 0x054C;
		constexpr uint16 NintendoVendor = 0x057E;
		constexpr uint8 ShortReport = 0x01;
		constexpr uint8 DualShock4BluetoothReport = 0x11;
		constexpr uint8 DualSenseBluetoothReport = 0x31;

		constexpr uint16 DualShock4Adapter = 0x0BA0;

		/** Where USB's report holds the adapter's status, and its bit for no controller paired. */
		constexpr std::size_t AdapterStatus = 31;
		constexpr uint8 AdapterWithoutController = 0x04;

		/** How far an axis that stands for a d-pad leans before it presses a direction. */
		constexpr float AxisPressed = 0.5f;

		/** A Bluetooth output report's checksum is the CRC-32 of this byte and the report before it. */
		constexpr uint8 OutputChecksumSeed = 0xA2;

		constexpr uint32 Bit(EGamepadControl Control)
		{
			return GamepadControlBit(Control);
		}

		constexpr uint32 Up = Bit(EGamepadControl::DPadUp);
		constexpr uint32 Down = Bit(EGamepadControl::DPadDown);
		constexpr uint32 Left = Bit(EGamepadControl::DPadLeft);
		constexpr uint32 Right = Bit(EGamepadControl::DPadRight);

		/** A stick's byte, 128 its centre, from -1 to 1. */
		float StickOf(uint8 Value)
		{
			const float Offset = static_cast<float>(Value) - 128.0f;
			return Offset / (Value < 128 ? 128.0f : 127.0f);
		}

		/** The d-pad two axes lean towards. */
		uint32 AxisButtons(std::optional<float> X, std::optional<float> Y)
		{
			uint32 Buttons = 0;
			Buttons |= X && *X < -AxisPressed ? Left : X && *X > AxisPressed ? Right : 0;
			Buttons |= Y && *Y < -AxisPressed ? Up : Y && *Y > AxisPressed ? Down : 0;
			return Buttons;
		}

		/**
		 * Sony's three button bytes: the hat and the face buttons; the shoulders, Share (Create) and Options and the sticks'
		 * clicks; then the PS button and the touchpad's click, which is Select too, as PlayStation's own collections of its
		 * first console's games have it.
		 */
		uint32 SonyButtons(uint8 Faces, uint8 Middle, uint8 Last)
		{
			uint32 Buttons = GamepadHatButtons(Faces & 0x0F);
			const auto Add = [&Buttons](uint8 Byte, uint8 Mask, EGamepadControl Control) { Buttons |= (Byte & Mask) != 0 ? Bit(Control) : 0; };
			Add(Faces, 0x10, EGamepadControl::FaceLeft);
			Add(Faces, 0x20, EGamepadControl::FaceBottom);
			Add(Faces, 0x40, EGamepadControl::FaceRight);
			Add(Faces, 0x80, EGamepadControl::FaceTop);
			Add(Middle, 0x01, EGamepadControl::LeftShoulder);
			Add(Middle, 0x02, EGamepadControl::RightShoulder);
			Add(Middle, 0x10, EGamepadControl::SpecialLeft);
			Add(Middle, 0x20, EGamepadControl::SpecialRight);
			Add(Middle, 0x40, EGamepadControl::LeftThumb);
			Add(Middle, 0x80, EGamepadControl::RightThumb);
			Add(Last, 0x02, EGamepadControl::SpecialLeft);
			return Buttons;
		}

		/** The sticks, the three button bytes and the triggers: the DualShock 4's report, and the DualSense's short one. */
		void ReadDualShock4Layout(std::span<const uint8> Data, FGamepadReading& Reading)
		{
			Reading.LeftX = StickOf(Data[0]);
			Reading.LeftY = -StickOf(Data[1]);
			Reading.RightX = StickOf(Data[2]);
			Reading.RightY = -StickOf(Data[3]);
			Reading.Buttons = SonyButtons(Data[4], Data[5], Data[6]);
			Reading.LeftTrigger = Data[7] / 255.0f;
			Reading.RightTrigger = Data[8] / 255.0f;
		}

		/** The sticks, the triggers, a counter, then the three button bytes: the DualSense's full report. */
		void ReadDualSenseLayout(std::span<const uint8> Data, FGamepadReading& Reading)
		{
			Reading.LeftX = StickOf(Data[0]);
			Reading.LeftY = -StickOf(Data[1]);
			Reading.RightX = StickOf(Data[2]);
			Reading.RightY = -StickOf(Data[3]);
			Reading.LeftTrigger = Data[4] / 255.0f;
			Reading.RightTrigger = Data[5] / 255.0f;
			Reading.Buttons = SonyButtons(Data[7], Data[8], Data[9]);
		}

		/** Writes the checksum a Bluetooth output report ends with into its last four bytes, least significant first. */
		void Sign(std::vector<uint8>& Report)
		{
			const uint8 Seed = OutputChecksumSeed;
			const uint32 Crc = Crc32(std::span<const uint8>(Report).first(Report.size() - 4), Crc32(std::span<const uint8>(&Seed, 1)));
			for (std::size_t Byte = 0; Byte < 4; ++Byte)
			{
				Report[Report.size() - 4 + Byte] = static_cast<uint8>(Crc >> (Byte * 8));
			}
		}
	}

	uint32 HeldGamepadControls(const FGamepadReading& Reading)
	{
		uint32 Held = Reading.Buttons;
		const auto Hold = [&Held](bool bHeld, EGamepadControl Control) { Held |= bHeld ? Bit(Control) : 0; };
		Hold(Reading.LeftTrigger > GamepadTriggerThreshold, EGamepadControl::LeftTrigger);
		Hold(Reading.RightTrigger > GamepadTriggerThreshold, EGamepadControl::RightTrigger);
		Hold(Reading.LeftY > GamepadLeftStickDeadZone, EGamepadControl::LeftStickUp);
		Hold(Reading.LeftY < -GamepadLeftStickDeadZone, EGamepadControl::LeftStickDown);
		Hold(Reading.LeftX < -GamepadLeftStickDeadZone, EGamepadControl::LeftStickLeft);
		Hold(Reading.LeftX > GamepadLeftStickDeadZone, EGamepadControl::LeftStickRight);
		Hold(Reading.RightY > GamepadRightStickDeadZone, EGamepadControl::RightStickUp);
		Hold(Reading.RightY < -GamepadRightStickDeadZone, EGamepadControl::RightStickDown);
		Hold(Reading.RightX < -GamepadRightStickDeadZone, EGamepadControl::RightStickLeft);
		Hold(Reading.RightX > GamepadRightStickDeadZone, EGamepadControl::RightStickRight);
		return Held;
	}

	uint32 GamepadHatButtons(int32 Hat)
	{
		static constexpr std::array<uint32, 8> Directions = {Up, Up | Right, Right, Down | Right, Down, Down | Left, Left, Up | Left};
		return Hat >= 0 && Hat < static_cast<int32>(Directions.size()) ? Directions[static_cast<std::size_t>(Hat)] : 0;
	}

	EHidGamepadModel HidGamepadModelOf(uint16 VendorId, uint16 ProductId)
	{
		// The Pro Controller (and every gamepad that passes for it to the Switch), HORI's wireless pad and PDP's wireless line.
		const bool bSwitchPro = (VendorId == NintendoVendor && ProductId == 0x2009) || (VendorId == 0x0F0D && ProductId == 0x00F6)
			|| (VendorId == 0x0E6F && ProductId == 0x0186);
		if (bSwitchPro)
		{
			return EHidGamepadModel::SwitchPro;
		}
		if (VendorId != SonyVendor)
		{
			return EHidGamepadModel::Other;
		}
		switch (ProductId)
		{
		case 0x05C4:
		case 0x09CC:
		case 0x0BA0:
			return EHidGamepadModel::DualShock4;
		case 0x0CE6:
		case 0x0DF2:
			return EHidGamepadModel::DualSense;
		case 0x0CDA:
			return EHidGamepadModel::PlayStationClassic;
		default:
			return EHidGamepadModel::Other;
		}
	}

	std::string_view HidGamepadName(EHidGamepadModel Model)
	{
		switch (Model)
		{
		case EHidGamepadModel::DualShock4:
			return "DualShock 4";
		case EHidGamepadModel::DualSense:
			return "DualSense";
		case EHidGamepadModel::PlayStationClassic:
			return "PlayStation Classic Controller";
		case EHidGamepadModel::SwitchPro:
			return "Nintendo Switch Pro Controller";
		case EHidGamepadModel::Other:
			break;
		}
		return {};
	}

	bool IsDualShock4Adapter(uint16 VendorId, uint16 ProductId)
	{
		return VendorId == SonyVendor && ProductId == DualShock4Adapter;
	}

	bool ReadDualShock4Report(std::span<const uint8> Report, bool bAdapter, FGamepadReading& Reading)
	{
		if (bAdapter && Report.size() > AdapterStatus && Report[0] == ShortReport && (Report[AdapterStatus] & AdapterWithoutController) != 0)
		{
			Reading = {};
			return true;
		}
		if (Report.size() >= 10 && Report[0] == ShortReport)
		{
			ReadDualShock4Layout(Report.subspan(1), Reading);
			return true;
		}
		if (Report.size() >= 12 && Report[0] == DualShock4BluetoothReport)
		{
			ReadDualShock4Layout(Report.subspan(3), Reading);
			return true;
		}
		return false;
	}

	bool ReadDualSenseReport(std::span<const uint8> Report, bool bBluetooth, FGamepadReading& Reading)
	{
		if (Report.size() >= 11 && Report[0] == ShortReport && !bBluetooth)
		{
			ReadDualSenseLayout(Report.subspan(1), Reading);
			return true;
		}
		if (Report.size() >= 10 && Report[0] == ShortReport && bBluetooth)
		{
			ReadDualShock4Layout(Report.subspan(1), Reading);
			return true;
		}
		if (Report.size() >= 12 && Report[0] == DualSenseBluetoothReport)
		{
			ReadDualSenseLayout(Report.subspan(2), Reading);
			return true;
		}
		return false;
	}

	FGamepadReading ReadHidControls(EHidGamepadModel Model, const FHidControls& Controls)
	{
		// The buttons by number from 1; a trigger among them is held all the way.
		static constexpr std::array<EGamepadControl, 10> ClassicButtons = {EGamepadControl::FaceTop, EGamepadControl::FaceRight,
			EGamepadControl::FaceBottom, EGamepadControl::FaceLeft, EGamepadControl::LeftTrigger, EGamepadControl::RightTrigger,
			EGamepadControl::LeftShoulder, EGamepadControl::RightShoulder, EGamepadControl::SpecialLeft, EGamepadControl::SpecialRight};
		static constexpr std::array<EGamepadControl, 12> CommonButtons = {EGamepadControl::FaceLeft, EGamepadControl::FaceBottom,
			EGamepadControl::FaceRight, EGamepadControl::FaceTop, EGamepadControl::LeftShoulder, EGamepadControl::RightShoulder,
			EGamepadControl::LeftTrigger, EGamepadControl::RightTrigger, EGamepadControl::SpecialLeft, EGamepadControl::SpecialRight,
			EGamepadControl::LeftThumb, EGamepadControl::RightThumb};
		const bool bClassic = Model == EHidGamepadModel::PlayStationClassic;
		const std::span<const EGamepadControl> Layout = bClassic ? std::span<const EGamepadControl>(ClassicButtons) : std::span<const EGamepadControl>(CommonButtons);
		FGamepadReading Reading;
		for (const uint16 Number : Controls.Buttons)
		{
			if (Number == 0 || Number > Layout.size())
			{
				continue;
			}
			const EGamepadControl Control = Layout[Number - 1u];
			if (Control == EGamepadControl::LeftTrigger)
			{
				Reading.LeftTrigger = 1.0f;
			}
			else if (Control == EGamepadControl::RightTrigger)
			{
				Reading.RightTrigger = 1.0f;
			}
			else
			{
				Reading.Buttons |= Bit(Control);
			}
		}
		if (bClassic)
		{
			// Its d-pad is its only pair of axes.
			Reading.Buttons |= AxisButtons(Controls.X, Controls.Y);
			return Reading;
		}
		Reading.Buttons |= Controls.Hat ? GamepadHatButtons(*Controls.Hat) : 0;
		Reading.LeftX = Controls.X.value_or(0.0f);
		Reading.LeftY = -Controls.Y.value_or(0.0f);
		// The right stick on Z and Rz, as most such gamepads put it, or else on the rotations about X and Y.
		const bool bZ = Controls.Z && Controls.Rz;
		Reading.RightX = (bZ ? Controls.Z : Controls.Rx).value_or(0.0f);
		Reading.RightY = -(bZ ? Controls.Rz : Controls.Ry).value_or(0.0f);
		return Reading;
	}

	std::vector<uint8> DualShock4MotorReport(bool bBluetooth, uint8 Small, uint8 Large)
	{
		// The motors' flag alone: the light bar keeps its colour.
		constexpr uint8 MotorsValid = 0x01;
		if (!bBluetooth)
		{
			std::vector<uint8> Report(32, 0);
			Report[0] = 0x05;
			Report[1] = MotorsValid;
			Report[4] = Small;
			Report[5] = Large;
			return Report;
		}
		// The report carries HID data and a checksum.
		constexpr uint8 HidWithChecksum = 0xC0;
		std::vector<uint8> Report(78, 0);
		Report[0] = DualShock4BluetoothReport;
		Report[1] = HidWithChecksum;
		Report[3] = MotorsValid;
		Report[6] = Small;
		Report[7] = Large;
		Sign(Report);
		return Report;
	}

	std::vector<uint8> DualSenseMotorReport(bool bBluetooth, uint8 Sequence, uint8 Small, uint8 Large)
	{
		// Rumble as the DualShock 4's: the flag firmwares before 2.21 read, and the one later firmwares read.
		constexpr uint8 RumbleValid = 0x03;
		constexpr std::size_t SecondFlags = 38;
		constexpr uint8 NewerRumbleValid = 0x04;
		const std::size_t Common = bBluetooth ? 3 : 1;
		std::vector<uint8> Report(bBluetooth ? 78 : 63, 0);
		Report[0] = bBluetooth ? DualSenseBluetoothReport : 0x02;
		if (bBluetooth)
		{
			Report[1] = static_cast<uint8>((Sequence & 0x0F) << 4);
			Report[2] = 0x10;
		}
		Report[Common] = RumbleValid;
		Report[Common + 2] = Small;
		Report[Common + 3] = Large;
		Report[Common + SecondFlags] = NewerRumbleValid;
		if (bBluetooth)
		{
			Sign(Report);
		}
		return Report;
	}
}
