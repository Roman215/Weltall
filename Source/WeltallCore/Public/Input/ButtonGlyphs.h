#pragma once

#include "Foundation/Types.h"
#include "Input/ControlBindings.h"
#include "Input/PadState.h"

#include <string>
#include <string_view>

namespace Weltall
{
	/**
	 * A button's glyph, or its name in words, in a line of text a sprite draws: the sprite in its list, where the cell starts
	 * in it and how wide it is (pixels), the button it names, and where the line's sprites end in the list (one past its
	 * last; 0: the line is this sprite alone), so that what follows the cell in its line moves with a picture of another width.
	 */
	struct FTextButtonCell
	{
		uint32 Primitive = 0;
		int16 OffsetX = 0;
		int16 Width = 12;
		EPadButton Button = EPadButton::Cross;
		/** The directional buttons together (Button is Up). */
		bool bDirections = false;
		uint32 LineEnd = 0;
		/**
		 * A sprite of the same line that lies apart from the cell's in the list, one more than its place (0 none): the line's
		 * second page, where a list draws the pages one after the other. It moves with what follows the cell too.
		 */
		uint32 LineRest = 0;
	};

	/** The sets of button icons a binding is shown with: the keyboard's keys, and each family of controllers' own symbols. */
	enum class EButtonIconSet : uint8
	{
		Keyboard,
		Xbox,
		PlayStation,
		Nintendo,
		SteamDeck,
	};

	/**
	 * The icon set of a device, from whether it is a gamepad and the name its driver gives it (the
	 * PlayStation, Nintendo and Steam controllers by name; any other gamepad shows Xbox's symbols,
	 * which most PC controllers carry).
	 */
	WELTALLCORE_API EButtonIconSet IconSetOfDevice(bool bGamepad, std::string_view HardwareName);

	/** The controller's selected symbols, falling back to the detected family on Automatic. */
	WELTALLCORE_API EButtonIconSet ControllerIconSet(EControllerIcons Choice, EButtonIconSet Detected);

	/** The devices whose keys an icon set shows. */
	inline EInputFamily FamilyOfIconSet(EButtonIconSet Set)
	{
		return Set == EButtonIconSet::Keyboard ? EInputFamily::KeyboardAndMouse : EInputFamily::Gamepad;
	}

	/** How a PlayStation button is shown: the icon set, the key bound to it on that set's devices, and the icon for that key. */
	struct FButtonGlyph
	{
		EButtonIconSet Set = EButtonIconSet::Keyboard;
		/** The bound key, by the engine's name; empty when the button has none on the device. */
		std::string Key;
		/** The icon within the set: the symbol printed on the controller's button, or the key's label. */
		std::string Icon;

		bool operator==(const FButtonGlyph&) const = default;
	};

	/** The icon of a key (by the engine's name) in a set: what the device prints on it. */
	WELTALLCORE_API std::string ButtonIcon(EButtonIconSet Set, std::string_view Key);

	/** Where a controller's face button lies among the four. */
	enum class EFaceButtonPlace : uint8
	{
		None,
		Bottom,
		Right,
		Left,
		Top,
	};

	/** The place of the face button a set shows as Icon; None for any other icon and for the keyboard's. */
	WELTALLCORE_API EFaceButtonPlace FaceButtonPlace(EButtonIconSet Set, std::string_view Icon);

	/** How a PlayStation button is shown: the original PSX symbol when requested, otherwise its binding on a device of Set. */
	WELTALLCORE_API FButtonGlyph ButtonGlyph(EPadButton Button, const FControlBindings& Bindings, EButtonIconSet Set);

	/**
	 * How the game's PlayStation buttons are shown right now: the host follows the device the player
	 * last used and the bindings. Revision changes whenever either does, so what is drawn from the
	 * glyphs knows to draw them again.
	 */
	class IButtonGlyphs
	{
	public:
		virtual ~IButtonGlyphs() = default;

		virtual FButtonGlyph Glyph(EPadButton Button) const = 0;
		virtual uint32 Revision() const = 0;
	};
}
