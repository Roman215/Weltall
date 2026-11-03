#pragma once

#include "Foundation/Result.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Game/GameState.h"
#include "Game/OriginalSave.h"
#include "Menu/MenuGameMenus.h"
#include "Sound/GameSoundSession.h"
#include "Sound/UnheardGameSound.h"
#include "Support/XorShift32.h"
#include "Support/PlayedGame.h"
#include "Support/StorySweepGround.h"
#include "Support/StoryValues.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace Weltall
{
	class FStoryPacks;
	struct FEventContext;
}

namespace Weltall::Testing
{
	/** How long a stretch of play is waited on. */
	struct FStorySweepPatience
	{
		/** The frames the player must have had the party, with no window open, before the scenes count as over. */
		int32 QuietFrames = 0;
		/**
		 * The frames the scripts may keep the party from the player without going on (no thread at an instruction
		 * new to the stretch, no window opened or closed, no movie playing) before the run gives up on them.
		 */
		int32 HeldFrames = 0;
		/**
		 * The frames the player must have had the party for the scenes to count as over though a window is open: a
		 * town's voices speak by themselves, and a sign's words stay up for as long as the party stands by it.
		 */
		int32 SpokenOverFrames = 0;
		/**
		 * The frames the player must have had the party, in all, since the scripts last came to an instruction new to
		 * the stretch, for the scenes to count as over though they never leave the party alone for long: a floor that
		 * hurts whoever stands on it does so again and again.
		 */
		int32 RoundFrames = 0;
	};

	/** A change to one of the story's saved variables before a map is entered. */
	struct FStorySweepVariable
	{
		/** The variable's byte address. */
		uint32 Address = 0;
		/** '=' sets the value, '|' sets its bits, '-' clears them. */
		char Kind = '=';
		uint16 Value = 0;
	};

	struct FStorySweepOptions
	{
		/** A map's own scenes, which may begin a while after its entry: eight seconds' quiet, a minute held. */
		FStorySweepPatience Scenes{240, 1800, 960, 600};
		/** A talk, a touch or a zone, which begins at once: a second and a half's quiet after its last window, a minute held. */
		FStorySweepPatience Triggers{45, 1800, 1200, 300};
		/** The frames of a whole stretch of play. */
		int32 StretchFrames = 36000;
		/** The frames a stretch may stay on one map with the story where it is: five minutes of scenes that lead nowhere. */
		int32 PlaceFrames = 9000;
		/** The frames a battle is fought for. */
		int32 BattleFrames = 12000;
		/** The answer a choice is given, 0 its first entry; one asked again and again is given the next after two of the same. */
		int32 Choice = 0;
		/** The second disc's story told the expanded way, for the port's own documents that tell it. */
		bool bExpanded = false;
		/**
		 * A value's state is the last save from before the story reached it, not the last by then: what a scene that
		 * plays as the story reaches the value finds, where a save made at the value has the scene behind it.
		 */
		bool bSaveBefore = false;
		/**
		 * A talk or a touch the game's own check does not start for the leader beside its actor is started outright:
		 * its script is played, from a state no player may reach.
		 */
		bool bForcesTriggers = true;
		/** Every so many frames of a scene, what the scripts wait at is written to the error stream; 0: never. */
		int32 TraceFrames = 0;
		/** An actor whose place and movement are written with each of those lines; -1: none; WatchedLeader: whoever the pad walks. */
		int32 WatchedActor = -1;
		static constexpr int32 WatchedLeader = -2;
		/**
		 * The party kept in the order of its people from the last to the first, so that every party of two or more is
		 * shown in another order than the story's wherever the drawing shows the player's.
		 */
		bool bReversesOrder = false;
		/**
		 * A hold the drawing takes for play is written to the error stream as it ends when it lasted this many frames
		 * ("HOLD", its map, the story's progress, its frames, the windows opened in it); 0: none is.
		 */
		int32 HoldFramesTold = 0;
		/**
		 * Every character and gear made far stronger than the story asks as a map is entered (StrengthenParty), so
		 * that the battles a random pad fights are won and the play goes on past them.
		 */
		bool bStrongParty = false;
		/**
		 * Elly back in the party in Regurus as a map is entered, in the second slot where she is not in it, the game a
		 * clear game's (ClearFile.h): the final dungeon played with her there, as a clear file plays it.
		 */
		bool bClearGame = false;
		/**
		 * The game a Keep Elly game's (KeepElly.h) as a map is entered, its rule holding Elly where it does, and she put
		 * in the second slot where the rule has her back and she is not in the party: Merkava and on played with her.
		 */
		bool bKeepElly = false;
		/** Fei drawn as Id as a map is entered, the game a New Game+ where it is no clear game: his battles played as Id's. */
		bool bFeiAsId = false;
		/** The saved variables changed in every state before its map is entered: a flag as the story has it where the save does not. */
		std::vector<FStorySweepVariable> Variables;
	};

