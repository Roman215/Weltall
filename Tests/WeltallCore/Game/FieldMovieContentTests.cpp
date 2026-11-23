#include "Field/FieldLight.h"
#include "Game/FieldMovie.h"
#include "Game/FieldProgram.h"
#include "Mods/PerfectWorks.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <optional>

using namespace Weltall;

// Movies the field plays inside itself, asked for as the scripts ask for them, on a map running from a
// new game: the passes of each view, the skip, the end and the field's way back.

namespace
{
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	/** Map 35: a village square; movie 15, disc 1's record 16 (137 frames). */
	constexpr int32 VillageMap = 35;
	constexpr uint16 ShortMovie = 15;
	/** Kind 2 of FE 60: full screen in 24-bit colour; kind 0: over the field. */
	constexpr uint16 FullScreenKind = 2;

	/** The Field program entered at the village and run into its main loop. */
	std::unique_ptr<Testing::FGameLoopSession> Village(const FIso9660& Disc)
	{
		auto Game = Testing::FGameLoopSession::Start(Disc);
		Game->EnterField(VillageMap);
		FFieldProgram* Field = Game->Loop->Field();
		REQUIRE(Field != nullptr);
		Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		Game->Frame();
		return Game;
	}

	/** A movie asked for as the scripts ask for one: its frames, flags, view and the place it decodes to (in the scene, at X). */
	void AskForMovie(FEventContext& Context, uint16 EndFrame, uint8 Flags, EFieldMovieView View, int16 X = 0x140, uint16 Number = ShortMovie)
	{
		FFieldMovieRequest& Movie = Context.Requests.Movie;
		Movie = {};
		Movie.Movie = Number;
		Movie.StartFrame = 1;
		Movie.EndFrame = EndFrame;
		Movie.Flags = Flags;
		Movie.Kind = FullScreenKind;
		Movie.Width = 0x140;
		Movie.Height = View == EFieldMovieView::InScene ? 0xE0 : 0x100;
		Movie.Buffers = View == EFieldMovieView::InScene ? std::array<int16, 4>{X, 0x100, X, 0x100} : std::array<int16, 4>{0, 0, 0, 0x100};
		Context.MovieView = View;
		Context.Requests.bMovie = true;
	}

	/** Runs until the field plays the movie, and returns the play time then. */
	uint32 RunToMovie(Testing::FGameLoopSession& Game)
	{
		FFieldProgram* Field = Game.Loop->Field();
		Game.RunUntil([&] { return Field->Movie() != nullptr; }, 20);
		return Game.Session->Game().PlayTime();
	}
}

TEST_CASE("A full-screen movie stands the field still for its passes then dissolves back to it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldProgram* Field = Game->Loop->Field();
	FFieldRuntime& Runtime = Field->Runtime();
	AskForMovie(Runtime.Context(), 30, 0, EFieldMovieView::FullScreen);
	const uint32 Started = RunToMovie(*Game);
	REQUIRE(Runtime.Request().Kind == EFieldRequest::Movie);
	CHECK(Field->Movie()->View() == EFieldMovieView::FullScreen);
	const std::array<int32, 3> Leader = Runtime.Context().States[Runtime.Context().LeaderActor].Position;

	// Its first picture in a vertical blank, then passes of two until it shows frame 30.
	CHECK(Runtime.VblanksPerTick() == 1);
	Game->Frame();
	CHECK(Runtime.LastFrame() == EFieldFrame::Movie);
	CHECK(Runtime.Context().Movie.Frame == 1);
	int32 Passes = 0;
	while (Runtime.Request().Kind == EFieldRequest::Movie)
	{
		CHECK(Runtime.VblanksPerTick() == 2);
		REQUIRE_FALSE(Game->Frame());
		++Passes;
		REQUIRE(Passes < 100);
	}
	// Frame 30 is due 29 / 15 s in, 115.7 vertical blanks: the pass that reaches it ends at the 117th.
	CHECK(Game->Session->Game().PlayTime() - Started == 1 + 2 * 58);
	CHECK(Passes == 58);
	CHECK(Field->Movie() == nullptr);
	CHECK(Runtime.Context().States[Runtime.Context().LeaderActor].Position == Leader);
	CHECK(Runtime.Context().Movie.Frame == 30);
	// The dissolve from the still takes the next frames.
	CHECK(Runtime.Context().Requests.Dissolve == 1);
	CHECK(Runtime.Context().Requests.DissolveTicks == 0x20);
	Game->Frame();
	CHECK(Runtime.LastFrame() == EFieldFrame::Tick);
	CHECK(Game->Loop->Stage() == EGameLoopStage::Program);
}

