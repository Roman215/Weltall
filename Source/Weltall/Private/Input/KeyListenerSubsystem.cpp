#include "Input/KeyListenerSubsystem.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/ButtonGlyphSubsystem.h"
#include "InputCoreTypes.h"

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
	void Restart()
	{
		Travel = FVector2D::ZeroVector;
		Pushed.Reset();
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& Application, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& Application, const FKeyEvent& Event) override
	{
		if (!Event.IsRepeat() && !UButtonGlyphSubsystem::IsPhoneButton(Event.GetKey()))
		{
			Hear(Event.GetKey(), 0);
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
};

void UKeyListenerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UButtonGlyphSubsystem>();
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FKeyListenProcessor>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}

void UKeyListenerSubsystem::Deinitialize()
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
	Super::Deinitialize();
}

void UKeyListenerSubsystem::Listen(bool bOn)
{
	bListening = bOn;
	Pressed.clear();
	if (bOn && Processor)
	{
		Processor->Restart();
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
