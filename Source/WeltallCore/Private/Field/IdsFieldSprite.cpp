#include "Field/IdsFieldSprite.h"

#include "Field/FieldMap.h"
#include "Field/FieldSpriteFiles.h"
#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <array>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** His file as map 682 has it: resident, 55 frames, five animations of five facings' lists. */
		constexpr int32 OwnFrames = 55;
		constexpr int32 OwnAnimations = 5;
		constexpr uint32 Facings = 5;
		constexpr uint32 FacingListsMode = 2;
		/** His animations whose lists give each facing's pose: standing, his walk's first stride, his head bowed, his arms crossed. */
		constexpr int32 HisStand = 0;
		constexpr int32 HisWalk = 1;
		constexpr int32 HisBow = 2;
		constexpr int32 HisArmsCrossed = 3;
		/** His walk steps through eight frames from its list's; the fifth, legs apart, is the one held in the air. */
		constexpr uint16 StrideFrame = 4;
		constexpr uint8 KernelCellBit = 0x10;
		constexpr int16 PageColumns = 0x40;

		/** The script bytes: a list's next frame or the next frame, with the wait in animation ticks; the waits on the fall and the landing; End, Hold, Restart. */
		constexpr uint8 NextListFrame(uint8 Wait) { return static_cast<uint8>(0x10 | (Wait - 1)); }
		constexpr uint8 NextFrame(uint8 Wait) { return static_cast<uint8>(Wait - 1); }
		constexpr uint8 WaitFalling = 0x86;
		constexpr uint8 WaitLanding = 0x87;
		constexpr uint8 End = 0x80;
		constexpr uint8 Hold = 0x81;
		constexpr uint8 Restart = 0x82;

		/** Each facing's pose, by the poses his own animations show. */
		enum class EPose : uint8
		{
			Stand,
			Stride,
			Airborne,
			Bow,
			ArmsCrossed,
		};

		/** One animation of the new table: the poses its facing's list holds, and its script. */
		struct FAnimation
		{
			std::vector<EPose> Poses;
			std::vector<uint8> Script;
		};

		/** A walk's eight frames from its list's, at Wait ticks each, then again. */
		std::vector<uint8> Stepping(uint8 Wait)
		{
			std::vector<uint8> Script(9, NextFrame(Wait));
			Script.front() = NextListFrame(Wait);
			Script.back() = Restart;
			return Script;
		}

		/** The animations the party's sprites number alike, made of his poses. */
		std::array<FAnimation, 15> PartyAnimations()
		{
			const FAnimation Stand{{EPose::Stand}, {NextListFrame(4), Hold}};
			// The take-off and the landing with his head bowed, a stride held through the rise and the fall.
			const FAnimation Jump{{EPose::Bow, EPose::Airborne, EPose::Bow}, {NextListFrame(4), NextListFrame(8), WaitFalling, WaitLanding, NextListFrame(4), End}};
			return {{
				Stand,
				{{EPose::Stride}, Stepping(6)},
				// Twice the walk's pace; the run covers ground over three times as fast.
				{{EPose::Stride}, Stepping(3)},
				Jump,
				// A nod, and its first half held.
				{{EPose::Stand, EPose::Bow, EPose::Bow, EPose::Bow, EPose::Stand}, {NextListFrame(6), NextListFrame(6), NextListFrame(6), NextListFrame(6), NextListFrame(6), Hold}},
				Stand,
				// The climb's cycle of 56 ticks, which runs only while the climb moves.
				{{EPose::Stride}, Stepping(7)},
				Stand,
				Stand,
				Jump,
				{{EPose::ArmsCrossed}, {NextListFrame(4), Hold}},
				{{EPose::Bow}, {NextListFrame(4), Hold}},
				{{EPose::Stand, EPose::Bow, EPose::Bow}, {NextListFrame(6), NextListFrame(6), NextListFrame(6), Hold}},
				Stand,
				Stand,
			}};
		}

		/** The first entry of facing Facing's list in his animation Animation. */
		uint16 ListFrame(const FSpriteFile& File, int32 Animation, uint32 Facing)
		{
			const uint32 At = File.AnimationHeader(File.Tables(), Animation) + 4 + 2 * Facing;
			return File.Word(At + File.Word(At));
		}

		uint16 PoseFrame(const FSpriteFile& File, EPose Pose, uint32 Facing)
		{
			switch (Pose)
			{
			case EPose::Stand:
				return ListFrame(File, HisStand, Facing);
			case EPose::Stride:
				return ListFrame(File, HisWalk, Facing);
			case EPose::Airborne:
				return static_cast<uint16>(ListFrame(File, HisWalk, Facing) + StrideFrame);
			case EPose::Bow:
				return ListFrame(File, HisBow, Facing);
			case EPose::ArmsCrossed:
				return ListFrame(File, HisArmsCrossed, Facing);
			}
			return 0;
		}

		/** His file with the party's animations in a table of their own after its bytes, its header pointed at it. */
		TResult<FSpriteFile> WithPartyAnimations(const FSpriteFile& Own)
		{
			const std::span<const uint8> Bytes = Own.From(0);
			std::vector<uint8> Data(Bytes.begin(), Bytes.end());
			Data.resize((Data.size() + 1) & ~std::size_t{1});
			const std::array<FAnimation, 15> Animations = PartyAnimations();
			const uint16 Flags = Own.Word(Own.AnimationHeader(Own.Tables(), HisWalk));
			const uint32 Table = static_cast<uint32>(Data.size());
			Data.resize(Table + 2 + 2 * Animations.size());
			LittleEndian::WriteU16(Data, Table, static_cast<uint16>((Own.Word(Own.Tables().Animations) & ~0x3F) | Animations.size()));
			for (std::size_t Number = 0; Number < Animations.size(); ++Number)
			{
				// A header, its facings' lists and its script, all after it: the kernel's offsets only count forward.
				const FAnimation& Animation = Animations[Number];
				const uint32 Header = static_cast<uint32>(Data.size());
				const uint32 Lists = Header + 4 + 2 * Facings;
				const uint32 Script = Lists + 2 * Facings * static_cast<uint32>(Animation.Poses.size());
				Data.resize(Script + Animation.Script.size());
				Data.resize((Data.size() + 1) & ~std::size_t{1});
				LittleEndian::WriteU16(Data, Table + 2 + 2 * static_cast<uint32>(Number), static_cast<uint16>(Header - Table));
				LittleEndian::WriteU16(Data, Header, Flags);
				LittleEndian::WriteU16(Data, Header + 2, static_cast<uint16>(Script - (Header + 2)));
				for (uint32 Facing = 0; Facing < Facings; ++Facing)
				{
					const uint32 List = Lists + 2 * Facing * static_cast<uint32>(Animation.Poses.size());
					LittleEndian::WriteU16(Data, Header + 4 + 2 * Facing, static_cast<uint16>(List - (Header + 4 + 2 * Facing)));
					for (std::size_t Entry = 0; Entry < Animation.Poses.size(); ++Entry)
					{
						LittleEndian::WriteU16(Data, List + 2 * static_cast<uint32>(Entry), PoseFrame(Own, Animation.Poses[Entry], Facing));
					}
				}
				std::copy(Animation.Script.begin(), Animation.Script.end(), Data.begin() + Script);
			}
			LittleEndian::WriteU32(Data, 4, Table);
			return FSpriteFile::Parse(Data);
		}
	}

	TResult<FIdsFieldSprite> FIdsFieldSprite::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		TResult<FFieldMap> Loaded = LoadFieldMap(Image, Index, Map);
		if (!Loaded)
		{
			return Loaded.Error();
		}
		return Make(*Loaded);
	}

	TResult<FIdsFieldSprite> FIdsFieldSprite::Make(const FFieldMap& Loaded)
	{
		TResult<std::vector<FSpriteFile>> Files = ParseFieldSpriteFiles(Loaded.SpriteSets);
		if (!Files)
		{
			return Files.Error();
		}
		if (Files->size() <= MapSprite)
		{
			return MakeError("map 682 lacks Id's field sprite");
		}
		const FSpriteFile& Own = (*Files)[MapSprite];
		const FSpriteTables& Tables = Own.Tables();
		const FFieldSpriteSlot& Slot = Loaded.SpriteSlots[MapSprite];
		if (!Own.IsResident(Tables) || Own.FrameCount(Tables) != OwnFrames || Own.AnimationCount(Tables) != OwnAnimations
			|| (Own.Word(Own.AnimationHeader(Tables, HisWalk)) & 3) != FacingListsMode || (Slot.X & (PageColumns - 1)) != (TextureX & (PageColumns - 1))
			|| (Slot.Y & 0xFF) != (TextureY & 0xFF))
		{
			return MakeError("map 682's fourth sprite is not Id's field sprite as Weltall knows it");
		}

		// His cells' extent in the map's texture: page columns from the slot's and rows from its top.
		int32 Columns = 1;
		int32 Rows = 0;
		for (int32 Number = 1; Number <= Own.FrameCount(Tables); ++Number)
		{
			const uint32 At = Own.Frame(Tables, Number);
			const uint32 Parts = Own.Byte(At) & 0x3Fu;
			for (uint32 Part = 0; Part < Parts; ++Part)
			{
				const uint32 Cell = Tables.Frames + Own.Word(At + 4 + 2 * Part);
				const uint8 CellFlags = Own.Byte(Cell);
				if ((CellFlags & KernelCellBit) != 0)
				{
					return MakeError("Id's field sprite draws from the kernel's own textures");
				}
				Columns = std::max(Columns, (CellFlags >> 1 & 7) + 1);
				Rows = std::max(Rows, Own.Byte(Cell + 2) + Own.Byte(Cell + 4));
			}
		}
		FVram Textures;
		UploadFieldTextures(Loaded, Textures, false);
		FVramImage Picture;
		Picture.Rect = {TextureX, TextureY, static_cast<int16>(PageColumns * Columns), static_cast<int16>(Rows)};
		Picture.Units.reserve(static_cast<std::size_t>(Picture.Rect.Width) * Picture.Rect.Height);
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Picture.Rect.Width; ++Column)
			{
				Picture.Units.push_back(Textures.At(Slot.X + Column, Slot.Y + Row));
			}
		}

		TResult<FSpriteFile> Edited = WithPartyAnimations(Own);
		if (!Edited)
		{
			return Edited.Error();
		}
		return FIdsFieldSprite(std::move(*Edited), std::move(Picture));
	}
}
