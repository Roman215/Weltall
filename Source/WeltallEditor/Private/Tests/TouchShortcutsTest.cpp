#include "Engine/GameInstance.h"
#include "Input/KeyListenerSubsystem.h"
#include "Input/TouchPad.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTouchShortcutsTest, "Weltall.Input.TouchButtonsBindAndRunOrderedShortcuts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTouchShortcutsTest::RunTest(const FString&)
{
	using namespace Weltall;
	UKeyListenerSubsystem* Listener = NewObject<UKeyListenerSubsystem>(NewObject<UGameInstance>());
	FTouchPad Pad = FTouchPad::Standard(20.0f / 9.0f);
	FControlBindings Bindings = FControlBindings::Defaults();
	const auto Change = [&](const auto& Update)
	{
		const uint16 Before = Pad.HeldButtons();
		Update();
		Listener->TouchButtonsChanged(Pad, Before, Bindings);
	};
	const auto Down = [&](uint64 Finger, EPadButton Button)
	{
		for (const FTouchControl& Control : Pad.Controls())
		{
			if (Control.Kind == ETouchControlKind::Button && Control.Button == Button)
			{
				Change([&] { Pad.Press(Finger, Control.X, Control.Y); });
				return;
			}
		}
		AddError(TEXT("Missing touch control"));
	};
	const auto Up = [&](uint64 Finger) { Change([&] { Pad.Release(Finger); }); };

	Listener->Listen(true, true);
	Down(1, EPadButton::Select);
	Up(1);
	Down(2, EPadButton::R1);
	Up(2);
	TestFalse(TEXT("Two separate taps do not bind"), Listener->TakePressed().has_value());
	Down(1, EPadButton::Select);
	Down(2, EPadButton::R1);
	const auto Captured = Listener->TakePressed();
	if (!TestTrue(TEXT("Two held touch buttons were captured"), Captured.has_value()))
	{
		return false;
	}
	TestTrue(TEXT("The shortcut uses the standard gamepad binding"), Captured->Key == "Gamepad_Special_Left+Gamepad_RightShoulder");
	Bindings.SetKeys(EControlAction::SaveState, {Captured->Key});
	Bindings.SetKeys(EControlAction::LoadState, {"Gamepad_Special_Left+Gamepad_LeftShoulder"});
	TestTrue(TEXT("Binding the shortcut does not run it"), Listener->TakeChords().IsEmpty());

	Listener->Listen(false);
	Up(1);
	Up(2);
	Listener->Filter(Pad.Take(Bindings));
	Listener->Filter(Pad.Take(Bindings));
	Down(2, EPadButton::R1);
	Down(1, EPadButton::Select);
	TestTrue(TEXT("Reversed order does not run the shortcut"), Listener->TakeChords().IsEmpty());
	TestTrue(TEXT("R1 still reaches gameplay in reverse order"), Pad.Take(Bindings).IsHeld(EPadButton::R1));
	Up(2);
	Pad.Take(Bindings);

	Down(2, EPadButton::R1);
	auto Actions = Listener->TakeChords();
	TestTrue(TEXT("The ordered pair saves"), Actions.Num() == 1 && Actions[0] == EControlAction::SaveState);
	TestEqual(TEXT("Only the modifier reaches gameplay"), Pad.Take(Bindings).Buttons, FPadState::Bit(EPadButton::Select));
	TestTrue(TEXT("Holding the pair does not repeat"), Listener->TakeChords().IsEmpty());
	Up(2);
	TestFalse(TEXT("Releasing the trigger does not leak it to gameplay"), Pad.Take(Bindings).IsHeld(EPadButton::R1));
	Down(3, EPadButton::L1);
	Up(3);
	Actions = Listener->TakeChords();
	TestTrue(TEXT("A trigger tapped within one frame can load"), Actions.Num() == 1 && Actions[0] == EControlAction::LoadState);
	TestFalse(TEXT("A quick trigger tap does not leak to gameplay"), Pad.Take(Bindings).IsHeld(EPadButton::L1));
	return true;
}

#endif