	/** How a stretch of play ended. */
	enum class EStorySweepEnd : uint8
	{
		/** The player has the party on a field map. */
		Control,
		/** The scripts keep the party from the player and have not gone on for as long as they are waited on. */
		Held,
		/** The game stopped: what it said is the detail. */
		Stops,
		/** The world map, the player travelling. */
		WorldMap,
		/** A menu that neither cancelling nor confirming closes. */
		Menu,
		/** A battle still fought when its frames ran out. */
		BattleRuns,
		BattleLost,
		/** The arena, a bout still fought when its frames ran out. */
		Arena,
		/** The scenes came back to a map they had left several times over. */
		Loops,
		/** The stretch's frames ran out with scenes still playing. */
		Long,
	};

	const char* StorySweepEndName(EStorySweepEnd End);

	/** How the talk or touch a stretch was asked to begin with came to start. */
	enum class EStorySweepOpening : uint8
	{
		/** The stretch was asked for none: a map's own scenes. */
		None,
		/** The field's own check started it for the leader beside the actor: a player's talk or touch. The leader stands in the zone. */
		Answered,
		/** Started outright: the leader could not be stood beside the actor, or the field's check did not answer there. */
		Forced,
		/** Not started: the view never showed the actor, something else had the party by then, or the actor answered no longer. */
		Unanswered,
		/**
		 * Not started: no walk of the leader's gets there from where it stands without meeting something else first
		 * (a body in the way, another actor's touch not met yet), or at all. Only where nothing is started outright.
		 */
		Unreached,
	};

	/** What a line says of an opening after its trigger's name: nothing where the game's own check answered, else " forced", " unanswered", " unreached". */
	const char* StorySweepOpeningSuffix(EStorySweepOpening Opening);

	struct FStorySweepStretch
	{
		EStorySweepOpening Opening = EStorySweepOpening::None;
		EStorySweepEnd End = EStorySweepEnd::Control;
		/** What the game said as it stopped, or what the scripts wait at while they hold the party. */
		std::string Detail;
		/** Of an opening not reached: what the leader's walk would have had to pass to go further (FieldWalkBarriers). */
		std::string Barred;
		/** The maps and programs played through, in order. */
		std::string Path;
		/** The cases no script was thought to reach that the stretch met, which a player's game carries on past. */
		std::vector<std::string> Unreached;
		int32 Frames = 0;
		/** Where the stretch ended: the field map (none in another program) and the story's progress. */
		int32 Map = -1;
		uint16 Story = 0;
		/**
		 * The longest run of frames in which the scripts held the party for no talk and the field's drawing took it
		 * for play (no cutscene by its watch, so the party is drawn in its player's order), and the map of it.
		 */
		int32 UnstagedFrames = 0;
		int32 UnstagedMap = -1;
		/** The cutscenes the drawing's watch began and ended within two seconds on one map: the party's look flipping. */
		int32 ShortCutscenes = 0;
		/**
		 * The story's saved variables the stretch changed that the scripts of the map it began or ended on test (the
		 * progress aside), each as its byte address and its values before and after: "102h 3>4".
		 */
		std::string Changed;
	};

