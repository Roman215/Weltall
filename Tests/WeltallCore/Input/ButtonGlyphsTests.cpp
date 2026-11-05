#include "Input/ButtonGlyphs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("A device's icon set follows its name", "[Input]")
{
	CHECK(IconSetOfDevice(false, "KBM") == EButtonIconSet::Keyboard);
	CHECK(IconSetOfDevice(true, "DualSense Wireless Controller") == EButtonIconSet::PlayStation);
	CHECK(IconSetOfDevice(true, "PS4 Controller") == EButtonIconSet::PlayStation);
	CHECK(IconSetOfDevice(true, "Nintendo Switch Pro Controller") == EButtonIconSet::Nintendo);
	CHECK(IconSetOfDevice(true, "Steam Deck") == EButtonIconSet::SteamDeck);
	CHECK(IconSetOfDevice(true, "XboxOne") == EButtonIconSet::Xbox);
	CHECK(IconSetOfDevice(true, "Hid") == EButtonIconSet::Xbox);
}

TEST_CASE("Controller symbols can override the driver's family while the bindings keep their places", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	const EButtonIconSet Detected = IconSetOfDevice(true, "XboxOne");
	CHECK(ControllerIconSet(EControllerIcons::Automatic, Detected) == EButtonIconSet::Xbox);
	CHECK(ControllerIconSet(EControllerIcons::Xbox, EButtonIconSet::Nintendo) == EButtonIconSet::Xbox);
	CHECK(ControllerIconSet(EControllerIcons::PlayStation, Detected) == EButtonIconSet::PlayStation);
	CHECK(ControllerIconSet(EControllerIcons::SteamDeck, Detected) == EButtonIconSet::SteamDeck);
	const EButtonIconSet Nintendo = ControllerIconSet(EControllerIcons::Nintendo, Detected);
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, Nintendo).Icon == "B");
	CHECK(ButtonGlyph(EPadButton::Circle, Bindings, Nintendo).Icon == "A");
	CHECK(ButtonGlyph(EPadButton::Square, Bindings, Nintendo).Icon == "Y");
	CHECK(ButtonGlyph(EPadButton::Triangle, Bindings, Nintendo).Icon == "X");
	CHECK(ButtonGlyph(EPadButton::L2, Bindings, Nintendo).Icon == "ZL");
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, Nintendo).Icon == "A");
	Bindings.bOriginalButtonIcons = true;
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, Nintendo).Icon == "Cross");
}

TEST_CASE("A PlayStation button's glyph is its binding on the device the player uses", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Xbox) == FButtonGlyph{EButtonIconSet::Xbox, "Gamepad_FaceButton_Bottom", "A"});
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::PlayStation).Icon == "Cross");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Nintendo).Icon == "B");
	CHECK(ButtonGlyph(EPadButton::Triangle, Bindings, EButtonIconSet::SteamDeck).Icon == "Y");
	CHECK(ButtonGlyph(EPadButton::Start, Bindings, EButtonIconSet::PlayStation).Icon == "Options");
	CHECK(ButtonGlyph(EPadButton::Up, Bindings, EButtonIconSet::Xbox).Icon == "DPad_Up");
	CHECK(ButtonGlyph(EPadButton::L2, Bindings, EButtonIconSet::Keyboard) == FButtonGlyph{EButtonIconSet::Keyboard, "One", "1"});
	CHECK(ButtonGlyph(EPadButton::Select, Bindings, EButtonIconSet::Keyboard).Icon == "Backspace");

	Bindings.Bind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::PlayStation).Icon == "Circle");
	const FButtonGlyph Unbound = ButtonGlyph(EPadButton::Circle, Bindings, EButtonIconSet::PlayStation);
	CHECK(Unbound.Key.empty());
	CHECK(Unbound.Icon.empty());
	CHECK(ButtonGlyph(EPadButton::L3, Bindings, EButtonIconSet::Xbox).Key.empty());
}

TEST_CASE("A face button's place among the four is the one its controller prints the icon at", "[Input]")
{
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Cross") == EFaceButtonPlace::Bottom);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Circle") == EFaceButtonPlace::Right);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Square") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Triangle") == EFaceButtonPlace::Top);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "A") == EFaceButtonPlace::Bottom);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "Y") == EFaceButtonPlace::Top);
	// Nintendo's letters sit across from Xbox's.
	CHECK(FaceButtonPlace(EButtonIconSet::Nintendo, "A") == EFaceButtonPlace::Right);
	CHECK(FaceButtonPlace(EButtonIconSet::Nintendo, "Y") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::SteamDeck, "X") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "LB") == EFaceButtonPlace::None);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "A") == EFaceButtonPlace::None);
	CHECK(FaceButtonPlace(EButtonIconSet::Keyboard, "A") == EFaceButtonPlace::None);
}

