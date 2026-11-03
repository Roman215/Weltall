#include "Game/MessageLogScreen.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesCharacters.h"
#include "Text/MessageLog.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr std::array<uint16, 32> TextClut{};
	constexpr std::array<uint8, 3> WindowColour{136, 118, 84};
	constexpr double Frame = 1.0 / 60.0;

	/** Plain text in the game's codes: '\n' a line's end, '@' the font's Circle, then the string's end. */
	std::vector<uint8> Encode(std::string_view Text)
	{
		std::vector<uint8> Bytes;
		for (std::size_t At = 0; At < Text.size(); ++At)
		{
			Bytes.push_back(Text[At] == '\n' ? 0x01 : Text[At] == '@' ? 0x6E : FMesCharacters::Plain().CodeAt(Text.substr(At))->Code.Code);
		}
		Bytes.push_back(0x00);
		return Bytes;
	}

	/** A log of the messages, each typed whole in a window of its own. */
	void LogMessages(FMessageLog& Log, const FMesFont& Font, const std::vector<std::string>& Messages)
	{
		FMesContext Context;
		Context.Font = &Font;
		Context.Log = &Log;
		Context.Speed = EMesTextSpeed::Instant;
		for (const std::string& Text : Messages)
		{
			FMesWindow Window(70, 4);
			Window.Queue(Encode(Text));
			for (int32 Tick = 0; Tick < 100 && (Window.HasText() || Window.QueuedCount() != 0); ++Tick)
			{
				if (Window.IsStopped())
				{
					Window.Continue();
				}
				UpdateLoggedWindow(Window, Context, {});
			}
		}
	}

	/** The text sprites a build drew: those reading the pictures' text strips. */
	std::vector<FDrawPrimitive> TextSprites(const FDrawList& List)
	{
		std::vector<FDrawPrimitive> Sprites;
		std::copy_if(List.Primitives.begin(), List.Primitives.end(), std::back_inserter(Sprites),
			[](const FDrawPrimitive& Primitive) { return Primitive.Texture.has_value() && Primitive.Texture->Page.Depth == ETextureDepth::Indexed4; });
		return Sprites;
	}

	/** Frames of a press of Button: one held, then let go, and the glide given time to come to rest. */
	void Press(FMessageLogScreen& Screen, EPadButton Button)
	{
		FPadState Held;
		Held.Buttons = FPadState::Bit(Button);
		REQUIRE(Screen.Update(Frame, Held));
		for (int32 Rest = 0; Rest < 60; ++Rest)
		{
			REQUIRE(Screen.Update(Frame, {}));
		}
	}

	float TopOf(const FDrawPrimitive& Sprite)
	{
		return Sprite.Corners[0].Y;
	}
}

