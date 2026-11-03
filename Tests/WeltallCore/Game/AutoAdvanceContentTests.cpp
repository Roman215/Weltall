#include "Game/FieldProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Text/MesReading.h"

#include <catch_amalgamated.hpp>

#include <memory>
#include <optional>
#include <string>

using namespace Weltall;

// A scene that starts as its map is entered on a new game and stops at its first page until the button is pressed. With
// auto-advancing text the whole scene plays on through the game loop with no button down, each page turning once the
// pace's time for its glyphs is up.

namespace
{
	constexpr uint16 SceneMap = 12;
	/** Frames to play the scene with the setting on, and with it off (its first page waiting from about frame 150). */
	constexpr int32 ReadingFrames = 2400;
	constexpr int32 WaitingFrames = 900;

	struct FScenePlay
	{
		/** The stretches of frames in which a page waited for the button. */
		int32 Pages = 0;
		/** The first page's: its frames waiting, the glyphs it showed and the vertical blanks of each of its frames. */
		int32 FirstFrames = 0;
		int32 FirstGlyphs = 0;
		int32 FirstFrameVblanks = 0;
	};

	FScenePlay PlayScene(const FIso9660& Disc, EMesAutoAdvance Pace, int32 Frames)
	{
		const std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.AutoAdvance = Pace;
		Game->Session->SetSettings(Settings);
		Game->EnterField(SceneMap);
		FScenePlay Play;
		bool bWaited = false;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const int32 Vblanks = Game->Loop->VblanksPerFrame();
			const std::optional<FError> Stopped = Game->Frame();
			INFO("frame " << Frame << ": " << (Stopped ? Stopped->Message : std::string()));
			REQUIRE_FALSE(Stopped.has_value());
			const FFieldProgram* Field = Game->Loop->Field();
			REQUIRE(Field != nullptr);
			const FFieldMessageWindow* Waiting = nullptr;
			for (const FFieldMessageWindow& Window : Field->Runtime().Context().Messages.Windows)
			{
				if (Window.bOpen && Window.bTextWaits)
				{
					Waiting = &Window;
				}
			}
			if (Waiting != nullptr && !bWaited)
			{
				++Play.Pages;
			}
			if (Waiting != nullptr && Play.Pages == 1)
			{
				// The rows show a tick's typing from the next: the page's glyphs are whole from its second frame waiting.
				++Play.FirstFrames;
				Play.FirstGlyphs = MesShownGlyphs(*Waiting->Text);
				Play.FirstFrameVblanks = Vblanks;
			}
			bWaited = Waiting != nullptr;
		}
		return Play;
	}
}

TEST_CASE("Auto-advancing text plays a scene's pages on without the button, each after its reading time", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FScenePlay Waiting = PlayScene(*Disc, EMesAutoAdvance::Off, WaitingFrames);
	CHECK(Waiting.Pages == 1);
	CHECK(Waiting.FirstFrames > WaitingFrames / 2);

	const FScenePlay Reading = PlayScene(*Disc, EMesAutoAdvance::Normal, ReadingFrames);
	CHECK(Reading.Pages > 10);
	REQUIRE(Reading.FirstFrameVblanks > 0);
	const int32 Expected = (MesReadingVblanks(EMesAutoAdvance::Normal, Reading.FirstGlyphs) + Reading.FirstFrameVblanks - 1) / Reading.FirstFrameVblanks;
	INFO("the first page: " << Reading.FirstGlyphs << " glyphs, " << Reading.FirstFrames << " frames waiting of " << Reading.FirstFrameVblanks
		<< " vertical blanks, " << Expected << " expected");
	CHECK(Reading.FirstFrames >= Expected - 1);
	CHECK(Reading.FirstFrames <= Expected + 1);
}
