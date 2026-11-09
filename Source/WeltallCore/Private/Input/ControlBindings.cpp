#include "Input/ControlBindings.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Weltall
{
	namespace
	{
		constexpr char Negated = '-';
		constexpr std::string_view GamepadPrefix = "Gamepad_";

		constexpr std::array<std::pair<EControlAction, EPadButton>, 14> PadActions{{
			{EControlAction::Up, EPadButton::Up},
			{EControlAction::Down, EPadButton::Down},
			{EControlAction::Left, EPadButton::Left},
			{EControlAction::Right, EPadButton::Right},
			{EControlAction::Cross, EPadButton::Cross},
			{EControlAction::Circle, EPadButton::Circle},
			{EControlAction::Square, EPadButton::Square},
			{EControlAction::Triangle, EPadButton::Triangle},
			{EControlAction::L1, EPadButton::L1},
			{EControlAction::R1, EPadButton::R1},
			{EControlAction::L2, EPadButton::L2},
			{EControlAction::R2, EPadButton::R2},
			{EControlAction::Start, EPadButton::Start},
			{EControlAction::Select, EPadButton::Select},
		}};

		/** The pairs of face buttons a layout changes over: confirm and cancel, then menu and jump. */
		constexpr std::array<std::pair<EControlAction, EControlAction>, 2> FacePairs{{
			{EControlAction::Cross, EControlAction::Circle},
			{EControlAction::Square, EControlAction::Triangle},
		}};

		/** An action's keys of one family, in their order. */
		std::vector<std::string> FamilyKeys(const FControlBindings& Bindings, EControlAction Action, EInputFamily Family)
		{
			std::vector<std::string> Keys;
			for (const std::string& Key : Bindings.Keys(Action))
			{
				if (FamilyOfKey(Key) == Family)
				{
					Keys.push_back(Key);
				}
			}
			return Keys;
		}
	}

	float MouseDegreesPerCount(int32 Level)
	{
		// Each level a step of the same ratio, four of them a tripling: the lowest a third of the middle, the highest some
		// four times it.
		constexpr float AtDefault = 0.585f;
		constexpr float StepsToTriple = 4.0f;
		return AtDefault * std::pow(3.0f, static_cast<float>(std::clamp(Level, LowestSensitivity, HighestSensitivity) - DefaultSensitivity) / StepsToTriple);
	}

	float StickRateScale(int32 Level)
	{
		// The middle level the standard rates, the lowest half of them; each level a step of the same ratio.
		constexpr float StepsToDouble = 4.0f;
		return std::pow(2.0f, static_cast<float>(std::clamp(Level, LowestSensitivity, HighestSensitivity) - DefaultSensitivity) / StepsToDouble);
	}

	std::optional<EPadButton> PadButtonOf(EControlAction Action)
	{
		for (const auto& [Pad, Button] : PadActions)
		{
			if (Pad == Action)
			{
				return Button;
			}
		}
		return std::nullopt;
	}

	bool IsViewAxis(EControlAction Action)
	{
		return Action == EControlAction::ViewTurn || Action == EControlAction::ViewTilt || Action == EControlAction::ViewZoom;
	}

	std::optional<EControlAction> ActionOf(EPadButton Button)
	{
		for (const auto& [Pad, Bit] : PadActions)
		{
			if (Bit == Button)
			{
				return Pad;
			}
		}
		return std::nullopt;
	}

	std::string_view BoundKeyName(std::string_view Binding)
	{
		return IsNegatedBinding(Binding) ? Binding.substr(1) : Binding;
	}

	bool IsNegatedBinding(std::string_view Binding)
	{
		return !Binding.empty() && Binding.front() == Negated;
	}

	EInputFamily FamilyOfKey(std::string_view Binding)
	{
		return BoundKeyName(Binding).starts_with(GamepadPrefix) ? EInputFamily::Gamepad : EInputFamily::KeyboardAndMouse;
	}

	std::optional<FControllerChord> ControllerChordOf(std::string_view Binding)
	{
		const std::size_t Plus = Binding.find('+');
		if (Plus == std::string_view::npos)
		{
			return std::nullopt;
		}
		const std::string_view Modifier = Binding.substr(0, Plus);
		const std::string_view Trigger = Binding.substr(Plus + 1);
		if (!Modifier.starts_with(GamepadPrefix) || Modifier.size() == GamepadPrefix.size()
			|| !Trigger.starts_with(GamepadPrefix) || Trigger.size() == GamepadPrefix.size()
			|| Trigger.find('+') != std::string_view::npos || Modifier == Trigger)
		{
			return std::nullopt;
		}
		return FControllerChord{Modifier, Trigger};
	}

	bool CanBindControl(EControlAction Action, std::string_view Binding)
	{
		return !BoundKeyName(Binding).empty()
			&& (Binding.find('+') == std::string_view::npos || (IsPortAction(Action) && ControllerChordOf(Binding).has_value()));
	}

	FControlBindings FControlBindings::Defaults(EButtonLayout Layout)
	{
		FControlBindings Bindings;
		Bindings.ButtonLayout = Layout;
		const auto Set = [&Bindings](EControlAction Action, std::initializer_list<const char*> Keys)
		{
			Bindings.Bound[static_cast<std::size_t>(Action)].assign(Keys.begin(), Keys.end());
		};
		// The Japanese kernel reads each face button as itself, where the USA's swaps them: the USA's Cross (confirm) is
		// Circle there and its Square (the menu) Triangle.
		const bool bCircleConfirms = Layout != EButtonLayout::Usa;
		const bool bJapan = Layout == EButtonLayout::Japan;
		Set(EControlAction::Up, {"W", "Up", "Gamepad_DPad_Up", "Gamepad_LeftStick_Up"});
		Set(EControlAction::Down, {"S", "Down", "Gamepad_DPad_Down", "Gamepad_LeftStick_Down"});
		Set(EControlAction::Left, {"A", "Left", "Gamepad_DPad_Left", "Gamepad_LeftStick_Left"});
		Set(EControlAction::Right, {"D", "Right", "Gamepad_DPad_Right", "Gamepad_LeftStick_Right"});
		// Enter confirms and Escape cancels as on any computer; the menus need only these and the arrows.
		Set(EControlAction::Cross, {"E", "Enter", bCircleConfirms ? "Gamepad_FaceButton_Right" : "Gamepad_FaceButton_Bottom"});
		Set(EControlAction::Circle, {"Q", "Escape", bCircleConfirms ? "Gamepad_FaceButton_Bottom" : "Gamepad_FaceButton_Right"});
		Set(EControlAction::Square, {"F", bJapan ? "Gamepad_FaceButton_Top" : "Gamepad_FaceButton_Left"});
		Set(EControlAction::Triangle, {"SpaceBar", bJapan ? "Gamepad_FaceButton_Left" : "Gamepad_FaceButton_Top"});
		Set(EControlAction::L1, {"Tab", "Gamepad_LeftShoulder"});
		Set(EControlAction::R1, {"R", "Gamepad_RightShoulder"});
		Set(EControlAction::L2, {"One", "Gamepad_LeftTrigger"});
		Set(EControlAction::R2, {"Four", "Gamepad_RightTrigger"});
		Set(EControlAction::Start, {"P", "Gamepad_Special_Right"});
		Set(EControlAction::Select, {"BackSpace", "Gamepad_Special_Left"});
		// The mouse moved right turns the view as R1 does; the wheel turned forward and the stick pulled back
		// bring it nearer. Nothing tilts it: the tilt is the Camera page's setting (the player may bind it).
		Set(EControlAction::ViewTurn, {"MouseX", "Gamepad_RightX"});
		Set(EControlAction::ViewTilt, {});
		Set(EControlAction::ViewZoom, {"MouseWheelAxis", "-Gamepad_RightY"});
		Set(EControlAction::ViewReset, {"MiddleMouseButton", "Gamepad_RightThumbstick"});
		// As emulators have them; a controller's every button already has a part in the game.
		Set(EControlAction::SaveState, {"F5"});
		Set(EControlAction::LoadState, {"F9"});
		Set(EControlAction::NextStateSlot, {"F7"});
		Set(EControlAction::PreviousStateSlot, {"F6"});
		// The left stick's press has no part in the game.
		Set(EControlAction::MessageLog, {"L", "Gamepad_LeftThumbstick"});
		return Bindings;
	}

	void FControlBindings::SetKeys(EControlAction Action, const std::vector<std::string>& Keys)
	{
		Bound[static_cast<std::size_t>(Action)].clear();
		for (const std::string& Key : Keys)
		{
			Bind(Action, static_cast<int32>(Keys.size()), Key);
		}
	}

	void FControlBindings::Bind(EControlAction Action, int32 Slot, std::string_view Binding)
	{
		if (!CanBindControl(Action, Binding))
		{
			return;
		}
		Unbind(BoundKeyName(Binding));
		std::vector<std::string>& Keys = Bound[static_cast<std::size_t>(Action)];
		const EInputFamily Family = FamilyOfKey(Binding);
		int32 Seen = 0;
		for (std::string& Key : Keys)
		{
			if (FamilyOfKey(Key) == Family && Seen++ == Slot)
			{
				Key = Binding;
				return;
			}
		}
		Keys.emplace_back(Binding);
	}

	void FControlBindings::Rebind(EControlAction Action, int32 Slot, std::string_view Binding)
	{
		if (!CanBindControl(Action, Binding))
		{
			return;
		}
		const EInputFamily Family = FamilyOfKey(Binding);
		const std::string Replaced(KeyAt(Action, Family, Slot));
		if (const std::optional<FBoundPlace> Was = Find(Binding))
		{
			std::string& Held = *KeyIn(Was->Action, Family, Was->Slot);
			if (Was->Action == Action && Replaced.empty())
			{
				// Already Action's, in an earlier slot than the free one asked for: it stays there.
				Held = Binding;
				return;
			}
			if (!CanBindControl(Was->Action, Replaced))
			{
				std::vector<std::string>& Keys = Bound[static_cast<std::size_t>(Was->Action)];
				Keys.erase(Keys.begin() + (&Held - Keys.data()));
			}
			else
			{
				Held = Replaced;
			}
		}
		if (std::string* Key = KeyIn(Action, Family, Slot))
		{
			*Key = Binding;
		}
		else
		{
			Bound[static_cast<std::size_t>(Action)].emplace_back(Binding);
		}
	}

	void FControlBindings::Clear(EControlAction Action, EInputFamily Family, int32 Slot)
	{
		if (const std::string* Key = KeyIn(Action, Family, Slot))
		{
			std::vector<std::string>& Keys = Bound[static_cast<std::size_t>(Action)];
			Keys.erase(Keys.begin() + (Key - Keys.data()));
		}
	}

	bool FControlBindings::SwapFirstTwo(EControlAction Action, EInputFamily Family)
	{
		std::string* First = KeyIn(Action, Family, 0);
		std::string* Second = KeyIn(Action, Family, 1);
		if (First == nullptr || Second == nullptr)
		{
			return false;
		}
		std::swap(*First, *Second);
		return true;
	}

	void FControlBindings::ResetFamily(EInputFamily Family)
	{
		const FControlBindings Defaults = FControlBindings::Defaults(ButtonLayout);
		for (std::size_t Action = 0; Action < Bound.size(); ++Action)
		{
			std::erase_if(Bound[Action], [Family](const std::string& Key) { return FamilyOfKey(Key) == Family; });
			for (const std::string& Key : Defaults.Bound[Action])
			{
				if (FamilyOfKey(Key) == Family)
				{
					Bound[Action].push_back(Key);
				}
			}
		}
	}

	bool FControlBindings::ChangeButtonLayout(EButtonLayout Layout)
	{
		const FControlBindings From = Defaults(ButtonLayout);
		const FControlBindings To = Defaults(Layout);
		bool bMoved = true;
		for (const auto& [First, Second] : FacePairs)
		{
			if (FamilyKeys(From, First, EInputFamily::Gamepad) == FamilyKeys(To, First, EInputFamily::Gamepad))
			{
				continue;
			}
			if (FamilyKeys(*this, First, EInputFamily::Gamepad) != FamilyKeys(From, First, EInputFamily::Gamepad)
				|| FamilyKeys(*this, Second, EInputFamily::Gamepad) != FamilyKeys(From, Second, EInputFamily::Gamepad))
			{
				bMoved = false;
				continue;
			}
			// The pair's keys change places between its two actions, so each key stays on one action.
			for (const EControlAction Action : {First, Second})
			{
				const std::vector<std::string> Moved = FamilyKeys(To, Action, EInputFamily::Gamepad);
				for (int32 Slot = 0; Slot < static_cast<int32>(Moved.size()); ++Slot)
				{
					*KeyIn(Action, EInputFamily::Gamepad, Slot) = Moved[static_cast<std::size_t>(Slot)];
				}
			}
		}
		ButtonLayout = Layout;
		return bMoved;
	}

	std::string_view FControlBindings::KeyAt(EControlAction Action, EInputFamily Family, int32 Slot) const
	{
		const std::string* Key = const_cast<FControlBindings*>(this)->KeyIn(Action, Family, Slot);
		return Key != nullptr ? std::string_view(*Key) : std::string_view();
	}

	int32 FControlBindings::Count(EControlAction Action, EInputFamily Family) const
	{
		const std::vector<std::string>& Keys = Bound[static_cast<std::size_t>(Action)];
		return static_cast<int32>(std::count_if(Keys.begin(), Keys.end(), [Family](const std::string& Key) { return FamilyOfKey(Key) == Family; }));
	}

	std::optional<FBoundPlace> FControlBindings::Find(std::string_view Key) const
	{
		const std::string_view Name = BoundKeyName(Key);
		const EInputFamily Family = FamilyOfKey(Key);
		for (std::size_t Action = 0; Action < Bound.size(); ++Action)
		{
			int32 Slot = 0;
			for (const std::string& Kept : Bound[Action])
			{
				if (FamilyOfKey(Kept) != Family)
				{
					continue;
				}
				if (BoundKeyName(Kept) == Name)
				{
					return FBoundPlace{static_cast<EControlAction>(Action), Slot};
				}
				++Slot;
			}
		}
		return std::nullopt;
	}

	std::string* FControlBindings::KeyIn(EControlAction Action, EInputFamily Family, int32 Slot)
	{
		int32 Seen = 0;
		for (std::string& Key : Bound[static_cast<std::size_t>(Action)])
		{
			if (FamilyOfKey(Key) == Family && Seen++ == Slot)
			{
				return &Key;
			}
		}
		return nullptr;
	}

	void FControlBindings::Unbind(std::string_view Key)
	{
		const std::string_view Name = BoundKeyName(Key);
		for (std::vector<std::string>& Keys : Bound)
		{
			std::erase_if(Keys, [Name](const std::string& Kept) { return BoundKeyName(Kept) == Name; });
		}
	}

	std::string_view FControlBindings::FirstKey(EControlAction Action, EInputFamily Family) const
	{
		for (const std::string& Key : Keys(Action))
		{
			if (FamilyOfKey(Key) == Family)
			{
				return Key;
			}
		}
		return {};
	}
}
