#pragma once

#include "Foundation/Types.h"

#include <array>
#include <cstddef>
#include <span>

namespace Weltall
{
	/**
	 * How the player has the story's later Citan and Emeralda drawn, the port's own: the second Citan without his
	 * sword, drawn as the first is, and the grown Emeralda as the child; and, back in a clear game, Elly with the purple
	 * hair of Merkava's scene, her own pictures recoloured (EllysHair), and Fei as Id, drawn with the pictures the game has
	 * of him and named Id in the menus (IdsPictures). Only the pictures change: the story's record goes on playing and
	 * levelling, and its scenes, which are written for it, show it as it is.
	 */
	class FCharacterLooks
	{
	public:
		static constexpr uint8 Fei = 0;
		static constexpr uint8 Elly = 1;
		static constexpr uint8 Citan = 2;
		static constexpr uint8 Emeralda = 6;
		static constexpr uint8 SwordCitan = 9;
		static constexpr uint8 GrownEmeralda = 10;
		/** The first Heimdal, which fights with its fists; the second Citan's two, which carry his sword. */
		static constexpr uint8 Heimdal = 3;
		static constexpr uint8 Fenrir = 11;
		static constexpr uint8 SwordHeimdal = 16;
		/** Fei's gears, which only he pilots. */
		static constexpr uint8 Weltall = 0;
		static constexpr uint8 Weltall2 = 1;
		static constexpr uint8 Xenogears = 15;
		static constexpr std::size_t SavedBytes = 1;

		/** Whether Character has another look to switch to: the second Citan and the grown Emeralda; Fei and Elly in a clear game. */
		static constexpr bool CanSwitch(uint8 Character, bool bClearGame)
		{
			return Character == SwordCitan || Character == GrownEmeralda || ((Character == Fei || Character == Elly) && bClearGame);
		}

		/** Switches Character between its looks; false, changing nothing, for one that has no other. */
		WELTALLCORE_API bool Switch(uint8 Character);

		/** The record whose pictures draw Character: the first Citan's or the child's while switched, its own otherwise. */
		WELTALLCORE_API uint8 Drawn(uint8 Character) const;

		/**
		 * The gear whose battle files play Gear's part (its moves, their sounds and its attacks' names): while Citan's sword
		 * is put away, the first Heimdal's for the second Heimdal and for Fenrir.
		 */
		WELTALLCORE_API uint8 PerformingGear(uint8 Gear) const;

		/** Whether Gear's own model is drawn over the performing gear's moves: Fenrir's, with no sword, doing Heimdal's. */
		WELTALLCORE_API bool WearsOwnModel(uint8 Gear) const;

		/** Whether Character's own pictures are drawn with Elly's purple hair. */
		bool HasPurpleHair(uint8 Character) const { return Character == Elly && bEllyPurpleHair; }

		/** Whether Character is drawn and named as Id. */
		bool IsId(uint8 Character) const { return Character == Fei && bFeiAsId; }

		/** Whether Gear is Fei's, drawn as Id's red gear while he is drawn as Id (its moves, numbers and commands its own). */
		bool WearsIdsGear(uint8 Gear) const { return bFeiAsId && (Gear == Weltall || Gear == Weltall2 || Gear == Xenogears); }

		/** Whether any is drawn otherwise than the story has it. */
		bool IsSet() const { return bCitanWithoutSword || bChildEmeralda || bEllyPurpleHair || bFeiAsId; }

		/**
		 * These looks with every member drawn from pictures numbered as its own record's: Elly's hair and Fei as Id kept (Id's
		 * file numbers his poses as Fei's), the second Citan and the grown Emeralda, drawn as other records, as the story has them.
		 */
		FCharacterLooks WithOwnNumbering() const
		{
			FCharacterLooks Kept = *this;
			Kept.bCitanWithoutSword = false;
			Kept.bChildEmeralda = false;
			return Kept;
		}

		/** As a save keeps it; FromSaved reads it back, ignoring bits it does not know. */
		WELTALLCORE_API std::array<uint8, SavedBytes> ToSaved() const;
		WELTALLCORE_API static FCharacterLooks FromSaved(std::span<const uint8, SavedBytes> Bytes);

		bool operator==(const FCharacterLooks&) const = default;

	private:
		bool bCitanWithoutSword = false;
		bool bChildEmeralda = false;
		bool bEllyPurpleHair = false;
		bool bFeiAsId = false;
	};
}
