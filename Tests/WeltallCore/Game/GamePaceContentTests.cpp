#include "Battle/BattleEnd.h"
#include "Battle/BattleRuntime.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GamePace.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Game/PlayerSettings.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <utility>
#include <vector>

using namespace Weltall;

// The battle speed in the game's loop: where it makes the game run faster, and the buttons that step it.

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	constexpr FPadState L1Held{FPadState::Bit(EPadButton::L1)};
	constexpr FPadState R1Held{FPadState::Bit(EPadButton::R1)};
	/** Map 15: a forest, its formation 1. */
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	/** Map 199, whose script starts Rico's fight on foot (formation 7: no escape, a battle event); its formation 0, no escape, is its encounter. */
	constexpr uint16 RicoMap = 199;
	constexpr uint8 RicoFight = 7;
	constexpr uint8 RicoMapEncounter = 0;

	/** A new game with a party of three on the map (the forest), past the map's entry into the field's loop. */
	FFieldProgram& EnterForest(Testing::FGameLoopSession& Game, uint16 Map = ForestMap)
	{
		FGameState& State = Game.Session->Game();
		State.SetParty({0, 1, 2});
		State.SetJoinedCharacters(0x1FF);
		Game.EnterField(Map);
		FFieldProgram* Field = Game.Loop->Field();
		REQUIRE(Field != nullptr);
		Game.RunUntil([&] { return Field->Runtime().Context().bFieldRunning; }, 100);
		Game.Frame();
		return *Field;
	}

	/** Asks the field for the map's battle (the forest's) as a script asks for one, or as an encounter, with Held down as the Battle program starts. */
	void StartBattle(Testing::FGameLoopSession& Game, FFieldProgram& Field, FPadState Held = NoButtons, uint8 Formation = ForestFormation, bool bScripted = true)
	{
		FEventContext& Context = Field.Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = bScripted;
		Context.Encounters.bBattleDrawn = !bScripted;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = Formation;
		Game.RunUntil([&] { return Game.Loop->Program() == EGameProgram::Battle; }, 8, Held);
		REQUIRE(Game.Loop->Battle() != nullptr);
	}

	EBattleSpeed Speed(const Testing::FGameLoopSession& Game)
	{
		return Game.Session->Settings().BattleSpeed;
	}

	/** A press of Buttons for a frame, then a frame with nothing held. */
	void Press(Testing::FGameLoopSession& Game, FPadState Buttons)
	{
		REQUIRE_FALSE(Game.Frame(Buttons));
		REQUIRE_FALSE(Game.Frame(NoButtons));
	}
}

TEST_CASE("The battle speed makes the game run faster only while a battle is fought", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.BattleSpeed = EBattleSpeed::Quadruple;
	Game->Session->SetSettings(Settings);

	// The kernel's start and the field keep the game's pace whatever the battle speed.
	Game->PowerOn();
	CHECK_FALSE(IsBattleFought(*Game->Loop));
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
	Game = Testing::FGameLoopSession::Start(*Disc);
	Game->Session->SetSettings(Settings);
	FFieldProgram& Field = EnterForest(*Game);
	CHECK_FALSE(IsBattleFought(*Game->Loop));
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);

	// The battle is fought from its transition on, at the speed set.
	StartBattle(*Game, Field);
	const FBattleRuntime& Battle = Game->Loop->Battle()->Runtime();
	REQUIRE(Battle.IsInTransition());
	CHECK(IsBattleFought(*Game->Loop));
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 4);
	Game->RunUntil([&] { return !Battle.IsInTransition(); }, 400);
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 4);
	Game->Session->BattleSpeed() = EBattleSpeed::Double;
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 2);
	Game->Session->BattleSpeed() = EBattleSpeed::Normal;
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
	Game->Session->BattleSpeed() = EBattleSpeed::Quadruple;

	// Won, its result's screen is back at the game's pace while the Battle program still runs, and so is the field after it.
	const int32 Runs = Game->Loop->ProgramRuns();
	int32 ResultFrames = 0;
	for (int32 Frame = 0; Frame < 4000 && Game->Loop->ProgramRuns() == Runs; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 24 < 2 ? CrossHeld : NoButtons));
		if (Game->Loop->ProgramRuns() == Runs && !Battle.IsFought())
		{
			++ResultFrames;
			REQUIRE_FALSE(IsBattleFought(*Game->Loop));
			REQUIRE(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
		}
	}
	REQUIRE(Game->Loop->ProgramRuns() != Runs);
	CHECK(ResultFrames > 60);
	REQUIRE(Game->Loop->Program() == EGameProgram::Field);
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
	CHECK(Speed(*Game) == EBattleSpeed::Quadruple);
}

