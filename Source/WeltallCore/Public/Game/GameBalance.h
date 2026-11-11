#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/** How much of a battle's experience or gold the party gets: the original's, or a multiple of it. */
	enum class ERewardRate : uint8
	{
		Original,
		OneAndAHalf,
		Double,
		Triple,
		Quadruple,
	};

	/** How often random battles come against the original: none, a quarter, half, the original's or twice. */
	enum class EEncounterRate : uint8
	{
		None,
		Quarter,
		Half,
		Original,
		Double,
	};

	/** The player's changes to the game's balance: the original's unless chosen. */
	struct FGameBalance
	{
		ERewardRate Experience = ERewardRate::Original;
		ERewardRate Gold = ERewardRate::Original;
		EEncounterRate Encounters = EEncounterRate::Original;
		bool bExplorationFastForward = false;

		bool operator==(const FGameBalance&) const = default;
	};

	/** Amount at Rate, rounded down; an amount past 32 bits stays at the largest. */
	WELTALLCORE_API uint32 RewardAt(uint32 Amount, ERewardRate Rate);

	/**
	 * A period of moving ticks holding a set number of encounters, lengthened or shortened so that they come at Rate:
	 * four times as long for a quarter of them, half as long (at least a tick) for twice as many. None and Original keep
	 * it: where encounters stop, their caller stops counting.
	 */
	WELTALLCORE_API int32 EncounterPeriodAt(int32 Period, EEncounterRate Rate);
}
