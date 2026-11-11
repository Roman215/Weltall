#include "Input/KeyListenerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "Input/TouchControlsSubsystem.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "UnrealClient.h"

#include <bit>

namespace
{
	/** How far a stick is pushed, and how far the mouse travels (in its counts), before it is heard as moved. */
	constexpr float StickPushed = 0.5f;
	constexpr float StickBack = 0.25f;
	constexpr double MouseTravel = 40.0;

	FKey TouchKey(const Weltall::FTouchPad& Pad, uint16 Button)
	{
		const std::string_view Name = Pad.KeyOf(static_cast<Weltall::EPadButton>(std::countr_zero(Button)));
		return FKey(FName(static_cast<int32>(Name.size()), Name.data()));
	}
}

/** Hears what the player presses, before the game takes it, while the keys listen. */
class FKeyListenProcessor final : public IInputProcessor
{
public:
	explicit FKeyListenProcessor(UKeyListenerSubsystem& InKeys) : Keys(&InKeys) {}

	void Detach() { Keys = nullptr; }

	/** Listening began: what the mouse and sticks did before is forgotten. */
	void Restart(bool bChords)
	{
		Travel = FVector2D::ZeroVector;
		Pushed.Reset();
		bCaptureChords = bChords;
		Pending.Reset();
	}

	void Forget(FInputDeviceId Device)
	{
		Controllers.Remove(Device);
		if (Pending && Pending->Key == Device)
		{
			Pending.Reset();
		}
	}

