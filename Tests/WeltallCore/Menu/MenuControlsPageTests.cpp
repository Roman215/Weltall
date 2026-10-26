#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuControlsPage.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/PressedKeys.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

// The options screen's Controls page: the keys it shows of the device last touched, listening for a key and binding
// it, the keys a button keeps, the camera's axes, the reset, and its words in the game's font.
namespace
{
	using Testing::FPressedKeys;
	namespace Text = OptionsText::Controls;

	int32 RowOf(const FMenuControlsPage& Page, std::string_view Label)
	{
		const std::vector<FSettingsRow> Rows = Page.Rows(FControlBindings::Defaults());
		for (int32 Row = 0; Row < static_cast<int32>(Rows.size()); ++Row)
		{
			if (Rows[static_cast<std::size_t>(Row)].Label == Label)
			{
				return Row;
			}
		}
		FAIL("No row " << Label);
		return -1;
	}

	FSettingsRow RowNamed(const FMenuControlsPage& Page, const FControlBindings& Bindings, std::string_view Label)
	{
		return Page.Rows(Bindings)[static_cast<std::size_t>(RowOf(Page, Label))];
	}

	/** The page listening on a row's cell, then hearing one key. */
	EControlsHeard BindOn(FMenuControlsPage& Page, FControlBindings& Bindings, std::string_view Label, FPressedKey Key)
	{
		REQUIRE(Page.Activate(RowOf(Page, Label), Bindings));
		REQUIRE(Page.Listening());
		return Page.Heard(Key, Bindings);
	}

	constexpr std::array<std::string_view, 18> ActionLabels = {Text::Up.Label, Text::Down.Label, Text::Left.Label, Text::Right.Label,
		Text::Confirm.Label, Text::Cancel.Label, Text::Menu.Label, Text::Jump.Label, Text::CameraLeft.Label, Text::CameraRight.Label,
		Text::GearLeft.Label, Text::GearRight.Label, Text::Pause.Label, Text::Map.Label, Text::Rotate.Label, Text::Tilt.Label, Text::Zoom.Label,
		Text::ResetCamera.Label};

	bool IsAxis(std::string_view Label)
	{
		return Label == Text::Rotate.Label || Label == Text::Tilt.Label || Label == Text::Zoom.Label;
	}
}

TEST_CASE("The Controls page shows two keys of each button and camera action, of the device last touched", "[Menu]")
{
	FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	const std::vector<FSettingsRow> Rows = Page.Rows(Bindings);
	REQUIRE(Rows.size() == 33);
	CHECK(Rows[0].Label == Text::Device);
	CHECK(Rows[0].Value == Text::Keyboard);
	CHECK(Rows[0].bReadOnly);
	CHECK(Rows[1].bHeading);
	CHECK(Rows[16].Label == Text::AlwaysRun.Label);
	CHECK(Rows[17].Label == Text::AnalogMovement.Label);
	CHECK(Rows[18].Label == Text::WalkIntoDoors.Label);
	CHECK(Rows[19].Label == Text::OriginalButtonIcons.Label);
	CHECK(Rows[20].Label == Text::ButtonLayout.Label);
	CHECK(Rows[21].Label == Text::CameraHeading);
	CHECK(Rows[26].Label == Text::MouseSensitivity.Label);
	CHECK(Rows[27].Label == Text::StickSensitivity.Label);
	CHECK(Rows[28].Label == Text::TouchHeading);
	CHECK(Rows[28].bHeading);
	CHECK(Rows[29].Label == Text::TouchMovement.Label);
	CHECK(Rows[29].Value == "D-pad");
	CHECK(Rows[30].Label == Text::TouchOpacity.Label);
	CHECK(Rows[30].Value == "100%");
	CHECK(Rows[31].Label == Text::TouchVibration.Label);
	CHECK(Rows[31].Value == OptionsText::On);
	CHECK(Rows.back().Label == Text::ResetKeys);
	for (const std::string_view Label : ActionLabels)
	{
		CHECK(RowNamed(Page, Bindings, Label).Cells.size() == ControlSlots);
	}
	const FSettingsRow Up = RowNamed(Page, Bindings, Text::Up.Label);
	CHECK(Up.Cells[0] == FButtonGlyph{EButtonIconSet::Keyboard, "W", "W"});
	CHECK(Up.Cells[1] == FButtonGlyph{EButtonIconSet::Keyboard, "Up", "Up"});
	const FSettingsRow Cross = RowNamed(Page, Bindings, Text::Confirm.Label);
	CHECK(Cross.Cells[0].Icon == "E");
	CHECK(Cross.Cells[1].Icon == "Enter");
	CHECK(RowNamed(Page, Bindings, Text::Jump.Label).Cells[0].Icon == "Space");
	// What the button does, then the buttons that clear and swap, by their rows' names (the US discs' Triangle and Square).
	CHECK(Cross.Help[0] == Text::Confirm.Help[0]);
	CHECK(Cross.Help[1] == OptionsText::Fill(Text::ClearAndSwap, {Text::Jump.Label, Text::Menu.Label}));
	CHECK(RowNamed(Page, Bindings, Text::Tilt.Label).Help[0] == Text::Tilt.Help[0]);

	// The player touches a controller: its buttons, in its icons; the keyboard's are kept.
	CHECK_FALSE(Page.Follow());
	Keys.Family = EInputFamily::Gamepad;
	Keys.Icons = EButtonIconSet::PlayStation;
	REQUIRE(Page.Follow());
	CHECK(Page.Family() == EInputFamily::Gamepad);
	CHECK(Page.Rows(Bindings)[0].Value == Text::Controller);
	CHECK(RowNamed(Page, Bindings, Text::Confirm.Label).Cells[0] == FButtonGlyph{EButtonIconSet::PlayStation, "Gamepad_FaceButton_Bottom", "Cross"});
	CHECK(Page.Rows(Bindings).back().Label == Text::ResetButtons);
	Keys.Icons = EButtonIconSet::Xbox;
	REQUIRE(Page.Follow());
	CHECK(RowNamed(Page, Bindings, Text::Confirm.Label).Cells[0].Icon == "A");

	// Left and Right on a row of keys move between them, no further; the device row is only shown.
	const int32 Row = RowOf(Page, Text::Confirm.Label);
	CHECK_FALSE(Page.Step(Row, -1, Bindings));
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK(Page.Cell() == 1);
	CHECK_FALSE(Page.Step(Row, 1, Bindings));
	CHECK_FALSE(Page.Step(0, 1, Bindings));
	CHECK_FALSE(Page.Activate(0, Bindings));
}

