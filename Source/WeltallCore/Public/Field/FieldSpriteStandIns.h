#pragma once

#include "Field/FieldCharacters.h"
#include "Foundation/Types.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"

#include <vector>

namespace Weltall
{
	class FVram;
	struct FGteArctangents;
	struct FVramImage;

	/**
	 * Sprites the presenter draws in place of the game's, each following one of them: in a sprite system of their own
	 * (their own random numbers; children their scripts spawn ended at once), a stand-in per place, its frames streamed
	 * into an area of VRAM's port rows. Each tick it is drawn, a stand-in takes the step the sprite it follows took
	 * after the last drawing (unless that one was paused or frozen), then plays what that sprite started since, the
	 * animation the caller names for it, at its time scale and facing.
	 *
	 * Only the drawing changes: the game's sprite goes on running the game, its animations' times and signals, its
	 * size, the random numbers its scripts draw.
	 */
	class FFieldSpriteStandIns final : private ISpriteListener
	{
	public:
		/** A place's frames are an area this wide, as the party's own places in VRAM; its CLUTs a row of their own. */
		static constexpr int16 AreaWidth = 0x20;

		/**
		 * Places stand-ins, their frames' areas side by side from (AreaX, AreaY) of VRAM's port rows (y counted from the
		 * console's top, as FVram::UploadToPortRows counts it), their CLUTs from (ClutX, ClutRow) a row each.
		 */
		WELTALLCORE_API FFieldSpriteStandIns(int32 Places, int16 AreaX, int16 AreaY, int16 ClutX, int16 ClutRow);
		FFieldSpriteStandIns(const FFieldSpriteStandIns&) = delete;
		FFieldSpriteStandIns& operator=(const FFieldSpriteStandIns&) = delete;

		/**
		 * Place's stand-in, a sprite of File, brought up to Followed: playing Animation from Followed's starts, at Scale
		 * (4.12) of its size, in its place, tint and blend, falling and landing as it does. Followed's own animation runs after the drawing unless
		 * bAnimates is off (its actor paused or frozen). Tick numbers the field's ticks: a stand-in steps once a tick,
		 * however many passes draw it, and its new frame goes into Vram's port rows. A resident File draws from Picture
		 * instead, its texture's origin the picture's corner, which goes into Vram's port rows as the stand-in is made.
		 * None where its script stopped on something the kernel would not run (for the rest of the map).
		 */
		WELTALLCORE_API const FSprite* Follow(int32 Place, const FSpriteFile& File, int32 Animation, const FSprite& Followed, bool bAnimates, int32 Scale,
			uint32 Tick, const FGteArctangents* Arctangents, FVram& Vram, const FVramImage* Picture = nullptr);

		/** The pass a stand-in is drawn in for its sprite's: the same place and sorting, at its scale (4.12), with its own pivot. */
		WELTALLCORE_API static FFieldSpritePass PassFor(const FSprite& Followed, const FSprite& StandIn, const FFieldSpritePass& Pass, int32 Scale);

		/** A new map: the stand-ins are made again as they are needed. */
		WELTALLCORE_API void Clear();

		/** A place's stand-in as it was last drawn, and the file it shows; none until one was made on the map. */
		const FSprite* Of(int32 Place) const
		{
			const FStandIn& StandIn = StandIns[static_cast<std::size_t>(Place)];
			return StandIn.File != nullptr ? &StandIn.Sprite : nullptr;
		}
		const FSpriteFile* FileOf(int32 Place) const { return StandIns[static_cast<std::size_t>(Place)].File; }

	private:
		/** An ended animation holds its last frame, as the field's sprites' do, until the sprite followed starts another. */
		void OnAnimationEvent(FSprite&) override {}

		struct FStandIn
		{
			FSprite Sprite;
			const FSpriteFile* File = nullptr;
			/** The followed sprite's starts seen, the animation the stand-in was last asked to play, and the tick it last stepped in. */
			uint32 SeenStarts = 0;
			int32 Asked = 0;
			uint32 Tick = 0;
			/** Whether the followed sprite's animation runs in the tick it was last followed in: the stand-in takes that step in the next. */
			bool bFollowedAnimates = false;
			bool bFailed = false;
		};

		void Make(FStandIn& StandIn, int32 Place, const FSpriteFile& File, const FVramImage* Picture, FVram& Vram);
		void Step(FStandIn& StandIn, const FSprite& Followed, int32 Animation, uint32 Tick, bool bMade, FVram& Vram);

		int16 AreaX;
		int16 AreaY;
		int16 ClutX;
		int16 ClutRow;
		/** Made in place once, never moved: a sprite points at its own resource block, and the system at the sprites it builds. */
		std::vector<FStandIn> StandIns;
		FSpriteSystem Kernel;
		FPsxRandom Random;
	};
}
