#pragma once

#include "Foundation/Types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

/** Where a save's game data keeps each part of the game state (offsets from the data's start). */
namespace Weltall::SaveLayout
{
	struct FRegion
	{
		std::size_t SaveOffset;
		std::size_t StateOffset;
		std::size_t Size;
	};

	/** The characters' records and learned skills, which the gears' recompute reads. */
	inline constexpr std::array<FRegion, 2> RegionsBeforeGears{{
		{0x290, 0x026C, 11 * 0xA4},
		{0xEC4, 0x16C0, 0x160},
	}};

	/** The names, the gears' names and the rest of the state, copied back after the gears. */
	inline constexpr std::array<FRegion, 5> RegionsAfterGears{{
		{0x024, 0x0000, 0x0DC},
		{0x100, 0x00DC, 0x190},
		{0xE4C, 0x1648, 0x078},
		{0x1024, 0x1820, 0x100},
		{0x1124, 0x1920, 0xA38},
	}};

	/** Each gear keeps part of its record (offsets within the gear's save record and its state record). */
	inline constexpr std::size_t GearsOffset = 0x99C;
	inline constexpr std::size_t GearSize = 0x3C;

	/** Its ids and equipment, which its recompute reads. */
	inline constexpr std::array<FRegion, 2> GearFieldsBeforeRecompute{{
		{0x00, 0x00, 0x28},
		{0x28, 0x5C, 4},
	}};

	/** The weight tolerance, which the overweight penalty is computed from. */
	inline constexpr FRegion GearWeightTolerance{0x3A, 0x75, 1};

	/** Fuel, HP, armor break and the attack multiplier and weight tolerance, restored over its recompute. */
	inline constexpr std::array<FRegion, 5> GearFieldsAfterRecompute{{
		{0x34, 0x38, 2},
		{0x2C, 0x60, 4},
		{0x38, 0x99, 1},
		{0x39, 0x74, 1},
		GearWeightTolerance,
	}};

	/** The summary the File screen shows. */
	namespace Summary
	{
		inline constexpr std::size_t PlayTime = 0x000;
		inline constexpr std::size_t Hp = 0x004;
		inline constexpr std::size_t MaxHp = 0x00A;
		inline constexpr std::size_t Ep = 0x010;
		inline constexpr std::size_t MaxEp = 0x013;
		inline constexpr std::size_t Levels = 0x016;
		inline constexpr std::size_t EtherLevels = 0x019;
		inline constexpr std::size_t Party = 0x01C;
		inline constexpr std::size_t CardSlot = 0x023;
	}

	/** A saved name is up to ten glyph indices, ended early by a zero one. */
	inline constexpr uint16 SavedNameEnd = 0;

	/** Copies regions from a save's data (or a gear's save record) into the state (or its gear record). */
	inline void CopyToState(std::span<const FRegion> Regions, std::span<const uint8> Save, std::span<uint8> State)
	{
		for (const FRegion& Region : Regions)
		{
			std::copy_n(Save.begin() + static_cast<std::ptrdiff_t>(Region.SaveOffset), Region.Size, State.begin() + static_cast<std::ptrdiff_t>(Region.StateOffset));
		}
	}

	/** The reverse: regions of the state into a save's data. */
	inline void CopyToSave(std::span<const FRegion> Regions, std::span<const uint8> State, std::span<uint8> Save)
	{
		for (const FRegion& Region : Regions)
		{
			std::copy_n(State.begin() + static_cast<std::ptrdiff_t>(Region.StateOffset), Region.Size, Save.begin() + static_cast<std::ptrdiff_t>(Region.SaveOffset));
		}
	}

	/** Event variable 0, which picks the chapter's title: in the state's region from 1920h, at 1930h. */
	inline constexpr std::size_t StoryProgress = 0x1134;

	/**
	 * The port's own part of a save, in the bytes between the state's last region and the checksum, which the game
	 * writes as zeros and never reads: a mark, then what the port keeps. Without the mark there is none, as in every
	 * save the game itself made.
	 */
	namespace Port
	{
		inline constexpr std::size_t Mark = 0x1B5C;
		inline constexpr std::array<uint8, 4> MarkBytes{'W', 'L', 'T', 'L'};
		/** How the game tells the second disc's story (EStoryMode). */
		inline constexpr std::size_t StoryMode = 0x1B60;
		/**
		 * A save of the expanded second disc made where the original stands elsewhere is the original's save at its
		 * point there (FWayBackPoint), and a game with Elly back is the original's with her away (ClearFile.h,
		 * KeepElly.h); the bytes of the game's own that differ from it are kept here, to be put back when the port loads
		 * it as such a game: how many runs, then each run's offset in the data (two bytes), its length (one) and its bytes.
		 */
		inline constexpr std::size_t KeptRuns = 0x1B61;
		/**
		 * The expansion a game of the expanded second disc plays, by its mod's folder (ExpansionMods.h): the name's bytes,
		 * zeros after them, the kept runs ending before it. A save made before it was kept has zeros: the examples'.
		 */
		inline constexpr std::size_t Expansion = 0x1EB5;
		inline constexpr std::size_t RunsEnd = Expansion;
		/** The order the player keeps the party in (FPartyOrder::ToSaved). */
		inline constexpr std::size_t PartyOrder = 0x1EF5;
		/** How the player has the second Citan and the grown Emeralda drawn (FCharacterLooks::ToSaved). */
		inline constexpr std::size_t CharacterLooks = 0x1EF9;
		/** What the game is after the ending (EClearGame); the own party of the kinds that keep Elly apart the kept runs hold. */
		inline constexpr std::size_t ClearGame = 0x1EFA;
		/** The port's part ends before the checksum. */
		inline constexpr std::size_t End = 0x1EFF;
	}
}
