#include "Input/PadPorts.h"

#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/IInputInterface.h"
#include "Input/PadStick.h"

namespace
{
	/** The second player among the engine's users. */
	constexpr int32 SecondUserIndex = 1;

	/** The game runs a controller's small motor alone, on or off. */
	FForceFeedbackValues MotorValues(bool bOn)
	{
		FForceFeedbackValues Values;
		Values.RightSmall = bOn ? 1.0f : 0.0f;
		return Values;
	}

	/**
	 * Sends a gamepad its motor's state. The engine's force feedback names a gamepad by its user on Windows (who
	 * has one gamepad for it) and by the device itself elsewhere.
	 */
	void SendMotor(IInputInterface& Input, FInputDeviceId Device, bool bOn)
	{
#if PLATFORM_WINDOWS
		IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
		const int32 Address = Mapper.GetUserIndexForPlatformUser(Mapper.GetUserForInputDevice(Device));
#else
		const int32 Address = Device.GetId();
#endif
		if (Address >= 0)
		{
			Input.SetForceFeedbackChannelValues(Address, MotorValues(bOn));
		}
	}
}

void FPadPorts::SetSecondRead(bool bRead, FInputDeviceId FirstGamepad)
{
	if (bRead == bSecondRead)
	{
		return;
	}
	bSecondRead = bRead;
	if (bRead)
	{
		First = FirstGamepad;
	}
	else
	{
		ReleaseSecond(true);
	}
}

Weltall::FPadState FPadPorts::SecondHeld(const Weltall::FControlBindings& Bindings) const
{
	Weltall::FPadState Held;
	if (!Second.IsValid())
	{
		return Held;
	}
	for (int32 Index = 0; Index < Weltall::ControlActionCount; ++Index)
	{
		const Weltall::EControlAction Action = static_cast<Weltall::EControlAction>(Index);
		const std::optional<Weltall::EPadButton> Button = Weltall::PadButtonOf(Action);
		if (!Button)
		{
			continue;
		}
		for (const std::string& Binding : Bindings.Keys(Action))
		{
			const std::string_view Name = Weltall::BoundKeyName(Binding);
			if (Weltall::FamilyOfKey(Binding) == Weltall::EInputFamily::Gamepad && SecondDown.Contains(FName(static_cast<int32>(Name.size()), Name.data())))
			{
				Held.Buttons |= Weltall::FPadState::Bit(*Button);
			}
		}
	}
	Held.StickButtons = PadStick::OnlyStickHolds(Bindings, Held, [this](const FKey& Key) { return SecondDown.Contains(Key.GetFName()); });
	const TPair<FKey, FKey> Axes = PadStick::Axes(PadStick::BoundStick(Bindings));
	PadStick::Report(SecondAxes.FindRef(Axes.Key.GetFName()), SecondAxes.FindRef(Axes.Value.GetFName()), Held);
	Held.bAnalog = true;
	return Held;
}

void FPadPorts::AxisChanged(const FKey& Key, FInputDeviceId Device, float Value)
{
	if (Device == Second && Device.IsValid())
	{
		SecondAxes.Add(Key.GetFName(), Value);
	}
}

bool FPadPorts::KeyChanged(const FKey& Key, FInputDeviceId Device, bool bDown)
{
	if (!Key.IsGamepadKey() || !Device.IsValid())
	{
		return false;
	}
	if (Device == Second)
	{
		if (bDown)
		{
			SecondDown.Add(Key.GetFName());
		}
		else
		{
			SecondDown.Remove(Key.GetFName());
		}
		return false;
	}
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId Owner = Mapper.GetUserForInputDevice(Device);
	if (!bDown || !bSecondRead || Second.IsValid() || Device == First)
	{
		return false;
	}
	bSecondRemapped = Owner == Mapper.GetPrimaryPlatformUser()
		&& Mapper.Internal_ChangeInputDeviceUserMapping(Device, Mapper.GetPlatformUserForUserIndex(SecondUserIndex), Owner);
	Second = Device;
	SecondDown.Reset();
	SecondAxes.Reset();
	SecondDown.Add(Key.GetFName());
	return true;
}

void FPadPorts::DeviceDisconnected(FInputDeviceId Device)
{
	if (Device == Second)
	{
		ReleaseSecond(false);
	}
}

void FPadPorts::ReleaseSecond(bool bStillConnected)
{
	if (!Second.IsValid())
	{
		return;
	}
	if (bStillConnected && bSecondRemapped)
	{
		IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
		Mapper.Internal_ChangeInputDeviceUserMapping(Second, Mapper.GetPrimaryPlatformUser(), Mapper.GetUserForInputDevice(Second));
	}
	Second = FInputDeviceId();
	bSecondRemapped = false;
	SecondDown.Reset();
	SecondAxes.Reset();
}

void FPadPorts::SetMotors(bool bFirst, bool bSecond)
{
	bFirstMotor = bFirst;
	bSecondMotor = bSecond;
}

void FPadPorts::SendMotors(IInputInterface& Input, FInputDeviceId FirstGamepad)
{
	// A gamepad that left the second port with its motor running is sent off before anything else: it may be the first player's again.
	if (SecondMotorSent.IsValid() && SecondMotorSent != Second)
	{
		SendMotor(Input, SecondMotorSent, false);
	}
	SecondMotorSent = FInputDeviceId();
	if (FirstGamepad.IsValid() && FirstGamepad != Second)
	{
		SendMotor(Input, FirstGamepad, bFirstMotor);
	}
	if (Second.IsValid())
	{
		SendMotor(Input, Second, bSecondMotor);
		if (bSecondMotor)
		{
			SecondMotorSent = Second;
		}
	}
}
