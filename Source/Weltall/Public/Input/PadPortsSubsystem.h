#pragma once

#include "CoreMinimal.h"
#include "Input/PadPorts.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "PadPortsSubsystem.generated.h"

class FPadPortsProcessor;

/**
 * The game's two controller ports for the running game: the second player's gamepad as the players' presses
 * choose it (FPadPorts), heard before anything else of the game hears a key, read through the player's own
 * bindings; and both ports' motors, the first player's on the gamepad they last used.
 */
UCLASS()
class WELTALL_API UPadPortsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Whether the game reads the second port now. */
	void SetSecondRead(bool bRead);

	/** Whether a gamepad fills the second port. */
	bool HasSecond() const { return Ports.HasSecond(); }
	bool IsSecond(FInputDeviceId Device) const { return Ports.IsSecond(Device); }

	/** The buttons held on the second port's gamepad. */
	Weltall::FPadState SecondHeld() const;

	/** Whether the first port's controller has a motor: the player is using a gamepad. */
	bool FirstHasMotor() const;

	/** Each port's motor on or off, as the game last sent them. */
	void SetMotors(bool bFirst, bool bSecond) { Ports.SetMotors(bFirst, bSecond); }

	/** Sends the gamepads their motors' states. */
	void SendMotors(IInputInterface& Input);

	/** A device's key went down or up. */
	void KeyChanged(const FKey& Key, FInputDeviceId Device, bool bDown);

	/** An axis of a device moved, as the input heard it first. */
	void AxisChanged(const FKey& Key, FInputDeviceId Device, float Value) { Ports.AxisChanged(Key, Device, Value); }

private:
	void DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);

	FPadPorts Ports;
	TSharedPtr<FPadPortsProcessor> Processor;
	FDelegateHandle ConnectionChanged;
};
