#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Battle/BattlePartyState.h"
#include "Text/MesReading.h"
#include "Text/MesWindow.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <vector>

using namespace Weltall;

namespace
{
	/** A window of two lines that has typed Text (the font's codes, then the end) and stopped for the button, its rows drawn. */
	FMesWindow StoppedWindow(std::vector<uint8> Text)
	{
		FMesContext Context;
		FMesWindow Window(20, 2);
		Text.push_back(0x00);
		Window.Queue(Text);
		for (int32 Tick = 0; Tick < 40 && !Window.IsStopped(); ++Tick)
		{
			Window.Update(Context);
		}
		REQUIRE(Window.IsStopped());
		// The kernel draws the lines before it types: the last glyph shows from the tick after.
		Window.Update(Context);
		return Window;
	}
}

TEST_CASE("A page is read in a pause and a time for each glyph, longer at a slower pace", "[Text]")
{
	CHECK(MesReadingVblanks(EMesAutoAdvance::Slow, 0) == 120);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Normal, 0) == 90);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Fast, 0) == 60);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Slow, 30) == 120 + 30 * 5);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Normal, 30) == 90 + 30 * 4);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Fast, 30) == 60 + 30 * 3);
	CHECK(MesReadingVblanks(EMesAutoAdvance::Fast, -4) == 60);
}

TEST_CASE("The reader turns a page only after it has waited the whole time, then starts again", "[Text]")
{
	const int32 Needed = MesReadingVblanks(EMesAutoAdvance::Normal, 10);
	FMesReading Reading;
	for (int32 Vblank = 1; Vblank < Needed; ++Vblank)
	{
		REQUIRE_FALSE(Reading.Tick(EMesAutoAdvance::Normal, true, 10, 1));
	}
	CHECK(Reading.Tick(EMesAutoAdvance::Normal, true, 10, 1));
	CHECK(Reading.Waited() == 0);

	// A tick without a waiting page starts the count again; so does the setting off.
	for (int32 Vblank = 1; Vblank < Needed; ++Vblank)
	{
		REQUIRE_FALSE(Reading.Tick(EMesAutoAdvance::Normal, true, 10, 1));
	}
	CHECK_FALSE(Reading.Tick(EMesAutoAdvance::Normal, false, 10, 1));
	CHECK(Reading.Waited() == 0);
	CHECK_FALSE(Reading.Tick(EMesAutoAdvance::Off, true, 10, 600));
	CHECK(Reading.Waited() == 0);

	// Ticks count their vertical blanks: a field tick of two takes half as many; a tick reporting none counts one.
	int32 Ticks = 1;
	while (!Reading.Tick(EMesAutoAdvance::Normal, true, 10, 2))
	{
		++Ticks;
	}
	CHECK(Ticks == (Needed + 1) / 2);
	Ticks = 1;
	while (!Reading.Tick(EMesAutoAdvance::Normal, true, 10, 0))
	{
		++Ticks;
	}
	CHECK(Ticks == Needed);
}

TEST_CASE("The glyphs a page shows are those of every row drawn", "[Text]")
{
	const FMesWindow Window = StoppedWindow({0x20, 0x21, 0x22, 0x01, 0x23});
	CHECK(MesShownGlyphs(Window) == 4);
}

TEST_CASE("A battle event's message that waits for the button is confirmed for the player once read", "[Text][Battle]")
{
	auto Context = std::make_unique<FBattleContext>();
	FBattleEventWork& Event = Context->Event;
	Event.Text = StoppedWindow({0x20, 0x21});
	Event.bMessageShown = true;
	Event.bKeyWait = true;
	Context->Party.Event = BattleMenuEvents::Nothing;
	const int32 Steps = MesReadingVblanks(EMesAutoAdvance::Fast, 2);

	// Off, the input keeps waiting for the player's press.
	for (int32 Step = 0; Step < Steps * 2; ++Step)
	{
		TakeBattleMessageInput(*Context, false);
	}
	CHECK(Context->Party.Event == BattleMenuEvents::Nothing);

	// A step is a vertical blank: the confirm comes on the step the page has been read.
	Context->AutoAdvance = EMesAutoAdvance::Fast;
	for (int32 Step = 1; Step < Steps; ++Step)
	{
		TakeBattleMessageInput(*Context, false);
		REQUIRE(Context->Party.Event == BattleMenuEvents::Nothing);
	}
	TakeBattleMessageInput(*Context, false);
	CHECK(Context->Party.Event == BattleMenuEvents::Confirm);

	// Without a message waiting (the result screen's pages, a message still typing) the input presses nothing.
	Event.bMessageShown = false;
	Context->Party.Event = BattleMenuEvents::Nothing;
	for (int32 Step = 0; Step < Steps * 2; ++Step)
	{
		TakeBattleMessageInput(*Context, false);
	}
	CHECK(Context->Party.Event == BattleMenuEvents::Nothing);
	CHECK(Event.Reading.Waited() == 0);
}
