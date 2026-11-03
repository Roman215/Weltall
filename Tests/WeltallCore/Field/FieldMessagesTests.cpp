#include "Field/EventMachine.h"
#include "Game/PsxRandom.h"
#include "Support/ByteWriter.h"
#include "Text/MesReading.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FByteWriter;

namespace
{
	constexpr uint16 Counter = 0x400;

	/** One script actor running Code from its default script, with a table of two one-line messages. */
	class FMessageMap
	{
	public:
		explicit FMessageMap(std::vector<uint8> Code)
		{
			FByteWriter Writer;
			for (int32 Word = 0; Word < 32; ++Word)
			{
				Writer.U32(0);
			}
			Writer.U32(1);
			for (int32 Script = 0; Script < 32; ++Script)
			{
				Writer.U16(0);
			}
			Writer.Bytes.insert(Writer.Bytes.end(), Code.begin(), Code.end());
			auto Parsed = FEventScripts::Parse(Writer.Bytes);
			REQUIRE(Parsed);
			Scripts = std::make_unique<FEventScripts>(std::move(*Parsed));

			// Two messages of 20 units and one line: "A" and "B".
			FByteWriter Table;
			Table.U16(1).U16(0).U16(12).U16(14).U16(20 | 1 << 8).U16(20 | 1 << 8).U8(0x20).U8(0).U8(0x21).U8(0);
			MessageBytes = std::move(Table.Bytes);
			auto Messages = FMessageTable::Parse(MessageBytes);
			REQUIRE(Messages);

			Context.Scripts = Scripts.get();
			Context.Random = &Random;
			Context.Actors.push_back(FFieldActor{0x200});
			Context.States.emplace_back().Reset(Random.Next());
			Context.bInitDone = true;
			Context.Messages.Table = *Messages;
			Context.Messages.EnterMap();
		}

		/** A field tick's vertical blanks at the full rate. */
		static constexpr int32 TickVblanks = 2;

		/** A tick of the scripts and of the windows, with the tick's pressed buttons; the scripts' fault, if one stopped them. */
		std::string Tick(uint16 Pressed = 0)
		{
			const auto Error = FEventMachine::Update(Context);
			Context.Messages.Tick();
			UpdateFieldMessages(Context, Pressed, 0, TickVblanks);
			return Error ? Error->Message : std::string();
		}

		/** Ticks until window 0's text waits for the button, at most Limit ticks; whether it does. */
		bool TickUntilWaiting(int32 Limit)
		{
			for (int32 Ticks = 0; Ticks < Limit && !Window(0).bTextWaits; ++Ticks)
			{
				if (!Tick().empty())
				{
					return false;
				}
			}
			return Window(0).bTextWaits;
		}

		const FFieldMessageWindow& Window(int32 Index) const { return Context.Messages.Windows[static_cast<std::size_t>(Index)]; }

		std::unique_ptr<FEventScripts> Scripts;
		std::vector<uint8> MessageBytes;
		FPsxRandom Random;
		FEventContext Context;
	};
}

TEST_CASE("A window no band places keeps its place from the script and the flags of its window's last message", "[Field]")
{
	// D2 message 0 in the top band, F4 closes it (at the tick's end); CF sets the place (x 0x40, y
	// 0x40, 48 units, 2 lines) and D2 message 1 asks for no band; then the thread loops on a yield.
	FMessageMap Map({0xD2, 0x00, 0x00, 0x10, 0xF4, 0x00, 0xCF, 0x20, 0x40, 0x10, 0x02, 0xD2, 0x01, 0x00, 0x30, 0xC3, 0x01, 0x0F, 0x00});
	CHECK(Map.Tick().empty());
	CHECK_FALSE(Map.Window(0).bOpen);
	CHECK(Map.Window(0).Flags == FieldMessageWindowFlags::Banded);
	CHECK(Map.Tick().empty());
	REQUIRE(Map.Window(0).bOpen);
	CHECK(Map.Window(0).Message.front() == 0x21);
	CHECK(Map.Window(0).Rect == FFieldScreenRect{0x40, 0x38, 0x30 * 4 + 16, 2 * 14 + 16});
	CHECK(Map.Window(0).Flags == FieldMessageWindowFlags::Banded);
}

