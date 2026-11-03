#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/Vram.h"

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	struct FFieldMap;

	/**
	 * Fei drawn as Id on the field and the world map (FCharacterLooks): Id's own field sprite, map 682's fourth (the same
	 * file on both discs). It stays resident, as it is there, its picture cut from the map's texture into a page of the
	 * port's own rows. His file has five animations; it is given the fifteen every member's own sprite numbers alike:
	 * standing and walking as his own, running as his walk at twice its pace, jumping with his head bowed at the take-off
	 * and the landing and a stride of his walk held in the air, climbing as his walk at the climb's pace, and the gestures
	 * as his nearest poses (a nod and a bow with his head bowed, a crouch too, a raised arm with his arms crossed, the
	 * rest standing).
	 */
	class FIdsFieldSprite
	{
	public:
		static constexpr int32 Map = 682;
		static constexpr int32 MapSprite = 3;
		/** His picture's page: the first band of the port's rows, beside the party's stand-ins' areas. */
		static constexpr int16 TextureX = 0xC0;
		static constexpr int16 TextureY = FVram::Height;
		/**
		 * His two palettes' place where a drawing keeps its sprites' palettes only 16 colours apart (the world map's: its
		 * own member files have one), below his picture. His frames toward the camera draw with the second.
		 */
		static constexpr int16 ClutX = 0xC0;
		static constexpr int16 ClutY = FVram::Height + 0xC0;

		WELTALLCORE_API static TResult<FIdsFieldSprite> Load(const FCdImage& Image, const FGameFileIndex& Index);
		/** Made from map 682 as the field loads it. */
		WELTALLCORE_API static TResult<FIdsFieldSprite> Make(const FFieldMap& Map);

		const FSpriteFile& File() const { return Sprite; }
		/** His picture at its place in the port's rows; a sprite of File takes its texture from (TextureX, TextureY). */
		const FVramImage& Picture() const { return Texture; }

	private:
		FIdsFieldSprite(FSpriteFile InSprite, FVramImage InTexture) : Sprite(std::move(InSprite)), Texture(std::move(InTexture)) {}

		FSpriteFile Sprite;
		FVramImage Texture;
	};
}
