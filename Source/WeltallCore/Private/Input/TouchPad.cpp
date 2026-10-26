#include "Input/TouchPad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace Weltall
{
	namespace
	{
		/** In display heights: the cross and the face buttons' cluster sit this far in from the sides, low down. */
		constexpr float ClusterInset = 0.24f;
		constexpr float ClusterY = 0.70f;
		constexpr float CrossRadius = 0.17f;
		constexpr float FaceSpacing = 0.12f;
		constexpr float FaceRadius = 0.075f;
		/** The shoulder buttons near the top corners, the second pair below the first; Select and Start beside them. */
		constexpr float ShoulderInset = 0.10f;
		constexpr float UpperShoulderY = 0.09f;
		constexpr float LowerShoulderY = 0.26f;
		constexpr float ShoulderRadius = 0.065f;
		constexpr float MiddleInset = 0.30f;
		constexpr float MiddleRadius = 0.05f;
		/** A direction counts once the finger's offset along its axis passes half the offset's length (30 degrees off it). */
		constexpr float DirectionShare = 0.5f;

		float Distance(float X, float Y, const FTouchControl& Control)
		{
			return std::hypot(X - Control.X, Y - Control.Y);
		}

		/** The engine's gamepad key at each DualShock button's place, by the button's bit: what the bindings name it by. */
		constexpr std::array<std::string_view, 16> GamepadKeys{"Gamepad_Special_Left", "Gamepad_LeftThumbstick", "Gamepad_RightThumbstick",
			"Gamepad_Special_Right", "Gamepad_DPad_Up", "Gamepad_DPad_Right", "Gamepad_DPad_Down", "Gamepad_DPad_Left", "Gamepad_LeftTrigger",
			"Gamepad_RightTrigger", "Gamepad_LeftShoulder", "Gamepad_RightShoulder", "Gamepad_FaceButton_Top", "Gamepad_FaceButton_Right",
			"Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Left"};
		/** The same with the left stick's directions in the d-pad's places: the keys the stick layout's directions press. */
		constexpr std::array<std::string_view, 16> StickKeys{"Gamepad_Special_Left", "Gamepad_LeftThumbstick", "Gamepad_RightThumbstick",
			"Gamepad_Special_Right", "Gamepad_LeftStick_Up", "Gamepad_LeftStick_Right", "Gamepad_LeftStick_Down", "Gamepad_LeftStick_Left",
			"Gamepad_LeftTrigger", "Gamepad_RightTrigger", "Gamepad_LeftShoulder", "Gamepad_RightShoulder", "Gamepad_FaceButton_Top",
			"Gamepad_FaceButton_Right", "Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Left"};
		constexpr uint16 Directions = 0xF0;

		/** A push from -1 to 1 as the report's byte, 0x80 its centre. */
		uint8 ReportByte(float Push)
		{
			return static_cast<uint8>(std::clamp(static_cast<int32>(std::floor(FPadState::StickCentre + Push * 128.0f)), 0, 0xFF));
		}
	}

	FTouchPad FTouchPad::Standard(float Aspect, ETouchMovement Movement)
	{
		FTouchPad Pad;
		const float Right = Aspect - ClusterInset;
		const ETouchControlKind Mover = Movement == ETouchMovement::Stick ? ETouchControlKind::Stick : ETouchControlKind::DirectionPad;
		Pad.ControlList = {
			{Mover, EPadButton::Up, ClusterInset, ClusterY, CrossRadius},
			{ETouchControlKind::Button, EPadButton::Triangle, Right, ClusterY - FaceSpacing, FaceRadius},
			{ETouchControlKind::Button, EPadButton::Circle, Right + FaceSpacing, ClusterY, FaceRadius},
			{ETouchControlKind::Button, EPadButton::Cross, Right, ClusterY + FaceSpacing, FaceRadius},
			{ETouchControlKind::Button, EPadButton::Square, Right - FaceSpacing, ClusterY, FaceRadius},
			{ETouchControlKind::Button, EPadButton::L2, ShoulderInset, UpperShoulderY, ShoulderRadius},
			{ETouchControlKind::Button, EPadButton::L1, ShoulderInset, LowerShoulderY, ShoulderRadius},
			{ETouchControlKind::Button, EPadButton::R2, Aspect - ShoulderInset, UpperShoulderY, ShoulderRadius},
			{ETouchControlKind::Button, EPadButton::R1, Aspect - ShoulderInset, LowerShoulderY, ShoulderRadius},
			{ETouchControlKind::Button, EPadButton::Select, MiddleInset, UpperShoulderY, MiddleRadius},
			{ETouchControlKind::Button, EPadButton::Start, Aspect - MiddleInset, UpperShoulderY, MiddleRadius},
		};
		return Pad;
	}

	bool FTouchPad::Press(uint64 Finger, float X, float Y)
	{
		Release(Finger);
		FFinger Pressed{Finger, X, Y, false};
		Pressed.bSteers = !ControlList.empty() && Distance(X, Y, Mover()) <= Mover().Radius * ButtonReach;
		Fingers.push_back(Pressed);
		return Pressed.bSteers || ButtonsOf(Pressed) != 0;
	}

	void FTouchPad::Move(uint64 Finger, float X, float Y)
	{
		const auto Found = std::find_if(Fingers.begin(), Fingers.end(), [Finger](const FFinger& Each) { return Each.Id == Finger; });
		if (Found != Fingers.end())
		{
			Latched |= ButtonsOf(*Found);
			Found->X = X;
			Found->Y = Y;
		}
	}

	void FTouchPad::Release(uint64 Finger)
	{
		for (const FFinger& Each : Fingers)
		{
			Latched |= Each.Id == Finger ? ButtonsOf(Each) : 0;
		}
		std::erase_if(Fingers, [Finger](const FFinger& Each) { return Each.Id == Finger; });
	}

	uint16 FTouchPad::ButtonsOf(const FFinger& Finger) const
	{
		if (Finger.bSteers)
		{
			const FTouchControl& Cross = Mover();
			const float Dx = Finger.X - Cross.X;
			const float Dy = Finger.Y - Cross.Y;
			const float Length = std::hypot(Dx, Dy);
			if (Length <= Cross.Radius * DirectionDeadZone)
			{
				return 0;
			}
			// The stick's directions are each axis's past the dead zone, as a gamepad's stick bindings take them.
			const float Share = Cross.Kind == ETouchControlKind::Stick ? Cross.Radius * DirectionDeadZone : Length * DirectionShare;
			uint16 Buttons = 0;
			Buttons |= Dx > Share ? FPadState::Bit(EPadButton::Right) : 0;
			Buttons |= Dx < -Share ? FPadState::Bit(EPadButton::Left) : 0;
			Buttons |= Dy > Share ? FPadState::Bit(EPadButton::Down) : 0;
			Buttons |= Dy < -Share ? FPadState::Bit(EPadButton::Up) : 0;
			return Buttons;
		}
		// The nearest button within reach, by its distance in the button's own radii.
		const FTouchControl* Nearest = nullptr;
		float NearestShare = ButtonReach;
		for (const FTouchControl& Control : ControlList)
		{
			const float Share = Distance(Finger.X, Finger.Y, Control) / Control.Radius;
			if (Control.Kind == ETouchControlKind::Button && Share <= NearestShare)
			{
				Nearest = &Control;
				NearestShare = Share;
			}
		}
		return Nearest != nullptr ? FPadState::Bit(Nearest->Button) : 0;
	}

	uint16 FTouchPad::HeldButtons() const
	{
		uint16 Buttons = 0;
		for (const FFinger& Finger : Fingers)
		{
			Buttons |= ButtonsOf(Finger);
		}
		return Buttons;
	}

	uint16 FTouchPad::TakeButtons()
	{
		const uint16 Buttons = static_cast<uint16>(HeldButtons() | Latched);
		Latched = 0;
		return Buttons;
	}

	FPadState FTouchPad::Take(const FControlBindings& Bindings)
	{
		FPadState State;
		const uint16 Buttons = TakeButtons();
		const bool bStick = HasStick();
		const std::array<std::string_view, 16>& Keys = bStick ? StickKeys : GamepadKeys;
		for (std::size_t Bit = 0; Bit < Keys.size(); ++Bit)
		{
			if ((Buttons & (1u << Bit)) == 0)
			{
				continue;
			}
			if (const std::optional<FBoundPlace> Place = Bindings.Find(Keys[Bit]))
			{
				if (const std::optional<EPadButton> Pressed = PadButtonOf(Place->Action))
				{
					State.Buttons |= FPadState::Bit(*Pressed);
					State.StickButtons |= bStick && (Directions & (1u << Bit)) != 0 ? FPadState::Bit(*Pressed) : 0;
				}
			}
		}
		if (bStick)
		{
			State.bAnalog = true;
			if (const std::optional<std::array<float, 2>> Push = StickPush())
			{
				State.StickX = ReportByte((*Push)[0]);
				State.StickY = ReportByte((*Push)[1]);
			}
		}
		return State;
	}

	std::optional<std::array<float, 2>> FTouchPad::StickPush() const
	{
		if (!HasStick())
		{
			return std::nullopt;
		}
		const auto Steering = std::find_if(Fingers.begin(), Fingers.end(), [](const FFinger& Finger) { return Finger.bSteers; });
		if (Steering == Fingers.end())
		{
			return std::nullopt;
		}
		const FTouchControl& Stick = Mover();
		const float Dx = (Steering->X - Stick.X) / Stick.Radius;
		const float Dy = (Steering->Y - Stick.Y) / Stick.Radius;
		const float Length = std::hypot(Dx, Dy);
		const float Scale = Length > 1.0f ? 1.0f / Length : 1.0f;
		return std::array<float, 2>{Dx * Scale, Dy * Scale};
	}

	uint16 FTouchPad::HeldOn(std::size_t Control) const
	{
		const FTouchControl& Shown = ControlList[Control];
		uint16 Held = 0;
		for (const FFinger& Finger : Fingers)
		{
			const uint16 Buttons = ButtonsOf(Finger);
			if (Shown.Kind != ETouchControlKind::Button ? Finger.bSteers : (!Finger.bSteers && Buttons == FPadState::Bit(Shown.Button)))
			{
				Held |= Buttons;
			}
		}
		return Held;
	}
}