TEST_CASE("Cross skips a skippable movie whose sound then fades for 5 vertical blanks", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldProgram* Field = Game->Loop->Field();
	FFieldRuntime& Runtime = Field->Runtime();
	AskForMovie(Runtime.Context(), 130, FFieldMovieRequest::Skippable, EFieldMovieView::FullScreen);
	RunToMovie(*Game);
	Game->Frame();
	Game->Frame();
	CHECK_FALSE(Runtime.IsMovieSkipped());
	// Cross (the field's logical 20h) pressed during a pass: the pass's read after it skips.
	Game->Frame(CrossHeld);
	REQUIRE(Runtime.IsMovieSkipped());
	const uint32 Picture = Field->Movie()->Player().PictureNumber();
	CHECK(Runtime.VblanksPerTick() == FMoviePlayer::SkipVblanks);
	const uint32 Before = Game->Session->Game().PlayTime();
	Game->Frame();
	CHECK(Game->Session->Game().PlayTime() == Before + 5);
	CHECK(Runtime.Request().Kind == EFieldRequest::None);
	CHECK(Field->Movie() == nullptr);
	CHECK(Picture < 10);
	CHECK(Runtime.Context().Requests.Dissolve == 1);
}

TEST_CASE("A movie that forbids skipping plays on through Cross", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
	AskForMovie(Runtime.Context(), 130, 0, EFieldMovieView::FullScreen);
	RunToMovie(*Game);
	for (int32 Pass = 0; Pass < 20; ++Pass)
	{
		Game->Frame(CrossHeld);
		CHECK_FALSE(Runtime.IsMovieSkipped());
	}
	CHECK(Runtime.Request().Kind == EFieldRequest::Movie);
}

TEST_CASE("A movie in the scene runs the field's whole ticks and decodes into VRAM for the map's models", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldProgram* Field = Game->Loop->Field();
	FFieldRuntime& Runtime = Field->Runtime();
	AskForMovie(Runtime.Context(), 20, 0, EFieldMovieView::InScene);
	RunToMovie(*Game);
	const FVram& Vram = Field->Drawing().Vram();
	bool bPictured = false;
	for (int32 Y = 0x100; Y < 0x1E0 && !bPictured; ++Y)
	{
		for (int32 X = 0x140; X < 0x280 && !bPictured; ++X)
		{
			bPictured = Vram.At(X, Y) != 0;
		}
	}
	CHECK(bPictured);
	// Beside the picture of the screen's area, the picture stays the screen's copy.
	CHECK(Runtime.Context().Render.Cover.bScreen);
	Game->Frame();
	CHECK(Runtime.LastFrame() == EFieldFrame::Movie);
	const int32 SettleTicks = Runtime.Context().SettleTicks;
	int32 Ticks = 0;
	while (Runtime.Request().Kind == EFieldRequest::Movie)
	{
		CHECK(Runtime.VblanksPerTick() == 2 + Runtime.Context().FrameDelay);
		REQUIRE_FALSE(Game->Frame());
		CHECK(Runtime.LastFrame() == EFieldFrame::Tick);
		++Ticks;
		REQUIRE(Ticks < 100);
	}
	// Frame 20 is due 19 / 15 s in, 75.8 vertical blanks: after the first picture's, ticks of 2 and the scripts' delay.
	const int32 PerTick = 2 + Runtime.Context().FrameDelay;
	CHECK(Ticks == (75 + PerTick - 1) / PerTick);
	CHECK(Runtime.Context().SettleTicks == SettleTicks);
	// In the scene the field goes on without a dissolve.
	CHECK(Runtime.Context().Requests.Dissolve == 0);
	CHECK(Field->Movie() == nullptr);
}

