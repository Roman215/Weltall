#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/IdsFieldSprite.h"
#include "Game/PsxRandom.h"
#include "Graphics/SpriteSystem.h"
#include "Graphics/Texture.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

using namespace Weltall;

// Id's own field sprite, from map 682, for Fei drawn as Id on the field and the world map.
namespace
{
	constexpr int32 Facings = 8;
	constexpr int32 Turn = 0x1000;
	constexpr int32 PartRecords = 0x20;

	FGameFileIndex IndexOf(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return std::move(*Index);
	}

	/** A sprite of File placed at a texture origin, playing in a kernel of its own at one animation tick a step. */
	struct FPlayer
	{
		FPsxRandom Random;
		FSpriteSystem Kernel;
		FSprite Sprite;

		FPlayer(const FSpriteFile& File, int32 TextureX, int32 TextureY)
		{
			Kernel.Random = &Random;
			Kernel.TickExtraFrames = 0;
			Kernel.Create(Sprite, File, 0, FVram::Height, TextureX, TextureY);
			Kernel.SetPartCapacity(Sprite, PartRecords);
		}

		void Play(int32 Animation, int32 Facing)
		{
			Sprite.Facing = static_cast<int16>(Facing * Turn / Facings);
			Kernel.SetAnimation(Sprite, Animation);
			Kernel.BuildPendingFrames();
		}

		void Tick()
		{
			Kernel.TickAnimation(Sprite);
			Kernel.BuildPendingFrames();
		}
	};

	/** The units under each of the frame's parts, from the page its part names, row by row. */
	std::vector<uint16> PartTexels(const FSprite& Sprite, const FVram& Vram)
	{
		std::vector<uint16> Units;
		for (int32 Part = 0; Part < Sprite.PartCount(); ++Part)
		{
			const FSpritePart& Drawn = Sprite.Parts[static_cast<std::size_t>(Part)];
			const FTexturePage Page = FTexturePage::FromPortAttribute(Drawn.TexturePage);
			for (int32 Row = 0; Row < Drawn.Height; ++Row)
			{
				for (int32 Unit = Drawn.U / 4; Unit <= (Drawn.U + Drawn.Width - 1) / 4; ++Unit)
				{
					Units.push_back(Vram.TexelAt(Page.X + Unit, Page.Y + Drawn.V + Row));
				}
			}
		}
		return Units;
	}
}

TEST_CASE("Id's field sprite draws from a page of the port's rows the texels map 682 draws him with", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const FGameFileIndex Index = IndexOf(*Disc);
	const TResult<FFieldMap> Map = LoadFieldMap(Disc->Image(), Index, FIdsFieldSprite::Map);
	REQUIRE(Map);
	const TResult<FIdsFieldSprite> Id = FIdsFieldSprite::Make(*Map);
	REQUIRE(Id);
	TResult<std::vector<FSpriteFile>> Files = ParseFieldSpriteFiles(Map->SpriteSets);
	REQUIRE(Files);
	const FSpriteFile& Own = (*Files)[FIdsFieldSprite::MapSprite];
	const FFieldSpriteSlot& Slot = Map->SpriteSlots[FIdsFieldSprite::MapSprite];

	const FSpriteFile& Party = Id->File();
	CHECK(Party.IsResident(Party.Tables()));
	CHECK(Party.FrameCount(Party.Tables()) == 55);
	CHECK(Party.AnimationCount(Party.Tables()) == 15);
	// One page column of 4-bit texels, as deep as his cells reach.
	CHECK(Id->Picture().Rect == FVramRect{FIdsFieldSprite::TextureX, FIdsFieldSprite::TextureY, 64, 187});

	auto MapVram = std::make_unique<FVram>();
	UploadFieldTextures(*Map, *MapVram, false);
	auto PortVram = std::make_unique<FVram>();
	PortVram->UploadToPortRows(Id->Picture());

	// His standing, his walk, his bowed head and his crossed arms: every one of his frames, drawn both ways.
	const std::vector<std::pair<int32, int32>> Same{{0, 0}, {1, 1}, {11, 2}, {10, 3}};
	int32 Compared = 0;
	for (const auto& [Drawn, His] : Same)
	{
		for (int32 Facing = 0; Facing < Facings; ++Facing)
		{
			FPlayer AsMap(Own, Slot.X, Slot.Y);
			FPlayer AsParty(Party, FIdsFieldSprite::TextureX, FIdsFieldSprite::TextureY);
			AsMap.Play(His, Facing);
			AsParty.Play(Drawn, Facing);
			for (int32 Tick = 0; Tick < 48; ++Tick)
			{
				AsMap.Tick();
				AsParty.Tick();
				INFO("animation " << Drawn << " facing " << Facing << " tick " << Tick);
				REQUIRE(AsParty.Sprite.Frame == AsMap.Sprite.Frame);
				REQUIRE(AsParty.Sprite.PartCount() == AsMap.Sprite.PartCount());
				for (int32 Part = 0; Part < AsParty.Sprite.PartCount(); ++Part)
				{
					CHECK(FTexturePage::FromPortAttribute(AsParty.Sprite.Parts[static_cast<std::size_t>(Part)].TexturePage).Y == FVram::Height);
				}
				const std::vector<uint16> Texels = PartTexels(AsParty.Sprite, *PortVram);
				CHECK(Texels == PartTexels(AsMap.Sprite, *MapVram));
				Compared += Texels.empty() ? 0 : 1;
			}
		}
	}
	CHECK(Compared == 4 * Facings * 48);
}