TEST_CASE("The message log opens at its newest line and scrolls a whole line at a time to its first", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	FMessageLog Log;
	std::vector<std::string> Messages;
	for (int32 Number = 0; Number < 20; ++Number)
	{
		Messages.push_back("Elly\n\"Message " + std::to_string(Number) + "\nsays this.\"");
	}
	LogMessages(Log, Resident.Font, Messages);
	REQUIRE(Log.Messages().size() == 20);

	FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
	CHECK(Screen.IsAtNewest());
	FDrawList List;
	std::vector<FTextButtonCell> Cells;
	REQUIRE(Screen.Update(Frame, {}));
	Screen.Build(List, Cells);
	// The newest line's bottom at the area's.
	const int32 AreaBottom = FMessageLogScreen::TextArea.Y + FMessageLogScreen::TextArea.Height;
	std::vector<FDrawPrimitive> Sprites = TextSprites(List);
	REQUIRE_FALSE(Sprites.empty());
	CHECK(TopOf(Sprites.back()) + FMessageLogScreen::LineSpacing == AreaBottom);
	// The whole display darkened first, by the windows' colour taken from it.
	REQUIRE_FALSE(List.Primitives.empty());
	CHECK(List.Primitives.front().bSpansDisplay);
	CHECK(List.Primitives.front().UntexturedBlend == EBlendMode::Subtract);
	// Its clip leaves nothing out to either side of the screen, where a wide display shows the game.
	CHECK(List.Primitives.front().Clip.X < -320);
	CHECK(List.Primitives.front().Clip.X + List.Primitives.front().Clip.Width > 640);

	// A line up: the line above the view's top comes whole into it.
	const float FirstTop = TopOf(Sprites.front());
	REQUIRE(FirstTop < FMessageLogScreen::TextArea.Y);
	Press(Screen, EPadButton::Up);
	CHECK_FALSE(Screen.IsAtNewest());
	Screen.Build(List, Cells);
	Sprites = TextSprites(List);
	const auto Whole = std::find_if(Sprites.begin(), Sprites.end(), [](const FDrawPrimitive& Sprite) { return TopOf(Sprite) >= FMessageLogScreen::TextArea.Y; });
	REQUIRE(Whole != Sprites.end());
	CHECK(TopOf(*Whole) == FMessageLogScreen::TextArea.Y);

	// Up again and again: the log's first line at the area's top, and no further.
	for (int32 Line = 0; Line < 100; ++Line)
	{
		Screen.Scroll(1);
	}
	for (int32 Rest = 0; Rest < 60; ++Rest)
	{
		Screen.Update(Frame, {});
	}
	Screen.Build(List, Cells);
	Sprites = TextSprites(List);
	CHECK(TopOf(Sprites.front()) == FMessageLogScreen::TextArea.Y);

	// R1 a page down at a time, back to the newest.
	for (int32 Page = 0; Page < 20; ++Page)
	{
		Press(Screen, EPadButton::R1);
	}
	CHECK(Screen.IsAtNewest());
	Screen.Build(List, Cells);
	Sprites = TextSprites(List);
	CHECK(TopOf(Sprites.back()) + FMessageLogScreen::LineSpacing == AreaBottom);
}

TEST_CASE("The message log glides to a line, repeats a held direction and closes on Confirm or Cancel once pressed", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	FMessageLog Log;
	std::vector<std::string> Messages(30, "Line one\nLine two");
	LogMessages(Log, Resident.Font, Messages);

	FPadState Cancel;
	Cancel.Buttons = FPadState::Bit(EPadButton::Circle);
	{
		// Cancel held as it opens (the key that opened it, a press the game had) closes nothing until pressed anew.
		FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
		CHECK(Screen.Update(Frame, Cancel));
		CHECK(Screen.Update(Frame, Cancel));
		CHECK(Screen.Update(Frame, {}));
		CHECK_FALSE(Screen.Update(Frame, Cancel));
	}
	{
		FPadState Confirm;
		Confirm.Buttons = FPadState::Bit(EPadButton::Cross);
		FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
		CHECK(Screen.Update(Frame, {}));
		CHECK_FALSE(Screen.Update(Frame, Confirm));
	}

	FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
	FDrawList List;
	std::vector<FTextButtonCell> Cells;
	REQUIRE(Screen.Update(Frame, {}));
	Screen.Build(List, Cells);
	const float Before = TopOf(TextSprites(List).back());
	// Up pressed: a frame later the text has moved part of a line down, and comes to rest a line on.
	FPadState Up;
	Up.Buttons = FPadState::Bit(EPadButton::Up);
	REQUIRE(Screen.Update(Frame, Up));
	REQUIRE(Screen.Update(Frame, {}));
	Screen.Build(List, Cells);
	const float Gliding = TopOf(TextSprites(List).back());
	for (int32 Rest = 0; Rest < 60; ++Rest)
	{
		Screen.Update(Frame, {});
	}
	Screen.Build(List, Cells);
	const std::vector<FDrawPrimitive> Rested = TextSprites(List);
	CHECK(Gliding > Before);
	CHECK(Gliding < TopOf(Rested.back()));
	const auto Whole = std::find_if(Rested.begin(), Rested.end(), [](const FDrawPrimitive& Sprite) { return TopOf(Sprite) >= FMessageLogScreen::TextArea.Y; });
	REQUIRE(Whole != Rested.end());
	CHECK(TopOf(*Whole) == FMessageLogScreen::TextArea.Y);

	// Held: one line at once, then from the delay a line each repeat.
	FMessageLogScreen Held(Log, Resident.Font, TextClut, WindowColour);
	REQUIRE(Held.Update(Frame, {}));
	const int32 Frames = 60;
	for (int32 Step = 0; Step < Frames; ++Step)
	{
		REQUIRE(Held.Update(Frame, Up));
	}
	for (int32 Rest = 0; Rest < 120; ++Rest)
	{
		Held.Update(Frame, {});
	}
	Held.Build(List, Cells);
	// The line moved down by one per scroll, with the gaps between messages passed over: count the lines by the text's movement.
	FMessageLogScreen Counted(Log, Resident.Font, TextClut, WindowColour);
	REQUIRE(Counted.Update(Frame, {}));
	const int32 Repeats = static_cast<int32>((Frames * Frame - FMessageLogScreen::RepeatDelaySeconds) / FMessageLogScreen::RepeatSeconds);
	Counted.Scroll(1 + Repeats);
	for (int32 Rest = 0; Rest < 120; ++Rest)
	{
		Counted.Update(Frame, {});
	}
	FDrawList Expected;
	Counted.Build(Expected, Cells);
	const float HeldTop = TopOf(TextSprites(List).back());
	const float ExpectedTop = TopOf(TextSprites(Expected).back());
	CHECK(std::abs(HeldTop - ExpectedTop) <= FMessageLogScreen::LineSpacing + FMessageLogScreen::MessageGap);
}