	/** What a player does to a map to make something happen. */
	enum class EStorySweepTrigger : uint8
	{
		/** Talks to an actor: its script 2. */
		Talk,
		/** Touches one: its script 3. */
		Touch,
		/**
		 * Gets onto a floor of the map no walk of the leader's reaches from its own (FWalkedFloor): from the rim of the
		 * ground it walks, by a walk off it, a jump or a running jump toward the floor, or a walk onto a floor that
		 * hands the walker to the layer above. Number is the floor's layer times FloorLayerSpan, plus its first triangle.
		 */
		Floor,
		/** Walks into one of the map's zones, which its scripts test the leader against: a way out, a scene's place. */
		Area,
		/** Travels to a place of the world map and enters it: Number is its record's list times PlaceListSpan, plus the record. */
		Place,
	};

	/** A world map place's trigger number counts its record within this many a list. */
	inline constexpr int32 PlaceListSpan = 256;
	/** A floor's trigger number counts its first triangle within this many a layer. */
	inline constexpr int32 FloorLayerSpan = 0x10000;

	struct FStorySweepTrigger
	{
		EStorySweepTrigger Kind = EStorySweepTrigger::Talk;
		/** The actor, or the zone. */
		int32 Number = 0;

		/** "talk 12", "touch 3", "floor 1.46" (the layer and the first triangle), "area 0", "place 0.4" (the list and the record). */
		std::string Name() const;
	};

	/** How a crossing from one floor onto another sets off: a walk, a jump as it walks, or a jump as it runs. */
	enum class ECrossingMove : uint8
	{
		Walk,
		Jump,
		RunningJump,
	};

	/** A way from one of a map's floors onto another that the leader's walk took when played: done again as it was, it goes there again. */
	struct FFloorCrossing
	{
		FWalkedFloor From;
		FWalkedFloor To;
		/** Where the leader was stood to set off, the point it made for, and where it jumped (a walk's is its aim). */
		FFieldStanding Start;
		int32 AimX = 0;
		int32 AimZ = 0;
		int32 TakeOffX = 0;
		int32 TakeOffZ = 0;
		ECrossingMove Move = ECrossingMove::Walk;
		/** Where the leader came to rest on To. */
		FFieldStanding Landed;
	};

	/**
	 * The game played from a field map at a value of the story's progress, as a player's build plays it: from the
	 * last save of a card folder the story has reached by then (a new game's state before the first), the map entered
	 * by its first entrance, the confirm button tapped while the scripts have the party. Each stretch is played until
	 * the player has the party or it ends otherwise: through the scripts' own map changes, the movies, the world map's
	 * cutscenes and the battles (fought with a random pad).
	 */
	class FStorySweepRun
	{
	public:
		FStorySweepRun(const FPlayedGame& InGame, std::span<const FOriginalSave> InSaves, const FStoryPacks* InStory, const FStorySweepOptions& InOptions)
			: Game(InGame), Saves(InSaves), Story(InStory), Options(InOptions)
		{
		}

		/** Starts the game anew and enters Map with the story's progress at StoryValue. */
		std::optional<FError> Enter(int32 Map, uint16 StoryValue);

		/**
		 * Starts the game anew from a state the game itself made (a save's, or State() of a run) and goes on where it
		 * is: on its field map or on the world map, as a game loaded there goes on.
		 */
		std::optional<FError> Resume(const FGameState& State);

		/** The entrance the party came onto the world map by while it travels there; -1 elsewhere. */
		/** With a trace, the battle's turn and every slot's HP and status, as it is fought. */
		void TraceBattle(int32 Fought) const;
		int32 WorldMapEntrance() const;

		/**
		 * The game's state as it stands, kept so that the game goes on from it where it is: with the disc in the drive
		 * noted as a save notes it, and aboard the ship on the world map the ship's place (KeepShipOnWorldMap).
		 */
		FGameState State() const;

		/** Plays on until the stretch ends. */
		FStorySweepStretch Play(const FStorySweepPatience& Patience);

		/**
		 * The talks and touches the map's actors answer as they stand, in the actors' order, then the map's zones:
		 * none while the player has not the party. On the world map, the places the party's travel reaches and may
		 * enter (WorldMapWays).
		 */
		std::vector<FStorySweepTrigger> Triggers() const;

