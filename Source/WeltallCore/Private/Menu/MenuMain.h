#pragma once

#include "Game/CharacterEquipment.h"
#include "Game/StoryMode.h"
#include "Graphics/Vram.h"
#include "Menu/MenuMainFileParts.h"
#include "Menu/MenuMainParts.h"
#include "Menu/MenuModule.h"
#include "Menu/MenuPrimitive.h"
#include "Menu/MenuControlsPage.h"
#include "Menu/MenuModFiles.h"
#include "Menu/MenuSavePages.h"
#include "Menu/MenuSettingsPages.h"
#include "Menu/MenuProgram.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuTask.h"
#include "Text/MesWindow.h"

#include <array>
#include <coroutine>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * The Menu's main module (its Normal Menu, title screen and disc change) as it runs: its state and
	 * code, each routine of the original as a member, the ones that wait for frames as TMenuTasks.
	 */
	class FMenuMain final : public FMenuModule
	{
	public:
		FMenuMain(const FMenuServices& InServices, const FMenuRequest& InRequest);

		TMenuTask<> Run() override;

	private:
		// The frame (MenuMainFrame.cpp).
		void DrawFrame() override;
		void DrawScene();
		void DrawTitle();
		void DrawBackground();
		void AddLabels();
		void AddTargetPointers();

		// Setting up and ending (MenuMain.cpp).
		void Init();
		void InitParty();
		void LoadResources();
		void InitBackground();
		TMenuTask<> Shutdown();


		// The Normal Menu (MenuMainNormal.cpp) and its party panels (MenuMainParty.cpp).
		void OpenNormalMenuWindows();
		TMenuTask<> OpenNormalMenu();
		TMenuTask<> MainMenu();
		void DrawNormalMenu();
		void RenderName(uint8 Name, int32 Strip);
		/** A member's name and its gear's into its slot's strips. */
		void RenderMemberNames(int32 Slot);
		/** Where the panels rest, x and y by slot. */
		static constexpr std::array<std::array<int32, 2>, 3> PanelPlaces = {{{0x60, 6}, {0x68, 0x3E}, {0x70, 0x76}}};
		TMenuTask<> SlideParty(bool bIn, bool bKeepsGold);
		void StartSlide(int32 FromX, int32 FromY, int32 ToX, int32 ToY, int32 Steps, int32 Slot);
		void StepSlide(int32 Slot);
		void DrawPartyPanel(int32 Slot, uint8 Character);
		void DrawDigits(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked);
		void AddPartyPanels();
		void ShowGold();
		void DrawGold(int32 X, int32 Y);
		void ShowPlayTime();
		void DrawPlayTime(int32 X, int32 Y);
		/** Where the play time's first digit is: its window's corner and margin. */
		static constexpr int32 PlayTimeX = 0xD0;
		static constexpr int32 PlayTimeY = 0xCA;
		void AddGoldAndTime();
		/** A play time's seven digits as the menu draws them: hours (three), minutes, seconds. */
		static std::array<uint32, 7> SplitPlayTime(uint32 Time);
		/** Perfect Works Build's rebalance is in play: its menu's rules for items and shown stats. */
		bool UsesPerfectWorksRules() const;

		// Arranging the party on its panels (MenuMainOrder.cpp), the port's: Right from the arc, a member picked, then another
		// to swap with; Triangle puts the story's order back.
		TMenuTask<> ArrangeParty();
		/**
		 * The next of the shown members up or down the panels from Slot but Skipped, round past the ends (down from the
		 * last: the first member).
		 */
		int32 NextPanel(int32 Slot, bool bUp, int32 Skipped = -1) const;
		/** Target pointer Target beside the panel of Slot as it rests. */
		void PointAtPanel(std::size_t Target, int32 Slot);
		/** The panels drawn where they are, those not in LitSlots (a bit a slot) dimmed. */
		void ShadePanels(uint32 LitSlots);
		/** Two members trade slots, their panels sliding each to the other's place; the game keeps the order shown. */
		TMenuTask<> SwapPanels(int32 First, int32 Second);
		/** The party shown in the story's order again, the game keeping no order of the player's. */
		TMenuTask<> RestoreStoryOrder();
		/** Each slot shows the member slot From[Slot] showed, the panels that move sliding there together. */
		TMenuTask<> MovePanels(const std::array<int32, 3>& From);
		/** The game's party slot a menu slot shows: the menus show the party in the order its player keeps it (FPartyOrder). */
		int32 GameSlot(int32 Slot) const { return GameSlots[static_cast<std::size_t>(Slot)]; }

		// The Status screen and its sheet (MenuMainStatus.cpp, MenuMainSheet.cpp).
		TMenuTask<bool> StatusScreen();
		void OpenStatus();
		void ShowSheetLabels(bool bShown);
		void PlaceSheetSubLabel();
		TMenuTask<> DrawStatusSheet(int32 Slot, bool bGear);
		void BuildStatusSheet(int32 Slot, bool bGear);
		void DrawRideBadge(int32 Slot, bool bGear);
		void DrawSheetDigits(std::vector<FMenuPrimitive>& Digits, uint32 Value, int32 First, int32 Count, int32 X, int32 Y, bool bPacked);
		/** The stats: where the Status screen or the Equip screen puts them, compared with those kept aside, a gear's. */
		void DrawStats(bool bEquipPlace, bool bCompare, bool bGear);
		/** The equipment's names: the sheet's (Kind 0) or an Equip page's (1, 2), from the slots kept aside. */
		void DrawEquipNames(int32 Slot, int32 Kind, bool bSaved, bool bGear);
		void DrawMemberHints(int32 Kind, bool bGears);
		int32 NextMember(int32 Slot, bool bPrevious, bool bGearsOnly) const;
		TMenuTask<> CloseCommand(int32 Offset);
		void AddStatusSheet();
		void AddStats();
		void AddEquipNames();
		void AddMemberHints();

		// The Skills screen (MenuMainSkills.cpp).
		TMenuTask<> SkillsScreen(int32 Slot);
		void OpenSkills();
		TMenuTask<> CloseSkills();
		void DrawDeathblows(int32 Slot);
		void DrawDeathblowRow(int32 Row, int32 Kind, uint32 Percent);
		void DrawMemberName(FMenuPrimitive& Name, int32 Slot, int32 Place, bool bGear);
		void AddDeathblows();

		// The Equip screen (MenuMainEquip.cpp, MenuMainEquipList.cpp, MenuMainEquipRules.cpp).
		TMenuTask<bool> EquipScreen(int32 Slot);
		TMenuTask<> Equip(int32 Slot, bool bFromArc, bool bGear);
		void OpenEquip(bool bGear);
		TMenuTask<> CloseEquip();
		void SaveEquip(int32 Slot, bool bGear);
		void RevertEquip(int32 Slot, bool bGear);
		void PreviewEquip(int32 Slot, int32 Which, int32 Row, int32 Top, int32 Page, bool bGear);
		bool CommitEquip(int32 Slot, int32 Which, int32 Page, bool bGear);
		void ReturnBillyAmmo(int32 Slot, bool bGear);
		/** A type of Billy's ammunition's shots left: his guns', or his gear's. */
		uint8& AmmunitionCount(bool bGear, uint8 Ammunition);
		/** Another of the member's guns (or its gear's) than At holds the ammunition. */
		bool OtherGunHolds(int32 Slot, bool bGear, std::size_t At, uint8 Ammunition);
		/** A load changed with its shots kept (FMenuOptions::bAmmunitionKept), the inventory with it. */
		void KeepAmmunition(int32 Slot, bool bGear, std::size_t At, uint8 New, uint8 Old);
		int32 BuildEquipList(int32 Slot, int32 Top, int32 Which, int32 Page, bool bGear);
		void DrawEquipSlots(int32 Page);
		void DrawEquipDescription(int32 Which, int32 Row, int32 Top, int32 Page, bool bGear, bool bWorn, int32 Slot);
		void PlaceEquipPointer(int32 Page, int32 Which);
		void AddEquipList();
		/** The state's byte at Offset of a member's record, or of its gear's. */
		uint8& RecordByte(int32 Slot, bool bGear, std::size_t Offset);

		// The Items screen (MenuMainItems.cpp, MenuMainItemUse.cpp) and the target windows (MenuMainTargets.cpp).
		TMenuTask<bool> ItemsScreen();
		void OpenItems();
		void CloseItems();
		void CountItemRows();
		void DrawItemPage(int32 Top);
		void DrawItemHelp(int32 Cursor, int32 Top);
		TMenuTask<bool> UseItem(int32 Top, int32 Cursor);
		/** The item list's pieces dimmed (a target window over them) or brightened again. */
		void DimItemList(bool bDim);
		void AddItemList();
		void DrawTargets(int32 Kind);
		void BuildTargetPanel(FMenuTargetPanel& Panel, uint8 Character, int32 Slot, bool bGear);
		void AddTargetPanels();
		/** A window's pieces dimmed or brightened (their colour a quarter, or whole). */
		void DimWindow(int32 Index, bool bDim);
		/** The textured pieces' shading as the lists change it: 0 bright, 1 dimmed and blended, 2 bright, 3 dim. */
		static void ShadePiece(FMenuPrimitive& Piece, int32 Mode);
		/** The item's record, empty past the table. */
		std::span<const uint8> ItemRecordOf(uint8 Item) const;

		// The Abilities screen (MenuMainAbilities.cpp, MenuMainAbilityUse.cpp): a character's Ethers (Kind
		// 0), a gear's abilities (1) or its own (2).
		TMenuTask<> AbilitiesScreen(int32 Slot, bool bFromArc, int32 Kind);
		void OpenAbilities(int32 Kind);
		TMenuTask<> CloseAbilities();
		void BuildAbilityList(int32 Slot, int32 Kind);
		void DrawAbilityHelp(int32 Slot, int32 Index, int32 Kind);
		TMenuTask<> UseAbility(int32 Slot, int32 Index, int32 Kind);
		void ApplyAbility(uint8 Caster, uint8 Target, int32 Index, int32 Kind);
		/** An ability's command record: a character's Ethers, a gear's list or its own abilities. */
		std::span<const uint8> AbilityRecord(int32 Slot, int32 Index, int32 Kind) const;
		/** What the ability costs the member now: its EP, or for a gear's own its fuel (the fix that heals as battle does doubles an Ether's with Ether Doubler). */
		int32 AbilityCost(int32 Slot, int32 Index, int32 Kind) const;
		/** What a gear's own ability costs in fuel (by the gear's record order: Index is its row pair). */
		int32 GearOwnCost(int32 Slot, int32 Ability) const;
		void DimAbilityList(bool bDim);
		void AddAbilityList();

		// The Gear screen (MenuMainGear.cpp): the gear's sheet, its inner arc, boarding.
		TMenuTask<bool> GearScreen();
		int32 OpenGear();
		void SetGearLabels(bool bRiding);
		/** A member's gear recomputed as the Gear screen and gear Equip recompute it, its stats for the sheet. */
		void RecomputeGear(int32 Slot);

		// The lists' pieces (MenuMainListPieces.cpp): cursors, the scroll bar's knob, the member's name.
		void AllocCursor(int32 Which);
		void DrawCursor(int32 Row, int32 Top, int32 Kind, int32 Which);
		/** The cursor's arrow at a place of the layer. */
		void DrawCursorAt(int32 X, int32 Y, int32 Which);
		/** The arrow's next cel, as each frame that draws it turns it. */
		void AdvanceCursor(int32 Which);
		void PlaceCursor(int32 X, int32 Y, int32 Which);
		void DrawScrollKnob(int32 X, int32 Y, int32 Height);
		void AddCursors();
		void AddScrollKnob();

		// The title screen (MenuMainTitle.cpp).
		TMenuTask<> TitleMenu();
		TMenuTask<bool> RunCommand(int32 Offset);
		TMenuTask<bool> SoundOption();
		TMenuTask<> RequireDisc(int32 Disc);
		/** The place of the title's arc a command stands at, and the command at a place: an Exit orb at the foot raises the others a place. */
		int32 TitlePlace(int32 TitleCommand) const;
		int32 TitleCommandAt(int32 Place) const;
		void InitTargetPointers(int32 Kind);
		/** The red pointer at half its size as target pointer Place, at a place of the screen. */
		void PlaceTargetPointer(std::size_t Place, int32 X, int32 Y);

		// The options screen (MenuMainOptions.cpp), the port's: the player's settings over pages, in the lists' style.
		/** The arcs as this menu shows them: the title's Sound as Options and its Exit orb, the Normal Menu's eighth orb, its place. */
		void InitArcs();
		TMenuTask<bool> OptionsScreen();
		ESettingsPage SettingsPage() const;
		bool OnSavesPages() const;
		/** The Modding page, the last L1 and R1 turn to, whether or not it has opened the Assets page. */
		bool OnModdingTurned() const;
		/**
		 * The rows the screen adds under the Modding page's own: the mods' heading, the Assets page's row (External
		 * assets itself where the device's files go through its picker) and each mod's, those the menu has; then the
		 * device's rows.
		 */
		void AddModdingRows();
		/** Left or Right on one of those rows: a mod's or External assets switched on or off; false on the others. */
		bool StepModdingRow();
		/** Confirm on one of those rows: the Assets page or the mod's page opened from it, External assets switched, a device's row's action; false on a heading. */
		bool ActivateModdingRow();
		/** Where the device's rows start on the Modding page. */
		int32 ModFilesRow() const;
		/** Follows the device's transfer on the Modding page, the cursor kept on its row as rows above it come and go: true when the rows changed. */
		bool RefreshModFiles();
		/** The Assets page, opened from the Modding page. */
		bool OnAssetsPage() const;
		void FillAssetRows();
		bool RefreshAssetProgress();
		bool StepAssetOption();
		bool ActivateAssetOption();
		bool OnControlsPage() const;
		/** How many pages L1 and R1 turn through: the settings', the controls' and the saves' where the menu has them, then Modding. */
		int32 TurnedPages() const;
		/** The page's title and rows as they stand. */
		void FillOptionsRows();
		/** The page's rows again, the cursor on its first row or kept where it was. */
		void ShowOptionsPage(bool bFromTop);
		/** Left, Right, L1 or R1, Confirm, Cancel: false where nothing changed (Cancel: the screen closes). */
		bool StepOption(int32 Direction);
		void TurnOptionsPage(int32 Direction);
		bool ActivateOption();
		bool BackOption();
		/** Square: the Controls page's key cleared, or Triangle: its row's two keys swapped; false where nothing changed. */
		bool KeyOption(bool bSwap);
		/** The keys pressed while the Controls page listens, until one ends it: true when one did. */
		bool HearKeys();
		/** The settings handed on when the Controls page made the bindings other than Before. */
		void ControlsChanged(const FControlBindings& Before);
		void DrawOptionsPage();
		/** Two lines of the port's words typed into strip Strip of the lists' strips, one to a plane: the labels' widths and textures. */
		void TypeStripPair(int32 Strip, std::string_view FirstText, std::string_view SecondText, FMenuLabel& First, FMenuLabel& Second);
		/** A row's keys as pictures at the row's height Y, each named in the frame's keys. */
		void DrawOptionsCells(const FSettingsRow& Shown, int32 Y);
		void DrawOptionsHelp();
		void CloseOptions();
		void AddOptionsScreen();
		/**
		 * The Fixes page's row of the second disc's story, after the page's own: in play, in a game that tells it extended,
		 * and once switched until the screen closes.
		 */
		bool ShowsSecondDiscRow() const;
		bool OnSecondDiscRow() const;
		FSettingsRow SecondDiscRow() const;
		/**
		 * Confirm on that row: the first asks, the second tells the rest of the second disc as the original does, the
		 * game going on where the original stands for its story (the way back's point) as a game loaded there does;
		 * false where it does nothing (switched already).
		 */
		bool ActivateSecondDiscRow();

		// The disc change and the messages it asks (MenuMainDisc.cpp).
		TMenuTask<> ChangeDisc();
		/**
		 * A message's two options asked: 1 for the left one, 0 for the right one (or Cancel: bAskCancelled).
		 * With a second text, the left option asks that one next and answers with it. A notice (bWaits off)
		 * waits for any button, or until it has been checked long enough.
		 */
		TMenuTask<uint8> Ask(uint8 FirstText, uint8 SecondText = NoSecondText, bool bWaits = true);
		static constexpr uint8 NoSecondText = 0xFF;
		TMenuTask<uint8> Choose(bool bWaits);
		/** Texts FirstText on, three lines, in the message window grown at the screen's foot. */
		TMenuTask<> ShowMessage(uint8 FirstText);
		/** The buzzer, and under a disc prompt's message (texts FirstText on) the port's line that the disc is not where the game looks. */
		void SayDiscNotFound(uint8 FirstText);
		/**
		 * Where the player's files come in through the device's file picker: the disc missing at a disc prompt is picked
		 * there and then, the copy's progress told under the message (texts FirstText on) until it ends; true once the
		 * disc is there.
		 */
		TMenuTask<bool> AddDiscThroughPicker(int32 Disc, uint8 FirstText);
		TMenuTask<> CloseMessage();
		void DrawDiscChange();
		/**
		 * Whether the game goes on to the second disc with its story mode still to choose: at the disc change or as a
		 * save is loaded, the story no further than the disc change, where the port has the expanded second disc.
		 */
		bool OffersStoryMode() const;
		/** The question how the second disc is told, its two answers under the pointer, until one is confirmed: the game's story mode. */
		TMenuTask<> OfferStoryMode();
		/** The question's lines, with what Choice means under its answers and the pointer on it. */
		void DrawStoryOffer(EStoryMode Choice);

		// The File screen (MenuMainFile.cpp): its sub-arc and its four commands, on two of the player's save
		// stores at a time standing for the original's two memory cards.
		/** From the title (Load) or elsewhere (Save where allowed): true while the menu goes on, or for the title a save loaded. */
		TMenuTask<bool> FileScreen(bool bFromTitle, bool bStaysAfterSave);
		/** The sub-arc's row of the mode, as the screen reveals it or as it draws it after. */
		int32 FileArcRow(bool bDrawn) const;
		/** Whether the sub-arc's command Load is: the title's, or the Normal Menu's where it loads in play. */
		bool FileCommandLoads(int32 FileCommand) const;
		void RenderFileLabels();
		TMenuTask<> OpenStores();
		/** A frame of the screen's loops (the first after a copy or a delete passed over), then the stores read again where they changed. */
		TMenuTask<> ScanStores(bool bShowsFrame);
		TMenuTask<bool> RunFileCommand(bool bStaysAfterSave);
		TMenuTask<bool> FileLoad();
		TMenuTask<bool> FileSave(bool bStaysAfterSave);
		TMenuTask<bool> FileCopy();
		TMenuTask<bool> FileDelete();
		/** A save's data loaded over the game, as the original's Load does once the checksum holds: false where it cannot be. */
		bool LoadStoreSave(const std::vector<uint8>& Data);
		void CloseFileScreen();
		/** The other pair of stores, Direction pairs on, read at the screen's next frame. */
		void TurnStorePair(int32 Direction);
		/** A store's slot by the card it stands in and its slot there. */
		int32 StoreSlot(int32 Card, int32 Slot) const;

		// The stores read as the cards are (MenuMainFileStores.cpp): their lists, files, icons and cells.
		void InitFileStores();
		/** Where the frame's end reads the cards' lists asked for again. */
		void UpdateStoreLists();
		void ListStore(int32 Card);
		void ReadStoreFile(int32 Card, int32 File);
		void LoadFileIcon(int32 File);
		void MarkGameSaves(int32 Card);
		/** The 200h bytes a card's file would start with: the header the game writes, then the data's start. */
		std::array<uint8, 0x200> SaveFileHead(const std::vector<uint8>& Data, int32 Slot) const;

		// The cursor on the cards' cells (MenuMainFileCursor.cpp), by mode: 0 any file, 1 a save, 2 any cell.
		void ResetCellCursor();
		bool IsCellSelectable(int32 Mode);
		int32 FirstSelectableCell(int32 Mode);
		bool CellSelectable(int32 Mode, int32 Place) const;
		bool CardShown(int32 Card) const;
		void CursorRight(int32 Mode, int32 From);
		void CursorLeft(int32 Mode, int32 From);
		void CursorDown(int32 Mode, int32 From);
		void CursorUp(int32 Mode, int32 From);
		/** The frame's code on the cells: 1 a cell chosen, 2 cancelled, 0 otherwise (the details follow the cursor). */
		int32 UpdateCellCursor(int32 Mode);
		/** A new save's slot on a card: the one it replaces, or the first free. */
		int32 PickSaveSlot(int32 Card, int32 Replaced) const;
		/** The cell the cursor stands on. */
		int32 CursorCell() const;

		// The cards and the save's details as drawn (MenuMainFileDraw.cpp, MenuMainFileDetails.cpp).
		void InitCardCells();
		void DrawCards();
		void DrawCellHighlight();
		void DrawCellIcons(int32 FirstFile, int32 FirstCell, int32 EndFile);
		void DrawCellOutlines();
		void DrawCardLabels();
		void ShowFileDetails(uint8 File);
		void BuildSaveDetails(int32 File);
		void BuildMemberLabels();
		void RenderMemberName(int32 Slot, uint8 Character, int32 File);
		void BuildTimeAndNumber(std::span<const uint8> Data);
		void InitDetailsBand();
		void RenderFileTitle(int32 File);
		/** The French translation's: the save's chapter title in its own words, rendered with the message font. */
		void RenderFrenchFileTitle(int32 File);
		void ClearFileTitle();
		void DrawFileDetails();

		// The frame.
		uint32 IdleFrames = 0;
		std::array<uint32, 7> PlayTimeDigits{};
		// The party.
		std::array<bool, 16> Available{};
		int32 Members = 0;
		int32 MembersWithGears = 0;
		int32 FirstMember = 0;
		std::array<int8, 3> GameSlots{0, 1, 2};

		// The target pointers.
		std::array<std::vector<FMenuPrimitive>, 4> TargetPointers;
		std::array<bool, 4> TargetPointerShown{};

		// The party's panels, gold and time.
		std::array<FMenuSlide, 3> Slides{};
		std::array<FMenuPartyPanel, 3> Panels{};
		std::vector<FMenuPrimitive> GoldDigits;
		std::vector<FMenuPrimitive> GoldMark;
		std::array<std::vector<FMenuPrimitive>, 7> TimeDigits{};
		std::array<std::vector<FMenuPrimitive>, 2> TimeColons{};

		// The Status screen.
		FMenuStatusSheet StatusSheet;
		FMenuStatBars StatBars;
		std::array<FMenuLabel, 5> EquipNames{};
		std::array<bool, 5> EquipNameShown{};
		std::vector<FMenuPrimitive> MemberHints;
		/** The stats last computed (a character's seven, or a gear's), which the sheet shows. */
		std::array<uint16, 9> ShownStats{};
		/** The party slot the screen shows. */
		int32 SheetSlot = 0;
		FMenuDeathblows Deathblows;
		FMenuEquipScreen EquipList;
		std::array<FMenuLabel, 6> EquipSlotLabels{};
		std::array<FMenuLabel, 8> HelpTags{};
		std::array<FMenuLayerSprite, 2> Cursors{};
		FMenuLayerSprite ScrollKnob;
		FMenuItemsScreen ItemList;
		FMenuAbilitiesScreen AbilityList;
		/** The Gear screen's labels: its inner arc's four (placed, never drawn) and its equipment's headings (on the layer). */
		std::array<FMenuLabel, 6> GearLabels{};
		std::array<bool, 6> GearLabelOnLayer{};
		/** The gears' part tables as this menu reads them: the pilots can change while it runs. */
		FGearPartTables GearParts;
		std::array<FMenuTargetPanel, 3> TargetPanels{};
		bool bTargetPanelsMade = false;
		std::array<FMenuLabel, 2> SkillsLabels{};

		// The text labels, and what renders them.
		std::array<FMenuLabel, 4> HelpLabels{};
		std::array<FMenuLabel, 8> ArcLabels{};
		std::array<FMenuLabel, 4> SubArcLabels{};
		/** The status sheet's text labels: its inner arc's three, then its equipment's headings (on the layer). */
		std::array<FMenuLabel, 6> SheetLabels{};
		/** A message's three lines, and whether its last answer was Cancel. */
		std::array<FMenuLabel, 3> MessageLines{};
		bool bMessageShown = false;
		bool bAskCancelled = false;
		/** The questions asked by a command of the arc have the cards' lists read while they wait. */
		bool bCardWatchInAsk = false;
		/** The second disc's question: its line, its two answers, and three lines on the answer under the pointer. */
		std::array<FMenuLabel, 6> StoryOfferLines{};
		bool bStoryOfferShown = false;

		// The arcs shown (the title's by place), and the arcs' places with the Normal Menu's eighth after the module's own.
		std::vector<FMenuArcCommand> TitleArc;
		/** The inner arcs' rows: the module's, then the port's for the Normal Menu's File where saving is allowed (Delete, Copy, Save, Load). */
		std::array<std::array<FMenuArcCommand, FMenuMainTables::SubArcCommands>, FMenuMainTables::SubArcRows + 1> SubRows{};
		std::vector<FMenuArcCommand> NormalArc;
		std::array<int32, FMenuMainTables::ArcPlaces + 1> PlaceX{};
		std::array<int32, FMenuMainTables::ArcPlaces + 1> PlaceY{};

		// The options screen: what it shows, and where the player is in it.
		FMenuOptionsScreen Options;
		struct FMenuOptionsState
		{
			FPlayerSettings Settings;
			/** What the display offered and the voices and subtitles the game had as the screen opened. */
			FSettingsContext Context;
			/** The page L1 and R1 have turned to: the settings' by number, then the saves'. */
			int32 Turned = 0;
			/** A list opened from a page's row (the fixes' own, the CRT's parts), the row, and Custom picked while the fixes match a preset. */
			std::optional<ESettingsPage> Opened;
			int32 OpenedFrom = 0;
			bool bCustomPicked = false;
			int32 Row = 0;
			int32 Top = 0;
			std::unique_ptr<FMenuSavePages> SavePages;
			std::unique_ptr<FMenuControlsPage> ControlsPage;
			/** The Modding page's rows for the device's mods, where its files go through its picker. */
			std::unique_ptr<FMenuModFiles> ModFiles;
			/** Where the device's rows started as the Modding page's rows were last filled. */
			int32 ModFilesShownAt = 0;
			uint64 AssetRevision = 0;
			/** The Assets page is open over the Modding page, from the row OpenedFrom. */
			bool bAssetsOpened = false;
			/** The page listened for a key: the buttons do nothing until every one is let go. */
			bool bAwaitRelease = false;
			/** The second disc's row asked whether to switch, and switched. */
			bool bSecondDiscAsked = false;
			bool bSecondDiscSwitched = false;
			std::string Title;
			std::vector<FSettingsRow> Rows;
		} OptionsState;
		FVramImage OptionsImage;
		FMesWindow OptionsTyping{0x3D, 1};

		// The File screen.
		FMenuFileScreen Files;
		std::array<FMenuLabel, 6> FileLabels{};
		/** The File screen stays after a save, for another (the disc change's), and its saves are disc 2's. */
		bool bFileStaysAfterSave = false;
		bool bSaveAsSecondDisc = false;
		/** A save was loaded over the game. */
		bool bGameLoaded = false;
		/** The save's place name, typed into the image the details show it from. */
		FVramImage TitleImage;
		FMesWindow FrenchTitleTyping{0x3C, 1};
		FVramImage DetailNameImage;
		/** A list's description, typed alone into a wider strip. */
		FVramImage HelpImage;
		FMesWindow HelpTyping{0x39, 1};
	};
}
