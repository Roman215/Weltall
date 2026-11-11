#pragma once

#include "CoreMinimal.h"
#include "Input/ControlBindings.h"
#include "Input/PadState.h"
#include "InputCoreTypes.h"

class IInputInterface;

/**
 * The console's second controller port and both ports' motors, among the engine's input devices.
 *
 * Every gamepad is the first player's until the game reads its second port. From then on a gamepad other than
 * the one the first player was using at that moment is the second player's as soon as a button is pressed on
 * it. Its events are consumed until the game stops reading the port or the gamepad is unplugged. Its buttons are the gamepad keys
 * the player's bindings name.
 *
 * Each port's motor is the gamepad's small one, on or off: the first port's on the first player's gamepad, the
 * second's on the second player's.
 */
class WELTALL_API FPadPorts
{
public:
	/**
	 * Whether the game reads its second port now. As the reading begins, FirstGamepad is the gamepad the first
	 * player is using (none: the keyboard), which stays theirs; as it ends, the second player's goes back to
	 * the first.
	 */
	void SetSecondRead(bool bRead, FInputDeviceId FirstGamepad);

	/** Whether a gamepad fills the second port. */
	bool HasSecond() const { return Second.IsValid(); }
	bool IsSecond(FInputDeviceId Device) const { return HasSecond() && Device == Second; }

	/**
	 * The buttons held on the second port's gamepad, by the bindings' gamepad keys, with the directions only its
	 * stick holds and that stick's tilt (PadStick): an analog pad.
	 */
	Weltall::FPadState SecondHeld(const Weltall::FControlBindings& Bindings) const;

	/** A key of a device went down or up. True when this press made its gamepad the second player's. */
	bool KeyChanged(const FKey& Key, FInputDeviceId Device, bool bDown);

	/** An axis of a device moved. */
	void AxisChanged(const FKey& Key, FInputDeviceId Device, float Value);

	/** A device is gone. */
	void DeviceDisconnected(FInputDeviceId Device);

	/** The second port loses its gamepad, which goes back to the first player unless it is gone. */
	void ReleaseSecond(bool bStillConnected);

	/** Each port's motor on or off, as the game last sent them. */
	void SetMotors(bool bFirst, bool bSecond);
	bool AnyMotorOn() const { return bFirstMotor || bSecondMotor; }

	/** Sends the gamepads their motors' states: the first port's to FirstGamepad (none: no gamepad to run it). */
	void SendMotors(IInputInterface& Input, FInputDeviceId FirstGamepad);

private:
	bool bSecondRead = false;
	/** The gamepad the first player was using as the second port began to be read; none when that was the keyboard. */
	FInputDeviceId First;
	FInputDeviceId Second;
	bool bSecondRemapped = false;
	/** The gamepad keys held on the second port's gamepad, and where its axes stand. */
	TSet<FName> SecondDown;
	TMap<FName, float> SecondAxes;
	bool bFirstMotor = false;
	bool bSecondMotor = false;
	/** The gamepad the second port's motor was last sent on to, which is sent off once the port has lost it. */
	FInputDeviceId SecondMotorSent;
};