		/**
		 * Has the next stretch begin with a trigger as a player begins it: the leader stood near the actor (or short of
		 * the zone) where its walk reaches with nothing else met on its way, then, once the view has come over, walked
		 * up to it with the pad, the talk's button pressed or the touch left to the field. Where the walk does not get
		 * there the leader is stood beside the actor, or in the zone; where that starts no talk or touch the script is
		 * started outright, if the options say so. False where the actor no longer answers the trigger. A place of
		 * the world map is come to by starting the world map anew with the party standing in it, as a game saved
		 * there goes on; the confirm button then enters it.
		 */
		bool Ask(const FStorySweepTrigger& Trigger);

		/** The floor of its field map the leader stands on; none off the field. */
		std::optional<FWalkedFloor> Floor() const;

		/** Where the leader stands on its field map; none off the field. */
		std::optional<FFieldStanding> Standing() const;

		/** Whether a walk of the leader's, with nothing met on its way, gets to Place on the floor it stands on. */
		bool Reaches(const FFieldStanding& Place) const;

		/** Whether a walk of the leader's, by the crossings the stretches found, gets back to Floor. */
		bool ReachesFloor(const FWalkedFloor& Floor) const;

		/**
		 * Whether the leader's walk reaches next to nothing from where it stands on its field map: a corner that the
		 * map's people closed around it, where a player would have walked off as they came.
		 */
		bool IsHemmedIn() const;

		/** Whether the last stretch made a crossing from floor to floor itself: a try onto a floor, or a route to a trigger. */
		bool MadeCrossing() const { return bCrossed; }

		/**
		 * Whether the actor the last stretch's talk or touch was asked of came, in it, to instructions of its scripts no
		 * stretch on the map entered came to before, at the story's progress it was entered at: a talk that said
		 * something it had not said.
		 */
		bool SaidMore() const { return bSaidMore; }

		/** Whether the last stretch answered a question some of whose entries no stretch on the map entered has given yet. */
		bool AsksMore() const { return bAsksMore; }

		/**
		 * Whether the last stretch's scripts changed the floor of the map it began and ended on: a triangle's
		 * attributes, by which they open and close ways (a well's lid, a gate). The map loaded anew has them as before.
		 */
		bool FloorChanged() const { return bFloorChanged; }

		/** The map's zones, the nearest to the leader first: none off the field or while the scripts have the party. */
		std::vector<FStorySweepTrigger> ZonesByNearness() const;

		/**
		 * The story's saved variables a field map's scripts test (FStoryReferences::Tested), each by its place among
		 * the saved variables: none for a map the first disc has not.
		 */
		const std::vector<int32>& TestedVariables(int32 Map) const;

		/** What a field map's scripts make of the story (FindStoryReferences): nothing for a map the first disc has not. */
		const FStoryReferences& MapReferences(int32 Map) const;

		/** The save the run started from: its story's progress, -1 for a new game's state; the state's own for one resumed. */
		int32 SaveStory() const { return StartedFrom; }
		/** The party the run started with. */
		std::string Party() const { return StartedParty; }

