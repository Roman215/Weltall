#include "Battle/Show/BattleShowHud.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleHudButtons.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Formats/SectionContainer.h"
#include "Formats/Tim.h"
#include "Formats/VramImageList.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/Sha1.h"
#include "Graphics/TextureRegion.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Text/MesRender.h"
#include "Text/MessageOverrides.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace Weltall
{
	using namespace BattleHudButtons;
	using namespace BattleHudGpu;

	namespace
	{
		constexpr uint32 SheetSection = 1;
		/** The French translation's Battle program (unpacked) and result program (as the disc holds it), by their SHA-1. */
		constexpr std::string_view FrenchProgram = "ff6e9fc59534f6867e183db0faf3981cb28695e7";
		constexpr std::string_view FrenchResultProgram = "21bbc471a6e4d41f8c71ed7090f5ab8d6e8ca0e8";
		constexpr uint32 ImagesSection = 2;
		constexpr uint32 PortraitsSection = 0x24;
		constexpr uint32 MessagesSection = 0x26;
		constexpr uint32 ItemHelpSection = 0x10;
		constexpr uint32 ResultSheetSection = 2;
		constexpr uint32 ResultImagesSection = 3;
		constexpr uint32 ResultPortraitsSection = 4;
		/** The kernel's text CLUTs in the executable. */
		constexpr uint32 TextClutAddress = 0x80050190;
		/** Where the kernel puts the text's CLUTs for the battle, the odd plane's 16 units along. */
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1F0;
		constexpr int16 OddTextClutX = 16;
		constexpr std::size_t PortraitBytes = 0x460;
		constexpr uint8 SpecialPartyPortrait = 11;
		/** The sheet's frames the set-up takes textures from, and where the party's portraits are drawn from. */
		constexpr std::array<int32, 4> EdgeFrames{0x4B, 0x50, 0x4D, 0x4E};
		/** The gauges' animated CLUT rows: 198 units each from x 1, stored four times over. */
		constexpr int16 TextureRowX = 1;
		constexpr int16 TextureRowUnits = 0xC6;
		constexpr int32 TextureRowCopies = 4;
		constexpr uint32 TextureRowWrap = 0xC6;
		constexpr std::size_t DigitUnits = 2;
		constexpr std::size_t TextRows = 13;
		constexpr std::size_t BoxUnits = 0x39;
		/** A glow pulses by 4 between 4 and 80h. */
		constexpr int32 GlowStep = 4;
		constexpr uint8 ModeResult = 0;
		constexpr uint8 ModeBattle = 1;
		constexpr uint8 ModeIntro = 2;
		constexpr uint8 NoMember = 0x7F;
		constexpr uint8 ChuChu = 7;
		constexpr int32 DigitLabelTable = 18;

		/** Where the kernel loads the Battle program, and the tables the HUD reads there. */
		constexpr uint32 ProgramBase = 0x8006FAF0;
		constexpr uint32 DigitPlacesAddress = 0x800C3068;
		constexpr uint32 ApTicksAddress = 0x800C3202;
		constexpr uint32 PanelOffsetsAddress = 0x800C3254;
		constexpr uint32 IconGroupsAddress = 0x800C2F4C;
		constexpr uint32 MenuStatesAddress = 0x800C3000;
		constexpr uint32 CharacterIconsAddress = 0x800C20F0;
		constexpr uint32 GearIconsAddress = 0x800C2130;
		constexpr uint32 OuterGearIconsAddress = 0x800C2134;
		constexpr uint32 ChuChuGearIconsAddress = 0x800C2138;
		constexpr uint32 CockpitGlassAddress = 0x800C33B0;
		constexpr uint32 ComboFramesAddress = 0x800C33B4;
		constexpr uint32 ComboXAddress = 0x800C33C4;
		constexpr uint32 ComboYAddress = 0x800C3404;
		/** The result screen's labels: each table's frames, then its places across and down. */
		constexpr uint32 ResultPanelFramesAddress = 0x800C3268;
		constexpr uint32 ResultPanelXAddress = 0x800C327C;
		constexpr uint32 ResultPanelYAddress = 0x800C32A0;
		constexpr uint32 ResultLevelLabelsAddress = 0x800C32C4;
		constexpr uint32 ResultLevelXAddress = 0x800C3318;
		constexpr uint32 ResultLevelYAddress = 0x800C3350;
		constexpr uint32 ResultGoldFramesAddress = 0x800C3388;
		constexpr uint32 ResultGoldXAddress = 0x800C3390;
		constexpr uint32 ResultGoldYAddress = 0x800C33A0;
		constexpr std::size_t LevelLabelSize = 3;
		constexpr uint16 IconGroupEnd = 0xFFFF;
		constexpr std::size_t IconElementSize = 8;

		/** The program's bytes at a memory address, or none past the program. */
		std::optional<std::span<const uint8>> ProgramAt(std::span<const uint8> Program, uint32 Address, std::size_t Count)
		{
			if (Address < ProgramBase || Address - ProgramBase + Count > Program.size())
			{
				return std::nullopt;
			}
			return Program.subspan(Address - ProgramBase, Count);
		}

		/** The bytes a pointer in the program's table points at, or none when either lies outside the program. */
		std::optional<std::span<const uint8>> ProgramPointee(std::span<const uint8> Program, uint32 PointerAddress, std::size_t Count)
		{
			const std::optional<std::span<const uint8>> Pointer = ProgramAt(Program, PointerAddress, 4);
			return Pointer ? ProgramAt(Program, LittleEndian::ReadU32(*Pointer, 0), Count) : std::nullopt;
		}

		/** An icon group's elements up to its end mark, or none when it runs past the program. */
		std::optional<std::vector<FBattleHudIconElement>> ReadIconGroup(std::span<const uint8> Program, uint32 PointerAddress)
		{
			const std::optional<std::span<const uint8>> Pointer = ProgramAt(Program, PointerAddress, 4);
			if (!Pointer)
			{
				return std::nullopt;
			}
			std::vector<FBattleHudIconElement> Elements;
			for (uint32 At = LittleEndian::ReadU32(*Pointer, 0);; At += IconElementSize)
			{
				const std::optional<std::span<const uint8>> Element = ProgramAt(Program, At, IconElementSize);
				if (!Element)
				{
					return std::nullopt;
				}
				const uint16 Frame = LittleEndian::ReadU16(*Element, 0);
				if (Frame == IconGroupEnd)
				{
					return Elements;
				}
				Elements.push_back({Frame, LittleEndian::ReadS16(*Element, 2), LittleEndian::ReadS16(*Element, 4), LittleEndian::ReadU16(*Element, 6)});
			}
		}

		/** The command rings' tables: each state's layers and groups, the groups, and the pictures the set-up gives the slots. */
		bool ParseCommandRings(std::span<const uint8> Program, FBattleHudTables& Tables)
		{
			for (std::size_t State = 0; State < Tables.StateLayers.size(); ++State)
			{
				const std::optional<std::span<const uint8>> Entry = ProgramPointee(Program, static_cast<uint32>(MenuStatesAddress + 4 * State), 4);
				if (!Entry)
				{
					return false;
				}
				Tables.StateLayers[State] = {(*Entry)[0], (*Entry)[1]};
				Tables.StateGroups[State] = {(*Entry)[2], (*Entry)[3]};
			}
			for (std::size_t Group = 0; Group < Tables.IconGroups.size(); ++Group)
			{
				std::optional<std::vector<FBattleHudIconElement>> Elements = ReadIconGroup(Program, static_cast<uint32>(IconGroupsAddress + 4 * Group));
				if (!Elements)
				{
					return false;
				}
				Tables.IconGroups[Group] = std::move(*Elements);
			}
			for (std::size_t Character = 0; Character < Tables.CharacterIcons.size(); ++Character)
			{
				const std::optional<std::span<const uint8>> Icons = ProgramPointee(Program, static_cast<uint32>(CharacterIconsAddress + 4 * Character), 8);
				if (!Icons)
				{
					return false;
				}
				std::copy(Icons->begin(), Icons->end(), Tables.CharacterIcons[Character].begin());
			}
			const std::optional<std::span<const uint8>> Gear = ProgramPointee(Program, GearIconsAddress, 4);
			const std::optional<std::span<const uint8>> Outer = ProgramPointee(Program, OuterGearIconsAddress, 4);
			const std::optional<std::span<const uint8>> GiantForm = ProgramPointee(Program, ChuChuGearIconsAddress, 4);
			if (!Gear || !Outer || !GiantForm)
			{
				return false;
			}
			std::copy(Gear->begin(), Gear->end(), Tables.GearIcons.begin());
			std::copy(Outer->begin(), Outer->end(), Tables.OuterGearIcons.begin());
			std::copy(GiantForm->begin(), GiantForm->end(), Tables.ChuChuGearIcons.begin());
			const std::size_t Sprites = Tables.ComboSprites.size();
			const std::optional<std::span<const uint8>> ComboFrames = ProgramAt(Program, ComboFramesAddress, Sprites);
			const std::optional<std::span<const uint8>> ComboX = ProgramAt(Program, ComboXAddress, 4 * Sprites);
			const std::optional<std::span<const uint8>> ComboY = ProgramAt(Program, ComboYAddress, 4 * Sprites);
			if (!ComboFrames || !ComboX || !ComboY)
			{
				return false;
			}
			for (std::size_t Sprite = 0; Sprite < Sprites; ++Sprite)
			{
				Tables.ComboSprites[Sprite] = {(*ComboFrames)[Sprite], LittleEndian::ReadS16(*ComboX, 4 * Sprite), LittleEndian::ReadS16(*ComboY, 4 * Sprite)};
			}
			const std::optional<std::span<const uint8>> Glass = ProgramAt(Program, CockpitGlassAddress, Tables.CockpitGlass.size());
			if (!Glass)
			{
				return false;
			}
			std::copy(Glass->begin(), Glass->end(), Tables.CockpitGlass.begin());
			return true;
		}

		/** The result screen's label tables. */
		bool ParseResultLabels(std::span<const uint8> Program, FBattleHudTables& Tables)
		{
			const auto Read = [Program](std::span<FBattleHudResultLabel> Labels, uint32 Frames, uint32 X, uint32 Y) {
				const std::optional<std::span<const uint8>> FrameBytes = ProgramAt(Program, Frames, Labels.size());
				const std::optional<std::span<const uint8>> XBytes = ProgramAt(Program, X, 2 * Labels.size());
				const std::optional<std::span<const uint8>> YBytes = ProgramAt(Program, Y, 2 * Labels.size());
				if (!FrameBytes || !XBytes || !YBytes)
				{
					return false;
				}
				for (std::size_t Label = 0; Label < Labels.size(); ++Label)
				{
					Labels[Label] = {(*FrameBytes)[Label], LittleEndian::ReadS16(*XBytes, 2 * Label), LittleEndian::ReadS16(*YBytes, 2 * Label)};
				}
				return true;
			};
			if (!Read(Tables.ResultPanelLabels, ResultPanelFramesAddress, ResultPanelXAddress, ResultPanelYAddress)
				|| !Read(Tables.ResultGoldLabels, ResultGoldFramesAddress, ResultGoldXAddress, ResultGoldYAddress))
			{
				return false;
			}
			const std::size_t Count = Tables.ResultLevelLabels.size();
			const std::optional<std::span<const uint8>> Labels = ProgramAt(Program, ResultLevelLabelsAddress, LevelLabelSize * Count);
			const std::optional<std::span<const uint8>> X = ProgramAt(Program, ResultLevelXAddress, 2 * Count);
			const std::optional<std::span<const uint8>> Y = ProgramAt(Program, ResultLevelYAddress, 2 * Count);
			if (!Labels || !X || !Y)
			{
				return false;
			}
			for (std::size_t Label = 0; Label < Count; ++Label)
			{
				const std::size_t At = LevelLabelSize * Label;
				Tables.ResultLevelLabels[Label] = {(*Labels)[At], (*Labels)[At + 1] != 0, (*Labels)[At + 2], LittleEndian::ReadS16(*X, 2 * Label),
					LittleEndian::ReadS16(*Y, 2 * Label)};
			}
			return true;
		}

		/** A text image as the battle allocates one for Units units: three more units of pitch, 13 rows. */
		FVramImage TextImage(std::size_t Units)
		{
			FVramImage Image;
			Image.Rect.Width = static_cast<int16>((Units | 1) + 3);
			Image.Rect.Height = static_cast<int16>(TextRows);
			Image.Units.assign(static_cast<std::size_t>(Image.Rect.Width) * TextRows, 0);
			return Image;
		}
	}

	TResult<FBattleHudTables> FBattleHudTables::Parse(std::span<const uint8> Program)
	{
		FBattleHudTables Tables;
		const std::optional<std::span<const uint8>> Places = ProgramAt(Program, DigitPlacesAddress, 2 * 24 * 3);
		const std::optional<std::span<const uint8>> Ticks = ProgramAt(Program, ApTicksAddress, 6 * 8);
		const std::optional<std::span<const uint8>> Offsets = ProgramAt(Program, PanelOffsetsAddress, 2 * 9);
		if (!Places || !Ticks || !Offsets)
		{
			return MakeError("the Battle program is too short to hold the HUD's tables");
		}
		for (std::size_t Slot = 0; Slot < Tables.DigitPlaces.size(); ++Slot)
		{
			for (std::size_t Place = 0; Place < Tables.DigitPlaces[Slot].size(); ++Place)
			{
				Tables.DigitPlaces[Slot][Place] = LittleEndian::ReadS16(*Places, 2 * (24 * Slot + Place));
			}
		}
		for (std::size_t Row = 0; Row < Tables.ApTicks.size(); ++Row)
		{
			std::copy_n(Ticks->begin() + static_cast<std::ptrdiff_t>(6 * Row), 6, Tables.ApTicks[Row].begin());
		}
		for (std::size_t Index = 0; Index < Tables.PanelOffsets.size(); ++Index)
		{
			Tables.PanelOffsets[Index] = LittleEndian::ReadS16(*Offsets, 2 * Index);
		}
		if (!ParseCommandRings(Program, Tables))
		{
			return MakeError("the Battle program's command ring tables run past it");
		}
		if (!ParseResultLabels(Program, Tables))
		{
			return MakeError("the Battle program is too short to hold the result screen's tables");
		}
		if (FSha1::ToHex(FSha1::Hash(Program)) == FrenchProgram)
		{
			Tables.Places = FBattleHudPlaces::French();
		}
		return Tables;
	}

	FBattleHudPlaces FBattleHudPlaces::French()
	{
		FBattleHudPlaces Places;
		Places.MessageWindowY = 0xC4;
		Places.MessageBoxY = 0xC6;
		Places.ItemCursorY = 0x61;
		Places.ItemCellsX = 0x32;
		Places.ItemCellsBeyondX = 0x9A;
		Places.ItemTargetsX = 0xD3;
		Places.EtherCursorY = 0x37;
		Places.EtherCostX = 0x24;
		Places.EtherKindX = 0xD8;
		Places.EtherTargetsX = 0x103;
		Places.SpecialCursorX = 0x97;
		Places.SpecialCursorY = 0x57;
		Places.SpecialListX = 0xA0;
		Places.SpecialListRightX = 0x108;
		Places.SpecialCostX = 0x88;
		Places.SpecialTargetsX = 0x4F;
		Places.SpecialHelpX = 0x94;
		Places.ComboTensY = 0x3C;
		Places.ComboUnitsY = 0x3C;
		Places.EtherWindows[2][2] = 0x5E;
		Places.SpecialWindows[0][0] = 0x80;
		Places.SpecialWindows[1][0] = 0x90;
		Places.SpecialWindows[2][2] = 0x5E;
		return Places;
	}

	FBattleResultPlaces FBattleResultPlaces::French()
	{
		FBattleResultPlaces Places;
		Places.OldMaxHpX = 0xC0;
		Places.OldMaxEpX = 0xC8;
		Places.ChangeSignX = 0xE0;
		Places.ChangeDigitsX = 0xE8;
		Places.BarX = 0x80;
		Places.FallBarX = 0x80;
		Places.StatValueX = 0xC0;
		Places.LearnedLabelX = 0x55;
		return Places;
	}

	TResult<FBattleHudResources> FBattleHudResources::Parse(std::span<const uint8> Data, std::span<const uint8> Program, std::span<const uint16, 32> TextClut)
	{
		TResult<FBattleHudTables> Tables = FBattleHudTables::Parse(Program);
		if (!Tables)
		{
			return Tables.Error();
		}
		TResult<std::vector<uint8>> SheetBytes = UnpackContainerSection(Data, SheetSection);
		TResult<std::vector<uint8>> ImagesBytes = UnpackContainerSection(Data, ImagesSection);
		TResult<std::vector<uint8>> PortraitsBytes = UnpackContainerSection(Data, PortraitsSection);
		TResult<std::vector<uint8>> MessagesBytes = UnpackContainerSection(Data, MessagesSection);
		TResult<std::vector<uint8>> ItemHelpBytes = UnpackContainerSection(Data, ItemHelpSection);
		if (!SheetBytes || !ImagesBytes || !PortraitsBytes || !MessagesBytes || !ItemHelpBytes)
		{
			return MakeError("the battle data file lacks the HUD's sections");
		}
		TResult<FMenuSpriteSheet> Sheet = FMenuSpriteSheet::Parse(*SheetBytes);
		TResult<std::vector<FVramImage>> Images = ReadVramImageList(*ImagesBytes);
		if (!Sheet)
		{
			return Sheet.Error();
		}
		if (!Images)
		{
			return Images.Error();
		}
		FBattleHudResources Resources;
		Resources.Sheet = std::move(*Sheet);
		Resources.SheetRegions = MenuSheetTextureRegions(Resources.Sheet);
		Resources.Images = std::move(*Images);
		Resources.Portraits = std::move(*PortraitsBytes);
		Resources.Messages = std::move(*MessagesBytes);
		Resources.ItemHelp = std::move(*ItemHelpBytes);
		std::copy(TextClut.begin(), TextClut.end(), Resources.TextClut.begin());
		Resources.Tables = *Tables;
		return Resources;
	}

	std::optional<FError> FBattleHudResources::ParseResult(std::span<const uint8> File)
	{
		TResult<std::vector<uint8>> SheetBytes = UnpackContainerSection(File, ResultSheetSection);
		TResult<std::vector<uint8>> ImagesBytes = UnpackContainerSection(File, ResultImagesSection);
		TResult<std::vector<uint8>> PortraitsBytes = UnpackContainerSection(File, ResultPortraitsSection);
		if (!SheetBytes || !ImagesBytes || !PortraitsBytes)
		{
			return MakeError("the battle result's file lacks the result screen's sections");
		}
		TResult<FMenuSpriteSheet> Sprites = FMenuSpriteSheet::Parse(*SheetBytes);
		if (!Sprites)
		{
			return Sprites.Error();
		}
		TResult<std::vector<FVramImage>> Archive = ReadTimArchive(*ImagesBytes);
		if (!Archive)
		{
			return Archive.Error();
		}
		ResultSheet = std::move(*Sprites);
		ResultSheetRegions = MenuSheetTextureRegions(ResultSheet);
		ResultImages = std::move(*Archive);
		ResultPortraits = std::move(*PortraitsBytes);
		return std::nullopt;
	}

	TResult<FBattleHudResources> FBattleHudResources::Load(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable)
	{
		const FIndexRecord* Record = Index.Find(DataDirectory, DataFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the battle's data file");
		}
		const TResult<std::vector<uint8>> File = ReadFileSectors(Image, *Record);
		if (!File)
		{
			return File.Error();
		}
		const TResult<std::vector<uint8>> Program = ReadBattleProgram(Image, Index);
		if (!Program)
		{
			return Program.Error();
		}
		if (!Executable.Contains(TextClutAddress, 64))
		{
			return MakeError("the executable lacks the text's CLUTs");
		}
		const std::span<const uint8> ClutBytes = Executable.Read(TextClutAddress, 64);
		std::array<uint16, 32> TextClut{};
		for (std::size_t Colour = 0; Colour < TextClut.size(); ++Colour)
		{
			TextClut[Colour] = LittleEndian::ReadU16(ClutBytes, 2 * Colour);
		}
		TResult<FBattleHudResources> Resources = Parse(*File, *Program, TextClut);
		if (!Resources)
		{
			return Resources;
		}
		for (int32 Help = 0; Help < static_cast<int32>(Resources->HelpTexts.size()); ++Help)
		{
			const FIndexRecord* HelpRecord = Index.Find(HelpDirectory, 1 + Help);
			if (HelpRecord == nullptr)
			{
				return MakeError("the disc lacks the battle lists' help");
			}
			TResult<std::vector<uint8>> HelpFile = ReadFileSectors(Image, *HelpRecord);
			if (!HelpFile)
			{
				return HelpFile.Error();
			}
			Resources->HelpTexts[static_cast<std::size_t>(Help)] = std::move(*HelpFile);
		}
		const FIndexRecord* ResultRecord = Index.Find(ResultDirectory, ResultFile);
		if (ResultRecord == nullptr)
		{
			return MakeError("the disc lacks the battle result's file");
		}
		const TResult<std::vector<uint8>> ResultBytes = ReadFileSectors(Image, *ResultRecord);
		if (!ResultBytes)
		{
			return ResultBytes.Error();
		}
		if (std::optional<FError> Error = Resources->ParseResult(*ResultBytes))
		{
			return *Error;
		}
		if (const FIndexRecord* ProgramRecord = Index.Find(ResultProgramDirectory, ResultProgramFile))
		{
			const TResult<std::vector<uint8>> ResultProgram = ReadFileSectors(Image, *ProgramRecord);
			const std::size_t Size = static_cast<std::size_t>(ProgramRecord->Size);
			if (ResultProgram && ResultProgram->size() >= Size
				&& FSha1::ToHex(FSha1::Hash(std::span<const uint8>(*ResultProgram).first(Size))) == FrenchResultProgram)
			{
				Resources->ResultPlaces = FBattleResultPlaces::French();
			}
		}
		const std::array<std::pair<std::string_view, std::vector<uint8>*>, 5> Tables = {{
			{MessageTables::Battle, &Resources->Messages},
			{MessageTables::BattleItemHelp, &Resources->ItemHelp},
			{MessageTables::BattleEtherHelp, &Resources->HelpTexts[0]},
			{MessageTables::BattleGearEtherHelp, &Resources->HelpTexts[1]},
			{MessageTables::BattleSpecialHelp, &Resources->HelpTexts[2]},
		}};
		for (const auto& [Table, Bytes] : Tables)
		{
			if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, Table, *Bytes, EMessageWindows::Program))
			{
				*Bytes = std::move(Applied->Table);
			}
		}
		return Resources;
	}

	std::optional<FError> ForEachBattleHudPicture(const FCdImage& Image, const FGameFileIndex& Index, bool bResult, const FBattleHudPictureVisit& Visit)
	{
		const auto Read = [&Image, &Index](int32 Directory, int32 Number) -> TResult<std::vector<uint8>>
		{
			const FIndexRecord* Record = Index.Find(Directory, Number);
			if (Record == nullptr)
			{
				return MakeError("the disc lacks the battle HUD's files");
			}
			return ReadFileSectors(Image, *Record);
		};
		const TResult<std::vector<uint8>> Data = Read(FBattleHudResources::DataDirectory, FBattleHudResources::DataFile);
		if (!Data)
		{
			return Data.Error();
		}
		TResult<std::vector<uint8>> SheetBytes = UnpackContainerSection(*Data, SheetSection);
		TResult<std::vector<uint8>> ImagesBytes = UnpackContainerSection(*Data, ImagesSection);
		TResult<std::vector<uint8>> PortraitsBytes = UnpackContainerSection(*Data, PortraitsSection);
		if (!SheetBytes || !ImagesBytes || !PortraitsBytes)
		{
			return MakeError("the battle data file lacks the HUD's sections");
		}
		FBattleHudResources Pictures;
		TResult<FMenuSpriteSheet> Sheet = FMenuSpriteSheet::Parse(*SheetBytes);
		TResult<std::vector<FVramImage>> Images = ReadVramImageList(*ImagesBytes);
		if (!Sheet)
		{
			return Sheet.Error();
		}
		if (!Images)
		{
			return Images.Error();
		}
		Pictures.Sheet = std::move(*Sheet);
		Pictures.Portraits = std::move(*PortraitsBytes);
		const auto Vram = std::make_unique<FVram>();
		for (const FVramImage& Picture : *Images)
		{
			Vram->Upload(Picture);
		}
		const FMenuSpriteSheet* Drawn = &Pictures.Sheet;
		std::span<const uint8> Portraits = Pictures.Portraits;
		int32 PortraitFrame = FBattleHudResources::PortraitFrame;
		if (bResult)
		{
			const TResult<std::vector<uint8>> File = Read(FBattleHudResources::ResultDirectory, FBattleHudResources::ResultFile);
			if (!File)
			{
				return File.Error();
			}
			if (std::optional<FError> Error = Pictures.ParseResult(*File))
			{
				return Error;
			}
			for (const FVramImage& Picture : Pictures.ResultImages)
			{
				Vram->Upload(Picture);
			}
			Drawn = &Pictures.ResultSheet;
			Portraits = Pictures.ResultPortraits;
			PortraitFrame = FBattleHudResources::ResultPortraitFrame;
		}
		const auto ViewOf = [Drawn](int32 Frame)
		{
			const std::span<const FMenuSpritePart> Parts = Drawn->Frame(Frame);
			return Parts.empty() ? NoTextureView
				: PackTextureView(FTexturePage::FromAttribute(MenuGpu::TexturePage(Parts[0].Depth, 0, Parts[0].ImageX, Parts[0].ImageY)),
					  FClutPosition::FromAttribute(MenuGpu::Clut(Parts[0].ClutX, Parts[0].ClutY)));
		};
		std::array<uint32, FBattleHud::PartySlots> PortraitViews{};
		for (int32 Slot = 0; Slot < FBattleHud::PartySlots; ++Slot)
		{
			PortraitViews[static_cast<std::size_t>(Slot)] = ViewOf(PortraitFrame + Slot);
		}
		const std::vector<FTextureRegion> Regions = MenuSheetTextureRegions(*Drawn);
		for (const FTextureRegion& Region : Regions)
		{
			if (std::find(PortraitViews.begin(), PortraitViews.end(), Region.View) == PortraitViews.end())
			{
				Visit(*Vram, Region);
			}
		}
		const FMenuSpriteTexture Place = Drawn->Texture(PortraitFrame);
		for (std::size_t At = 0; At + PortraitBytes <= Portraits.size(); At += PortraitBytes)
		{
			TResult<FTim> Tim = ParseTim(Portraits.subspan(At, PortraitBytes));
			if (!Tim)
			{
				return Tim.Error();
			}
			if (Tim->Clut)
			{
				FVramImage Clut = *Tim->Clut;
				Clut.Rect.X = static_cast<int16>(Place.ClutX);
				Clut.Rect.Y = static_cast<int16>(Place.ClutY);
				Vram->Upload(Clut);
			}
			FVramImage Pixels = Tim->Image;
			Pixels.Rect.X = static_cast<int16>(Place.VramX);
			Pixels.Rect.Y = static_cast<int16>(Place.VramY);
			Vram->Upload(Pixels);
			for (const FTextureRegion& Region : Regions)
			{
				if (Region.View == PortraitViews[0])
				{
					Visit(*Vram, Region);
				}
			}
		}
		return std::nullopt;
	}

	FBattleHud::FBattleHud(const FBattleHudResources& InResources, FMesContext& InText, FVram& InVram, IVramUploads* InObserver)
		: Resources(InResources), Sheet(&InResources.Sheet), Text(InText), Vram(InVram), Observer(InObserver)
	{
		for (FVramImage& Row : BoxRows)
		{
			Row = TextImage(BoxUnits);
		}
		for (FVramImage& Digit : DigitImages)
		{
			Digit = TextImage(DigitUnits);
		}
	}

	void FBattleHud::Upload(const FVramImage& Image)
	{
		Vram.Upload(Image);
		if (Observer != nullptr)
		{
			Observer->Upload(Image);
		}
	}

	FBattleHudTexture FBattleHud::TextureOf(int32 Frame) const
	{
		const FMenuSpriteTexture Texture = Sheet->Texture(Frame);
		return {Texture.Depth, Texture.ClutX, Texture.ClutY, Texture.VramX, Texture.VramY};
	}

	void FBattleHud::Begin(FBattleContext& Context, const FBattleHudSettings& InSettings)
	{
		Battle = &Context;
		Settings = InSettings;
		int32 Members = 0;
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			const uint8 Occupant = Context.Slots[static_cast<std::size_t>(Slot)].Occupant;
			Members += (Occupant & NoMember) != NoMember;
			FBattleHudPanel& Panel = Panels[static_cast<std::size_t>(Slot)];
			const bool bInGear = Context.Slots[static_cast<std::size_t>(Slot)].InGear != 0;
			Panel.Mode = Occupant == NoMember ? 0 : bInGear && Occupant != ChuChu ? 2 : 1;
		}
		Layout = static_cast<uint8>(Members - 1);
		for (const FVramImage& Image : Resources.Images)
		{
			Upload(Image);
		}
		Upload({{TextClutX, TextClutY, static_cast<int16>(Resources.TextClut.size()), 1}, {Resources.TextClut.begin(), Resources.TextClut.end()}});
		EvenTextClut = MenuGpu::Clut(TextClutX, TextClutY);
		OddTextClut = MenuGpu::Clut(OddTextClutX, TextClutY);
		InitCommandIcons();
		LoadPortraits(Resources.Portraits, FBattleHudResources::PortraitFrame);
		InitPanelPrims();
		InitPanelSprites();
		InitFlash();
		RenderDigitLabels();
		InitGauges();
	}

	void FBattleHud::LoadPortraits(std::span<const uint8> Portraits, int32 FirstFrame)
	{
		const FBattleContext& Context = *Battle;
		for (int32 Slot = 0; Slot < PartySlots; ++Slot)
		{
			uint8 Character = Context.PartyCharacters[static_cast<std::size_t>(Slot)];
			if (Character == NoMember)
			{
				continue;
			}
			if (Context.bSpecialParty && Slot > 0)
			{
				Character = SpecialPartyPortrait;
			}
			const std::size_t At = PortraitBytes * Character;
			if (At + PortraitBytes > Portraits.size())
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			TResult<FTim> Tim = ParseTim(Portraits.subspan(At, PortraitBytes));
			if (!Tim)
			{
				FaultState = EBattleHudFault::IndexOutsideHud;
				return;
			}
			const FBattleHudTexture Texture = TextureOf(FirstFrame + Slot);
			if (Tim->Clut)
			{
				FVramImage Clut = *Tim->Clut;
				Clut.Rect.X = static_cast<int16>(Texture.ClutX);
				Clut.Rect.Y = static_cast<int16>(Texture.ClutY);
				Upload(Clut);
			}
			FVramImage Pixels = Tim->Image;
			Pixels.Rect.X = static_cast<int16>(Texture.VramX + 6 * Slot);
			Pixels.Rect.Y = static_cast<int16>(Texture.VramY);
			Upload(Pixels);
		}
	}

	void FBattleHud::RenderDigitLabels()
	{
		for (int32 Digit = 0; Digit < DigitLabels; ++Digit)
		{
			RenderMesString(Text, DigitTyping, Text.Tables != nullptr ? Text.Tables->String(DigitLabelTable, Digit) : std::span<const uint8>{},
				DigitImages[static_cast<std::size_t>(Digit)], 0);
		}
	}

	void FBattleHud::InitGauges()
	{
		// The gauges' four CLUT rows, as they lie now, stored four times each for the scrolling uploads.
		const std::array<int32, 4> RowOffsets{-1, 0, -2, -3};
		for (std::size_t Row = 0; Row < TextureRows.size(); ++Row)
		{
			const FVramRect Rect{TextureRowX, static_cast<int16>(GaugeTexture.ClutY + RowOffsets[Row]), TextureRowUnits, 1};
			TextureRows[Row] = Rect;
			StoredRows[Row].resize(static_cast<std::size_t>(TextureRowUnits) * TextureRowCopies);
			for (int32 Copy = 0; Copy < TextureRowCopies; ++Copy)
			{
				for (int32 Unit = 0; Unit < TextureRowUnits; ++Unit)
				{
					StoredRows[Row][static_cast<std::size_t>(Copy * TextureRowUnits + Unit)] = Vram.At(Rect.X + Unit, Rect.Y);
				}
			}
		}
		for (std::size_t Edge = 0; Edge < EdgeFrames.size(); ++Edge)
		{
			EdgeTextures[Edge] = TextureOf(EdgeFrames[Edge]);
		}
		// The window edges' first two frames are taken from their texels' own page, whatever the sheet says.
		EdgeTextures[0].VramX = 0x3C0;
		EdgeTextures[0].VramY = 0x34;
		EdgeTextures[1].VramX = 0x3C8;
		EdgeTextures[1].VramY = 0x34;
		TextureWindows[0] = {0, 0, 0x100, 0x100};
		TextureWindows[1] = {static_cast<int16>((GaugeTexture.VramX & 0x3F) << 1), static_cast<int16>(GaugeTexture.VramY), 0x100, 0x100};
		TextureWindows[2] = {static_cast<int16>((EdgeTextures[0].VramX & 0x3F) << 1), static_cast<int16>(EdgeTextures[0].VramY), 8, 0x10};
		TextureWindows[3] = {static_cast<int16>((EdgeTextures[1].VramX & 0x3F) << 1), static_cast<int16>(EdgeTextures[1].VramY), 8, 0x10};
		TextureWindows[4] = {static_cast<int16>((EdgeTextures[2].VramX & 0x3F) * 2 + 0xE), static_cast<int16>(EdgeTextures[2].VramY), 0x10, 8};
		TextureWindows[5] = {static_cast<int16>((EdgeTextures[3].VramX & 0x3F) * 2 + 0xE), static_cast<int16>(EdgeTextures[3].VramY), 0x10, 8};
		const uint16 Page = MenuGpu::TexturePage(GaugeTexture.Depth, 0, GaugeTexture.VramX, GaugeTexture.VramY);
		for (int32 Pair = 0; Pair < 2; ++Pair)
		{
			SetDrawMode(GaugeMode[static_cast<std::size_t>(Pair)], Page, TextureWindows[1]);
			SetDrawMode(PlainMode[static_cast<std::size_t>(Pair)], Page, TextureWindows[0]);
		}
	}

	void FBattleHud::Draw(FBattleHudFrame& Frame)
	{
		Frame.Primitives.clear();
		Drawing = &Frame;
		switch (Battle->Party.InputMode)
		{
		case ModeBattle:
			if (!bHidden)
			{
				GrowWindows();
				UpdatePartyPanels();
				BuildWaitGauges();
				DrawTurnIcons();
				DrawFlash();
				DrawPanels();
				LinkCommandIcons();
				LinkListSprites();
				DrawMessageBoxes();
				LinkHints();
				LinkListText();
				DrawDirectionArrows();
				DrawWindows();
				DrawReticle();
				LinkCockpit();
			}
			break;
		case ModeIntro:
			GrowWindows();
			DrawTurnIcons();
			LinkListSprites();
			DrawMessageBoxes();
			DrawEventMessage();
			DrawWindows();
			DrawReticle();
			LinkCockpit();
			break;
		case ModeResult:
			GrowWindows();
			DrawResult();
			DrawWindows();
			break;
		default:
			break;
		}
		// The ordering table's entry is drawn from the primitive linked last.
		std::reverse(Frame.Primitives.begin(), Frame.Primitives.end());
		// A text's buttons were noted by their sprites' places as linked.
		const uint32 Count = static_cast<uint32>(Frame.Primitives.size());
		for (FTextButtonCell& Cell : Frame.TextButtons)
		{
			Cell.Primitive = Count - 1 - Cell.Primitive;
			Cell.LineRest = Cell.LineRest != 0 ? Count - Cell.LineRest + 1 : 0;
		}
		Drawing = nullptr;
	}

	void FBattleHud::StepMenuInput()
	{
		Flight.Steps = static_cast<uint8>(Flight.Steps + 6);
		Flight.Growth = static_cast<uint8>(Flight.Growth + 1);
		if (bFlashOn)
		{
			if (!bFlashFalling)
			{
				FlashGlow += GlowStep;
				if (FlashGlow > 0x80)
				{
					FlashGlow = 0x80 - GlowStep;
					bFlashFalling = true;
				}
			}
			else
			{
				FlashGlow -= GlowStep;
				if (FlashGlow < 0)
				{
					FlashGlow = GlowStep;
					bFlashFalling = false;
				}
			}
		}
		AnimateMenuTextures();
	}

	void FBattleHud::AnimateMenuTextures()
	{
		++TextureRowStep;
		if ((TextureRowStep & 1) != 0)
		{
			for (std::size_t Row = 0; Row < TextureRows.size(); ++Row)
			{
				const std::size_t From = TextureRowOffset * 2;
				FVramImage Image;
				Image.Rect = TextureRows[Row];
				Image.Units.assign(StoredRows[Row].begin() + static_cast<std::ptrdiff_t>(From),
					StoredRows[Row].begin() + static_cast<std::ptrdiff_t>(From + TextureRowUnits));
				Upload(Image);
			}
			TextureRowOffset += 4;
			if (TextureRowOffset > TextureRowWrap)
			{
				TextureRowOffset = 0;
			}
		}
	}

	void FBattleHud::Link(const FMenuPrimitive& Primitive)
	{
		// A primitive never built is an empty packet the GPU passes over.
		if (Primitive.Code != 0)
		{
			Drawing->Primitives.push_back(Primitive);
		}
	}

	void FBattleHud::LinkPairs(std::span<const FBattleHudPair> Pairs, int32 Count, uint8 From)
	{
		for (int32 Pair = 0; Pair < Count; ++Pair)
		{
			if (Pair >= static_cast<int32>(Pairs.size()))
			{
				FaultState = EBattleHudFault::PartsOutsidePrimitives;
				return;
			}
			Link(Pairs[static_cast<std::size_t>(Pair)][From & 1]);
		}
	}

	int32 FBattleHud::DrawSheet(int32 Frame, std::span<FBattleHudPair> Pairs, int32 First, int32 X, int32 Y, int32 Scale)
	{
		const std::span<const FMenuSpritePart> Parts = Sheet->Frame(Frame);
		const auto Scaled = [Scale](int32 Value) {
			int32 Product = static_cast<int32>(static_cast<uint32>(Value) * static_cast<uint32>(Scale & 0xFFFF));
			if (Product < 0)
			{
				Product += 0xFFF;
			}
			return static_cast<int16>(Product >> 12);
		};
		// The buttons' pictures are the HUD's own sheet's.
		const FHudButton Button = Sheet == &Resources.Sheet ? HudButton(Frame) : FHudButton{};
		int32 Index = First;
		for (const FMenuSpritePart& Part : Parts)
		{
			if (Index >= static_cast<int32>(Pairs.size()))
			{
				FaultState = EBattleHudFault::PartsOutsidePrimitives;
				return static_cast<int32>(Parts.size());
			}
			FMenuPrimitive& Quad = Pairs[static_cast<std::size_t>(Index)][Buffer];
			++Index;
			Quad.ButtonRole = Button.Role;
			Quad.ButtonSlot = Button.Slot;
			BattleHudGpu::Set(Quad, TexturedQuad);
			SetSemiTrans(Quad, false);
			SetShadeTex(Quad, true);
			Quad.Page = MenuGpu::TexturePage(Part.Depth, 0, Part.ImageX, Part.ImageY);
			Quad.Clut = MenuGpu::Clut(Part.ClutX, Part.ClutY);
			const int16 Left = static_cast<int16>(X + Scaled(Part.X));
			const int16 Right = static_cast<int16>(Left + Scaled(Part.Width));
			const int16 Top = static_cast<int16>(Y + Scaled(Part.Y));
			const int16 Bottom = static_cast<int16>(Top + Scaled(Part.Height));
			uint16 U = Part.U;
			uint16 V = Part.V;
			uint8 Width = static_cast<uint8>(Part.Width);
			uint8 Height = static_cast<uint8>(Part.Height);
			// A mirrored part's texels start one lower, as far as the page's first one.
			if (Part.bFlipX)
			{
				U = static_cast<uint16>(Part.U - 1);
				if (static_cast<int16>(U) < 0)
				{
					U = 0;
					--Width;
				}
			}
			if (Part.bFlipY)
			{
				V = static_cast<uint16>(Part.V - 1);
				if (static_cast<int16>(V) < 0)
				{
					V = 0;
					--Height;
				}
			}
			const int16 X0 = Part.bFlipX ? Right : Left;
			const int16 X1 = Part.bFlipX ? Left : Right;
			const int16 Y0 = Part.bFlipY ? Bottom : Top;
			const int16 Y1 = Part.bFlipY ? Top : Bottom;
			SetCorners(Quad, X0, Y0, X1, Y0, X0, Y1, X1, Y1);
			SetTexels(Quad, U, V, U + Width, V, U, V + Height, U + Width, V + Height);
		}
		return static_cast<int32>(Parts.size());
	}
}
