#include "Battle/BattleRuntime.h"
#include "Field/FieldMap.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameMenus.h"
#include "Game/MovieProgram.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>

using namespace Weltall;

// The game run from power-on and from a field map, as its kernel hands one program to the next.

namespace
{
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState SoftResetHeld{static_cast<uint16>(FPadState::Bit(EPadButton::L1) | FPadState::Bit(EPadButton::R1) |
														  FPadState::Bit(EPadButton::Start) | FPadState::Bit(EPadButton::Select))};
	constexpr int32 TitleMap = 490;
	/** Map 35: a village square the player walks at once. */
	constexpr int32 VillageMap = 35;
	constexpr int32 NextVillageMap = 36;

	bool Mentions(const std::optional<FError>& Error, const std::string& Text)
	{
		return Error && Error->Message.find(Text) != std::string::npos;
	}

	/** The Menu as Load Game's New Game leaves it after a few frames: a new game's state, result 0. */
	class FNewGameMenu final : public IGameMenus
	{
	public:
		class FProgram final : public IGameProgram
		{
		public:
			explicit FProgram(FGameSession& InSession) : Session(InSession) {}
			int32 VblanksPerFrame() const override { return 1; }
			std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override
			{
				Session.Vblank(HeldPerVblank[0]);
				if (++Frames == 10)
				{
					if (auto Error = Session.InitNewGame())
					{
						return Error;
					}
					Session.Game().ProgramResult = 0;
					Ended = EGameProgramState::Ended;
				}
				return std::nullopt;
			}
			EGameProgramState State() const override { return Ended; }

		private:
			FGameSession& Session;
			int32 Frames = 0;
			EGameProgramState Ended = EGameProgramState::Running;
		};

		TResult<std::unique_ptr<IGameProgram>> Open(FGameSession& Session, const FFieldRequest& Request) override
		{
			Modes.push_back(Request.MenuMode);
			return std::unique_ptr<IGameProgram>(std::make_unique<FProgram>(Session));
		}

		std::vector<uint8> Modes;
	};

	/** Runs a map entered as a new game enters it past its entry, into the field's main loop. */
	void RunPastEntry(Testing::FGameLoopSession& Game)
	{
		FFieldProgram* Field = Game.Loop->Field();
		REQUIRE(Field != nullptr);
		Game.RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		Game.Frame();
	}
}

TEST_CASE("Powered on, the French translation shows its translators' picture then the opening movie", "[Game][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	REQUIRE(Loop.Stage() == EGameLoopStage::Boot);
	REQUIRE(Loop.Notice() != nullptr);
	CHECK(Loop.Notice()->Vblanks() == 361);
	Game->RunUntil([&] { return Loop.Stage() != EGameLoopStage::Boot; }, 400);
	CHECK(Game->Vblanks == 1 + 361);
	CHECK(Loop.Program() == EGameProgram::Movie);
}

TEST_CASE("Powered on the game shows its notice then the opening movie then the title's map", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	REQUIRE(Loop.Stage() == EGameLoopStage::Boot);
	REQUIRE(Loop.Notice() != nullptr);
	// The pad read once, then the notice's 143 vertical blanks.
	CHECK(Loop.Notice()->Vblanks() == 143);
	Game->RunUntil([&] { return Loop.Stage() != EGameLoopStage::Boot; }, 200);
	CHECK(Game->Vblanks == 1 + 143);
	REQUIRE(Loop.Program() == EGameProgram::Movie);
	REQUIRE(Loop.Movie() != nullptr);
	REQUIRE(Loop.Movie()->Player() != nullptr);

	// The logos, movie 16, to frame 233: 232 pictures of 1/15 s after the first.
	const int32 MovieStart = Game->Vblanks;
	Game->RunUntil([&] { return Loop.Program() != EGameProgram::Movie; }, 2000);
	CHECK(Game->Vblanks - MovieStart == 926);
	REQUIRE(Loop.Program() == EGameProgram::Field);
	const FFieldProgram* Field = Loop.Field();
	REQUIRE(Field != nullptr);
	CHECK(Field->Runtime().Context().RunningMap == TitleMap);
	const FGameState& State = Game->Session->Game();
	CHECK(State.SavedEventVariable(0x28) == 1);
	CHECK(State.ProgramResult == 0);
	CHECK(Game->Session->Kernel().bFieldEntered);
	CHECK(State.PlayTime() == static_cast<uint32>(Game->Vblanks));

	// The title waits for the player's button after its entry.
	const int32 FieldStart = Game->Frames;
	Game->RunUntil([&] { return Game->Frames > FieldStart + 150; }, 400);
	CHECK(Loop.Stage() == EGameLoopStage::Program);
	CHECK(Field->Runtime().Request().Kind == EFieldRequest::None);
	REQUIRE(Field->LatestFrame() != nullptr);
}

