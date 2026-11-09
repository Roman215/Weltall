#include "Input/ControlBindings.h"
#include "Game/PlayerSettings.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("Controller shortcuts round-trip and are exclusive by ordered pair", "[Input]")
{
	const std::string Chord = "Gamepad_Special_Left+Gamepad_RightShoulder";
	const std::string Reversed = "Gamepad_RightShoulder+Gamepad_Special_Left";
	const auto Parsed = ControllerChordOf(Chord);
	REQUIRE(Parsed);
	CHECK(Parsed->Modifier == "Gamepad_Special_Left");
	CHECK(Parsed->Trigger == "Gamepad_RightShoulder");
	CHECK_FALSE(ControllerChordOf("Gamepad_Special_Left+Gamepad_Special_Left"));
	CHECK_FALSE(ControllerChordOf("Gamepad_Special_Left+F5"));
	CHECK_FALSE(ControllerChordOf("Gamepad_Special_Left+Gamepad_RightShoulder+Gamepad_LeftShoulder"));

	FPlayerSettings Settings;
	Settings.Controls.Rebind(EControlAction::SaveState, 0, Chord);
	Settings.Controls.Rebind(EControlAction::LoadState, 0, Reversed);
	Settings.Controls.Rebind(EControlAction::NextStateSlot, 0, Chord);
	CHECK(Settings.Controls.FirstKey(EControlAction::SaveState, EInputFamily::Gamepad).empty());
	CHECK(Settings.Controls.FirstKey(EControlAction::NextStateSlot, EInputFamily::Gamepad) == Chord);
	CHECK(Settings.Controls.FirstKey(EControlAction::LoadState, EInputFamily::Gamepad) == Reversed);
	CHECK(Settings.Controls.FirstKey(EControlAction::Select, EInputFamily::Gamepad) == Parsed->Modifier);
	CHECK(Settings.Controls.FirstKey(EControlAction::R1, EInputFamily::Gamepad) == Parsed->Trigger);
	CHECK(ReadPlayerSettings(WritePlayerSettings(Settings)).Controls == Settings.Controls);
	Settings.Controls.ResetFamily(EInputFamily::Gamepad);
	CHECK(Settings.Controls == FControlBindings::Defaults());
}

TEST_CASE("Every PlayStation button but the stick buttons has its action", "[Input]")
{
	int32 Actions = 0;
	for (int32 Bit = 0; Bit < 16; ++Bit)
	{
		const EPadButton Button = static_cast<EPadButton>(Bit);
		const std::optional<EControlAction> Action = ActionOf(Button);
		CHECK(Action.has_value() == (Button != EPadButton::L3 && Button != EPadButton::R3));
		if (Action)
		{
			CHECK(PadButtonOf(*Action) == Button);
			++Actions;
		}
	}
	CHECK(Actions == 14);
	CHECK_FALSE(PadButtonOf(EControlAction::ViewTurn).has_value());
	CHECK_FALSE(PadButtonOf(EControlAction::ViewReset).has_value());
}

TEST_CASE("A key's family and direction come from its name", "[Input]")
{
	CHECK(FamilyOfKey("Gamepad_FaceButton_Bottom") == EInputFamily::Gamepad);
	CHECK(FamilyOfKey("-Gamepad_RightY") == EInputFamily::Gamepad);
	CHECK(FamilyOfKey("K") == EInputFamily::KeyboardAndMouse);
	CHECK(FamilyOfKey("-MouseY") == EInputFamily::KeyboardAndMouse);
	CHECK(IsNegatedBinding("-MouseY"));
	CHECK_FALSE(IsNegatedBinding("MouseY"));
	CHECK(BoundKeyName("-MouseY") == "MouseY");
}

