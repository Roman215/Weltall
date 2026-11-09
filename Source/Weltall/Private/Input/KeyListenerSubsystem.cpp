#include "Input/KeyListenerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "InputCoreTypes.h"
#include "Misc/CoreDelegates.h"
#include "Shell/PlayerSettingsSubsystem.h"

namespace
{
	/** How far a stick is pushed, and how far the mouse travels (in its counts), before it is heard as moved. */
	constexpr float StickPushed = 0.5f;
	constexpr float StickBack = 0.25f;
	constexpr double MouseTravel = 40.0;
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
		const auto& Mapper = IPlatformInputDeviceMapper::Get();
		if (Mapper.GetUserForInputDevice(Device) != Mapper.GetPrimaryPlatformUser())
		{
			Forget(Device);
			return false;
		}
		FControllerButtons& Buttons = Controllers.FindOrAdd(Device);
		if (Buttons.Consumed.Contains(Key))
		{
			return true;
		}
		if (Event.IsRepeat() || Buttons.Held.Contains(Key))
		{
			return false;
		}
		if (!Keys->IsListening() && !Buttons.Held.IsEmpty() && Keys->TriggerChord(Key, Buttons.Held))
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
		return false;
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
		return false;
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
		return false;
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
		Hear(Event.GetEffectingButton(), 0);
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

bool UKeyListenerSubsystem::TriggerChord(const FKey& Trigger, const TSet<FKey>& Held)
{
	if (bWithheld)
	{
		return false;
	}
	const auto& Bindings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().Controls;
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
	if (Processor && State == EInputDeviceConnectionState::Disconnected)
	{
		Processor->Forget(Device);
	}
}

void UKeyListenerSubsystem::AppActivationChanged(bool bActive)
{
	if (!bActive && Processor)
	{
		Processor->ResetControllers();
		ChordsPressed = 0;
	}
}
