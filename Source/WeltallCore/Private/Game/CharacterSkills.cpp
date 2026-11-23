#include "Game/CharacterSkills.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GameStateLayout.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 Counters = 7;
		constexpr uint16 UpperTierOpen = 0x4000;
		constexpr uint16 CountedAsNothing = 0xFFFF;
		constexpr uint32 Whole = 100;
	}

	uint16 LearnableDeathblowMask(uint8 Character, std::span<const uint8> GrowthTable)
	{
		const std::size_t Start = Character * CharacterSkills::GrowthRecordSize + CharacterSkills::DeathblowLevels;
		if (Start + CharacterSkills::DeathblowCount > GrowthTable.size())
		{
			return 0;
		}
		const auto Levels = GrowthTable.subspan(Start, CharacterSkills::DeathblowCount);
		const auto Count = std::find(Levels.begin(), Levels.end(), uint8{0xFF}) - Levels.begin();
		return static_cast<uint16>(0xFFFFu << (16 - Count));
	}

	bool IsDeathblowLearned(const FGameState& State, int32 Character, int32 Deathblow)
	{
		using namespace GameStateLayout;
		const uint16 Learned = LittleEndian::ReadU16(State.Block(), LearnedSkillsOffset + static_cast<std::size_t>(Character) * LearnedSkillsSize + Skills::Deathblows);
		return (Learned & (0x8000u >> Deathblow)) != 0;
	}

	uint32 DeathblowPercent(const FGameState& State, int32 Character, int32 Deathblow, std::span<const uint8> GrowthTable)
	{
		using namespace GameStateLayout;
		const std::span<const uint8> Block = State.Block();
		if (Deathblow >= CharacterSkills::FirstUpperTier && (LittleEndian::ReadU16(Block, StoryFlagsOffset) & UpperTierOpen) == 0)
		{
			return 0;
		}
		const std::size_t Thresholds = static_cast<std::size_t>(Character) * CharacterSkills::GrowthRecordSize + static_cast<std::size_t>(Deathblow) * Counters * 2;
		uint32 Sum = 0;
		uint32 Counted = 0;
		for (int32 Counter = 0; Counter < Counters; ++Counter)
		{
			const std::size_t At = Thresholds + static_cast<std::size_t>(Counter) * 2;
			const uint32 Threshold = At + 2 <= GrowthTable.size() ? LittleEndian::ReadU16(GrowthTable, At) : 0;
			const uint32 Value = LittleEndian::ReadU16(Block, CharacterOffset(static_cast<std::size_t>(Character)) + Character::DeathblowCounters + static_cast<std::size_t>(Counter) * 2);
			if (Threshold == 0)
			{
				continue;
			}
			if (Value != 0 && Threshold != CountedAsNothing)
			{
				Sum += std::min(Whole, Value * Whole / Threshold);
			}
			++Counted;
		}
		return Counted == 0 ? 0 : Sum / Counted;
	}
}