TEST_CASE("Cross on the title's map opens Load Game which stops the game while no menu is run", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	// Cross skips the movie too.
	Game->RunUntil([&] { return Loop.Stage() == EGameLoopStage::Stopped; }, 1500, CrossHeld, true);
	REQUIRE(Loop.Stage() == EGameLoopStage::Stopped);
	CHECK(Loop.Program() == EGameProgram::Field);
	INFO(Loop.StopReason()->Message);
	CHECK(Mentions(Loop.StopReason(), "the Menu program (mode 2"));
	CHECK(Game->Session->Kernel().MenuMode == 2);
}

TEST_CASE("Load Game's New Game on the title's map plays the opening's second movie then enters the first map", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FNewGameMenu Menus;
	Game->Session->SetMenus(&Menus);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Program() == EGameProgram::Field; }, 1500, CrossHeld);
	Game->RunUntil([&] { return Loop.Field()->Movie() != nullptr; }, 600, CrossHeld);
	CHECK(Menus.Modes == std::vector<uint8>{2});
	FFieldProgram* Field = Loop.Field();
	CHECK(Game->Session->Game().Place() == 4);
	CHECK(Field->Runtime().Context().bFrameVariablesFrozen);
	const FFieldRequest& Request = Field->Runtime().Request();
	REQUIRE(Request.Kind == EFieldRequest::Movie);
	CHECK(Request.Movie.Movie == 13);
	CHECK(Request.MovieView == EFieldMovieView::FullScreen);
	CHECK((Request.Movie.Flags & FFieldMovieRequest::Skippable) != 0);

	// Cross skips it; the title's scripts then start the field again, on the map the menu's New Game set.
	const int32 Runs = Loop.ProgramRuns();
	Game->RunUntil([&] { return Loop.ProgramRuns() != Runs; }, 200, CrossHeld);
	REQUIRE(Loop.Program() == EGameProgram::Field);
	Field = Loop.Field();
	CHECK(Field->Runtime().Context().RunningMap == 4);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
	CHECK(Loop.Stage() == EGameLoopStage::Program);
}

TEST_CASE("Left alone on the first disc the title's map plays the opening's second movie and comes back", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Program() == EGameProgram::Field; }, 1500);
	FFieldProgram* Field = Loop.Field();
	REQUIRE(Field != nullptr);
	// After 960 of its ticks without Cross the title's scripts fade out and play movie 13 to its end frame.
	Game->RunUntil([&] { return Field->Movie() != nullptr; }, 3000);
	const FFieldRequest& Request = Field->Runtime().Request();
	REQUIRE(Request.Kind == EFieldRequest::Movie);
	CHECK(Request.Movie.Movie == 13);
	CHECK(Request.Movie.EndFrame == 4462);
	CHECK(Request.MovieView == EFieldMovieView::FullScreen);
	CHECK((Request.Movie.Flags & FFieldMovieRequest::Skippable) != 0);
	CHECK(Field->Runtime().Context().RunningMap == TitleMap);
	CHECK(Field->Runtime().Context().Variables.Raw(0x408) == 1);

	// Cross skips it and the title fades in again, waiting as before.
	Game->RunUntil([&] { return Field->Movie() == nullptr; }, 20, CrossHeld);
	CHECK(Field->Movie() == nullptr);
	const int32 Runs = Loop.ProgramRuns();
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		const auto Error = Game->Frame();
		REQUIRE_FALSE(Error);
	}
	CHECK(Loop.Stage() == EGameLoopStage::Program);
	CHECK(Loop.ProgramRuns() == Runs);
	CHECK(Field->Runtime().Context().RunningMap == TitleMap);
	CHECK(Field->Runtime().Request().Kind == EFieldRequest::None);
}