TEST_CASE("Confirm on a key listens for the one to put there, which leaves the action it was bound to", "[Menu]")
{
	FPressedKeys Keys;
	// Confirm and Cancel with one key each, Enter and Escape taken off.
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Unbind("Enter");
	Bindings.Unbind("Escape");
	{
		FMenuControlsPage Page(Keys);
		const int32 Cross = RowOf(Page, Text::Confirm.Label);
		REQUIRE(Page.Activate(Cross, Bindings));
		CHECK(Keys.bListening);
		CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[0] == OptionsText::Fill(Text::PressKey, {Text::KeyNoun}));
		CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[1] == Text::StopListening);
		// Nothing else is done while it listens, and the device shown stays.
		CHECK_FALSE(Page.Activate(Cross, Bindings));
		CHECK_FALSE(Page.Step(Cross, 1, Bindings));
		CHECK_FALSE(Page.Clear(Cross, Bindings));
		Keys.Family = EInputFamily::Gamepad;
		CHECK_FALSE(Page.Follow());
		Keys.Family = EInputFamily::KeyboardAndMouse;
		// An axis moved is not a button.
		CHECK(Page.Heard({"MouseX", 1}, Bindings) == EControlsHeard::Ignored);
		CHECK(Page.Listening());
		// Q, Circle's: Circle takes E in its place.
		CHECK(Page.Heard({"Q"}, Bindings) == EControlsHeard::Bound);
		CHECK_FALSE(Page.Listening());
		CHECK_FALSE(Keys.bListening);
		CHECK(Bindings.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 0) == "Q");
		CHECK(Bindings.KeyAt(EControlAction::Circle, EInputFamily::KeyboardAndMouse, 0) == "E");
		CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[2].empty());

		// The menus' Triangle swaps the row's two keys: the first is the one the game's prompts show.
		CHECK_FALSE(Page.Swap(Cross, Bindings));
		CHECK(Page.Swap(RowOf(Page, Text::Down.Label), Bindings));
		CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 0) == "Down");
		CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 1) == "S");

		// Into Cross's free second place, Up's arrow key: Up keeps W.
		REQUIRE(Page.Step(Cross, 1, Bindings));
		CHECK(BindOn(Page, Bindings, Text::Confirm.Label, {"Up"}) == EControlsHeard::Bound);
		CHECK(Bindings.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 1) == "Up");
		CHECK(Bindings.Keys(EControlAction::Up) == std::vector<std::string>{"W", "Gamepad_DPad_Up", "Gamepad_LeftStick_Up"});

		// A controller's button heard on the keyboard's page goes among the controller's buttons, now shown; the
		// controller's Select is a button like any other.
		REQUIRE(Page.Step(Cross, -1, Bindings));
		CHECK(BindOn(Page, Bindings, Text::Pause.Label, {"Gamepad_Special_Left"}) == EControlsHeard::Bound);
		CHECK(Page.Family() == EInputFamily::Gamepad);
		CHECK(Bindings.KeyAt(EControlAction::Start, EInputFamily::Gamepad, 0) == "Gamepad_Special_Left");
		CHECK(Bindings.KeyAt(EControlAction::Select, EInputFamily::Gamepad, 0) == "Gamepad_Special_Right");
		CHECK(Bindings.KeyAt(EControlAction::Start, EInputFamily::KeyboardAndMouse, 0) == "P");

		// Esc stops listening and changes nothing; so does waiting five seconds.
		const FControlBindings Before = Bindings;
		CHECK(BindOn(Page, Bindings, Text::Pause.Label, {"Escape"}) == EControlsHeard::Stopped);
		REQUIRE(Page.Activate(RowOf(Page, Text::Pause.Label), Bindings));
		for (int32 Frame = 1; Frame < FMenuControlsPage::ListenFrames; ++Frame)
		{
			REQUIRE_FALSE(Page.Waited());
		}
		CHECK(Page.Waited());
		CHECK_FALSE(Page.Listening());
		CHECK_FALSE(Keys.bListening);
		CHECK(Bindings == Before);
		CHECK(Keys.Listens == 5);

		// A page closed while it listens stops listening.
		REQUIRE(Page.Activate(Cross, Bindings));
	}
	CHECK_FALSE(Keys.bListening);
}

