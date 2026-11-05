#include "Input/ButtonGlyphs.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Weltall
{
	namespace
	{
		/** A gamepad key's icon in each controller family's set: Xbox, PlayStation, Nintendo, Steam Deck. */
		struct FGamepadIcons
		{
			std::string_view Key;
			std::array<std::string_view, 4> Icons;
		};

		constexpr std::array<FGamepadIcons, 20> GamepadIcons{{
			{"Gamepad_FaceButton_Bottom", {"A", "Cross", "B", "A"}},
			{"Gamepad_FaceButton_Right", {"B", "Circle", "A", "B"}},
			{"Gamepad_FaceButton_Left", {"X", "Square", "Y", "X"}},
			{"Gamepad_FaceButton_Top", {"Y", "Triangle", "X", "Y"}},
			{"Gamepad_LeftShoulder", {"LB", "L1", "L", "L1"}},
			{"Gamepad_RightShoulder", {"RB", "R1", "R", "R1"}},
			{"Gamepad_LeftTrigger", {"LT", "L2", "ZL", "L2"}},
			{"Gamepad_RightTrigger", {"RT", "R2", "ZR", "R2"}},
			{"Gamepad_LeftTriggerAxis", {"LT", "L2", "ZL", "L2"}},
			{"Gamepad_RightTriggerAxis", {"RT", "R2", "ZR", "R2"}},
			{"Gamepad_Special_Left", {"View", "Create", "Minus", "View"}},
			{"Gamepad_Special_Right", {"Menu", "Options", "Plus", "Menu"}},
			{"Gamepad_LeftThumbstick", {"LS", "L3", "LS", "L3"}},
			{"Gamepad_RightThumbstick", {"RS", "R3", "RS", "R3"}},
			{"Gamepad_LeftX", {"LeftStick", "LeftStick", "LeftStick", "LeftStick"}},
			{"Gamepad_LeftY", {"LeftStick", "LeftStick", "LeftStick", "LeftStick"}},
			{"Gamepad_Left2D", {"LeftStick", "LeftStick", "LeftStick", "LeftStick"}},
			{"Gamepad_RightX", {"RightStick", "RightStick", "RightStick", "RightStick"}},
			{"Gamepad_RightY", {"RightStick", "RightStick", "RightStick", "RightStick"}},
			{"Gamepad_Right2D", {"RightStick", "RightStick", "RightStick", "RightStick"}},
		}};

		/** Keys whose label is not their name. */
		constexpr std::array<std::pair<std::string_view, std::string_view>, 19> KeyLabels{{
			{"One", "1"},
			{"Two", "2"},
			{"Three", "3"},
			{"Four", "4"},
			{"Five", "5"},
			{"Six", "6"},
			{"Seven", "7"},
			{"Eight", "8"},
			{"Nine", "9"},
			{"Zero", "0"},
			{"BackSpace", "Backspace"},
			{"SpaceBar", "Space"},
			{"LeftMouseButton", "MouseLeft"},
			{"RightMouseButton", "MouseRight"},
			{"MiddleMouseButton", "MouseMiddle"},
			{"MouseX", "Mouse"},
			{"MouseY", "Mouse"},
			{"MouseWheelAxis", "MouseWheel"},
			{"Escape", "Esc"},
		}};

		std::string Lowered(std::string_view Text)
		{
			std::string Lower(Text);
			std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](char Letter) { return static_cast<char>(std::tolower(static_cast<unsigned char>(Letter))); });
			return Lower;
		}

		bool ContainsAny(const std::string& Text, std::initializer_list<std::string_view> Parts)
		{
			return std::any_of(Parts.begin(), Parts.end(), [&Text](std::string_view Part) { return Text.find(Part) != std::string::npos; });
		}
	}

	EButtonIconSet IconSetOfDevice(bool bGamepad, std::string_view HardwareName)
	{
		if (!bGamepad)
		{
			return EButtonIconSet::Keyboard;
		}
		const std::string Name = Lowered(HardwareName);
		if (ContainsAny(Name, {"dualsense", "dualshock", "playstation", "ps3", "ps4", "ps5"}))
		{
			return EButtonIconSet::PlayStation;
		}
		if (ContainsAny(Name, {"nintendo", "switch", "joy-con", "joycon", "pro controller"}))
		{
			return EButtonIconSet::Nintendo;
		}
		if (ContainsAny(Name, {"steam"}))
		{
			return EButtonIconSet::SteamDeck;
		}
		return EButtonIconSet::Xbox;
	}

	EButtonIconSet ControllerIconSet(EControllerIcons Choice, EButtonIconSet Detected)
	{
		switch (Choice)
		{
		case EControllerIcons::Xbox: return EButtonIconSet::Xbox;
		case EControllerIcons::PlayStation: return EButtonIconSet::PlayStation;
		case EControllerIcons::Nintendo: return EButtonIconSet::Nintendo;
		case EControllerIcons::SteamDeck: return EButtonIconSet::SteamDeck;
		case EControllerIcons::Automatic: return Detected;
		}
		return Detected;
	}

	std::string ButtonIcon(EButtonIconSet Set, std::string_view Key)
	{
		const std::string_view Name = BoundKeyName(Key);
		if (Set != EButtonIconSet::Keyboard)
		{
			for (const FGamepadIcons& Icons : GamepadIcons)
			{
				if (Icons.Key == Name)
				{
					return std::string(Icons.Icons[static_cast<std::size_t>(Set) - 1]);
				}
			}
			// The D-pad and the sticks' directions look alike on every controller: DPad_Up, LeftStick_Up...
			constexpr std::string_view Prefix = "Gamepad_";
			return std::string(Name.starts_with(Prefix) ? Name.substr(Prefix.size()) : Name);
		}
		for (const auto& [Bound, Label] : KeyLabels)
		{
			if (Bound == Name)
			{
				return std::string(Label);
			}
		}
		return std::string(Name);
	}

	EFaceButtonPlace FaceButtonPlace(EButtonIconSet Set, std::string_view Icon)
	{
		if (Set == EButtonIconSet::Keyboard)
		{
			return EFaceButtonPlace::None;
		}
		// The table's first four keys are the face buttons, in the places' order.
		for (std::size_t Place = 0; Place < 4; ++Place)
		{
			if (GamepadIcons[Place].Icons[static_cast<std::size_t>(Set) - 1] == Icon)
			{
				return static_cast<EFaceButtonPlace>(Place + 1);
			}
		}
		return EFaceButtonPlace::None;
	}

	FButtonGlyph ButtonGlyph(EPadButton Button, const FControlBindings& Bindings, EButtonIconSet Set)
	{
		const FControlBindings* Shown = &Bindings;
		if (Bindings.bOriginalButtonIcons)
		{
			// The PlayStation's symbols at the places the layout gives each button.
			static const std::array<FControlBindings, 3> Originals{FControlBindings::Defaults(EButtonLayout::Usa),
				FControlBindings::Defaults(EButtonLayout::CircleConfirms), FControlBindings::Defaults(EButtonLayout::Japan)};
			Shown = &Originals[static_cast<std::size_t>(Bindings.ButtonLayout)];
			Set = EButtonIconSet::PlayStation;
		}
		FButtonGlyph Glyph;
		Glyph.Set = Set;
		if (const std::optional<EControlAction> Action = ActionOf(Button))
		{
			Glyph.Key = std::string(Shown->FirstKey(*Action, FamilyOfIconSet(Set)));
		}
		if (!Glyph.Key.empty())
		{
			Glyph.Icon = ButtonIcon(Set, Glyph.Key);
			if (Bindings.bOriginalButtonIcons && Button == EPadButton::Start) { Glyph.Icon = "Start"; }
			if (Bindings.bOriginalButtonIcons && Button == EPadButton::Select) { Glyph.Icon = "Select"; }
		}
		return Glyph;
	}
}
