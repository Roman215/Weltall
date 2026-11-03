#include "Support/StorySweepRun.h"

#include "Field/EventContext.h"
#include "Field/EventMachine.h"
#include "Field/EventScripts.h"
#include "Field/FieldArchive.h"
#include "Field/FieldMap.h"
#include "Field/FieldWalkmesh.h"
#include "Battle/BattleRuntime.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/KernelState.h"
#include "Game/PartyOrder.h"
#include "Game/StoryMode.h"
#include "Game/WorldMapProgram.h"
#include "Graphics/Sprite.h"
#include "Menu/MenuProgram.h"
#include "Support/StorySweepStrength.h"
#include "Support/StorySweepWorldMap.h"
#include "Support/StoryValues.h"
#include "Timing/PsxTiming.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <set>
#include <span>
#include <utility>

namespace Weltall::Testing
{
	namespace
	{
		constexpr uint16 Cross = FPadState::Bit(EPadButton::Cross);
		constexpr uint16 Circle = FPadState::Bit(EPadButton::Circle);
		constexpr uint16 Down = FPadState::Bit(EPadButton::Down);
		constexpr uint16 Start = FPadState::Bit(EPadButton::Start);
		/** A button is tapped for two frames of every twelve. */
		constexpr int32 TapCycle = 12;
		constexpr int32 TapFrames = 2;
		/** A question is given the same answer this many times before another is tried. */
		constexpr int32 AnswersBeforeNext = 2;
		/** The 64-bit FNV-1a digest a question is known by: its start and its prime. */
		constexpr uint64 QuestionDigestStart = 0xCBF29CE484222325ull;
		constexpr uint64 QuestionDigestPrime = 0x100000001B3ull;
		/** The frames a scene may go round where it is, no window open, before the buttons other than the confirm are tried. */
		constexpr int32 OtherButtonFrames = 240;
		/** The game state's word for the disc a save was made on. */
		constexpr std::size_t SavedDiscWord = 0x19D4;
		/** The points of ground (four units apart) below which the leader's walk goes nowhere. */
		constexpr int32 HemmedInPoints = 64;
		/** The frames a scene comes to no instruction new to the stretch, whatever its windows do, before the other buttons are tried. */
		constexpr int32 StaleButtonFrames = 900;
		/** The frames each of them is tapped for in its turn. */
		constexpr int32 OtherButtonTurn = 48;
		constexpr std::array<uint16, 9> OtherButtons = {FPadState::Bit(EPadButton::Circle), FPadState::Bit(EPadButton::Triangle), FPadState::Bit(EPadButton::Square),
			FPadState::Bit(EPadButton::Down), FPadState::Bit(EPadButton::Up), FPadState::Bit(EPadButton::Left), FPadState::Bit(EPadButton::Right),
			FPadState::Bit(EPadButton::Start), FPadState::Bit(EPadButton::Select)};
		/** A cutscene over within this many frames is the party's look flipping there and back. */
		constexpr int32 ShortCutsceneFrames = 120;
		/** A menu is cancelled out of for this long, then confirmed through, before the run gives up on it. */
		constexpr int32 MenuCancelFrames = 360;
		constexpr int32 MenuFrames = 900;
		/** The frames the world map has to show itself a cutscene rather than the player's travel. */
		constexpr int32 WorldMapFrames = 30;
		/** A stretch that arrives on one map more often than this with the story where it was goes round in circles. */
		constexpr int32 MostArrivals = 12;
		constexpr std::size_t MostThreadsNamed = 12;

		constexpr uint8 ExtendedOpcode = 0xFE;
		constexpr uint8 MessageChoiceOpcode = 0xA9;
		/** An actor's second flag word: the leader's collisions met its body this tick. */
		constexpr uint32 MetByLeader = 0x400000;
		/** The game state's result after a battle lost. */
		constexpr uint8 LostResult = 1;
		/** Each character's own first gear, for one a save gives none: the maps skipped on the way would have given it. */
		constexpr std::array<uint8, 9> OwnGears = {0, 2, 3, 4, 5, 6, 9, 7, 8};
		constexpr uint8 NoGear = 0xFF;
		/** A place's field map, without the bits that say the party is in gears; from this value up, a place of the world map. */
		constexpr uint16 PlaceMapBits = 0x3FFF;
		constexpr uint16 FirstWorldMapPlace = 0x400;

		/** Notes where every thread of the scripts stands; whether one stands where none of the stretch's has stood before. */
		bool ReachesNewSteps(const FEventContext& Context, std::set<uint32>& Steps)
		{
			bool bNew = false;
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				for (const FEventThread& Thread : Context.States[Actor].Threads)
				{
					if (!Thread.IsFree())
					{
						bNew = Steps.insert(static_cast<uint32>(Actor) << 16 | Thread.Pc).second || bNew;
					}
				}
			}
			return bNew;
		}

		/**
		 * A choice about to open: a window's text has ended and its owner's script stands at the choice, which takes the
		 * window's lines without a press. A tap then would answer it too, on its first entry, before it is seen.
		 */
		bool OpensChoice(const FEventContext& Context)
		{
			if (Context.Scripts == nullptr)
			{
				return false;
			}
			for (const FFieldMessageWindow& Window : Context.Messages.Windows)
			{
				if (!Window.bOpen || Window.bChoice || !Window.Text || Window.Text->StopReason() != EMesStop::End || Window.Owner < 0
					|| static_cast<std::size_t>(Window.Owner) >= Context.States.size())
				{
					continue;
				}
				for (const FEventThread& Thread : Context.States[static_cast<std::size_t>(Window.Owner)].Threads)
				{
					if (!Thread.IsFree() && Thread.Pc < Context.Scripts->CodeSize() && Context.Scripts->Code()[Thread.Pc] == MessageChoiceOpcode)
					{
						return true;
					}
				}
			}
			return false;
		}

