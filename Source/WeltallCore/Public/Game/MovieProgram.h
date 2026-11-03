#pragma once

#include "Game/GameProgram.h"
#include "Media/MoviePlayer.h"

#include <memory>

namespace Weltall
{
	class FGameSession;

	/**
	 * The Movie program: the movie the kernel's globals ask for, played to its end frame (233 unless
	 * another is asked for), skippable with Cross or Start (Circle with the Italian translation's Japanese commands'
	 * program) unless the request forbids it, a vertical blank
	 * a frame; then the program the request names. A movie past the disc's is not played, and one whose
	 * data is the disc's stand-in hands over to the debug menu.
	 */
	class FMovieProgram final : public IGameProgram
	{
	public:
		/** The program started as the kernel runs it. The session must outlive it. */
		WELTALLCORE_API static TResult<std::unique_ptr<FMovieProgram>> Start(FGameSession& Session);

		int32 VblanksPerFrame() const override { return 1; }
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override;
		EGameProgramState State() const override { return ProgramState; }

		/** The movie as it plays; null when none is played. */
		const FMoviePlayer* Player() const { return MoviePlayer.get(); }

	private:
		explicit FMovieProgram(FGameSession& InSession) : Session(InSession) {}

		void End();

		FGameSession& Session;
		std::unique_ptr<FMoviePlayer> MoviePlayer;
		EGameProgramState ProgramState = EGameProgramState::Running;
	};
}
