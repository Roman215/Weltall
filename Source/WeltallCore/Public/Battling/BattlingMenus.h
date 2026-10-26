#pragma once

#include "Battling/BattlingData.h"
#include "Battling/BattlingLabels.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	/** The program's menus, by their numbers in its table of eight. */
	enum class EBattlingMenu : int8
	{
		None = -1,
		Pause = 0,
		GiveUp = 2,
		/** The arena's: its top menu, bonus battling, and each side's gear choice. */
		Top = 3,
		Bonus = 4,
		PlayerOneGear = 5,
		PlayerTwoGear = 6,
		PracticePause = 7,
	};

	/** What choosing an item does. */
	enum class EBattlingMenuAction : uint8
	{
		/** A gap's: none. */
		None,
		/** Opens the menu the item's argument names. */
		Open,
		/** Back to the bout. */
		Resume,
		/** Ends the set: the winner screen when it has no limit and a round was played, else leaves. */
		GiveUp,
		StepPracticeStyle,
		StepFrameRate,
		/** The gear choice for the kind of bout the item's argument names. */
		ChooseGears,
		/** The tutorial, Weltall against Vierge. */
		StartTutorial,
		/** Back to the field. */
		Exit,
		StepMatches,
		StepLevel,
		StepRubberBand,
		/** A side's gear item: its cursor over the roster, confirm choosing the gear and cancel taking it back. */
		ChoosePlayerOneGear,
		ChoosePlayerTwoGear,
		StepVibrationOne,
		StepVibrationTwo,
	};

	namespace BattlingMenuItemFlags
	{
		/** The action runs every pass the cursor is on the item, not on confirm. */
		constexpr uint8 EveryPass = 1;
		/** A value printed beside its label: its label stands where the menu's widest one does. */
		constexpr uint8 Value = 2;
		/** A gap the cursor skips. */
		constexpr uint8 Separator = 4;
	}

	struct FBattlingMenuItem
	{
		uint8 Flags = 0;
		/** The help line's message in the program's message table (48/3). */
		uint8 Help = 0;
		EBattlingMenuAction Action = EBattlingMenuAction::Open;
		/** The menu an Open item opens, or the kind of bout a gear choice is for. */
		int8 Argument = -1;
	};

	struct FBattlingMenuDefinition
	{
		std::span<const FBattlingMenuItem> Items;
		/** Where cancel goes as the program starts (the gear choice changes its own); a menu going to itself buzzes instead. */
		EBattlingMenu Back = EBattlingMenu::None;
		/** 0: the items centred on the screen; else printed from a left edge, the box this many pixels wider. */
		uint8 Layout = 0;
	};

	/** A menu as the program's table defines it. */
	WELTALLCORE_API const FBattlingMenuDefinition& BattlingMenuDefinition(EBattlingMenu Menu);

	/** Where a menu stands, as the program places it from its labels' widths at their own size. */
	struct FBattlingMenuPlace
	{
		/** A left-aligned menu's left edge, and the items' first line (a line every 14h pixels). */
		int16 X = 0;
		int16 Y = 0;
		/** Half each item's width, as a centred item is printed from the screen's middle less it. */
		std::array<int16, 8> HalfWidths{};
		/** The box the menu darkens behind its items. */
		int16 BoxX = 0;
		int16 BoxY = 0;
		int16 BoxWidth = 0;
		int16 BoxHeight = 0;
	};

	WELTALLCORE_API FBattlingMenuPlace BattlingMenuPlace(EBattlingMenu Menu, const FBattlingLabels& Labels);

	/** The gear choice: each side's place in the roster, what is chosen, the carousels. */
	struct FBattlingGearChoice
	{
		/** Each side's place in the roster, kept from set to set. */
		std::array<int32, 2> Cursors{0, 1};
		/** Bit 0 player 1 has chosen, bit 1 player 2. */
		uint32 Chosen = 0;
		/** The place each carousel showed the pass before, and its slides: the near neighbour's (2 a pass) and its own (4). */
		std::array<int32, 2> Shown{};
		std::array<int32, 2> NearSlides{};
		std::array<int32, 2> FarSlides{};
		/** Each side's name pulses while it chooses. */
		std::array<bool, 2> NameFlashes{};
		/** Player 2's input runs (the human against the computer, once player 1 has chosen; two players). */
		bool bSecondChoosing = false;
		/** The portraits uploaded for the gear choice, which happens once a start of the menus; the program uploads them while pending. */
		bool bPortraitsUploaded = false;
		bool bPortraitsUploadPending = false;

		/** A portrait the carousels showed this pass, in the order linked: its gear, its side, how far it slid and how bright (40h at rest). */
		struct FShown
		{
			uint8 Gear = 0;
			uint8 Side = 0;
			int16 Offset = 0;
			int16 Brightness = 0;
		};
		std::array<FShown, 8> Portraits{};
		int32 PortraitCount = 0;
	};

	/** The menus' state: the open menu, each menu's cursor, the help lines, the paused frame behind them, the arena's menus. */
	struct FBattlingMenus
	{
		EBattlingMenu Current = EBattlingMenu::None;
		/** The menu open before the last one opened. */
		EBattlingMenu Previous = EBattlingMenu::None;
		std::array<int16, 8> Cursors{};
		/** The message each help line shows (0 none); line 0 takes the item the cursor was on a pass before. */
		std::array<uint8, 2> HelpShown{};
		uint8 HelpNext = 0;
		/**
		 * The message the pass typed into each help line's image (0 none): a line is typed as its message changes, even when
		 * the same pass then clears the lines (a resume).
		 */
		std::array<uint8, 2> HelpTyped{};
		/** A stick pushed off its centre has buzzed once. */
		bool bStickBuzzed = false;
		/** The paused frame is drawn behind the menus, blurred once and once more for each L2 press (up to five). */
		bool bCaptureShown = false;
		bool bCaptureRestorePending = false;
		uint8 BlurPresses = 0;
		/** The paused frame is drawn once more, on the pass that resumes the bout. */
		bool bCaptureAfterResume = false;

		/** Where each menu's cancel goes: the program's table, which the gear choice rewrites for its two. */
		std::array<EBattlingMenu, 8> Backs{EBattlingMenu::Pause, EBattlingMenu::Pause, EBattlingMenu::Pause, EBattlingMenu::Top, EBattlingMenu::Top,
			EBattlingMenu::Top, EBattlingMenu::Top, EBattlingMenu::PracticePause};
		/** The gear items' help messages, which COM VS COM changes, and whether each side's VIBRATION item is a gap. */
		std::array<uint8, 2> GearHelps{37, 38};
		std::array<bool, 2> VibrationHidden{};
		/** Each pad's held buttons as the last merge left them, which the menus' passes read as they begin. */
		std::array<uint16, 2> LastHeld{};
		/** The pad the last input read (0 or 1) and its words then: held, pressed and on the repeat beat. */
		uint8 InputPad = 0;
		uint16 InputHeld = 0;
		uint16 InputPressed = 0;
		uint16 InputRepeated = 0;
		/** The gears the arena's menus offer, by their numbers, in the roster's order. */
		std::vector<uint8> Roster;
		/** Argento has joined the roster since the program started. */
		bool bArgentoListed = false;
		FBattlingGearChoice Gears;
		/** The menus' passes: the idle countdown to the demonstration, and how long pad 1 has held L2 on the top menu. */
		int32 IdleCountdown = 0;
		int32 L2HeldPasses = 0;
		/** The bout the menus chose, whose gears' files are read next, and the gear each side asked for (-1 none). */
		bool bBoutChosen = false;
		/** The idle countdown ran out: the demonstration starts, its gears read next. */
		bool bDemonstrationChosen = false;
		std::array<int32, 2> GearsRead{-1, -1};
		/** EXIT was chosen: the program leaves for the field. */
		bool bExited = false;

		/**
		 * Argento joins the roster's end, once a program run, even where its threshold listed it already
		 * (a story's progress of 999).
		 */
		void ListArgento()
		{
			if (!bArgentoListed)
			{
				Roster.push_back(BattlingGears::Argento);
				bArgentoListed = true;
			}
		}

		int16& Cursor(EBattlingMenu Menu) { return Cursors[static_cast<std::size_t>(Menu)]; }
		int16 Cursor(EBattlingMenu Menu) const { return Cursors[static_cast<std::size_t>(Menu)]; }
		EBattlingMenu& Back(EBattlingMenu Menu) { return Backs[static_cast<std::size_t>(Menu)]; }

		/** An item's flags as the program's table holds them now (a hidden VIBRATION item is a gap) and its help message. */
		WELTALLCORE_API uint8 ItemFlags(EBattlingMenu Menu, std::size_t Item) const;
		WELTALLCORE_API uint8 ItemHelp(EBattlingMenu Menu, std::size_t Item) const;

		/** Both help lines cleared, as a round's start, the result screen and the winner screen's end clear them too. */
		void InvalidateHelp()
		{
			HelpShown = {};
			HelpNext = 0;
		}
	};

	/**
	 * The roster the arena's menus offer: every gear whose threshold in the gear table is at or below the story's
	 * progress, in their order. Argento joins it once unlocked (FBattlingMenus::ListArgento).
	 */
	WELTALLCORE_API std::vector<uint8> BattlingRoster(const FBattlingGearTable& GearTable, int32 StoryProgress);
}