TEST_CASE("Id's field sprite runs on his walk, jumps with his head bowed and climbs on his walk", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const TResult<FIdsFieldSprite> Id = FIdsFieldSprite::Load(Disc->Image(), IndexOf(*Disc));
	REQUIRE(Id);
	// Facing 0 is side on: his walk 24-31 from its first stride, his head bowed 53.
	constexpr int32 Side = 0;
	constexpr int32 FirstStride = 24;
	constexpr int32 Bowed = 53;
	const auto Frames = [&Id](int32 Animation, int32 Ticks)
	{
		FPlayer Player(Id->File(), FIdsFieldSprite::TextureX, FIdsFieldSprite::TextureY);
		Player.Play(Animation, Side);
		std::vector<int32> Shown;
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Player.Tick();
			Shown.push_back(Player.Sprite.Frame);
		}
		return Shown;
	};
	const auto Stepping = [](int32 Wait, int32 Ticks)
	{
		std::vector<int32> Shown;
		for (int32 Tick = 1; Tick <= Ticks; ++Tick)
		{
			Shown.push_back(FirstStride + (Tick - 1) / Wait % 8);
		}
		return Shown;
	};
	CHECK(Frames(1, 96) == Stepping(6, 96));
	CHECK(Frames(2, 48) == Stepping(3, 48));
	CHECK(Frames(6, 112) == Stepping(7, 112));
	CHECK(Frames(0, 8) == std::vector<int32>(8, 32));

	// The jump: bowed at the take-off, a stride held through the rise and the fall until he lands, bowed again.
	FPlayer Jumper(Id->File(), FIdsFieldSprite::TextureX, FIdsFieldSprite::TextureY);
	Jumper.Play(3, Side);
	Jumper.Sprite.FloorY = 0;
	Jumper.Sprite.Velocity[1] = -0x10000;
	Jumper.Sprite.Position[1] = -0x200000;
	for (int32 Tick = 0; Tick < 40; ++Tick)
	{
		Jumper.Tick();
		CHECK(Jumper.Sprite.Frame == (Tick < 4 ? Bowed : FirstStride + 4));
	}
	Jumper.Sprite.Velocity[1] = 0x10000;
	for (int32 Tick = 0; Tick < 20; ++Tick)
	{
		Jumper.Tick();
		CHECK(Jumper.Sprite.Frame == FirstStride + 4);
	}
	Jumper.Sprite.Position[1] = 0;
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		Jumper.Tick();
		CHECK(Jumper.Sprite.Frame == Bowed);
	}
	// The jump's end plays the next animation, his standing, as every member's does.
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		Jumper.Tick();
	}
	CHECK(Jumper.Sprite.Frame == 32);
}

TEST_CASE("Id's field sprite is the same on both discs", "[Field][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const TResult<FIdsFieldSprite> FromFirst = FIdsFieldSprite::Load(First->Image(), IndexOf(*First));
	const TResult<FIdsFieldSprite> FromSecond = FIdsFieldSprite::Load(Second->Image(), IndexOf(*Second));
	REQUIRE(FromFirst);
	REQUIRE(FromSecond);
	const std::span<const uint8> A = FromFirst->File().From(0);
	const std::span<const uint8> B = FromSecond->File().From(0);
	CHECK(std::equal(A.begin(), A.end(), B.begin(), B.end()));
	CHECK(FromFirst->Picture().Units == FromSecond->Picture().Units);
}
