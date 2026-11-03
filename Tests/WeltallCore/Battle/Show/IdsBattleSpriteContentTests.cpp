#include "Battle/Show/BattleSceneGraphics.h"
#include "Battle/Show/IdsBattleSprite.h"
#include "Graphics/SpriteFile.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <utility>
#include <vector>

using namespace Weltall;

// Id's own battle sprite, from scene 51's graphics (his fight in the Zeboim ruins), for Fei drawn as Id.
namespace
{
	TResult<FIdsBattleSprite> IdsSprite(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		return FIdsBattleSprite::Load(Disc.Image(), *Index);
	}

	uint32 ScriptOf(const FSpriteFile& Sprite, int32 Animation)
	{
		const uint32 Header = Sprite.AnimationHeader(Sprite.Tables(), Animation);
		return Header + 2 + Sprite.Word(Header + 2);
	}
}

TEST_CASE("Id's battle sprite is his fight's, his poses named where the battle plays its own numbers, his images in the port's rows", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const TResult<FIdsBattleSprite> Id = IdsSprite(*Disc);
	REQUIRE(Id);
	const FSpriteFile& Sprite = Id->File().Sprite();
	const FSpriteTables& Tables = Sprite.Tables();
	CHECK(Sprite.IsResident(Tables));
	CHECK(Sprite.FrameCount(Tables) == 154);
	CHECK(Sprite.AnimationCount(Tables) == 37);

	// The scene's own file, which his fight plays.
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const TResult<FBattleSceneGraphics> Scene = FBattleSceneGraphics::Load(Disc->Image(), *Index, FIdsBattleSprite::Scene);
	REQUIRE(Scene);
	const TResult<FBattleSpriteFile> Own = FBattleSpriteFile::Parse(Scene->Bytes().subspan(Scene->Entries()[0].Data, Scene->KeptEnd() - Scene->Entries()[0].Data));
	REQUIRE(Own);
	const FSpriteFile& Fight = Own->Sprite();
	const auto Header = [](const FSpriteFile& File, int32 Animation) { return File.AnimationHeader(File.Tables(), Animation); };
	// His stance, his flips to a place and home, his three combos and his plain attack are as in his fight.
	for (const int32 Kept : {0, 1, 3, 4, 6, 7, 8, 14, 16, 17, 0x1D, 0x1E, 0x1F, 0x20})
	{
		INFO("animation " << Kept);
		CHECK(Header(Sprite, Kept) == Header(Fight, Kept));
	}
	// Where the battle plays a number his fight never needed, his nearest pose.
	const std::vector<std::pair<int32, int32>> Nearest{{2, 3}, {5, 4}, {10, 8}, {18, 17}, {19, 17}, {22, 3}, {23, 3}, {24, 17}, {27, 12}, {28, 17}};
	for (const auto& [Battle, His] : Nearest)
	{
		INFO("animation " << Battle);
		CHECK(Header(Sprite, Battle) == Header(Fight, His));
	}
	// The weak pose holds his crouch; the collapse calls his fall as he is knocked down, then holds.
	const uint32 Crouch = ScriptOf(Fight, 6);
	const uint32 Weak = ScriptOf(Sprite, 20);
	CHECK(Sprite.Byte(Weak) == Fight.Byte(Crouch));
	CHECK(Sprite.Word(Weak + 1) == Fight.Word(Crouch + 1));
	CHECK(Sprite.Byte(Weak + 3) == 0x81);
	const uint32 Fall = ScriptOf(Fight, 14);
	const uint32 FallBody = Fall + static_cast<uint32>(static_cast<int16>(Fight.Word(Fall + 1)));
	const uint32 Collapse = ScriptOf(Sprite, 21);
	CHECK(Sprite.Byte(Collapse) == 0xE2);
	CHECK(Collapse + static_cast<uint32>(static_cast<int16>(Sprite.Word(Collapse + 1))) == FallBody);
	CHECK(Sprite.Byte(Collapse + 3) == 0x81);

	// Four images side by side from the port rows' first column, as the scene's setup places them from its own.
	REQUIRE(Id->Images().size() == 4);
	for (std::size_t Image = 0; Image < 4; ++Image)
	{
		const FVramRect& Rect = Id->Images()[Image].Rect;
		CHECK(Rect.X == 0x40 * static_cast<int32>(Image));
		CHECK(Rect.Y == FVram::Height);
		CHECK(Rect.Width == 64);
		CHECK(Rect.Height == (Image < 3 ? 256 : 82));
	}
}

TEST_CASE("Id's battle sprite is the same on both discs", "[Battle][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const TResult<FIdsBattleSprite> FromFirst = IdsSprite(*First);
	const TResult<FIdsBattleSprite> FromSecond = IdsSprite(*Second);
	REQUIRE(FromFirst);
	REQUIRE(FromSecond);
	REQUIRE(FromFirst->File().Sprite().Size() == FromSecond->File().Sprite().Size());
	for (uint32 Byte = 0; Byte < FromFirst->File().Sprite().Size(); ++Byte)
	{
		if (FromFirst->File().Sprite().Byte(Byte) != FromSecond->File().Sprite().Byte(Byte))
		{
			FAIL("byte " << Byte << " differs");
		}
	}
	for (std::size_t Image = 0; Image < FromFirst->Images().size(); ++Image)
	{
		CHECK(FromFirst->Images()[Image].Units == FromSecond->Images()[Image].Units);
	}
}

TEST_CASE("A party record's attack codes play Id's plain attack and his combos", "[Battle]")
{
	for (uint8 Code = 0; Code < 7; ++Code)
	{
		CHECK(FIdsBattleSprite::AttackAnimation(Code) == 0x20);
	}
	for (const uint8 Code : {uint8{7}, uint8{8}, uint8{9}})
	{
		CHECK(FIdsBattleSprite::AttackAnimation(Code) == 0x1D);
	}
	for (const uint8 Code : {uint8{10}, uint8{11}, uint8{12}})
	{
		CHECK(FIdsBattleSprite::AttackAnimation(Code) == 0x1E);
	}
	CHECK(FIdsBattleSprite::AttackAnimation(13) == 0x1F);
}
