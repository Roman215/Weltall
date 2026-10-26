#pragma once

#include "Battle/Show/BattleShowFrame.h"
#include "Battle/Show/BattleShowHudMenus.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/TextureRegion.h"
#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"
#include "Menu/MenuSpriteSheet.h"
#include "Text/MesButtonCells.h"
#include "Text/MesWindow.h"

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;
	class FPsxExecutable;
	struct FBattleCommand;
	struct FBattleContext;
	struct FTim;

	/** A picture of a command ring's group: a sheet frame, or from 4000h the slot's icon byte it names; its place; its brightness word. */
	struct FBattleHudIconElement
	{
		uint16 Frame = 0;
		int16 X = 0;
		int16 Y = 0;
		/** From 2000h dim when command (Flags & Fh) is disabled, else at (Flags & F0h) / 2; below, at (Flags & FFh) / 2 (0 full and opaque). */
		uint16 Flags = 0;
	};

	/** A sprite of the Combo list: its sheet frame and its place. */
	struct FBattleHudComboSprite
	{
		uint8 Frame = 0;
		int16 X = 0;
		int16 Y = 0;
	};

	/** A label of the result screen: a sheet frame and its place. */
	struct FBattleHudResultLabel
	{
		uint8 Frame = 0;
		int16 X = 0;
		int16 Y = 0;
	};

	/** A label of the level-up window: a sheet frame (FFh none) and its place, and the tint it is drawn in (0 red, 1 dim) when tinted. */
	struct FBattleHudLevelLabel
	{
		static constexpr uint8 None = 0xFF;

		uint8 Frame = None;
		bool bTinted = false;
		uint8 Tint = 0;
		int16 X = 0;
		int16 Y = 0;
	};

	/**
	 * Where the Battle program's code (not its tables) puts the HUD's message boxes and lists: the game's places, or
	 * those of the French translation's program, which moves them for its longer words.
	 */
	struct FBattleHudPlaces
	{
		/** The message boxes' window at the foot, and the boxes' text in it. */
		int16 MessageWindowY = 0xC8;
		int16 MessageBoxY = 0xCA;
		/** The item list: its cursor's first row, its cells' two quads, the target label. */
		int16 ItemCursorY = 0x63;
		int16 ItemCellsX = 0x30;
		int16 ItemCellsBeyondX = 0x98;
		int16 ItemTargetsX = 0xD0;
		/** The Ether list: its cursor's first row, the cost, the help's kind and target labels. */
		int16 EtherCursorY = 0x38;
		int16 EtherCostX = 0x20;
		int16 EtherKindX = 0xDA;
		int16 EtherTargetsX = 0xFE;
		/** A gear's Specials: the cursor, the list's two quads, the cost, the target label and the help's lines. */
		int16 SpecialCursorX = 0x8C;
		int16 SpecialCursorY = 0x58;
		int16 SpecialListX = 0x94;
		int16 SpecialListRightX = 0xFC;
		int16 SpecialCostX = 0x7C;
		int16 SpecialTargetsX = 0x48;
		int16 SpecialHelpX = 0x88;
		/** The Combo list's stored AP: its tens' row and its units'. */
		int16 ComboTensY = 0x38;
		int16 ComboUnitsY = 0x38;
		/** The lists' windows: x, y, width, height. */
		std::array<std::array<int16, 4>, 4> EtherWindows{{{0x1C, 0xA0, 0xAC, 0x38}, {0x20, 0x2C, 0x118, 0x70}, {0xD0, 0xA4, 0x50, 0x18}, {0xE6, 0xC0, 0x4C, 0x18}}};
		std::array<std::array<int16, 4>, 3> SpecialWindows{{{0x74, 0xA0, 0xAC, 0x38}, {0x84, 0x4C, 0x9C, 0x50}, {0x1C, 0xA0, 0x50, 0x18}}};

		/**
		 * The French translation's: the Specials 12 pixels right, the costs' windows wider, the message boxes higher. Its
		 * program moves the stored AP's units and "/28" 4 down but leaves the tens: here they go down with them.
		 */
		WELTALLCORE_API static FBattleHudPlaces French();
	};

	/** Where the result program's code puts the level-up's numbers and bars and the learned label: the game's, or the French translation's. */
	struct FBattleResultPlaces
	{
		/** The old maximum HP and EP, a change's sign and its digits, the stat bars, a stat's old value. */
		int16 OldMaxHpX = 0xB8;
		int16 OldMaxEpX = 0xC0;
		int16 ChangeSignX = 0xD8;
		int16 ChangeDigitsX = 0xE0;
		int16 BarX = 0x78;
		/** Where a fallen stat's loss starts from: its bar's end less the loss, past this. */
		int16 FallBarX = 0x78;
		int16 StatValueX = 0xB8;
		/** The learned label, this far right of the name's width. */
		int16 LearnedLabelX = 0x5A;

		/**
		 * The French translation's: the numbers and bars 8 pixels right, the learned label 5 pixels left. Its program leaves
		 * a fallen stat's loss where the bars started (78h), 8 pixels short of its bar's end: here it moves with them.
		 */
		WELTALLCORE_API static FBattleResultPlaces French();
	};

	/** The Battle program's own tables the HUD reads, from the program as the game loads it. */
	struct FBattleHudTables
	{
		static constexpr int32 MenuStateCount = 26;
		static constexpr int32 IconGroupCount = 19;
		static constexpr int32 CharacterIconLists = 16;
		static constexpr int32 ComboSpriteCount = 16;

		/** The panels' offsets, by the party's count less one and the slot. */
		std::array<int16, 9> PanelOffsets{};
		/** Each party slot's digits' places: HP 0-2, the slash 3, the maximum 4-6, a gear's HP 7-11 (and more the HUD does not read). */
		std::array<std::array<int16, 24>, 3> DigitPlaces{};
		/** The AP ticks' heights by the AP the turn began with. */
		std::array<std::array<uint8, 6>, 8> ApTicks{};
		/** By menu state: the two layers the ring draws into (FFh ends) and the icon group each draws. */
		std::array<std::array<uint8, 2>, MenuStateCount> StateLayers{};
		std::array<std::array<uint8, 2>, MenuStateCount> StateGroups{};
		std::array<std::vector<FBattleHudIconElement>, IconGroupCount> IconGroups{};
		/** The command menus' pictures the set-up gives a slot: on foot by character id, a gear's, Chu-Chu's giant form's, the outer ring in a gear. */
		std::array<std::array<uint8, 8>, CharacterIconLists> CharacterIcons{};
		std::array<uint8, 4> GearIcons{};
		std::array<uint8, 4> ChuChuGearIcons{};
		std::array<uint8, 4> OuterGearIcons{};
		/** The gear cockpit's glass: two frames drawn added over the scene, two subtracted from it. */
		std::array<uint8, 4> CockpitGlass{};
		/** The Combo list's sprites: its eight cells' pictures (the eighth accepts the queue), then the stored AP's "/28" and the list's ornaments. */
		std::array<FBattleHudComboSprite, ComboSpriteCount> ComboSprites{};
		/** The result screen's labels: a party panel's (in the first slot's place), the level-up window's, the gold's. */
		std::array<FBattleHudResultLabel, 18> ResultPanelLabels{};
		std::array<FBattleHudLevelLabel, 27> ResultLevelLabels{};
		std::array<FBattleHudResultLabel, 7> ResultGoldLabels{};
		FBattleHudPlaces Places;

		/** From the program as it lies in memory (unpacked), loaded at its base address. */
		WELTALLCORE_API static TResult<FBattleHudTables> Parse(std::span<const uint8> Program);
	};

	/**
	 * What the HUD reads from the disc: the battle data file's HUD sheet, its images, the party's portraits and the battle's
	 * messages, the program's tables and the kernel's text CLUT.
	 */
	struct FBattleHudResources
	{
		/** The battle data file (directory 12 file 3): its sections 1 (the sheet), 2 (the images), 10h (the items' help), 24h (the portraits) and 26h (the messages). */
		static constexpr int32 DataDirectory = 12;
		static constexpr int32 DataFile = 3;
		/** The sheet's frames the three members' portraits are uploaded over, and the result sheet's. */
		static constexpr int32 PortraitFrame = 0x61;
		static constexpr int32 ResultPortraitFrame = 0xFC;

		FMenuSpriteSheet Sheet;
		/** The pictures Sheet's frames draw (MenuSheetTextureRegions), and those of the result screen's sheet. */
		std::vector<FTextureRegion> SheetRegions;
		std::vector<FTextureRegion> ResultSheetRegions;
		/** The HUD's images and CLUTs as the set-up uploads them. */
		std::vector<FVramImage> Images;
		/** A 24 x 24 8-bit TIM of 460h bytes per character, the last for the special party's members. */
		std::vector<uint8> Portraits;
		/** The battle's messages (section 26h, a message table): the actions' names. */
		std::vector<uint8> Messages;
		/** The battle items' help (section 10h, a message table), by item. */
		std::vector<uint8> ItemHelp;
		/** The lists' help (directory 35 files 1-3, message tables of two lines an entry): the Ether on foot by character, in a gear by gear, the gears' Specials. */
		static constexpr int32 HelpDirectory = 35;
		std::array<std::vector<uint8>, 3> HelpTexts{};
		/** The message font's two CLUTs (the text's planes), which the kernel uploads for the battle. */
		std::array<uint16, 32> TextClut{};
		FBattleHudTables Tables;
		/**
		 * The result's file (directory 18 file 2): its sections 2 (the result screen's sheet, which takes the HUD's
		 * place), 3 (its images) and 4 (the portraits again, for the sheet's own frames).
		 */
		static constexpr int32 ResultDirectory = 18;
		static constexpr int32 ResultFile = 2;
		FMenuSpriteSheet ResultSheet;
		std::vector<FVramImage> ResultImages;
		std::vector<uint8> ResultPortraits;
		/** The result program (directory 16 file 4) the disc carries decides these. */
		static constexpr int32 ResultProgramDirectory = 16;
		static constexpr int32 ResultProgramFile = 4;
		FBattleResultPlaces ResultPlaces;

		WELTALLCORE_API static TResult<FBattleHudResources> Parse(std::span<const uint8> Data, std::span<const uint8> Program,
			std::span<const uint16, 32> TextClut);
		/** The result's file read into the resources. */
		WELTALLCORE_API std::optional<FError> ParseResult(std::span<const uint8> File);
		WELTALLCORE_API static TResult<FBattleHudResources> Load(const FCdImage& Image, const FGameFileIndex& Index, const FPsxExecutable& Executable);
	};

	/** A picture of the battle HUD's: a region of a VRAM loaded as the battle loads it. */
	using FBattleHudPictureVisit = std::function<void(const FVram&, const FTextureRegion&)>;

	/**
	 * Every picture the battle's HUD sheet draws, or with bResult the result screen's (its images over the HUD's), each region
	 * (MenuSheetTextureRegions) over the images as the battle uploads them, so that their keys are play's: for the texture
	 * export. The members' portraits are visited once a character, in the first member's place (each place gives the same
	 * key); the other places' views are left out.
	 */
	WELTALLCORE_API std::optional<FError> ForEachBattleHudPicture(const FCdImage& Image, const FGameFileIndex& Index, bool bResult,
		const FBattleHudPictureVisit& Visit);

	/** What the kernel keeps for the battle's windows: their fill colour and blend (a new game's settings). */
	struct FBattleHudSettings
	{
		std::array<uint8, 3> WindowColour{};
		uint8 WindowBlend = 0;
	};

	/** Why the HUD stopped: its game would read or write memory the port does not model. */
	enum class EBattleHudFault : uint8
	{
		None,
		/** A sheet frame with more parts than the primitives kept for it. */
		PartsOutsidePrimitives,
		/** The turn-start icon moved in a frame without input steps: the game draws it at stale stack values. */
		IconMovedWithoutSteps,
		/** A digit's frame past the sheet, or a window or box past the HUD's. */
		IndexOutsideHud,
		/** A window's frame drawn before the window was ever opened, or after it was closed: the game writes it into a block it does not hold. */
		WindowNotMade,
		/** A name or message past its table: the game reads an offset from the bytes after the table's. */
		MessageOutsideTable,
		/** The attack's hints drawn while no command HUD is made: the game writes them into a block it freed. */
		CommandHudNotMade,
		/** A list's text drawn or linked while no list's work is made: the game uses a block it freed. */
		ListNotMade,
		/** A battle event's message drawn before its text's window was set up: the game draws from a block it does not hold. */
		EventTextNotMade,
		/** A battle event's portrait that is no image with a CLUT: the game uploads from what its stack held. */
		EventPortrait,
		/** A part of the result screen built or drawn while its work is not made: the game writes through pointers it has not set. */
		ResultNotMade,
		/** A level-up whose stats are all zero: the game divides the bars' lengths by zero. */
		ResultStatsZero,
		/** A drop of no kind the inventory lists: the game draws its name as wide as a register's stale value. */
		ResultDropKind,
	};

	/** One of the seven menu windows: its frame's pieces per buffer and its growth. */
	struct FBattleHudWindow
	{
		static constexpr int32 CornerParts = 8;

		/** The four corners' parts in the order drawn, two per buffer. */
		std::array<FBattleHudPair, CornerParts> Corners{};
		int32 CornerCount = 0;
		/** Each edge's two halves: top, bottom, left, right. */
		std::array<std::array<FBattleHudPair, 2>, 4> Edges{};
		FBattleHudPair Fill{};
		FBattleHudPair FillMode{};
		/** The buffer the frame was last built in, which it is drawn from. */
		uint8 Buffer = 0;
		/** Growing: the place and size it grows to, the size reached. */
		int16 X = 0;
		int16 Y = 0;
		uint16 Width = 0;
		uint16 Height = 0;
		uint16 GrownWidth = 0;
		uint16 GrownHeight = 0;
		/** The window it grows as (the record's own number). */
		uint8 Number = 0;
		/** Its blocks are the HUD's: made by opening it, freed by closing it. */
		bool bMade = false;
		bool bShown = false;
		bool bGrowing = false;
		bool bGrown = false;
	};

	/** One of the eight message boxes: two quads over its text's row, which the pair of boxes shares in VRAM (a plane each). */
	struct FBattleHudMessageBox
	{
		FBattleHudPair Quad{};
		/** Where its row lies in VRAM. */
		FVramRect Row{};
		/** The text's plane: the box's parity. */
		uint8 Plane = 0;
		bool bShown = false;
		/** The text's width in pixels. */
		uint8 Width = 0;
	};

	/** A party member's panel: what the HUD builds and keeps for each party slot. */
	struct FBattleHudPanel
	{
		static constexpr int32 RestingIconParts = 6;
		static constexpr int32 IconParts = 10;
		static constexpr int32 HpParts = 40;
		static constexpr int32 ApParts = 4;
		static constexpr int32 ApLabelParts = 2;
		static constexpr int32 FuelLabelParts = 4;

		/** 0 no member, 1 the stored AP shown, 2 in a gear: the fuel shown. */
		uint8 Mode = 0;
		/** The resting portrait icon beside the gauge, built at the set-up. */
		std::array<FBattleHudPair, RestingIconParts> RestingIcon{};
		int32 RestingIconCount = 0;
		FBattleHudPair Portrait{};
		/** The "AP" and fuel labels, built at the set-up. */
		std::array<FBattleHudPair, ApLabelParts> ApLabel{};
		std::array<FBattleHudPair, FuelLabelParts> FuelLabel{};
		int32 ApLabelCount = 0;
		int32 FuelLabelCount = 0;
		uint8 LabelBuffer = 0;
		/** The turn-start icon at rest over the panel. */
		std::array<FBattleHudPair, IconParts> Icon{};
		int32 IconCount = 0;
		uint8 IconBuffer = 0;
		/** HP (or the gear's) and max HP, rebuilt when the panel's refresh flag is set. */
		std::array<FBattleHudPair, HpParts> Hp{};
		int32 HpCount = 0;
		uint8 HpBuffer = 0;
		/** The stored AP or the fuel, four digits. */
		std::array<FBattleHudPair, ApParts> Ap{};
		int32 ApCount = 0;
		uint8 ApBuffer = 0;
		FBattleHudPair Gauge{};
		/** The stored-AP or fuel bar. */
		FBattleHudPair Bar{};
		bool bGaugeShown = false;
		/** The icon at rest, the labels and the bar are shown (while no turn-start icon flies from here). */
		bool bIconShown = false;
	};

	/** The turn-start icon's flight from a panel to the command window. */
	struct FBattleHudFlight
	{
		static constexpr int32 Parts = 10;

		std::array<FBattleHudPair, Parts> Icon{};
		int32 Count = 0;
		uint8 Buffer = 0;
		int32 StartX = 0;
		int32 TargetX = 0;
		int32 StartY = 0;
		int32 TargetY = 0;
		/** Per step, in 256ths: left along x, down along y; and how far it has come. */
		uint32 StepX = 0;
		uint32 StepY = 0;
		int32 MovedX = 0;
		int32 MovedY = 0;
		uint16 Scale = 0;
		uint16 Angle = 0;
		/** Steps of movement (6 an input step) and of growth (1 an input step) since the last frame. */
		uint8 Steps = 0;
		uint8 Growth = 0;
	};

	/** A sheet frame's texture as the kernel reports it. */
	struct FBattleHudTexture
	{
		int32 Depth = 0;
		int32 ClutX = 0;
		int32 ClutY = 0;
		int32 VramX = 0;
		int32 VramY = 0;
	};

	/**
	 * The battle's HUD: the party's panels, gauges and icons, the turn-start
	 * icon's flight, the actor's flash, the message boxes, the menu windows and a battle event's message, drawn by the input mode each frame
	 * into the ordering table's HUD entry as the game draws them, from primitives built when the game builds them and
	 * kept per draw buffer as it keeps them. It reads the battle's rules state and writes back the flags it owns
	 * there (the panels' refresh, the turn-start icon's arrival, window 0's growth).
	 */
	class FBattleHud
	{
	public:
		static constexpr int32 PartySlots = 3;
		static constexpr int32 WindowCount = 7;
		static constexpr int32 MessageBoxCount = 8;
		static constexpr int32 DigitLabels = 10;

		/** The HUD draws into Vram (and reads it where the game stores from it), and reports its uploads to Observer. */
		WELTALLCORE_API FBattleHud(const FBattleHudResources& Resources, FMesContext& Text, FVram& Vram, IVramUploads* Observer = nullptr);

		/**
		 * The set-up: the images, the text's CLUT and the portraits, the panels, the flash
		 * and the digits (the setup's last stage), then the gauges, as the battle's start runs them.
		 */
		WELTALLCORE_API void Begin(FBattleContext& Context, const FBattleHudSettings& Settings);
		/** The message boxes and their windows, after the frame the battle's start draws between. */
		WELTALLCORE_API void BeginMessageBoxes();

		/** A new frame: the draw buffer flips (at the frame's start, before the show draws). */
		void Flip() { Buffer ^= 1; }
		uint8 DrawBuffer() const { return Buffer; }

		/** The frame's HUD by the battle's input mode, into Frame (emptied first). */
		WELTALLCORE_API void Draw(FBattleHudFrame& Frame);

		/** The presentation half of the menu's input step, each step of the battle's input mode: the flight's and flash's counters, then the textures. */
		WELTALLCORE_API void StepMenuInput();
		/** The menu's textures: every other step, the gauges' CLUT rows a step along their stored copies. */
		WELTALLCORE_API void AnimateMenuTextures();

		/** The HUD hidden (the sprites' system command 6Ah) or shown. */
		void SetHidden(bool bHide) { bHidden = bHide; }

		/** The pictures the sheet it draws from holds (MenuSheetTextureRegions): the HUD's, then the result screen's. */
		std::span<const FTextureRegion> TextureRegions() const { return Sheet == &Resources.ResultSheet ? Resources.ResultSheetRegions : Resources.SheetRegions; }

		/** Window Index made (when not shown) and built at its size, or set growing from nothing. */
		WELTALLCORE_API void OpenWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bGrows);
		/** A window's close up to its frame: the window no longer drawn or growing. */
		WELTALLCORE_API void CloseWindow(int32 Index);
		/** Whether window Index is drawn, grows, or has grown to its size since it last opened growing. */
		bool WindowShown(int32 Index) const { return Windows[static_cast<std::size_t>(Index)].bShown; }
		bool WindowGrowing(int32 Index) const { return Windows[static_cast<std::size_t>(Index)].bGrowing; }
		bool WindowGrown(int32 Index) const { return Windows[static_cast<std::size_t>(Index)].bGrown; }
		/** Whether window Index's blocks are the HUD's: opened and not closed since. */
		bool WindowMade(int32 Index) const { return Windows[static_cast<std::size_t>(Index)].bMade; }

		/** An enemy's queued message drawn: Text typed into box Box's row (its plane of the pair's) and uploaded. */
		WELTALLCORE_API void TypeMessageBox(int32 Box, std::span<const uint8> Text);
		/** A message box shown or hidden: the box, and window 4 behind the boxes. */
		WELTALLCORE_API void ShowMessageBox(int32 Box);
		WELTALLCORE_API void HideMessageBox(int32 Box);
		/** The message boxes hidden, up to its frame: every box, and windows 4 and 5. */
		WELTALLCORE_API void HideMessageBoxes();
		/** The acting enemy's name box: its name typed into box 0, shown with window 5. */
		WELTALLCORE_API void ShowNameBox(std::span<const uint8> Name);

		/** The AP bar's ticks for the AP the turn began with, as many drawn as AP left (all but one when none was spent). */
		WELTALLCORE_API void ShowApTicks(uint8 ApAtStart, uint8 ApLeft);
		/** The ticks gone as the turn takes the member's input back. */
		void HideApTicks() { ApTickCount = 0; }
		/** "AP left / AP at the start" beside the AP bar; shown or not. */
		WELTALLCORE_API void DrawApNumbers(uint8 ApLeft, uint8 ApAtStart);
		void ShowApNumbers(bool bShow) { bApNumbersShown = bShow; }
		/** The attack mode left before its first hit: the AP numbers gone. */
		void HideApNumbers()
		{
			ApNumberCount = 0;
			bApNumbersShown = false;
		}

		/**
		 * The command ring of the menu's state for party slot Slot, built in this buffer; once
		 * the first hit has made the menu fade, the ring's pictures dimmed a step instead.
		 */
		WELTALLCORE_API void DrawCommandIcons(uint8 Slot);
		/** The target cursor's arrows made anew in this buffer, none pointing yet. */
		WELTALLCORE_API void InitDirectionArrows();
		void HideDirectionArrows() { Arrows.bShown = false; }
		/** The ways a target lies in from the cursor (bit n for way n: right, down, left, up), as the target cursor finds them. */
		WELTALLCORE_API void SetArrowTargets(uint8 Ways);
		/** The command HUD's block made as a party member's turn starts on foot: no hints in it yet. */
		WELTALLCORE_API void MakeCommandHud();
		/** The command HUD's close: the block freed, the hints no longer drawn. */
		WELTALLCORE_API void CloseCommandHud();
		/** The hints no longer drawn (a gear's attack mode left before its first hit). */
		void HideHints() { bHintsShown = false; }
		/** A combo's buttons drawn on foot: slot Slot's buttons pressed so far and the Deathblows they lead to. */
		WELTALLCORE_API void DrawDeathblowHints(uint8 Slot);
		/**
		 * A gear's combo buttons drawn: slot Slot's buttons pressed so far in its gear and the attacks the press
		 * of Button (0 Weak, 1 Strong, 2 Fierce; FFh as Infinity opens) leads to, each with its name and fuel cost.
		 */
		WELTALLCORE_API void DrawGearHints(uint8 Slot, uint8 Button);

		/**
		 * The command HUD's open up to its first frame, as a gear's turn starts: the three buttons' fuel costs typed
		 * into the slot's command pictures, the command HUD's block made and, unless the gear is Chu-Chu's own form, the
		 * cockpit's instruments built.
		 */
		WELTALLCORE_API void OpenCockpit(uint8 Slot);
		/** Slot Slot's gear's fuel and its most. */
		WELTALLCORE_API void DrawCockpitFuel(uint8 Slot);
		/** The cockpit shown once its two frames have passed. */
		void ShowCockpit()
		{
			bCockpitShown = true;
			bCockpitLevelShown = true;
		}
		/** The instruments and the reticle gone as an action's results are taken (the level and the fuel stay). */
		void HideCockpit() { bCockpitShown = false; }

		/** A list opening: window Window at the list's place, made at its size (the first also starts the list's loop anew). */
		WELTALLCORE_API void OpenListWindow(EBattleMenuList List, int32 Window);
		/**
		 * The item list: the first time, the lists' work, the labels and the scroll
		 * bar; then the listed items' names and counts typed into VRAM (the frame after it is the caller's).
		 */
		WELTALLCORE_API void BuildItemList(bool bFirst);
		/** The build's frame has passed: the list's rows are drawn. */
		WELTALLCORE_API void ShowListCells();
		/** A frame of the item list's loop: the item's info when the cursor moved, the rows when they scrolled, the cursor. */
		WELTALLCORE_API void DrawItemListFrame();
		/** The name, help and count of the item in the cell Cell of row Row down from the top. */
		WELTALLCORE_API void DrawItemInfo(uint8 Cell, uint8 Row);
		/** The scroll bar's thumb, Offset pixels down (two a row). */
		WELTALLCORE_API void DrawItemThumb(int32 Offset);
		/** The rows' quads scrolled Scroll pixels. */
		WELTALLCORE_API void SetItemCells(int32 Scroll);
		/**
		 * The lists' work, labels and scroll bar, slot Slot's learned abilities' names and costs
		 * typed into VRAM, and its EP drawn (the frame after it is the caller's).
		 */
		WELTALLCORE_API void BuildEtherList(uint8 Slot);
		/** The Ether build's frame has passed: its info quad takes the even plane's CLUT in both buffers, its rows are drawn. */
		WELTALLCORE_API void ShowEtherCells();
		/** A frame of the Ether list's loop for slot Slot: the ability's info when the cursor moved, the rows when they scrolled, the cursor. */
		WELTALLCORE_API void DrawEtherListFrame(uint8 Slot);
		/** The cost (Elly's success rate too), kind, targets and help of the ability in cell Cell of row Row. */
		WELTALLCORE_API void DrawEtherInfo(uint8 Slot, uint8 Cell, uint8 Row);
		/** The scroll bar's thumb, Scroll pixels down. */
		WELTALLCORE_API void DrawEtherThumb(int32 Scroll);
		/** The rows' quads scrolled Scroll pixels. */
		WELTALLCORE_API void SetEtherCells(int32 Scroll);
		/**
		 * The lists' work and labels, slot Slot's gear's Specials offered (learned and
		 * not switched off) with their fuel costs typed into VRAM, and the rows' quads, drawn at once.
		 */
		WELTALLCORE_API void BuildSpecialList(uint8 Slot);
		/** A frame of the Special list's loop for slot Slot: the Special's info when the cursor moved, the cursor. */
		WELTALLCORE_API void DrawSpecialListFrame(uint8 Slot);
		/** The kind, targets and help of the Special in cell Cell, or none when it is not offered. */
		WELTALLCORE_API void DrawSpecialInfo(uint8 Slot, uint8 Cell);
		/**
		 * The lists' work and labels, the Deathblows the Combo command offers slot Slot with
		 * their AP costs typed into VRAM, the cells' pictures and the stored AP's bar, the rows drawn at once.
		 */
		WELTALLCORE_API void BuildComboList(uint8 Slot);
		/** A frame of the Combo list's loop: the queue and the stored AP left when a Deathblow was queued or taken back, the cursor. */
		WELTALLCORE_API void DrawComboListFrame();
		/** The stored AP left as digits and as a bar, the queued Deathblows' pictures with arrows between. */
		WELTALLCORE_API void DrawComboQueue(uint8 Pool);
		/** The Combo list closing: its sprites and the arrows no longer drawn (its windows close after). */
		WELTALLCORE_API void HideComboList();
		/**
		 * The lists' work and labels, the names of the two guns' ammunition (on foot, or his
		 * gear's) and their counts typed into VRAM, the rows drawn, and window 0 opened behind them (its frame is the caller's).
		 */
		WELTALLCORE_API void BuildAmmunition(uint8 Slot);
		/** The ammunition window no longer drawn (the target locked), until its close. */
		void HideAmmunition() { Windows[0].bShown = false; }
		/** The list's cursor at (X, Y), its frame stepping every third draw. */
		WELTALLCORE_API void DrawListCursor(int16 X, int16 Y);
		/** A list's windows and sprites hidden (a choice made, or the list closing), or shown again after the target selector. */
		WELTALLCORE_API void HideListWindows(EBattleMenuList List);
		WELTALLCORE_API void ShowListWindows(EBattleMenuList List);
		/** The arrows drawn again as they were (the target selector's, over a list). */
		void ShowDirectionArrows() { Arrows.bShown = true; }
		/** The lists' work freed, after its frame. */
		void FreeListWork() { ListWork.reset(); }

		/** The actor's flash as a party member's turn builds it (over its panel's portrait, in the buffer last drawn), and off. */
		WELTALLCORE_API void StartActorFlash(uint8 Slot);
		void StopActorFlash() { bFlashOn = false; }

		/** A member boarded its gear: its panel shows the fuel (unless the gear is Chu-Chu's own form). */
		WELTALLCORE_API void BoardGear(uint8 Slot);

		/**
		 * The portrait of a battle event's message: its CLUT and pixels uploaded and its
		 * quad placed in this buffer inside the corner of the message's window at (X, Y), Width wide: at its left, or
		 * mirrored at its right. The intro's set draws it while the event shows it.
		 */
		WELTALLCORE_API void LoadEventPortrait(const FTim& Portrait, bool bAtRight, int16 X, int16 Y, int16 Width);
		/**
		 * The kernel's window as the event sets it up for a message: where the event's text
		 * types on the screen, and its text's image. The intro's set ticks the window and draws its lines.
		 */
		WELTALLCORE_API void BeginEventText();
		/** The "more" arrow where the event's text has stopped, in the list cursor's sprites; and gone as the player goes on. */
		WELTALLCORE_API void DrawEventArrow();
		void HideEventArrow() { ListCursor.bShown = false; }
		/** Whether the lists' cursor's sprites are shown: a list's cursor, or the event's arrow drawn in them. */
		bool IsListCursorShown() const { return ListCursor.bShown; }

		/** The result begins: the wait gauges go. */
		WELTALLCORE_API void BeginResult();
		/**
		 * The result's files: its images uploaded, its sheet in the HUD's place (every sprite and window corner is drawn
		 * from it from now on), the party's portraits over the sheet's own frames, the labels typed anew.
		 */
		WELTALLCORE_API void LoadResult();
		/** The result screen's work made, empty. */
		WELTALLCORE_API void MakeResultWork();
		/** The panels' parts, each built in this buffer for every party slot that holds a member. */
		WELTALLCORE_API void BuildResultPortraits();
		WELTALLCORE_API void BuildResultPanelLabels();
		WELTALLCORE_API void BuildResultHpEp();
		/** The levels the battle began at, or the levels now. */
		WELTALLCORE_API void BuildResultLevels(bool bNew);
		WELTALLCORE_API void BuildResultExpCounters();
		WELTALLCORE_API void BuildResultExpGains();
		/** One of the screen's windows made at its size and shown at once: 0 the items', 1 the level-ups', 2 the gold's. */
		WELTALLCORE_API void OpenResultWindow(int32 Window);
		/** A window drawn or not, as it was last built. */
		WELTALLCORE_API void ShowWindow(int32 Window, bool bShown);
		/** A member's level-up: its portrait and the window's labels, max HP and EP as they were with their changes, each stat's bars, value and change. */
		WELTALLCORE_API void BuildResultLevelUp(uint8 Slot);
		/** A member's portrait for its learned skills' messages, the image their names are typed into and the message's quads. */
		WELTALLCORE_API void BuildResultLearnedPortrait(uint8 Slot);
		/** A learned skill's message: the Deathblow's or ability's name typed and uploaded, its quad in its colour and the "learned." label after it. */
		WELTALLCORE_API void BuildResultLearned(uint8 Slot, uint8 Skill, bool bAbility);
		/** The items and gold: the gold's labels, the gold gained and the party's, the headings, and each drop's name and count. */
		WELTALLCORE_API void BuildResultSpoils();

		EBattleHudFault Fault() const { return FaultState; }

		/** What the tests compare with the game's: the flight's counters, the gauges' texture rows, the flash's pulse. */
		uint8 IconSteps() const { return Flight.Steps; }
		uint8 IconGrowth() const { return Flight.Growth; }
		uint8 TextureStep() const { return TextureRowStep; }
		uint32 TextureRow() const { return TextureRowOffset; }
		int32 FlashLevel() const { return FlashGlow; }
		bool FlashFalling() const { return bFlashFalling; }
		const FBattleHudPanel& Panel(int32 Slot) const { return Panels[static_cast<std::size_t>(Slot)]; }

	private:
		// The set-up.
		void Upload(const FVramImage& Image);
		/** The party's portraits (a 24 x 24 image a character) uploaded where the sheet's frames from FirstFrame take theirs, a slot each. */
		void LoadPortraits(std::span<const uint8> Portraits, int32 FirstFrame);
		void InitPanelPrims();
		void InitPanelSprites();
		void InitFlash();
		void RenderDigitLabels();
		void InitGauges();
		void InitWindowFill(FBattleHudWindow& Window);

		// Sheet drawing.
		FBattleHudTexture TextureOf(int32 Frame) const;
		int32 DrawSheet(int32 Frame, std::span<FBattleHudPair> Pairs, int32 First, int32 X, int32 Y, int32 Scale);
		int32 DrawSheetRotated(int32 Frame, std::span<FBattleHudPair> Pairs, int32 First, int32 X, int32 Y, int32 Scale, int32 Angle);

		// The frame's parts.
		void Link(const FMenuPrimitive& Primitive);
		void LinkPairs(std::span<const FBattleHudPair> Pairs, int32 Count, uint8 From);
		int16 PanelX(int32 Slot) const;
		int16 PanelOffset(int32 Slot) const;
		void GrowWindows();
		void UpdatePartyPanels();
		void DrawApOrFuel(int32 Slot);
		struct FPanelDigits;
		FPanelDigits FormatPartyHp(int32 Slot) const;
		void DrawPartyHp(int32 Slot, const FPanelDigits& Digits);
		void DrawPartyGearHp(int32 Slot, const FPanelDigits& Digits);
		void TintDigits(FBattleHudPanel& Panel, int32 From, int32 To, uint8 Colour);
		void BuildFuelBars();
		void BuildWaitGauges();
		void DrawTurnIcons();
		void StartTurnIcon(int32 Slot);
		void MoveTurnIcon(int32 Slot);
		void DrawFlash();
		void DrawPanels();
		void DrawMessageBoxes();
		void DrawWindows();
		void DrawWindowFrame(FBattleHudWindow& Window, int16 X, int16 Y, int16 Width, int16 Height);
		void DrawWindowCorners(FBattleHudWindow& Window, int16 X, int16 Y, int16 Width, uint16 Height);
		void DrawWindowEdges(FBattleHudWindow& Window, int16 X, int16 Y, uint16 Width, uint16 Height);
		void SetWindowPart(FMenuPrimitive& Part) const;
		static void SetDimSub(FMenuPrimitive& Part);
		bool InGearPanel(int32 Slot) const;

		// The party's menus (BattleShowHudMenus.cpp).
		void InitCommandIcons();
		void DrawIconElement(uint8 Slot, int32 Layer, const FBattleHudIconElement& Element, uint16 PromptRole);
		void FadeCommandIcons();
		void LinkCommandIcons();
		void DrawDirectionArrows();
		void LinkHints();
		bool Learned(uint8 Slot, uint8 Deathblow);
		int32 DrawHintName(uint8 Slot, int32 Hint, int32 Row, uint8 Deathblow, int32 Beyond, std::span<FVramImage> Images);

		// The lists (BattleShowHudLists.cpp).
		void OpenListWork();
		void RenderLabels();
		void DrawScrollBar(int16 X, int16 TopY, int16 BottomY, int16 FirstY, int32 Links);
		void SetItemNameQuads(uint8 Cell, uint8 Row);
		void RenderItemHelp(uint8 Item);
		void SetItemCountQuads(uint8 Item);
		void SetLabelQuad(FMenuPrimitive& Quad, int16 X, int16 Y, int32 Label);
		/** The Ether list's entry: its command (the ability on foot, the gear's ether command in a gear), and whether it is learned. */
		const FBattleCommand* EtherCommand(uint8 Slot, int32 Entry);
		bool EtherLearned(uint8 Slot, int32 Entry);
		void DrawEtherPoints(uint8 Slot);
		void SetEtherCostQuads(uint8 Slot, int32 Entry);
		void SetEtherLabelQuads(uint8 Slot, int32 Entry);
		void RenderEtherHelp(uint8 Slot, int32 Entry);
		/** A gear Special: offered when learned and not switched off by a disabled command; its command. */
		bool SpecialOffered(uint8 Slot, int32 Special);
		const FBattleCommand* SpecialCommand(uint8 Slot, int32 Special);
		/** A help text's two lines (entry and entry + 1 of help file File) typed into the wide row and uploaded; returns their widths. */
		std::array<int32, 2> RenderHelpLines(int32 File, int32 Entry);
		/** A name from the kernel's tables, or none (and a fault) past its table. */
		std::span<const uint8> TableString(int32 Table, int32 Entry);
		void DrawComboCells();

		// The gear cockpit (BattleShowHudCockpit.cpp).
		void DrawGearHud();
		/** A sheet frame drawn after a run's parts so far (or as the run's only ones), its parts counted into the run. */
		int32 DrawCockpitSprite(FBattleHudCockpit::ERun Run, int32 Frame, int32 X, int32 Y, bool bAppend);
		/** Digits of Value from the place First on, each drawn where its place lies or closed up to the left; returns how many. */
		int32 DrawCockpitDigits(FBattleHudCockpit::ERun Run, uint32 Value, std::size_t First, int16 X, int16 Y, int16 Step, bool bCloseUp);
		void BlendCockpitRun(FBattleHudCockpit::ERun Run, bool bSubtract);
		void LinkCockpitRun(FBattleHudCockpit::ERun Run);
		void DrawReticle();
		void LinkCockpit();
		int32 DrawGearHintName(uint8 Slot, int32 Hint, int32 Row, uint32 Attack, std::span<FVramImage> Images);
		bool GearAttackLearned(uint8 Slot, uint32 Attack);
		/** A game-state byte, or none (and a fault) past the state. */
		uint8 StateByte(std::size_t Offset);
		void LinkListSprites();
		void LinkListText();
		void LinkListQuad(int32 Quad, uint8 From);

		// A battle event's message (BattleShowHudEvent.cpp).
		void DrawEventMessage();

		// The result screen (BattleShowHudResult.cpp).
		void DrawResult();
		/** A text quad made in both buffers: its plane's CLUT and the page of its mode (0-3: the four pages the battle types text into). */
		void InitTextQuads(FBattleHudPair& Quads, uint8 Plane, int32 Mode);
		/** The places from First on of a number's nine digits drawn into a run: 8 pixels apart from X, or closed up from X; a blank place draws nothing. */
		void DrawResultDigits(std::span<FBattleHudPair> Parts, int32& Count, uint32 Value, std::size_t First, int32 X, int32 Y, bool bCloseUp);
		/** A run's parts in this buffer drawn in a tint instead of their texture's own colours. */
		void TintResultParts(std::span<FBattleHudPair> Parts, int32 First, int32 End, const std::array<uint8, 3>& Tint);
		/** A party slot's character's record in the game state, or none (and a fault) when the slot's id names no character. */
		std::span<const uint8> ResultRecord(std::size_t Slot);

		const FBattleHudResources& Resources;
		/** The sheet the sprites are drawn from: the HUD's, then the result screen's. */
		const FMenuSpriteSheet* Sheet = nullptr;
		FMesContext& Text;
		FVram& Vram;
		IVramUploads* Observer;
		FBattleHudSettings Settings;
		/** The battle the HUD shows, from Begin on. */
		FBattleContext* Battle = nullptr;
		FBattleHudFrame* Drawing = nullptr;
		EBattleHudFault FaultState = EBattleHudFault::None;

		uint8 Buffer = 0;
		bool bHidden = false;
		/** The panels' places: 0 for one member ... (the party's count less one). */
		uint8 Layout = 0;

		std::array<FBattleHudPanel, PartySlots> Panels{};
		/** The gauges' and the resting icons' shared buffers, and the portraits'. */
		uint8 RestingIconBuffer = 0;
		uint8 PortraitBuffer = 0;
		/** The AP bar beside the command window, and whether it is drawn this frame. */
		FBattleHudPair ApBar{};
		bool bApBarShown = false;
		/** Its ticks, and the AP numbers beside it. */
		std::array<FBattleHudPair, 6> ApTicks{};
		int32 ApTickCount = 0;
		uint8 ApTickBuffer = 0;
		std::array<FBattleHudPair, 8> ApNumbers{};
		int32 ApNumberCount = 0;
		uint8 ApNumberBuffer = 0;
		bool bApNumbersShown = false;
		/** Each party slot's command pictures (the set-up's, from 10h a gear's costs), as the ring's elements from 4000h name them. */
		std::array<std::array<uint8, 0x1C>, PartySlots> MenuIcons{};
		FBattleHudCommandRing Ring;
		FBattleHudArrows Arrows;
		FBattleHudHints Hints;
		bool bHintsShown = false;
		bool bCommandHudMade = false;
		/** The gear cockpit in the command HUD's block, its reticle, and what is drawn of it: the instruments, the level and the fuel. */
		FBattleHudCockpit Cockpit;
		FBattleCockpitReticle Reticle;
		bool bCockpitShown = false;
		bool bCockpitLevelShown = false;
		FMesWindow HintTyping{0x1B, 1};
		/** The lists' text work while a list is open, their sprites in the show work, the labels typed for them, and the loop's memory. */
		std::optional<FBattleHudListWork> ListWork;
		FBattleHudSpriteRun<60> ListThumb;
		FBattleHudSpriteRun<30> ScrollBar;
		FBattleHudSpriteRun<20> ListCursor;
		std::array<FBattleHudLabel, 22> Labels{};
		FBattleHudListLoop ListLoop;
		FMesWindow ListTyping{0x1B, 1};
		FBattleHudFlight Flight;
		/** The actor's flash over its panel, its draw mode, its level and way, the buffer it was built in. */
		FBattleHudPair Flash{};
		FBattleHudPair FlashMode{};
		int32 FlashGlow = 0;
		bool bFlashFalling = false;
		bool bFlashOn = false;
		uint8 FlashBuffer = 0;
		/** The draw modes around the gauges and windows: one with the gauges' texture window, one without. */
		FBattleHudPair GaugeMode{};
		FBattleHudPair PlainMode{};
		/** The texture windows the draw modes take (the game keeps six). */
		std::array<FVramRect, 6> TextureWindows{};
		/** The gauges' CLUT rows, four copies each in a row, uploaded a step along every other input step. */
		std::array<FVramRect, 4> TextureRows{};
		std::array<std::vector<uint16>, 4> StoredRows{};
		uint8 TextureRowStep = 0;
		uint32 TextureRowOffset = 0;
		/** The textures of the sheet's gauge (5Ch), window edge (4Bh, 50h, 4Dh, 4Eh) frames, and the gauges' CLUTs by state. */
		FBattleHudTexture GaugeTexture;
		std::array<FBattleHudTexture, 4> EdgeTextures{};
		uint16 NormalClut = 0;
		uint16 PlainClut = 0;
		uint16 SlowClut = 0;
		uint16 FastClut = 0;
		/** The text's CLUTs for its even and odd planes. */
		uint16 EvenTextClut = 0;
		uint16 OddTextClut = 0;

		std::array<FBattleHudWindow, WindowCount> Windows{};
		std::array<FBattleHudMessageBox, MessageBoxCount> MessageBoxes{};
		/** The boxes' text rows, one per pair of boxes. */
		std::array<FVramImage, MessageBoxCount / 2> BoxRows{};
		/** The digits 0-9 typed for the lists. */
		std::array<FVramImage, DigitLabels> DigitImages{};
		FMesWindow BoxTyping{0x39, 1};
		FMesWindow DigitTyping{2, 1};
		/** A battle event's message: its portrait's quad and the buffer it was placed in, where its text types, and the text's image. */
		FBattleHudPair EventPortrait{};
		uint8 EventPortraitBuffer = 0;
		int16 EventTextX = 0;
		int16 EventTextY = 0;
		FVramImage EventTextImage;
		/** Finds the buttons the event's message shows. */
		FMesButtonCells EventButtons;
		/** The result screen's work, from the screen's start. */
		std::optional<FBattleHudResult> Result;
		std::vector<FMenuPrimitive> Scratch;
	};
}
