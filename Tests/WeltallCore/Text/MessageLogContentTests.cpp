#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Text/MesCharacters.h"
#include "Text/MessageLog.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace Weltall;

// The log follows the game's own windows: map 12's opening scene on a new game, played through the game loop with the
// pages turning by themselves, leaves each message it showed in the log once, every page of it, as the messages' text
// typed whole gives them.

namespace
{
	constexpr uint16 SceneMap = 12;
	constexpr int32 SceneFrames = 2400;

	std::vector<std::string> LinesOf(const FLoggedMessage& Message)
	{
		std::vector<std::string> Lines;
		for (const std::vector<FMesPlacedGlyph>& Line : Message.Lines)
		{
			std::string Text;
			for (const FMesPlacedGlyph& Glyph : Line)
			{
				Text += FMesCharacters::Font().CharacterOf(Glyph.Code);
			}
			Lines.push_back(Text);
		}
		return Lines;
	}

	/**
	 * A message of the map's table typed whole in a window wide enough for any of its lines (the kernel's line end puts the
	 * cursor at 100 units, so narrower than that), each page turned.
	 */
	std::vector<std::string> TypedWhole(FMesContext Context, std::span<const uint8> Message)
	{
		FMessageLog Log;
		Context.Log = &Log;
		// The field's names are its tick's own; the scene's messages insert none.
		Context.State = nullptr;
		Context.Speed = EMesTextSpeed::Instant;
		FMesWindow Window(99, 8);
		Window.Queue(Message);
		for (int32 Tick = 0; Tick < 400 && (Window.HasText() || Window.QueuedCount() != 0); ++Tick)
		{
			if (Window.IsStopped())
			{
				Window.Continue();
			}
			UpdateLoggedWindow(Window, Context, {});
		}
		return Log.Messages().empty() ? std::vector<std::string>() : LinesOf(Log.Messages().front());
	}
}

TEST_CASE("The message log keeps each message a scene shows once, every page of it, as its text gives it", "[Text][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(*Disc);
	FMessageLog Log;
	Game->Session->SetMessageLog(&Log);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.AutoAdvance = EMesAutoAdvance::Fast;
	Game->Session->SetSettings(Settings);
	Game->EnterField(SceneMap);
	// Each window's opening that showed a glyph, by the map's message it showed, in the order they began to.
	std::vector<int32> Shown;
	std::set<std::pair<std::size_t, uint16>> Seen;
	for (int32 Frame = 0; Frame < SceneFrames; ++Frame)
	{
		const std::optional<FError> Stopped = Game->Frame();
		INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
		REQUIRE_FALSE(Stopped.has_value());
		const FFieldProgram* Field = Game->Loop->Field();
		REQUIRE(Field != nullptr);
		const FFieldMessages& Messages = Field->Runtime().Context().Messages;
		for (std::size_t Slot = 0; Slot < Messages.Windows.size(); ++Slot)
		{
			const FFieldMessageWindow& Window = Messages.Windows[Slot];
			if (!Window.bOpen || !Window.Text || Window.Text->Row(0).WidthUnits == 0 || !Seen.insert({Slot, Window.Openings}).second)
			{
				continue;
			}
			for (int32 Index = 0; Index < Messages.Table.Count(); ++Index)
			{
				if (Messages.Table.String(Index).data() == Window.Message.data())
				{
					Shown.push_back(Index);
				}
			}
		}
	}
	const FFieldProgram* Field = Game->Loop->Field();
	const FFieldMessages& Messages = Field->Runtime().Context().Messages;
	REQUIRE(Shown.size() > 10);
	REQUIRE(Log.Messages().size() == Shown.size());
	for (std::size_t Number = 0; Number < Shown.size(); ++Number)
	{
		INFO("message " << Shown[Number] << ", the log's " << Number);
		const std::vector<std::string> Logged = LinesOf(Log.Messages()[Number]);
		const std::vector<std::string> Whole = TypedWhole(Messages.Text, Messages.Table.String(Shown[Number]));
		if (Number + 1 < Shown.size())
		{
			CHECK(Logged == Whole);
			continue;
		}
		// The last may still be typing as the frames end: what it has typed so far.
		REQUIRE(Logged.size() <= Whole.size());
		for (std::size_t Line = 0; Line < Logged.size(); ++Line)
		{
			CHECK(Whole[Line].starts_with(Logged[Line]));
		}
	}
	// The scene's second message turns a page after its third line: the log goes on with the page's three, under the
	// speaker's name.
	const auto Second = std::find(Shown.begin(), Shown.end(), 1);
	REQUIRE(Second != Shown.end());
	const std::vector<std::string> Paged = LinesOf(Log.Messages()[static_cast<std::size_t>(Second - Shown.begin())]);
	REQUIRE(Paged.size() == 6);
	CHECK(Paged.front() == "Fei");
}
