#pragma once

#include "Foundation/Types.h"

#include <array>

namespace Weltall
{
	/** The events the input step gives the party's menus, one per frame. */
	namespace BattleMenuEvents
	{
		inline constexpr uint8 Right = 0;
		inline constexpr uint8 Down = 1;
		inline constexpr uint8 Left = 2;
		inline constexpr uint8 Up = 3;
		/** Confirm, and the Fierce attack (logical Circle: the US release's Cross). */
		inline constexpr uint8 Confirm = 4;
		/** Cancel (logical Cross: the US release's Circle). */
		inline constexpr uint8 Cancel = 5;
		/** The Weak attack (logical Square: the US release's Triangle). */
		inline constexpr uint8 Weak = 6;
		/** The Strong attack (logical Triangle: the US release's Square). */
		inline constexpr uint8 Strong = 7;
		inline constexpr uint8 Nothing = 8;
		inline constexpr uint8 L2 = 0x0C;
		inline constexpr uint8 Select = 0x0D;
		/** Read while the battle has ended or the queued actions have played. */
		inline constexpr uint8 Over = 0xFF;
	}

	/** The HUD's drawing of the party's menus, which the turn's code asks for where the game calls the HUD's routines. */
	enum class EBattleMenuCue : uint8
	{
		/** The command HUD's block made as an on-foot turn starts; closed as the turn ends. */
		MakeCommandHud,
		CloseCommandHud,
		/** The AP bar's ticks for the turn's AP, each frame of an on-foot turn's menu; gone once the member's input is over. */
		ApTicks,
		HideApTicks,
		/** The command ring for the menu's state, or its fade once the first hit was made. */
		CommandIcons,
		/** The AP numbers built from the AP left, shown (Arg 1) or not, and taken away by the attack mode's cancel. */
		ApNumbers,
		ShowApNumbers,
		CancelApNumbers,
		/** The target cursor's arrows made, hidden, and pointing the ways of Arg's bits (right, down, left, up). */
		InitArrows,
		HideArrows,
		ArrowTargets,
		/** The buttons pressed and the Deathblows they lead to; hidden. */
		DeathblowHints,
		HideHints,
		/** A list's window opened at its place (Arg: the list (EBattleMenuList) times 16 plus the window). */
		OpenListWindow,
		/** The item list's build (Arg 1 the first, which makes the lists' work), its rows shown once the build's frame has passed, a frame of its loop. */
		BuildItemList,
		ShowListCells,
		ItemListFrame,
		/** The Ether list's build for the slot, its rows shown once the build's frame has passed, a frame of its loop. */
		BuildEtherList,
		ShowEtherCells,
		EtherListFrame,
		/** A list's windows and sprites hidden, or shown again after the target selector (Arg the list). */
		HideListWindows,
		ShowListWindows,
		/** The arrows drawn again as they were, for the target selector over a list. */
		ShowArrows,
		/** The lists' work freed after its frame; a list's window closed (Arg the window). */
		FreeListWork,
		CloseListWindow,
		/** The gear Special list's build for the slot (its rows shown at once), a frame of its loop. */
		BuildSpecialList,
		SpecialListFrame,
		/** The Combo list's build for the slot once its windows have grown, a frame of its loop, its sprites hidden as it closes. */
		BuildComboList,
		ComboListFrame,
		HideComboList,
		/** Billy's ammunition window built for the slot and opened; hidden as the target is locked. */
		BuildAmmunition,
		HideAmmunition,
		/** A gear's cockpit as its turn starts: the command HUD's block with the buttons' costs and the instruments, a frame; its fuel, a frame; then shown. */
		OpenCockpit,
		CockpitFuel,
		ShowCockpit,
		/** The cockpit's instruments gone as an action's results are taken. */
		HideCockpit,
		/** A gear's buttons pressed and the attacks the press of Arg leads to. */
		GearHints,
		/** The acting party member's flash over its panel as its turn starts (Arg 1), gone as the turn ends (Arg 0). */
		ActorFlash,
		/** The member has boarded its gear: its panel takes a gear's labels. */
		BoardedPanel,
		/** A battle event's message is set up: the kernel's window its text types in is made for it. */
		EventText,
		/** The "more" arrow where a battle event's text has stopped (Arg 1), gone as the player goes on (Arg 0). */
		EventArrow,
		/** The result begins: the battle's gauges go, the HUD draws the result's set from now on. */
		ResultBegin,
		/** The result's files are read: its sprite sheet in place of the HUD's, its images, the party's portraits, the labels typed anew. */
		ResultData,
		/** The result screen's work is made, empty. */
		ResultScreen,
		/** The panels' parts, one a frame: the portraits, the labels, HP and EP, the levels (Arg 1: the new ones), the EXP counters, the EXP still to add. */
		ResultPortraits,
		ResultPanelLabels,
		ResultHpEp,
		ResultLevels,
		ResultExpCounters,
		ResultExpGains,
		/** One of the screen's windows (the slot: 0 the items', 1 the level-ups', 2 the gold's) made at its size and shown; shown or hidden (Arg). */
		ResultOpenWindow,
		ResultWindow,
		/** A member's level-up in the window: its portrait and labels, max HP and EP, the stats' bars and figures. */
		ResultLevelUp,
		/** A member's portrait for its learned skills, and a skill's message (Arg the skill, 80h set for an ability). */
		ResultLearnedPortrait,
		ResultLearned,
		/** The items and gold: the gold's labels and figures, the headings, the drops listed. */
		ResultSpoils,
	};