TEST_CASE("The default bindings put a key on one action and every button on both families", "[Input]")
{
	const FControlBindings Bindings = FControlBindings::Defaults();
	// The tilt is a setting: nothing tilts the view unless the player binds it.
	CHECK(Bindings.Keys(EControlAction::ViewTilt).empty());
	for (int32 Action = 0; Action < ControlActionCount; ++Action)
	{
		const EControlAction Each = static_cast<EControlAction>(Action);
		if (Each == EControlAction::ViewTilt)
		{
			continue;
		}
		// A controller's every button already has its part in the game: the save states are the keyboard's alone, and the
		// message log takes the left stick's press, which has none.
		CHECK(Bindings.FirstKey(Each, EInputFamily::Gamepad).empty() == (IsPortAction(Each) && Each != EControlAction::MessageLog));
		CHECK_FALSE(Bindings.FirstKey(Each, EInputFamily::KeyboardAndMouse).empty());
		for (const std::string& Key : Bindings.Keys(Each))
		{
			for (int32 Other = Action + 1; Other < ControlActionCount; ++Other)
			{
				for (const std::string& OtherKey : Bindings.Keys(static_cast<EControlAction>(Other)))
				{
					CHECK(BoundKeyName(Key) != BoundKeyName(OtherKey));
				}
			}
		}
	}
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::Gamepad) == "Gamepad_FaceButton_Bottom");
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::KeyboardAndMouse) == "E");
	CHECK(Bindings.FirstKey(EControlAction::Circle, EInputFamily::KeyboardAndMouse) == "Q");
	CHECK(Bindings.FirstKey(EControlAction::Square, EInputFamily::KeyboardAndMouse) == "F");
	CHECK(Bindings.FirstKey(EControlAction::Triangle, EInputFamily::KeyboardAndMouse) == "SpaceBar");
	CHECK(Bindings.FirstKey(EControlAction::L1, EInputFamily::KeyboardAndMouse) == "Tab");
	CHECK(Bindings.FirstKey(EControlAction::R1, EInputFamily::KeyboardAndMouse) == "R");
	CHECK(Bindings.FirstKey(EControlAction::L2, EInputFamily::KeyboardAndMouse) == "One");
	CHECK(Bindings.FirstKey(EControlAction::R2, EInputFamily::KeyboardAndMouse) == "Four");
	CHECK(Bindings.Keys(EControlAction::SaveState) == std::vector<std::string>{"F5"});
	CHECK(Bindings.Keys(EControlAction::LoadState) == std::vector<std::string>{"F9"});
	CHECK(Bindings.Keys(EControlAction::NextStateSlot) == std::vector<std::string>{"F7"});
	CHECK(Bindings.Keys(EControlAction::PreviousStateSlot) == std::vector<std::string>{"F6"});
	CHECK(Bindings.Keys(EControlAction::MessageLog) == std::vector<std::string>{"L", "Gamepad_LeftThumbstick"});
	CHECK_FALSE(PadButtonOf(EControlAction::SaveState).has_value());
	CHECK_FALSE(PadButtonOf(EControlAction::MessageLog).has_value());
	CHECK(IsPortAction(EControlAction::SaveState));
	CHECK(IsPortAction(EControlAction::MessageLog));
	CHECK_FALSE(IsPortAction(EControlAction::ViewReset));
	CHECK_FALSE(IsViewAxis(EControlAction::LoadState));
}

