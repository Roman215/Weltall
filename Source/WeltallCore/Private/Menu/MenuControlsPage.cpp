#include "Menu/MenuControlsPage.h"

#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuProgram.h"
#include "Menu/OptionsText.h"
#include "Menu/SettingsRows.h"

#include <algorithm>
#include <optional>

namespace Weltall
{
	namespace
	{
		namespace Text = OptionsText::Controls;

		enum class ERowKind : uint8
		{
			Device,
			Heading,
			Action,
			AlwaysRun,
			AnalogMovement,
			WalkIntoDoors,
			OriginalButtonIcons,
			ButtonLayout,
			MouseSensitivity,
			StickSensitivity,
			TouchMovement,
			TouchOpacity,
			TouchVibration,
			Reset,
		};

		/** A row: its kind, its label, its words (a button's label is what it does first, its help all it does on the US discs), and its action. */
		struct FControlsRow
		{
			ERowKind Kind = ERowKind::Action;
			std::string_view Label{};
			const OptionsText::FRow* Words = nullptr;
			EControlAction Action = EControlAction::Up;
		};

		constexpr FControlsRow KeysOf(const OptionsText::FRow& Words, EControlAction Action)
		{
			return {ERowKind::Action, Words.Label, &Words, Action};
		}

		constexpr FControlsRow Setting(ERowKind Kind, const OptionsText::FRow& Words)
		{
			return {Kind, Words.Label, &Words};
		}

		constexpr std::array<FControlsRow, 33> PageRows{{
			{ERowKind::Device, Text::Device},
			{ERowKind::Heading, Text::ButtonsHeading},
			KeysOf(Text::Up, EControlAction::Up),
			KeysOf(Text::Down, EControlAction::Down),
			KeysOf(Text::Left, EControlAction::Left),
			KeysOf(Text::Right, EControlAction::Right),
			KeysOf(Text::Confirm, EControlAction::Cross),
			KeysOf(Text::Cancel, EControlAction::Circle),
			KeysOf(Text::Menu, EControlAction::Square),
			KeysOf(Text::Jump, EControlAction::Triangle),
			KeysOf(Text::CameraLeft, EControlAction::L1),
			KeysOf(Text::CameraRight, EControlAction::R1),
			KeysOf(Text::GearLeft, EControlAction::L2),
			KeysOf(Text::GearRight, EControlAction::R2),
			KeysOf(Text::Pause, EControlAction::Start),
			KeysOf(Text::Map, EControlAction::Select),
			Setting(ERowKind::AlwaysRun, Text::AlwaysRun),
			Setting(ERowKind::AnalogMovement, Text::AnalogMovement),
			Setting(ERowKind::WalkIntoDoors, Text::WalkIntoDoors),
			Setting(ERowKind::OriginalButtonIcons, Text::OriginalButtonIcons),
			Setting(ERowKind::ButtonLayout, Text::ButtonLayout),
			{ERowKind::Heading, Text::CameraHeading},
			KeysOf(Text::Rotate, EControlAction::ViewTurn),
			KeysOf(Text::Tilt, EControlAction::ViewTilt),
			KeysOf(Text::Zoom, EControlAction::ViewZoom),
			KeysOf(Text::ResetCamera, EControlAction::ViewReset),
			Setting(ERowKind::MouseSensitivity, Text::MouseSensitivity),
			Setting(ERowKind::StickSensitivity, Text::StickSensitivity),
			{ERowKind::Heading, Text::TouchHeading},
			Setting(ERowKind::TouchMovement, Text::TouchMovement),
			Setting(ERowKind::TouchOpacity, Text::TouchOpacity),
			Setting(ERowKind::TouchVibration, Text::TouchVibration),
			{ERowKind::Reset},
		}};

		/** The key that stops listening, which is therefore never bound by it. */
		constexpr std::string_view StopKey = "Escape";

		/** The menus' logical Square, which clears a key, and Triangle, which swaps a row's two. */
		constexpr uint16 ClearRole = 0x80;
		constexpr uint16 SwapRole = 0x10;

		/** The axes a camera action can take. */
		constexpr std::array<std::string_view, 7> ViewAxes{
			"MouseX", "MouseY", "MouseWheelAxis", "Gamepad_LeftX", "Gamepad_LeftY", "Gamepad_RightX", "Gamepad_RightY"};