	/** The lists the party's menus open, each with its windows. */
	enum class EBattleMenuList : uint8
	{
		Items,
		Ether,
		Special,
		Combo,
		Ammunition,
	};

	/** Where the target selector stands: its target, the candidates' mask and their list (the attack cursor's too). */
	struct FBattleTargeting
	{
		static constexpr uint8 None = 0xFF;

		uint8 Target = None;
		uint16 Candidates = 0;
		/** The candidates in order, FFh after them; the direction search reads the first eleven. */
		std::array<uint8, 12> List{};
		uint8 Count = 0;
	};

	/** The routines of a party turn, which run across frames as the game's turn code does between its frames. */
	enum class EBattlePartyRoutine : uint8
	{
		None,
		Turn,
		AutoTurn,
		AttackMode,
		GearAttackMode,
		ItemMenu,
		EtherMenu,
		SpecialMenu,
		ComboMenu,
		SelectTarget,
		PerformAbility,
		PerformItem,
		/** Billy's ammunition window opened, or closed with its work. */
		OpenAmmunition,
		CloseAmmunition,
		/** The attack mode entered from the command ring: the playback begun, then the target cursor. */
		StartAttack,
		/** A member boards its gear from the command ring. */
		Board,
		/** Frames that pass without the event being read (windows opening and closing). */
		Frames,
	};

	/** A routine under way: where it resumes and, for Frames, how many are left. */
	struct FBattlePartyFrame
	{
		EBattlePartyRoutine Routine = EBattlePartyRoutine::None;
		uint8 Pc = 0;
		uint8 Count = 0;
	};

	/** The Ether, Item and gear Special lists: the cursor among the cells shown and the top row shown. */
	struct FBattleListMenu
	{
		uint8 Cell = 0;
		uint8 TopRow = 0;
		/** 2 while the list is open, 1 when something was chosen, 0 when it was left. */
		uint8 Result = 2;
		/** The item list: rebuilt when set (1 + the page), the page shown, and whether its windows are still to be made. */
		uint8 Build = 0;
		uint8 Page = 0;
		bool bFirstBuild = false;
	};

	/** The Combo command's menu: the learned Deathblows 0-6 offered, those queued and the stored AP left to spend. */
	struct FBattleComboMenu
	{
		uint8 Cursor = 0;
		uint8 Queued = 0;
		uint8 Pool = 0;
		std::array<uint8, 7> Offered{};
		std::array<uint8, 7> Costs{};
		std::array<uint8, 7> Paid{};
		/** 1 chose a target, 2 left the menu, 0 still open. */
		uint8 Result = 0;
	};

	/** The target selector under way: the command's targeting word and what the confirm keeps. */
	struct FBattleTargetSelect
	{
		uint16 Word = 0;
		/** The candidates to confirm: all of them, or the cursor's group. */
		uint16 Confirmed = 0;
		/** 0 still choosing, 1 cancelled (or no candidate), 2 confirmed. */
		uint8 Result = 0;
		uint8 Actor = 0;
		bool bReachable = false;
		/** The event the HUD's arrows were last pointed after (FEh before the first frame). */
		uint8 ArrowsEvent = 0xFE;
	};

	/** The automatic turn's choice: the kind of action (1 attack, 2 ability, 4 board, else guard) and its argument. */
	struct FBattleAutoTurn
	{
		uint8 Kind = 0;
		uint8 Argument = 0;
	};

	/**
	 * A party member's turn as the game's turn code keeps it across frames: the command menu and the
	 * attack modes (AP, the buttons pressed, the Deathblow and combo state, the target cursor), the lists
	 * and the target selector, the input's events and directions, the menu's own flags the turn reads, and
	 * the routines under way.
	 */
	struct FBattlePartyInput
	{
		static constexpr int32 CommandCount = 16;
		static constexpr int32 MaxButtons = 7;
		static constexpr int32 MaxRoutines = 4;

