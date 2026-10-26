#include "Game/GameLoop.h"

#include "Game/BattleGameProgram.h"
#include "Game/BattlingGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameSession.h"
#include "Game/MovieProgram.h"
#include "Game/WorldMapProgram.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** L1, R1, Start and Select together: the kernel's start waits for them to be let go. */
		constexpr uint16 SoftResetButtons = 0x090C;

		/** A program the port does not run, named with what the kernel's globals and the game state hand it. */
		FError NotRunYet(const FGameSession& Session, EGameProgram Program)
		{
			const FKernelState& Kernel = Session.Kernel();
			std::string What = std::string(GameProgramName(Program)) + " is not run yet";
			switch (Program)
			{
			case EGameProgram::Menu:
				What += ": mode " + std::to_string(Kernel.MenuMode) + " on its own";
				break;
			case EGameProgram::DebugMenu:
				What += ": the last program named none to follow it";
				break;
			default:
				break;
			}
			return MakeError(What);
		}
	}

	FGameLoop::~FGameLoop() = default;

	TResult<std::unique_ptr<FGameLoop>> FGameLoop::PowerOn(FGameSession& Session, const FPsxExecutable& Executable)
	{
		TResult<FBootNotice> Notice = FBootNotice::Load(Executable, Session.Image(), Session.Index());
		if (!Notice)
		{
			return Notice.Error();
		}
		std::unique_ptr<FGameLoop> Loop(new FGameLoop(Session));
		Loop->BootNotice.emplace(std::move(*Notice));
		return Loop;
	}

	TResult<std::unique_ptr<FGameLoop>> FGameLoop::EnterField(FGameSession& Session, uint16 Number)
	{
		std::unique_ptr<FGameLoop> Loop(new FGameLoop(Session));
		Session.Game().SetPlace(Number);
		Session.Kernel().NextProgram = EGameProgram::Field;
		if (auto Error = Loop->RunNextProgram())
		{
			return *Error;
		}
		return Loop;
	}

	TResult<std::unique_ptr<FGameLoop>> FGameLoop::EnterWorldMap(FGameSession& Session)
	{
		std::unique_ptr<FGameLoop> Loop(new FGameLoop(Session));
		Session.Kernel().NextProgram = EGameProgram::WorldMap;
		if (auto Error = Loop->RunNextProgram())
		{
			return *Error;
		}
		return Loop;
	}

	int32 FGameLoop::VblanksPerFrame() const
	{
		return LoopStage == EGameLoopStage::Program ? RunningEntry->VblanksPerFrame() : 1;
	}

	std::optional<FError> FGameLoop::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (static_cast<int32>(HeldPerVblank.size()) != VblanksPerFrame())
		{
			return MakeError("the game's frame lasts " + std::to_string(VblanksPerFrame()) + " vertical blanks, not " + std::to_string(HeldPerVblank.size()));
		}
		switch (LoopStage)
		{
		case EGameLoopStage::Boot:
			return Boot(HeldPerVblank[0]);
		case EGameLoopStage::Program:
		{
			if (auto Error = RunningEntry->Frame(HeldPerVblank))
			{
				return Stop(std::move(*Error));
			}
			switch (RunningEntry->State())
			{
			case EGameProgramState::Running:
				return std::nullopt;
			case EGameProgramState::SoftReset:
				LoopStage = EGameLoopStage::SoftReset;
				return std::nullopt;
			case EGameProgramState::Ended:
				return RunNextProgram();
			}
			return std::nullopt;
		}
		default:
			return std::nullopt;
		}
	}

	std::optional<FError> FGameLoop::Boot(FPadState Held)
	{
		Session.Vblank(Held);
		// The kernel's start lets go of the soft reset's buttons still held before its notice.
		if (!bResetButtonsReleased)
		{
			bResetButtonsReleased = Session.Pad().Held() != SoftResetButtons;
			return std::nullopt;
		}
		BootNotice->Tick(Session.Pad().Held());
		if (!BootNotice->IsOver())
		{
			return std::nullopt;
		}
		BootNotice.reset();
		Session.Kernel().NextProgram = EGameProgram::Movie;
		return RunNextProgram();
	}

	std::optional<FError> FGameLoop::RunNextProgram()
	{
		FKernelState& Kernel = Session.Kernel();
		RunningEntry.reset();
		RunningProgram = Kernel.NextProgram;
		LoopStage = EGameLoopStage::Program;
		++Runs;
		// Every program starts with the pad's queue emptied and the debug menu to follow unless it names another.
		Session.Pad().Reset();
		Kernel.NextProgram = EGameProgram::DebugMenu;
		switch (RunningProgram)
		{
		case EGameProgram::Movie:
		{
			TResult<std::unique_ptr<FMovieProgram>> Movie = FMovieProgram::Start(Session);
			if (!Movie)
			{
				return Stop(Movie.Error());
			}
			RunningEntry = std::move(*Movie);
			break;
		}
		case EGameProgram::Field:
		{
			TResult<std::unique_ptr<FFieldProgram>> Field = FFieldProgram::Start(Session);
			if (!Field)
			{
				return Stop(Field.Error());
			}
			RunningEntry = std::move(*Field);
			break;
		}
		case EGameProgram::WorldMap:
		{
			TResult<std::unique_ptr<FWorldMapProgram>> WorldMap = FWorldMapProgram::Start(Session);
			if (!WorldMap)
			{
				return Stop(WorldMap.Error());
			}
			RunningEntry = std::move(*WorldMap);
			break;
		}
		case EGameProgram::Battle:
		{
			TResult<std::unique_ptr<FBattleGameProgram>> Battle = FBattleGameProgram::Start(Session);
			if (!Battle)
			{
				return Stop(Battle.Error());
			}
			RunningEntry = std::move(*Battle);
			break;
		}
		case EGameProgram::Battling:
		{
			TResult<std::unique_ptr<FBattlingGameProgram>> Battling = FBattlingGameProgram::Start(Session);
			if (!Battling)
			{
				return Stop(Battling.Error());
			}
			RunningEntry = std::move(*Battling);
			break;
		}
		default:
			return Stop(NotRunYet(Session, RunningProgram));
		}
		// A program that has nothing to do ends as it starts.
		if (RunningEntry->State() == EGameProgramState::Ended)
		{
			return RunNextProgram();
		}
		return std::nullopt;
	}

	std::optional<FError> FGameLoop::Stop(FError Error)
	{
		LoopStage = EGameLoopStage::Stopped;
		Stopped = Error;
		return Error;
	}

	const FMovieProgram* FGameLoop::Movie() const
	{
		return RunningProgram == EGameProgram::Movie ? static_cast<const FMovieProgram*>(RunningEntry.get()) : nullptr;
	}

	FFieldProgram* FGameLoop::Field()
	{
		return RunningProgram == EGameProgram::Field ? static_cast<FFieldProgram*>(RunningEntry.get()) : nullptr;
	}

	const FFieldProgram* FGameLoop::Field() const
	{
		return RunningProgram == EGameProgram::Field ? static_cast<const FFieldProgram*>(RunningEntry.get()) : nullptr;
	}

	FWorldMapProgram* FGameLoop::WorldMap()
	{
		return RunningProgram == EGameProgram::WorldMap ? static_cast<FWorldMapProgram*>(RunningEntry.get()) : nullptr;
	}

	const FWorldMapProgram* FGameLoop::WorldMap() const
	{
		return RunningProgram == EGameProgram::WorldMap ? static_cast<const FWorldMapProgram*>(RunningEntry.get()) : nullptr;
	}

	const FBattlingGameProgram* FGameLoop::Battling() const
	{
		return RunningProgram == EGameProgram::Battling ? static_cast<const FBattlingGameProgram*>(RunningEntry.get()) : nullptr;
	}

	const FBattleGameProgram* FGameLoop::Battle() const
	{
		return RunningProgram == EGameProgram::Battle ? static_cast<const FBattleGameProgram*>(RunningEntry.get()) : nullptr;
	}
}