TEST_CASE("A PlayStation button keeps its last key of each device", "[Menu]")
{
	FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	// Confirm and Cancel with one key each, Enter and Escape taken off.
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Unbind("Enter");
	Bindings.Unbind("Escape");
	const FControlBindings Start = Bindings;
	// Q into Cross's free second place would leave Circle with no key.
	const int32 Cross = RowOf(Page, Text::Confirm.Label);
	REQUIRE(Page.Step(Cross, 1, Bindings));
	CHECK(BindOn(Page, Bindings, Text::Confirm.Label, {"Q"}) == EControlsHeard::Refused);
	CHECK(Bindings == Start);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[2] == OptionsText::Fill(Text::OnlyKey, {Text::Cancel.Label}));

	// Cross's only key stays; nothing is in its second place to clear.
	CHECK_FALSE(Page.Clear(Cross, Bindings));
	REQUIRE(Page.Step(Cross, -1, Bindings));
	CHECK_FALSE(Page.Clear(Cross, Bindings));
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[2] == OptionsText::Fill(Text::CannotClear, {Text::KeyNoun}));

	// Up has two; the camera's actions may have none.
	CHECK(Page.Clear(RowOf(Page, Text::Up.Label), Bindings));
	CHECK(Bindings.Keys(EControlAction::Up) == std::vector<std::string>{"Up", "Gamepad_DPad_Up", "Gamepad_LeftStick_Up"});
	CHECK(Page.Clear(RowOf(Page, Text::ResetCamera.Label), Bindings));
	CHECK(Bindings.Count(EControlAction::ViewReset, EInputFamily::KeyboardAndMouse) == 0);
	CHECK(BindOn(Page, Bindings, Text::ResetCamera.Label, {"MiddleMouseButton"}) == EControlsHeard::Bound);
	CHECK(Page.Clear(RowOf(Page, Text::Rotate.Label), Bindings));

	// On a controller.
	Keys.Family = EInputFamily::Gamepad;
	REQUIRE(Page.Follow());
	CHECK_FALSE(Page.Clear(Cross, Bindings));
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Cross)].Help[2] == OptionsText::Fill(Text::CannotClear, {Text::ButtonNoun}));
}

