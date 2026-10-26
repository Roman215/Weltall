#pragma once

#include "Input/ControlBindings.h"
#include "Input/PadState.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** What a control of the touch pad is: the cross of directions, one button, or the stick in the cross's place. */
	enum class ETouchControlKind : uint8
	{
		DirectionPad,
		Button,
		Stick,
	};

	/**
	 * A control of the touch pad: where it is drawn and touched, in display heights from the display's top left corner
	 * (a display of width over height Aspect runs from 0 to Aspect across and 0 to 1 down), and the DualShock's button it is.
	 */
	struct FTouchControl
	{
		ETouchControlKind Kind = ETouchControlKind::Button;
		/** The button, for a button. */
		EPadButton Button = EPadButton::Cross;
		float X = 0.0f;
		float Y = 0.0f;
		float Radius = 0.0f;
	};

	/**
	 * A PlayStation controller drawn on a touch screen, the way games on phones show one: the directions' cross at the
	 * left, the four face buttons at the right, the shoulder buttons above them, Select and Start between, in the bars a
	 * 4:3 picture leaves at a phone's sides. Each finger holds what lies under it: a finger that lands on the cross keeps
	 * steering it wherever it goes, by its direction from the cross's centre; any other finger holds the button it is on,
	 * and sliding it from button to button presses each in turn, as the face buttons' sequences of the battles need. Its
	 * buttons are a DualShock's, which press the game's through the player's bindings as a gamepad's do. In the stick's
	 * layout the cross's finger is a DualShock's left stick in analog mode instead: pushed as far as it is from the
	 * stick's centre (its rim a full push), holding the directions the stick's bindings take from it.
	 */
	class FTouchPad
	{
	public:
		/** A finger's touch counts on a button this far past the button's drawn edge, in its radii. */
		static constexpr float ButtonReach = 1.25f;
		/** A finger this near the cross's or the stick's centre, in its radii, holds no direction. */
		static constexpr float DirectionDeadZone = 0.2f;

		/** The controls laid out for a display of width over height Aspect, the party moved by Movement. */
		WELTALLCORE_API static FTouchPad Standard(float Aspect, ETouchMovement Movement = ETouchMovement::Cross);

		/** A finger put down; whether it landed on a control (the cross or the stick, or a button within reach). */
		WELTALLCORE_API bool Press(uint64 Finger, float X, float Y);
		WELTALLCORE_API void Move(uint64 Finger, float X, float Y);
		WELTALLCORE_API void Release(uint64 Finger);
		void ReleaseAll() { Fingers.clear(); }

		/** The DualShock buttons the fingers hold. */
		WELTALLCORE_API uint16 HeldButtons() const;

		/**
		 * The DualShock buttons held since the last take: those the fingers hold, and those a finger held and has lifted or
		 * slid off since, so that a tap shorter than the game's frame is still a press.
		 */
		WELTALLCORE_API uint16 TakeButtons();

		/** TakeButtons as the game's buttons: each as the gamepad key at its place presses through Bindings. */
		WELTALLCORE_API FPadState Take(const FControlBindings& Bindings);

		std::span<const FTouchControl> Controls() const { return ControlList; }

		/** Whether a control (by its place in Controls) is held; for the cross and the stick, which of their directions are held as buttons. */
		WELTALLCORE_API uint16 HeldOn(std::size_t Control) const;

		/** Where the stick is pushed, in its radii from its centre (within its rim; none while no finger holds it). */
		WELTALLCORE_API std::optional<std::array<float, 2>> StickPush() const;

	private:
		struct FFinger
		{
			uint64 Id = 0;
			float X = 0.0f;
			float Y = 0.0f;
			/** It landed on the cross, which it steers until it lifts. */
			bool bSteers = false;
		};

		uint16 ButtonsOf(const FFinger& Finger) const;
		/** The cross or the stick, which the first control always is. */
		const FTouchControl& Mover() const { return ControlList.front(); }
		bool HasStick() const { return !ControlList.empty() && Mover().Kind == ETouchControlKind::Stick; }

		std::vector<FTouchControl> ControlList;
		std::vector<FFinger> Fingers;
		/** What fingers held before they lifted or moved, since the last take. */
		uint16 Latched = 0;
	};
}
