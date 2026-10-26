#include "Game/BootNotice.h"

#include "Formats/Lzss.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** Where the executable carries the packed picture (both discs of the USA release). */
		constexpr uint32 PictureAddress = 0x8004EABC;
		/** The kernel puts the picture and its CLUT here, whatever the file says. */
		constexpr int16 PictureX = 640;
		constexpr int16 PictureY = 0;
		constexpr int16 ClutX = 0;
		constexpr int16 ClutY = 240;
		/** The default drawing environment's texture page: 4-bit at (640, 0). */
		constexpr uint16 DefaultTexturePage = 10;
		constexpr int32 SpriteX = 32;
		constexpr int32 SpriteY = 88;
		constexpr int32 GameHoldVblanks = 110;
		constexpr uint32 SpriteBlendKey = 1;

		/**
		 * The French translation's boot code jumps from the notice's unpacking into its own, which reads its picture from
		 * file 12 of its own files (FGameFileIndex::TranslationFilesStart). The picture: a packed 8-bit TIM of 320 x 224
		 * whose CLUT and pixels go where the game's go, drawn from (0, 8) as two sprites, 256 and 64 wide (a page's width;
		 * the second's texels the pixels past 256), held 328 blanks.
		 */
		constexpr uint32 BootJumpAddress = 0x80019D6C;
		constexpr uint32 TranslationBootJump = 0x08013AAF;
		constexpr int32 TranslationPictureFile = 12;
		constexpr int32 TranslationWidth = 320;
		constexpr int32 TranslationHeight = 224;
		constexpr int32 TranslationY = 8;
		constexpr int32 PageWidth = 256;
		/** The 8-bit pages at 640 and 768 across. */
		constexpr uint16 TranslationPages[2] = {0x8A, 0x8C};
		constexpr int32 TranslationHoldVblanks = 328;

		/**
		 * The translation's file reading calls the same code for file 13, its credits, before it reads the record 63Fh
		 * before its files' start. Their hold ends at the first blank whose Start or Circle was not held at the one before.
		 */
		constexpr int32 TranslationCreditsFile = 13;
		constexpr std::size_t TranslationCreditsCueDistance = 0x63F;
		constexpr uint16 TranslationCreditsButtons = 0x0820;

		/** The brightness the kernel drew with by a vertical blank, from 1. */
		uint8 BrightnessAt(int32 Vblank, int32 Hold)
		{
			if (Vblank <= FBootNotice::FadeInVblanks)
			{
				return static_cast<uint8>((Vblank - 1) * 8);
			}
			if (Vblank <= FBootNotice::FadeInVblanks + Hold)
			{
				return static_cast<uint8>((FBootNotice::FadeInVblanks - 1) * 8);
			}
			return static_cast<uint8>(128 - (Vblank - FBootNotice::FadeInVblanks - Hold - 1) * 8);
		}

		bool HasTranslationBoot(const FPsxExecutable& Executable)
		{
			return Executable.Contains(BootJumpAddress, 4) && LittleEndian::ReadU32(Executable.Read(BootJumpAddress, 4), 0) == TranslationBootJump;
		}

		/** The picture TIM, its CLUT and pixels moved where the kernel puts them, uploaded. */
		TResult<FTim> UploadPicture(std::span<const uint8> Packed, FVram& Screen)
		{
			TResult<std::vector<uint8>> Unpacked = UnpackLzss(Packed);
			if (!Unpacked)
			{
				return Unpacked.Error();
			}
			TResult<FTim> Picture = ParseTim(*Unpacked);
			if (!Picture)
			{
				return Picture.Error();
			}
			if (!Picture->Clut)
			{
				return MakeError("the boot notice's picture has no CLUT");
			}
			FVramImage Clut = *Picture->Clut;
			Clut.Rect.X = ClutX;
			Clut.Rect.Y = ClutY;
			Screen.Upload(Clut);
			FVramImage Image = Picture->Image;
			Image.Rect.X = PictureX;
			Image.Rect.Y = PictureY;
			Screen.Upload(Image);
			return Picture;
		}
	}

	TResult<FBootNotice> FBootNotice::Load(const FPsxExecutable& Executable, const FCdImage& Image, const FGameFileIndex& Index)
	{
		if (HasTranslationBoot(Executable))
		{
			return LoadTranslationPicture(Image, Index, TranslationPictureFile);
		}
		if (!Executable.Contains(PictureAddress, 4))
		{
			return MakeError("the executable does not carry the boot notice");
		}
		FBootNotice Notice;
		const uint32 End = Executable.LoadAddress() + static_cast<uint32>(Executable.Body().size());
		const TResult<FTim> Picture = UploadPicture(Executable.Read(PictureAddress, End - PictureAddress), Notice.Screen);
		if (!Picture)
		{
			return Picture.Error();
		}
		if (Picture->PixelWidth() != 256 || Picture->Image.Rect.Height != 48)
		{
			return MakeError("the boot notice's picture is not 256 x 48");
		}
		Notice.SpriteTextures.push_back({0, FTexturePage::FromAttribute(DefaultTexturePage), {ClutX, ClutY}});
		Notice.Sprites = {{SpriteX, SpriteY, 256, 48, 0}};
		Notice.HoldVblanks = GameHoldVblanks;
		return Notice;
	}

	TResult<FBootNotice> FBootNotice::LoadTranslationCredits(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<FBootNotice> Notice = LoadTranslationPicture(Image, Index, TranslationCreditsFile);
		if (Notice)
		{
			Notice->HoldVblanks = 0;
			Notice->bHeldUntilPressed = true;
		}
		return Notice;
	}

	const FIndexRecord* FBootNotice::TranslationCreditsCue(const FGameFileIndex& Index)
	{
		const std::optional<std::size_t> Start = Index.TranslationFilesStart();
		if (!Start || *Start < TranslationCreditsCueDistance || *Start - TranslationCreditsCueDistance >= Index.Records().size())
		{
			return nullptr;
		}
		return &Index.Records()[*Start - TranslationCreditsCueDistance];
	}

	TResult<FBootNotice> FBootNotice::LoadTranslationPicture(const FCdImage& Image, const FGameFileIndex& Index, int32 File)
	{
		const std::optional<std::size_t> Start = Index.TranslationFilesStart();
		const std::size_t Record = Start.value_or(0) + static_cast<std::size_t>(File) - 1;
		if (!Start || Record >= Index.Records().size())
		{
			return MakeError("the disc lacks the translation's picture " + std::to_string(File));
		}
		const TResult<std::vector<uint8>> Packed = ReadFileSectors(Image, Index.Records()[Record]);
		if (!Packed)
		{
			return Packed.Error();
		}
		FBootNotice Notice;
		const TResult<FTim> Picture = UploadPicture(*Packed, Notice.Screen);
		if (!Picture)
		{
			return Picture.Error();
		}
		if (Picture->PixelWidth() != TranslationWidth || Picture->Image.Rect.Height != TranslationHeight || Picture->Clut->Rect.Width != 256)
		{
			return MakeError("the translation's picture " + std::to_string(File) + " is not an 8-bit 320 x 224");
		}
		for (std::size_t Page = 0; Page < 2; ++Page)
		{
			Notice.SpriteTextures.push_back({0, FTexturePage::FromAttribute(TranslationPages[Page]), {ClutX, ClutY}});
		}
		Notice.Sprites = {{0, TranslationY, PageWidth, TranslationHeight, 0},
			{PageWidth, TranslationY, TranslationWidth - PageWidth, TranslationHeight, 1}};
		Notice.HoldVblanks = TranslationHoldVblanks;
		return Notice;
	}

	bool FBootNotice::Tick(uint16 Held)
	{
		if (IsOver())
		{
			return false;
		}
		++Vblank;
		if (bHeldUntilPressed && !bPressed)
		{
			// Each blank of the hold is drawn, then the pad read: a new press ends it with that blank.
			if (Vblank > FadeInVblanks)
			{
				HoldVblanks = Vblank - FadeInVblanks;
				bPressed = ((Held ^ HeldBefore) & Held & TranslationCreditsButtons) != 0;
			}
			HeldBefore = Held;
		}
		return true;
	}

	FDrawList FBootNotice::DrawList() const
	{
		FDrawList List;
		DrawList(List);
		return List;
	}

	void FBootNotice::DrawList(FDrawList& Into) const
	{
		Into.Primitives.clear();
		if (Vblank == 0)
		{
			return;
		}
		const uint8 Level = BrightnessAt(Vblank, HoldVblanks);
		for (const FSprite& Sprite : Sprites)
		{
			FDrawPrimitive Drawn = MakeSprite(Sprite.X, Sprite.Y, Sprite.Width, Sprite.Height, SpriteTextures[Sprite.Texture], 0, 0, {Level, Level, Level});
			Drawn.BlendKey = SpriteBlendKey + static_cast<uint32>(&Sprite - Sprites.data());
			Into.Primitives.push_back(Drawn);
		}
	}
}
