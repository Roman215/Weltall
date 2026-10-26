#include "Input/ButtonGlyphSubsystem.h"

#include "Framework/Application/IInputProcessor.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "InputCoreTypes.h"
#include "Shell/PlayerSettingsSubsystem.h"

/** Watches what the player presses, before the game takes it, for the device it came from. */
class FDeviceUseProcessor final : public IInputProcessor
{
public:
	explicit FDeviceUseProcessor(UButtonGlyphSubsystem& InGlyphs) : Glyphs(&InGlyphs) {}

	/** The glyphs are going: nothing is told any more. */
	void Detach() { Glyphs = nullptr; }

	virtual void Tick(const float DeltaTime, FSlateApplication& Application, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		Use(Event.GetKey(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& Application, const FAnalogInputEvent& Event) override
	{
		// A stick resting off centre is not the player picking the gamepad up.
		constexpr float Pushed = 0.5f;
		if (FMath::Abs(Event.GetAnalogValue()) >= Pushed)
		{
			Use(Event.GetKey(), Event.GetInputDeviceId());
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& Application, const FPointerEvent& Event) override
	{
		if (Event.IsTouchEvent())
		{
			if (Glyphs != nullptr)
			{
				Glyphs->UseTouch();
			}
			return false;
		}
		Use(Event.GetEffectingButton(), Event.GetInputDeviceId());
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& Application, const FPointerEvent& Event, const FPointerEvent* Gesture) override
	{
		// A gesture (a finger held, a pinch) is the touch screen's, which its touches have told already.
		if (Gesture == nullptr && !Event.IsTouchEvent())
		{
			Use(EKeys::MouseWheelAxis, Event.GetInputDeviceId());
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("WeltallDeviceUse"); }

private:
	void Use(const FKey& Key, FInputDeviceId Device) const
	{
		if (Glyphs == nullptr || UButtonGlyphSubsystem::IsPhoneButton(Key))
		{
			return;
		}
		const bool bGamepad = Key.IsGamepadKey();
		if (bGamepad && Device.IsValid())
		{
			// A second player's gamepad is not the player's device: it has a user of its own.
			const IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
			const FPlatformUserId User = Mapper.GetUserForInputDevice(Device);
			if (User.IsValid() && User != Mapper.GetPrimaryPlatformUser())
			{
				return;
			}
		}
		FName Hardware;
		if (const UInputDeviceSubsystem* Devices = bGamepad ? UInputDeviceSubsystem::Get() : nullptr)
		{
			Hardware = Devices->GetInputDeviceHardwareIdentifier(Device).HardwareDeviceIdentifier;
		}
		Glyphs->UseDevice(bGamepad, Hardware, Device);
	}

	UButtonGlyphSubsystem* Glyphs;
};

void UButtonGlyphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UPlayerSettingsSubsystem* Settings = Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	SettingsChanged = Settings->OnSettingsChanged.AddUObject(this, &UButtonGlyphSubsystem::Changed);
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FDeviceUseProcessor>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
	ConnectionChanged = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &UButtonGlyphSubsystem::DeviceConnectionChanged);
	// A phone's player has the screen in hand before anything else.
	if (FPlatformMisc::SupportsTouchInput())
	{
		UseTouch();
	}
}

void UButtonGlyphSubsystem::Deinitialize()
{
	if (Processor)
	{
		Processor->Detach();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
		}
		Processor.Reset();
	}
	if (UPlayerSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>())
	{
		Settings->OnSettingsChanged.Remove(SettingsChanged);
	}
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionChanged);
	Super::Deinitialize();
}

Weltall::FButtonGlyph UButtonGlyphSubsystem::Glyph(Weltall::EPadButton Button) const
{
	return Weltall::ButtonGlyph(Button, GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls, Set);
}

bool UButtonGlyphSubsystem::IsPhoneButton(const FKey& Key)
{
	return Key == EKeys::Android_Volume_Up || Key == EKeys::Android_Volume_Down || Key == EKeys::Android_Back || Key == EKeys::Android_Menu;
}

void UButtonGlyphSubsystem::UseTouch()
{
	bHasController = true;
	bUsingGamepad = false;
	bUsingTouch = true;
	if (Set != Weltall::EButtonIconSet::PlayStation)
	{
		Set = Weltall::EButtonIconSet::PlayStation;
		Changed();
	}
}

void UButtonGlyphSubsystem::UseDevice(bool bGamepad, FName HardwareName, FInputDeviceId Device)
{
	// Anything pressed, on whatever device, is the player's controller.
	bHasController = true;
	bUsingGamepad = bGamepad;
	bUsingTouch = false;
	if (bGamepad)
	{
		LastGamepad = Device;
	}
	const Weltall::EButtonIconSet Used = Weltall::IconSetOfDevice(bGamepad, TCHAR_TO_UTF8(*HardwareName.ToString()));
	if (bGamepad)
	{
		GamepadSet = Used;
	}
	if (Used != Set)
	{
		Set = Used;
		Changed();
	}
}

void UButtonGlyphSubsystem::DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
{
	if (State == EInputDeviceConnectionState::Connected)
	{
		bHasController = true;
	}
	else if (State == EInputDeviceConnectionState::Disconnected && bUsingGamepad && Device == LastGamepad)
	{
		bHasController = false;
	}
}

void UButtonGlyphSubsystem::Changed()
{
	++Changes;
	OnGlyphsChanged.Broadcast();
}
