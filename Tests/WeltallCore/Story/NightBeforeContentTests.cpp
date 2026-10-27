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
	 * The chair's fourth session (map 617 by entrance 4), the second time through (its bit set), tells in three
	 * pages of the force on Mahanon and the decision to go at dawn, then gives Fei and Elly to the Yggdrasil's deck
	 * (map 303 by entrance 3), where the original's night plays. The expanded second disc passes over the pages
	 * and goes to the ship's bridge, a map of its own, with the story at the eve's own value, where that is said;
	 * from the bridge the game goes to the same deck with the same party and story.
	 */
	constexpr uint16 ChairMap = 617;
	constexpr uint16 SessionEntrance = 4;
	constexpr uint16 SessionBitsVariable = 0x248 / 2;
	constexpr uint16 SecondTimeBit = 4;
	constexpr uint16 ChapterProgress = 285;
	constexpr uint16 EveProgress = 286;
	constexpr uint16 BridgeMap = 752;
	constexpr uint16 DeckMap = 303;
	constexpr uint16 DeckEntrance = 3;
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 NoOne = 0xFF;
	/**
	 * The saved variables of the field's place and time, not of what the story has done (as StoryContentTests has them),
	 * the entrance (0x002) and the last choice's answer (0x014) among them, and the map the game came from (0x030,
	 * 0x036), which is the bridge's own map in the expanded game and the chair's in the original.
	 */
	constexpr uint16 EntranceVariable = 0x002;
	constexpr uint16 MapLeftVariable = 0x004;
	constexpr uint16 LastChoiceVariable = 0x014;
	constexpr uint16 MapFromVariable = 0x030;
	constexpr uint16 MapFromAgainVariable = 0x036;
	constexpr std::array<uint16, 19> PlaceAndTimeVariables = {EntranceVariable, MapLeftVariable, LastChoiceVariable, MapFromVariable, MapFromAgainVariable, 0x006, 0x008, 0x00C, 0x00E, 0x012, 0x01E, 0x020, 0x022, 0x024, 0x02A, 0x02C, 0x02E, 0x048, 0x04A};

	/** A game of the second disc's story mode Mode at the chair's fourth session, the second time through. */
	std::unique_ptr<Testing::FGameLoopSession> StartAtSession(const FIso9660& Disc, EStoryMode Mode)
	{
		std::unique_ptr<Testing::FGameLoopSession> Game = Testing::FGameLoopSession::Start(Disc);
		Game->Session->SetStory(&Testing::PortStory());
		REQUIRE_FALSE(Game->Session->InitNewGame());
		FGameState& State = Game->Session->Game();
		State.SetStoryMode(Mode);
		State.SetSavedEventVariable(0, ChapterProgress);
		State.SetSavedEventVariable(1, SessionEntrance);
		State.SetSavedEventVariable(SessionBitsVariable, SecondTimeBit);
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
		std::string Where = "map " + std::to_string(Context->RunningMap) + " lock " + std::to_string(Context->PlayerLock) + (Context->bMenuLocked ? " menu-locked" : "") + " cursor-ticks " + std::to_string(Context->Messages.CursorTicks) + (Context->bFieldRunning ? " running" : " not-running") + (Context->bPlayerControlled ? " controlled" : " uncontrolled") + " world-map " + std::to_string(Context->Requests.bWorldMap) + " scripted-battle " + std::to_string(Context->Requests.bScriptedBattle) + " movie-view " + std::to_string(static_cast<int32>(Context->MovieView)) + " init-done " + std::to_string(Context->bInitDone) + " party-walking " + std::to_string(Context->bPartyWalking) + " movie " + std::to_string(Context->Requests.bMovie) + " program " + std::to_string(Context->Requests.bProgram) + " map-change " + std::to_string(Context->Requests.bMapChange) + " leaving " + std::to_string(Context->Requests.bLeavingField);
		for (const FFieldMessageWindow& Window : Context->Messages.Windows)
		{
			if (Window.bOpen)
			{
				Where += " window(owner " + std::to_string(Window.Owner) + " speaker " + std::to_string(Window.Speaker) + " focus " + std::to_string(Window.bFocus)
					+ " waits " + std::to_string(Window.bTextWaits) + " open-ticks " + std::to_string(Window.OpenTicks) + " flags " + std::to_string(Window.Flags) + " text " + std::to_string(Window.Text.has_value()) + " bytes " + std::to_string(Window.Message.size()) + " age " + std::to_string(Window.Age) + (Window.Text ? " text-state(flags " + std::to_string(Window.Text->State().Flags) + " reason " + std::to_string(Window.Text->State().Reason) + " wait " + std::to_string(Window.Text->State().WaitTimer) + " lines-typed " + std::to_string(Window.Text->State().LinesTyped) + " line " + std::to_string(Window.Text->State().Line) + " queued " + std::to_string(Window.Text->State().Queued) + " next " + std::to_string(Window.Text->State().Next) + " closing " + std::to_string(Window.Text->State().bClosing) + " x " + std::to_string(Window.Text->State().X) + " top " + std::to_string(Window.Text->State().TopLine) + " rect-w " + std::to_string(Window.Rect.Width) + " entries " + std::to_string(Window.Text->State().Widths.size()) + " speed " + std::to_string(Window.Text->State().Speed) + " base-speed " + std::to_string(Window.Text->State().BaseSpeed) + " widths" + [&Window]{ std::string W; for (const int32 Width : Window.Text->State().Widths) { W += " " + std::to_string(Width); } return W; }() + " delay " + std::to_string(Window.Text->State().DelayCounter) + "/" + std::to_string(Window.Text->State().DelayReload) + ")" : "") + " choice " + std::to_string(Window.ChoiceCursor) + "/" + std::to_string(Window.ChoiceCount) + ")";
			}
		}
		for (std::size_t Actor = 0; Actor < Context->States.size(); ++Actor)
		{
			for (std::size_t SlotIndex = 0; SlotIndex < Context->States[Actor].Threads.size(); ++SlotIndex)
			{
				const FEventThread& Thread = Context->States[Actor].Threads[SlotIndex];
				if (Thread.Script != FEventThread::NoScript || Actor == 17 || Actor == static_cast<std::size_t>(Context->LeaderActor))
				{
					Where += " slot" + std::to_string(SlotIndex);
					Where += " thread(actor " + std::to_string(Actor) + " script " + std::to_string(Thread.Script) + " pc " + std::to_string(Thread.Pc) + " wait " + std::to_string(Thread.WaitCount) + " priority " + std::to_string(Thread.Priority()) + " request-wait " + std::to_string((Thread.Word >> 16) & 3) + " slot-running " + std::to_string(Context->States[Actor].Slot) + " frozen " + std::to_string((Context->States[Actor].Flags0 & EventActorFlags0::Frozen) != 0) + " takes-part " + std::to_string(Context->Actors[Actor].TakesPartInEvents()) + " removed " + std::to_string((Context->States[Actor].Flags1 & EventActorFlags1::Removed) != 0) + " entry " + std::to_string(Context->Scripts != nullptr ? Context->Scripts->EntryPoint(static_cast<int32>(Actor), Thread.Script) : -1) + " bytes";
					for (int32 Offset = 0; Context->Scripts != nullptr && Offset < 6 && Thread.Pc + Offset < Context->Scripts->CodeSize(); ++Offset)
					{
						Where += " " + std::to_string(Context->Scripts->Code()[Thread.Pc + Offset]);
					}
					Where += ")";
				}
			}
		}
		for (const int32 Actor : {1, 2, 12, 13, 14, 17})
		{
			if (static_cast<std::size_t>(Actor) < Context->States.size())
			{
				const FEventActorState& State = Context->States[static_cast<std::size_t>(Actor)];
				Where += " actor" + std::to_string(Actor) + "(at " + std::to_string(State.Position[0] >> 16) + "," + std::to_string(State.Position[2] >> 16) + (State.Flags1 & EventActorFlags1::OffScreen ? " off-screen" : " on-screen") + ")";
			}
		}
		for (const FFieldPortraitSlot& Slot : Context->Messages.Portraits)
		{
			Where += " portrait(" + std::to_string(Slot.Character) + " load " + std::to_string(static_cast<int32>(Slot.Load)) + ")";
		}
		if (Context->LeaderActor >= 0 && static_cast<std::size_t>(Context->LeaderActor) < Context->States.size())
		{
			const FEventActorState& Leader = Context->States[static_cast<std::size_t>(Context->LeaderActor)];
			Where += " leader at " + std::to_string(Leader.Position[0] >> 16) + ", " + std::to_string(Leader.Position[2] >> 16);
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
			INFO(Whereabouts(Context) + (Field != nullptr ? " last-frame " + std::to_string(static_cast<int32>(Field->Runtime().LastFrame())) + " map-loads " + std::to_string(Field->Runtime().MapLoads()) : std::string()));
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

	/** Walks the leader from where it stands into a trigger at (X, Z) until the trigger has taken the player or the map has changed. */
	void WalkIntoTrigger(Testing::FGameLoopSession& Game, int32 Map, int32 X, int32 Z)
	{
		constexpr int32 WalkFrames = 3000;
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

TEST_CASE("The expanded second disc plays the night before Mahanon's briefing on the Yggdrasil's bridge, from the chair's session to the deck", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FrameLimit = 30000;

	// The original: the session's pages, then the deck.
	std::unique_ptr<Testing::FGameLoopSession> Original = StartAtSession(*Disc, EStoryMode::Original);
	CHECK(PlayFieldUntil(*Original, Reached(DeckMap), FrameLimit) == std::vector<int32>{ChairMap, DeckMap});
	const FGameState& Theirs = Original->Session->Game();
	CHECK(Theirs.SavedEventVariable(1) == DeckEntrance);
	CHECK(Theirs.SavedEventVariable(0) == ChapterProgress);
	CHECK(Theirs.Party() == std::array<uint8, 3>{Fei, Elly, NoOne});

	// The expanded game: from the chair to the bridge, the story at the eve's value, the player given the party.
	std::unique_ptr<Testing::FGameLoopSession> Expanded = StartAtSession(*Disc, EStoryMode::Expanded);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(BridgeMap), FrameLimit) == std::vector<int32>{ChairMap, BridgeMap});
	FGameState& Ours = Expanded->Session->Game();
	CHECK(Ours.SavedEventVariable(0) == EveProgress);
	CHECK(Ours.Party() == Theirs.Party());

	// The briefing at the chart, walked to, then the door to the deck, the story and state the original's.
	WalkIntoTrigger(*Expanded, BridgeMap, 0, 150);
	CHECK(PlayFieldUntil(*Expanded, PlayerHasPartyIn(BridgeMap), FrameLimit) == std::vector<int32>{BridgeMap});
	WalkIntoTrigger(*Expanded, BridgeMap, 0, 700);
	CHECK(PlayFieldUntil(*Expanded, Reached(DeckMap), FrameLimit) == std::vector<int32>{BridgeMap, DeckMap});
	CHECK(Ours.SavedEventVariable(1) == DeckEntrance);
	CHECK(Ours.SavedEventVariable(0) == ChapterProgress);
	CHECK(StoryDifferences(Ours, Theirs) == std::vector<std::string>{});
	CHECK(Ours.Party() == Theirs.Party());
}

TEST_CASE("The night-before documents read as the port's story, a patch of the chair and a map of its own", "[Story][Content][Packs]")
{
	const FStoryPacks& Story = Testing::PortStory();
	std::vector<int32> Maps;
	for (const FStoryDocument& Document : Story.Documents())
	{
		if (Document.bExpansion && Document.Name.find("disc2/night-before-") == 0)
		{
			Maps.push_back(Document.Patch.Map());
		}
	}
	std::sort(Maps.begin(), Maps.end());
	CHECK(Maps == std::vector<int32>{ChairMap, BridgeMap});
	REQUIRE(Story.WayBack() != nullptr);
	CHECK(Story.SubtitleAt(EveProgress, true) != nullptr);
}