TEST_CASE("The second disc boots through the logos to the title's map and its idle title plays no movie", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Stage() != EGameLoopStage::Boot; }, 200);
	// The logos are the disc's movie 7, played to frame 233 as on disc 1.
	REQUIRE(Loop.Program() == EGameProgram::Movie);
	const int32 MovieStart = Game->Vblanks;
	Game->RunUntil([&] { return Loop.Program() != EGameProgram::Movie; }, 2000);
	CHECK(Game->Vblanks - MovieStart == 926);
	CHECK(Loop.Program() == EGameProgram::Field);
	const FFieldProgram* Field = Loop.Field();
	REQUIRE(Field != nullptr);
	CHECK(Field->Runtime().Context().RunningMap == TitleMap);

	// The title's scripts read the disc's number when their wait runs out, and play no movie on it.
	constexpr uint32 DiscVariable = 0x408;
	Game->RunUntil([&] { return Field->Runtime().Context().Variables.Raw(DiscVariable) != 0; }, 3000);
	CHECK(Field->Runtime().Context().Variables.Raw(DiscVariable) == 2);
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		const auto Error = Game->Frame();
		REQUIRE_FALSE(Error);
		REQUIRE(Field->Movie() == nullptr);
	}
	CHECK(Loop.Stage() == EGameLoopStage::Program);
	CHECK(Field->Runtime().Request().Kind == EFieldRequest::None);
}

TEST_CASE("A field map entered at once runs on through its map changes", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	FGameLoop& Loop = *Game->Loop;
	FFieldProgram* Field = Loop.Field();
	// As a script's map change asks for it: the next map by its entrance 0.
	FEventContext& Context = Field->Runtime().Context();
	Context.Requests.bMapChange = true;
	Context.MapNumber = NextVillageMap;
	Context.Variables.Set(2, 0);
	Game->RunUntil([&] { return Field->Runtime().MapLoads() == 2; }, 40);
	Game->RunUntil([&] { return Field->Runtime().LastFrame() == EFieldFrame::Tick; }, 100);
	CHECK(Loop.Stage() == EGameLoopStage::Program);
	CHECK(Field->Runtime().Context().RunningMap == NextVillageMap);
	CHECK(Field->PreviousFrame() == nullptr);
	Game->Frame();
	CHECK(Field->PreviousFrame() != nullptr);
	// The map's encounter data lies over the kernel's formations.
	const std::vector<uint8>& Encounters = Field->Runtime().Map().Encounters;
	const auto& Formations = Game->Session->Kernel().Formations;
	const std::size_t Kept = std::min(Encounters.size(), Formations.size());
	CHECK(std::equal(Encounters.begin(), Encounters.begin() + static_cast<std::ptrdiff_t>(Kept), Formations.begin()));
}

TEST_CASE("The field leaves for a script's battle with its formation and kind, which the Battle program starts from", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	FFieldProgram* Field = Game->Loop->Field();
	FEventContext& Context = Field->Runtime().Context();
	const int32 Song = Context.Songs->ChosenSong();
	Context.bBattleRequested = true;
	Context.Requests.bScriptedBattle = true;
	Context.Requests.bLeavingField = true;
	Context.Requests.BattleKind = 5;
	Context.Encounters.Formation = 2;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 4);
	FGameLoop& Loop = *Game->Loop;
	REQUIRE(Loop.Stage() == EGameLoopStage::Program);
	REQUIRE(Loop.Battle() != nullptr);
	const FKernelState& Kernel = Game->Session->Kernel();
	const auto Record = Kernel.Formation(2);
	REQUIRE(Record);
	CHECK(Loop.Battle()->Runtime().Context().Formation.Scene == FBattleFormation::FromRecord(*Record).Scene);
	CHECK(Kernel.Field.BattleFormation == 2);
	CHECK(Kernel.BattleKind == 5);
	CHECK_FALSE(Kernel.bBattleDebug);
	CHECK(Kernel.FieldTrips == 1);
	CHECK(Game->Session->Game().FieldSong() == Song);
	CHECK(Game->Session->Game().Place() == VillageMap);
}