TEST_CASE("Opt-in field fast-forward steps during scenes and keeps battle speed independent", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FFieldProgram& Field = EnterForest(*Game);
	FPlayerSettings Settings = Game->Session->Settings();
	Press(*Game, R1Held);
	CHECK(GamePace(*Game->Loop, Settings) == 1);
	Settings.Balance.bExplorationFastForward = true;
	Game->Session->SetSettings(Settings);

	// Losing player control for a scripted scene still leaves fast-forward available.
	Field.Runtime().Context().bPlayerControlled = false;
	REQUIRE(CanFastForwardExploration(*Game->Loop, Settings));
	for (int32 Frame = 0; Frame < 8; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(R1Held));
	}
	CHECK(GamePace(*Game->Loop, Settings) == 2);
	CHECK(Game->Session->Pad().Held() == 0);
	REQUIRE_FALSE(Game->Frame());
	Press(*Game, R1Held);
	CHECK(GamePace(*Game->Loop, Settings) == 4);
	CHECK(Speed(*Game) == EBattleSpeed::Normal);

	StartBattle(*Game, Field, R1Held);
	CHECK(GamePace(*Game->Loop, Settings) == 1);
	CHECK(Game->Loop->ExplorationSpeed() == EBattleSpeed::Quadruple);
	REQUIRE_FALSE(Game->Frame(R1Held));
	CHECK(Speed(*Game) == EBattleSpeed::Normal);
}

TEST_CASE("Opt-in world map fast-forward counts play time at normal speed and keeps soft reset", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FPlayerSettings Settings = Session.Settings();
	Settings.Balance.bExplorationFastForward = true;
	Session.SetSettings(Settings);
	Session.Game().SetSavedEventVariable(0, 30);
	Session.Game().SetWorldMapPlace(0x401, 1, 0xC00, 1);
	auto Loop = FGameLoop::EnterWorldMap(Session);
	REQUIRE(Loop);
	Game->Loop = std::move(*Loop);
	CHECK_FALSE(CanFastForwardExploration(*Game->Loop, Settings));
	Game->RunUntil([&] { return Game->Loop->WorldMap()->TicksMap(); }, FWorldMapProgram::EntryFadeFrames + 1, R1Held);
	REQUIRE_FALSE(Game->Frame(R1Held));
	CHECK(GamePace(*Game->Loop, Settings) == 1);
	REQUIRE_FALSE(Game->Frame());
	Press(*Game, R1Held);
	Press(*Game, R1Held);
	REQUIRE(GamePace(*Game->Loop, Settings) == 4);

	Session.SetPace(4);
	const uint32 Before = Session.Game().PlayTime();
	const uint8 Clock = Session.Clock().Frames;
	for (int32 Tick = 0; Tick < 8; ++Tick)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK(Session.Game().PlayTime() == Before + 4);
	CHECK(Session.Clock().Frames == (Clock + 4) % 60);
	Press(*Game, L1Held);
	CHECK(GamePace(*Game->Loop, Settings) == 2);

	const FPadState Reset{FPadState::Bit(EPadButton::L1) | FPadState::Bit(EPadButton::R1)
		| FPadState::Bit(EPadButton::Start) | FPadState::Bit(EPadButton::Select)};
	REQUIRE_FALSE(Game->Frame(Reset));
	CHECK(Game->Loop->Stage() == EGameLoopStage::SoftReset);
	CHECK(GamePace(*Game->Loop, Settings) == 1);
}

