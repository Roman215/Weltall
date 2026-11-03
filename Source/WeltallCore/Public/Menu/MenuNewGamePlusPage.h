#pragma once

#include "Game/NewGamePlus.h"
#include "Menu/MenuSettingsPages.h"

#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * The New Game+ page, in the options screen's style: a row for each part of the clear save that may carry over, each
	 * Reset (as the original's new game has it, the default) or Keep; the Limiter, Intact (the default) or Removed (the
	 * 7-AP flag carried over); Elly, who Leaves as in the story (the default) or Returns (Keep Elly); then Start.
	 */
	class FMenuNewGamePlusPage
	{
	public:
		static constexpr int32 LimiterRow = 5;
		static constexpr int32 EllyRow = 6;
		static constexpr int32 StartRow = 7;

		WELTALLCORE_API static std::string_view Title();
		WELTALLCORE_API std::vector<FSettingsRow> Rows() const;
		/** Left, Right or Confirm on a part's row: kept or reset (the Limiter removed or intact, Elly back or not); false on Start. */
		WELTALLCORE_API bool Switch(int32 Row);
		const FNewGamePlusCarry& Carry() const { return Chosen; }

	private:
		FNewGamePlusCarry Chosen;
	};
}
