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
	 * Mahanon in the original: the approach (map 652, narrated) goes to a camera's flight through the wreck (map 653,
	 * narrated, with the save prompt) and on to the hallway to the nucleus (map 654, played); after the nucleus the
	 * hallway goes to Razael's cavern's scene (map 655: the descent narrated, the data, Krelian, Grahf's battle). The
	 * expanded second disc walks the wreck, a map of its own, between the approach and the hallway, the story at the
	 * wreck's own value, and walks the cavern's floor, a map of its own, before the scene, the story at the Tree's
	 * own value, which the scene then passes over the pages the walk has said.
	 */
	constexpr uint16 ApproachMap = 652;
	constexpr uint16 FlightMap = 653;
	constexpr uint16 HallwayMap = 654;
	constexpr uint16 CavernSceneMap = 655;
	constexpr uint16 WreckMap = 749;
	constexpr uint16 CavernMap = 750;
	constexpr uint16 ChainProgress = 288;
	constexpr uint16 WreckProgress = 289;
	constexpr uint16 SceneProgress = 290;
	constexpr uint16 TreeProgress = 291;
	constexpr uint16 FirstEntrance = 0;
	constexpr uint16 LogEntrance = 1;
	constexpr uint16 RingEntrance = 2;
	constexpr uint16 DownEntrance = 3;
	constexpr uint16 CentreEntrance = 1;
	constexpr uint8 Fei = 0;
	constexpr uint8 Bart = 3;
	constexpr uint8 Citan = 9;
	/**
	 * The saved variables of the field's place and time, not of what the story has done (as StoryContentTests has them),
	 * the entrance (0x002) among them, since the tests enter the maps of their own by their own entrances, and the
	 * last choice's answer (0x014), which the flight's save prompt leaves and any later message resets.
	 */
	constexpr uint16 EntranceVariable = 0x002;
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr uint16 LastChoiceVariable = 0x014;
	constexpr std::array<uint16, 17> PlaceAndTimeVariables = {EntranceVariable, MapLeftVariable, LastChoiceVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};

	/** A game of the second disc's story mode Mode at Map by Entrance, the story at Progress, the party Fei, Citan and Bart. */
	std::unique_ptr<Testing::FGameLoopSession> StartAt(const FIso9660& Disc, EStoryMode Mode, uint16 Map, uint16 Entrance, uint16 Progress)
	{
		std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(Mode);
		State.SetParty({Fei, Citan, Bart});
		State.SetSavedEventVariable(0, Progress);
		State.SetSavedEventVariable(1, Entrance);
		State.SetFieldEntrance(Entrance);
		Game->EnterField(Map);
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

TEST_CASE("The expanded second disc walks Mahanon's wreck, from the approach to the hallway the flight through it goes on to", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 30000;

	// The original: the approach's narration, the flight through the wreck and its save prompt, then the hallway.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAt(*Disc, EStoryMode::Original, ApproachMap, FirstEntrance, ChainProgress);
	CHECK(PlayFieldUntil(*Original, Reached(HallwayMap), FrameLimit) == std::vector<int32>{ApproachMap, FlightMap, HallwayMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(1) == FirstEntrance);
	CHECK(Theirs.SavedEventVariable(0) == ChainProgress);

	// The expanded game: from the approach to the wreck's breach, the story at the wreck's value, the player given the party.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAt(*Disc, EStoryMode::Expanded, ApproachMap, FirstEntrance, ChainProgress);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(WreckMap), FrameLimit) == std::vector<int32>{ApproachMap, WreckMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == WreckProgress);
	CHECK(Ours.Party() == Theirs.Party());

	// The log terminal and the ring, each walked into, say their pages and leave the player the party.
	WalkIntoTrigger(*Expanded, WreckMap, LogEntrance, 0, -1500);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(WreckMap), FrameLimit) == std::vector<int32>{WreckMap});
	WalkIntoTrigger(*Expanded, WreckMap, RingEntrance, 0, -383);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(WreckMap), FrameLimit) == std::vector<int32>{WreckMap});

	// The way down leads to the hallway, the story and state the original's.
	WalkIntoTrigger(*Expanded, WreckMap, DownEntrance, 0, 830);
	CHECK(PlayFieldUntil(*Expanded, Reached(HallwayMap), FrameLimit) == std::vector<int32>{WreckMap, HallwayMap});
	CHECK(Ours.SavedEventVariable(1) == FirstEntrance);
	CHECK(Ours.SavedEventVariable(0) == ChainProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The expanded second disc walks Razael's cavern's floor, then plays the original's scene past the pages the walk has said", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 30000;

	// Until Grahf's battle takes the field, once the scene has run.
	const auto BattleBegun = [](int32 Map)
	{
		return [Map, bSeen = false](const FEventContext* Context) mutable
		{
			bSeen = bSeen || (Context != nullptr && Context->RunningMap == Map);
			return bSeen && Context == nullptr;
		};
	};

	// The original: the scene from its start to the battle.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAt(*Disc, EStoryMode::Original, CavernSceneMap, FirstEntrance, SceneProgress);
	CHECK(PlayFieldUntil(*Original, BattleBegun(CavernSceneMap), FrameLimit) == std::vector<int32>{CavernSceneMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(0) == SceneProgress);

	// The expanded game: the cavern's floor walked to the centre, then the scene to the battle, the story and state the original's.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAt(*Disc, EStoryMode::Expanded, CavernMap, CentreEntrance, TreeProgress);
	WalkIntoTrigger(*Expanded, CavernMap, CentreEntrance, 0, -1300);
	CHECK(PlayFieldUntil(*Expanded, Reached(CavernSceneMap), FrameLimit) == std::vector<int32>{CavernMap, CavernSceneMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(PlayFieldUntil(*Expanded, BattleBegun(CavernSceneMap), FrameLimit) == std::vector<int32>{CavernSceneMap});
	CHECK(Ours.SavedEventVariable(0) == SceneProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The Mahanon documents read as the port's story: patches of the approach, the hallway and the cavern's scene, and two maps of their own", "[Story][Content][Packs]")
{
	const FStoryPacks& Story = Testing::PortStory();
	std::vector<int32> Maps;
	for (const FStoryDocument& Document : Story.Documents())
	{
		if (Document.bExpansion && Document.Name.find("disc2/mahanon-") == 0)
		{
			Maps.push_back(Document.Patch.Map());
		}
	}
	std::sort(Maps.begin(), Maps.end());
	CHECK(Maps == std::vector<int32>{ApproachMap, HallwayMap, CavernSceneMap, WreckMap, CavernMap});
	REQUIRE(Story.WayBack() != nullptr);
	CHECK(Story.SubtitleAt(WreckProgress, true) != nullptr);
	CHECK(Story.SubtitleAt(TreeProgress, true) != nullptr);
}