TEST_CASE("By default only a battle uses R1 and L1 for speed, each press a step", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FFieldProgram& Field = EnterForest(*Game);

	// On the field the two turn the camera and leave the speed.
	Press(*Game, R1Held);
	Press(*Game, L1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Normal);

	// A button held into the battle is no press there.
	StartBattle(*Game, Field, R1Held);
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(R1Held));
	}
	CHECK(Speed(*Game) == EBattleSpeed::Normal);
	REQUIRE_FALSE(Game->Frame(NoButtons));

	// From the transition on: a press a step, however long it is held, to four times and no further.
	REQUIRE(Game->Loop->Battle()->Runtime().IsInTransition());
	Press(*Game, R1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Double);
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(R1Held));
	}
	CHECK(Speed(*Game) == EBattleSpeed::Quadruple);
	Press(*Game, R1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Quadruple);
	Press(*Game, L1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Double);
	// The battle's own buttons leave it.
	Press(*Game, CrossHeld);
	Press(*Game, FPadState{static_cast<uint16>(FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2))});
	CHECK(Speed(*Game) == EBattleSpeed::Double);
	Press(*Game, L1Held);
	Press(*Game, L1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Normal);
	Press(*Game, R1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Double);

	// On the result's screen the buttons do nothing: the speed chosen waits for the next battle.
	const FBattleRuntime& Battle = Game->Loop->Battle()->Runtime();
	const int32 Runs = Game->Loop->ProgramRuns();
	for (int32 Frame = 0; Frame < 4000 && Battle.IsFought(); ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 24 < 2 ? CrossHeld : NoButtons));
	}
	REQUIRE(Game->Loop->ProgramRuns() == Runs);
	REQUIRE_FALSE(Battle.IsFought());
	Press(*Game, R1Held);
	Press(*Game, L1Held);
	Press(*Game, L1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Double);
}

TEST_CASE("A battle of the story begins at the game's pace however fast the last fight was left, and R1 steps it up again", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto StartFast = [&Disc]
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.BattleSpeed = EBattleSpeed::Quadruple;
		Game->Session->SetSettings(Settings);
		return Game;
	};

	// An encounter keeps the speed, one that cannot be escaped too; so does a battle a script starts that can be.
	auto Game = StartFast();
	StartBattle(*Game, EnterForest(*Game, RicoMap), NoButtons, RicoMapEncounter, false);
	CHECK(Speed(*Game) == EBattleSpeed::Quadruple);
	Game = StartFast();
	StartBattle(*Game, EnterForest(*Game));
	CHECK(Speed(*Game) == EBattleSpeed::Quadruple);

	// Rico's fight, as map 199's script starts it.
	Game = StartFast();
	StartBattle(*Game, EnterForest(*Game, RicoMap), NoButtons, RicoFight);
	CHECK(Speed(*Game) == EBattleSpeed::Normal);
	REQUIRE(Game->Loop->Battle()->Runtime().IsInTransition());
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
	REQUIRE_FALSE(Game->Frame(NoButtons));
	Press(*Game, R1Held);
	CHECK(Speed(*Game) == EBattleSpeed::Double);
}

