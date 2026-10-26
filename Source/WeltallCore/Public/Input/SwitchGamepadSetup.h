#pragma once

#include "Input/SwitchGamepads.h"

namespace Weltall
{
	/** A Switch gamepad's reports as the setup exchanges them. */
	class ISwitchLink
	{
	public:
		virtual ~ISwitchLink() = default;

		/** Sends an output report; false if the gamepad would not take it. */
		virtual bool Send(std::span<const uint8> Report) = 0;

		/** The next input report it sends within Milliseconds; empty when none comes. Valid until the next call. */
		virtual std::span<const uint8> Receive(uint32 Milliseconds) = 0;

		/** A clock in milliseconds. */
		virtual uint64 Milliseconds() = 0;
	};

	/** What the setup found. */
	struct FSwitchSetup
	{
		/** Whether the gamepad answered as a Pro Controller does; one that did not is read through its HID description. */
		bool bAnswered = false;
		bool bUsb = false;
		FSwitchSticks Sticks;
		/** The count the next output report carries. */
		uint8 NextPacket = 0;
	};

	/**
	 * Sets a Switch gamepad up as players' tools do: over USB the greeting that makes it talk (and stay on USB) and its full
	 * report, over Bluetooth the report it already sends (changing it would leave other programs unable to read it until it
	 * reconnects); then its sticks' calibration, its motors and its first player light. Each reply is awaited 100 ms, each
	 * request tried five times.
	 */
	WELTALLCORE_API FSwitchSetup SetUpSwitchGamepad(ISwitchLink& Link);
}