TEST_CASE("The message log tints a speaker's name, draws a choice in its window's colour and shows the font's buttons as buttons", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	FMessageLog Log;
	LogMessages(Log, Resident.Font, {"Fei\n\"Hello!\"", "Press @ to jump.", "Which?\n    Yes\n    No"});
	FMesWindow Window(70, 4);
	Window.SetColor(239, 30, 240);
	{
		// The choice's window is the last message's: ended on its third row.
		FMesContext Context;
		Context.Font = &Resident.Font;
		Context.Log = &Log;
		Context.Speed = EMesTextSpeed::Instant;
		Window.Queue(Encode("Which?\n    Yes\n    No"));
		for (int32 Tick = 0; Tick < 10; ++Tick)
		{
			UpdateLoggedWindow(Window, Context, {EMessageLogSource::Field, 3});
		}
		LogChoice(Context, {EMessageLogSource::Field, 3}, Window, 2);
	}
	REQUIRE(Log.Messages().size() == 4);
	REQUIRE(Log.Messages().back().Chosen == 2);

	FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
	FDrawList List;
	std::vector<FTextButtonCell> Cells;
	REQUIRE(Screen.Update(Frame, {}));
	Screen.Build(List, Cells);
	const std::vector<FDrawPrimitive> Sprites = TextSprites(List);
	// Fei, "Hello!", the button's line, then the two choices' three lines each.
	REQUIRE(Sprites.size() == 9);
	CHECK(Sprites[0].bModulate);
	CHECK_FALSE(Sprites[0].Corners[0].Color == FDrawColor{});
	CHECK_FALSE(Sprites[1].bModulate);
	CHECK_FALSE(Sprites[2].bModulate);
	// The unchosen copy plain, the chosen line in the window's colour.
	CHECK_FALSE(Sprites[5].bModulate);
	CHECK(Sprites[8].bModulate);
	CHECK(Sprites[8].Corners[0].Color == FDrawColor{239, 30, 240});
	// The font's Circle on the button's line, at its sixth glyph.
	REQUIRE(Cells.size() == 1);
	CHECK(Cells[0].Button == EPadButton::Circle);
	CHECK(List.Primitives[Cells[0].Primitive].Corners[0].Y == Sprites[2].Corners[0].Y);
	CHECK(Cells[0].OffsetX == Log.Messages()[1].Lines[0][6].XUnits * 4);
}

TEST_CASE("The message log says so while it has nothing", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const FMessageLog Log;
	FMessageLogScreen Screen(Log, Resident.Font, TextClut, WindowColour);
	FDrawList List;
	std::vector<FTextButtonCell> Cells;
	REQUIRE(Screen.Update(Frame, {}));
	Screen.Build(List, Cells);
	const std::vector<FDrawPrimitive> Sprites = TextSprites(List);
	REQUIRE(Sprites.size() == 1);
	CHECK(TopOf(Sprites[0]) == FMessageLogScreen::TextArea.Y);
	// Its width is the line's: every letter of it typed.
	CHECK(Sprites[0].Corners[1].X - Sprites[0].Corners[0].X > 100.0f);
	CHECK(Screen.IsAtNewest());
}