	void ResetControllers()
	{
		Controllers.Reset();
		Pending.Reset();
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& Application, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		const FKey Key = Event.GetKey();
		if (Keys == nullptr || UButtonGlyphSubsystem::IsPhoneButton(Key))
		{
			return false;
		}
		const FInputDeviceId Device = Event.GetInputDeviceId();
		if (!Key.IsGamepadKey() || !Device.IsValid())
		{
			if (!Event.IsRepeat())
			{
				Hear(Key, 0);
			}
			return false;
		}
		FControllerButtons& Buttons = Controllers.FindOrAdd(Device);
		if (Buttons.Consumed.Contains(Key))
		{
			return true;
		}
		if (Event.IsRepeat() || Buttons.Held.Contains(Key))
		{
			return RouteButton(Event, IE_Repeat);
		}
		if (!Keys->IsListening() && !Buttons.Held.IsEmpty() && Keys->TriggerChord(Key, Buttons.Held, Keys->Bindings()))
		{
			Buttons.Consumed.Add(Key);
			return true;
		}
		Buttons.Held.Add(Key);
		if (Keys->IsListening() && bCaptureChords)
		{
			if (!Pending)
			{
				Pending.Emplace(Device, Key);
			}
			else if (Pending->Key == Device)
			{
				Keys->HeardChord(Pending->Value, Key);
				Pending.Reset();
			}
		}
		else
		{
			Hear(Key, 0);
		}
		return RouteButton(Event, IE_Pressed);
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		const FInputDeviceId Device = Event.GetInputDeviceId();
		const FKey Key = Event.GetKey();
		if (FControllerButtons* Buttons = Controllers.Find(Device))
		{
			Buttons->Held.Remove(Key);
			if (Buttons->Consumed.Remove(Key) != 0)
			{
				return true;
			}
		}
		if (Pending && Pending->Key == Device && Pending->Value == Key)
		{
			Pending.Reset();
			Hear(Key, 0);
		}
		return RouteButton(Event, IE_Released);
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& Application, const FAnalogInputEvent& Event) override
	{
		// Heard once as it passes half way, again only once it has come back near the middle.
		const FKey Key = Event.GetKey();
		const float Value = Event.GetAnalogValue();
		if (FMath::Abs(Value) < StickBack)
		{
			Pushed.Remove(Key);
		}
		else if (FMath::Abs(Value) >= StickPushed && !Pushed.Contains(Key))
		{
			Pushed.Add(Key);
			Hear(Key, Value > 0.0f ? 1 : -1);
		}
		return RouteAxis(Event);
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& Application, const FPointerEvent& Event) override
	{
		if (Keys == nullptr || !Keys->IsListening())
		{
			return false;
		}
		Travel += Event.GetCursorDelta();
		if (FMath::Abs(Travel.X) >= MouseTravel || FMath::Abs(Travel.Y) >= MouseTravel)
		{
			// The engine's MouseY grows as the mouse is pushed away, the cursor's as it comes nearer.
			const bool bAcross = FMath::Abs(Travel.X) >= FMath::Abs(Travel.Y);
			Hear(bAcross ? EKeys::MouseX : EKeys::MouseY, bAcross ? (Travel.X > 0.0 ? 1 : -1) : (Travel.Y < 0.0 ? 1 : -1));
			Travel = FVector2D::ZeroVector;
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& Application, const FPointerEvent& Event) override
	{
		if (!Event.IsTouchEvent())
		{
			Hear(Event.GetEffectingButton(), 0);
		}
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& Application, const FPointerEvent& Event, const FPointerEvent* Gesture) override
	{
		if (Event.GetWheelDelta() != 0.0f)
		{
			Hear(EKeys::MouseWheelAxis, Event.GetWheelDelta() > 0.0f ? 1 : -1);
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("WeltallKeyListener"); }

private:
	/** Slate routes to the device owner's focus path, which is empty for a gamepad with no local player. */
	UGameViewportClient* SharedViewport(const FKey& Key, FInputDeviceId Device) const
	{
		const auto& Mapper = IPlatformInputDeviceMapper::Get();
		if (Keys == nullptr || !Key.IsGamepadKey() || !Device.IsValid()
			|| Mapper.GetUserForInputDevice(Device) == Mapper.GetPrimaryPlatformUser())
		{
			return nullptr;
		}
		UGameViewportClient* Client = Keys->GetGameInstance()->GetGameViewportClient();
		if (Client == nullptr || Client->IgnoreInput() || Client->Viewport == nullptr
			|| !Client->Viewport->HasFocus() || Client->Viewport->IsPlayInEditorViewport())
		{
			return nullptr;
		}
		return Client;
	}

	bool RouteButton(const FKeyEvent& Event, EInputEvent Action) const
	{
		UGameViewportClient* Client = SharedViewport(Event.GetKey(), Event.GetInputDeviceId());
		APlayerController* Player = Client != nullptr ? Keys->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (Player == nullptr) return false;
		Player->InputKey(FInputKeyEventArgs(Client->Viewport, Event.GetInputDeviceId(), Event.GetKey(), Action,
			1.0f, false, Event.GetEventTimestamp()));
		return true;
	}

	bool RouteAxis(const FAnalogInputEvent& Event) const
	{
		UGameViewportClient* Client = SharedViewport(Event.GetKey(), Event.GetInputDeviceId());
		APlayerController* Player = Client != nullptr ? Keys->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (Player == nullptr) return false;
		const float Value = Event.GetKey() == EKeys::Gamepad_RightY ? -Event.GetAnalogValue() : Event.GetAnalogValue();
		Player->InputKey(FInputKeyEventArgs(Client->Viewport, Event.GetInputDeviceId(), Event.GetKey(), Value,
			FApp::GetDeltaTime(), 1, Event.GetEventTimestamp()));
		return true;
	}

	void Hear(const FKey& Key, int32 Axis) const
	{
		if (Keys != nullptr && Keys->IsListening())
		{
			Keys->Heard(Key, Axis);
		}
	}

	UKeyListenerSubsystem* Keys;
	FVector2D Travel = FVector2D::ZeroVector;
	TSet<FKey> Pushed;
	struct FControllerButtons
	{
		TSet<FKey> Held;
		TSet<FKey> Consumed;
	};
	TMap<FInputDeviceId, FControllerButtons> Controllers;
	TOptional<TPair<FInputDeviceId, FKey>> Pending;
	bool bCaptureChords = false;
};

void UKeyListenerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UButtonGlyphSubsystem>();
	Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FKeyListenProcessor>(*this);
		FSlateApplication& Slate = FSlateApplication::Get();
		Slate.RegisterInputPreProcessor(Processor);
		ActivationChanged = Slate.OnApplicationActivationStateChanged().AddUObject(this, &UKeyListenerSubsystem::AppActivationChanged);
	}
	ConnectionChanged = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &UKeyListenerSubsystem::DeviceConnectionChanged);
	Backgrounded = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddWeakLambda(this, [this]() { AppActivationChanged(false); });
}

void UKeyListenerSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(Backgrounded);
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionChanged);
	if (Processor)
	{
		Processor->Detach();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
			FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationChanged);
		}
		Processor.Reset();
	}
	Super::Deinitialize();
}

void UKeyListenerSubsystem::Listen(bool bOn, bool bControllerChords)
{
	bListening = bOn;
	bWithheld |= bOn;
	bCaptureTouchChords = bOn && bControllerChords;
	ForgetTouch();
	ChordsPressed = 0;
	Pressed.clear();
	if (Processor)
	{
		Processor->Restart(bOn && bControllerChords);
	}
}

std::optional<Weltall::FPressedKey> UKeyListenerSubsystem::TakePressed()
{
	if (Pressed.empty())
	{
		return std::nullopt;
	}
	Weltall::FPressedKey Next = std::move(Pressed.front());
	Pressed.pop_front();
	return Next;
}

Weltall::EInputFamily UKeyListenerSubsystem::LastFamily() const
{
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	return Glyphs != nullptr ? Weltall::FamilyOfIconSet(Glyphs->IconSet()) : Weltall::EInputFamily::KeyboardAndMouse;
}