TEST_CASE("The camera's axes take a stick or the mouse moved the way the page asks", "[Menu]")
{
	FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(RowNamed(Page, Bindings, Text::Rotate.Label).Help[0] == Text::Rotate.Help[0]);

	// A key is not an axis; an axis moved the other way round turns the camera the other way.
	REQUIRE(Page.Activate(RowOf(Page, Text::Rotate.Label), Bindings));
	CHECK(RowNamed(Page, Bindings, Text::Rotate.Label).Help[0] == Text::MoveMouseRight);
	CHECK(Page.Heard({"K"}, Bindings) == EControlsHeard::Ignored);
	CHECK(Page.Heard({"MouseY", -1}, Bindings) == EControlsHeard::Bound);
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"-MouseY", "Gamepad_RightX"});
	CHECK(Bindings.Keys(EControlAction::ViewTilt).empty());
	CHECK(RowNamed(Page, Bindings, Text::Rotate.Label).Help[2] == Text::Inverted);

	// The tilt, unbound, takes the mouse moved up to look up, not inverted; the turn gives the axis up.
	CHECK(BindOn(Page, Bindings, Text::Tilt.Label, {"MouseY", 1}) == EControlsHeard::Bound);
	CHECK(Bindings.KeyAt(EControlAction::ViewTilt, EInputFamily::KeyboardAndMouse, 0) == "-MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"Gamepad_RightX"});
	CHECK(RowNamed(Page, Bindings, Text::Tilt.Label).Help[2].empty());

	// A stick's axis, not a trigger's or a stick's direction as a button.
	REQUIRE(Page.Activate(RowOf(Page, Text::Zoom.Label), Bindings));
	CHECK(Page.Heard({"Gamepad_LeftTriggerAxis", 1}, Bindings) == EControlsHeard::Ignored);
	CHECK(Page.Heard({"Gamepad_LeftStick_Up"}, Bindings) == EControlsHeard::Ignored);
	// Pulled down to come nearer: the default zoom's way on a stick, not inverted.
	CHECK(Page.Heard({"Gamepad_LeftY", -1}, Bindings) == EControlsHeard::Bound);
	CHECK(Bindings.KeyAt(EControlAction::ViewZoom, EInputFamily::Gamepad, 0) == "-Gamepad_LeftY");
	CHECK(RowNamed(Page, Bindings, Text::Zoom.Label).Help[2].empty());
	REQUIRE(Page.Activate(RowOf(Page, Text::Zoom.Label), Bindings));
	CHECK(RowNamed(Page, Bindings, Text::Zoom.Label).Help[0] == Text::PushStickDown);
	CHECK(Page.Heard({"Gamepad_LeftY", 1}, Bindings) == EControlsHeard::Bound);
	CHECK(Bindings.KeyAt(EControlAction::ViewZoom, EInputFamily::Gamepad, 0) == "Gamepad_LeftY");
	CHECK(RowNamed(Page, Bindings, Text::Zoom.Label).Help[2] == Text::Inverted);
}

TEST_CASE("The last row puts the shown device's keys back as they start", "[Menu]")
{
	FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(BindOn(Page, Bindings, Text::Confirm.Label, {"Q"}) == EControlsHeard::Bound);
	CHECK(BindOn(Page, Bindings, Text::Confirm.Label, {"Gamepad_FaceButton_Right"}) == EControlsHeard::Bound);
	const int32 Reset = static_cast<int32>(Page.Rows(Bindings).size()) - 1;
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Reset)].Help[0] == OptionsText::Fill(Text::ResetHelp[0], {Text::ButtonNoun}));
	REQUIRE(Page.Activate(Reset, Bindings));
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Reset)].Help[2] == Text::ButtonsReset);
	CHECK(Bindings.KeyAt(EControlAction::Cross, EInputFamily::Gamepad, 0) == "Gamepad_FaceButton_Bottom");
	CHECK(Bindings.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 0) == "Q");
	CHECK_FALSE(Page.Activate(1, Bindings));
}

