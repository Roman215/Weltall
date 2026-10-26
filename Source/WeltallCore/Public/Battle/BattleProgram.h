#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** The Battle program as the kernel loads it (directory 1 file 16, unpacked), whose tables the battle reads. */
	WELTALLCORE_API TResult<std::vector<uint8>> ReadBattleProgram(const FCdImage& Image, const FGameFileIndex& Index);

	/**
	 * The Battle program's own tables that the party's commands read: the Deathblows' button patterns
	 * and the Deathblow each completes, the basic attacks' chain, the gears' attack commands, the AP
	 * bar's curve, the command menu's bits and the effects each character's records cast. Read from
	 * the program's file as the game loads it.
	 */
	class FBattleProgramTables
	{
	public:
		static constexpr int32 PatternCount = 13;
		static constexpr int32 PatternLength = 7;
		/** Each character's Deathblow list: 13 for the patterns, then those one more press completes. */
		static constexpr int32 DeathblowListSize = 23;
		static constexpr int32 DeathblowListCount = 16;
		static constexpr int32 ApBarRows = 8;
		static constexpr int32 ApBarColumns = 8;
		static constexpr int32 GearAttackCount = 15;

		/** From the program as it lies in memory (unpacked), loaded at its base address. */
		WELTALLCORE_API static TResult<FBattleProgramTables> Parse(std::span<const uint8> Program);

		/** Read from the disc as the kernel loads the Battle program. */
		WELTALLCORE_API static TResult<FBattleProgramTables> Load(const FCdImage& Image, const FGameFileIndex& Index);

		/** The buttons before a Deathblow's Fierce press (0 Fierce, 2 Weak, 3 Strong), FFh after them. */
		std::array<std::array<uint8, PatternLength>, PatternCount> Patterns{};
		/** By character id: the Deathblow each pattern completes, then the ones a further Weak or Strong press completes. */
		std::array<std::array<uint8, DeathblowListSize>, DeathblowListCount> Deathblows{};
		/** The AP bar's height by the AP a turn began with and the AP left (the rows below 3 are the bytes the table follows). */
		std::array<std::array<uint8, ApBarColumns>, ApBarRows> ApBar{};
		/** By the attack before (0 none, else its command + 1) and the press's AP - 1: the next basic attack's command + 1. */
		std::array<uint8, 24> BasicAttacks{};
		/** The gears' attack commands by button and follow-up. */
		std::array<uint8, GearAttackCount> GearAttacks{};
		/** The command menu's bit for each of its sixteen commands. */
		std::array<uint16, 16> CommandBits{};
		/**
		 * By character id: the effect its record code 10h casts (each code above it the next effect), and the first of its effects
		 * cast from the plain cast pose (11h) rather than its own (1Ch).
		 */
		static constexpr int32 EffectCharacterCount = 12;
		std::array<int16, EffectCharacterCount> EffectBases{};
		std::array<int16, EffectCharacterCount> EffectCastLimits{};
		/**
		 * The voices a battle event plays, by set: the set's sound effects' file in the party's directory (its banks the
		 * files after it, one a voice) and the effect its first voice plays.
		 */
		struct FVoiceSet
		{
			int16 File = 0;
			int16 FirstEffect = 0;
		};
		static constexpr int32 VoiceSetCount = 11;
		/**
		 * The curve a sprite script's trail is drawn along, through its control points four at a time: by the span's
		 * place in the curve (the first, the second, one between, the one before the last, the last) and its eight steps,
		 * each control point's weight, 16384 their sum.
		 */
		static constexpr int32 TrailSpanKinds = 5;
		static constexpr int32 TrailSteps = 8;
		std::array<std::array<int16, 4>, TrailSpanKinds * TrailSteps> TrailCurve{};
		/** The set whose voices share one bank, the file after its effects'. */
		static constexpr int32 SingleBankVoiceSet = 8;
		std::array<FVoiceSet, VoiceSetCount> VoiceSets{};
		/**
		 * The program keeps the battle camera after an enemy's queue write whose third byte is set, as Perfect Works Build
		 * does (FBattleContext::KeepCamera): the French translation's.
		 */
		bool bQueueKeepsCamera = false;
	};
}
