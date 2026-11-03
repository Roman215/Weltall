#include "Battle/Show/IdsBattleSprite.h"

#include "Battle/Show/BattleSceneGraphics.h"
#include "Foundation/LittleEndian.h"

#include <array>
#include <span>
#include <utility>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr int32 SceneEntry = 0;
		constexpr int16 ImageColumns = 0x40;
		/** A number the battle plays that his file names his nearest animation for, and his. */
		struct FNearest
		{
			uint8 Battle = 0;
			uint8 His = 0;
		};
		constexpr std::array<FNearest, 10> Nearest{{
			{2, 3},   // the run to a waypoint: his leap
			{5, 4},   // the escape: his flip home
			{10, 8},  // the flinch
			{18, 17}, // the wait on an effect: his raised hand
			{19, 17}, // an item's use
			{22, 3},  // boarding, and out of the battle
			{23, 3},  // the run-in
			{24, 17}, // the victory
			{27, 12}, // a counterattack: his kick
			{28, 17}, // his own cast
		}};
		/** The weak pose and the collapse, which hold: his crouch and his fall as he is knocked down, held. */
		constexpr uint8 Weak = 20;
		constexpr uint8 Collapse = 21;
		constexpr uint8 Crouch = 6;
		constexpr uint8 KnockedDown = 14;
		constexpr uint8 PlainAttack = 0x20;
		constexpr uint8 QuickCombo = 0x1D;
		constexpr uint8 SecondQuickCombo = 0x1E;
		constexpr uint8 BigCombo = 0x1F;
		constexpr uint8 End = 0x80;
		constexpr uint8 Hold = 0x81;
		constexpr uint8 SetFrame = 0xBE;
		constexpr uint8 Call = 0xE2;

		uint32 ScriptOf(const FSpriteFile& Sprite, uint32 Header)
		{
			return Header + 2 + Sprite.Word(Header + 2);
		}

		/**
		 * A new animation at the end of Data, with Source's flags (one frame list) and the first entries of its list copied
		 * after the header, since a list lies after its offset; Script, made for where it lands, runs it. Its header's place.
		 */
		template <typename TScript>
		uint32 Append(std::vector<uint8>& Data, const FSpriteFile& Sprite, uint32 Source, std::size_t ScriptSize, TScript&& MakeScript)
		{
			constexpr uint32 ListEntries = 2;
			Data.resize((Data.size() + 1) & ~std::size_t{1});
			const uint32 Header = static_cast<uint32>(Data.size());
			const uint32 SourceList = Source + 4 + Sprite.Word(Source + 4);
			const uint32 List = Header + 6;
			const uint32 Script = List + 2 * ListEntries;
			Data.resize(Script + ScriptSize);
			LittleEndian::WriteU16(Data, Header, Sprite.Word(Source));
			LittleEndian::WriteU16(Data, Header + 2, static_cast<uint16>(Script - (Header + 2)));
			LittleEndian::WriteU16(Data, Header + 4, static_cast<uint16>(List - (Header + 4)));
			for (uint32 Entry = 0; Entry < ListEntries; ++Entry)
			{
				LittleEndian::WriteU16(Data, List + 2 * Entry, Sprite.Word(SourceList + 2 * Entry));
			}
			MakeScript(std::span<uint8>(Data).subspan(Script, ScriptSize), Script);
			return Header;
		}
	}

	TResult<FIdsBattleSprite> FIdsBattleSprite::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<FBattleSceneGraphics> Graphics = FBattleSceneGraphics::Load(Image, Index, Scene);
		if (!Graphics)
		{
			return Graphics.Error();
		}
		return Make(*Graphics);
	}

	TResult<FIdsBattleSprite> FIdsBattleSprite::Make(const FBattleSceneGraphics& Graphics)
	{
		const std::vector<FBattleSceneEntry>& Entries = Graphics.Entries();
		if (Entries.empty() || !Entries[SceneEntry].IsSprite() || !Entries[SceneEntry].HasOwnImages() || Entries[SceneEntry].Data <= 7)
		{
			return MakeError("scene 51's first entry is not a sprite with images of its own");
		}
		const FBattleSceneEntry& Entry = Entries[SceneEntry];
		const std::span<const uint8> Bytes = Graphics.Bytes();
		// The entry's file runs to the next entry's, or to the end of the data the battle keeps.
		uint32 FileEnd = Graphics.KeptEnd();
		for (const FBattleSceneEntry& Other : Entries)
		{
			FileEnd = Other.Data > Entry.Data && Other.Data < FileEnd ? Other.Data : FileEnd;
		}
		if (FileEnd > Bytes.size() || FileEnd <= Entry.Data)
		{
			return MakeError("scene 51's first sprite runs past its graphics");
		}
		std::vector<uint8> Data(Bytes.begin() + Entry.Data, Bytes.begin() + FileEnd);
		const TResult<FBattleSpriteFile> Own = FBattleSpriteFile::Parse(Data);
		if (!Own)
		{
			return Own.Error();
		}
		const FSpriteFile& Sprite = Own->Sprite();
		const FSpriteTables& Tables = Sprite.Tables();
		if (!Sprite.IsResident(Tables) || Sprite.AnimationCount(Tables) <= PlainAttack)
		{
			return MakeError("Id's battle sprite lacks the animations of his fight");
		}
		const uint32 CrouchScript = ScriptOf(Sprite, Sprite.AnimationHeader(Tables, Crouch));
		const uint32 FallScript = ScriptOf(Sprite, Sprite.AnimationHeader(Tables, KnockedDown));
		if (Sprite.Byte(CrouchScript) != SetFrame || Sprite.Byte(CrouchScript + 3) != End || Sprite.Byte(FallScript) != Call || Sprite.Byte(FallScript + 3) != End)
		{
			return MakeError("Id's battle sprite's crouch and fall are not the ones Weltall knows");
		}

		const auto Point = [&Data, &Tables](uint8 Animation, uint32 Header)
		{
			LittleEndian::WriteU16(Data, Tables.Animations + 2 + 2 * static_cast<uint32>(Animation), static_cast<uint16>(Header - Tables.Animations));
		};
		for (const FNearest& Pose : Nearest)
		{
			Point(Pose.Battle, Sprite.AnimationHeader(Tables, Pose.His));
		}
		Point(Weak, Append(Data, Sprite, Sprite.AnimationHeader(Tables, Crouch), 4,
			[&Sprite, CrouchScript](std::span<uint8> Script, uint32)
			{
				for (uint32 Byte = 0; Byte < 3; ++Byte)
				{
					Script[Byte] = Sprite.Byte(CrouchScript + Byte);
				}
				Script[3] = Hold;
			}));
		// The fall is a call of its body; called from the new script's place, then held.
		const uint32 FallBody = FallScript + static_cast<uint32>(static_cast<int16>(Sprite.Word(FallScript + 1)));
		Point(Collapse, Append(Data, Sprite, Sprite.AnimationHeader(Tables, KnockedDown), 4,
			[FallBody](std::span<uint8> Script, uint32 At)
			{
				Script[0] = Call;
				LittleEndian::WriteU16(Script, 1, static_cast<uint16>(FallBody - At));
				Script[3] = Hold;
			}));
		TResult<FBattleSpriteFile> Edited = FBattleSpriteFile::Parse(Data);
		if (!Edited)
		{
			return Edited.Error();
		}

		const TResult<std::vector<FBattleImageSetImage>> Set = ReadBattleImageSet(Bytes, Entry.Images);
		if (!Set)
		{
			return Set.Error();
		}
		std::vector<FVramImage> Pictures;
		for (std::size_t Number = 0; Number < Set->size(); ++Number)
		{
			const FBattleImageSetImage& Read = (*Set)[Number];
			FVramImage& Picture = Pictures.emplace_back();
			Picture.Rect = {static_cast<int16>(TextureX + ImageColumns * static_cast<int32>(Number)), TextureY, static_cast<int16>(Read.Width), static_cast<int16>(Read.Height)};
			Picture.Units.resize(static_cast<std::size_t>(Read.Width) * Read.Height);
			for (std::size_t Unit = 0; Unit < Picture.Units.size(); ++Unit)
			{
				Picture.Units[Unit] = LittleEndian::ReadU16(Bytes, Read.Units + 2 * Unit);
			}
		}
		return FIdsBattleSprite(std::move(*Edited), std::move(Pictures));
	}

	uint8 FIdsBattleSprite::AttackAnimation(uint8 Code)
	{
		constexpr uint8 FirstDeathblow = 7;
		constexpr uint8 FirstSixApDeathblow = 10;
		constexpr uint8 DoubleFierce = 13;
		return Code < FirstDeathblow ? PlainAttack : Code < FirstSixApDeathblow ? QuickCombo : Code < DoubleFierce ? SecondQuickCombo : Code == DoubleFierce ? BigCombo : PlainAttack;
	}
}
