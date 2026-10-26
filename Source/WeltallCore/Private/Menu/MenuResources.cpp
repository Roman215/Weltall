#include "Menu/MenuResources.h"

#include "Formats/SectionContainer.h"
#include "Formats/Tim.h"
#include "Game/ChapterTitles.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuPortFrames.h"
#include "Text/MesResident.h"
#include "Text/MessageOverrides.h"
#include "Text/MessageTableIds.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr int32 MenuDirectory = 16;
		/** The places saves are made at. */
		constexpr int32 PlaceNamesDirectory = 17;
		constexpr int32 PlaceNamesFile = 1;
		constexpr int32 DataFileNumber = 1;
		/** The game's data file: the items', the equipment's and the gears' tables. */
		constexpr int32 GameDataFileNumber = 2;
		constexpr int32 MainModuleFile = 5;
		constexpr int32 MemberModuleFile = 6;
		constexpr int32 NameModuleFile = 8;
		constexpr int32 ShopModuleFile = 9;
		constexpr int32 GearShopModuleFile = 10;
		/** The data file's sections: the save icon's TIM, the TIM archive, the sprite sheet, the text, the portraits. */
		constexpr uint32 SaveIconSection = 1;
		constexpr uint32 ArchiveSection = 2;
		constexpr uint32 SheetSection = 3;
		/** In the game's data file: the characters' command records (one section a character from the 4th), the growth table. */
		constexpr uint32 FirstCommandsSection = 4;
		constexpr uint32 GrowthSection = 44;
		constexpr uint32 ItemsSection = 1;
		constexpr uint32 GearWeaponsSection = 43;
		/** One section a gear from the 23rd. */
		constexpr uint32 FirstGearCommandsSection = 23;
		/** The texts by FMenuResources::EText. */
		constexpr std::array<uint32, 19> TextSections = {15, 16, 21, 22, 53, 54, 55, 56, 40, 41, 42, 51, 52, 48, 49, 50, 45, 46, 47};
		constexpr uint32 TextSection = 4;
		constexpr uint32 PortraitSection = 5;
		constexpr uint32 NameEntrySection = 6;
		constexpr uint32 ShopStockSection = 7;
		constexpr uint32 GearShopStockSection = 8;
		/** The kernel's two CLUTs for the message font's even and odd lines. */
		constexpr uint32 KernelTextClut = 0x80050190;

		/** Where the main module's tables lie in its file (the module loads at 801C5000). */
		namespace ModuleOffsets
		{
			constexpr std::size_t ArcX = 0x24A00;
			constexpr std::size_t ArcY = 0x24A2C;
			constexpr std::size_t MainLabelOffsets = 0x24E64;
			constexpr std::size_t TitleLabelOffsets = 0x24E84;
			constexpr std::size_t SoundLabelOffsets = 0x24F88;
			constexpr std::size_t MainCommands = 0x2519C;
			constexpr std::size_t TitleCommands = 0x251D4;
			constexpr std::size_t SubCommands = 0x251EC;
			constexpr std::size_t HelpLabels = 0x25524;
			constexpr std::size_t MainLabels = 0x25528;
			constexpr std::size_t TitleLabels = 0x25530;
			constexpr std::size_t SoundLabels = 0x25574;
			constexpr std::size_t NameStripX = 0x25578;
			constexpr std::size_t NameStripY = 0x255C4;
			constexpr std::size_t FileLabels = 0x2553C;
			constexpr std::size_t FileLabelOffsets = 0x24EA0;
			constexpr std::size_t CellX = 0x24894;
			constexpr std::size_t CellY = 0x24914;
			constexpr std::size_t CursorCells = 0x2481C;
			constexpr std::size_t DetailLabels = 0x25494;
			constexpr std::size_t DetailLabelX = 0x24F98;
			constexpr std::size_t DetailLabelY = 0x24FBC;
			constexpr std::size_t DetailPortraitX = 0x25004;
			constexpr std::size_t DetailPortraitY = 0x25010;
			constexpr std::size_t DetailNumbers = 0x2501C;
			constexpr std::size_t DetailColonX = 0x24FE0;
			constexpr std::size_t DetailTimeX = 0x24FE8;
			constexpr std::size_t PlaceCell = 0x2504C;
			constexpr std::size_t PanelLabels = 0x2534C;
			constexpr std::size_t PanelLabelX = 0x24A78;
			constexpr std::size_t PanelLabelY = 0x24AC8;
			constexpr std::size_t PanelNumbers = 0x24B18;
			constexpr std::size_t PanelName = 0x24B58;
			constexpr std::size_t RideBadges = 0x2477C;
			constexpr std::size_t SheetLabelX = 0x24B60;
			constexpr std::size_t SheetLabelY = 0x24C20;
			constexpr std::size_t SheetNumbers = 0x24CE0;
			constexpr std::size_t SheetName = 0x24D38;
			constexpr std::size_t StatLabelX = 0x24D40;
			constexpr std::size_t StatLabelY = 0x24D5C;
			constexpr std::size_t StatRow = 0x24D78;
			constexpr std::size_t EquipNameY = 0x24D88;
			constexpr std::size_t SheetHeadingX = 0x24E4C;
			constexpr std::size_t SheetHeadingY = 0x24E58;
			constexpr std::size_t SheetSubLabelOffsets = 0x24E94;
			constexpr std::size_t MemberHintX = 0x25164;
			constexpr std::size_t MemberHintY = 0x2516C;
			constexpr std::size_t SheetLabels = 0x2539C;
			constexpr std::size_t StatLabels = 0x2545C;
			constexpr std::size_t SheetTexts = 0x25534;
			constexpr std::size_t SkillBits = 0x246C8;
			constexpr std::size_t DeathblowCombos = 0x247AC;
			constexpr std::size_t SkillsTexts = 0x25564;
			constexpr std::size_t MemberNameX = 0x2517C;
			constexpr std::size_t MemberNameY = 0x2518C;
			constexpr std::size_t LabelSets = 0x25548;
			constexpr std::size_t EquipPointerY = 0x24DBC;
			constexpr std::size_t SlotLabelY = 0x24F30;
			constexpr std::size_t TargetPointerX = 0x24A58;
			constexpr std::size_t TargetPointerY = 0x24A68;
			constexpr std::size_t TargetLabels = 0x254DC;
			constexpr std::size_t TargetX = 0x25054;
			constexpr std::size_t TargetY = 0x250DC;
			constexpr std::size_t GearTargetX = 0x25098;
			constexpr std::size_t GearTargetY = 0x25120;
			constexpr std::size_t HelpTagX = 0x24EC4;
			constexpr std::size_t HelpTagY = 0x24EE4;
			constexpr std::size_t AbilityTagX = 0x24EE8;
			constexpr std::size_t AbilityTagY = 0x24F28;
			constexpr std::size_t EtherMasks = 0x247F0;
			constexpr std::size_t CostX = 0x24DDC;
			constexpr std::size_t CostY = 0x24E14;
			constexpr std::size_t AbilityListWidth = 0x24788;
			constexpr std::size_t AbilityPointsX = 0x24794;
			constexpr std::size_t AbilityPointsWidth = 0x247A0;
			constexpr std::size_t GearArcLabelOffsets = 0x24F48;
			constexpr std::size_t End = 0x255E8;
		}

		int32 ReadWord(std::span<const uint8> Bytes, std::size_t Offset)
		{
			return static_cast<int32>(ReadU32(Bytes, Offset));
		}

		template <std::size_t Count>
		void ReadWords(std::span<const uint8> Module, std::size_t At, std::array<int32, Count>& Values)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Values[Index] = ReadWord(Module, At + 4 * Index);
			}
		}

		template <std::size_t Count>
		void ReadCommands(std::span<const uint8> Module, std::size_t At, std::array<FMenuArcCommand, Count>& Commands)
		{
			for (std::size_t Index = 0; Index < Count; ++Index)
			{
				Commands[Index] = {ReadWord(Module, At + 8 * Index), ReadWord(Module, At + 8 * Index + 4)};
			}
		}

		template <std::size_t Count>
		void ReadBytes(std::span<const uint8> Module, std::size_t At, std::array<uint8, Count>& Values)
		{
			std::copy_n(Module.begin() + static_cast<std::ptrdiff_t>(At), Count, Values.begin());
		}

		TResult<std::vector<uint8>> ReadMenuFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
		{
			const FIndexRecord* Record = Index.Find(MenuDirectory, Number);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks the Menu's files");
			}
			// In full: the data file's last stream may end in the padding after its recorded size.
			return ReadFileSectors(Image, *Record);
		}

		/** The French translation's Menu program (directory 16, file 5), by its SHA-1. */
		constexpr std::string_view FrenchMainModule = "78f877c0e250269d98d302b3e3e9e22dc616d8f3";

		bool IsFrenchMainModule(const FGameFileIndex& Index, std::span<const uint8> Module)
		{
			const FIndexRecord* Record = Index.Find(MenuDirectory, MainModuleFile);
			return Record != nullptr && static_cast<std::size_t>(Record->Size) <= Module.size()
				&& FSha1::ToHex(FSha1::Hash(Module.first(static_cast<std::size_t>(Record->Size)))) == FrenchMainModule;
		}

		/**
		 * Its chapters' titles, past its own code at the module's end: the story value each chapter ends before, the
		 * titles' offsets from their strings, the strings.
		 */
		namespace FrenchChapterOffsets
		{
			constexpr std::size_t Ends = 0x25B28;
			constexpr std::size_t Titles = 0x25BB0;
			constexpr std::size_t Strings = 0x25C38;
			constexpr std::size_t Count = 68;
		}

		std::vector<std::pair<uint16, std::vector<uint8>>> ReadFrenchChapters(std::span<const uint8> Module)
		{
			using namespace FrenchChapterOffsets;
			std::vector<std::pair<uint16, std::vector<uint8>>> Chapters;
			if (Module.size() < Strings)
			{
				return Chapters;
			}
			for (std::size_t Chapter = 0; Chapter < Count; ++Chapter)
			{
				const std::size_t First = Strings + ReadU16(Module, Titles + Chapter * 2);
				const auto End = std::find(Module.begin() + static_cast<std::ptrdiff_t>(std::min(First, Module.size())), Module.end(), uint8{0});
				if (End == Module.end())
				{
					return {};
				}
				Chapters.emplace_back(ReadU16(Module, Ends + Chapter * 2), std::vector<uint8>(Module.begin() + static_cast<std::ptrdiff_t>(First), End + 1));
			}
			return Chapters;
		}
	}

	TResult<FMenuMainTables> FMenuMainTables::Parse(std::span<const uint8> Module)
	{
		if (Module.size() < ModuleOffsets::End)
		{
			return MakeError("the Menu's main module is too short to hold its tables");
		}
		FMenuMainTables Tables;
		ReadWords(Module, ModuleOffsets::ArcX, Tables.ArcX);
		ReadWords(Module, ModuleOffsets::ArcY, Tables.ArcY);
		ReadCommands(Module, ModuleOffsets::TitleCommands, Tables.TitleCommands);
		ReadCommands(Module, ModuleOffsets::MainCommands, Tables.MainCommands);
		for (std::size_t Row = 0; Row < Tables.SubCommands.size(); ++Row)
		{
			ReadCommands(Module, ModuleOffsets::SubCommands + Row * 8 * SubArcCommands, Tables.SubCommands[Row]);
		}
		ReadWords(Module, ModuleOffsets::TitleLabelOffsets, Tables.TitleLabelOffsets);
		ReadWords(Module, ModuleOffsets::MainLabelOffsets, Tables.MainLabelOffsets);
		ReadWords(Module, ModuleOffsets::SoundLabelOffsets, Tables.SoundLabelOffsets);
		ReadBytes(Module, ModuleOffsets::HelpLabels, Tables.HelpLabels);
		ReadBytes(Module, ModuleOffsets::TitleLabels, Tables.TitleLabels);
		ReadBytes(Module, ModuleOffsets::SoundLabels, Tables.SoundLabels);
		ReadBytes(Module, ModuleOffsets::MainLabels, Tables.MainLabels);
		ReadWords(Module, ModuleOffsets::NameStripX, Tables.NameStripX);
		ReadWords(Module, ModuleOffsets::NameStripY, Tables.NameStripY);
		ReadWords(Module, ModuleOffsets::PanelLabels, Tables.PanelLabels);
		ReadWords(Module, ModuleOffsets::PanelLabelX, Tables.PanelLabelX);
		ReadWords(Module, ModuleOffsets::PanelLabelY, Tables.PanelLabelY);
		for (std::size_t Number = 0; Number < Tables.PanelNumbers.size(); ++Number)
		{
			Tables.PanelNumbers[Number] = {ReadWord(Module, ModuleOffsets::PanelNumbers + 8 * Number), ReadWord(Module, ModuleOffsets::PanelNumbers + 8 * Number + 4)};
		}
		ReadWords(Module, ModuleOffsets::PanelName, Tables.PanelName);
		ReadBytes(Module, ModuleOffsets::FileLabels, Tables.FileLabels[0]);
		ReadBytes(Module, ModuleOffsets::FileLabels + Tables.FileLabels[0].size(), Tables.FileLabels[1]);
		ReadWords(Module, ModuleOffsets::FileLabelOffsets, Tables.FileLabelOffsets);
		ReadWords(Module, ModuleOffsets::CellX, Tables.CellX);
		ReadWords(Module, ModuleOffsets::CellY, Tables.CellY);
		ReadWords(Module, ModuleOffsets::CursorCells, Tables.CursorCells);
		ReadWords(Module, ModuleOffsets::DetailLabels, Tables.DetailLabels);
		ReadWords(Module, ModuleOffsets::DetailLabelX, Tables.DetailLabelX);
		ReadWords(Module, ModuleOffsets::DetailLabelY, Tables.DetailLabelY);
		ReadWords(Module, ModuleOffsets::DetailPortraitX, Tables.DetailPortraitX);
		ReadWords(Module, ModuleOffsets::DetailPortraitY, Tables.DetailPortraitY);
		for (std::size_t Number = 0; Number < Tables.DetailNumbers.size(); ++Number)
		{
			Tables.DetailNumbers[Number] = {ReadWord(Module, ModuleOffsets::DetailNumbers + 8 * Number), ReadWord(Module, ModuleOffsets::DetailNumbers + 8 * Number + 4)};
		}
		ReadWords(Module, ModuleOffsets::DetailColonX, Tables.DetailColonX);
		ReadWords(Module, ModuleOffsets::DetailTimeX, Tables.DetailTimeX);
		ReadWords(Module, ModuleOffsets::PlaceCell, Tables.PlaceCell);
		ReadWords(Module, ModuleOffsets::SheetLabels, Tables.SheetLabels);
		ReadWords(Module, ModuleOffsets::SheetLabelX, Tables.SheetLabelX);
		ReadWords(Module, ModuleOffsets::SheetLabelY, Tables.SheetLabelY);
		for (std::size_t Number = 0; Number < Tables.SheetNumbers.size(); ++Number)
		{
			Tables.SheetNumbers[Number] = {ReadWord(Module, ModuleOffsets::SheetNumbers + 8 * Number), ReadWord(Module, ModuleOffsets::SheetNumbers + 8 * Number + 4)};
		}
		ReadWords(Module, ModuleOffsets::SheetName, Tables.SheetName);
		ReadBytes(Module, ModuleOffsets::SheetTexts, Tables.SheetTexts);
		ReadWords(Module, ModuleOffsets::SheetHeadingX, Tables.SheetHeadingX);
		ReadWords(Module, ModuleOffsets::SheetHeadingY, Tables.SheetHeadingY);
		ReadWords(Module, ModuleOffsets::SheetSubLabelOffsets, Tables.SheetSubLabelOffsets);
		ReadWords(Module, ModuleOffsets::EquipNameY, Tables.EquipNameY);
		ReadWords(Module, ModuleOffsets::StatLabels, Tables.StatLabels);
		ReadWords(Module, ModuleOffsets::StatLabelX, Tables.StatLabelX);
		ReadWords(Module, ModuleOffsets::StatLabelY, Tables.StatLabelY);
		ReadWords(Module, ModuleOffsets::StatRow, Tables.StatRow);
		ReadWords(Module, ModuleOffsets::RideBadges, Tables.RideBadges);
		ReadWords(Module, ModuleOffsets::MemberHintX, Tables.MemberHintX);
		ReadWords(Module, ModuleOffsets::MemberHintY, Tables.MemberHintY);
		for (std::size_t Skill = 0; Skill < Tables.SkillBits.size(); ++Skill)
		{
			Tables.SkillBits[Skill] = ReadU16(Module, ModuleOffsets::SkillBits + 2 * Skill);
		}
		ReadBytes(Module, ModuleOffsets::DeathblowCombos, Tables.DeathblowCombos);
		ReadBytes(Module, ModuleOffsets::SkillsTexts, Tables.SkillsTexts);
		ReadWords(Module, ModuleOffsets::MemberNameX, Tables.MemberNameX);
		ReadWords(Module, ModuleOffsets::MemberNameY, Tables.MemberNameY);
		ReadBytes(Module, ModuleOffsets::LabelSets, Tables.LabelSets);
		ReadWords(Module, ModuleOffsets::EquipPointerY, Tables.EquipPointerY);
		ReadWords(Module, ModuleOffsets::SlotLabelY, Tables.SlotLabelY);
		ReadWords(Module, ModuleOffsets::TargetPointerX, Tables.TargetPointerX);
		ReadWords(Module, ModuleOffsets::TargetPointerY, Tables.TargetPointerY);
		ReadWords(Module, ModuleOffsets::TargetLabels, Tables.TargetLabels);
		ReadWords(Module, ModuleOffsets::TargetX, Tables.TargetX[0]);
		ReadWords(Module, ModuleOffsets::TargetY, Tables.TargetY[0]);
		ReadWords(Module, ModuleOffsets::GearTargetX, Tables.TargetX[1]);
		ReadWords(Module, ModuleOffsets::GearTargetY, Tables.TargetY[1]);
		ReadWords(Module, ModuleOffsets::HelpTagX, Tables.HelpTagX);
		Tables.HelpTagY = ReadWord(Module, ModuleOffsets::HelpTagY);
		ReadWords(Module, ModuleOffsets::AbilityTagX, Tables.AbilityTagX);
		ReadWords(Module, ModuleOffsets::AbilityTagY, Tables.AbilityTagY);
		for (std::size_t Character = 0; Character < Tables.EtherMasks.size(); ++Character)
		{
			Tables.EtherMasks[Character] = ReadU16(Module, ModuleOffsets::EtherMasks + 2 * Character);
		}
		ReadWords(Module, ModuleOffsets::CostX, Tables.CostX);
		ReadWords(Module, ModuleOffsets::CostY, Tables.CostY);
		ReadWords(Module, ModuleOffsets::AbilityListWidth, Tables.AbilityListWidth);
		ReadWords(Module, ModuleOffsets::AbilityPointsX, Tables.AbilityPointsX);
		ReadWords(Module, ModuleOffsets::AbilityPointsWidth, Tables.AbilityPointsWidth);
		ReadWords(Module, ModuleOffsets::GearArcLabelOffsets, Tables.GearArcLabelOffsets);
		return Tables;
	}

	TResult<FMenuResources> FMenuResources::Parse(std::span<const uint8> DataFile, std::span<const uint8> MainModule,
		const std::array<uint16, 32>& TextClut, std::span<const uint8> GameData)
	{
		FMenuResources Resources;
		const TResult<std::vector<uint8>> Archive = UnpackContainerSection(DataFile, ArchiveSection);
		if (!Archive)
		{
			return Archive.Error();
		}
		const std::span<const uint8> Tims = *Archive;
		if (Tims.size() < 4 || Tims.size() < 4 + 4 * static_cast<std::size_t>(ReadU32(Tims, 0)))
		{
			return MakeError("the Menu's TIM archive is too short");
		}
		for (uint32 Tim = ReadU32(Tims, 0); Tim-- > 0;)
		{
			const std::size_t Offset = ReadU32(Tims, 4 + 4 * static_cast<std::size_t>(Tim)) & ~3u;
			if (Offset >= Tims.size())
			{
				return MakeError("a TIM of the Menu's archive starts past its end");
			}
			TResult<FTim> Image = ParseTim(Tims.subspan(Offset));
			if (!Image)
			{
				return Image.Error();
			}
			if (Image->Clut)
			{
				Resources.Archive.push_back(std::move(*Image->Clut));
			}
			Resources.Archive.push_back(std::move(Image->Image));
		}
		const TResult<std::vector<uint8>> Sheet = UnpackContainerSection(DataFile, SheetSection);
		if (!Sheet)
		{
			return Sheet.Error();
		}
		TResult<FMenuSpriteSheet> ParsedSheet = FMenuSpriteSheet::Parse(*Sheet);
		if (!ParsedSheet)
		{
			return ParsedSheet.Error();
		}
		Resources.SpriteSheet = std::move(*ParsedSheet);
		TResult<std::vector<uint8>> Text = UnpackContainerSection(DataFile, TextSection);
		if (!Text)
		{
			return Text.Error();
		}
		Resources.TextBytes = std::move(*Text);
		TResult<FMessageTable> Table = FMessageTable::Parse(Resources.TextBytes);
		if (!Table)
		{
			return Table.Error();
		}
		Resources.TextTable = *Table;
		TResult<std::vector<uint8>> Portraits = UnpackContainerSection(DataFile, PortraitSection);
		if (!Portraits)
		{
			return Portraits.Error();
		}
		if (Portraits->size() < PortraitCount * PortraitBytes)
		{
			return MakeError("the Menu's portraits are too short");
		}
		Resources.Portraits = std::move(*Portraits);
		TResult<std::vector<uint8>> NameEntry = UnpackContainerSection(DataFile, NameEntrySection);
		if (!NameEntry)
		{
			return MakeError("the Menu's name entry table cannot be read: " + NameEntry.Error().Message);
		}
		Resources.NameEntryTable = std::move(*NameEntry);
		TResult<std::vector<uint8>> Icon = UnpackContainerSection(DataFile, SaveIconSection);
		if (!Icon)
		{
			return MakeError("the save icon cannot be read: " + Icon.Error().Message);
		}
		TResult<FTim> IconTim = ParseTim(*Icon);
		if (!IconTim || !IconTim->Clut || IconTim->Clut->Units.size() < Resources.IconColours.size() / 2
			|| IconTim->Image.Units.size() < Resources.IconPixels.size() / 2)
		{
			return MakeError("the save icon is not a 16-colour TIM of 16 x 16 pixels");
		}
		for (std::size_t Unit = 0; Unit < Resources.IconColours.size() / 2; ++Unit)
		{
			WriteU16(Resources.IconColours, Unit * 2, IconTim->Clut->Units[Unit]);
		}
		for (std::size_t Unit = 0; Unit < Resources.IconPixels.size() / 2; ++Unit)
		{
			WriteU16(Resources.IconPixels, Unit * 2, IconTim->Image.Units[Unit]);
		}
		TResult<std::vector<uint8>> Stock = UnpackContainerSection(DataFile, ShopStockSection);
		if (!Stock)
		{
			return MakeError("the shops' stock cannot be read: " + Stock.Error().Message);
		}
		Resources.ShopStockTable = std::move(*Stock);
		TResult<std::vector<uint8>> GearStock = UnpackContainerSection(DataFile, GearShopStockSection);
		if (!GearStock)
		{
			return MakeError("the gear shops' stock cannot be read: " + GearStock.Error().Message);
		}
		Resources.GearShopStockTable = std::move(*GearStock);
		TResult<FMenuMainTables> Tables = FMenuMainTables::Parse(MainModule);
		if (!Tables)
		{
			return Tables.Error();
		}
		Resources.MainTables = *Tables;
		AddMenuPortFrames(Resources.SpriteSheet, Resources.MainTables);
		Resources.TextColours = TextClut;
		TResult<FEquipmentTables> Equipment = FEquipmentTables::Parse(GameData);
		if (!Equipment)
		{
			return Equipment.Error();
		}
		Resources.EquipmentTables = std::move(*Equipment);
		for (std::size_t Character = 0; Character < Resources.Commands.size(); ++Character)
		{
			TResult<std::vector<uint8>> Commands = UnpackContainerSection(GameData, FirstCommandsSection + static_cast<uint32>(Character));
			if (!Commands)
			{
				return MakeError("the characters' commands cannot be read: " + Commands.Error().Message);
			}
			Resources.Commands[Character] = std::move(*Commands);
		}
		TResult<std::vector<uint8>> Growth = UnpackContainerSection(GameData, GrowthSection);
		if (!Growth)
		{
			return MakeError("the growth table cannot be read: " + Growth.Error().Message);
		}
		Resources.GrowthTable = std::move(*Growth);
		TResult<FGearPartTables> GearParts = FGearPartTables::Parse(GameData, MainModule);
		if (!GearParts)
		{
			return GearParts.Error();
		}
		Resources.GearPartTables = std::move(*GearParts);
		for (const auto& [Section, Into] : {std::pair{ItemsSection, &Resources.ItemTable}, std::pair{GearWeaponsSection, &Resources.GearWeaponTable}})
		{
			TResult<std::vector<uint8>> Unpacked = UnpackContainerSection(GameData, Section);
			if (!Unpacked)
			{
				return MakeError("the game's data file cannot be read: " + Unpacked.Error().Message);
			}
			*Into = std::move(*Unpacked);
		}
		for (std::size_t Gear = 0; Gear < Resources.GearCommandTables.size(); ++Gear)
		{
			TResult<std::vector<uint8>> Commands = UnpackContainerSection(GameData, FirstGearCommandsSection + static_cast<uint32>(Gear));
			if (!Commands)
			{
				return MakeError("the gears' commands cannot be read: " + Commands.Error().Message);
			}
			Resources.GearCommandTables[Gear] = std::move(*Commands);
		}
		for (std::size_t Kind = 0; Kind < TextSections.size(); ++Kind)
		{
			TResult<std::vector<uint8>> Bytes = UnpackContainerSection(GameData, TextSections[Kind]);
			if (!Bytes)
			{
				return MakeError("the screens' texts cannot be read: " + Bytes.Error().Message);
			}
			Resources.TextBytesByKind[Kind] = std::move(*Bytes);
			TResult<FMessageTable> Parsed = FMessageTable::Parse(Resources.TextBytesByKind[Kind]);
			if (!Parsed)
			{
				return Parsed.Error();
			}
			Resources.Texts[Kind] = *Parsed;
		}
		return Resources;
	}

	std::span<const uint8> FMenuResources::GearCommands(int32 Gear) const
	{
		if (Gear < 0 || Gear >= static_cast<int32>(GearCommandTables.size()))
		{
			return {};
		}
		return GearCommandTables[static_cast<std::size_t>(Gear)];
	}

	int32 FMenuResources::SectionOfText(EText Which)
	{
		return static_cast<int32>(TextSections[static_cast<std::size_t>(Which)]);
	}

	void FMenuResources::UsePlayersMessages(const FCdImage& Image)
	{
		if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, MessageTables::Menu, TextBytes, EMessageWindows::Program))
		{
			TextBytes = std::move(Applied->Table);
			TextTable = *FMessageTable::Parse(TextBytes);
		}
		for (std::size_t Kind = 0; Kind < TextSections.size(); ++Kind)
		{
			const std::string Table = MessageTables::MenuHelp(static_cast<int32>(TextSections[Kind]));
			if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, Table, TextBytesByKind[Kind], EMessageWindows::Program))
			{
				TextBytesByKind[Kind] = std::move(Applied->Table);
				Texts[Kind] = *FMessageTable::Parse(TextBytesByKind[Kind]);
			}
		}
	}

	TResult<FMenuResources> FMenuResources::Load(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Kernel)
	{
		const TResult<std::vector<uint8>> Data = ReadMenuFile(Image, Index, DataFileNumber);
		if (!Data)
		{
			return Data.Error();
		}
		const TResult<std::vector<uint8>> Module = ReadMenuFile(Image, Index, MainModuleFile);
		if (!Module)
		{
			return Module.Error();
		}
		const TResult<std::vector<uint8>> GameData = ReadMenuFile(Image, Index, GameDataFileNumber);
		if (!GameData)
		{
			return GameData.Error();
		}
		const std::span<const uint8> Clut = Kernel.Read(KernelTextClut, 64);
		if (Clut.size() != 64)
		{
			return MakeError("the kernel lacks the message font's colours");
		}
		std::array<uint16, 32> TextClut{};
		for (std::size_t Colour = 0; Colour < TextClut.size(); ++Colour)
		{
			TextClut[Colour] = ReadU16(Clut, Colour * 2);
		}
		TResult<FMenuResources> Resources = Parse(*Data, *Module, TextClut, *GameData);
		if (!Resources)
		{
			return Resources;
		}
		Resources->bFrench = IsFrenchMainModule(Index, *Module);
		if (Resources->bFrench)
		{
			Resources->FrenchChapters = ReadFrenchChapters(*Module);
		}
		// A translation redraws the letters the port pieces "Options" from: its own label for an options screen shows
		// instead where Weltall knows one, else the word typed in the disc's font.
		if (!HasUsaOptionsLetters(Resources->SpriteSheet, Resources->Archive)
			&& !UseTranslatedOptionsLabel(Resources->SpriteSheet, Resources->MainTables, Resources->Archive))
		{
			const TResult<FMesResident> Resident = LoadMesResident(Image, Index, Kernel);
			if (Resident)
			{
				TypeMenuPortWords(Resources->SpriteSheet, Resources->MainTables, Resident->Font, Resources->Archive);
			}
		}
		Resources->UsePlayersMessages(Image);
		const TResult<std::vector<uint8>> Member = ReadMenuFile(Image, Index, MemberModuleFile);
		if (!Member)
		{
			return Member.Error();
		}
		TResult<FMenuMemberTables> MemberTables = FMenuMemberTables::Parse(*Member);
		if (!MemberTables)
		{
			return MemberTables.Error();
		}
		Resources->MemberTables = *MemberTables;
		const TResult<std::vector<uint8>> NameModule = ReadMenuFile(Image, Index, NameModuleFile);
		if (!NameModule)
		{
			return NameModule.Error();
		}
		TResult<FMenuNameTables> NameTables = FMenuNameTables::Parse(*NameModule);
		if (!NameTables)
		{
			return NameTables.Error();
		}
		Resources->NameTables = std::move(*NameTables);
		const TResult<std::vector<uint8>> ShopModule = ReadMenuFile(Image, Index, ShopModuleFile);
		if (!ShopModule)
		{
			return ShopModule.Error();
		}
		TResult<FMenuShopTables> ShopTables = FMenuShopTables::Parse(*ShopModule);
		if (!ShopTables)
		{
			return ShopTables.Error();
		}
		Resources->ShopTables = *ShopTables;
		const TResult<std::vector<uint8>> GearShopModule = ReadMenuFile(Image, Index, GearShopModuleFile);
		if (!GearShopModule)
		{
			return GearShopModule.Error();
		}
		TResult<FMenuGearShopTables> GearShopTables = FMenuGearShopTables::Parse(*GearShopModule);
		if (!GearShopTables)
		{
			return GearShopTables.Error();
		}
		Resources->GearShopTables = *GearShopTables;
		TResult<FGearShopTable> GearShopModels = ReadGearShopTable(Image, Index);
		if (!GearShopModels)
		{
			return GearShopModels.Error();
		}
		Resources->GearShopModelTable = *GearShopModels;
		const FIndexRecord* Places = Index.Find(PlaceNamesDirectory, PlaceNamesFile);
		if (Places == nullptr)
		{
			return MakeError("the disc lacks the places saves are made at");
		}
		TResult<std::vector<uint8>> PlaceNames = ReadFileSectors(Image, *Places);
		if (!PlaceNames)
		{
			return PlaceNames.Error();
		}
		PlaceNames->resize(static_cast<std::size_t>(Places->Size));
		Resources->PlaceNameBytes = WithPlayersChapterTitles(Image, *PlaceNames);
		TResult<FGteArctangents> Arctangents = FGteArctangents::Read(Kernel);
		if (!Arctangents)
		{
			return Arctangents.Error();
		}
		Resources->ArctangentTable = *Arctangents;
		return Resources;
	}

	std::span<const uint8> FMenuResources::FrenchChapterTitle(uint16 Progress) const
	{
		if (FrenchChapters.empty())
		{
			return {};
		}
		// The first chapter that ends past the value; past them all, the first: the program's table of each save's chapter
		// starts at 0, and its search leaves a later value's entry as it was.
		const auto Found = std::find_if(FrenchChapters.begin(), FrenchChapters.end(), [Progress](const auto& Chapter) { return Chapter.first > Progress; });
		return Found != FrenchChapters.end() ? Found->second : FrenchChapters.front().second;
	}

	std::span<const uint8> FMenuResources::CharacterCommands(int32 Character) const
	{
		if (Character < 0 || Character >= static_cast<int32>(Commands.size()))
		{
			return {};
		}
		return Commands[static_cast<std::size_t>(Character)];
	}

	std::span<const uint8> FMenuResources::Portrait(int32 Character) const
	{
		if (Character < 0 || Character >= PortraitCount)
		{
			return {};
		}
		return std::span<const uint8>(Portraits).subspan(static_cast<std::size_t>(Character) * PortraitBytes, PortraitBytes);
	}

	TResult<FGameIcon> ReadGameIcon(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const TResult<std::vector<uint8>> Data = ReadMenuFile(Image, Index, DataFileNumber);
		if (!Data)
		{
			return Data.Error();
		}
		const TResult<std::vector<uint8>> Icon = UnpackContainerSection(*Data, SaveIconSection);
		if (!Icon)
		{
			return MakeError("the save icon cannot be read: " + Icon.Error().Message);
		}
		const TResult<FTim> Tim = ParseTim(*Icon);
		constexpr std::size_t PixelCount = FGameIcon::Size * FGameIcon::Size;
		if (!Tim || !Tim->Clut || Tim->BitsPerPixel != 4 || Tim->Clut->Units.size() < 16 || Tim->Image.Units.size() < PixelCount / 4)
		{
			return MakeError("the save icon is not a 16-colour TIM of 16 x 16 pixels");
		}
		const auto Channel = [](uint16 Colour, int32 Shift) { return static_cast<uint8>((Colour >> Shift & 31) * 255 / 31); };
		FGameIcon Result;
		for (std::size_t Pixel = 0; Pixel < PixelCount; ++Pixel)
		{
			const uint16 Index4 = static_cast<uint16>(Tim->Image.Units[Pixel / 4] >> (4 * (Pixel % 4)) & 15);
			const uint16 Colour = Tim->Clut->Units[Index4];
			Result.Pixels[Pixel] = {Channel(Colour, 0), Channel(Colour, 5), Channel(Colour, 10), static_cast<uint8>(Colour == 0 ? 0 : 255)};
		}
		return Result;
	}
}
