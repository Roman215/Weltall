#include "Input/TouchPad.h"

#include <catch_amalgamated.hpp>

#include <cmath>

using namespace Weltall;

namespace
{
	constexpr float PhoneAspect = 20.0f / 9.0f;

	const FTouchControl& ControlOf(const FTouchPad& Pad, ETouchControlKind Kind, EPadButton Button = EPadButton::Up)
	{
		for (const FTouchControl& Control : Pad.Controls())
		{
			if (Control.Kind == Kind && (Kind != ETouchControlKind::Button || Control.Button == Button))
			{
				return Control;
			}
		}
		FAIL("no such control");
		return Pad.Controls().front();
	}

	/** The buttons one finger holds at a point. */
	uint16 HeldAt(float X, float Y)
	{
		FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
		Pad.Press(1, X, Y);
		return Pad.HeldButtons();
	}

	constexpr uint16 Bit(EPadButton Button) { return FPadState::Bit(Button); }
}

TEST_CASE("The touch pad lies on a phone's screen beside a 4:3 picture, its controls drawn apart", "[Input]")
{
	const FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	REQUIRE(Pad.Controls().size() == 11);
	const float Bar = (PhoneAspect - 4.0f / 3.0f) / 2.0f;
	for (const FTouchControl& Control : Pad.Controls())
	{
		CHECK(Control.Y - Control.Radius >= 0.0f);
		CHECK(Control.Y + Control.Radius <= 1.0f);
		CHECK(Control.X - Control.Radius >= 0.0f);
		CHECK(Control.X + Control.Radius <= PhoneAspect);
		CHECK((Control.X + Control.Radius <= Bar || Control.X - Control.Radius >= PhoneAspect - Bar));
		for (const FTouchControl& Other : Pad.Controls())
		{
			if (&Other != &Control)
			{
				CHECK(std::hypot(Control.X - Other.X, Control.Y - Other.Y) > Control.Radius + Other.Radius);
			}
		}
	}
	// The face buttons stand as on the pad: triangle above, cross below, square left, circle right.
	const FTouchControl& Triangle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Triangle);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Cross);
	const FTouchControl& Square = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Square);
	const FTouchControl& Circle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Circle);
	CHECK(Triangle.Y < Square.Y);
	CHECK(Cross.Y > Square.Y);
	CHECK(Square.X < Triangle.X);
	CHECK(Circle.X > Triangle.X);
}

TEST_CASE("A finger holds the button under it, a little past its edge, and nothing elsewhere", "[Input]")
{
	const FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	for (const FTouchControl& Control : Pad.Controls())
	{
		if (Control.Kind == ETouchControlKind::Button)
		{
			INFO(static_cast<int32>(Control.Button));
			CHECK(HeldAt(Control.X, Control.Y) == Bit(Control.Button));
			CHECK(HeldAt(Control.X + Control.Radius * 1.2f, Control.Y) == Bit(Control.Button));
		}
	}
	CHECK(HeldAt(PhoneAspect / 2.0f, 0.5f) == 0);
	const FTouchControl& Circle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Circle);
	CHECK(HeldAt(Circle.X + Circle.Radius * 1.3f, Circle.Y) == 0);
}

TEST_CASE("The cross holds directions by the finger's angle, diagonals between, none at its centre", "[Input]")
{
	const FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::DirectionPad);
	const auto At = [&](float Degrees, float Share)
	{
		const float Radians = Degrees * 3.14159265f / 180.0f;
		return HeldAt(Cross.X + std::cos(Radians) * Cross.Radius * Share, Cross.Y - std::sin(Radians) * Cross.Radius * Share);
	};
	CHECK(At(0.0f, 0.8f) == Bit(EPadButton::Right));
	CHECK(At(25.0f, 0.8f) == Bit(EPadButton::Right));
	CHECK(At(45.0f, 0.8f) == (Bit(EPadButton::Right) | Bit(EPadButton::Up)));
	CHECK(At(90.0f, 0.8f) == Bit(EPadButton::Up));
	CHECK(At(135.0f, 0.8f) == (Bit(EPadButton::Left) | Bit(EPadButton::Up)));
	CHECK(At(180.0f, 0.8f) == Bit(EPadButton::Left));
	CHECK(At(225.0f, 0.8f) == (Bit(EPadButton::Left) | Bit(EPadButton::Down)));
	CHECK(At(270.0f, 0.8f) == Bit(EPadButton::Down));
	CHECK(At(315.0f, 0.8f) == (Bit(EPadButton::Right) | Bit(EPadButton::Down)));
	CHECK(At(90.0f, 0.1f) == 0);
}

