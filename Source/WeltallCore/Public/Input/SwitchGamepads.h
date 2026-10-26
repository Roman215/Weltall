#pragma once

#include "Input/HidGamepads.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * The Nintendo Switch Pro Controller's protocol, which the gamepads made for the Switch as well as for PCs speak too
	 * (they report as the Pro Controller). Over USB it sends nothing until it is greeted; over Bluetooth it sends its simple
	 * report until it is told otherwise.
	 */
	namespace SwitchReport
	{
		/** Output: rumble and a subcommand; rumble alone; the USB link's own commands. */
		inline constexpr uint8 Subcommand = 0x01;
		inline constexpr uint8 Rumble = 0x10;
		inline constexpr uint8 UsbCommand = 0x80;
		/** Input: a subcommand's reply; the full report (and with the NFC/IR chip's data); the simple one; a USB command's reply. */
		inline constexpr uint8 SubcommandReply = 0x21;
		inline constexpr uint8 Full = 0x30;
		inline constexpr uint8 FullWithMcu = 0x31;
		inline constexpr uint8 Simple = 0x3F;
		inline constexpr uint8 UsbReply = 0x81;
	}

	enum class ESwitchUsbCommand : uint8
	{
		Status = 0x01,
		Handshake = 0x02,
		HighSpeed = 0x03,
		/** Talk over USB from now on, without falling back to Bluetooth after a quiet while. */
		ForceUsb = 0x04,
	};

	enum class ESwitchSubcommand : uint8
	{
		SetInputMode = 0x03,
		ReadSpi = 0x10,
		SetPlayerLights = 0x30,
		EnableVibration = 0x48,
	};

	/**
	 * One axis of a stick: its centre and how far it reaches below and above it, in the report's units. The reach starts
	 * short of the controller's own calibration, so that a full push reads as full, and grows as the stick is seen further out.
	 */
	struct FSwitchAxis
	{
		int32 Center = 0;
		int32 Below = 1;
		int32 Above = 1;

		/** Raw from -1 to 1, up or right positive as the axis counts. */
		float Read(int32 Raw);

		bool operator==(const FSwitchAxis&) const = default;
	};

	/** The four axes (left X and Y, right X and Y) of the full report's 12-bit sticks and of the simple report's 16-bit ones. */
	struct FSwitchSticks
	{
		std::array<FSwitchAxis, 4> Full;
		std::array<FSwitchAxis, 4> Simple;

		bool operator==(const FSwitchSticks&) const = default;
	};

	/** Where the calibration lives in the controller's flash memory, and its length: the factory's, and the player's own. */
	inline constexpr uint32 SwitchFactorySticksAddress = 0x603D;
	inline constexpr uint8 SwitchFactorySticksLength = 18;
	inline constexpr uint32 SwitchUserSticksAddress = 0x8010;
	inline constexpr uint8 SwitchUserSticksLength = 22;

	/**
	 * The sticks from the factory's calibration and the player's own (each its flash bytes, or empty when it could not be
	 * read): the player's for a stick its mark says was calibrated, else the factory's; what neither gives, the middle of
	 * the range.
	 */
	WELTALLCORE_API FSwitchSticks SwitchSticksOf(std::span<const uint8> Factory, std::span<const uint8> User);

	/**
	 * A Pro Controller's input report: the full one (30h, 31h) or the simple one (3Fh), its buttons by where they sit (B at the
	 * bottom, A at the right, Minus and Plus as Select and Start, ZL and ZR as the triggers). False for any other report.
	 */
	WELTALLCORE_API bool ReadSwitchReport(std::span<const uint8> Report, FSwitchSticks& Sticks, FGamepadReading& Reading);

	/** A USB command: 80h and its number. */
	WELTALLCORE_API std::vector<uint8> SwitchUsbCommand(ESwitchUsbCommand Command);

	/** A subcommand with its data, the motors at rest, Packet counting the output reports (0-15). */
	WELTALLCORE_API std::vector<uint8> SwitchSubcommand(uint8 Packet, ESwitchSubcommand Subcommand, std::span<const uint8> Data);

	/** Whether Report is the controller's reply to Command. */
	WELTALLCORE_API bool IsSwitchUsbReply(std::span<const uint8> Report, ESwitchUsbCommand Command);

	/** The data of the controller's acknowledgement of Subcommand in Report; empty when Report is no such reply. */
	WELTALLCORE_API std::span<const uint8> SwitchSubcommandReply(std::span<const uint8> Report, ESwitchSubcommand Subcommand);

	/**
	 * The rumble report: the large motor's speed (0-255) on the low band at 160 Hz, the small one's on the high band at 320 Hz,
	 * both sides alike, as the DualShock's large motor rumbles and its small one buzzes.
	 */
	WELTALLCORE_API std::vector<uint8> SwitchRumble(uint8 Packet, uint8 Small, uint8 Large);

	/**
	 * When a Switch gamepad's motors are written: never sooner than 30 ms after the last time (sooner can turn one on Bluetooth
	 * off), on any change, and every 50 ms while they run, which they need to keep running.
	 */
	class WELTALLCORE_API FSwitchRumbleTimer
	{
	public:
		bool IsDue(uint64 Milliseconds, std::array<uint8, 2> Motors) const;
		void Written(uint64 Milliseconds, std::array<uint8, 2> Motors);

	private:
		bool bWritten = false;
		uint64 LastWritten = 0;
		std::array<uint8, 2> Running{};
	};
}
