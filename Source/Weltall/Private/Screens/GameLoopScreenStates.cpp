#include "Screens/GameLoopScreen.h"

#include "StateNoticeView.h"
#include "Game/GameLoop.h"
#include "Game/GameSession.h"
#include "Game/SaveState.h"
#include "Game/SaveStateSlots.h"
#include "WeltallLog.h"

void FGameLoopScreen::PressPortKey(Weltall::EControlAction Action)
{
	if (Action == Weltall::EControlAction::MessageLog)
	{
		ToggleMessageLog();
		return;
	}
	if (StateSlots == nullptr || !Session || !GameLoop || HoldsGame())
	{
		return;
	}
	switch (Action)
	{
	case Weltall::EControlAction::SaveState:
		if (!HasStopped())
		{
			Tell(StateSlots->Save(*GameLoop, *Session));
		}
		break;
	case Weltall::EControlAction::LoadState:
		LoadState();
		break;
	case Weltall::EControlAction::NextStateSlot:
		Tell(StateSlots->Step(1));
		break;
	case Weltall::EControlAction::PreviousStateSlot:
		Tell(StateSlots->Step(-1));
		break;
	default:
		break;
	}
}

void FGameLoopScreen::LoadState()
{
	Weltall::FSaveStateTold Told;
	const std::optional<Weltall::FSaveState> State = StateSlots->Read(Told);
	if (!State)
	{
		Tell(Told);
		return;
	}
	// Asked of the running console first: a disc it was not given is looked for, and kept for the next.
	if (!Session->HasDisc(State->Kernel.Disc))
	{
		Tell({StateSlots->NeedsDisc(State->Kernel.Disc), {}});
		return;
	}
	if (const std::optional<Weltall::FError> Unloaded = Start(std::nullopt, &*State))
	{
		// The console that held the state runs nothing: it powers on afresh, as a reset starts it.
		Tell({StateSlots->OtherFiles(), Unloaded->Message});
		if (const std::optional<Weltall::FError> Unstarted = Start(std::nullopt))
		{
			Stop(*Unstarted);
		}
		return;
	}
	UE_LOG(LogWeltall, Log, TEXT("Save state slot %d loaded"), StateSlots->Chosen());
	Tell({StateSlots->Loaded(), {}});
}

void FGameLoopScreen::Tell(const Weltall::FSaveStateTold& Told)
{
	if (!Told.Why.empty())
	{
		UE_LOG(LogWeltall, Warning, TEXT("%hs: %hs"), Told.Line.c_str(), Told.Why.c_str());
	}
	StateNotice->Show(Told.Line);
}