TEST_CASE("Run faster, the play time and the play clock count the player's time and a held button repeats at the hand's pace", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	const FPadState Up{FPadState::Bit(EPadButton::Up)};
	const auto Seconds = [&Session] { return Session.Clock().Seconds * 60 + Session.Clock().Frames; };
	/** The vertical blanks of the next Count at which the held button repeated, and the play time at each. */
	const auto Repeats = [&Session, Up](int32 Count)
	{
		std::vector<std::pair<int32, uint32>> At;
		for (int32 Vblank = 1; Vblank <= Count; ++Vblank)
		{
			Session.Vblank(Up);
			if (Session.Pad().TakeMerged().Repeated != 0)
			{
				At.emplace_back(Vblank, Session.Game().PlayTime());
			}
		}
		return At;
	};

	// At the game's own pace every vertical blank counts, and the repeat falls on the play time's multiples of 4.
	Session.Game().SetPlayTime(1000);
	const int32 ClockBefore = Seconds();
	CHECK(Repeats(44) == std::vector<std::pair<int32, uint32>>{{1, 1001}, {36, 1036}, {40, 1040}, {44, 1044}});
	CHECK(Session.Game().PlayTime() == 1044);
	CHECK(Seconds() - ClockBefore == 44);
	Session.Vblank({});
	Session.Pad().TakeMerged();

	// Four times as fast, one vertical blank of four is the player's: 400 of them are 100 of the play time and of the
	// clock, and the repeat waits four times the vertical blanks, still on the play time's multiples of 4.
	Session.SetPace(4);
	Session.Game().SetPlayTime(2000);
	const int32 ClockFast = Seconds();
	const std::vector<std::pair<int32, uint32>> Fast = Repeats(400);
	CHECK(Session.Game().PlayTime() == 2100);
	CHECK(Seconds() - ClockFast == 100);
	REQUIRE(Fast.size() >= 4);
	CHECK(Fast[0] == std::pair<int32, uint32>{1, 2000});
	CHECK(Fast[1] == std::pair<int32, uint32>{144, 2036});
	CHECK(Fast[2] == std::pair<int32, uint32>{160, 2040});
	CHECK(Fast[3] == std::pair<int32, uint32>{176, 2044});
	CHECK(Fast.size() == 1 + (400 - 144) / 16 + 1);

	// Back at the game's pace every vertical blank counts again from the next.
	Session.SetPace(1);
	Session.Vblank({});
	CHECK(Session.Game().PlayTime() == 2101);
	CHECK(Seconds() - ClockFast == 101);
}

TEST_CASE("A battle lost to the game over is seen at the game's pace from the party's fall however fast it was fought", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FPlayerSettings Settings = Game->Session->Settings();
	Settings.BattleSpeed = EBattleSpeed::Quadruple;
	Game->Session->SetSettings(Settings);
	FFieldProgram& Field = EnterForest(*Game, RicoMap);
	// Everyone a point from death against the D-block's guards, attacking turn after turn.
	for (const int32 Character : {0, 1, 2})
	{
		Game->Session->Game().SetCharacterPoints(Character, FGameState::ECharacterPoints::Hp, 1);
	}
	StartBattle(*Game, Field, NoButtons, RicoMapEncounter, false);
	const FBattleRuntime& Battle = Game->Loop->Battle()->Runtime();
	const int32 Runs = Game->Loop->ProgramRuns();
	int32 FoughtFast = 0;
	int32 LostFrames = 0;
	for (int32 Frame = 0; Frame < 12000 && Game->Loop->ProgramRuns() == Runs; ++Frame)
	{
		REQUIRE_FALSE(Game->Frame(Frame % 24 < 2 ? CrossHeld : NoButtons));
		if (Game->Loop->ProgramRuns() != Runs)
		{
			break;
		}
		if (IsPartyDown(Battle.Context()))
		{
			// From the blow that fells the last member: the fall, the defeat's end, at the game's own pace, the speed's mark gone.
			++LostFrames;
			REQUIRE_FALSE(IsBattleFought(*Game->Loop));
			REQUIRE(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
		}
		else if (Battle.IsFought() && !Battle.IsInTransition())
		{
			++FoughtFast;
			REQUIRE(GamePace(*Game->Loop, Game->Session->Settings()) == 4);
		}
	}
	CHECK(FoughtFast > 0);
	// The fall and the end are seen: more than a moment.
	CHECK(LostFrames > 30);
	// The game over follows in the field, at the game's pace.
	REQUIRE(Game->Loop->Program() == EGameProgram::Field);
	CHECK(Game->Session->Game().ProgramResult == BattleModeResult::Lost);
	CHECK(GamePace(*Game->Loop, Game->Session->Settings()) == 1);
}