Weltall::EButtonIconSet UKeyListenerSubsystem::GamepadIcons() const
{
	const UButtonGlyphSubsystem* Glyphs = GetGameInstance()->GetSubsystem<UButtonGlyphSubsystem>();
	if (Glyphs != nullptr && Glyphs->IsTouchInUse())
	{
		return Weltall::EButtonIconSet::PlayStation;
	}
	return Glyphs != nullptr ? Glyphs->GamepadIconSet() : Weltall::EButtonIconSet::Xbox;
}

Weltall::FPadState UKeyListenerSubsystem::Filter(Weltall::FPadState Held)
{
	if (bListening)
	{
		bWithheld = true;
		return {};
	}
	if (bWithheld && Held.Buttons != 0)
	{
		return {};
	}
	bWithheld = false;
	return Held;
}

void UKeyListenerSubsystem::Heard(const FKey& Key, int32 Axis)
{
	Pressed.push_back({TCHAR_TO_UTF8(*Key.GetFName().ToString()), Axis});
}

void UKeyListenerSubsystem::HeardChord(const FKey& Modifier, const FKey& Trigger)
{
	const FString Chord = Modifier.GetFName().ToString() + TEXT("+") + Trigger.GetFName().ToString();
	Pressed.push_back({TCHAR_TO_UTF8(*Chord), 0});
}

void UKeyListenerSubsystem::TouchButtonsChanged(Weltall::FTouchPad& Pad, uint16 Before, const Weltall::FControlBindings& Bindings)
{
	const uint16 Held = Pad.HeldButtons();
	const uint16 StillHeld = Pad.Unsuppressed(static_cast<uint16>(Before & Held));
	const uint16 PressedNow = static_cast<uint16>(Held & ~Before);
	PendingTouch &= StillHeld;
	// A diagonal presses two directions at once, so it has no ordered trigger.
	if (!std::has_single_bit(PressedNow))
	{
		return;
	}
	if (bListening)
	{
		if (bCaptureTouchChords)
		{
			if (PendingTouch != 0)
			{
				HeardChord(TouchKey(Pad, PendingTouch), TouchKey(Pad, PressedNow));
				ForgetTouch();
			}
			else
			{
				PendingTouch = PressedNow;
			}
		}
		return;
	}
	if (StillHeld == 0 || bWithheld)
	{
		return;
	}
	TSet<FKey> Modifiers;
	for (uint16 Remaining = StillHeld; Remaining != 0; Remaining &= static_cast<uint16>(Remaining - 1))
	{
		Modifiers.Add(TouchKey(Pad, static_cast<uint16>(1u << std::countr_zero(Remaining))));
	}
	if (TriggerChord(TouchKey(Pad, PressedNow), Modifiers, Bindings))
	{
		Pad.SuppressButtons(PressedNow);
	}
}

const Weltall::FControlBindings& UKeyListenerSubsystem::Bindings() const
{
	return GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls;
}

bool UKeyListenerSubsystem::TriggerChord(const FKey& Trigger, const TSet<FKey>& Held, const Weltall::FControlBindings& Bindings)
{
	if (bWithheld)
	{
		return false;
	}
	for (int32 Index = 0; Index < Weltall::PortActionCount; ++Index)
	{
		const auto Action = static_cast<Weltall::EControlAction>(static_cast<int32>(Weltall::EControlAction::SaveState) + Index);
		for (const auto& Binding : Bindings.Keys(Action))
		{
			const auto Chord = Weltall::ControllerChordOf(Binding);
			if (Chord && Trigger.GetFName() == FName(static_cast<int32>(Chord->Trigger.size()), Chord->Trigger.data())
				&& Held.Contains(FKey(FName(static_cast<int32>(Chord->Modifier.size()), Chord->Modifier.data()))))
			{
				ChordsPressed |= static_cast<uint8>(1u << Index);
				return true;
			}
		}
	}
	return false;
}

TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>> UKeyListenerSubsystem::TakeChords()
{
	TArray<Weltall::EControlAction, TFixedAllocator<Weltall::PortActionCount>> Actions;
	for (int32 Index = 0; Index < Weltall::PortActionCount; ++Index)
	{
		if ((ChordsPressed & (1u << Index)) != 0)
		{
			Actions.Add(static_cast<Weltall::EControlAction>(static_cast<int32>(Weltall::EControlAction::SaveState) + Index));
		}
	}
	ChordsPressed = 0;
	return Actions;
}

void UKeyListenerSubsystem::DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
{
	if (State == EInputDeviceConnectionState::Disconnected)
	{
		ForgetController(Device);
	}
}

void UKeyListenerSubsystem::ForgetController(FInputDeviceId Device)
{
	if (Processor)
	{
		Processor->Forget(Device);
	}
}

void UKeyListenerSubsystem::AppActivationChanged(bool bActive)
{
	if (!bActive)
	{
		if (Processor)
		{
			Processor->ResetControllers();
		}
		ChordsPressed = 0;
		ForgetTouch();
		if (UTouchControlsSubsystem* Touch = GetGameInstance()->GetSubsystem<UTouchControlsSubsystem>())
		{
			Touch->ResetTouches();
		}
	}
}
