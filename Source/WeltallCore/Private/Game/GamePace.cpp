#include "Game/GamePace.h"

#include "Battle/BattleRuntime.h"
#include "Game/BattleGameProgram.h"
#include "Game/ClearFile.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/KernelState.h"
#include "Game/PlayerSettings.h"
#include "Game/WorldMapProgram.h"

namespace Weltall
{
	bool IsBattleFought(const FGameLoop& Loop)
	{
		const FBattleGameProgram* Battle = Loop.Stage() == EGameLoopStage::Program ? Loop.Battle() : nullptr;
		return Battle != nullptr && Battle->Runtime().IsFought();
	}

	bool CanFastForwardExploration(const FGameLoop& Loop, const FPlayerSettings& Settings)
	{
		if (!Settings.Balance.bExplorationFastForward || Loop.Stage() != EGameLoopStage::Program)
		{
			return false;
		}
		if (const FFieldProgram* Field = Loop.Field())
		{
			const FFieldRuntime& Runtime = Field->Runtime();
			const FEventContext& Context = Runtime.Context();
			return Field->TicksField() && Context.RunningMap != FKernelState::TitleMap
				&& Runtime.LastFrame() != EFieldFrame::Paused && Runtime.LastFrame() != EFieldFrame::MapPicture
				&& Runtime.LastFrame() != EFieldFrame::MenuShade && !ClearFile::IsEndingWaiting(Context);
		}
		const FWorldMapProgram* Map = Loop.WorldMap();
		return Map != nullptr && Map->TicksMap() && Map->Runtime().LastFrame() != EWorldMapFrame::Paused;
	}

	EBattleSpeed ActiveGameSpeed(const FGameLoop& Loop, const FPlayerSettings& Settings)
	{
		if (IsBattleFought(Loop))
		{
			return Settings.BattleSpeed;
		}
		return CanFastForwardExploration(Loop, Settings) ? Loop.ExplorationSpeed() : EBattleSpeed::Normal;
	}

	int32 GamePace(const FGameLoop& Loop, const FPlayerSettings& Settings)
	{
		return BattleSpeedRate(ActiveGameSpeed(Loop, Settings));
	}
}
