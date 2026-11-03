#include "Game/SaveState.h"

#include "Field/FieldFrameVariables.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Game/WorldMapProgram.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** Where the menu's button would open the Normal Menu, with nothing on screen and the leader on its floor. */
		bool IsFieldQuiet(const FFieldProgram& Field)
		{
			const FFieldRuntime& Runtime = Field.Runtime();
			const FEventContext& Context = Runtime.Context();
			const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
			return Field.TicksField() && Runtime.IsAtLoopTop() && Context.bFieldRunning && Context.bPlayerControlled && !Context.bMenuLocked
				&& Context.MenuRequest == FEventContext::NoMenu && Context.Messages.OpenMask == 0 && Leader != nullptr
				&& (Leader->Flags0 & EventActorFlags0::InTheAir) == 0;
		}

		/** What the field leaves the game state and the kernel as it leaves for a battle, made on copies: the field plays on. */
		TResult<std::optional<FSaveState>> CaptureField(FFieldProgram& Field, FGameSession& Session)
		{
			FFieldRuntime& Runtime = Field.Runtime();
			const FEventContext& Context = Runtime.Context();
			FSaveState State;
			State.Game = Session.Game();
			FEventVariables Variables = Context.Variables;
			SaveFieldVariables(Context, *Context.StateOf(Context.LeaderActor), Variables, State.Game);

			FFieldSnapshot Snapshot;
			if (auto Error = Runtime.SaveForReturn(Snapshot))
			{
				return *Error;
			}
			FKernelState& Kernel = State.Kernel;
			Kernel = Session.Kernel();
			Kernel.Field = Runtime.KernelState();
			Kernel.Field.Saved = std::move(Snapshot);
			Kernel.LightMatrix = Runtime.LightLeft();
			Kernel.NextProgram = EGameProgram::Field;
			Kernel.bFieldEntered = true;
			Kernel.FieldTrips = 1;
			Kernel.ArenaTrips = 0;
			Kernel.bEventFadeIn = false;
			Kernel.WorldMapKeep = nullptr;
			State.Random = Session.Random();
			State.Clock = Session.Clock();
			return std::optional<FSaveState>(std::move(State));
		}

		/** What the world map's menu leaves the game state for a save made in it, made on a copy; the kernel as its leave sets it. */
		std::optional<FSaveState> CaptureWorldMap(const FWorldMapProgram& WorldMap, FGameSession& Session)
		{
			const FWorldMapRuntime& Runtime = WorldMap.Runtime();
			FSaveState State;
			State.Game = Session.Game();
			Runtime.KeepPlaceAsMenuDoes(State.Game);
			FKernelState& Kernel = State.Kernel;
			Kernel = Session.Kernel();
			Kernel.NextProgram = EGameProgram::WorldMap;
			Kernel.Field.bGearsLocked = Runtime.Context().GearLock != 0;
			// Read only as the field comes back from a battle or the arena, which no start of the world map's is.
			Kernel.Field.Saved.reset();
			Kernel.FieldTrips = 0;
			Kernel.ArenaTrips = 0;
			Kernel.WorldMapKeep = nullptr;
			State.Random = Session.Random();
			State.Clock = Session.Clock();
			return State;
		}
	}

	TResult<std::optional<FSaveState>> CaptureSaveState(FGameLoop& Loop, FGameSession& Session)
	{
		if (Loop.Stage() != EGameLoopStage::Program)
		{
			return std::optional<FSaveState>();
		}
		if (FFieldProgram* Field = Loop.Field(); Field != nullptr && IsFieldQuiet(*Field))
		{
			return CaptureField(*Field, Session);
		}
		if (const FWorldMapProgram* WorldMap = Loop.WorldMap(); WorldMap != nullptr && WorldMap->TicksMap() && WorldMap->Runtime().CouldOpenMenu())
		{
			return std::optional<FSaveState>(CaptureWorldMap(*WorldMap, Session));
		}
		return std::optional<FSaveState>();
	}

	TResult<std::unique_ptr<FGameLoop>> LoadSaveState(FGameSession& Session, const FSaveState& State)
	{
		if (!Session.HasDisc(State.Kernel.Disc))
		{
			return MakeError("the state was saved on disc " + std::to_string(State.Kernel.Disc + 1) + ", which the game was not given");
		}
		Session.Game() = State.Game;
		Session.Kernel() = State.Kernel;
		Session.Random() = State.Random;
		Session.Clock() = State.Clock;
		Session.ChapterCard().Know(Session.Chapter(), Session.Subtitle());
		return FGameLoop::GoOn(Session);
	}
}