TEST_CASE("Original button icons keep the PlayStation symbols independently of devices and bindings", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	Bindings.Rebind(EControlAction::Cross, 0, "Z");
	Bindings.Clear(EControlAction::Triangle, EInputFamily::Gamepad, 0);
	Bindings.Clear(EControlAction::Triangle, EInputFamily::KeyboardAndMouse, 0);
	const FControlBindings Rebound = Bindings;
	Bindings.bOriginalButtonIcons = true;
	for (const EButtonIconSet Set : {EButtonIconSet::Keyboard, EButtonIconSet::Xbox, EButtonIconSet::PlayStation,
		EButtonIconSet::Nintendo, EButtonIconSet::SteamDeck})
	{
		INFO(static_cast<int32>(Set));
		const FButtonGlyph Cross = ButtonGlyph(EPadButton::Cross, Bindings, Set);
		CHECK(Cross.Set == EButtonIconSet::PlayStation);
		CHECK(Cross.Icon == "Cross");
		CHECK(ButtonGlyph(EPadButton::Circle, Bindings, Set).Icon == "Circle");
		CHECK(ButtonGlyph(EPadButton::Triangle, Bindings, Set).Icon == "Triangle");
		CHECK(ButtonGlyph(EPadButton::L1, Bindings, Set).Icon == "L1");
		CHECK(ButtonGlyph(EPadButton::L2, Bindings, Set).Icon == "L2");
		CHECK(ButtonGlyph(EPadButton::Up, Bindings, Set).Icon == "DPad_Up");
		CHECK(ButtonGlyph(EPadButton::Start, Bindings, Set).Icon == "Start");
		CHECK(ButtonGlyph(EPadButton::Select, Bindings, Set).Icon == "Select");
	}
	Bindings.bOriginalButtonIcons = false;
	CHECK(Bindings == Rebound);
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Keyboard).Icon == "Z");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Nintendo).Icon == "A");
	CHECK(ButtonGlyph(EPadButton::Triangle, Bindings, EButtonIconSet::Xbox).Icon.empty());
}

TEST_CASE("Original button icons show each button at the place its layout gives it", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.bOriginalButtonIcons = true;
	const auto Icon = [&Bindings](EPadButton Button) { return ButtonGlyph(Button, Bindings, EButtonIconSet::Xbox).Icon; };
	// The game's buttons by the USA's names: Cross confirms, Circle cancels, Square opens the menu, Triangle jumps.
	Bindings.ChangeButtonLayout(EButtonLayout::CircleConfirms);
	CHECK(Icon(EPadButton::Cross) == "Circle");
	CHECK(Icon(EPadButton::Circle) == "Cross");
	CHECK(Icon(EPadButton::Square) == "Square");
	CHECK(Icon(EPadButton::Triangle) == "Triangle");
	Bindings.ChangeButtonLayout(EButtonLayout::Japan);
	CHECK(Icon(EPadButton::Cross) == "Circle");
	CHECK(Icon(EPadButton::Circle) == "Cross");
	CHECK(Icon(EPadButton::Square) == "Triangle");
	CHECK(Icon(EPadButton::Triangle) == "Square");
	CHECK(Icon(EPadButton::L1) == "L1");
	CHECK(Icon(EPadButton::Start) == "Start");
	// Even when the player put confirm elsewhere: the layout's place, as the original icons are.
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_LeftShoulder");
	CHECK(Icon(EPadButton::Cross) == "Circle");

	// Without them each button shows its binding, which the layout moved.
	FControlBindings Bound = FControlBindings::Defaults();
	Bound.ChangeButtonLayout(EButtonLayout::Japan);
	CHECK(ButtonGlyph(EPadButton::Cross, Bound, EButtonIconSet::PlayStation).Icon == "Circle");
	CHECK(ButtonGlyph(EPadButton::Cross, Bound, EButtonIconSet::Xbox).Icon == "B");
	CHECK(ButtonGlyph(EPadButton::Triangle, Bound, EButtonIconSet::PlayStation).Icon == "Square");
	CHECK(ButtonGlyph(EPadButton::Cross, Bound, EButtonIconSet::Keyboard).Icon == "E");
}