TEST_CASE("Binding a key replaces its family's slot and takes it off other actions", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Bind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	CHECK(Bindings.Keys(EControlAction::Cross) == std::vector<std::string>{"E", "Enter", "Gamepad_FaceButton_Right"});
	CHECK(Bindings.Keys(EControlAction::Circle) == std::vector<std::string>{"Q", "Escape"});
	CHECK(Bindings.FirstKey(EControlAction::Circle, EInputFamily::Gamepad).empty());

	Bindings.Bind(EControlAction::Up, 1, "SpaceBar");
	CHECK(Bindings.Keys(EControlAction::Up) == std::vector<std::string>{"W", "SpaceBar", "Gamepad_DPad_Up", "Gamepad_LeftStick_Up"});
	CHECK(Bindings.Keys(EControlAction::Triangle) == std::vector<std::string>{"Gamepad_FaceButton_Top"});

	Bindings.Bind(EControlAction::Start, 5, "O");
	CHECK(Bindings.Keys(EControlAction::Start) == std::vector<std::string>{"P", "Gamepad_Special_Right", "O"});

	// An axis bound the other way round is the same key.
	Bindings.Bind(EControlAction::ViewTurn, 0, "-MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"-MouseY", "Gamepad_RightX"});
	CHECK(Bindings.Keys(EControlAction::ViewTilt).empty());

	Bindings.Unbind("Gamepad_RightX");
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"-MouseY"});
	Bindings.Bind(EControlAction::ViewZoom, 0, "");
	CHECK(Bindings.Keys(EControlAction::ViewZoom) == FControlBindings::Defaults().Keys(EControlAction::ViewZoom));
}

TEST_CASE("Rebinding a key gives the action it leaves the key it replaced, or takes it off", "[Input]")
{
	using Keys = std::vector<std::string>;
	FControlBindings Bindings = FControlBindings::Defaults();
	// Q, Circle's, in Cross's first slot: Circle takes E in its place.
	Bindings.Rebind(EControlAction::Cross, 0, "Q");
	CHECK(Bindings.Keys(EControlAction::Cross) == Keys{"Q", "Enter", "Gamepad_FaceButton_Bottom"});
	CHECK(Bindings.Keys(EControlAction::Circle) == Keys{"E", "Escape", "Gamepad_FaceButton_Right"});

	// Into a free slot: the key just leaves its action, from wherever it was among that action's keys.
	Bindings.Rebind(EControlAction::Cross, 1, "Gamepad_DPad_Up");
	CHECK(Bindings.Keys(EControlAction::Cross) == Keys{"Q", "Enter", "Gamepad_FaceButton_Bottom", "Gamepad_DPad_Up"});
	CHECK(Bindings.Keys(EControlAction::Up) == Keys{"W", "Up", "Gamepad_LeftStick_Up"});
	CHECK(Bindings.KeyAt(EControlAction::Up, EInputFamily::Gamepad, 0) == "Gamepad_LeftStick_Up");

	// An action's own key in its other slot: the two change places, the sign the new binding's.
	Bindings.Rebind(EControlAction::Up, 0, "Up");
	CHECK(Bindings.Keys(EControlAction::Up) == Keys{"Up", "W", "Gamepad_LeftStick_Up"});
	Bindings.Rebind(EControlAction::ViewTilt, 0, "MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTilt) == Keys{"MouseY"});
	// Its own key asked for a free slot after it: it stays where it is.
	Bindings.Rebind(EControlAction::ViewTilt, 1, "-MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTilt) == Keys{"-MouseY"});

	// A free key replaces the slot's, which is then nobody's.
	Bindings.Rebind(EControlAction::Start, 0, "O");
	CHECK(Bindings.Keys(EControlAction::Start) == Keys{"O", "Gamepad_Special_Right"});
	CHECK_FALSE(Bindings.Find("P").has_value());
	Bindings.Rebind(EControlAction::Start, 0, "");
	CHECK(Bindings.Keys(EControlAction::Start) == Keys{"O", "Gamepad_Special_Right"});
}

TEST_CASE("A binding's slots are counted within its family, and a family is put back alone", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 1) == "Down");
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::Gamepad, 1) == "Gamepad_LeftStick_Down");
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::Gamepad, 2).empty());
	CHECK(Bindings.Count(EControlAction::Square, EInputFamily::Gamepad) == 1);
	CHECK(Bindings.Count(EControlAction::ViewTilt, EInputFamily::Gamepad) == 0);
	CHECK(Bindings.Find("Gamepad_LeftStick_Down") == FBoundPlace{EControlAction::Down, 1});
	CHECK(Bindings.Find("-Gamepad_RightY") == FBoundPlace{EControlAction::ViewZoom, 0});
	CHECK_FALSE(Bindings.Find("F12").has_value());

	Bindings.Clear(EControlAction::Down, EInputFamily::KeyboardAndMouse, 0);
	Bindings.Clear(EControlAction::Down, EInputFamily::KeyboardAndMouse, 5);
	CHECK(Bindings.Keys(EControlAction::Down) == std::vector<std::string>{"Down", "Gamepad_DPad_Down", "Gamepad_LeftStick_Down"});
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	Bindings.Rebind(EControlAction::Select, 1, "Insert");

	const auto AsDefaults = [&Bindings](EInputFamily Family)
	{
		const FControlBindings Defaults = FControlBindings::Defaults();
		for (int32 Action = 0; Action < ControlActionCount; ++Action)
		{
			const EControlAction Each = static_cast<EControlAction>(Action);
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if (Bindings.KeyAt(Each, Family, Slot) != Defaults.KeyAt(Each, Family, Slot))
				{
					return false;
				}
			}
		}
		return true;
	};

	// The controller's buttons back as they start; the keyboard's changes stay.
	Bindings.ResetFamily(EInputFamily::Gamepad);
	CHECK(AsDefaults(EInputFamily::Gamepad));
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 0) == "Down");
	CHECK(Bindings.KeyAt(EControlAction::Select, EInputFamily::KeyboardAndMouse, 1) == "Insert");
	Bindings.ResetFamily(EInputFamily::KeyboardAndMouse);
	CHECK(AsDefaults(EInputFamily::KeyboardAndMouse));
	CHECK(AsDefaults(EInputFamily::Gamepad));
}