	private:
		/** The game powered on anew, a save loaded where one is given: its own sound, the menus, the port's documents. */
		std::optional<FError> Begin(const FOriginalSave* Save);
		/** The game's loop entered on field map Place (with its gear bits) or, from 400h up, on the world map; the run's counts reset. */
		std::optional<FError> EnterPlace(uint16 Place);
		std::optional<FError> Frame(FPadState Held);
		/**
		 * The buttons a frame of a scene is played with: the confirm button tapped, a choice's cursor moved to the
		 * answer wanted first; none while the player has the party and no window is open (a tap there talks to
		 * whoever stands by). Idle counts the frames the scripts have gone nowhere: with no window open, the
		 * other buttons are then tapped in turn; Stale those they came to no new instruction in, their windows
		 * opening and closing or not: after a longer while the other buttons are tapped then too.
		 */
		FPadState ScenePad(const FEventContext* Context, bool bFree, int32 Idle, int32 Stale);
		/**
		 * Walks the leader with the pad toward a ground point, as long as the player keeps the party, until Arrived
		 * says so, the walk makes no way or its frames run out; an error where the game stopped meanwhile. Buttons
		 * gives what else is held each frame.
		 */
		std::optional<FError> WalkLeader(double X, double Z, const std::function<bool(const FEventContext&)>& Arrived, bool& bArrived,
			const std::function<uint16(const FEventContext&)>& Buttons = nullptr);
		/** Plays frames until the view shows the actor, as long as the player keeps the party; an error where the game stopped meanwhile. */
		std::optional<FError> AwaitView(int32 Actor, bool& bShown);
		/**
		 * Waits by shut gates (ShutGatesBeside) until the scripts change one's word (bChanged), at most GateWaitFrames
		 * (bWaitedOut: all of them passed in the field, nothing such as a random battle breaking in): held toward the
		 * first, its lines read on. An error where the game stopped meanwhile.
		 */
		std::optional<FError> AwaitGate(const std::set<int32>& Gates, bool& bChanged, bool& bWaitedOut);
		/** Reads on through the windows open while the player keeps the party (ReadOnFrames at most); an error where the game stopped meanwhile. */
		std::optional<FError> ReadOn();
		/**
		 * The asked trigger's start, at a stretch's beginning: how it came to start; an error where the game stopped
		 * meanwhile. One somebody's body bars the way to is asked again a few times, a second apart: people walk about,
		 * and whoever stands in a doorway now has walked on in a moment.
		 */
		std::optional<FError> Open(FStorySweepStretch& Stretch);
		std::optional<FError> OpenOnce(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch);
		std::optional<FError> OpenByActor(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch);
		/** A floor the crossings found lead to from the leader's: where a walk on it sets off from, and the crossings that lead there in turn. */
		struct FReachedFloor
		{
			FWalkedFloor Floor;
			FFieldStanding Standing;
			std::vector<std::size_t> Route;
		};

		/** A way to try onto a floor not reached: from one that is, by the route there, stood at Start and walked toward the aim. */
		struct FFloorProbe
		{
			FReachedFloor From;
			FWalkedFloor Toward;
			FFieldStanding Start;
			int32 AimX = 0;
			int32 AimZ = 0;
			int32 TakeOffX = 0;
			int32 TakeOffZ = 0;
			/** Onto a floor that hands the walker up: walked, not jumped. */
			bool bUpward = false;
			int32 Gap = 0;
		};

		/** The floors the crossings found lead to, the leader's own first, each by the fewest crossings. */
		std::vector<FReachedFloor> ReachedFloors(const FEventContext& Context) const;
		/** The ways to try onto the map's floors not reached, nearest first: for each, the nearest from each floor reached. */
		const std::vector<FFloorProbe>& FloorProbes(const FEventContext& Context) const;
		/** The route to the first floor reached from which Reaches says a trigger's walk gets there; none where the leader's own does, or none does. */
		std::vector<std::size_t> RouteToward(const std::function<bool(const FFieldStanding&)>& Reaches) const;
		/** The route's crossings made in turn as they were found: whether the leader ended on the floor the last leads to. */
		std::optional<FError> FollowRoute(const std::vector<std::size_t>& Steps, bool& bFollowed);
		/** The leader stood at Start and the move played toward the aim until it comes to rest: whether it could be stood there. */
		std::optional<FError> PlayCrossing(const FFieldStanding& Start, int32 AimX, int32 AimZ, int32 TakeOffX, int32 TakeOffZ, ECrossingMove Move, bool& bPlayed);
		/** A floor trigger's start: the ways onto it tried until one gets there. */
		std::optional<FError> OpenFloor(const FStorySweepTrigger& Trigger, FStorySweepStretch& Stretch);
		/** What a walk to a trigger may meet on its way: the touches met before; anything where triggers are started outright. */
		const std::set<int32>* MetOnWalks() const { return Options.bForcesTriggers ? nullptr : &Crossed; }
		/** A battle or a bout: fought with a random pad until it ends, false when its frames run out first. */
		bool Fight(FStorySweepStretch& Stretch, std::optional<FError>& Error);
		void Visit(FStorySweepStretch& Stretch, const std::string& Place);
		const FEventContext* FieldContext() const;
		std::string HeldDetail() const;
		/** The watched actor's place, flags, floor, heading, animation and its sprite's speed and velocity. */
		std::string WatchedDetail() const;
		void Finish(FStorySweepStretch& Stretch, EStorySweepEnd End, std::string Detail);
		/** A frame's count toward the stretch's unstaged holds and short cutscenes. */
		void WatchStaging();