TEST_CASE("Every Controls row's words are in the game's font and fit beside its keys", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
	// In pixels: a label from its place, a gap, then the arrow before the first key; a help line in its strip.
	constexpr int32 LabelX = 0x28;
	constexpr int32 FirstCursor = 0x128 - 12 - 44 - 12;
	constexpr int32 ValueRight = 0x128;
	constexpr int32 Gap = 8;
	constexpr int32 HelpWidth = 0x3D * 4;
	const auto Width = [&Text](std::string_view Words)
	{
		int32 Units = 0;
		for (const char Character : Words)
		{
			const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
			CHECK(Code.has_value());
			Units += Code ? Text.Font.WidthUnits(*Code) : 0;
		}
		return Units * 4;
	};
	const auto Fits = [&](const std::vector<FSettingsRow>& Rows)
	{
		for (const FSettingsRow& Row : Rows)
		{
			INFO(Row.Label << " = " << Row.Value);
			CHECK(LabelX + Width(Row.Label) + Gap <= (Row.Cells.empty() ? ValueRight - Width(Row.Value) : FirstCursor));
			for (const std::string& Line : Row.Help)
			{
				INFO(Line);
				CHECK(Width(Line) <= HelpWidth);
			}
		}
	};
	CHECK(Width(FMenuControlsPage::Title()) <= ValueRight - LabelX);
	for (const EButtonIconSet Icons : {EButtonIconSet::Xbox, EButtonIconSet::PlayStation, EButtonIconSet::Nintendo, EButtonIconSet::SteamDeck})
	{
		for (const EInputFamily Family : {EInputFamily::KeyboardAndMouse, EInputFamily::Gamepad})
		{
			FPressedKeys Keys;
			Keys.Family = Family;
			Keys.Icons = Icons;
			FMenuControlsPage Page(Keys);
			FControlBindings Bindings = FControlBindings::Defaults();
			Fits(Page.Rows(Bindings));
			for (const std::string_view Label : ActionLabels)
			{
				// Listening on each row, and each thing it can tell: a swap, a key moved, a refusal, a key a button needs.
				const int32 Row = RowOf(Page, Label);
				REQUIRE(Page.Activate(Row, Bindings));
				Fits(Page.Rows(Bindings));
				REQUIRE(Page.Heard({"Escape"}, Bindings) == EControlsHeard::Stopped);
				for (int32 Cell = 0; Cell < ControlSlots; ++Cell)
				{
					while (Page.Cell() != Cell)
					{
						REQUIRE(Page.Step(Row, Cell > Page.Cell() ? 1 : -1, Bindings));
					}
					for (const std::string_view Other : ActionLabels)
					{
						FControlBindings Trial = FControlBindings::Defaults();
						const FButtonGlyph From = Page.Rows(Trial)[static_cast<std::size_t>(RowOf(Page, Other))].Cells[0];
						if (Other == Label || IsAxis(Other) != IsAxis(Label) || From.Key.empty())
						{
							continue;
						}
						REQUIRE(Page.Activate(Row, Trial));
						CHECK(Page.Heard({std::string(BoundKeyName(From.Key)), IsAxis(Label) ? -1 : 0}, Trial) != EControlsHeard::Ignored);
						Fits(Page.Rows(Trial));
					}
				}
				Page.Clear(Row, Bindings);
				Fits(Page.Rows(Bindings));
				Page.CursorMoved();
			}
			REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
			Fits(Page.Rows(Bindings));
			// Each layout, and what the row tells when the player's own buttons stay.
			FControlBindings Own = FControlBindings::Defaults();
			Own.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
			for (std::size_t Layout = 0; Layout < Text::ButtonLayoutValues.size(); ++Layout)
			{
				REQUIRE(Page.Step(RowOf(Page, Text::ButtonLayout.Label), 1, Own));
				Fits(Page.Rows(Own));
			}
		}
	}
}

TEST_CASE("Every key's picture fits its place on the Controls page", "[Menu]")
{
	// The pictures are drawn about their place's centre: the first's must end before the second's arrow, the
	// second's inside the list.
	constexpr int32 FirstCentre = 0x128 - 12 - 44 + 6;
	constexpr int32 SecondArrow = 0x128 - 12 - 12;
	constexpr int32 SecondCentre = 0x128 - 6;
	constexpr int32 ListInside = 0x0C + 0x124;
	const std::vector<std::string> Keyboard = {"A", "M", "W", "Zero", "Nine", "F1", "F12", "Escape", "Tab", "CapsLock", "LeftShift", "RightShift",
		"LeftControl", "RightControl", "LeftAlt", "RightAlt", "SpaceBar", "Enter", "BackSpace", "Insert", "Delete", "Home", "End", "PageUp",
		"PageDown", "Up", "Down", "Left", "Right", "Pause", "ScrollLock", "NumLock", "NumPadZero", "NumPadNine", "Multiply", "Add", "Subtract",
		"Divide", "Decimal", "Semicolon", "Equals", "Comma", "Hyphen", "Period", "Slash", "Tilde", "LeftBracket", "RightBracket", "Backslash",
		"Apostrophe", "LeftMouseButton", "RightMouseButton", "MiddleMouseButton", "ThumbMouseButton", "ThumbMouseButton2", "MouseX", "MouseY",
		"MouseWheelAxis", ""};
	const std::vector<std::string> Gamepad = {"Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Right", "Gamepad_FaceButton_Left",
		"Gamepad_FaceButton_Top", "Gamepad_LeftShoulder", "Gamepad_RightShoulder", "Gamepad_LeftTrigger", "Gamepad_RightTrigger",
		"Gamepad_Special_Left", "Gamepad_Special_Right", "Gamepad_LeftThumbstick", "Gamepad_RightThumbstick", "Gamepad_DPad_Up", "Gamepad_DPad_Down",
		"Gamepad_DPad_Left", "Gamepad_DPad_Right", "Gamepad_LeftStick_Up", "Gamepad_LeftStick_Down", "Gamepad_LeftStick_Left",
		"Gamepad_LeftStick_Right", "Gamepad_RightStick_Up", "Gamepad_RightStick_Right", "Gamepad_LeftX", "Gamepad_LeftY", "Gamepad_RightX",
		"Gamepad_RightY", ""};
	const auto Check = [&](EButtonIconSet Set, const std::vector<std::string>& Keys)
	{
		for (const std::string& Key : Keys)
		{
			const std::string Icon = Key.empty() ? std::string() : ButtonIcon(Set, Key);
			const int32 Across = (DrawMenuButtonIcon(Set, Icon, EMenuIconSlot::FaceButton).Width + MenuIconTexelsPerPixel - 1) / MenuIconTexelsPerPixel;
			INFO(Key << " as " << Icon << ": " << Across << " pixels");
			CHECK(FirstCentre + Across / 2 < SecondArrow);
			CHECK(SecondCentre + (Across + 1) / 2 <= ListInside);
		}
	};
	Check(EButtonIconSet::Keyboard, Keyboard);
	for (const EButtonIconSet Set : {EButtonIconSet::Xbox, EButtonIconSet::PlayStation, EButtonIconSet::Nintendo, EButtonIconSet::SteamDeck})
	{
		Check(Set, Gamepad);
	}
}

