#include "Screens/GameLoopScreen.h"

#include "MessageLogView.h"
#include "Game/GameSession.h"
#include "Game/KernelState.h"

bool FGameLoopScreen::HoldsGame() const
{
	return LogView->IsOpen();
}

void FGameLoopScreen::ToggleMessageLog()
{
	if (LogView->IsOpen())
	{
		LogView->Close();
		HeldThroughLog = LastHeld;
		return;
	}
	if (!Session || !GameLoop || HasStopped())
	{
		return;
	}
	const Weltall::FMesResident& Text = Session->Text();
	LogView->Open(*MessageLog, Text.Font, Text.TextClut, Session->Kernel().WindowColour);
	LogWheel = 0.0f;
}
