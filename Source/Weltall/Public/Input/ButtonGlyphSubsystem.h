#pragma once

#include "CoreMinimal.h"
#include "Input/ButtonGlyphs.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ButtonGlyphSubsystem.generated.h"

class FDeviceUseProcessor;
struct FKey;

/**
 * How the game's PlayStation buttons are shown right now: each as the key or button the player's
 * bindings put it on, from the icons of the device the player last pressed something on (the
 * keyboard and mouse, a gamepad's family by its name, or the touch screen's pad, a DualShock's). A change of device or of bindings changes
 * the revision and is told. It also follows whether the player still has a controller: the gamepad
 * they last used going away leaves them without one until a gamepad is connected or they press
 * something on any device.
 */
UCLASS()
class WELTALL_API UButtonGlyphSubsystem : public UGameInstanceSubsystem, public Weltall::IButtonGlyphs
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnGlyphsChanged);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual Weltall::FButtonGlyph Glyph(Weltall::EPadButton Button) const override;
	virtual uint32 Revision() const override { return Changes; }

	/** The icons the glyphs are drawn from now. */
	Weltall::EButtonIconSet IconSet() const { return Set; }

	/** The icons of the gamepad the player uses or last used: Xbox's before any. */
	Weltall::EButtonIconSet GamepadIconSet() const { return GamepadSet; }

	/** The player pressed something on a device: its icons are shown from now on. */
	void UseDevice(bool bGamepad, FName HardwareName, FInputDeviceId Device);

	/**
	 * A phone's own buttons (its volume, back and menu keys), which the engine names as a gamepad's but are no device the
	 * player plays with; a controller's Select and Start come as them too, beside their own keys.
	 */
	static bool IsPhoneButton(const FKey& Key);

	/** The player touched the screen: the touch screen's pad is their device from now on. */
	void UseTouch();

	/** Whether the touch screen's pad is the device the player uses: from a touch, or from the start where the screen is one. */
	bool IsTouchInUse() const { return bUsingTouch; }

	/** Whether the player has a controller: false from the moment the gamepad they last used is disconnected. */
	bool HasController() const { return bHasController; }

	/** The gamepad the player is using now; none while the keyboard or the mouse was the last thing they pressed. */
	FInputDeviceId GamepadInUse() const { return bUsingGamepad ? LastGamepad : FInputDeviceId(); }

	/** The gamepad the player last used, whatever they pressed since; none before any. */
	FInputDeviceId LastGamepadUsed() const { return LastGamepad; }

	/** Told when the device or the bindings change. */
	FOnGlyphsChanged OnGlyphsChanged;

private:
	void Changed();
	void DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);

	TSharedPtr<FDeviceUseProcessor> Processor;
	Weltall::EButtonIconSet Set = Weltall::EButtonIconSet::Keyboard;
	Weltall::EButtonIconSet GamepadSet = Weltall::EButtonIconSet::Xbox;
	uint32 Changes = 0;
	FDelegateHandle SettingsChanged;
	FDelegateHandle ConnectionChanged;
	/** The gamepad the player last pressed something on, while it is the device in use. */
	bool bUsingGamepad = false;
	bool bUsingTouch = false;
	FInputDeviceId LastGamepad;
	bool bHasController = true;
};
