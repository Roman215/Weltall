#include "Text/MesCharacters.h"
#include "Text/MessageLog.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr FMessageLogKey FirstWindow{EMessageLogSource::Field, 0};
	constexpr FMessageLogKey SecondWindow{EMessageLogSource::Field, 1};

	/** Plain text in the game's codes: '\n' a line's end, '|' a page, '^' a hold (with its line end), then the string's end. */
	std::vector<uint8> Encode(std::string_view Text)
	{
		std::vector<uint8> Bytes;
		for (std::size_t At = 0; At < Text.size(); ++At)
		{
			switch (Text[At])
			{
			case '\n':
				Bytes.push_back(0x01);
				break;
			case '|':
				Bytes.insert(Bytes.end(), {0x02, 0x01});
				break;
			case '^':
				Bytes.insert(Bytes.end(), {0x03, 0x01});
				break;
			default:
				Bytes.push_back(FMesCharacters::Plain().CodeAt(Text.substr(At))->Code.Code);
				break;
			}
		}
		Bytes.push_back(0x00);
		return Bytes;
	}

	/** A logged message's lines as plain text. */
	std::vector<std::string> LinesOf(const FLoggedMessage& Message)
	{
		std::vector<std::string> Lines;
		for (const std::vector<FMesPlacedGlyph>& Line : Message.Lines)
		{
			std::string Text;
			for (const FMesPlacedGlyph& Glyph : Line)
			{
				Text += FMesCharacters::Plain().CharacterOf(Glyph.Code);
			}
			Lines.push_back(Text);
		}
		return Lines;
	}

	/** Ticks of a window as a dialogue's, the player's button pressed as it stops, until its text is over. */
	void ReadThrough(FMesWindow& Window, FMesContext& Context, FMessageLogKey Key)
	{
		for (int32 Tick = 0; Tick < 2000 && (Window.HasText() || Window.QueuedCount() != 0); ++Tick)
		{
			if (Window.IsStopped())
			{
				Window.Continue();
			}
			UpdateLoggedWindow(Window, Context, Key);
		}
		REQUIRE_FALSE(Window.HasText());
	}
}

TEST_CASE("The log keeps a message's lines as typed, its pages one after another and an empty line kept", "[Text]")
{
	for (const EMesTextSpeed Speed : {EMesTextSpeed::Original, EMesTextSpeed::Quadruple, EMesTextSpeed::Instant})
	{
		FMessageLog Log;
		FMesContext Context;
		Context.Speed = Speed;
		Context.Log = &Log;
		FMesWindow Window(80, 3);
		Window.Queue(Encode("Yui\n\"You're welcome.|If you like my\n\ncooking...^so there!\""));
		ReadThrough(Window, Context, FirstWindow);
		REQUIRE(Log.Messages().size() == 1);
		CHECK(LinesOf(Log.Messages().front()) == std::vector<std::string>{"Yui", "\"You're welcome.", "If you like my", "", "cooking...", "so there!\""});
		CHECK(Log.Messages().front().Chosen == -1);
	}
}

TEST_CASE("The log keeps the glyphs' places, and the lines a window scrolled away", "[Text]")
{
	FMessageLog Log;
	FMesContext Context;
	Context.Log = &Log;
	FMesWindow Window(20, 2);
	Window.Queue(Encode("one\ntwo\nthree\nfour\nfive"));
	ReadThrough(Window, Context, FirstWindow);
	REQUIRE(Log.Messages().size() == 1);
	const FLoggedMessage& Message = Log.Messages().front();
	CHECK(LinesOf(Message) == std::vector<std::string>{"one", "two", "three", "four", "five"});
	// Without a font a glyph is three units wide.
	CHECK(Message.Lines[2][0].XUnits == 0);
	CHECK(Message.Lines[2][1].XUnits == 3);
	CHECK(Message.Lines[2][4].XUnits == 12);
}

