#include "Input/PadPortsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "Input/KeyListenerSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"

/** Watches the gamepads' keys before anything else of the game hears them. */
class FPadPortsProcessor final : public IInputProcessor
{
public:
	explicit FPadPortsProcessor(UPadPortsSubsystem& InPorts) : Ports(&InPorts) {}

	/** The ports are going: nothing is told any more. */
	void Detach() { Ports = nullptr; }

	virtual void Tick(const float DeltaTime, FSlateApplication& Application, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		if (Ports != nullptr && !Event.IsRepeat())
		{
			Ports->KeyChanged(Event.GetKey(), Event.GetInputDeviceId(), true);
		}
		return Ports != nullptr && Event.GetKey().IsGamepadKey() && Ports->IsSecond(Event.GetInputDeviceId());
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		if (Ports != nullptr)
		{
			Ports->KeyChanged(Event.GetKey(), Event.GetInputDeviceId(), false);
		}
		return Ports != nullptr && Event.GetKey().IsGamepadKey() && Ports->IsSecond(Event.GetInputDeviceId());
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& Application, const FAnalogInputEvent& Event) override
	{
		if (Ports != nullptr)
		{
			Ports->AxisChanged(Event.GetKey(), Event.GetInputDeviceId(), Event.GetAnalogValue());
		}
		return Ports != nullptr && Event.GetKey().IsGamepadKey() && Ports->IsSecond(Event.GetInputDeviceId());
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("WeltallPadPorts"); }

private:
	UPadPortsSubsystem* Ports;
};

void UPadPortsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	Collection.InitializeDependency<UButtonGlyphSubsystem>();
	if (FSlateApplication::IsInitialized())
	{
		// Before the game's other listeners: a gamepad taken for the second port is not the first player's from that press on.
		Processor = MakeShared<FPadPortsProcessor>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor, EInputPreProcessorType::PreGame);
	}
	ConnectionChanged = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &UPadPortsSubsystem::DeviceConnectionChanged);
}

void UPadPortsSubsystem::Deinitialize()
{
	if (Ports.AnyMotorOn())
	{
		Ports.SetMotors(false, false);
		if (IInputInterface* Input = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetInputInterface() : nullptr)
		{
			SendMotors(*Input);
		}
	}
	Ports.ReleaseSecond(true);
	if (Processor)
	{
		Processor->Detach();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
		}
		Processor.Reset();
	}
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionChanged);
	Super::Deinitialize();
}

void UPadPortsSubsystem::SetSecondRead(bool bRead)
{
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	Ports.SetSecondRead(bRead, Glyphs != nullptr ? Glyphs->GamepadInUse() : FInputDeviceId());
}

Weltall::FPadState UPadPortsSubsystem::SecondHeld() const
{
	return Ports.SecondHeld(GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls);
}

bool UPadPortsSubsystem::FirstHasMotor() const
{
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	return Glyphs != nullptr && Glyphs->GamepadInUse().IsValid();
}

void UPadPortsSubsystem::SendMotors(IInputInterface& Input)
{
	// The engine's force feedback asks its device subsystem which gamepad is a user's.
	if (UInputDeviceSubsystem::Get() == nullptr)
	{
		return;
	}
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	Ports.SendMotors(Input, Glyphs != nullptr ? Glyphs->LastGamepadUsed() : FInputDeviceId());
}

void UPadPortsSubsystem::KeyChanged(const FKey& Key, FInputDeviceId Device, bool bDown)
{
	if (Ports.KeyChanged(Key, Device, bDown))
	{
		if (UKeyListenerSubsystem* Listener = GetGameInstance()->GetSubsystem<UKeyListenerSubsystem>())
		{
			Listener->ForgetController(Device);
		}
		// What the first player's input heard held on this gamepad would never be heard let go.
		if (APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController())
		{
			Player->FlushPressedKeys();
		}
	}
}

void UPadPortsSubsystem::DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
{
	if (State == EInputDeviceConnectionState::Disconnected)
	{
		Ports.DeviceDisconnected(Device);
	}
}
