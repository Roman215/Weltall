#include "Field/FieldRuntime.h"
#include "Game/FieldProgram.h"
#include "Story/StoryPacks.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/PortStory.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/**
	 * Fei's chair's first session (map 618 by entrance 0, the story at the front's value) tells of the Mobile Weapon
	 * and of going to Nortune, then goes to Nortune's gate at night (map 198 by entrance 8), where the rising of the
	 * Central District is narrated and the Yggdrasil IV's battle fought, and from there back to the chair. The
	 * expanded second disc leaves the chair for the gate under curfew, a map of its own, with Fei, Bart and Rico and
	 * the story at the battleship's own value; the Kaiser's hall, the tactical command room and the bridge, maps of
	 * their own, follow, and from the bridge the game goes on to the same scene at the gate with the same party and
	 * story as the original.
	 */
	constexpr uint16 ChairMap = 618;
	constexpr uint16 SessionEntrance = 0;
	constexpr uint16 FrontProgress = 252;
	constexpr uint16 BattleshipProgress = 254;
	constexpr uint16 GateMap = 745;
	constexpr uint16 HallMap = 746;
	constexpr uint16 CommandMap = 747;
	constexpr uint16 BridgeMap = 748;
	constexpr uint16 SceneEntrance = 1;
	constexpr int32 KeelEntrance = 1;
	constexpr int32 EnginesEntrance = 2;
	constexpr int32 LinkEntrance = 3;
	constexpr uint16 LiftEntrance = 4;
	constexpr uint16 RisingMap = 198;
	constexpr uint16 RisingEntrance = 8;
	constexpr uint8 Fei = 0;
	constexpr uint8 Bart = 3;
	constexpr uint8 Rico = 5;
	constexpr uint8 NoOne = 0xFF;
	/**
	 * The saved variables of the field's place and time, not of what the story has done (as StoryContentTests has them),
	 * the entrance (0x002) among them, since the tests enter the maps of their own by their own entrances, and the
	 * last choice's answer (0x014), which the chair's save prompt leaves and any later message resets.
	 */
	constexpr uint16 EntranceVariable = 0x002;
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr uint16 LastChoiceVariable = 0x014;
	constexpr std::array<uint16, 17> PlaceAndTimeVariables = {EntranceVariable, MapLeftVariable, LastChoiceVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};

	/** A game of the second disc's story mode Mode at the chair's first session, the story at the front's value. */
	std::unique_ptr<Testing::FGameLoopSession> StartAtSession(const FIso9660& Disc, EStoryMode Mode)
	{
		std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(Mode);
		State.SetSavedEventVariable(0, FrontProgress);
		State.SetSavedEventVariable(1, SessionEntrance);
		State.SetFieldEntrance(SessionEntrance);
		Game->EnterField(ChairMap);
		return Game;
	}

	bool PageWaits(const FEventContext& Context)
	{
		return std::any_of(Context.Messages.Windows.begin(), Context.Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bTextWaits; });
	}

	/** Whether an open choice window's cursor is not yet on its last option (the chair's "Don't save" and "Start"). */
	bool ChoiceBelowLast(const FEventContext* Context)
	{
		return Context != nullptr && std::any_of(Context->Messages.Windows.begin(), Context->Messages.Windows.end(),
			[](const FFieldMessageWindow& Window) { return Window.bOpen && Window.bChoice && Window.ChoiceCursor < Window.ChoiceCount - 1; });
	}

	/** What the field is at, for the report of a play that goes nowhere. */
	std::string Whereabouts(const FEventContext* Context)
	{
		if (Context == nullptr)
		{
			return "no field running";
		}
		std::string Where = "map " + std::to_string(Context->RunningMap) + " lock " + std::to_string(Context->PlayerLock) + (Context->bMenuLocked ? " menu-locked" : "");
		for (const FFieldMessageWindow& Window : Context->Messages.Windows)
		{
			if (Window.bOpen)
			{
				Where += " window(owner " + std::to_string(Window.Owner) + " speaker " + std::to_string(Window.Speaker) + " focus " + std::to_string(Window.bFocus)
					+ " waits " + std::to_string(Window.bTextWaits) + " choice " + std::to_string(Window.ChoiceCursor) + "/" + std::to_string(Window.ChoiceCount) + ")";
			}
		}
		return Where;
	}

	/**
	 * Plays the field on, confirming each message and taking the last option of each choice (the chair's save prompt
	 * is declined and the game continued, the same in both ways of playing), until Done; the field maps played.
	 */
	std::vector<int32> PlayFieldUntil(Testing::FGameLoopSession& Game, const std::function<bool(const FEventContext*)>& Done, int32 FrameLimit)
	{
		constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
		constexpr FPadState DownHeld{FPadState::Bit(EPadButton::Down)};
		constexpr int32 FramesAPress = 8;
		std::vector<int32> Maps;
		for (int32 Frame = 0;; ++Frame)
		{
			const FFieldProgram* Field = Game.Loop->Field();
			const FEventContext* Context = Field != nullptr ? &Field->Runtime().Context() : nullptr;
			INFO(Whereabouts(Context));
			REQUIRE(Frame < FrameLimit);
			if (Context != nullptr && (Maps.empty() || Maps.back() != Context->RunningMap))
			{
				Maps.push_back(Context->RunningMap);
			}
			if (Done(Context))
			{
				return Maps;
			}
			const FPadState Pressed = ChoiceBelowLast(Context) ? DownHeld : CrossHeld;
			const std::optional<FError> Error = Game.Frame((Frame / FramesAPress) % 2 == 0 ? Pressed : FPadState{});
			INFO((Error ? Error->Message : std::string()));
			REQUIRE_FALSE(Error);
		}
	}

	/** Until the player has the party in Map, no page waiting. */
	std::function<bool(const FEventContext*)> PlayerHasPartyIn(int32 Map)
	{
		return [Map](const FEventContext* Context)
		{
			return Context != nullptr && Context->RunningMap == Map && Context->PlayerLock == 0 && !Context->bMenuLocked && !PageWaits(*Context);
		};
	}

	/** Until the field runs Map. */
	std::function<bool(const FEventContext*)> Reached(int32 Map)
	{
		return [Map](const FEventContext* Context) { return Context != nullptr && Context->RunningMap == Map; };
	}

	std::vector<std::string> StoryDifferences(const FGameState& Ours, const FGameState& Theirs)
	{
		std::vector<std::string> Differing;
		for (int32 Variable = 0; Variable < FGameState::SavedEventVariableCount; ++Variable)
		{
			const uint16 Address = static_cast<uint16>(Variable * 2);
			const bool bPlaceOrTime = std::find(PlaceAndTimeVariables.begin(), PlaceAndTimeVariables.end(), Address) != PlaceAndTimeVariables.end();
			if (!bPlaceOrTime && Ours.SavedEventVariable(Variable) != Theirs.SavedEventVariable(Variable))
			{
				char Line[32];
				std::snprintf(Line, sizeof(Line), "%03X: %d %d", Address, Ours.SavedEventVariable(Variable), Theirs.SavedEventVariable(Variable));
				Differing.emplace_back(Line);
			}
		}
		return Differing;
	}

	/** An angle of the game's (4096 a turn) of the way along Across and Along, as atan2 gives it. */
	int32 GameAngle(double Across, double Along)
	{
		return static_cast<int32>(std::lround(std::atan2(Across, Along) * 2048.0 / std::numbers::pi));
	}

	/** The d-pad buttons that walk the field's leader most nearly toward (X, Z) (as MassDriverContentTests has them). */
	uint16 FieldPadToward(const FEventContext& Context, int32 X, int32 Z)
	{
		constexpr uint16 Up = FPadState::Bit(EPadButton::Up);
		constexpr uint16 Right = FPadState::Bit(EPadButton::Right);
		constexpr uint16 Down = FPadState::Bit(EPadButton::Down);
		constexpr uint16 Left = FPadState::Bit(EPadButton::Left);
		static constexpr std::array<uint16, 8> ByEighth = {Right, Right | Down, Down, Down | Left, Left, Left | Up, Up, Up | Right};
		const FFieldPoint& At = Context.States[Context.LeaderActor].Position;
		const int32 Way = GameAngle(-(Z - (At[2] >> 16)), X - (At[0] >> 16));
		const int32 Pad = (Way + Context.Camera.GoalViewYaw + Context.ViewTurn) & 0xFFF;
		return ByEighth[static_cast<std::size_t>(((Pad + 0x100) >> 9) & 7)];
	}

	/**
	 * Enters Map by Entrance, a step before a trigger at (X, Z), and walks the leader into it until the trigger has
	 * taken the player (a scene begun) or the map has changed.
	 */
	void WalkIntoTrigger(Testing::FGameLoopSession& Game, int32 Map, uint16 Entrance, int32 X, int32 Z)
	{
		constexpr int32 WalkFrames = 1500;
		FGameState& State = Game.Session->Game();
		State.SetSavedEventVariable(1, Entrance);
		State.SetFieldEntrance(Entrance);
		Game.EnterField(Map);
		for (int32 Frame = 0;; ++Frame)
		{
			const FFieldProgram* Field = Game.Loop->Field();
			const FEventContext* Context = Field != nullptr ? &Field->Runtime().Context() : nullptr;
			if (Context == nullptr || Context->RunningMap != Map || Context->PlayerLock != 0)
			{
				return;
			}
			const FFieldPoint& At = Context->States[Context->LeaderActor].Position;
			INFO("walking into the trigger at " << X << ", " << Z << " of map " << Map << ", the leader at " << (At[0] >> 16) << ", " << (At[2] >> 16));
			REQUIRE(Frame < WalkFrames);
			const std::optional<FError> Error = Game.Frame(FPadState{FieldPadToward(*Context, X, Z)});
			INFO((Error ? Error->Message : std::string()));
			REQUIRE_FALSE(Error);
		}
	}
}