		/** The command menu's state: 1-10 the rings on foot, 5 and 100 the attack mode, 10h-18h a gear's, 19h and 65h its attack mode. */
		uint8 MenuState = 0;
		/** The state the menu last drew; a new one clears the blocked press (FFh draws it again). */
		uint8 ShownState = 0xFF;
		/** A press toward a disabled command was refused once: the same press again jumps past it. */
		bool bBlockedOnce = false;
		/** The command icons are up (the next hit leaves the menu fading, state 100 or 65h). */
		bool bIconsShown = false;
		/** The menu fades out after the first hit; its sounds stay off. */
		bool bMenuFading = false;
		/** The menu's sounds play. */
		bool bMenuSounds = false;

		uint8 ApLeft = 0;
		uint8 ApAtStart = 0;
		/** The AP (or the button) of the press being handled. */
		uint8 Cost = 0;
		/** The attack buttons pressed this turn in order (0 Fierce, 2 Weak, 3 Strong; a gear's 0-2), FFh after them. */
		std::array<uint8, MaxButtons> Buttons{};
		uint8 ButtonCount = 0;
		/** The Combo command's queued Deathblows and the menu cells they came from (FFh none). */
		std::array<uint8, MaxButtons> ComboQueue{};
		std::array<uint8, MaxButtons> ComboCells{};

		/** A command that ends the turn at once was carried out. */
		bool bTurnOver = false;
		/** The combo's end is queued; the attack mode does nothing more. */
		bool bComboClosed = false;
		/** An attack press was taken: the AP left is stored at the turn's end. */
		bool bStoresAp = false;
		/** The press makes a Deathblow (a count on foot; in a gear, the hit ends the combo). */
		uint8 Deathblow = 0;
		/** The hit just made ends the combo (a Deathblow, or the target's on-hit script asked). */
		bool bHitEndsCombo = false;
		/** Fei drawn as Id: the press that performs one of his whole combos (Nothing for none). */
		uint8 IdsPress = BattleMenuEvents::Nothing;
		/** The ability, item or gear Special chosen, and the targets an item was used on. */
		uint8 Choice = 0;
		uint16 UsedTargets = 0;
		/** A gear's hits this turn. */
		uint8 GearHits = 0;
		/** The attack mode's target cursor, and whether the first hit locked it. */
		uint8 Cursor = 0;
		bool bTargetLocked = false;
		/** The member did something other than guard or charge (the kernel's count of acting turns). */
		bool bActed = false;
		/** The first hit's walk into the target's group is done. */
		bool bApproached = false;
		/** The show's flags: the Combo command's Deathblows are playing; a gear press was taken; an automatic turn runs. */
		bool bStoredCombo = false;
		bool bGearPressed = false;
		bool bAutoTurn = false;

		/** The frame's event, and the last two directions pressed (FFh none). */
		uint8 Event = BattleMenuEvents::Nothing;
		/** The input's mode: 1 the battle's commands, 2 messages (the opening's and the event's), 0 the result screen. */
		uint8 InputMode = 0;
		/** The HUD's first window (the Combo menu's larger one, the event's messages) has grown to its size since it last opened growing. */
		bool bFirstWindowGrown = false;
		uint8 PreviousDirection = 0xFF;
		uint8 LastDirection = 0xFF;
		/** The Ether list's scroll (pixels) and its way (1 and 3 down, 2 and 4 up), stepped by the input every frame. */
		int32 ListScroll = 0;
		uint8 ListScrollWay = 0;
		/** The item list's row and cell of the item chosen, whose count its use takes one from. */
		uint8 ChosenRow = 0;
		uint8 ChosenCell = 0;
		/** Which list window is up (0 none, 1 Ether, 2 items, 3 Combo, 4 Special, 5 Billy's ammunition), and whether Billy's ammunition window stands. */
		uint8 ListWindow = 0;
		bool bAmmunitionShown = false;
		/** The turn-start icon of each party slot: 3 flying to the command window, 1 arrived (the show's), 0 none. */
		std::array<uint8, 3> TurnIcons{};
		/** Each party slot's commands disabled this turn, by the menu's command number (nonzero disabled). */
		std::array<std::array<uint16, CommandCount>, 3> Disabled{};

		/** The routines under way, innermost last. */
		std::array<FBattlePartyFrame, MaxRoutines> Routines{};
		uint8 Depth = 0;
		/** What the last routine to return gave its caller. */
		uint8 Returned = 0;
		/** The acting slot of the turn. */
		uint8 Actor = 0;
		FBattleListMenu List;
		FBattleComboMenu Combo;
		FBattleTargetSelect Select;
		FBattleAutoTurn Auto;
	};
}