TEST_CASE("A movie in the scene decoding over the picture of the screen's area leaves the picture drawn from VRAM", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldProgram* Field = Game->Loop->Field();
	FFieldRuntime& Runtime = Field->Runtime();
	REQUIRE(Runtime.Context().Render.Cover.bScreen);
	AskForMovie(Runtime.Context(), 20, 0, EFieldMovieView::InScene, FFieldCover::PictureArea.X);
	RunToMovie(*Game);
	CHECK_FALSE(Runtime.Context().Render.Cover.bScreen);
}

TEST_CASE("Transition 5 moves the picture of the screen in VRAM once the next map's textures are in, where its models show it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	// Map 131's doors go to map 155 with transition 5.
	Game->EnterField(131);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	FEventContext& Context = Field->Runtime().Context();
	const int32 Loads = Field->Runtime().MapLoads();
	Context.Requests.bMapChange = true;
	Context.MapNumber = 155;
	Context.Requests.MapTransition = 5;
	Game->RunUntil([&] { return Field->Runtime().MapLoads() != Loads; }, 20);
	const FFieldFrame* Frame = Field->LatestFrame();
	REQUIRE(Frame != nullptr);
	REQUIRE(Frame->ScreenInVram.size() == 1);
	const FFieldScreenInVram& Moved = Frame->ScreenInVram[0];
	CHECK((Moved.Rect.X == 0x140 && Moved.Rect.Y == 0xFF && Moved.Rect.Width == 0x140 && Moved.Rect.Height == 0xE0));
	CHECK((Moved.ScreenX == 0 && Moved.ScreenY == 0));
	CHECK(Moved.Taken == Frame->Cover.Taken);
}

TEST_CASE("A movie's end loads the map's gears again under the lighting the frames before it left", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	// The forest's clearing where Weltall lands, as the story reaches it: its gear has polygons lit as it is built and never again.
	constexpr int32 ClearingMap = 24;
	constexpr uint16 ClearingStory = 30;
	Game->Session->Game().SetSavedEventVariable(0, ClearingStory);
	Game->EnterField(ClearingMap);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	FFieldRuntime& Runtime = Field->Runtime();
	Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 100);
	Game->Frame();
	const FFieldGears& Gears = Runtime.Context().Gears;
	REQUIRE(Gears.Count != 0);
	REQUIRE(Gears.Models != nullptr);
	const int32 Loads = Gears.Loads;
	// The gears' drawing left its light, its colours and its ambient.
	const std::optional<FGte> Left = Runtime.LightingLeft();
	REQUIRE(Left);
	CHECK(Left->LightMatrix != FGteMatrix{});
	FGte GearColors;
	LoadFieldGearColors(Runtime.Context().Render.GearLights, GearColors);
	CHECK(Left->ColorMatrix == GearColors.ColorMatrix);
	CHECK(Left->BackColor == GearColors.BackColor);

	AskForMovie(Runtime.Context(), 30, 0, EFieldMovieView::FullScreen);
	RunToMovie(*Game);
	CHECK(Gears.Models == nullptr);
	Game->RunUntil([&] { return Runtime.Request().Kind != EFieldRequest::Movie; }, 100);
	CHECK(Gears.Loads == Loads + 1);
	REQUIRE(Gears.Models != nullptr);
	CHECK(Gears.Models->Model(0) != nullptr);
	// Nothing lit was drawn meanwhile.
	REQUIRE(Runtime.LightingLeft());
	CHECK(Runtime.LightingLeft()->LightMatrix == Left->LightMatrix);
	CHECK(Runtime.LightingLeft()->ColorMatrix == Left->ColorMatrix);
	CHECK(Runtime.LightingLeft()->BackColor == Left->BackColor);
	REQUIRE_FALSE(Game->Frame());
	CHECK(Game->Loop->Stage() == EGameLoopStage::Program);
}

