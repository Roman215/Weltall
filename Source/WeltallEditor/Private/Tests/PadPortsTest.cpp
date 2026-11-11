#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/IInputInterface.h"
#include "Input/PadPorts.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Keeps what the ports send the gamepads' motors: the address and whether the small motor runs. */
	class FPadPortsMotorsSent final : public IInputInterface
	{
	public:
		virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {}

		virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override
		{
			Sent.Emplace(ControllerId, Values.RightSmall > 0.0f);
			bOthersQuiet = bOthersQuiet && Values.LeftLarge == 0.0f && Values.LeftSmall == 0.0f && Values.RightLarge == 0.0f;
		}

		virtual void SetLightColor(int32 ControllerId, FColor Color) override {}
		virtual void ResetLightColor(int32 ControllerId) override {}

		TArray<TPair<int32, bool>> Sent;
		bool bOthersQuiet = true;
	};

	/** Two gamepads plugged in for a test, the first player's as the game's policy makes every device; unplugged again as it ends. */
	struct FPadPortsTestGamepads
	{
		FPadPortsTestGamepads()
		{
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			for (FInputDeviceId& Gamepad : Gamepads)
			{
				Gamepad = Mapper.AllocateNewInputDeviceId();
				Mapper.Internal_MapInputDeviceToUser(Gamepad, Mapper.GetPrimaryPlatformUser(), EInputDeviceConnectionState::Connected);
			}
		}

		~FPadPortsTestGamepads()
		{
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			for (const FInputDeviceId Gamepad : Gamepads)
			{
				Mapper.Internal_MapInputDeviceToUser(Gamepad, Mapper.GetUserForUnpairedInputDevices(), EInputDeviceConnectionState::Disconnected);
			}
		}

		bool IsFirstPlayers(FInputDeviceId Gamepad) const
		{
			const IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			return Mapper.GetUserForInputDevice(Gamepad) == Mapper.GetPrimaryPlatformUser();
		}

		/** How the ports address a gamepad's motor on this platform. */
		int32 MotorAddress(FInputDeviceId Gamepad) const
		{
#if PLATFORM_WINDOWS
			IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			return Mapper.GetUserIndexForPlatformUser(Mapper.GetUserForInputDevice(Gamepad));
#else
			return Gamepad.GetId();
#endif
		}

		FInputDeviceId Gamepads[2];
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadPortsSecondGamepadTest, "Weltall.Input.ASecondGamepadIsTheSecondPlayersWhileTheGameReadsItsSecondPort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadPortsSecondGamepadTest::RunTest(const FString&)
{
	const FPadPortsTestGamepads Plugged;
	const FInputDeviceId One = Plugged.Gamepads[0];
	const FInputDeviceId Two = Plugged.Gamepads[1];
	const Weltall::FControlBindings Bindings = Weltall::FControlBindings::Defaults();
	const uint16 Cross = Weltall::FPadState::Bit(Weltall::EPadButton::Cross);
	const uint16 Up = Weltall::FPadState::Bit(Weltall::EPadButton::Up);
	FPadPorts Ports;

	// While the second port is not read every gamepad stays the first player's, whatever is pressed.
	TestFalse(TEXT("Taken while the port is not read"), Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, Two, true));
	Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, Two, false);
	TestFalse(TEXT("A second pad before the port is read"), Ports.HasSecond());
	TestTrue(TEXT("The second gamepad is the first player's"), Plugged.IsFirstPlayers(Two));

	// Read, with the first player on the first gamepad: that one stays theirs, the other becomes the second player's at its first press.
	Ports.SetSecondRead(true, One);
	TestFalse(TEXT("The first player's gamepad taken"), Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, One, true));
	TestFalse(TEXT("A key let go takes a gamepad"), Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, Two, false));
	TestFalse(TEXT("A keyboard key takes a device"), Ports.KeyChanged(EKeys::Enter, Two, true));
	TestTrue(TEXT("The other gamepad's press takes it"), Ports.KeyChanged(EKeys::Gamepad_DPad_Up, Two, true));
	TestTrue(TEXT("The second port has a gamepad"), Ports.HasSecond());
	TestFalse(TEXT("The second player's gamepad is still the first player's"), Plugged.IsFirstPlayers(Two));
	TestTrue(TEXT("The first player's gamepad stays theirs"), Plugged.IsFirstPlayers(One));

	// Its buttons are the bindings' gamepad keys, the stick's directions among them; the first player's gamepad adds nothing.
	TestEqual(TEXT("Held after the press that took it"), Ports.SecondHeld(Bindings).Buttons, Up);
	Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, Two, true);
	TestEqual(TEXT("Held with a face button"), Ports.SecondHeld(Bindings).Buttons, static_cast<uint16>(Up | Cross));
	Ports.KeyChanged(EKeys::Gamepad_DPad_Up, Two, false);
	Ports.KeyChanged(EKeys::Gamepad_LeftStick_Up, Two, true);
	TestEqual(TEXT("Held with the stick pushed up"), Ports.SecondHeld(Bindings).Buttons, static_cast<uint16>(Up | Cross));
	Ports.KeyChanged(EKeys::Gamepad_LeftStick_Up, Two, false);
	Ports.KeyChanged(EKeys::Gamepad_FaceButton_Right, One, true);
	TestEqual(TEXT("Held with the first player pressing"), Ports.SecondHeld(Bindings).Buttons, Cross);

	// The motors: each port's on its own gamepad, the small motor alone.
	FPadPortsMotorsSent Motors;
	Ports.SetMotors(false, true);
	Ports.SendMotors(Motors, One);
	TestEqual(TEXT("Motors sent"), Motors.Sent.Num(), 2);
	if (Motors.Sent.Num() == 2)
	{
		TestTrue(TEXT("The first port's motor off on the first gamepad"), Motors.Sent[0] == TPair<int32, bool>(Plugged.MotorAddress(One), false));
		TestTrue(TEXT("The second port's motor on on the second gamepad"), Motors.Sent[1] == TPair<int32, bool>(Plugged.MotorAddress(Two), true));
	}
	TestTrue(TEXT("Only the small motor is run"), Motors.bOthersQuiet);

	// The port no longer read: the gamepad is the first player's again, nothing is held, and its running motor is sent off.
	Ports.SetSecondRead(false, One);
	TestFalse(TEXT("A second pad after the port is read"), Ports.HasSecond());
	TestTrue(TEXT("The gamepad is the first player's again"), Plugged.IsFirstPlayers(Two));
	TestEqual(TEXT("Held after the port is read"), Ports.SecondHeld(Bindings).Buttons, uint16{0});
	Motors.Sent.Reset();
	Ports.SetMotors(true, true);
	Ports.SendMotors(Motors, One);
	TestEqual(TEXT("Motors sent after the port is read"), Motors.Sent.Num(), 2);
	if (Motors.Sent.Num() == 2)
	{
		TestTrue(TEXT("The gamepad that left the port is sent off"), Motors.Sent[0] == TPair<int32, bool>(Plugged.MotorAddress(Two), false));
		TestTrue(TEXT("The first port's motor on"), Motors.Sent[1] == TPair<int32, bool>(Plugged.MotorAddress(One), true));
	}

	// With the first player on the keyboard, any gamepad is the second player's; unplugged, the port is empty.
	Ports.SetSecondRead(true, FInputDeviceId());
	TestTrue(TEXT("A gamepad's press beside the keyboard takes it"), Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, One, true));
	TestFalse(TEXT("A second gamepad taken beside the first"), Ports.KeyChanged(EKeys::Gamepad_FaceButton_Bottom, Two, true));
	TestTrue(TEXT("The gamepad not taken stays the first player's"), Plugged.IsFirstPlayers(Two));
	// The first player's own last gamepad is the second player's now: the first port has no gamepad to run.
	Motors.Sent.Reset();
	Ports.SetMotors(true, false);
	Ports.SendMotors(Motors, One);
	TestEqual(TEXT("Motors sent with one gamepad"), Motors.Sent.Num(), 1);
	Ports.DeviceDisconnected(One);
	TestFalse(TEXT("A second pad after it is unplugged"), Ports.HasSecond());
	Ports.SetSecondRead(false, FInputDeviceId());

	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId OtherUser = Mapper.GetPlatformUserForUserIndex(2);
	Mapper.Internal_MapInputDeviceToUser(Two, OtherUser, EInputDeviceConnectionState::Connected);
	Ports.SetSecondRead(true, One);
	TestTrue(TEXT("A gamepad assigned to another platform user can take the second port"),
		Ports.KeyChanged(EKeys::Gamepad_DPad_Up, Two, true));
	TestTrue(TEXT("Only the assigned controller is reserved"), Ports.IsSecond(Two) && !Ports.IsSecond(One));
	TestEqual(TEXT("The second port reads the other user's controller"), Ports.SecondHeld(Bindings).Buttons, Up);
	Ports.SetSecondRead(false, One);
	TestFalse(TEXT("The controller returns to play on the first port"), Ports.IsSecond(Two));
	TestTrue(TEXT("The platform assignment is retained"), Mapper.GetUserForInputDevice(Two) == OtherUser);
	return true;
}

#endif