TEST_CASE("The Controls page's Always run row switches whether the party runs by itself", "[Menu]")
{
	Testing::FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	REQUIRE(Bindings.bAlwaysRun);
	const int32 Row = RowOf(Page, Text::AlwaysRun.Label);
	REQUIRE(Row >= 0);
	const FSettingsRow On = Page.Rows(Bindings)[static_cast<std::size_t>(Row)];
	CHECK(On.Value == OptionsText::On);
	CHECK(On.Cells.empty());
	CHECK(On.Help == OptionsText::Fill(Text::AlwaysRun.Help, {Text::KeyNoun, Text::Cancel.Label}));
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK_FALSE(Bindings.bAlwaysRun);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Row)].Value == OptionsText::Off);
	REQUIRE(Page.Activate(Row, Bindings));
	CHECK(Bindings.bAlwaysRun);
	CHECK_FALSE(Page.Listening());
	// The devices' resets leave it.
	Bindings.bAlwaysRun = false;
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK_FALSE(Bindings.bAlwaysRun);
}

TEST_CASE("The Controls page's Analog movement row switches whether the stick walks any way at its push", "[Menu]")
{
	Testing::FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	REQUIRE(Bindings.bAnalogMovement);
	const int32 Row = RowOf(Page, Text::AnalogMovement.Label);
	REQUIRE(Row == RowOf(Page, Text::AlwaysRun.Label) + 1);
	const FSettingsRow On = Page.Rows(Bindings)[static_cast<std::size_t>(Row)];
	CHECK(On.Value == OptionsText::On);
	CHECK(On.Cells.empty());
	CHECK(On.Help == OptionsText::Fill(Text::AnalogMovement.Help));
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK_FALSE(Bindings.bAnalogMovement);
	CHECK(Bindings.bAlwaysRun);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Row)].Value == OptionsText::Off);
	REQUIRE(Page.Activate(Row, Bindings));
	CHECK(Bindings.bAnalogMovement);
	CHECK_FALSE(Page.Listening());
	// The devices' resets leave it.
	Bindings.bAnalogMovement = false;
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK_FALSE(Bindings.bAnalogMovement);
}

