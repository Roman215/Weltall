#pragma once

#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Regurus (Elly's Omnigear, gear 10) is unfinished in the original, which never puts her in it: of its actions only the
	 * plain attacks, the first level's Deathblows and the ethers have sequences, its opening never ends, and its attack names
	 * are Vierge's left as the Japanese spells them. In the port's clear file she fights in it as in Vierge: Vierge's
	 * moves, sounds and effects with Regurus' own model worn over them (FBattleGearDress), and Vierge's names.
	 */
	namespace BattleRegurus
	{
		inline constexpr uint8 Regurus = 10;
		inline constexpr uint8 Vierge = 2;

		/** The gear whose attack names name Gear's: Vierge's for Regurus, the same twelve. */
		constexpr uint8 NamingGear(uint8 Gear) { return Gear == Regurus ? Vierge : Gear; }
	}
}