		const FPlayedGame& Game;
		std::span<const FOriginalSave> Saves;
		const FStoryPacks* Story = nullptr;
		const FStorySweepOptions& Options;
		std::unique_ptr<FGameSoundSession> Sound;
		std::unique_ptr<FUnheardGameSound> Unheard;
		std::unique_ptr<FMenuGameMenus> Menus;
		std::unique_ptr<FGameSession> Session;
		std::unique_ptr<FGameLoop> Loop;
		FXorShift32 Random{1};
		int32 StartedFrom = -1;
		std::string StartedParty;
		/** The frames played since the run started. */
		int32 Played = 0;
		/** The trigger the next stretch begins with, and whether the leader could be stood near its actor. */
		std::optional<FStorySweepTrigger> Asked;
		bool bStoodNear = false;
		/**
		 * The actors whose touch the stretches asked for have met on the map entered, at the story's progress it was
		 * entered at: a later walk there may meet them again, where one not met yet bars the way past it.
		 */
		std::set<int32> Crossed;
		int32 CrossedMap = -1;
		uint16 CrossedStory = 0;
		/** The ways from floor to floor the stretches found on that map at that progress. */
		std::vector<FFloorCrossing> Crossings;
		/** The crossings the asked trigger's walk makes first, to the floor it is reached from. */
		std::vector<std::size_t> AskedRoute;
		/** The probes last found, and what they were found for: the map, the progress, the leader's floor, the crossings and touches met. */
		struct FKeptProbes
		{
			int32 Map = -1;
			uint16 Story = 0;
			std::optional<FWalkedFloor> Floor;
			std::size_t Crossings = 0;
			std::size_t Met = 0;
			std::vector<FFloorProbe> Probes;
		};
		mutable std::optional<FKeptProbes> KeptProbes;
		bool bCrossed = false;
		/** The instructions the scripts' threads have been at in the stretches on that map at that progress, each as its actor and offset. */
		std::set<uint32> Said;
		bool bSaidMore = false;
		/** What each map's scripts make of the story, and the variables they test, as read so far. */
		mutable std::map<int32, FStoryReferences> References;
		mutable std::map<int32, std::vector<int32>> Tested;
		/** The gates (each a map and an attribute record) that stayed shut through a whole wait by them. */
		std::set<std::pair<int32, int32>> StillGates;
		/** The story's saved variables as the stretch under way began, and the field map it began on. */
		std::array<uint16, FGameState::SavedEventVariableCount> StretchVariables{};
		int32 StretchMap = -1;

		/**
		 * How often each question (by its map and a digest of its message's bytes) was answered on the map entered, at
		 * the story's progress it was entered at, and whether the one open now has been; whether the last stretch
		 * answered one that has entries not given yet.
		 */
		std::map<uint64, int32> Answers;
		bool bAnswering = false;
		bool bAsksMore = false;
		bool bFloorChanged = false;
		/** The last place a stretch visited, and how often each field map was arrived at. */
		std::string LastPlace;
		std::map<std::string, int32> Arrivals;
		/** The frames of the hold under way that the drawing takes for play, and of the cutscene under way; the stretch's counts. */
		int32 Unstaged = 0;
		int32 CutsceneFrames = 0;
		int32 CutsceneMap = -1;
		int32 LongestUnstaged = 0;
		int32 LongestUnstagedMap = -1;
		int32 ShortCutscenes = 0;
		/** Of the hold under way: its map, the story's progress and the windows opened in it. */
		int32 UnstagedMap = -1;
		int32 UnstagedStory = 0;
		int32 UnstagedWindows = 0;
	};
}