TEST_CASE("The expanded second disc plays Nortune's secret battleship, from Fei's chair to the rising the chair narrates", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 30000;

	// The original: the session's narration, then the gate at night by the scene's entrance.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAtSession(*Disc, EStoryMode::Original);
	CHECK(PlayFieldUntil(*Original, Reached(RisingMap), FrameLimit) == std::vector<int32>{ChairMap, RisingMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(1) == RisingEntrance);
	CHECK(Theirs.SavedEventVariable(0) == FrontProgress);
	CHECK(Theirs.Party() == std::array<uint8, 3>{Fei, NoOne, NoOne});

	// The expanded game: from the chair to the gate under curfew, the story at the battleship's value, the player given the party.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAtSession(*Disc, EStoryMode::Expanded);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(GateMap), FrameLimit) == std::vector<int32>{ChairMap, GateMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == BattleshipProgress);
	CHECK(Ours.Party() == std::array<uint8, 3>{Fei, Bart, Rico});

	// The gate's scene, walked into from a step before the gate, leads to the Kaiser's hall.
	WalkIntoTrigger(*Expanded, GateMap, SceneEntrance, 425, -182);
	CHECK(PlayFieldUntil(*Expanded, Reached(HallMap), FrameLimit) == std::vector<int32>{GateMap, HallMap});

	// The Kaiser's hall: his scene plays through and leaves the player the party.
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(HallMap), FrameLimit) == std::vector<int32>{HallMap});
	CHECK(Ours.SavedEventVariable(0) == BattleshipProgress);

	// The tactical command room: each station, walked into, is set; the lift, the three set, leads to the bridge.
	for (const auto& [Station, X, Z] : {std::array<int32, 3>{KeelEntrance, -500, -55}, {EnginesEntrance, -100, -55}, {LinkEntrance, 1300, -155}})
	{
		WalkIntoTrigger(*Expanded, CommandMap, static_cast<uint16>(Station), X, Z);
		CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(CommandMap), FrameLimit) == std::vector<int32>{CommandMap});
	}
	WalkIntoTrigger(*Expanded, CommandMap, LiftEntrance, 397, -120);
	CHECK(PlayFieldUntil(*Expanded, Reached(BridgeMap), FrameLimit) == std::vector<int32>{CommandMap, BridgeMap});

	// The bridge: the Kaiser's scene, then the original's scene at the gate, the story and party the original's.
	CHECK(PlayFieldUntil(*Expanded, Reached(RisingMap), FrameLimit) == std::vector<int32>{BridgeMap, RisingMap});
	CHECK(Ours.SavedEventVariable(1) == RisingEntrance);
	CHECK(Ours.SavedEventVariable(0) == FrontProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The Nortune documents read as the port's story, a patch of the chair and four maps of their own", "[Story][Content][Packs]")
{
	const FStoryPacks& Story = Testing::PortStory();
	std::vector<int32> Maps;
	for (const FStoryDocument& Document : Story.Documents())
	{
		if (Document.bExpansion && Document.Name.find("disc2/nortune-") == 0)
		{
			Maps.push_back(Document.Patch.Map());
		}
	}
	std::sort(Maps.begin(), Maps.end());
	CHECK(Maps == std::vector<int32>{ChairMap, GateMap, HallMap, CommandMap, BridgeMap});
	REQUIRE(Story.WayBack() != nullptr);
	CHECK(Story.SubtitleAt(BattleshipProgress, true) != nullptr);
}
