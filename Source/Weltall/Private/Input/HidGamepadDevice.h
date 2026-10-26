#pragma once

#include "CoreMinimal.h"

/**
 * The gamepads Windows' XInput does not read, read through Windows' HID: Sony's DualShock 4, DualSense and PlayStation
 * Classic controller and the Switch Pro Controller (with the gamepads that pass for it) from their own reports, any other
 * gamepad through its HID description. They come to the game as gamepads do from XInput, plugged in or out at any time, the
 * PlayStation and Nintendo ones named so their buttons show. Registered with the engine's input devices on Windows; nothing
 * elsewhere, where the engine reads such gamepads itself.
 */
namespace HidGamepads
{
	void Register();
	void Unregister();
}