		bool IsViewAxisKey(std::string_view Key)
		{
			return std::find(ViewAxes.begin(), ViewAxes.end(), BoundKeyName(Key)) != ViewAxes.end();
		}

		/**
		 * How the page asks for a camera axis: the way the action takes the movement asked for (the mouse moved right
		 * turns the camera as its default does; moved up, it looks up as the default tilt's minus does; the wheel
		 * turned up and the stick pulled down come nearer), the words asking for it on each device, and the side of
		 * its axis the stick's movement is on, which tells a push the other way.
		 */
		struct FAxisAsked
		{
			EControlAction Action = EControlAction::ViewTurn;
			int32 Way = 1;
			std::string_view Mouse{};
			std::string_view Stick{};
			int32 StickSide = 1;
		};

		constexpr std::array<FAxisAsked, 3> AxesAsked{{
			{EControlAction::ViewTurn, 1, Text::MoveMouseRight, Text::PushStickRight},
			{EControlAction::ViewTilt, -1, Text::MoveMouseUp, Text::PushStickUp},
			{EControlAction::ViewZoom, 1, Text::TurnWheelUp, Text::PushStickDown, -1},
		}};

		const FAxisAsked& AxisAsked(EControlAction Action)
		{
			for (const FAxisAsked& Asked : AxesAsked)
			{
				if (Asked.Action == Action)
				{
					return Asked;
				}
			}
			return AxesAsked.front();
		}

		bool IsButton(EControlAction Action)
		{
			return PadButtonOf(Action).has_value();
		}

		const FControlsRow* RowAt(int32 Row)
		{
			return Row >= 0 && Row < static_cast<int32>(PageRows.size()) ? &PageRows[static_cast<std::size_t>(Row)] : nullptr;
		}

		const FControlsRow* ActionRow(int32 Row)
		{
			const FControlsRow* At = RowAt(Row);
			return At != nullptr && At->Kind == ERowKind::Action ? At : nullptr;
		}

		std::string_view LabelOf(EControlAction Action)
		{
			for (const FControlsRow& Row : PageRows)
			{
				if (Row.Kind == ERowKind::Action && Row.Action == Action)
				{
					return Row.Label;
				}
			}
			return {};
		}

		/** The level a sensitivity's row shows and steps; none for another row. */
		template <typename TBindings>
		auto* SensitivityOf(ERowKind Kind, TBindings& Bindings)
		{
			return Kind == ERowKind::MouseSensitivity ? &Bindings.MouseSensitivity : Kind == ERowKind::StickSensitivity ? &Bindings.StickSensitivity : nullptr;
		}
	}

	FMenuControlsPage::FMenuControlsPage(IMenuKeys& InKeys) : Keys(InKeys), Shown(InKeys.LastFamily()), PadIcons(InKeys.GamepadIcons())
	{
	}

	FMenuControlsPage::~FMenuControlsPage()
	{
		StopListening();
	}

