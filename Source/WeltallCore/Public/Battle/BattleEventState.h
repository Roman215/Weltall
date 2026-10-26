#pragma once

#include "Battle/BattleEventOptions.h"
#include "Foundation/Types.h"
#include "Text/MesWindow.h"
#include "Text/MessageTable.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace Weltall
{
	struct FReplacedMessages;

	/**
	 * What an event's instruction hands the show, beside the calls the rest of the battle makes (a
	 * member's boarding, the markers): the instruction's operands as the machine resolved them, the
	 * combatants as battle slots.
	 */
	enum class EBattleEventCue : uint8
	{
		/** The full-screen quad goes to a colour over twice Steps steps: Steps, Mode (the blend), R, G, B. */
		Fade,
		/** The camera's shake goes to an amplitude over twice Steps steps: X, Y, Z, Steps. */
		Shake,
		/** A combatant plays an animation sequence aimed at targets: Slot, Targets (a mask), Sequence. */
		Perform,
		/** The same, its end reported back (FinishBattleEventPerform). */
		PerformSignalled,
		/** A signalled sequence has ended: the show finishes its loads (the event waits while it is blocking). */
		FinishPerform,
		/** One of the event's objects: LoadObject Object, Entry (of the event's object file); ObjectAnim Object, Anim; FreeObject Object. */
		LoadObject,
		ObjectAnim,
		FreeObject,
		/** The camera to its fixed mode, locked, as an object's load leaves it. */
		LockCamera,
		/** The song playing stops (the kernel's music). */
		StopSong,
		/** The event's music: StartMusic File, Volume; StopMusic; FreeMusic; MusicVolume Volume, Time (of the battle's music handle, the event's music once it plays). */
		StartMusic,
		StopMusic,
		FreeMusic,
		MusicVolume,
		/** A sound effect: PlaySound Id, Volume, Pan, Bank (0 the event's own, 1 the battle's); SoundVolume Id, Volume, Bank. */
		PlaySound,
		SoundVolume,
		/** A combatant's sprite: SetAnim Slot, Anim (its end reported by FinishBattleEventMotion); RestoreAnim Slot; StopMotion Slot; StopTurn Slot. */
		SetAnim,
		RestoreAnim,
		StopMotion,
		StopTurn,
		/** A combatant walks or jumps to a point: Slot, X, Y, Z (its arrival reported by FinishBattleEventMotion). */
		Walk,
		Jump,
		/** A combatant faces a target and plays an animation: Slot, Target, Anim (its end reported by FinishBattleEventMotion). */
		Attack,
		/** A combatant plays an effect: Slot, Target, Effect (the event waits while the show is blocking). */
		SpecialAttack,
		/** The show finishes its reads and loads (the event waits while it is blocking). */
		FinishLoads,
		/** A voice of a set plays: Set, Number (the event waits while the show is blocking). */
		PlayVoice,
		/** A member's gear cockpit and command costs show, or go: Slot. */
		OpenCommandHud,
		CloseCommandHud,
		/** The sequences under way will not report their ends. */
		CancelPerformSignals,
		/** The message window opens: X, Y, Width, Height, Grows (1 growing, 0 at its size). */
		OpenWindow,
		/** A portrait beside the message: Portrait, Style, X, Y, Width (the window's). */
		ShowPortrait,
		/** The message window closes. */
		CloseWindow,
		/** The event's own sound effects stop and are released. */
		ReleaseSounds,
	};

	struct FBattleEventCue
	{
		EBattleEventCue Cue = EBattleEventCue::Fade;
		std::array<uint16, 5> Args{};
	};

	/**
	 * One of the event's sixteen records: an actor's threads, and the object and the combatant slot of
	 * the same number (the event names objects and sequences' performers by record).
	 */
	struct FBattleEventRecord
	{
		static constexpr int32 ThreadCount = 8;
		static constexpr uint8 Free = 0xFF;

		/** Each thread slot's pc (FFFFh free), priority (FFh free) and script number (FFh none). */
		std::array<uint16, ThreadCount> Pcs{};
		std::array<uint8, ThreadCount> Priorities{};
		std::array<uint8, ThreadCount> Scripts{};
		/** The slot running, and its script. */
		uint8 Running = 0;
		uint8 RunningScript = 0;
		/** SetOrder's operand, kept. */
		uint8 OrderArgument = 0;
		/** The script this actor last started (FFh none): what a waited start waits on. */
		uint8 StartedScript = 0;
		/** The actor that script was started on (the fix of FBattleEventOptions compares it too). */
		uint8 StartedActor = 0;
		/** The actor's portrait for its messages (FFh none). */
		uint8 Portrait = 0;
		/** Wait's countdown, and whether a Wait runs. */
		int16 WaitCount = 0;
		bool bWaiting = false;
		/** The combatant slot's signalled sequence: 0 none, 2 playing, 1 ended. */
		uint8 PerformState = 0;
		/** The object is loaded. */
		bool bObjectLoaded = false;
	};

	/** How a battle event's scripts measure their message windows: as the game's code does, or as Perfect Works Build's does. */
	enum class EBattleMessageLayout : uint8
	{
		Original,
		/**
		 * The mod's change to the battle events' code, which its scripts (with its translation) are written for: a
		 * window's width in the font's 4-pixel columns rather than 12-pixel letters, as many lines as the script asks
		 * (the game's window stops at 4) but 4 beside a portrait, and the "more" arrow 2 pixels left and 1 higher.
		 */
		PerfectWorks,
		/**
		 * The French translation's change, for its variable-width text: a window's width in 4-pixel columns, its lines
		 * as asked up to 4 without a portrait and 4 with one (the window keeps the count), and the "more" arrow 4 pixels
		 * past the text's last pixel, 1 higher, drawn as a quad of its own pointing down.
		 */
		French,
	};

	/**
	 * The next message's window: its place (7FFFh centred or the style's band), its width in 12-pixel units (Perfect
	 * Works' in 4-pixel ones), its lines and its style.
	 */
	struct FBattleEventWindow
	{
		static constexpr uint16 Automatic = 0x7FFF;

		uint16 X = Automatic;
		uint16 Y = Automatic;
		uint16 Width = 16;
		uint16 Lines = 8;
		uint16 Style = 0x1F0;

		bool operator==(const FBattleEventWindow&) const = default;
	};

	/** The movie an event asks for after the battle. */
	struct FBattleMovieRequest
	{
		/** The movie's kind, with bit 80h: it plays to EndFrame. */
		uint8 Type = 0;
		uint8 Number = 0;
		uint8 NoSkip = 0;
		uint16 EndFrame = 0;
	};

	/** The battle after this one, as an event chains it. */
	struct FBattleChain
	{
		/** The formation + 1. */
		uint8 Formation = 0;
		uint8 Kind = 0;
	};

	/** What an event asks of the battle and of the programs after it. */
	struct FBattleEventRequests
	{
		/** The event runs once more after the battle (Finish). */
		bool bRunsAfterBattle = false;
		/** No victory pose and no result screen. */
		bool bNoResults = false;
		/** The event ended the battle. */
		bool bEndedBattle = false;
		/** A defeat ends the battle as the story goes on. */
		bool bDefeatScripted = false;
		std::optional<FBattleChain> Chain;
		std::optional<FBattleMovieRequest> Movie;
		/** The field starts afresh at the map the event set in the game state. */
		bool bFieldStartsFresh = false;
		/** The field's return flag (1: the next map comes in with its own fade), when set. */
		std::optional<uint8> FieldReturn;
	};

	/** Which of the event's routines runs, frame by frame. */
	enum class EBattleEventRoutine : uint8
	{
		None,
		/** The event's load: a frame, then its work. */
		Load,
		/** The scripts' run at a hook: sweeps of the actors until one returns. */
		Run,
		/** The end: the event's music and sounds released. */
		Shutdown,
	};

	/** Where a routine of the event stopped for the next frame, and what the instruction under way keeps across its frames. */
	struct FBattleEventRun
	{
		EBattleEventRoutine Routine = EBattleEventRoutine::None;
		uint8 Pc = 0;
		/** The place in the order the sweep has reached, the actor and its instructions left. */
		uint8 Sweep = 0;
		uint8 Actor = 0;
		uint8 Budget = 0;
		/** The instruction under way: where it resumes (0 from its start) and frames it still takes. */
		uint8 Step = 0;
		uint16 Frames = 0;
		/** The message being opened: its window's place and width, its style and portrait. */
		uint16 MessageX = 0;
		uint16 MessageY = 0;
		uint16 BoxWidth = 0;
		uint16 Style = 0;
		bool bPortrait = false;
	};

	/**
	 * A battle event's state: the machine's sixteen records and order, the operands, the message window
	 * with what the HUD shows of it, the motions and sequences it waits on, its music, and what it asks
	 * of the battle. Its variables are the context's EventVariables, which the enemies' scripts write too.
	 */
	struct FBattleEventWork
	{
		static constexpr int32 RecordCount = 16;
		static constexpr int32 OperandCount = 8;

		/** The unpacked script and its messages, which must outlive the battle, and which of the messages are the player's. */
		std::span<const uint8> Script;
		FMessageTable Messages;
		const FReplacedMessages* Replaced = nullptr;
		EBattleMessageLayout Layout = EBattleMessageLayout::Original;
		/** The script's actors, and where its code starts in it. */
		uint32 ActorCount = 0;
		std::size_t CodeOffset = 0;

		std::array<FBattleEventRecord, RecordCount> Records{};
		/** The operands the last instruction fetched. */
		std::array<uint16, OperandCount> Operands{};
		/** The order the actors run in (FFh past them). */
		std::array<uint8, RecordCount> Order{};
		FBattleEventWindow Window;
		/** The event has finished: its runs return at once. */
		bool bDone = false;
		/** Return's countdown: the run returns at the end of the sweep that brings it to 1. */
		uint8 ReturnCountdown = 0;
		bool bMessageOpen = false;
		/** Each combatant slot's scripted motion under way (the show's report of its end clears it), and whether the instruction waiting on it began it. */
		std::array<bool, RecordCount> Moving{};
		std::array<bool, RecordCount> MotionStarted{};
		bool bMusicLoaded = false;
		uint16 MusicFile = 0;
		uint8 MusicVolume = 0;
		bool bSoundsRegistered = false;

		/** The message's text window, kept from one message to the next as the game keeps it; the HUD types it while a message shows (TickBattleEventMessage). */
		std::optional<FMesWindow> Text;
		/** What the HUD shows: the portrait, the message, the "more" arrow (its frame 0-4 and place). */
		bool bPortraitShown = false;
		bool bMessageShown = false;
		bool bArrowShown = false;
		int32 ArrowFrame = 0;
		int16 ArrowX = 0;
		int16 ArrowY = 0;
		/** Where the message's text starts on the screen. */
		uint32 TextX = 0;
		uint32 TextY = 0;
		/** The message waits for the confirm button: the input keeps the frame's event for it. */
		bool bKeyWait = false;
		/** The Wait instructions' countdowns run (with the message input). */
		bool bWaitCounters = false;

		FBattleEventRequests Requests;
		FBattleEventRun Run;
	};
}