TEST_CASE("A finger on the cross steers it wherever it slides; another slides from button to button", "[Input]")
{
	FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::DirectionPad);
	const FTouchControl& Square = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Square);
	const FTouchControl& Triangle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Triangle);
	Pad.Press(7, Cross.X, Cross.Y - Cross.Radius * 0.5f);
	Pad.Press(9, Square.X, Square.Y);
	CHECK(Pad.HeldButtons() == (Bit(EPadButton::Up) | Bit(EPadButton::Square)));

	// The steering finger drifts far past the cross, still up; the other slides onto the triangle, then off everything.
	Pad.Move(7, Cross.X, Cross.Y - Cross.Radius * 3.0f);
	Pad.Move(9, Triangle.X, Triangle.Y);
	CHECK(Pad.HeldButtons() == (Bit(EPadButton::Up) | Bit(EPadButton::Triangle)));
	CHECK(Pad.HeldOn(0) == Bit(EPadButton::Up));
	const std::size_t TriangleAt = static_cast<std::size_t>(&Triangle - Pad.Controls().data());
	CHECK(Pad.HeldOn(TriangleAt) == Bit(EPadButton::Triangle));
	Pad.Move(9, PhoneAspect / 2.0f, 0.5f);
	CHECK(Pad.HeldButtons() == Bit(EPadButton::Up));

	// A finger landing beside the buttons and sliding onto one presses it; lifting lets go.
	Pad.Move(9, Square.X, Square.Y);
	CHECK(Pad.HeldButtons() == (Bit(EPadButton::Up) | Bit(EPadButton::Square)));
	Pad.Release(7);
	CHECK(Pad.HeldButtons() == Bit(EPadButton::Square));
	Pad.ReleaseAll();
	CHECK(Pad.HeldButtons() == 0);
}

TEST_CASE("The touch pad's buttons press the game's as a gamepad's at their places do, through the player's bindings", "[Input]")
{
	FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	const FTouchControl& Circle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Circle);
	const FTouchControl& Start = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Start);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::DirectionPad);
	Pad.Press(1, Circle.X, Circle.Y);
	Pad.Press(2, Start.X, Start.Y);
	Pad.Press(3, Cross.X - Cross.Radius * 0.8f, Cross.Y);
	const uint16 Expected = Bit(EPadButton::Circle) | Bit(EPadButton::Start) | Bit(EPadButton::Left);
	CHECK(Pad.HeldButtons() == Expected);
	CHECK(Pad.Take(FControlBindings::Defaults()).Buttons == Expected);

	// With Circle confirms, the button at the right confirms: the game's Cross.
	CHECK(Pad.Take(FControlBindings::Defaults(EButtonLayout::CircleConfirms)).Buttons == (Bit(EPadButton::Cross) | Bit(EPadButton::Start) | Bit(EPadButton::Left)));

	// A player's own binding moves it; a key bound to nothing presses nothing.
	FControlBindings Own = FControlBindings::Defaults();
	Own.SetKeys(EControlAction::Triangle, {"Gamepad_FaceButton_Right"});
	CHECK(Pad.Take(Own).Buttons == (Bit(EPadButton::Triangle) | Bit(EPadButton::Start) | Bit(EPadButton::Left)));
	Own.Unbind("Gamepad_Special_Right");
	CHECK(Pad.Take(Own).Buttons == (Bit(EPadButton::Triangle) | Bit(EPadButton::Left)));
}

