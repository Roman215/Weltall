#pragma once

#include "Foundation/Types.h"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** A gamepad's controls in XInput's order, the order the engine's gamepad keys are sent in. */
	enum class EGamepadControl : uint8
	{
		FaceBottom,
		FaceRight,
		FaceLeft,
		FaceTop,
		LeftShoulder,
		RightShoulder,
		SpecialRight,
		SpecialLeft,
		LeftThumb,
		RightThumb,
		LeftTrigger,
		RightTrigger,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		LeftStickUp,
		LeftStickDown,
		LeftStickLeft,
		LeftStickRight,
		RightStickUp,
		RightStickDown,
		RightStickLeft,
		RightStickRight,
	};
	inline constexpr int32 GamepadControlCount = 24;

	/** XInput's XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, _RIGHT_THUMB_DEADZONE and _TRIGGER_THRESHOLD, over their ranges. */
	inline constexpr float GamepadLeftStickDeadZone = 7849.0f / 32767.0f;
	inline constexpr float GamepadRightStickDeadZone = 8689.0f / 32767.0f;
	inline constexpr float GamepadTriggerThreshold = 30.0f / 255.0f;

	constexpr uint32 GamepadControlBit(EGamepadControl Control)
	{
		return 1u << static_cast<uint32>(Control);
	}

	/** Where a gamepad's controls stand: its buttons (EGamepadControl bits), its sticks -1 to 1 (right and up positive), its triggers 0 to 1. */
	struct FGamepadReading
	{
		uint32 Buttons = 0;
		float LeftX = 0.0f;
		float LeftY = 0.0f;
		float RightX = 0.0f;
		float RightY = 0.0f;
		float LeftTrigger = 0.0f;
		float RightTrigger = 0.0f;

		bool operator==(const FGamepadReading&) const = default;
	};

	/** Every control held: the buttons, a trigger past XInput's threshold, and each stick's directions past XInput's dead zones. */
	WELTALLCORE_API uint32 HeldGamepadControls(const FGamepadReading& Reading);

	/** The d-pad a hat holds: 0 up to 7 clockwise, anything else at rest. */
	WELTALLCORE_API uint32 GamepadHatButtons(int32 Hat);

	/** The gamepads Weltall reads from their own reports rather than through XInput, by what it knows of their layout. */
	enum class EHidGamepadModel : uint8
	{
		DualShock4,
		DualSense,
		PlayStationClassic,
		/** The Switch Pro Controller, and the gamepads that pass for it (SwitchGamepads.h). */
		SwitchPro,
		/** Any other gamepad: its buttons as most PC gamepads number them. */
		Other,
	};

	WELTALLCORE_API EHidGamepadModel HidGamepadModelOf(uint16 VendorId, uint16 ProductId);

	/** The name the engine's input devices know a model by (which shows PlayStation's or Nintendo's buttons); empty for Other, which goes by its own product name. */
	WELTALLCORE_API std::string_view HidGamepadName(EHidGamepadModel Model);

	/** Sony's wireless adapter for the DualShock 4, which reports as one whether a controller is paired with it or not. */
	WELTALLCORE_API bool IsDualShock4Adapter(uint16 VendorId, uint16 ProductId);

	/**
	 * A DualShock 4's input report: 01h (USB's, or Bluetooth's short one) or Bluetooth's 11h. False for any other. Through the
	 * wireless adapter (bAdapter), a report while no controller is paired with it reads as a controller at rest.
	 */
	WELTALLCORE_API bool ReadDualShock4Report(std::span<const uint8> Report, bool bAdapter, FGamepadReading& Reading);

	/**
	 * A DualSense's input report: USB's 01h, Bluetooth's short 01h (laid out as the DualShock 4's) or its full 31h. False for
	 * any other.
	 */
	WELTALLCORE_API bool ReadDualSenseReport(std::span<const uint8> Report, bool bBluetooth, FGamepadReading& Reading);

	/**
	 * A HID gamepad's controls as the system's HID parser gives them: the buttons held by their number from 1, each axis it has
	 * from -1 to 1 (right and down positive), and its hat's direction, 0 (up) to 7 clockwise, while it is pushed.
	 */
	struct FHidControls
	{
		std::vector<uint16> Buttons;
		std::optional<float> X;
		std::optional<float> Y;
		std::optional<float> Z;
		std::optional<float> Rx;
		std::optional<float> Ry;
		std::optional<float> Rz;
		std::optional<int32> Hat;
	};

	/** What a gamepad read through its HID controls holds: the PlayStation Classic controller's own layout, or the one most PC gamepads share. */
	WELTALLCORE_API FGamepadReading ReadHidControls(EHidGamepadModel Model, const FHidControls& Controls);

	/** The output report that runs a DualShock 4's motors: USB's 05h, or Bluetooth's 11h with its checksum. Small runs the light right motor. */
	WELTALLCORE_API std::vector<uint8> DualShock4MotorReport(bool bBluetooth, uint8 Small, uint8 Large);

	/** The output report that runs a DualSense's motors: USB's 02h, or Bluetooth's 31h with its checksum, Sequence counting them (0-15). */
	WELTALLCORE_API std::vector<uint8> DualSenseMotorReport(bool bBluetooth, uint8 Sequence, uint8 Small, uint8 Large);
}