TEST_CASE("A window at its speaker keeps how it was placed, so another view places it as the opener does", "[Field]")
{
	// D2 message 0 in the top band at the actor's head; then the thread loops on a yield.
	FMessageMap Map({0xD2, 0x00, 0x00, 0x10, 0xC3, 0x01, 0x04, 0x00});
	FFieldActor& Speaker = Map.Context.Actors[0];
	Speaker.Matrix = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
	Speaker.Translation = {0, 0, 1000};
	CHECK(Map.Tick().empty());
	const FFieldMessageWindow& Window = Map.Window(0);
	REQUIRE(Window.bOpen);
	CHECK(Window.Openings == 1);
	const FFieldMessageWindow::FHeadPlace& Place = Window.HeadPlace;
	CHECK((Place.bX && Place.bY && !Place.bLower));
	CHECK((Place.Width == 20 && Place.Lines == 1));
	// The head 64 units up at depth 1000, H 512: y 112 - 32.8, the GTE rounding down. The window's place: x centred on it, y above it.
	const std::array<int32, 2> Head = FieldMessageSpeakerHead(Map.Context, 0, Map.Context.ScriptView.View, Map.Context.ScriptView.ProjectionDistance);
	CHECK(Head == std::array<int32, 2>{160, 79});
	CHECK(PlaceFieldMessageAtHead(Place, Head[0], Head[1]) == std::array<int32, 2>{Window.Rect.X, Window.Rect.Y + 8});
	CHECK(Window.Rect.X == 160 - 8 - 20 * 2);

	// Near the screen's right and top the place is held on the screen, as the opener holds it.
	CHECK(PlaceFieldMessageAtHead(Place, 300, 30) == std::array<int32, 2>{0x134 - 0x10 - 20 * 4, 0x10});
	FFieldMessageWindow::FHeadPlace Lower = Place;
	Lower.bLower = true;
	CHECK(PlaceFieldMessageAtHead(Lower, 10, 200) == std::array<int32, 2>{12, 0xD4 - 8 - 14});
}

TEST_CASE("A window no band and no script's place puts nowhere stops the scripts", "[Field]")
{
	// D3's fixed box ignores the script's place; D2 has none.
	for (const uint8 Opcode : {uint8{0xD3}, uint8{0xD2}})
	{
		FMessageMap Map({Opcode, 0x00, 0x00, 0x30, 0xC3});
		CHECK(Map.Tick().find("no band and no x") != std::string::npos);
	}
}

TEST_CASE("A message at an empty party slot's actor skips a byte past its instruction", "[Field]")
{
	// D4 at party slot 2 (empty), the stray byte, then Counter += 1.
	FMessageMap Map({0xD4, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x04, 0xC3, 0x01, 0x09, 0x00});
	CHECK(Map.Tick().empty());
	CHECK(Map.Context.Variables.Get(Counter, *Map.Scripts) == 1);
	CHECK_FALSE(Map.Window(0).bOpen);
}

TEST_CASE("A message past the map's table stops the scripts", "[Field]")
{
	FMessageMap Map({0xD2, 0x02, 0x00, 0x00, 0xC3});
	CHECK(Map.Tick().find("past the table") != std::string::npos);
}

TEST_CASE("Auto-advancing text turns a waiting page once its pace's time has passed, and never without it", "[Field]")
{
	// D2 message 0 in the top band; then the thread loops on a yield.
	const std::vector<uint8> Code{0xD2, 0x00, 0x00, 0x10, 0xC3, 0x01, 0x04, 0x00};
	{
		FMessageMap Map(Code);
		REQUIRE(Map.TickUntilWaiting(40));
		for (int32 Tick = 0; Tick < 600; ++Tick)
		{
			REQUIRE(Map.Tick().empty());
		}
		CHECK(Map.Window(0).bTextWaits);
	}
	FMessageMap Map(Code);
	Map.Context.Messages.Text.AutoAdvance = EMesAutoAdvance::Fast;
	REQUIRE(Map.TickUntilWaiting(40));
	// The page shows one glyph: Fast gives it 60 + 3 vertical blanks, 32 ticks of 2.
	REQUIRE(MesShownGlyphs(*Map.Window(0).Text) == 1);
	const int32 Turn = (MesReadingVblanks(EMesAutoAdvance::Fast, 1) + FMessageMap::TickVblanks - 1) / FMessageMap::TickVblanks;
	CHECK(Turn == 32);
	for (int32 Tick = 1; Tick < Turn; ++Tick)
	{
		REQUIRE(Map.Tick().empty());
		REQUIRE(Map.Window(0).bTextWaits);
	}
	// The button goes down for the player: the text ends, and the window closes at the next tick.
	REQUIRE(Map.Tick().empty());
	CHECK_FALSE(Map.Window(0).bTextWaits);
	REQUIRE(Map.Tick().empty());
	CHECK_FALSE(Map.Window(0).bOpen);
}

TEST_CASE("Auto-advancing text leaves a choice to the player", "[Field]")
{
	// D2 message 0 in the top band, A9 makes its one line a choice; then the thread loops on a yield.
	FMessageMap Map({0xD2, 0x00, 0x00, 0x10, 0xA9, 0x00, 0xC3, 0x01, 0x06, 0x00});
	Map.Context.Messages.Text.AutoAdvance = EMesAutoAdvance::Fast;
	for (int32 Tick = 0; Tick < 600; ++Tick)
	{
		REQUIRE(Map.Tick().empty());
	}
	REQUIRE(Map.Window(0).bOpen);
	CHECK(Map.Window(0).bChoice);
	CHECK(Map.Context.Messages.Reading.Waited() == 0);
	// The player's own button ends it.
	REQUIRE(Map.Tick(0x20).empty());
	CHECK_FALSE(Map.Window(0).bChoice);
}
