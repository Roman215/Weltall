#include "Game/BootNotice.h"
#include "Graphics/Texture.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	/** The game's logical buttons. */
	constexpr uint16 StartButton = 0x0800;
	constexpr uint16 CircleButton = 0x0020;
	constexpr uint16 CrossButton = 0x0040;
	constexpr uint16 SquareButton = 0x0080;

	/** The notice's brightness after a number of vertical blanks, with Held at each. */
	uint8 BrightnessAfter(FBootNotice& Notice, int32& Done, int32 Vblanks, uint16 Held = 0)
	{
		for (; Done < Vblanks; ++Done)
		{
			REQUIRE(Notice.Tick(Held));
		}
		const FDrawList List = Notice.DrawList();
		REQUIRE_FALSE(List.Primitives.empty());
		return List.Primitives[0].Corners[0].Color.R;
	}

	struct FFrenchIndex
	{
		explicit FFrenchIndex(const FIso9660& Disc)
		{
			const auto Executable = ReadBootExecutable(Disc);
			REQUIRE(Executable);
			auto Read = FGameFileIndex::Read(*Executable);
			REQUIRE(Read);
			Index = std::move(*Read);
		}

		FGameFileIndex Index;
	};
}

TEST_CASE("The boot notice fades in to 120, holds, then fades out from 128 in 143 blanks", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	auto Notice = FBootNotice::Load(*Executable, Disc->Image(), *Index);
	REQUIRE(Notice);
	CHECK(Notice->DrawList().Primitives.empty());
	int32 Done = 0;
	CHECK(BrightnessAfter(*Notice, Done, 1) == 0);
	CHECK(BrightnessAfter(*Notice, Done, 16) == 120);
	CHECK(BrightnessAfter(*Notice, Done, 126) == 120);
	CHECK(BrightnessAfter(*Notice, Done, 127) == 128);
	CHECK(BrightnessAfter(*Notice, Done, 143) == 0);
	CHECK_FALSE(Notice->Tick(0));

	// The sprite is the 256 x 48 picture at (32, 88), its texels from the kernel's page and CLUT.
	const FDrawPrimitive Sprite = Notice->DrawList().Primitives[0];
	CHECK(Sprite.Corners[0].X == 32.0f);
	CHECK(Sprite.Corners[0].Y == 88.0f);
	CHECK(Sprite.Corners[3].X == 288.0f);
	CHECK(Sprite.Corners[3].Y == 136.0f);
	int32 Drawn = 0;
	for (int32 V = 0; V < 48; ++V)
	{
		for (int32 U = 0; U < 256; ++U)
		{
			Drawn += FetchTexel(Notice->Vram(), Sprite.Texture->Page, Sprite.Texture->Clut, static_cast<uint8>(U), static_cast<uint8>(V)) != 0;
		}
	}
	CHECK(Drawn > 1000);
	CHECK(Drawn < 256 * 48);
}

TEST_CASE("The French translation's boot shows its own 320 x 224 picture as two sprites, held 328 blanks", "[Game][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	auto Notice = FBootNotice::Load(*Executable, Disc->Image(), *Index);
	REQUIRE(Notice);
	CHECK(Notice->Vblanks() == 16 + 328 + 17);
	int32 Done = 0;
	for (; Done < 200; ++Done)
	{
		REQUIRE(Notice->Tick(0));
	}
	const FDrawList List = Notice->DrawList();
	REQUIRE(List.Primitives.size() == 2);
	CHECK(List.Primitives[0].Corners[0].Color.R == 120);
	// A page's width, then the rest: the second's texels are the pixels past 256, from the page beside the first.
	CHECK(List.Primitives[0].Corners[0].X == 0.0f);
	CHECK(List.Primitives[0].Corners[0].Y == 8.0f);
	CHECK(List.Primitives[0].Corners[3].X == 256.0f);
	CHECK(List.Primitives[0].Corners[3].Y == 232.0f);
	CHECK(List.Primitives[1].Corners[0].X == 256.0f);
	CHECK(List.Primitives[1].Corners[3].X == 320.0f);
	for (const FDrawPrimitive& Sprite : List.Primitives)
	{
		const int32 Width = static_cast<int32>(Sprite.Corners[3].X - Sprite.Corners[0].X);
		int32 Drawn = 0;
		for (int32 V = 0; V < 224; ++V)
		{
			for (int32 U = 0; U < Width; ++U)
			{
				Drawn += FetchTexel(Notice->Vram(), Sprite.Texture->Page, Sprite.Texture->Clut, static_cast<uint8>(U), static_cast<uint8>(V)) != 0;
			}
		}
		CHECK(Drawn > Width * 100);
	}
}

