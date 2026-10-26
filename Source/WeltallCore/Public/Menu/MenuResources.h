#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Game/CharacterEquipment.h"
#include "Game/GearPartTables.h"
#include "Gears/GearShopModel.h"
#include "Graphics/GteMath.h"
#include "Graphics/Vram.h"
#include "Media/DiscIdentification.h"
#include "Menu/MenuModuleTables.h"
#include "Menu/MenuSpriteSheet.h"
#include "Text/MessageTable.h"

#include <array>
#include <span>
#include <utility>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;

	/**
	 * The tables of the Menu's main module (directory 16, file 5) its screens are laid out by: the arc's
	 * places, each arc's commands, the offsets of the labels beside them and the messages of the text
	 * labels each screen renders.
	 */
	struct FMenuMainTables
	{
		static constexpr int32 ArcPlaces = 11;
		static constexpr int32 SubArcRows = 11;
		static constexpr int32 SubArcCommands = 4;
		/** A sub-arc row's end: the frame numbers after its last command. */
		static constexpr int32 NoCommand = 0xFFFF;

		/** The command arc's places from the bottom, the Normal Menu's seven, then the four of the inner arc. */
		std::array<int32, ArcPlaces> ArcX{};
		std::array<int32, ArcPlaces> ArcY{};
		/** The title's commands from the bottom: Sound, Continue, New Game. */
		std::array<FMenuArcCommand, 3> TitleCommands{};
		/** The Normal Menu's commands from the bottom: Exit, File, Gear, Abilities, Items, Equip, Status. */
		std::array<FMenuArcCommand, 7> MainCommands{};
		/** The inner arcs, by row (the command that opens one, plus the title's 7): up to four commands, then NoCommand. */
		std::array<std::array<FMenuArcCommand, SubArcCommands>, SubArcRows> SubCommands{};
		/** How far right of its orb each command's text label sits: the title's, the Normal Menu's and the Sound option's. */
		std::array<int32, 3> TitleLabelOffsets{};
		std::array<int32, 7> MainLabelOffsets{};
		std::array<int32, 3> SoundLabelOffsets{};
		/** The menu texts' numbers the text labels are rendered from: the help labels, the title's commands', the Sound option's. */
		std::array<uint8, 4> HelpLabels{};
		std::array<uint8, 8> TitleLabels{};
		std::array<uint8, 4> SoundLabels{};
		std::array<uint8, 8> MainLabels{};
		/**
		 * Where each pair of names is rendered, by strip (a member's and its gear's per party slot, then the
		 * File screen's three members of a save): x less 384 and y in VRAM. A name reads its strip: from x's
		 * low bits times 4 and y's low byte in the page at (384, 0).
		 */
		std::array<int32, 9> NameStripX{};
		std::array<int32, 9> NameStripY{};
		/** A party panel's sprite-font labels (frame, or NoCommand for none) and their places from its origin. */
		std::array<int32, 20> PanelLabels{};
		std::array<int32, 20> PanelLabelX{};
		std::array<int32, 20> PanelLabelY{};
		/** Where a panel's numbers start: level, ether level, HP, max HP, EP, max EP, EXP to next, ether EXP to next. */
		std::array<std::array<int32, 2>, 8> PanelNumbers{};
		/** Where a panel's name lies from its origin. */
		std::array<int32, 2> PanelName{};

		/** The File screen's inner arc's text labels (where saving is allowed or not, then the title's), and their offsets. */
		std::array<std::array<uint8, 6>, 2> FileLabels{};
		std::array<int32, 3> FileLabelOffsets{};
		/** The two cards' cells: each one's place on the screen (16 a card, the 16th never drawn), and the cell each of the cursor's 30 places is. */
		std::array<int32, 32> CellX{};
		std::array<int32, 32> CellY{};
		std::array<int32, 30> CursorCells{};
		/**
		 * A save's details, per member 80 pixels apart: the sprite-font labels (a frame, or NoCommand for
		 * none) and their places; the portraits' places; where the level, ether level, HP, max HP, EP and
		 * max EP start; the play time's colons and digits.
		 */
		std::array<int32, 9> DetailLabels{};
		std::array<int32, 9> DetailLabelX{};
		std::array<int32, 9> DetailLabelY{};
		std::array<int32, 3> DetailPortraitX{};
		std::array<int32, 3> DetailPortraitY{};
		std::array<std::array<int32, 2>, 6> DetailNumbers{};
		std::array<int32, 2> DetailColonX{};
		std::array<int32, 7> DetailTimeX{};
		/** The save's place name: its first character's cell. */
		std::array<int32, 2> PlaceCell{};

		/**
		 * The status sheet (the Status and Gear screens): its sprite-font labels (a frame, or NoCommand for
		 * none; 24 a kind, the character's then the gear's) and their places.
		 */
		std::array<int32, 48> SheetLabels{};
		std::array<int32, 48> SheetLabelX{};
		std::array<int32, 48> SheetLabelY{};
		/**
		 * Where the sheet's numbers start: level, ether level, HP, max HP, EP, max EP, EXP, ether EXP,
		 * EXP to next, ether EXP to next, weight.
		 */
		std::array<std::array<int32, 2>, 11> SheetNumbers{};
		std::array<int32, 2> SheetName{};
		/** The texts of the sheet's text labels: the inner arc's three, then its equipment's headings. */
		std::array<uint8, 6> SheetTexts{};
		std::array<int32, 3> SheetHeadingX{};
		std::array<int32, 3> SheetHeadingY{};
		/** How far right of its orb each of the sheet's inner arc's text labels sits. */
		std::array<int32, 4> SheetSubLabelOffsets{};
		/** The rows of equipment names, by kind (from 0: a character's, 5 and 9 the gears'). */
		std::array<int32, 16> EquipNameY{};
		/** The stats' labels (7 a kind) and their places from the stats' origin. */
		std::array<int32, 14> StatLabels{};
		std::array<int32, 7> StatLabelX{};
		std::array<int32, 7> StatLabelY{};
		/** From a stat's row: the bar's x and y, the value's x and y. */
		std::array<int32, 4> StatRow{};
		/** The "Walk" and "Gear" badges. */
		std::array<int32, 2> RideBadges{};
		/** The L1 and R1 hints' x, and their y by screen. */
		std::array<int32, 2> MemberHintX{};
		std::array<int32, 6> MemberHintY{};

		/** The bits of a learned-skills word by skill: 0x8000 >> n. */
		std::array<uint16, 16> SkillBits{};
		/** Each Deathblow's combo as the Skills screen shows it: five sheet frames (× △ □) or 0xFF, the same for every character. */
		std::array<uint8, 65> DeathblowCombos{};
		/** The Skills screen's text labels (its title). */
		std::array<uint8, 2> SkillsTexts{};
		/** Where a screen's member name lies, by screen. */
		std::array<int32, 4> MemberNameX{};
		std::array<int32, 4> MemberNameY{};

		/**
		 * The texts of the screens' label sets, by set: the lists' tags (a character's, a gear's), the
		 * Equip screen's slots (a character's, a gear's), the Deathblows' title, the Gear screen's
		 * labels (walking, riding).
		 */
		std::array<uint8, 0x2C> LabelSets{};
		/** The red pointer beside the Equip screen's slots: its top by page and slot. */
		std::array<int32, 8> EquipPointerY{};
		/** Where a label is placed on a screen's left, by label (the Equip screen's slots). */
		std::array<int32, 16> SlotLabelY{};
		/** The four pointers the target windows and the title's end use: their places. */
		std::array<int32, 4> TargetPointerX{};
		std::array<int32, 4> TargetPointerY{};
		/**
		 * A target window's member panel, a character's then a gear's: its sprite-font labels (nine, or
		 * NoCommand), and the places of the labels, the portrait, the level, the ether level, HP, max HP,
		 * EP, max EP and the name, from the panel's origin (a slot's panel 56 rows below the one before).
		 */
		std::array<int32, 18> TargetLabels{};
		std::array<std::array<int32, 17>, 2> TargetX{};
		std::array<std::array<int32, 17>, 2> TargetY{};
		/** The item and ability lists' tags: their places. */
		std::array<int32, 8> HelpTagX{};
		int32 HelpTagY = 0;
		/** The Abilities screen's tags (a character's eight, then a gear's own), and their rows: the side's, the points'. */
		std::array<int32, 16> AbilityTagX{};
		std::array<int32, 2> AbilityTagY{};
		/** The Ethers each character can use from the menu: bit n, the n-th ability. */
		std::array<uint16, 16> EtherMasks{};
		/** Where the Abilities screen puts each entry's cost (and the points, entries 12 and 13). */
		std::array<int32, 14> CostX{};
		std::array<int32, 14> CostY{};
		/** The Abilities screen's windows by kind: the list's width, the points' place and width. */
		std::array<int32, 3> AbilityListWidth{};
		std::array<int32, 3> AbilityPointsX{};
		std::array<int32, 3> AbilityPointsWidth{};
		/** How far right of its orb each of the Gear screen's inner arc's labels sits: walking, then riding. */
		std::array<int32, 12> GearArcLabelOffsets{};

		/** From the module as the game loads it; the tables lie at the same places in every release's copy. */
		WELTALLCORE_API static TResult<FMenuMainTables> Parse(std::span<const uint8> Module);
	};

	/**
	 * What the Menu program reads and keeps while it runs: its data file (directory 16, file 1: the TIM
	 * archive of its pictures, the sprite sheet, the menu's text, the party's large portraits), its main
	 * module's tables and the kernel's colours for the message font.
	 */
	class FMenuResources
	{
	public:
		static constexpr int32 PortraitCount = 11;
		static constexpr std::size_t PortraitBytes = 0xB20;

		/**
		 * From the data file as read, the main module, the kernel's two 16-colour text CLUTs and the game's
		 * data file (the equipment's tables).
		 */
		WELTALLCORE_API static TResult<FMenuResources> Parse(std::span<const uint8> DataFile, std::span<const uint8> MainModule,
			const std::array<uint16, 32>& TextClut, std::span<const uint8> GameData);

		/** Read from a disc as the Menu reads them; the kernel is the disc's boot executable. */
		WELTALLCORE_API static TResult<FMenuResources> Load(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Kernel);

		FMenuResources(FMenuResources&&) = default;
		FMenuResources& operator=(FMenuResources&&) = default;
		FMenuResources(const FMenuResources&) = delete;
		FMenuResources& operator=(const FMenuResources&) = delete;

		/** The TIM archive's images as the menu uploads them: from the archive's last TIM to its first, each CLUT before its pixels. */
		const std::vector<FVramImage>& ArchiveUploads() const { return Archive; }
		const FMenuSpriteSheet& Sheet() const { return SpriteSheet; }
		const FMessageTable& Text() const { return TextTable; }
		/** A character's large portrait: a TIM. */
		WELTALLCORE_API std::span<const uint8> Portrait(int32 Character) const;
		/** The icon a save shows on a memory card (the data file's section 1): its 16 colours and its 16 x 16 pixels, 4 bits each. */
		const std::array<uint8, 0x20>& SaveIconColours() const { return IconColours; }
		const std::array<uint8, 0x80>& SaveIconPixels() const { return IconPixels; }
		/**
		 * The places a save is made at (directory 17, file 1): a line of 15 full-width characters and a
		 * carriage return for each value of the story's progress.
		 */
		std::span<const uint8> PlaceNames() const { return PlaceNameBytes; }
		const FMenuMainTables& Main() const { return MainTables; }
		/** The other modules' tables. */
		const FMenuMemberTables& Member() const { return MemberTables; }
		const FMenuNameTables& Name() const { return NameTables; }
		const FMenuShopTables& Shop() const { return ShopTables; }
		const FMenuGearShopTables& GearShop() const { return GearShopTables; }
		/** The gears the gear shop shows: each one's model, animation, floor and scale. */
		const FGearShopTable& GearShopModels() const { return GearShopModelTable; }
		/** libgte's arctangents, which the gears' cloths hang by. */
		const FGteArctangents& Arctangents() const { return ArctangentTable; }
		/** The shops' stock (the data file's section 7): 0x5C bytes a shop, 30 weapons, 30 accessories, 30 items. */
		std::span<const uint8> ShopStock() const { return ShopStockTable; }
		/**
		 * The gear shops' stock (the data file's section 8): 0x64 bytes a shop, lists of 20 ids: frames,
		 * armors, engines, gear weapons, parts.
		 */
		std::span<const uint8> GearShopStock() const { return GearShopStockTable; }
		/**
		 * Enter Name's table (the data file's section 6): for the gears' names (11 on), the character
		 * whose portrait shows; from 0x20, the three name slots each name number writes.
		 */
		std::span<const uint8> NameEntry() const { return NameEntryTable; }
		const std::array<uint16, 32>& TextClut() const { return TextColours; }
		/**
		 * The Menu's programs are the French translation's (its main module, which the others come with): they place some
		 * things otherwise for its variable-width text.
		 */
		bool IsFrench() const { return bFrench; }
		/**
		 * The title the French translation's Menu shows for a save made at a story value (event variable 0): its own
		 * chapters' titles, each up to a story value, rather than the card's title. Empty for the game's own Menu.
		 */
		WELTALLCORE_API std::span<const uint8> FrenchChapterTitle(uint16 Progress) const;
		/** The weapons' and accessories' records the screens recompute characters from. */
		const FEquipmentTables& Equipment() const { return EquipmentTables; }
		/** A character's command records (38 of 0x28 bytes: its attacks, Deathblows, abilities), by character id. */
		WELTALLCORE_API std::span<const uint8> CharacterCommands(int32 Character) const;
		/** The growth and learning table: 11 records of 0x110 bytes, the Deathblows' thresholds first. */
		std::span<const uint8> Growth() const { return GrowthTable; }
		/** The gears' frames, armors, engines and parts, and each gear's pilot. */
		const FGearPartTables& GearParts() const { return GearPartTables; }
		/** The gear weapons' records (0x14 bytes each). */
		std::span<const uint8> GearWeapons() const { return GearWeaponTable; }
		/** The items' records (0x10 bytes each). */
		std::span<const uint8> Items() const { return ItemTable; }
		/** A gear's command records (0x28 bytes each: its attacks, then its abilities), by gear id. */
		WELTALLCORE_API std::span<const uint8> GearCommands(int32 Gear) const;

		/** The texts the screens read from the game's data file, each a message table. */
		enum class EText : uint8
		{
			/** An item's description, by item. */
			ItemHelp,
			/** Two lines by ability: a character's (32 a character), a gear's list (32 a gear), a gear's own (8 a gear). */
			AbilityHelp,
			GearAbilityHelp,
			GearOptionHelp,
			/** Three lines by equipment: weapons, accessories, gear weapons, gear parts. */
			WeaponHelp,
			AccessoryHelp,
			GearWeaponHelp,
			GearPartHelp,
			/** The shops' lines: weapons, accessories, items. */
			ShopWeaponHelp,
			ShopAccessoryHelp,
			ShopItemHelp,
			/** The gear shop's lines: gear weapons, parts, armors, frames, engines. */
			GearShopWeaponHelp,
			GearShopPartHelp,
			GearArmorHelp,
			GearFrameHelp,
			GearEngineHelp,
			/** The names of the gears' armors, frames and engines. */
			GearArmorNames,
			GearFrameNames,
			GearEngineNames,
			Count
		};
		const FMessageTable& Text(EText Which) const { return Texts[static_cast<std::size_t>(Which)]; }
		/** The section of the menus' data (directory 16, file 2) that holds a kind of text, from 1. */
		WELTALLCORE_API static int32 SectionOfText(EText Which);

	private:
		FMenuResources() = default;

		/** The prompts' and descriptions' tables with the player's messages the image gives in place. */
		void UsePlayersMessages(const FCdImage& Image);

		std::vector<FVramImage> Archive;
		FMenuSpriteSheet SpriteSheet;
		std::vector<uint8> TextBytes;
		FMessageTable TextTable;
		std::vector<uint8> Portraits;
		std::array<uint8, 0x20> IconColours{};
		std::array<uint8, 0x80> IconPixels{};
		std::vector<uint8> PlaceNameBytes;
		FMenuMainTables MainTables;
		FMenuMemberTables MemberTables;
		FMenuNameTables NameTables;
		FMenuShopTables ShopTables;
		FMenuGearShopTables GearShopTables;
		FGearShopTable GearShopModelTable;
		FGteArctangents ArctangentTable;
		std::vector<uint8> ShopStockTable;
		std::vector<uint8> GearShopStockTable;
		std::vector<uint8> NameEntryTable;
		std::array<uint16, 32> TextColours{};
		bool bFrench = false;
		/** The French Menu's chapters: the story value each ends before, and its title's codes ended by 00. */
		std::vector<std::pair<uint16, std::vector<uint8>>> FrenchChapters;
		FEquipmentTables EquipmentTables;
		std::array<std::vector<uint8>, 11> Commands{};
		std::vector<uint8> GrowthTable;
		FGearPartTables GearPartTables;
		std::vector<uint8> GearWeaponTable;
		std::vector<uint8> ItemTable;
		std::array<std::vector<uint8>, 20> GearCommandTables{};
		std::array<std::vector<uint8>, static_cast<std::size_t>(EText::Count)> TextBytesByKind{};
		std::array<FMessageTable, static_cast<std::size_t>(EText::Count)> Texts{};
	};

	/** The game's own icon: the save icon a memory card shows, as 16 x 16 pixels, red, green, blue and alpha (its colour 0 clear). */
	struct FGameIcon
	{
		static constexpr int32 Size = 16;
		std::array<std::array<uint8, 4>, Size * Size> Pixels{};
	};

	/** The save icon of the Menu's data file (directory 16, file 1, section 1) on a disc. */
	WELTALLCORE_API TResult<FGameIcon> ReadGameIcon(const FCdImage& Image, const FGameFileIndex& Index);
}
