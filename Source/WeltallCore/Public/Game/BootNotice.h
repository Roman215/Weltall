#pragma once

#include "Foundation/Result.h"
#include "Graphics/Vram.h"
#include "Presentation/DrawList.h"

#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;
	struct FIndexRecord;

	/**
	 * The notice the kernel shows before the first movie: brightened in 16 vertical blanks to 120, held, then from 128
	 * back to black in 17. The game's is the publisher's 256 x 48 picture from the executable, at (32, 88), held 110
	 * blanks; the French translation's boot code shows its translators' 320 x 224 picture from a file of its own, held 328.
	 * The same code shows the translation's credits at the game's end, held until Start or Circle is newly pressed.
	 */
	class FBootNotice
	{
	public:
		static constexpr FScreenSize ScreenSize{320, 240};
		static constexpr int32 FadeInVblanks = 16;
		static constexpr int32 FadeOutVblanks = 17;
		/** The song the translation's credits play from its start, the field's music stopped first. */
		static constexpr int32 TranslationCreditsSong = 8;

		/** Takes the picture the boot code shows, as it unpacks and uploads it. */
		WELTALLCORE_API static TResult<FBootNotice> Load(const FPsxExecutable& Executable, const FCdImage& Image, const FGameFileIndex& Index);

		/** The French translation's credits from the disc's own files. */
		WELTALLCORE_API static TResult<FBootNotice> LoadTranslationCredits(const FCdImage& Image, const FGameFileIndex& Index);

		/**
		 * The file whose read makes the French translation's kernel show its credits first (directory 4's file 2129, the
		 * picture that ends the game); null on other discs.
		 */
		WELTALLCORE_API static const FIndexRecord* TranslationCreditsCue(const FGameFileIndex& Index);

		/** Runs one vertical blank, the game's logical buttons Held at it; returns whether the notice still shows. */
		WELTALLCORE_API bool Tick(uint16 Held);

		/** How many vertical blanks the notice shows for; the credits' count is known once their press comes. */
		int32 Vblanks() const { return FadeInVblanks + HoldVblanks + FadeOutVblanks; }

		/** Whether the notice has run its vertical blanks. */
		bool IsOver() const { return Vblank >= Vblanks(); }

		const FVram& Vram() const { return Screen; }

		/** The texels the notice's sprites sample, from Vram(). */
		std::span<const FDrawTexture> Textures() const { return SpriteTextures; }

		/** What the GPU draws now: nothing before the first blank. */
		WELTALLCORE_API FDrawList DrawList() const;
		/** The same into Into, its memory reused. */
		WELTALLCORE_API void DrawList(FDrawList& Into) const;

	private:
		/** A sprite of the picture: its place on the screen, its size and its texture (u and v 0). */
		struct FSprite
		{
			int32 X = 0;
			int32 Y = 0;
			int32 Width = 0;
			int32 Height = 0;
			std::size_t Texture = 0;
		};

		static TResult<FBootNotice> LoadTranslationPicture(const FCdImage& Image, const FGameFileIndex& Index, int32 File);

		FVram Screen;
		int32 Vblank = 0;
		int32 HoldVblanks = 0;
		/** The hold lasts until a new press; the buttons held at the blank before. */
		bool bHeldUntilPressed = false;
		bool bPressed = false;
		uint16 HeldBefore = 0;
		std::vector<FDrawTexture> SpriteTextures;
		std::vector<FSprite> Sprites;
	};
}