TEST_CASE("A movie over the field runs a pass each vertical blank with the first actor's scripts and the windows", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
	AskForMovie(Runtime.Context(), 12, 0, EFieldMovieView::OverField);
	RunToMovie(*Game);
	Game->Frame();
	int32 Passes = 0;
	while (Runtime.Request().Kind == EFieldRequest::Movie)
	{
		CHECK(Runtime.VblanksPerTick() == 1);
		const uint8 Buffer = Runtime.Context().DrawBuffer;
		REQUIRE_FALSE(Game->Frame());
		if (Runtime.Request().Kind == EFieldRequest::Movie)
		{
			CHECK(Runtime.Context().DrawBuffer == (Buffer ^ 1));
		}
		++Passes;
		REQUIRE(Passes < 100);
	}
	// Frame 12 shows 11 / 15 s in.
	CHECK(Passes + 1 == 44);
	CHECK(Runtime.Context().Requests.Dissolve == 1);
}

TEST_CASE("The field's movie shows its frames as the drive delivers them at 15 a second", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Village(*Disc);
	FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
	AskForMovie(Runtime.Context(), 130, 0, EFieldMovieView::FullScreen);
	RunToMovie(*Game);
	const FFieldMovie& Movie = *Game->Loop->Field()->Movie();
	int32 Vblanks = 0;
	for (int32 Pass = 0; Pass < 100; ++Pass)
	{
		Vblanks += Runtime.VblanksPerTick();
		Game->Frame();
		// Picture n (from 0) is due once the vertical blanks reach n x 1/15 s of the NTSC display's.
		const int64 Due = static_cast<int64>(Vblanks) * 15 * 263 * 3413 / 53693175;
		CHECK(Movie.Player().FrameNumber() == static_cast<uint32>(Due + 1));
		CHECK(Runtime.Context().Movie.Frame == static_cast<int32>(Due + 1));
	}
}

TEST_CASE("The field plays the movie of the disc in the drive, and stops at one ending before the frame it waits for", "[Game][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	// Movie 2 has 370 frames on disc 1 and 153 on disc 2: only disc 1's reaches frame 160.
	constexpr uint16 SecondMovie = 2;
	constexpr uint16 EndFrame = 160;
	for (const int32 Drive : {0, 1})
	{
		auto Game = Village(*First);
		Game->AddDisc(*Second);
		Game->Session->Kernel().Disc = Drive;
		FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
		AskForMovie(Runtime.Context(), EndFrame, 0, EFieldMovieView::FullScreen, 0x140, SecondMovie);
		RunToMovie(*Game);
		std::optional<FError> Stopped;
		for (int32 Pass = 0; Pass < 400 && !Stopped && Runtime.Request().Kind == EFieldRequest::Movie; ++Pass)
		{
			Stopped = Game->Frame();
		}
		if (Drive == 0)
		{
			CHECK_FALSE(Stopped);
			CHECK(Runtime.Request().Kind == EFieldRequest::None);
		}
		else
		{
			REQUIRE(Stopped);
			CHECK(Stopped->Message == "the field's movie 2 ends at frame 153, before the frame 160 the field waits for");
		}
	}
}

TEST_CASE("The childhood flashback plays movie 11 through frame 228 with Perfect Works", "[Game][Content][Mods]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
	if (!Release || Release->Version() != "0.11.4")
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	FPerfectWorksOptions Options;
	Options.bEnabled = true;
	Options.bScript = true;
	const auto Files = Release->Files(Options, 2);
	const Testing::FModFilesGiven Mod(*Disc, FDiscModFiles::Make(*Index, 2, Files, Release->Changes(Options, Files)));
	const auto Game = Testing::FGameLoopSession::Start(*Disc);
	constexpr int32 Flashback = 664;
	Game->Session->Game().SetSavedEventVariable(0, 324);
	Game->Session->Game().SetParty({0, 255, 255});
	Game->EnterField(Flashback);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	for (int32 Frame = 0; Frame < 5000 && Field->Movie() == nullptr; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 12 < 2 ? CrossHeld : FPadState{}));
	}
	REQUIRE(Field->Movie() != nullptr);
	CHECK(Field->Movie()->Request().Movie == 11);
	CHECK(Field->Movie()->Request().EndFrame == 228);
	Game->RunUntil([&] { return Field->Movie() == nullptr; }, 600);
	for (int32 Frame = 0; Frame < 3000 && Field->Runtime().Context().RunningMap == Flashback; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 12 < 2 ? CrossHeld : FPadState{}));
	}
	CHECK(Field->Runtime().Context().RunningMap == 665);
}