TEST_CASE("The field leaves for the world map, which starts at the place its scripts set", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bWorldMap = true;
	Context.bFrameVariablesFrozen = true;
	Game->Session->Game().SetWorldMapPlace(0x405, 1, 0x800, 2);
	const int32 Runs = Game->Loop->ProgramRuns();
	Game->RunUntil([&] { return Game->Loop->ProgramRuns() != Runs; }, 4);
	FGameLoop& Loop = *Game->Loop;
	REQUIRE(Loop.Stage() == EGameLoopStage::Program);
	CHECK(Loop.Program() == EGameProgram::WorldMap);
	const FWorldMapProgram* WorldMap = Loop.WorldMap();
	REQUIRE(WorldMap != nullptr);
	CHECK(WorldMap->Runtime().Context().SceneNumber == 2);
	CHECK(WorldMap->Runtime().Context().Location == 5);
	CHECK(Game->Session->Kernel().FieldTrips == 0);
}

TEST_CASE("The field's program change to the field starts it again where the game state is", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	const int32 Runs = Game->Loop->ProgramRuns();
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bProgram = true;
	Context.Requests.Program = 1;
	const int32 Song = Context.Songs->ChosenSong();
	Game->RunUntil([&] { return Game->Loop->ProgramRuns() != Runs; }, 4);
	FFieldProgram* Field = Game->Loop->Field();
	REQUIRE(Field != nullptr);
	CHECK(Game->Loop->Stage() == EGameLoopStage::Program);
	CHECK(Field->Runtime().Context().RunningMap == VillageMap);
	CHECK(Field->Runtime().MapLoads() == 1);
	CHECK(Game->Session->Game().FieldSong() == Song);
	Game->RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
}

TEST_CASE("The field's program change with a new game's state starts the field on the title's map", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	const int32 Runs = Game->Loop->ProgramRuns();
	FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Context.Requests.bProgram = true;
	Context.Requests.Program = 0x81;
	const uint32 PlayTime = Game->Session->Game().PlayTime();
	Game->RunUntil([&] { return Game->Loop->ProgramRuns() != Runs; }, 4);
	REQUIRE(Game->Loop->Field() != nullptr);
	CHECK(Game->Loop->Field()->Runtime().Context().RunningMap == TitleMap);
	CHECK(Game->Session->Game().PlayTime() >= PlayTime);
	CHECK(Game->Session->Kernel().bBattleDebug);
}

TEST_CASE("L1 R1 Start and Select held together restart the console from the field and the movie", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Field = Testing::FGameLoopSession::Start(*Disc);
	Field->EnterField(VillageMap);
	RunPastEntry(*Field);
	Field->RunUntil([&] { return Field->Loop->Stage() == EGameLoopStage::SoftReset; }, 4, SoftResetHeld);

	auto Movie = Testing::FGameLoopSession::Start(*Disc);
	Movie->PowerOn();
	Movie->RunUntil([&] { return Movie->Loop->Program() == EGameProgram::Movie; }, 200);
	// The notice never looks at the pad; the movie restarts at the first blank the buttons are held.
	Movie->RunUntil([&] { return Movie->Loop->Stage() == EGameLoopStage::SoftReset; }, 2, SoftResetHeld);
}

TEST_CASE("The game stops at the cases no script reaches unless its host lets play carry past them", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	FUnreachedCases& Cases = Game->Session->Unreached();
	CHECK_FALSE(Cases.Recover("a map change with a transition past the seven"));
	Cases.bStops = false;
	CHECK(Cases.Recover("a map change with a transition past the seven"));
	// The field's scripts meet their cases through the session's record.
	CHECK(Game->Loop->Field()->Runtime().Context().Unreached == &Cases);
}

TEST_CASE("A motor still running as the field takes over runs out on the field's vertical blanks", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	Game->EnterField(VillageMap);
	RunPastEntry(*Game);
	// As the arena leaves it: started for seven vertical blanks, sent on at the first, six left.
	FKernelVibration& Motors = Game->Session->Vibration();
	Motors.Start(0, 7);
	Motors.Vblank();
	REQUIRE(Motors.IsOn(0));
	const int32 Before = Game->Vblanks;
	for (int32 Frame = 0; Frame < 20 && Motors.IsOn(0); ++Frame)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK_FALSE(Motors.IsOn(0));
	// Six more on, the seventh finds it run out; the field's frame of two vertical blanks ends at the eighth.
	CHECK(Game->Vblanks - Before >= 7);
	CHECK(Game->Vblanks - Before <= 8);
}
