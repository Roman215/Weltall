#pragma once

namespace Weltall
{
	/** The fixes of the battle show's broken effects in use: each on unless the original's drawing is asked for. */
	struct FBattleShowOptions
	{
		/**
		 * A weapon trail's ribbon blends with what lies behind it: in the blend mode its sprite names, or half and
		 * half when the sprite names none. The original marks a scratch line semi-transparent in the ribbon's place,
		 * and only for a sprite that names a mode, so every ribbon covers what is behind it.
		 */
		bool bTrailsBlend = true;
		/**
		 * A lit model whose sprite loads no light of its own is lit by the stage's light turned into the model's
		 * frame, the same in every battle; the original lights it with whatever was lit last, in the battle or in
		 * the program before it.
		 */
		bool bUnlitModelsTakeStageLight = true;
		/**
		 * A stage's palette animation passes on to the tint's kept palettes only the colours the two share. The
		 * original measures the overlap from the palettes' near edges alone, so an animation of one row inside the
		 * stage's rows copies the memory after its own colours over the rows below it, and the stage keeps them
		 * for the rest of the battle (the burning village's flames go out or change colour after the first tint).
		 */
		bool bTintKeepsStagePalettes = true;
		/**
		 * Regurus, unfinished in the original, fights as Vierge wearing its own model (BattleRegurus): Vierge's moves,
		 * sounds and effects. The original's opening never ends, so a battle with Elly in it waits for good, and most of
		 * its actions draw nothing. No row: the original never puts her in it, only the port's clear file does.
		 */
		bool bRegurusFightsAsVierge = true;

		/** Every fix off: the original's drawing throughout. */
		static constexpr FBattleShowOptions Original()
		{
			FBattleShowOptions Options;
			Options.bTrailsBlend = false;
			Options.bUnlitModelsTakeStageLight = false;
			Options.bTintKeepsStagePalettes = false;
			Options.bRegurusFightsAsVierge = false;
			return Options;
		}
	};
}