TEST_CASE("A button layout's defaults put the face buttons where its release has them", "[Input]")
{
	const auto PadKey = [](const FControlBindings& Bindings, EControlAction Action) { return Bindings.FirstKey(Action, EInputFamily::Gamepad); };
	const FControlBindings Usa = FControlBindings::Defaults();
	CHECK(Usa.ButtonLayout == EButtonLayout::Usa);
	CHECK(PadKey(Usa, EControlAction::Cross) == "Gamepad_FaceButton_Bottom");
	CHECK(PadKey(Usa, EControlAction::Circle) == "Gamepad_FaceButton_Right");
	CHECK(PadKey(Usa, EControlAction::Square) == "Gamepad_FaceButton_Left");
	CHECK(PadKey(Usa, EControlAction::Triangle) == "Gamepad_FaceButton_Top");

	const FControlBindings Circle = FControlBindings::Defaults(EButtonLayout::CircleConfirms);
	CHECK(Circle.ButtonLayout == EButtonLayout::CircleConfirms);
	CHECK(PadKey(Circle, EControlAction::Cross) == "Gamepad_FaceButton_Right");
	CHECK(PadKey(Circle, EControlAction::Circle) == "Gamepad_FaceButton_Bottom");
	CHECK(PadKey(Circle, EControlAction::Square) == "Gamepad_FaceButton_Left");
	CHECK(PadKey(Circle, EControlAction::Triangle) == "Gamepad_FaceButton_Top");

	// The Japanese kernel's own buttons: the USA's menu (its Square) on Triangle, its jump (its Triangle) on Square.
	const FControlBindings Japan = FControlBindings::Defaults(EButtonLayout::Japan);
	CHECK(PadKey(Japan, EControlAction::Cross) == "Gamepad_FaceButton_Right");
	CHECK(PadKey(Japan, EControlAction::Circle) == "Gamepad_FaceButton_Bottom");
	CHECK(PadKey(Japan, EControlAction::Square) == "Gamepad_FaceButton_Top");
	CHECK(PadKey(Japan, EControlAction::Triangle) == "Gamepad_FaceButton_Left");

	// The keyboard's keys and every other button are the same in all three.
	for (int32 Action = 0; Action < ControlActionCount; ++Action)
	{
		const EControlAction Each = static_cast<EControlAction>(Action);
		INFO(Action);
		for (const FControlBindings* Other : {&Circle, &Japan})
		{
			CHECK(Other->Count(Each, EInputFamily::KeyboardAndMouse) == Usa.Count(Each, EInputFamily::KeyboardAndMouse));
			CHECK(Other->Count(Each, EInputFamily::Gamepad) == Usa.Count(Each, EInputFamily::Gamepad));
			for (int32 Slot = 0; Slot < Usa.Count(Each, EInputFamily::KeyboardAndMouse); ++Slot)
			{
				CHECK(Other->KeyAt(Each, EInputFamily::KeyboardAndMouse, Slot) == Usa.KeyAt(Each, EInputFamily::KeyboardAndMouse, Slot));
			}
			if (Each != EControlAction::Cross && Each != EControlAction::Circle && Each != EControlAction::Square && Each != EControlAction::Triangle)
			{
				CHECK(Other->Keys(Each) == Usa.Keys(Each));
			}
		}
	}
}