		bool Taps(int32 Frame, int32 Phase)
		{
			const int32 At = Frame % TapCycle;
			return At >= Phase && At < Phase + TapFrames;
		}

		/** The game's party, whoever is in it, kept in the order of its people from the last to the first. */
		void KeepReversed(FGameState& Game)
		{
			constexpr std::array<uint8, 9> People = {8, 7, 6, 5, 4, 3, 2, 1, 0};
			const std::array<uint8, 3> Party = Game.Party();
			FPartyOrder Order;
			Order.Arrange(Party, People);
			Game.SetPartyOrder(Order);
		}

		std::string Hex(uint32 Value)
		{
			char Text[16];
			std::snprintf(Text, sizeof(Text), "%X", Value);
			return Text;
		}

	}

	const char* StorySweepEndName(EStorySweepEnd End)
	{
		switch (End)
		{
		case EStorySweepEnd::Control:
			return "control";
		case EStorySweepEnd::Held:
			return "held";
		case EStorySweepEnd::Stops:
			return "STOPS";
		case EStorySweepEnd::WorldMap:
			return "world map";
		case EStorySweepEnd::Menu:
			return "menu";
		case EStorySweepEnd::BattleRuns:
			return "battle runs";
		case EStorySweepEnd::BattleLost:
			return "battle lost";
		case EStorySweepEnd::Arena:
			return "arena";
		case EStorySweepEnd::Loops:
			return "loops";
		case EStorySweepEnd::Long:
			return "long";
		}
		return "";
	}

	const char* StorySweepOpeningSuffix(EStorySweepOpening Opening)
	{
		switch (Opening)
		{
		case EStorySweepOpening::Forced:
			return " forced";
		case EStorySweepOpening::Unanswered:
			return " unanswered";
		case EStorySweepOpening::Unreached:
			return " unreached";
		default:
			return "";
		}
	}

	std::optional<FError> FStorySweepRun::Begin(const FOriginalSave* Save)
	{
		Loop.reset();
		Session.reset();
		Menus.reset();
		Unheard.reset();
		auto Heard = FGameSoundSession::Start(*Game.Discs[0].Iso);
		if (!Heard)
		{
			return Heard.Error();
		}
		Sound = std::move(*Heard);
		Unheard = std::make_unique<FUnheardGameSound>(*Sound->Sound());
		auto Started = Game.Start(0, Save, Sound.get());
		if (!Started)
		{
			return Started.Error();
		}
		Session = std::move(*Started);
		Menus = std::make_unique<FMenuGameMenus>(*Game.Discs[0].Executable);
		Session->SetMenus(Menus.get());
		Session->SetStory(Story);
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::EnterPlace(uint16 Place)
	{
		if (Options.bStrongParty)
		{
			StrengthenParty(Session->Game());
		}
		if (Options.bClearGame)
		{
			BringEllyBack(Session->Game());
		}
		if (Options.bKeepElly)
		{
			KeepEllyInParty(Session->Game());
		}
		if (Options.bFeiAsId)
		{
			DrawFeiAsId(Session->Game());
		}
		const FGameState& State = Session->Game();
		const std::array<uint8, 3> Members = State.Party();
		StartedParty = std::to_string(Members[0]) + "," + std::to_string(Members[1]) + "," + std::to_string(Members[2]);
		const uint16 StoryValue = State.SavedEventVariable(0);
		// What the walks met stays met while the same place is gone on from again.
		if ((Place & PlaceMapBits) != CrossedMap || StoryValue != CrossedStory)
		{
			Crossed.clear();
			Crossings.clear();
			Said.clear();
			Answers.clear();
			CrossedMap = Place & PlaceMapBits;
			CrossedStory = StoryValue;
		}
		if (Options.bReversesOrder)
		{
			KeepReversed(Session->Game());
		}
		auto Entered = (Place & PlaceMapBits) < FirstWorldMapPlace ? FGameLoop::EnterField(*Session, Place) : FGameLoop::EnterWorldMap(*Session);
		if (!Entered)
		{
			return Entered.Error();
		}
		Loop = std::move(*Entered);
		Random = FXorShift32(static_cast<uint32>((Place & PlaceMapBits) * 512 + StoryValue + 1));
		Played = 0;
		Asked.reset();
		AskedRoute.clear();
		KeptProbes.reset();
		return std::nullopt;
	}

	std::optional<FError> FStorySweepRun::Enter(int32 Map, uint16 StoryValue)
	{
		const FOriginalSave* Save = nullptr;
		for (const FOriginalSave& Candidate : Saves)
		{
			const bool bReached = Options.bSaveBefore ? Candidate.StoryProgress() < StoryValue : Candidate.StoryProgress() <= StoryValue;
			if (bReached && (Save == nullptr || Candidate.StoryProgress() >= Save->StoryProgress()))
			{
				Save = &Candidate;
			}
		}
		if (auto Error = Begin(Save))
		{
			return Error;
		}
		FGameState& State = Session->Game();
		// The disc the story plays on by then is in the drive: the second past the disc change, the save's own until it.
		if (Game.Discs[1].Iso != nullptr)
		{
			Session->Kernel().Disc = StoryValue > DiscChangeStoryProgress ? 1 : State.SavedDisc();
		}
		State.SetStoryMode(Options.bExpanded && StoryValue >= DiscChangeStoryProgress ? EStoryMode::Expanded : EStoryMode::Original);
		State.SetSavedEventVariable(0, StoryValue);
		for (const FStorySweepVariable& Variable : Options.Variables)
		{
			const int32 Index = static_cast<int32>(Variable.Address >> 1);
			const uint16 Was = State.SavedEventVariable(Index);
			State.SetSavedEventVariable(Index, Variable.Kind == '=' ? Variable.Value : Variable.Kind == '|' ? static_cast<uint16>(Was | Variable.Value)
				: static_cast<uint16>(Was & ~Variable.Value));
		}
		// The map's first entrance: the save's is another map's.
		State.SetSavedEventVariable(1, 0);
		State.SetFieldEntrance(0);
		for (uint8 Character = 0; Character < OwnGears.size(); ++Character)
		{
			if (State.CharacterGear(Character) == std::optional<uint8>(NoGear))
			{
				State.SetCharacterGear(Character, OwnGears[Character]);
			}
		}
		StartedFrom = Save != nullptr ? Save->StoryProgress() : -1;
		return EnterPlace(static_cast<uint16>(Map));
	}

	std::optional<FError> FStorySweepRun::Resume(const FGameState& From)
	{
		// The copy first: From may be this run's own state, which beginning anew replaces.
		FGameState State = From;
		if (auto Error = Begin(nullptr))
		{
			return Error;
		}
		if (State.StoryMode() == EStoryMode::Unchosen)
		{
			State.SetStoryMode(Options.bExpanded && State.SavedEventVariable(0) >= DiscChangeStoryProgress ? EStoryMode::Expanded : EStoryMode::Original);
		}
		if (Game.Discs[1].Iso != nullptr)
		{
			Session->Kernel().Disc = State.SavedDisc();
		}
		StartedFrom = State.SavedEventVariable(0);
		const uint16 Place = State.Place();
		Session->Game() = std::move(State);
		return EnterPlace(Place);
	}

	std::optional<FError> FStorySweepRun::Frame(FPadState Held)
	{
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop->VblanksPerFrame()), Held);
		Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		++Played;
		if (Options.bReversesOrder)
		{
			KeepReversed(Session->Game());
		}
		std::optional<FError> Error = Loop->Frame(PerVblank);
		WatchStaging();
		return Error;
	}

	void FStorySweepRun::WatchStaging()
	{
		const FFieldProgram* Field = Loop->Program() == EGameProgram::Field ? Loop->Field() : nullptr;
		if (Field == nullptr)
		{
			if (Unstaged >= Options.HoldFramesTold && Options.HoldFramesTold > 0)
			{
				std::fprintf(stderr, "HOLD\t%d\t%d\t%d\t%d\n", UnstagedMap, UnstagedStory, Unstaged, UnstagedWindows);
			}
			Unstaged = 0;
			CutsceneFrames = 0;
			return;
		}
		const FEventContext& Context = Field->Runtime().Context();
		const bool bCutscene = Field->Drawing().Cutscenes().InCutscene();
		const bool bTalk = std::any_of(Context.States.begin(), Context.States.end(), [](const FEventActorState& State)
			{
				return std::any_of(State.Threads.begin(), State.Threads.end(),
					[](const FEventThread& Thread) { return !Thread.IsFree() && Thread.Script == FEventMachine::TalkScript; });
			});
		// With an order kept, what counts is a slot drawn as another member; without one, the watch that would let it be.
		const FFieldMemberStandIns& Members = Field->Drawing().MemberStandIns();
		const bool bAsPlay = Options.bReversesOrder ? Members.ShownIn(0) || Members.ShownIn(1) || Members.ShownIn(2) : !bCutscene;
		// Held: locked by the scripts, or on a map with nobody for the pad to walk.
		const bool bLeader = std::any_of(Context.States.begin(), Context.States.end(),
			[](const FEventActorState& State) { return (State.Flags0 & EventActorFlags0::PadControlled) != 0; });
		const bool bUnstaged = Context.bFieldRunning && (Context.PlayerLock != 0 || !bLeader) && !bTalk && bAsPlay;
		if (!bUnstaged && Unstaged >= Options.HoldFramesTold && Options.HoldFramesTold > 0)
		{
			std::fprintf(stderr, "HOLD\t%d\t%d\t%d\t%d\n", UnstagedMap, UnstagedStory, Unstaged, UnstagedWindows);
		}
		if (bUnstaged && Unstaged == 0)
		{
			UnstagedMap = Context.RunningMap;
			UnstagedStory = Context.Variables.Raw(0);
			UnstagedWindows = 0;
		}
		UnstagedWindows += bUnstaged ? Context.MessagesOpened : 0;
		Unstaged = bUnstaged ? Unstaged + 1 : 0;
		if (Unstaged > LongestUnstaged)
		{
			LongestUnstaged = Unstaged;
			LongestUnstagedMap = Context.RunningMap;
		}
		if (bCutscene && Context.RunningMap == CutsceneMap)
		{
			++CutsceneFrames;
		}
		else
		{
			// A cutscene another map takes over has not ended for the player's eye.
			if (!bCutscene && CutsceneFrames > 0 && CutsceneFrames < ShortCutsceneFrames && Context.RunningMap == CutsceneMap)
			{
				++ShortCutscenes;
			}
			CutsceneFrames = bCutscene ? 1 : 0;
			CutsceneMap = Context.RunningMap;
		}
	}

	FPadState FStorySweepRun::ScenePad(const FEventContext* Context, bool bFree, int32 Idle, int32 Stale)
	{
		if (bFree)
		{
			return {};
		}
		// Nothing waits for a button while a map is still coming in: a tap then is read as the map's first, a talk
		// to whoever the party arrives beside (a Member Change's plate brings the party back onto itself).
		if (Context != nullptr && !Context->bFieldRunning && Context->Messages.OpenMask == 0)
		{
			return {};
		}
		const FFieldMessageWindow* Asking = nullptr;
		for (const FFieldMessageWindow& Window : Context != nullptr ? std::span<const FFieldMessageWindow>(Context->Messages.Windows) : std::span<const FFieldMessageWindow>())
		{
			if (Window.bOpen && Window.bChoice && Window.Age == 0 && Window.OpenTicks == 0 && Window.ChoiceCount > 0)
			{
				Asking = &Window;
			}
		}
		if (Asking == nullptr)
		{
			bAnswering = false;
			if (!Taps(Played, 0) || (Context != nullptr && OpensChoice(*Context)))
			{
				return {};
			}
			// A scene that goes round where it is with no window open may wait for another button: a toy the pad
			// plays with, left by a direction. Each is tried in its turn.
			const bool bWindow = Context != nullptr && Context->Messages.OpenMask != 0;
			if (Idle >= OtherButtonFrames && !bWindow)
			{
				return {OtherButtons[static_cast<std::size_t>((Idle - OtherButtonFrames) / OtherButtonTurn) % OtherButtons.size()]};
			}
			// So may a screen that goes on showing windows by itself and takes no confirm: the bridge's chart, which
			// names the place its cursor is on over and over and is left by the cancel button. The confirm button
			// keeps its every other turn: a long list read page by page comes to nothing new either, and wants it.
			const int32 StaleTurn = (Stale - StaleButtonFrames) / OtherButtonTurn;
			if (Stale >= StaleButtonFrames && StaleTurn % 2 == 0)
			{
				return {OtherButtons[static_cast<std::size_t>(StaleTurn / 2) % OtherButtons.size()]};
			}
			return {Cross};
		}
		// A choice: the answer asked for the first two times its question comes, then its last entry, the one
		// before it and so on round, so that a question asked until it hears another answer does hear it, and a
		// list that ends with its way out is left by it.
		// The question is known by its map and its words: the same one asked in a later stretch is the same question.
		uint64 Question = QuestionDigestStart ^ static_cast<uint64>(Context != nullptr ? Context->RunningMap : 0);
		for (const uint8 Byte : Asking->Message)
		{
			Question = (Question ^ Byte) * QuestionDigestPrime;
		}
		const int32 Times = Answers[Question];
		const int32 Round = Times / AnswersBeforeNext;
		const int32 Wanted = Round == 0 ? Options.Choice % Asking->ChoiceCount : Asking->ChoiceCount - 1 - (Round - 1) % Asking->ChoiceCount;
		if (!Taps(Played, 0))
		{
			return {};
		}
		if (Asking->ChoiceCursor != Wanted)
		{
			return {Down};
		}
		if (!bAnswering)
		{
			bAnswering = true;
			const int32 Given = ++Answers[Question];
			bAsksMore = bAsksMore || Given < AnswersBeforeNext + Asking->ChoiceCount - 1;
		}
		return {Cross};
	}

	void FStorySweepRun::TraceBattle(int32 Fought) const
	{
		const FBattleGameProgram* Battle = Loop != nullptr ? std::as_const(*Loop).Battle() : nullptr;
		if (Battle == nullptr)
		{
			return;
		}
		const FBattleContext& Context = Battle->Runtime().Context();
		// Each slot that takes part: its HP of its most, its status and its mental status.
		std::string Slots;
		for (int32 Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if (Context.InBattle[static_cast<std::size_t>(Slot)])
			{
				const FBattleCombatant& Combatant = Context.Combatants[static_cast<std::size_t>(Slot)];
				Slots += " " + std::to_string(Slot) + ":" + std::to_string(Combatant.Hp) + "/" + std::to_string(Combatant.MaxHp) + "/" + Hex(Combatant.Status) + "/" + Hex(Combatant.Mental);
				// A gear's frame and fuel, where a gear fights: the HP above is its pilot's.
				if (Combatant.GearPart.MaxHp != 0)
				{
					Slots += " gear " + std::to_string(Combatant.GearPart.Hp) + "/" + std::to_string(Combatant.GearPart.MaxHp) + " fuel " + std::to_string(Combatant.GearPart.Fuel);
				}
			}
		}
		std::fprintf(stderr, "%d battle frame %d: turn pc %d performer %d running %d fought %d;%s\n", Played, Fought, int32{Context.Turn.Pc},
			int32{Context.Turn.Performer}, Context.Turn.bRunning ? 1 : 0, Battle->Runtime().IsFought() ? 1 : 0, Slots.c_str());
	}

	FGameState FStorySweepRun::State() const
	{
		FGameState Kept = Session->Game();
		// The disc in the drive, which the game notes in its state only as it saves: a state kept past the disc
		// change with the first disc's word would go on with the first disc's movies.
		const uint16 Disc = static_cast<uint16>(Session->Kernel().Disc);
		std::memcpy(Kept.Block().data() + SavedDiscWord, &Disc, sizeof(Disc));
		if (const FWorldMapProgram* WorldMap = Loop != nullptr ? std::as_const(*Loop).WorldMap() : nullptr)
		{
			KeepShipOnWorldMap(Kept, WorldMap->Runtime().Context());
		}
		return Kept;
	}

	std::optional<FFieldStanding> FStorySweepRun::Standing() const
	{
		const FEventContext* Context = FieldContext();
		return Context != nullptr ? std::optional<FFieldStanding>(LeaderStanding(*Context)) : std::nullopt;
	}

	bool FStorySweepRun::Reaches(const FFieldStanding& Place) const
	{
		const FEventContext* Context = FieldContext();
		if (Context == nullptr)
		{
			return false;
		}
		const std::optional<FFloorUnder> Under = WalkedFloorAt(*Context, Place.Layer, Place.X, Place.Z);
		return Under && Under->Floor == LeaderFloor(*Context) && FWalkedGround(*Context, -1, MetOnWalks()).Reaches(Place.X, Place.Z);
	}

	bool FStorySweepRun::IsHemmedIn() const
	{
		const FEventContext* Context = FieldContext();
		return Context != nullptr && FWalkedGround(*Context, -1, MetOnWalks()).Size() < HemmedInPoints;
	}

	int32 FStorySweepRun::WorldMapEntrance() const
	{
		const FWorldMapProgram* WorldMap = Loop != nullptr ? std::as_const(*Loop).WorldMap() : nullptr;
		return WorldMap != nullptr ? WorldMap->Runtime().Context().Entrance : -1;
	}

	const FEventContext* FStorySweepRun::FieldContext() const
	{
		const FGameLoop* Running = Loop.get();
		const FFieldProgram* Field = Running != nullptr && Running->Program() == EGameProgram::Field ? Running->Field() : nullptr;
		return Field != nullptr ? &Field->Runtime().Context() : nullptr;
	}

	bool FStorySweepRun::Fight(FStorySweepStretch& Stretch, std::optional<FError>& Error)
	{
		// Cross mostly, the directions, the attack buttons, Circle now and then.
		constexpr uint8 Buttons[] = {14, 14, 14, 14, 14, 14, 14, 14, 13, 13, 4, 5, 6, 7, 12, 15, 12, 15};
		const int32 Runs = Loop->ProgramRuns();
		uint16 Held = 0;
		int32 HoldLeft = 0;
		bool bPressed = false;
		for (int32 Fought = 0; Fought < Options.BattleFrames && Loop->ProgramRuns() == Runs; ++Fought)
		{
			if (HoldLeft == 0)
			{
				bPressed = !bPressed;
				Held = bPressed ? static_cast<uint16>(1u << Buttons[Random.Next() % std::size(Buttons)]) : uint16{0};
				HoldLeft = bPressed ? 1 + static_cast<int32>(Random.Next() % 3) : 1 + static_cast<int32>(Random.Next() % 6);
			}
			--HoldLeft;
			if (Options.TraceFrames > 0 && Fought % Options.TraceFrames == 0)
			{
				TraceBattle(Fought);
			}
			Error = Frame(FPadState{Held});
			if (Error)
			{
				return false;
			}
		}
		if (Loop->ProgramRuns() == Runs)
		{
			return false;
		}
		if (Session->Game().ProgramResult == LostResult)
		{
			Stretch.Path += " lost";
		}
		return true;
	}

	void FStorySweepRun::Visit(FStorySweepStretch& Stretch, const std::string& Place)
	{
		if (Place == LastPlace)
		{
			return;
		}
		LastPlace = Place;
		Stretch.Path += Stretch.Path.empty() ? Place : " > " + Place;
		++Arrivals[Place];
	}

	std::string FStorySweepRun::HeldDetail() const
	{
		const FEventContext* Context = FieldContext();
		if (Context == nullptr || Context->Scripts == nullptr)
		{
			return {};
		}
		// The scripts something started (a scene's, a talk's) before the actors' own threads, which run on from their init.
		std::vector<std::string> Scene;
		std::vector<std::string> Own;
		for (std::size_t Actor = 0; Actor < Context->States.size(); ++Actor)
		{
			for (const FEventThread& Thread : Context->States[Actor].Threads)
			{
				if (Thread.IsFree() || Thread.Pc >= Context->Scripts->CodeSize())
				{
					continue;
				}
				const uint8* At = Context->Scripts->Code() + Thread.Pc;
				const bool bOwn = Thread.Script == FEventThread::NoScript;
				std::string Text = std::to_string(Actor) + (bOwn ? "" : "." + std::to_string(Thread.Script)) + " @" + Hex(Thread.Pc) + " " + Hex(At[0]);
				if (At[0] == ExtendedOpcode)
				{
					Text += " " + Hex(At[1]);
				}
				if (!bOwn)
				{
					// Where the actor stands: a walk that never arrives shows as a place that no longer changes.
					const std::array<int32, 3>& Place = Context->States[Actor].Position;
					Text += " (" + std::to_string(Place[0] >> 16) + "," + std::to_string(Place[2] >> 16) + ")";
				}
				(bOwn ? Own : Scene).push_back(std::move(Text));
			}
		}
		Scene.insert(Scene.end(), Own.begin(), Own.end());
		std::string Detail = "lock " + Hex(Context->PlayerLock) + " windows " + Hex(Context->Messages.OpenMask);
		if (!Context->bFieldRunning)
		{
			Detail += " entering";
		}
		else if (Context->bPlayerControlled && Context->PlayerLock == 0)
		{
			Detail += " free";
		}
		// The windows open: whose script opened each and at whom, and what it waits for.
		for (const FFieldMessageWindow& Window : Context->Messages.Windows)
		{
			if (Window.bOpen)
			{
				Detail += "; window " + std::to_string(Window.Owner) + ">" + std::to_string(Window.Speaker) + " flags " + Hex(Window.Flags)
					+ (Window.OpenTicks != 0 ? " opening" : "") + (Window.bChoice ? " choice" : "") + (Window.bTextWaits ? " waits" : "")
					+ (Window.bFocus ? " focus" : "") + (Window.bCloseRequested ? " closing" : "");
			}
		}
		for (std::size_t Number = 0; Number < Scene.size() && Number < MostThreadsNamed; ++Number)
		{
			Detail += "; " + Scene[Number];
		}
		// Whose body the leader's collisions met this tick: what a walk of the leader's that makes no way stands against.
		for (std::size_t Actor = 0; Actor < Context->States.size(); ++Actor)
		{
			if ((Context->States[Actor].Flags1 & MetByLeader) != 0)
			{
				const std::array<int32, 3>& Place = Context->States[Actor].Position;
				Detail += "; leader meets " + std::to_string(Actor) + " (" + std::to_string(Place[0] >> 16) + "," + std::to_string(Place[2] >> 16) + ")";
			}
		}
		return Detail;
	}

	std::string FStorySweepRun::WatchedDetail() const
	{
		const FEventContext* Context = FieldContext();
		const int32 Watched = Context != nullptr && Options.WatchedActor == FStorySweepOptions::WatchedLeader ? int32{Context->LeaderActor} : Options.WatchedActor;
		if (Context == nullptr || Watched < 0 || static_cast<std::size_t>(Watched) >= Context->States.size())
		{
			return {};
		}
		const FEventActorState& Actor = Context->States[static_cast<std::size_t>(Watched)];
		const FSprite* Sprite = Context->Models != nullptr ? Context->Models->ActorSprite(Watched) : nullptr;
		std::string Detail = "; actor " + std::to_string(Watched) + " at " + std::to_string(Actor.Position[0] >> 16) + "," + std::to_string(Actor.Position[1] >> 16) + ","
			+ std::to_string(Actor.Position[2] >> 16) + " flags " + Hex(Actor.Flags0) + " " + Hex(Actor.Flags1) + " layer " + std::to_string(Actor.Layer) + " triangle "
			+ std::to_string(Actor.Layer < Actor.FloorTriangles.size() ? Actor.FloorTriangles[Actor.Layer] : -1) + " attributes " + Hex(Actor.FloorAttributes) + " direction "
			+ Hex(Actor.MoveDirection) + " facing " + Hex(Actor.Facing) + " animation " + std::to_string(Actor.Animation) + "/" + std::to_string(Actor.AnimationRequest) + " divisor "
			+ std::to_string(Actor.SpeedDivisor);
		if (Sprite != nullptr)
		{
			Detail += " sprite speed " + std::to_string(Sprite->Speed) + " velocity " + std::to_string(Sprite->Velocity[0]) + "," + std::to_string(Sprite->Velocity[1]) + ","
				+ std::to_string(Sprite->Velocity[2]) + " flags " + Hex(Sprite->AnimationFlags) + " " + Hex(Sprite->TimeFlags);
		}
		return Detail;
	}

	void FStorySweepRun::Finish(FStorySweepStretch& Stretch, EStorySweepEnd End, std::string Detail)
	{
		Stretch.End = End;
		Stretch.Detail = std::move(Detail);
		Stretch.Unreached = Session->Unreached().TakeNoted();
		const FEventContext* Context = FieldContext();
		// On the world map, the place the game state names: 400h and up.
		const bool bTravels = Loop != nullptr && Loop->Program() == EGameProgram::WorldMap;
		Stretch.Map = Context != nullptr ? Context->RunningMap : bTravels ? int32{Session->Game().Place() & PlaceMapBits} : -1;
		Stretch.Story = Context != nullptr ? Context->Variables.Raw(0) : Session->Game().SavedEventVariable(0);
		Stretch.UnstagedFrames = LongestUnstaged;
		Stretch.UnstagedMap = LongestUnstagedMap;
		Stretch.ShortCutscenes = ShortCutscenes;
		std::set<int32> Watched(TestedVariables(StretchMap).begin(), TestedVariables(StretchMap).end());
		Watched.insert(TestedVariables(Stretch.Map).begin(), TestedVariables(Stretch.Map).end());
		Watched.erase(0);
		for (const int32 Variable : Watched)
		{
			const uint16 Before = StretchVariables[static_cast<std::size_t>(Variable)];
			const uint16 After = Session->Game().SavedEventVariable(Variable);
			if (Before != After)
			{
				char Text[32];
				std::snprintf(Text, sizeof Text, "%s%Xh %u>%u", Stretch.Changed.empty() ? "" : ", ", Variable * 2, unsigned{Before}, unsigned{After});
				Stretch.Changed += Text;
			}
		}
	}

	const std::vector<int32>& FStorySweepRun::TestedVariables(int32 Map) const
	{
		const auto Known = Tested.find(Map);
		if (Known != Tested.end())
		{
			return Known->second;
		}
		std::vector<int32>& Read = Tested[Map];
		for (const uint16 Address : MapReferences(Map).Tested)
		{
			Read.push_back(Address / 2);
		}
		return Read;
	}

	const FStoryReferences& FStorySweepRun::MapReferences(int32 Map) const
	{
		const auto Known = References.find(Map);
		if (Known != References.end())
		{
			return Known->second;
		}
		FStoryReferences& Read = References[Map];
		if (Map < 0 || Map >= FieldMapCount)
		{
			return Read;
		}
		const FPlayedDisc& Disc = Game.Discs[0];
		const auto Archive = LoadFieldArchive(Disc.Iso->Image(), *Disc.Index, Map);
		if (!Archive)
		{
			return Read;
		}
		if (const auto Scripts = FEventScripts::Parse(Archive->Section(EFieldSection::EventScripts)))
		{
			Read = FindStoryReferences(*Scripts);
		}
		return Read;
	}

	FStorySweepStretch FStorySweepRun::Play(const FStorySweepPatience& Patience)
	{
		FStorySweepStretch Stretch;
		LastPlace.clear();
		Arrivals.clear();
		bAnswering = false;
		bAsksMore = false;
		// The floor's attributes as the stretch finds them, and the map they are of.
		const FEventContext* Found = FieldContext();
		const int32 FloorMap = Found != nullptr ? Found->RunningMap : -1;
		StretchMap = FloorMap;
		for (int32 Variable = 0; Variable < FGameState::SavedEventVariableCount; ++Variable)
		{
			StretchVariables[static_cast<std::size_t>(Variable)] = Session->Game().SavedEventVariable(Variable);
		}
		const std::vector<uint32> Floor = Found != nullptr && Found->Walkmesh != nullptr ? Found->Walkmesh->Attributes : std::vector<uint32>();
		LongestUnstaged = 0;
		bCrossed = false;
		LongestUnstagedMap = -1;
		ShortCutscenes = 0;
		const int32 Begun = Played;
		// The actor the stretch's talk or touch is asked of; none: -1.
		const int32 AskedOf = Asked && (Asked->Kind == EStorySweepTrigger::Talk || Asked->Kind == EStorySweepTrigger::Touch) ? Asked->Number : -1;
		if (Asked)
		{
			if (auto Error = Open(Stretch))
			{
				Stretch.Frames = Played - Begun;
				Finish(Stretch, EStorySweepEnd::Stops, Error->Message);
				return Stretch;
			}
		}
		// The frames the player has had the party, and those of them since a window was last open.
		int32 Free = 0;
		int32 Quiet = 0;
		// The player had the party with no window open in the frame before.
		bool bAtRest = false;
		int32 Held = 0;
		// The frames the scripts have kept the party since they last came to an instruction new to the stretch, whatever their windows did.
		int32 Stale = 0;
		// The frames the player has had the party since the scripts last came to an instruction new to the stretch.
		int32 Round = 0;
		// The instructions the scripts' threads have been at in the stretch, each as its actor and offset, and the windows open.
		std::set<uint32> Steps;
		uint16 Windows = 0;
		// When the stretch came to the map it is on with the story's progress as it is: a battle's frames are not the map's.
		int32 PlaceBegun = Played;
		uint16 PlaceStory = Session->Game().SavedEventVariable(0);
		int32 InMenu = 0;
		int32 InWorldMap = 0;
		for (;;)
		{
			Stretch.Frames = Played - Begun;
			if (Stretch.Frames >= Options.StretchFrames || Played - PlaceBegun >= Options.PlaceFrames)
			{
				Finish(Stretch, EStorySweepEnd::Long, HeldDetail());
				break;
			}
			const EGameProgram Program = Loop->Program();
			if (Program == EGameProgram::Battle || Program == EGameProgram::Battling)
			{
				const bool bBattle = Program == EGameProgram::Battle;
				LastPlace.clear();
				Visit(Stretch, bBattle ? "battle " + std::to_string(Session->Kernel().Field.BattleFormation) : "arena");
				std::optional<FError> Error;
				const int32 Before = Played;
				const bool bOver = Fight(Stretch, Error);
				// The fight's frames are not the map's.
				PlaceBegun += Played - Before;
				Stretch.Frames = Played - Begun;
				if (Error)
				{
					Finish(Stretch, EStorySweepEnd::Stops, Error->Message);
					break;
				}
				if (!bOver)
				{
					Finish(Stretch, bBattle ? EStorySweepEnd::BattleRuns : EStorySweepEnd::Arena, {});
					break;
				}
				if (bBattle && Session->Game().ProgramResult == LostResult)
				{
					Finish(Stretch, EStorySweepEnd::BattleLost, {});
					break;
				}
				Free = Quiet = Held = Stale = Round = 0;
				continue;
			}
			const FEventContext* Context = FieldContext();
			const bool bMenu = Menus->Running() != nullptr || Program == EGameProgram::Menu;
			FPadState Pad;
			if (bMenu)
			{
				Visit(Stretch, "menu " + std::to_string(Session->Kernel().MenuMode));
				// A name's entry is left only by finishing it: Start does, keeping the name offered, as a player who takes it.
				const bool bNaming = Session->Kernel().MenuMode == static_cast<uint8>(EMenuMode::EnterName);
				const uint16 Button = bNaming ? Start : InMenu < MenuCancelFrames ? Circle : Cross;
				Pad = Taps(Played, 0) ? FPadState{Button} : FPadState{};
				if (++InMenu > MenuFrames)
				{
					Finish(Stretch, EStorySweepEnd::Menu, {});
					break;
				}
			}
			else
			{
				InMenu = 0;
				Pad = ScenePad(Context, bAtRest, Held, Stale);
				if (Program == EGameProgram::WorldMap)
				{
					const FWorldMapProgram* WorldMap = std::as_const(*Loop).WorldMap();
					const bool bCutscene = WorldMap != nullptr && WorldMap->Runtime().IsCutscene();
					if (bCutscene)
					{
						Visit(Stretch, "cutscene");
					}
					else if (WorldMap != nullptr && IsWorldMapVoyage(WorldMap->Runtime().Context()))
					{
						// The ship on a voyage of its own: the travel is not the player's until it ends.
						Visit(Stretch, "voyage");
						InWorldMap = 0;
					}
					else if (++InWorldMap >= WorldMapFrames)
					{
						Visit(Stretch, "world map");
						Finish(Stretch, EStorySweepEnd::WorldMap, {});
						break;
					}
				}
				else if (Program == EGameProgram::Movie)
				{
					Visit(Stretch, "movie");
				}
				else if (Context != nullptr)
				{
					const std::string Map = std::to_string(Context->RunningMap);
					if (Map != LastPlace || Context->Variables.Raw(0) != PlaceStory)
					{
						// The story moved on: the maps come back to before then were no circle (the second disc's
						// narrator's room, come back to after each of its tales).
						if (Context->Variables.Raw(0) != PlaceStory)
						{
							Arrivals.clear();
						}
						PlaceBegun = Played;
						PlaceStory = Context->Variables.Raw(0);
					}
					if (Map != LastPlace)
					{
						Held = Stale = 0;
						Steps.clear();
					}
					Visit(Stretch, Map);
					if (Arrivals[Map] > MostArrivals)
					{
						Finish(Stretch, EStorySweepEnd::Loops, {});
						break;
					}
				}
			}
			if (Program != EGameProgram::WorldMap)
			{
				InWorldMap = 0;
			}
			if (auto Error = Frame(Pad))
			{
				Stretch.Frames = Played - Begun;
				Finish(Stretch, EStorySweepEnd::Stops, Error->Message);
				break;
			}
			Context = FieldContext();
			if (Options.TraceFrames > 0 && Context != nullptr && Played % Options.TraceFrames == 0)
			{
				const FFieldPresenter& Drawing = Loop->Field()->Drawing();
				std::fprintf(stderr, "%d map %d story %d: cutscene %d shown %d %d %d; %s%s\n", Played, Context->RunningMap, int32{Context->Variables.Raw(0)},
					Drawing.Cutscenes().InCutscene() ? 1 : 0, Drawing.MemberStandIns().ShownIn(0).value_or(-1), Drawing.MemberStandIns().ShownIn(1).value_or(-1),
					Drawing.MemberStandIns().ShownIn(2).value_or(-1), HeldDetail().c_str(), WatchedDetail().c_str());
			}
			// The player has the party as the game itself tells it: the entry over, the pad moving the leader.
			const bool bFree = Context != nullptr && Context->bFieldRunning && Context->bPlayerControlled && Context->PlayerLock == 0
				&& Menus->Running() == nullptr;
			Free = bFree ? Free + 1 : 0;
			// The quiet that ends a stretch waits for the camera's turn: a scene may wait for it before it locks the
			// controls (a clerk's of map 213 at 120, the player's for a second), and a stretch over then left it half played.
			bAtRest = bFree && Context->Messages.OpenMask == 0;
			Quiet = bAtRest && Context->Camera.TurnTicks == 0 ? Quiet + 1 : 0;
			const bool bNewSteps = Context != nullptr && ReachesNewSteps(*Context, Steps);
			Round = bNewSteps ? 0 : Round + (bFree ? 1 : 0);
			if (Quiet >= Patience.QuietFrames || Free >= Patience.SpokenOverFrames || Round >= Patience.RoundFrames)
			{
				Stretch.Frames = Played - Begun;
				Finish(Stretch, EStorySweepEnd::Control, {});
				break;
			}
			// A scene goes on while its scripts reach instructions they have not been at, or its windows change; a
			// movie the field plays has its own length. What waits at the same places for long is held.
			const FFieldProgram* Field = Context != nullptr ? std::as_const(*Loop).Field() : nullptr;
			const bool bGoesOn = Context != nullptr && (bNewSteps || Context->Messages.OpenMask != Windows);
			Windows = Context != nullptr ? Context->Messages.OpenMask : uint16{0};
			const bool bMovie = Field != nullptr && Field->Movie() != nullptr;
			Held = Context == nullptr || bFree || bGoesOn || bMovie ? 0 : Held + 1;
			Stale = Context == nullptr || bFree || bNewSteps || bMovie ? 0 : Stale + 1;
			if (Held >= Patience.HeldFrames)
			{
				Stretch.Frames = Played - Begun;
				Finish(Stretch, EStorySweepEnd::Held, HeldDetail());
				break;
			}
		}
		const FEventContext* Left = FieldContext();
		bFloorChanged = Left != nullptr && Left->RunningMap == FloorMap && Left->Walkmesh != nullptr && Left->Walkmesh->Attributes != Floor;
		// What the actor talked to came to itself: the people of a map walk and wait by scripts of their own all the while.
		bSaidMore = false;
		for (const uint32 Step : Steps)
		{
			const bool bNew = Said.insert(Step).second;
			bSaidMore = bSaidMore || (bNew && static_cast<int32>(Step >> 16) == AskedOf);
		}
		return Stretch;
	}
}