TEST_CASE("The French translation's credits fade in, hold until Start or Circle is newly pressed, then fade out", "[Game][Content]")
{
	REQUIRE_PATCHED_DISC(Disc, EDiscRelease::UsaDisc2);
	const FFrenchIndex French(*Disc);
	auto Credits = FBootNotice::LoadTranslationCredits(Disc->Image(), French.Index);
	REQUIRE(Credits);
	// Start held from before the screen, and Cross and Square, never end it.
	int32 Done = 0;
	CHECK(BrightnessAfter(*Credits, Done, 1, StartButton) == 0);
	CHECK(BrightnessAfter(*Credits, Done, 16, StartButton) == 120);
	CHECK(BrightnessAfter(*Credits, Done, 400, StartButton) == 120);
	CHECK(BrightnessAfter(*Credits, Done, 401, StartButton | CrossButton) == 120);
	CHECK(BrightnessAfter(*Credits, Done, 402, SquareButton) == 120);
	// Circle pressed: that blank still holds, then 17 to black from 128.
	CHECK(BrightnessAfter(*Credits, Done, 403, SquareButton | CircleButton) == 120);
	CHECK(Credits->Vblanks() == 16 + 387 + 17);
	CHECK(BrightnessAfter(*Credits, Done, 404) == 128);
	CHECK(BrightnessAfter(*Credits, Done, 420) == 0);
	CHECK(Credits->IsOver());
	CHECK_FALSE(Credits->Tick(0));

	// Start pressed again after its release ends it as well, from the first blank of the hold.
	auto Again = FBootNotice::LoadTranslationCredits(Disc->Image(), French.Index);
	REQUIRE(Again);
	Done = 0;
	CHECK(BrightnessAfter(*Again, Done, 16) == 120);
	CHECK(BrightnessAfter(*Again, Done, 17, StartButton) == 120);
	CHECK(Again->Vblanks() == 16 + 1 + 17);

	// Its own picture, drawn as the logo's: the logo's pixels differ.
	const FDrawList List = Again->DrawList();
	REQUIRE(List.Primitives.size() == 2);
	CHECK(List.Primitives[0].Corners[0].Y == 8.0f);
	CHECK(List.Primitives[1].Corners[3].X == 320.0f);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Logo = FBootNotice::Load(*Executable, Disc->Image(), French.Index);
	REQUIRE(Logo);
	int32 Differing = 0;
	for (int32 V = 0; V < 224; ++V)
	{
		for (int32 U = 0; U < 256; ++U)
		{
			const FDrawTexture& Texture = *List.Primitives[0].Texture;
			Differing += FetchTexel(Again->Vram(), Texture.Page, Texture.Clut, static_cast<uint8>(U), static_cast<uint8>(V))
				!= FetchTexel(Logo->Vram(), Texture.Page, Texture.Clut, static_cast<uint8>(U), static_cast<uint8>(V));
		}
	}
	CHECK(Differing > 256 * 100);
}

TEST_CASE("The French translation's credits come before directory 4's file 2129 is read, on each disc; on no other disc", "[Game][Content]")
{
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2, EDiscRelease::UndubDisc1, EDiscRelease::UndubDisc2})
	{
		INFO(static_cast<int32>(Release));
		REQUIRE_PATCHED_DISC(Disc, Release);
		const FFrenchIndex French(*Disc);
		// The base the undub's own files are counted from, each disc's.
		CHECK(French.Index.TranslationFilesStart() == std::size_t{DiscNumberOfRelease(Release) == 1 ? 4150u : 4145u});
		const FIndexRecord* Cue = FBootNotice::TranslationCreditsCue(French.Index);
		REQUIRE(Cue != nullptr);
		CHECK(Cue == French.Index.Find(4, 2129));
	}
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc2, EDiscRelease::UndubDisc2})
	{
		REQUIRE_DISC(Disc, Release);
		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		CHECK_FALSE(Index->TranslationFilesStart());
		CHECK(FBootNotice::TranslationCreditsCue(*Index) == nullptr);
	}
}
