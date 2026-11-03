#include "Game/KeepElly.h"

#include "Game/GameState.h"

namespace Weltall::KeepElly
{
	namespace
	{
		constexpr uint16 FeiBit = 1 << 0;
		constexpr uint8 Elly = 1;
		constexpr uint16 EllyBit = 1 << Elly;
		constexpr uint8 Regurus = 10;
		/** The saved variable holding the story's progress. */
		constexpr int32 StoryVariable = 0;
	}

	bool IsBack(const FGameState& State)
	{
		return State.ClearGame() == EClearGame::KeepElly && State.SavedEventVariable(StoryVariable) >= ReturnStory;
	}

	void Hold(FGameState& State)
	{
		if (!IsBack(State) || (State.JoinedCharacters() & FeiBit) == 0)
		{
			return;
		}
		State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() | EllyBit));
		State.SetMenuCharacters(static_cast<uint16>(State.MenuCharacters() | EllyBit));
		State.SetCharacterGear(Elly, Regurus);
	}
}
