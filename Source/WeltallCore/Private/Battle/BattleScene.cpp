#include "Battle/BattleScene.h"

#include "Battle/BattleRecordBytes.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"
#include "Text/MessageOverrides.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** The scenes' directory: scene n's enemy file is file 2n + 2, its pictures 2n + 3. */
		constexpr int32 SceneDirectory = 13;
		/** The stages' directory: stage n's model is file 2n + 6, its layout 2n + 7, in a group of files at file 5. */
		constexpr int32 StageDirectory = 15;
		constexpr int32 StageGroupFile = 5;

		constexpr std::size_t SceneRecordsOffset = 0x32;
		constexpr std::size_t ScriptBlockSize = 8;

		/** The stage's layout starts after a word at the head of its file. */
		constexpr std::size_t StageLayoutOffset = 4;
		constexpr std::size_t StageGroupSize = 0x20;
		constexpr std::size_t StageGearPlacesOffset = 0x100;
		constexpr std::size_t StageRoutesOffset = 0x140;

		TResult<std::vector<uint8>> ReadFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number)
		{
			const FIndexRecord* Record = Index.Find(Directory, Number);
			if (Record == nullptr || !Record->IsFile())
			{
				return MakeError("the disc lacks file " + std::to_string(Number) + " of directory " + std::to_string(Directory));
			}
			return ReadFileBytes(Image, *Record);
		}

		/** A scene file's word naming where its name table is: its enemies' names, then what their scripts say. */
		constexpr std::size_t NameTableOffset = 0x30;

		void RepairGElementsNameTable(std::span<uint8> File)
		{
			// Perfect Works Story Mode with revised text duplicates the table's first byte without moving its offset.
			constexpr uint16 TableStart = 0x14FA;
			constexpr int32 EntryCount = 64;
			if (File.size() != 6030 || LittleEndian::ReadU16(File, NameTableOffset) != TableStart
				|| LittleEndian::ReadU16(File, TableStart) != 0x3F3F)
			{
				return;
			}
			const auto Table = FMessageTable::Parse(File.subspan(TableStart + 1));
			if (!Table || Table->Count() != EntryCount)
			{
				return;
			}
			for (int32 Entry = 0; Entry < EntryCount; ++Entry)
			{
				if (Table->StartsPastBytes(Entry))
				{
					return;
				}
			}
			LittleEndian::WriteU16(File, NameTableOffset, TableStart + 1);
		}

		/** The scene's file with the player's names and messages in place: a changed table after the file's end, its word moved there. */
		void UsePlayersNames(const FCdImage& Image, int32 Scene, std::vector<uint8>& File)
		{
			const uint16 At = File.size() >= NameTableOffset + 2 ? LittleEndian::ReadU16(File, NameTableOffset) : 0;
			if (At == 0 || At >= File.size())
			{
				return;
			}
			const std::string Table = MessageTables::Enemies(Scene);
			std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, Table, std::span<const uint8>(File).subspan(At), EMessageWindows::Program);
			if (!Applied)
			{
				return;
			}
			const std::size_t End = File.size();
			if (End > 0xFFFF)
			{
				Applied->Replaced.Overrides->ReportOnce(Table, Applied->Replaced.Messages.front(), "its scene's file is too large to take it: the disc's text is kept");
				return;
			}
			File.insert(File.end(), Applied->Table.begin(), Applied->Table.end());
			LittleEndian::WriteU16(File, NameTableOffset, static_cast<uint16>(End));
		}
	}

	FBattleFormation FBattleFormation::FromRecord(std::span<const uint8, RecordSize> Record)
	{
		FBattleFormation Formation;
		Formation.Scene = Record[0x00];
		Formation.Flags = Record[0x01];
		Formation.Stage = Record[0x02];
		Formation.EventScript = Record[0x03];
		for (std::size_t Slot = 0; Slot < Formation.PartyGroups.size(); ++Slot)
		{
			Formation.PartyGroups[Slot] = Record[0x04 + Slot];
		}
		for (std::size_t Enemy = 0; Enemy < Formation.EnemyTypes.size(); ++Enemy)
		{
			Formation.EnemyTypes[Enemy] = Record[0x08 + Enemy];
			Formation.EnemyFlags[Enemy] = Record[0x10 + Enemy];
			Formation.EnemyGroups[Enemy] = Record[0x18 + Enemy];
		}
		return Formation;
	}

	TResult<FBattleScene> FBattleScene::Parse(std::vector<uint8> File)
	{
		if (File.size() < SceneRecordsOffset + TypeCount * FBattleCombatant::RecordSize)
		{
			return MakeError("a battle scene's file is too short for its eight enemy records");
		}
		FBattleScene Scene;
		for (int32 Type = 0; Type < TypeCount; ++Type)
		{
			const std::size_t Record = SceneRecordsOffset + static_cast<std::size_t>(Type) * FBattleCombatant::RecordSize;
			Scene.Enemies[static_cast<std::size_t>(Type)] = FBattleCombatant::FromRecord(std::span<const uint8>(File).subspan(Record).first<FBattleCombatant::RecordSize>());
			const uint16 Block = LittleEndian::ReadU16(File, 2 * static_cast<std::size_t>(Type));
			if (Block + ScriptBlockSize > File.size())
			{
				return MakeError("a battle scene's enemy type " + std::to_string(Type) + " has its scripts past the file's end");
			}
			Scene.ScriptBlocks[static_cast<std::size_t>(Type)] = Block;
		}
		Scene.File = std::move(File);
		return Scene;
	}

	TResult<FBattleScene> FBattleScene::Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
	{
		TResult<std::vector<uint8>> File = ReadFile(Image, Index, SceneDirectory, 2 * Number + 2);
		if (!File)
		{
			return File.Error();
		}
		if (Number == 55)
		{
			RepairGElementsNameTable(*File);
		}
		UsePlayersNames(Image, Number, *File);
		return Parse(std::move(*File));
	}

	uint16 FBattleScene::ScriptOffset(int32 Type, int32 Script) const
	{
		return LittleEndian::ReadU16(File, ScriptBlock(Type) + 2 * static_cast<std::size_t>(Script));
	}

	TResult<FBattleStage> FBattleStage::Parse(std::span<const uint8> File)
	{
		if (File.size() < StageLayoutOffset + LayoutSize)
		{
			return MakeError("a battle stage's layout file is too short");
		}
		FBattleStage Stage;
		std::copy_n(File.begin() + StageLayoutOffset, LayoutSize, Stage.Layout.begin());
		return Stage;
	}

	FBattleStagePoint FBattleStage::Point(std::size_t Offset) const
	{
		return {LittleEndian::ReadU16(Layout, Offset), LittleEndian::ReadU16(Layout, Offset + 2)};
	}

	FBattleStagePoint FBattleStage::GroupPoint(int32 Group) const
	{
		return Point(static_cast<std::size_t>(Group) * StageGroupSize);
	}

	FBattleStagePoint FBattleStage::PartyPlace(int32 Group, int32 Place) const
	{
		return Point(static_cast<std::size_t>(Group) * StageGroupSize + static_cast<std::size_t>(Place) * 4 + 4);
	}

	FBattleStagePoint FBattleStage::EnemyPlace(int32 Group, int32 Place) const
	{
		return Point(static_cast<std::size_t>(Group) * StageGroupSize + static_cast<std::size_t>(Place) * 4 + 0x10);
	}

	FBattleStagePoint FBattleStage::PartyGearPlace(int32 Group) const
	{
		return Point(StageGearPlacesOffset + static_cast<std::size_t>(Group) * 8);
	}

	FBattleStagePoint FBattleStage::EnemyGearPlace(int32 Group) const
	{
		return Point(StageGearPlacesOffset + static_cast<std::size_t>(Group) * 8 + 4);
	}

	uint8& FBattleStage::Route(int32 From, int32 To, int32 Byte)
	{
		return Layout[StageRoutesOffset + static_cast<std::size_t>(From) * 0x40 + static_cast<std::size_t>(To) * 8 + static_cast<std::size_t>(Byte)];
	}

	TResult<FBattleStage> FBattleStage::Load(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
	{
		const FIndexRecord* Group = Index.Find(StageDirectory, StageGroupFile);
		if (Group == nullptr || Group->Size >= 0 || Number >= -Group->Size / 2)
		{
			return MakeError("the disc has no battle stage " + std::to_string(Number));
		}
		const TResult<std::vector<uint8>> File = ReadFile(Image, Index, StageDirectory, 2 * Number + 7);
		if (!File)
		{
			return File.Error();
		}
		return Parse(*File);
	}
}