	std::vector<FSettingsRow> FMenuControlsPage::Rows(const FControlBindings& Bindings) const
	{
		std::vector<FSettingsRow> Rows;
		Rows.reserve(PageRows.size());
		const bool bKeyboard = Shown == EInputFamily::KeyboardAndMouse;
		for (int32 Row = 0; Row < static_cast<int32>(PageRows.size()); ++Row)
		{
			const FControlsRow& Kind = PageRows[static_cast<std::size_t>(Row)];
			FSettingsRow& Made = Rows.emplace_back();
			Made.Label = std::string(Kind.Label);
			switch (Kind.Kind)
			{
			case ERowKind::Device:
				Made.Value = std::string(bKeyboard ? Text::Keyboard : Text::Controller);
				Made.bReadOnly = true;
				break;
			case ERowKind::Heading:
				Made.bHeading = true;
				break;
			case ERowKind::Action:
				for (int32 Cell = 0; Cell < ControlSlots; ++Cell)
				{
					FButtonGlyph& Glyph = Made.Cells.emplace_back();
					Glyph.Set = IconSet(Shown);
					Glyph.Key = std::string(Bindings.KeyAt(Kind.Action, Shown, Cell));
					Glyph.Icon = Glyph.Key.empty() ? std::string() : ButtonIcon(Glyph.Set, Glyph.Key);
				}
				break;
			case ERowKind::AlwaysRun:
				Made.Value = std::string(Bindings.bAlwaysRun ? OptionsText::On : OptionsText::Off);
				break;
			case ERowKind::AnalogMovement:
				Made.Value = std::string(Bindings.bAnalogMovement ? OptionsText::On : OptionsText::Off);
				break;
			case ERowKind::WalkIntoDoors:
				Made.Value = std::string(Bindings.bWalkIntoDoors ? OptionsText::On : OptionsText::Off);
				break;
			case ERowKind::OriginalButtonIcons:
				Made.Value = std::string(Bindings.bOriginalButtonIcons ? OptionsText::On : OptionsText::Off);
				break;
			case ERowKind::ButtonLayout:
				Made.Value = SettingsRows::ChoiceName(Bindings.ButtonLayout, Text::ButtonLayoutValues);
				break;
			case ERowKind::MouseSensitivity:
			case ERowKind::StickSensitivity:
				Made.Value = std::to_string(*SensitivityOf(Kind.Kind, Bindings));
				break;
			case ERowKind::TouchMovement:
				Made.Value = SettingsRows::ChoiceName(Bindings.TouchMovement, Text::TouchMovementValues);
				break;
			case ERowKind::TouchOpacity:
				Made.Value = std::to_string(Bindings.TouchOpacity * 10) + "%";
				break;
			case ERowKind::TouchVibration:
				Made.Value = std::string(Bindings.bTouchVibration ? OptionsText::On : OptionsText::Off);
				break;
			case ERowKind::Reset:
				Made.Label = std::string(bKeyboard ? Text::ResetKeys : Text::ResetButtons);
				break;
			}
			Made.Help = Help(Row);
		}
		return Rows;
	}

	std::array<std::string, 3> FMenuControlsPage::Help(int32 Row) const
	{
		const bool bKeyboard = Shown == EInputFamily::KeyboardAndMouse;
		const std::string_view Noun = KeyNoun();
		std::array<std::string, 3> Lines;
		const FControlsRow* At = RowAt(Row);
		if (At == nullptr)
		{
			return Lines;
		}
		if (bListening && Row == ListenRow)
		{
			const std::string Asked = IsViewAxis(At->Action) ? std::string(bKeyboard ? AxisAsked(At->Action).Mouse : AxisAsked(At->Action).Stick)
															 : OptionsText::Fill(Text::PressKey, {Noun});
			return {Asked, std::string(Text::StopListening), ""};
		}
		// The buttons the menus clear and swap with, by their rows' names.
		const auto NameOf = [](uint16 Role)
		{
			const std::optional<EControlAction> Action = ActionOf(MenuRoleButton(Role));
			return Action ? LabelOf(*Action) : std::string_view();
		};
		switch (At->Kind)
		{
		case ERowKind::Device:
		case ERowKind::Heading:
			break;
		case ERowKind::Action:
			Lines = OptionsText::Fill(At->Words->Help);
			Lines[1] = OptionsText::Fill(Text::ClearAndSwap, {NameOf(ClearRole), NameOf(SwapRole)});
			break;
		case ERowKind::AlwaysRun:
			Lines = OptionsText::Fill(At->Words->Help, {Noun, LabelOf(EControlAction::Circle)});
			break;
		case ERowKind::WalkIntoDoors:
			Lines = OptionsText::Fill(At->Words->Help, {LabelOf(EControlAction::Cross)});
			break;
		case ERowKind::AnalogMovement:
		case ERowKind::OriginalButtonIcons:
		case ERowKind::ButtonLayout:
		case ERowKind::MouseSensitivity:
		case ERowKind::StickSensitivity:
		case ERowKind::TouchMovement:
		case ERowKind::TouchOpacity:
		case ERowKind::TouchVibration:
			Lines = OptionsText::Fill(At->Words->Help);
			break;
		case ERowKind::Reset:
			Lines = OptionsText::Fill(Text::ResetHelp, {Noun});
			break;
		}
		if (Row == ToldRow)
		{
			Lines[2] = Told;
		}
		return Lines;
	}

	bool FMenuControlsPage::Follow()
	{
		const EInputFamily Family = Keys.LastFamily();
		const EButtonIconSet Icons = Keys.GamepadIcons();
		if (bListening || (Family == Shown && Icons == PadIcons))
		{
			return false;
		}
		Shown = Family;
		PadIcons = Icons;
		return true;
	}