TEST_CASE("The Controls page's touch screen rows choose the cross or a stick, how solid it is drawn and its vibration", "[Menu]")
{
	Testing::FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	const int32 Movement = RowOf(Page, Text::TouchMovement.Label);
	const int32 Opacity = RowOf(Page, Text::TouchOpacity.Label);
	const int32 Vibration = RowOf(Page, Text::TouchVibration.Label);
	REQUIRE(Movement >= 0);
	REQUIRE(Opacity == Movement + 1);
	REQUIRE(Vibration == Movement + 2);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Movement)].Help == OptionsText::Fill(Text::TouchMovement.Help));
	REQUIRE(Page.Step(Movement, 1, Bindings));
	CHECK(Bindings.TouchMovement == ETouchMovement::Stick);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Movement)].Value == "Stick");
	REQUIRE(Page.Activate(Movement, Bindings));
	CHECK(Bindings.TouchMovement == ETouchMovement::Cross);
	// The opacity steps by tenths, from 20% to the full drawing, which Confirm leaves.
	CHECK_FALSE(Page.Step(Opacity, 1, Bindings));
	CHECK_FALSE(Page.Activate(Opacity, Bindings));
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Page.Step(Opacity, -1, Bindings);
	}
	CHECK(Bindings.TouchOpacity == LowestTouchOpacity);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Opacity)].Value == "20%");
	REQUIRE(Page.Step(Vibration, 1, Bindings));
	CHECK_FALSE(Bindings.bTouchVibration);
	REQUIRE(Page.Activate(Vibration, Bindings));
	CHECK(Bindings.bTouchVibration);
	// The devices' resets leave them.
	Bindings.TouchMovement = ETouchMovement::Stick;
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK(Bindings.TouchMovement == ETouchMovement::Stick);
	CHECK(Bindings.TouchOpacity == LowestTouchOpacity);
}

TEST_CASE("The Controls page's Walk into doors row switches whether doors open without the button", "[Menu]")
{
	Testing::FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	REQUIRE(Bindings.bWalkIntoDoors);
	const int32 Row = RowOf(Page, Text::WalkIntoDoors.Label);
	REQUIRE(Row == RowOf(Page, Text::AnalogMovement.Label) + 1);
	const FSettingsRow On = Page.Rows(Bindings)[static_cast<std::size_t>(Row)];
	CHECK(On.Value == OptionsText::On);
	CHECK(On.Cells.empty());
	CHECK(On.Help == OptionsText::Fill(Text::WalkIntoDoors.Help, {Text::Confirm.Label}));
	REQUIRE(Page.Step(Row, -1, Bindings));
	CHECK_FALSE(Bindings.bWalkIntoDoors);
	CHECK(Bindings.bAlwaysRun);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Row)].Value == OptionsText::Off);
	REQUIRE(Page.Activate(Row, Bindings));
	CHECK(Bindings.bWalkIntoDoors);
	CHECK_FALSE(Page.Listening());
	// The devices' resets leave it.
	Bindings.bWalkIntoDoors = false;
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK_FALSE(Bindings.bWalkIntoDoors);
}

TEST_CASE("Original PSX icons can be selected without changing the Controls page's bound keys", "[Menu][Input]")
{
	FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Rebind(EControlAction::Cross, 0, "K");
	const FControlBindings Before = Bindings;
	const int32 Row = RowOf(Page, Text::OriginalButtonIcons.Label);
	CHECK(RowNamed(Page, Bindings, Text::OriginalButtonIcons.Label).Value == OptionsText::Off);
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK(Bindings.bOriginalButtonIcons);
	CHECK(RowNamed(Page, Bindings, Text::OriginalButtonIcons.Label).Value == OptionsText::On);
	CHECK(RowNamed(Page, Bindings, Text::OriginalButtonIcons.Label).Help == OptionsText::Fill(Text::OriginalButtonIcons.Help));
	CHECK(RowNamed(Page, Bindings, Text::Confirm.Label).Cells[0] == FButtonGlyph{EButtonIconSet::Keyboard, "K", "K"});
	Keys.Family = EInputFamily::Gamepad;
	Keys.Icons = EButtonIconSet::Xbox;
	REQUIRE(Page.Follow());
	CHECK(RowNamed(Page, Bindings, Text::Confirm.Label).Cells[0].Icon == "A");
	REQUIRE(Page.Activate(Row, Bindings));
	CHECK(Bindings == Before);
	CHECK_FALSE(Page.Listening());
}