TEST_CASE("Another layout moves the face buttons still at the old one's places and keeps the player's own", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Rebind(EControlAction::Cross, 0, "K");
	CHECK(Bindings.ChangeButtonLayout(EButtonLayout::Japan));
	CHECK(Bindings.ButtonLayout == EButtonLayout::Japan);
	FControlBindings Expected = FControlBindings::Defaults(EButtonLayout::Japan);
	Expected.Rebind(EControlAction::Cross, 0, "K");
	CHECK(Bindings == Expected);
	// Japan to Circle confirms changes only menu and jump back.
	CHECK(Bindings.ChangeButtonLayout(EButtonLayout::CircleConfirms));
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::Gamepad) == "Gamepad_FaceButton_Right");
	CHECK(Bindings.FirstKey(EControlAction::Square, EInputFamily::Gamepad) == "Gamepad_FaceButton_Left");
	CHECK(Bindings.FirstKey(EControlAction::Triangle, EInputFamily::Gamepad) == "Gamepad_FaceButton_Top");
	CHECK(Bindings.ChangeButtonLayout(EButtonLayout::Usa));
	Expected = FControlBindings::Defaults();
	Expected.Rebind(EControlAction::Cross, 0, "K");
	CHECK(Bindings == Expected);

	// A player who put confirm on Circle themselves keeps it; menu and jump, still at their places, move.
	Bindings = FControlBindings::Defaults();
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	const FControlBindings Own = Bindings;
	CHECK_FALSE(Bindings.ChangeButtonLayout(EButtonLayout::Japan));
	CHECK(Bindings.ButtonLayout == EButtonLayout::Japan);
	CHECK(Bindings.Keys(EControlAction::Cross) == Own.Keys(EControlAction::Cross));
	CHECK(Bindings.Keys(EControlAction::Circle) == Own.Keys(EControlAction::Circle));
	CHECK(Bindings.FirstKey(EControlAction::Square, EInputFamily::Gamepad) == "Gamepad_FaceButton_Top");
	CHECK(Bindings.FirstKey(EControlAction::Triangle, EInputFamily::Gamepad) == "Gamepad_FaceButton_Left");
	// A second key on one of a pair keeps the pair too.
	Bindings = FControlBindings::Defaults();
	Bindings.Rebind(EControlAction::Circle, 1, "Gamepad_LeftThumbstick");
	CHECK_FALSE(Bindings.ChangeButtonLayout(EButtonLayout::CircleConfirms));
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::Gamepad) == "Gamepad_FaceButton_Bottom");

	// The reset puts back the layout's own.
	Bindings.ResetFamily(EInputFamily::Gamepad);
	CHECK(Bindings == FControlBindings::Defaults(EButtonLayout::CircleConfirms));
}