TEST_CASE("Each string a window takes is a message of its own, and windows typing together keep theirs apart", "[Text]")
{
	FMessageLog Log;
	FMesContext Context;
	Context.Log = &Log;
	FMesWindow First(40, 2);
	FMesWindow Second(40, 2);
	First.Queue(Encode("Fei\n\"Hey!\""));
	Second.Queue(Encode("Citan\n\"Hmm.\""));
	for (int32 Tick = 0; Tick < 40; ++Tick)
	{
		UpdateLoggedWindow(First, Context, FirstWindow);
		UpdateLoggedWindow(Second, Context, SecondWindow);
	}
	First.Continue();
	ReadThrough(First, Context, FirstWindow);
	First.Queue(Encode("Fei\n\"Again!\""));
	ReadThrough(First, Context, FirstWindow);
	REQUIRE(Log.Messages().size() == 3);
	CHECK(LinesOf(Log.Messages()[0]) == std::vector<std::string>{"Fei", "\"Hey!\""});
	CHECK(LinesOf(Log.Messages()[1]) == std::vector<std::string>{"Citan", "\"Hmm.\""});
	CHECK(LinesOf(Log.Messages()[2]) == std::vector<std::string>{"Fei", "\"Again!\""});
}

TEST_CASE("A window with no log records nothing, and a string that types no glyph adds no message", "[Text]")
{
	FMessageLog Log;
	FMesContext Context;
	FMesWindow Window(40, 2);
	Window.Queue(Encode("Unseen"));
	ReadThrough(Window, Context, FirstWindow);
	CHECK(Log.Messages().empty());
	CHECK(Log.Revision() == 0);

	Context.Log = &Log;
	Window.Queue(Encode("\n\n"));
	ReadThrough(Window, Context, FirstWindow);
	CHECK(Log.Messages().empty());
	Window.Queue(Encode("\nLate start"));
	ReadThrough(Window, Context, FirstWindow);
	REQUIRE(Log.Messages().size() == 1);
	CHECK(LinesOf(Log.Messages().front()) == std::vector<std::string>{"Late start"});
	CHECK(Log.Revision() != 0);
}

TEST_CASE("A choice's line is marked on its page in the window's colour", "[Text]")
{
	FMessageLog Log;
	FMesContext Context;
	Context.Log = &Log;
	FMesWindow Window(40, 3);
	Window.Queue(Encode("Long ago...|Which way?\n    Left\n    Right"));
	for (int32 Tick = 0; Tick < 200 && Window.StopReason() != EMesStop::End; ++Tick)
	{
		if (Window.StopReason() == EMesStop::Page)
		{
			Window.Continue();
		}
		UpdateLoggedWindow(Window, Context, FirstWindow);
	}
	Window.SetColor(239, 30, 240);
	const uint64 Before = Log.Revision();
	LogChoice(Context, FirstWindow, Window, 2);
	REQUIRE(Log.Messages().size() == 1);
	const FLoggedMessage& Message = Log.Messages().front();
	CHECK(LinesOf(Message) == std::vector<std::string>{"Long ago...", "Which way?", "    Left", "    Right"});
	CHECK(Message.Chosen == 3);
	CHECK(Message.ChosenColor == std::array<uint8, 3>{239, 30, 240});
	CHECK(Log.Revision() != Before);

	// A row past the page's lines marks nothing.
	LogChoice(Context, FirstWindow, Window, 5);
	CHECK(Log.Messages().front().Chosen == 3);
}

TEST_CASE("The log keeps its latest messages, the oldest let go", "[Text]")
{
	FMessageLog Log;
	FMesContext Context;
	Context.Log = &Log;
	FMesWindow Window(40, 1);
	const std::size_t Count = FMessageLog::MostMessages + 5;
	for (std::size_t Number = 0; Number < Count; ++Number)
	{
		Window.Queue(Encode("Line " + std::to_string(Number)));
		ReadThrough(Window, Context, FirstWindow);
	}
	REQUIRE(Log.Messages().size() == FMessageLog::MostMessages);
	CHECK(LinesOf(Log.Messages().front()) == std::vector<std::string>{"Line 5"});
	CHECK(LinesOf(Log.Messages().back()) == std::vector<std::string>{"Line " + std::to_string(Count - 1)});
}
