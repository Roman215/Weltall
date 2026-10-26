#pragma once

#include "Foundation/Types.h"
#include "Input/PadState.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** What the player's keys and buttons do: press one of the PlayStation controller's buttons, or move the field's view. */
	enum class EControlAction : uint8
	{
		Up,
		Down,
		Left,
		Right,
		Cross,
		Circle,
		Square,
		Triangle,
		L1,
		R1,
		L2,
		R2,
		Start,
		Select,
		/** The player's field camera: axes whose keys push them one way, or with a leading '-' the other. */
		ViewTurn,
		ViewTilt,
		ViewZoom,
		/** The view back as the game frames it. */
		ViewReset,
	};

	inline constexpr int32 ControlActionCount = 18;

	/**
	 * Where a controller's face buttons put the USA release's buttons: as the USA release has them; with confirm and cancel
	 * changed over (Circle confirms and is the fierce attack, Cross cancels); or as the Japanese release has them, whose
	 * kernel leaves the face buttons unswapped (also Square jumps and is the weak attack, Triangle opens the menu).
	 */
	enum class EButtonLayout : uint8
	{
		Usa,
		CircleConfirms,
		Japan,
	};

	/** What moves the party on the touch screen's pad: its cross, or a stick that walks any way at its push (as a gamepad's left stick). */
	enum class ETouchMovement : uint8
	{
		Cross,
		Stick,
	};

	/** The touch screen's pad drawn from LowestTouchOpacity to HighestTouchOpacity tenths of its full strength. */
	inline constexpr int32 LowestTouchOpacity = 2;
	inline constexpr int32 HighestTouchOpacity = 10;

	/** The devices a key belongs to, which draw its glyph from their own icons. */
	enum class EInputFamily : uint8
	{
		KeyboardAndMouse,
		Gamepad,
	};

	/** How many keys of each family the options screen shows an action with, and rebinds. */
	inline constexpr int32 ControlSlots = 2;

	/** The levels the view's sensitivity to the mouse and to a stick is set to; a new player's is the middle one. */
	inline constexpr int32 LowestSensitivity = 1;
	inline constexpr int32 HighestSensitivity = 10;
	inline constexpr int32 DefaultSensitivity = 5;

	/** How many degrees the mouse turns or tilts the view for each count it moves, at a sensitivity level. */
	WELTALLCORE_API float MouseDegreesPerCount(int32 Level);

	/** How much faster than its standard rates a stick held all the way turns and tilts the view, at a sensitivity level. */
	WELTALLCORE_API float StickRateScale(int32 Level);

	/** The PlayStation button an action presses; none for the view's. */
	WELTALLCORE_API std::optional<EPadButton> PadButtonOf(EControlAction Action);

	/** Whether an action is one of the view's axes (turn, tilt, zoom), which a stick or the mouse moves. */
	WELTALLCORE_API bool IsViewAxis(EControlAction Action);

	/** The action that presses a PlayStation button; none for the stick buttons, which the game's controller lacks. */
	WELTALLCORE_API std::optional<EControlAction> ActionOf(EPadButton Button);

	/** The key a binding names (the engine's name), without the sign that picks an axis's other direction. */
	WELTALLCORE_API std::string_view BoundKeyName(std::string_view Binding);

	/** Whether a binding takes an axis's other direction ('-' before the key's name). */
	WELTALLCORE_API bool IsNegatedBinding(std::string_view Binding);

	/** The devices a key (by the engine's name) belongs to: a gamepad's keys are named Gamepad_. */
	WELTALLCORE_API EInputFamily FamilyOfKey(std::string_view Binding);

	/** Where a key is bound: its action, and its place among that action's keys of its family. */
	struct FBoundPlace
	{
		EControlAction Action = EControlAction::Up;
		int32 Slot = 0;

		bool operator==(const FBoundPlace&) const = default;
	};

	/**
	 * The keys bound to each action, by the engine's names for them (with a leading '-', an axis's
	 * other direction). A key does one thing: binding it to an action takes it off any other.
	 */
	class FControlBindings
	{
	public:
		/**
		 * The port's defaults: on a gamepad the PlayStation's buttons where they sit on its controller
		 * (the face buttons by position, the shoulders and triggers, the menu buttons for Start and
		 * Select, the D-pad and the left stick for the directions) and the right stick for the view; on
		 * the keyboard W A S D and the arrows, E and Enter to confirm, Q and Escape to cancel, F for the
		 * menu, Space to jump, Tab and R, 1 and 4, P and Backspace, with the mouse for the view. A layout
		 * other than the USA's moves the face buttons as it says; the keyboard's keys are the same in all.
		 */
		WELTALLCORE_API static FControlBindings Defaults(EButtonLayout Layout = EButtonLayout::Usa);

		const std::vector<std::string>& Keys(EControlAction Action) const { return Bound[static_cast<std::size_t>(Action)]; }

		/** Every key bound to Action, replacing its keys; each is taken off any other action. */
		WELTALLCORE_API void SetKeys(EControlAction Action, const std::vector<std::string>& Keys);

		/**
		 * Binds a key to Action in place of the Slot-th of its family's keys there (after them when
		 * Slot is past the last), and takes it off any other action.
		 */
		WELTALLCORE_API void Bind(EControlAction Action, int32 Slot, std::string_view Binding);

		/**
		 * Binds a key to Action as the Slot-th of its family's keys there (after them when Slot is past the last).
		 * Where the key was another action's, that action takes the key it replaces in its place, or just loses it
		 * when the slot was free; where it was Action's own in another slot, the two change places.
		 */
		WELTALLCORE_API void Rebind(EControlAction Action, int32 Slot, std::string_view Binding);

		/** Takes a key off whatever action it is bound to. */
		WELTALLCORE_API void Unbind(std::string_view Key);

		/** Takes the Slot-th key of a family off Action; nothing when it has fewer. */
		WELTALLCORE_API void Clear(EControlAction Action, EInputFamily Family, int32 Slot);

		/** Action's first two keys of a family change places (the first is the one its glyph shows); false when it has fewer than two. */
		WELTALLCORE_API bool SwapFirstTwo(EControlAction Action, EInputFamily Family);

		/** A family's keys put back as the layout's defaults have them, the other family's kept. */
		WELTALLCORE_API void ResetFamily(EInputFamily Family);

		/**
		 * Takes another layout: each pair of face buttons it changes over (confirm and cancel; menu and jump) moves to its
		 * places where both still have the old layout's gamepad keys and no others. Returns false when a pair the player
		 * bound otherwise kept its keys.
		 */
		WELTALLCORE_API bool ChangeButtonLayout(EButtonLayout Layout);

		/** The first key of a family bound to Action (the one its glyph shows), empty when there is none. */
		WELTALLCORE_API std::string_view FirstKey(EControlAction Action, EInputFamily Family) const;

		/** The Slot-th key of a family bound to Action, empty when it has fewer. */
		WELTALLCORE_API std::string_view KeyAt(EControlAction Action, EInputFamily Family, int32 Slot) const;

		/** How many keys of a family are bound to Action. */
		WELTALLCORE_API int32 Count(EControlAction Action, EInputFamily Family) const;

		/** Where a key (by its name, in either direction) is bound; none when it is free. */
		WELTALLCORE_API std::optional<FBoundPlace> Find(std::string_view Key) const;

		/** The party runs on the field unless the run button is held, which then walks. Off: it runs while the button is held, as the original. */
		bool bAlwaysRun = true;
		/** The stick walks the party in any direction, slowly when pushed lightly. Off: the d-pad's eight directions, as the original. */
		bool bAnalogMovement = true;
		/** A door opens when the party walks into it, as it does for the confirm button pressed at it. Off: the button alone, as the original. */
		bool bWalkIntoDoors = true;
		/** Keep the original PlayStation button pictures instead of showing the bound keys or controller buttons. */
		bool bOriginalButtonIcons = false;
		/** The face buttons' layout, whose places the original pictures show (ChangeButtonLayout moves the keys). */
		EButtonLayout ButtonLayout = EButtonLayout::Usa;
		/** How fast the mouse and a stick move the view: levels from LowestSensitivity to HighestSensitivity. */
		int32 MouseSensitivity = DefaultSensitivity;
		int32 StickSensitivity = DefaultSensitivity;
		/** The touch screen's pad: what moves the party, how strongly it is drawn (in tenths), and whether a press vibrates the device. */
		ETouchMovement TouchMovement = ETouchMovement::Cross;
		int32 TouchOpacity = HighestTouchOpacity;
		bool bTouchVibration = true;

		bool operator==(const FControlBindings&) const = default;

	private:
		/** The Slot-th key of a family bound to Action, null when it has fewer. */
		std::string* KeyIn(EControlAction Action, EInputFamily Family, int32 Slot);

		std::array<std::vector<std::string>, ControlActionCount> Bound;
	};
}
