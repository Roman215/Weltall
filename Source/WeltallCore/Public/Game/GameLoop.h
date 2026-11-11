#pragma once

#include "Foundation/Result.h"
#include "Game/BattleSpeed.h"
#include "Game/BootNotice.h"
#include "Game/GameProgram.h"
#include "Input/PadState.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FBattleGameProgram;
	class FBattlingGameProgram;
	class FFieldProgram;
	class FGameSession;
	class FMovieProgram;
	class FWorldMapProgram;
	class FPsxExecutable;

	/** Where the game's loop is. */
	enum class EGameLoopStage : uint8
	{
		/** The kernel's start: it waits for the soft reset's buttons to be let go, then shows its notice. */
		Boot,
		/** A program runs. */
		Program,
		/** The pad restarted the console: a new session starts it again from power-on. */
		SoftReset,
		/** Something the port does not run yet stopped the game (StopReason says what); the last program stays as it was. */
		Stopped,
	};

	/**
	 * The game as its kernel runs it: the start from power-on (the publisher's notice), then its programs
	 * one after another, each run frame by frame until it ends and the kernel's rules pick the next from
	 * what it leaves in the kernel's globals: the opening movie, the field and its maps, the programs the
	 * field hands over to. A program the port does not run yet is reached as the original reaches it and
	 * stops the loop with an error naming it and what it was handed.
	 */
	class FGameLoop
	{
	public:
		/**
		 * The console powered on with the session: the kernel's start, its notice (from the executable),
		 * then the Movie program with the opening movie. The session must outlive the loop.
		 */
		WELTALLCORE_API static TResult<std::unique_ptr<FGameLoop>> PowerOn(FGameSession& Session, const FPsxExecutable& Executable);

		/** A new game entering field map Number at once (bits 14 and 15: in gears): the Field program started with the game state's place Number. */
		WELTALLCORE_API static TResult<std::unique_ptr<FGameLoop>> EnterField(FGameSession& Session, uint16 Number);

		/** A game going on on the world map at once: the world map program started where the game state says the party travels. */
		WELTALLCORE_API static TResult<std::unique_ptr<FGameLoop>> EnterWorldMap(FGameSession& Session);

		/** A game going on at once with the program the kernel's globals name next, as they were given to the session. */
		WELTALLCORE_API static TResult<std::unique_ptr<FGameLoop>> GoOn(FGameSession& Session);

		WELTALLCORE_API ~FGameLoop();
		FGameLoop(const FGameLoop&) = delete;
		FGameLoop& operator=(const FGameLoop&) = delete;

		/** The vertical blanks the next frame lasts. */
		WELTALLCORE_API int32 VblanksPerFrame() const;

		/**
		 * The next frame, after its vertical blanks with the buttons held at each: of the kernel's start or
		 * of the program running, and when that program ends, the next one started. An error once, when
		 * the loop stops at something the port does not run yet.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank);

		EGameLoopStage Stage() const { return LoopStage; }
		/** The program running, or the last one that ran. */
		EGameProgram Program() const { return RunningProgram; }
		/** How many programs the kernel has started: a new count is a new program, even one of the same kind. */
		int32 ProgramRuns() const { return Runs; }
		EBattleSpeed ExplorationSpeed() const { return ExplorationPace; }
		void ResetExplorationSpeed() { ExplorationPace = EBattleSpeed::Normal; }
		/** Why the loop stopped, when it has. */
		const std::optional<FError>& StopReason() const { return Stopped; }

		/** The kernel's notice while it boots, null after. */
		const FBootNotice* Notice() const { return BootNotice ? &*BootNotice : nullptr; }
		/** The Movie program while it runs, null otherwise. */
		WELTALLCORE_API const FMovieProgram* Movie() const;
		/** The Field program while it runs (or stopped the loop), null otherwise. */
		WELTALLCORE_API FFieldProgram* Field();
		WELTALLCORE_API const FFieldProgram* Field() const;
		/** The world map program while it runs (or stopped the loop), null otherwise. */
		WELTALLCORE_API FWorldMapProgram* WorldMap();
		WELTALLCORE_API const FWorldMapProgram* WorldMap() const;
		/** The Battle program while it runs (or stopped the loop), null otherwise. */
		WELTALLCORE_API const FBattleGameProgram* Battle() const;
		/** The Battling program while it runs (or stopped the loop), null otherwise. */
		WELTALLCORE_API const FBattlingGameProgram* Battling() const;

	private:
		explicit FGameLoop(FGameSession& InSession) : Session(InSession) {}

		std::optional<FError> Boot(FPadState Held);
		/** The kernel runs the next program. */
		std::optional<FError> RunNextProgram();
		std::optional<FError> Stop(FError Error);
		std::span<const FPadState> ReadExplorationSpeed(std::span<const FPadState> HeldPerVblank);

		FGameSession& Session;
		EGameLoopStage LoopStage = EGameLoopStage::Boot;
		std::optional<FBootNotice> BootNotice;
		/** The kernel's start has seen the soft reset's buttons let go. */
		bool bResetButtonsReleased = false;
		EGameProgram RunningProgram = EGameProgram::DebugMenu;
		std::unique_ptr<IGameProgram> RunningEntry;
		int32 Runs = 0;
		EBattleSpeed ExplorationPace = EBattleSpeed::Normal;
		uint16 HeldBefore = 0xFFFF;
		std::vector<FPadState> FilteredInput;
		std::optional<FError> Stopped;
	};
}
