#pragma once

#include "Battle/Show/BattleSpriteFiles.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Vram.h"

#include <vector>

namespace Weltall
{
	class FBattleSceneGraphics;

	/**
	 * Fei drawn as Id in a battle on foot (FCharacterLooks): Id's own battle sprite, the first entry of scene 51's graphics
	 * (the Zeboim ruins' fight with him, the same on both discs). It stays resident, as it is there, its four images in the
	 * port's own rows, where nothing of the battle's goes. Where his file has nothing for a pose the battle names, or an
	 * attack of its own, it names his nearest: his leap where Fei runs or jumps in or boards, his flip home where Fei runs
	 * away, his flinch where Fei is hurt, his kick where Fei counters, his raised hand where Fei casts, waits on an effect,
	 * uses an item or wins; he crouches weak and stays down once knocked down.
	 */
	class FIdsBattleSprite
	{
	public:
		static constexpr int32 Scene = 51;
		/** His images: four 64-unit columns from x 0 of the port's rows. */
		static constexpr int16 TextureX = 0;
		static constexpr int16 TextureY = FVram::Height;

		WELTALLCORE_API static TResult<FIdsBattleSprite> Load(const FCdImage& Image, const FGameFileIndex& Index);
		/** Made from scene 51's graphics. */
		WELTALLCORE_API static TResult<FIdsBattleSprite> Make(const FBattleSceneGraphics& Scene);

		const FBattleSpriteFile& File() const { return Sprite; }
		/** His images at their places in the port's rows. */
		const std::vector<FVramImage>& Images() const { return Pictures; }

		/**
		 * The animation of his own file that plays a party record's code below 10h, which for anyone else names an animation
		 * of the attack file: Fei's basic attacks as Id's plain attack, and the Combo command's Deathblows as his combos (the
		 * quick one for those of 4 and 5 AP, the second quick one for those of 6, the double Fierce as the big one).
		 */
		WELTALLCORE_API static uint8 AttackAnimation(uint8 Code);

	private:
		FIdsBattleSprite(FBattleSpriteFile InSprite, std::vector<FVramImage> InPictures) : Sprite(std::move(InSprite)), Pictures(std::move(InPictures)) {}

		FBattleSpriteFile Sprite;
		std::vector<FVramImage> Pictures;
	};
}