TEST_CASE("A tap shorter than the game's frame, or a slide through a button, is still a press once", "[Input]")
{
	FTouchPad Pad = FTouchPad::Standard(PhoneAspect);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Cross);
	const FTouchControl& Square = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Square);
	const FTouchControl& Triangle = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Triangle);
	Pad.Press(1, Cross.X, Cross.Y);
	Pad.Release(1);
	CHECK(Pad.HeldButtons() == 0);
	CHECK(Pad.TakeButtons() == Bit(EPadButton::Cross));
	CHECK(Pad.TakeButtons() == 0);

	Pad.Press(2, Square.X, Square.Y);
	Pad.Move(2, Triangle.X, Triangle.Y);
	CHECK(Pad.TakeButtons() == (Bit(EPadButton::Square) | Bit(EPadButton::Triangle)));
	CHECK(Pad.TakeButtons() == Bit(EPadButton::Triangle));
}

TEST_CASE("The stick layout puts a stick in the cross's place, a DualShock's left stick whose rim is a full push", "[Input]")
{
	const FTouchPad Crossed = FTouchPad::Standard(PhoneAspect);
	FTouchPad Pad = FTouchPad::Standard(PhoneAspect, ETouchMovement::Stick);
	REQUIRE(Pad.Controls().size() == Crossed.Controls().size());
	const FTouchControl& Stick = ControlOf(Pad, ETouchControlKind::Stick);
	const FTouchControl& Cross = ControlOf(Crossed, ETouchControlKind::DirectionPad);
	CHECK(Stick.X == Cross.X);
	CHECK(Stick.Y == Cross.Y);
	CHECK(Stick.Radius == Cross.Radius);
	CHECK_FALSE(Pad.StickPush().has_value());
	const FControlBindings Bindings = FControlBindings::Defaults();

	// Half way up and a little right: of its axes, only the upward one is past the dead zone.
	REQUIRE(Pad.Press(1, Stick.X + Stick.Radius * 0.1f, Stick.Y - Stick.Radius * 0.5f));
	const FPadState Up = Pad.Take(Bindings);
	CHECK(Up.bAnalog);
	CHECK(Up.Buttons == Bit(EPadButton::Up));
	CHECK(Up.StickButtons == Bit(EPadButton::Up));
	CHECK(Up.StickX == 0x8C);
	CHECK(Up.StickY == 0x40);

	// Dragged far past the rim to the right: a full push that way, the knob kept at the rim.
	Pad.Move(1, Stick.X + Stick.Radius * 3.0f, Stick.Y);
	Pad.Take(Bindings);
	const FPadState Right = Pad.Take(Bindings);
	CHECK(Right.Buttons == Bit(EPadButton::Right));
	CHECK(Right.StickX == 0xFF);
	CHECK(Right.StickY == FPadState::StickCentre);
	REQUIRE(Pad.StickPush().has_value());
	CHECK((*Pad.StickPush())[0] == Catch::Approx(1.0f));
	CHECK(Pad.HeldOn(0) == Bit(EPadButton::Right));

	// Let go: the stick at rest, still an analog controller's.
	Pad.Release(1);
	Pad.Take(Bindings);
	const FPadState Rest = Pad.Take(Bindings);
	CHECK(Rest.Buttons == 0);
	CHECK(Rest.bAnalog);
	CHECK(Rest.StickX == FPadState::StickCentre);
	CHECK(Rest.StickY == FPadState::StickCentre);

	// The cross's layout is a digital pad's.
	FTouchPad Digital = FTouchPad::Standard(PhoneAspect);
	Digital.Press(1, Cross.X, Cross.Y - Cross.Radius * 0.5f);
	const FPadState Pressed = Digital.Take(Bindings);
	CHECK(Pressed.Buttons == Bit(EPadButton::Up));
	CHECK_FALSE(Pressed.bAnalog);
	CHECK(Pressed.StickButtons == 0);
}

TEST_CASE("A finger put down tells whether it landed on a control, which the device's vibration answers", "[Input]")
{
	FTouchPad Pad = FTouchPad::Standard(PhoneAspect, ETouchMovement::Stick);
	const FTouchControl& Stick = ControlOf(Pad, ETouchControlKind::Stick);
	const FTouchControl& Cross = ControlOf(Pad, ETouchControlKind::Button, EPadButton::Cross);
	CHECK(Pad.Press(1, Stick.X, Stick.Y));
	CHECK(Pad.Press(2, Cross.X, Cross.Y));
	CHECK_FALSE(Pad.Press(3, PhoneAspect / 2.0f, 0.5f));
}