	bool FMenuControlsPage::Step(int32 Row, int32 Direction, FControlBindings& Bindings)
	{
		const FControlsRow* At = RowAt(Row);
		if (At == nullptr || bListening)
		{
			return false;
		}
		switch (At->Kind)
		{
		case ERowKind::AlwaysRun:
			Bindings.bAlwaysRun = !Bindings.bAlwaysRun;
			return true;
		case ERowKind::AnalogMovement:
			Bindings.bAnalogMovement = !Bindings.bAnalogMovement;
			return true;
		case ERowKind::WalkIntoDoors:
			Bindings.bWalkIntoDoors = !Bindings.bWalkIntoDoors;
			return true;
		case ERowKind::OriginalButtonIcons:
			Bindings.bOriginalButtonIcons = !Bindings.bOriginalButtonIcons;
			return true;
		case ERowKind::ButtonLayout:
			StepLayout(Row, Direction, Bindings);
			return true;
		case ERowKind::MouseSensitivity:
		case ERowKind::StickSensitivity:
		{
			int32& Level = *SensitivityOf(At->Kind, Bindings);
			const int32 Next = std::clamp(Level + Direction, LowestSensitivity, HighestSensitivity);
			if (Next == Level)
			{
				return false;
			}
			Level = Next;
			return true;
		}
		case ERowKind::TouchMovement:
			Bindings.TouchMovement = Bindings.TouchMovement == ETouchMovement::Cross ? ETouchMovement::Stick : ETouchMovement::Cross;
			return true;
		case ERowKind::TouchOpacity:
		{
			const int32 Next = std::clamp(Bindings.TouchOpacity + Direction, LowestTouchOpacity, HighestTouchOpacity);
			if (Next == Bindings.TouchOpacity)
			{
				return false;
			}
			Bindings.TouchOpacity = Next;
			return true;
		}
		case ERowKind::TouchVibration:
			Bindings.bTouchVibration = !Bindings.bTouchVibration;
			return true;
		case ERowKind::Action:
		{
			const int32 Next = std::clamp(Slot + Direction, 0, ControlSlots - 1);
			if (Next == Slot)
			{
				return false;
			}
			Slot = Next;
			CursorMoved();
			return true;
		}
		case ERowKind::Device:
		case ERowKind::Heading:
		case ERowKind::Reset:
			break;
		}
		return false;
	}

	bool FMenuControlsPage::Activate(int32 Row, FControlBindings& Bindings)
	{
		const FControlsRow* At = RowAt(Row);
		if (At == nullptr || bListening)
		{
			return false;
		}
		switch (At->Kind)
		{
		case ERowKind::Device:
		case ERowKind::Heading:
		case ERowKind::MouseSensitivity:
		case ERowKind::StickSensitivity:
		case ERowKind::TouchOpacity:
			return false;
		case ERowKind::TouchMovement:
			Bindings.TouchMovement = Bindings.TouchMovement == ETouchMovement::Cross ? ETouchMovement::Stick : ETouchMovement::Cross;
			return true;
		case ERowKind::TouchVibration:
			Bindings.bTouchVibration = !Bindings.bTouchVibration;
			return true;
		case ERowKind::Action:
			bListening = true;
			ListenRow = Row;
			ListenedFrames = 0;
			ToldRow = -1;
			Keys.Listen(true);
			return true;
		case ERowKind::AlwaysRun:
			Bindings.bAlwaysRun = !Bindings.bAlwaysRun;
			return true;
		case ERowKind::AnalogMovement:
			Bindings.bAnalogMovement = !Bindings.bAnalogMovement;
			return true;
		case ERowKind::WalkIntoDoors:
			Bindings.bWalkIntoDoors = !Bindings.bWalkIntoDoors;
			return true;
		case ERowKind::OriginalButtonIcons:
			Bindings.bOriginalButtonIcons = !Bindings.bOriginalButtonIcons;
			return true;
		case ERowKind::ButtonLayout:
			StepLayout(Row, 1, Bindings);
			return true;
		case ERowKind::Reset:
			Bindings.ResetFamily(Shown);
			Tell(Row, std::string(Shown == EInputFamily::KeyboardAndMouse ? Text::KeysReset : Text::ButtonsReset));
			return true;
		}
		return false;
	}

