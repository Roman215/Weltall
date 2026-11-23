#pragma once

#include "Foundation/Types.h"

#include <span>

namespace Weltall
{
	class FGameState;

	/** A character's Deathblows: 13 of them, the last six the 7-AP tier. */
	namespace CharacterSkills
	{
		constexpr int32 DeathblowCount = 13;
		constexpr int32 FirstUpperTier = 7;
		/** A character's record in the growth table: the thresholds first, 7 words a Deathblow. */
		constexpr std::size_t GrowthRecordSize = 0x110;
		constexpr std::size_t DeathblowLevels = 0x100;
	}

	/** Bits for moves the character can learn. A level of FF ends its list, even if the save sets later bits. */
	WELTALLCORE_API uint16 LearnableDeathblowMask(uint8 Character, std::span<const uint8> GrowthTable);

	/** Whether a character has learned a Deathblow (bit 0x8000 >> n of its first learned-skills word). */
	WELTALLCORE_API bool IsDeathblowLearned(const FGameState& State, int32 Character, int32 Deathblow);

	/**
	 * How far a character has come toward learning a Deathblow, as the Skills screen shows it: the
	 * average over the Deathblow's thresholds that are not 0 (the growth table's, 7 a Deathblow) of
	 * min(100, counter × 100 / threshold) with the character's seven basic-attack counters, a threshold
	 * of FFFFh counting as 0; 0 for the upper tier until the story opens it (the story flags' bit 14).
	 */
	WELTALLCORE_API uint32 DeathblowPercent(const FGameState& State, int32 Character, int32 Deathblow, std::span<const uint8> GrowthTable);
}