TEST_CASE("The Button layout row moves the controller's face buttons and tells when the player's own stay", "[Menu][Input]")
{
	FPressedKeys Keys;
	Keys.Family = EInputFamily::Gamepad;
	Keys.Icons = EButtonIconSet::PlayStation;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	const int32 Row = RowOf(Page, Text::ButtonLayout.Label);
	const auto Shown = [&] { return RowNamed(Page, Bindings, Text::ButtonLayout.Label); };
	CHECK(Shown().Value == Text::ButtonLayoutValues[0]);
	CHECK(Shown().Help == OptionsText::Fill(Text::ButtonLayout.Help));

	// Confirm steps forward; Left and Right step either way, round the three.
	REQUIRE(Page.Activate(Row, Bindings));
	CHECK(Bindings.ButtonLayout == EButtonLayout::CircleConfirms);
	CHECK(Shown().Value == Text::ButtonLayoutValues[1]);
	CHECK(RowNamed(Page, Bindings, Text::Confirm.Label).Cells[0] == FButtonGlyph{EButtonIconSet::PlayStation, "Gamepad_FaceButton_Right", "Circle"});
	CHECK(RowNamed(Page, Bindings, Text::Cancel.Label).Cells[0].Icon == "Cross");
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK(Shown().Value == Text::ButtonLayoutValues[2]);
	CHECK(RowNamed(Page, Bindings, Text::Menu.Label).Cells[0].Icon == "Triangle");
	CHECK(RowNamed(Page, Bindings, Text::Jump.Label).Cells[0].Icon == "Square");
	CHECK(Shown().Help[2].empty());
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK(Bindings == FControlBindings::Defaults());
	REQUIRE(Page.Step(Row, -1, Bindings));
	CHECK(Bindings == FControlBindings::Defaults(EButtonLayout::Japan));

	// The player's own confirm and cancel stay, and the row says so until the cursor leaves it.
	REQUIRE(Page.Step(Row, 1, Bindings));
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Top");
	REQUIRE(Page.Step(Row, 1, Bindings));
	CHECK(Bindings.ButtonLayout == EButtonLayout::CircleConfirms);
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::Gamepad) == "Gamepad_FaceButton_Top");
	CHECK(Shown().Help[2] == Text::OwnButtonsKept);
	Page.CursorMoved();
	CHECK(Shown().Help[2].empty());

	// The reset gives the layout's own buttons.
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK(Bindings == FControlBindings::Defaults(EButtonLayout::CircleConfirms));
}

TEST_CASE("The Controls page's sensitivity rows step the mouse's and the stick's levels", "[Menu]")
{
	Testing::FPressedKeys Keys;
	FMenuControlsPage Page(Keys);
	FControlBindings Bindings = FControlBindings::Defaults();
	const int32 Mouse = RowOf(Page, Text::MouseSensitivity.Label);
	const int32 Stick = RowOf(Page, Text::StickSensitivity.Label);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Mouse)].Value == std::to_string(DefaultSensitivity));
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Stick)].Help[0] == Text::StickSensitivity.Help[0]);
	REQUIRE(Page.Step(Mouse, 1, Bindings));
	CHECK(Bindings.MouseSensitivity == DefaultSensitivity + 1);
	CHECK(Bindings.StickSensitivity == DefaultSensitivity);
	CHECK(Page.Rows(Bindings)[static_cast<std::size_t>(Mouse)].Value == std::to_string(DefaultSensitivity + 1));
	// Within the levels, no further; Confirm does nothing there.
	for (int32 Step = 0; Step < HighestSensitivity; ++Step)
	{
		Page.Step(Stick, -1, Bindings);
	}
	CHECK(Bindings.StickSensitivity == LowestSensitivity);
	CHECK_FALSE(Page.Step(Stick, -1, Bindings));
	CHECK_FALSE(Page.Activate(Stick, Bindings));
	CHECK_FALSE(Page.Listening());
	// The devices' resets leave them.
	REQUIRE(Page.Activate(static_cast<int32>(Page.Rows(Bindings).size()) - 1, Bindings));
	CHECK(Bindings.MouseSensitivity == DefaultSensitivity + 1);
}

TEST_CASE("A sensitivity's level scales the view's speed by steps of one ratio, the middle level a new player's", "[Input]")
{
	// The mouse in the middle, and a third of that at its lowest level.
	CHECK(MouseDegreesPerCount(DefaultSensitivity) == Catch::Approx(0.585f));
	CHECK(MouseDegreesPerCount(LowestSensitivity) == Catch::Approx(0.195f));
	CHECK(MouseDegreesPerCount(HighestSensitivity) / MouseDegreesPerCount(HighestSensitivity - 1)
		== Catch::Approx(MouseDegreesPerCount(LowestSensitivity + 1) / MouseDegreesPerCount(LowestSensitivity)));
	// A stick at the standard rates in the middle, half of them at the lowest.
	CHECK(StickRateScale(DefaultSensitivity) == Catch::Approx(1.0f));
	CHECK(StickRateScale(LowestSensitivity) == Catch::Approx(0.5f));
	CHECK(StickRateScale(HighestSensitivity) > 2.0f);
	CHECK(StickRateScale(HighestSensitivity + 5) == StickRateScale(HighestSensitivity));
}