	bool FMenuControlsPage::Clear(int32 Row, FControlBindings& Bindings)
	{
		const FControlsRow* At = ActionRow(Row);
		if (At == nullptr || bListening || Bindings.KeyAt(At->Action, Shown, Slot).empty())
		{
			return false;
		}
		if (IsButton(At->Action) && Bindings.Count(At->Action, Shown) == 1)
		{
			Tell(Row, OptionsText::Fill(Text::CannotClear, {KeyNoun()}));
			return false;
		}
		Bindings.Clear(At->Action, Shown, Slot);
		ToldRow = -1;
		return true;
	}

	EControlsHeard FMenuControlsPage::Heard(const FPressedKey& Pressed, FControlBindings& Bindings)
	{
		const FControlsRow* At = ActionRow(ListenRow);
		if (!bListening || At == nullptr)
		{
			return EControlsHeard::Ignored;
		}
		if (Pressed.Axis == 0 && Pressed.Key == StopKey)
		{
			StopListening();
			return EControlsHeard::Stopped;
		}
		const bool bAxisRow = IsViewAxis(At->Action);
		if (IsNegatedBinding(Pressed.Key) || bAxisRow != (Pressed.Axis != 0) || (bAxisRow && !IsViewAxisKey(Pressed.Key)))
		{
			return EControlsHeard::Ignored;
		}
		// A key of either device goes among that device's keys, which the rows show from now.
		Shown = FamilyOfKey(Pressed.Key);
		// Whichever way the player moved is the one that does what the page asked for: a stick pushed up for the zoom
		// is then the way nearer, against the page's own.
		const FAxisAsked& Asked = AxisAsked(At->Action);
		const bool bInverted = bAxisRow && Pressed.Axis != (Shown == EInputFamily::Gamepad ? Asked.StickSide : 1);
		const std::string Binding = (bAxisRow && Pressed.Axis != Asked.Way ? "-" : "") + Pressed.Key;
		const int32 Row = ListenRow;
		StopListening();
		const std::optional<FBoundPlace> Was = Bindings.Find(Binding);
		const bool bFree = Bindings.KeyAt(At->Action, Shown, Slot).empty();
		if (Was && Was->Action != At->Action && bFree && IsButton(Was->Action) && Bindings.Count(Was->Action, Shown) == 1)
		{
			Tell(Row, OptionsText::Fill(Text::OnlyKey, {LabelOf(Was->Action)}));
			return EControlsHeard::Refused;
		}
		if (bInverted)
		{
			Tell(Row, std::string(Text::Inverted));
		}
		Bindings.Rebind(At->Action, Slot, Binding);
		return EControlsHeard::Bound;
	}

	bool FMenuControlsPage::Swap(int32 Row, FControlBindings& Bindings)
	{
		const FControlsRow* At = ActionRow(Row);
		if (At == nullptr || bListening || !Bindings.SwapFirstTwo(At->Action, Shown))
		{
			return false;
		}
		ToldRow = -1;
		return true;
	}

	bool FMenuControlsPage::Waited()
	{
		if (!bListening || ++ListenedFrames < ListenFrames)
		{
			return false;
		}
		StopListening();
		return true;
	}

	void FMenuControlsPage::CursorMoved()
	{
		ToldRow = -1;
		Told.clear();
	}

	EButtonIconSet FMenuControlsPage::IconSet(EInputFamily Family) const
	{
		return Family == EInputFamily::KeyboardAndMouse ? EButtonIconSet::Keyboard : PadIcons;
	}

	std::string_view FMenuControlsPage::KeyNoun() const
	{
		return Shown == EInputFamily::KeyboardAndMouse ? Text::KeyNoun : Text::ButtonNoun;
	}

	void FMenuControlsPage::Tell(int32 Row, std::string Line)
	{
		ToldRow = Row;
		Told = std::move(Line);
	}

	void FMenuControlsPage::StepLayout(int32 Row, int32 Direction, FControlBindings& Bindings)
	{
		EButtonLayout Next = Bindings.ButtonLayout;
		SettingsRows::StepChoice(Next, Direction, static_cast<int32>(Text::ButtonLayoutValues.size()));
		if (Bindings.ChangeButtonLayout(Next))
		{
			CursorMoved();
		}
		else
		{
			Tell(Row, std::string(Text::OwnButtonsKept));
		}
	}

	void FMenuControlsPage::StopListening()
	{
		if (bListening)
		{
			Keys.Listen(false);
		}
		bListening = false;
		ListenRow = -1;
	}
}
