#pragma once

#include "CoreMinimal.h"
#include "Input/PadState.h"
#include "Menu/MenuProgram.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <deque>

#include "KeyListenerSubsystem.generated.h"

class FKeyListenProcessor;
namespace Weltall { class FTouchPad; }

/**
 * The keys the player presses, for the options screen's Controls page to bind: while it listens, each key,
 * mouse button and controller button pressed, each stick pushed past half way, the mouse moved a little way and
 * its wheel turned, in order, by the engine's names; and the game given no buttons meanwhile, nor after until
 * every one is let go, so the key pressed does nothing else.
 */
UCLASS()
class WELTALL_API UKeyListenerSubsystem : public UGameInstanceSubsystem, public Weltall::IMenuKeys
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Listen(bool bOn, bool bControllerChords = false) override;
	virtual std::optional<Weltall::FPressedKey> TakePressed() override;
	virtual Weltall::EInputFamily LastFamily() const override;
	virtual Weltall::EButtonIconSet GamepadIcons() const override;

	/** The buttons the game is given of those the player holds. */
	Weltall::FPadState Filter(Weltall::FPadState Held);

	bool IsListening() const { return bListening; }
	void ForgetController(FInputDeviceId Device);

	/** Capture or run a shortcut from the touch pad's raw button edges. */
	void TouchButtonsChanged(Weltall::FTouchPad& Pad, uint16 Before, const Weltall::FControlBindings& Bindings);
	void ForgetTouch() { PendingTouch = 0; }

	/** Controller shortcuts pressed since the last read, in the port actions' order. */
	TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>> TakeChords();

	/** The input heard something, kept while listening. */
	void Heard(const FKey& Key, int32 Axis);

private:
	friend class FKeyListenProcessor;
	void HeardChord(const FKey& Modifier, const FKey& Trigger);
	bool TriggerChord(const FKey& Trigger, const TSet<FKey>& Held, const Weltall::FControlBindings& Bindings);
	const Weltall::FControlBindings& Bindings() const;
	void DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);
	void AppActivationChanged(bool bActive);

	TSharedPtr<FKeyListenProcessor> Processor;
	FDelegateHandle ConnectionChanged;
	FDelegateHandle ActivationChanged;
	FDelegateHandle Backgrounded;
	uint8 ChordsPressed = 0;
	uint16 PendingTouch = 0;
	bool bListening = false;
	bool bCaptureTouchChords = false;
	/** Listening stopped with buttons held, which the game is not given until they are let go. */
	bool bWithheld = false;
	std::deque<Weltall::FPressedKey> Pressed;
};
