#pragma once

#include "Battle/BattleCombatant.h"
#include "Battle/BattleCommand.h"
#include "Battle/BattleEndingState.h"
#include "Battle/BattleEventState.h"
#include "Battle/BattlePartyState.h"
#include "Battle/BattleResultState.h"
#include "Battle/BattleRuleOptions.h"
#include "Battle/BattleScene.h"
#include "Battle/BattleTables.h"
#include "Battle/BattleTurnState.h"
#include "Foundation/Types.h"
#include "Game/GameBalance.h"

#include <array>
#include <cstddef>
#include <span>

namespace Weltall
{
	class FBattleProgramTables;
	class FGameState;
	class FPsxRandom;
	struct FGteArctangents;

	/** Where a battle's slots stand and what fills them: slots 0-2 the party, 3-10 the enemies. */
	struct FBattleSlot
	{
		static constexpr uint8 Empty = 0x7F;

		uint8 Group = 0;
		/** The member's place within its group. */
		uint8 Place = 0;
		/** A party slot's character, an enemy slot's type in the scene; Empty for none. */
		uint8 Occupant = Empty;
		/** Enemies: 0x80 hidden (not targetable, not alive, no name, no reward) until shown; 1 gone. */
		uint8 Presence = 0;
		/** Nonzero: the slot fights in a gear. */
		uint8 InGear = 0;
		uint8 Byte5 = 0;
		/** Nonzero: the slot's default target stood left of it when it was last chosen, the way it faces at home. */
		uint8 FacesLeft = 0;
		FBattleStagePoint Position;
	};

	/** A battle slot's side of the command input: AP, and what the turn-start panel shows. */
	struct FBattleApRecord
	{
		/** AP left unused by earlier turns, 0 to 28. */
		uint8 StoredAp = 0;
		/** Nonzero in a gear: 1 from the start, 2 after boarding. */
		uint8 InGear = 0;
		uint8 ApPerTurn = 0;
		uint8 Level = 0;
		uint8 EtherLevel = 0;
	};

	/** A stage group's place counts for one side: how many stand there and which places they take. */
	struct FBattleOccupancy
	{
		uint8 Count = 0;
		uint8 Places = 0;
	};

	/**
	 * What an action works with: the actor, command and targets it was given, and what it leaves per
	 * slot (an amount and a result code). The rules point at the combatants they work on the way the
	 * game does, by the slot numbers and by the records last pointed at, which not every step moves
	 * together: a counterattack swaps the actor and the target but not the gear parts or the target's
	 * battle fields, and rolling a wait moves the actor's records without its slot.
	 */
	struct FBattleAction
	{
		static constexpr int32 EntryCount = 12;
		/** A record pointer the rules have not set yet. */
		static constexpr uint8 Unset = 0xFF;
		static constexpr uint16 UnsetCommand = 0xFFFF;

		std::array<uint32, EntryCount> Amounts{};
		/** BattleResults codes; NoResult for a slot the action did not reach. */
		std::array<uint8, EntryCount> Results{};
		uint16 Targets = 0;
		/** The living combatants when the action began. */
		uint16 LivingAtStart = 0;
		/** The show's number for the action: the command's, or the command's own animation. */
		uint16 Animation = 0;
		/** Party slots revived (bits 0-2); bit 15 an item's use on a gear, which has no effect. */
		uint16 Revived = 0;
		/** The command's kind, 0x21 byte, element (the actor's own when the command has none) and 0x23 byte, and its number. */
		std::array<uint8, 4> Info{};
		uint8 InfoCommand = 0;
		/** The actor and the command number the action was given. */
		uint8 Actor = 0;
		uint8 Command = 0;
		/** A battle message to show (1-based), 0 for none. */
		uint8 Message = 0;

		uint8 ActorSlot = 0;
		uint8 TargetSlot = 0;
		/** The combatants whose records, gear parts and battle fields the rules read as the actor's and the target's. */
		uint8 ActorCombatant = Unset;
		uint8 ActorGear = Unset;
		/** The battle fields (attack level) a gear action raises: the action's actor's. */
		uint8 ActorFields = Unset;
		uint8 TargetCombatant = Unset;
		uint8 TargetGear = Unset;
		uint8 TargetFields = Unset;
		/** The command the rules read, as an index into the context's command records. */
		uint16 CommandIndex = UnsetCommand;
		/** Elly's ether failed this action: its damage is a third. */
		bool bEtherFailed = false;
		/** Billy's gun had no ammunition: the shot missed and none is used. */
		bool bNoAmmunition = false;
		/** Slots the action knocked out or found knocked out. */
		uint16 KnockedOut = 0;
	};

	/** The result codes an action leaves per slot. */
	namespace BattleResults
	{
		inline constexpr uint8 HpDamage = 0;
		inline constexpr uint8 EpDamage = 1;
		inline constexpr uint8 HpHeal = 2;
		inline constexpr uint8 EpHeal = 3;
		inline constexpr uint8 Miss = 4;
		inline constexpr uint8 HalvedDamage = 5;
		inline constexpr uint8 NoEffect = 6;
		inline constexpr uint8 Counterattack = 7;
		inline constexpr uint8 TurnHpLoss = 8;
		inline constexpr uint8 TurnEpLoss = 9;
		inline constexpr uint8 FuelLoss = 10;
		inline constexpr uint8 FuelGain = 11;
		inline constexpr uint8 NoResult = 0xFF;
	}

	/** One action's record for the show: what each slot took, and what to play. */
	struct FBattlePerformance
	{
		static constexpr int32 SlotCount = 11;

		std::array<uint16, SlotCount> Amounts{};
		/** The same amounts whole: a gear's HP takes them so when the 16 bits would wrap (`bWholeAmounts`). */
		std::array<uint32, SlotCount> WholeAmounts{};
		uint16 Targets = 0;
		std::array<uint8, SlotCount> Results{};
		uint8 Actor = 0;
		/** Each slot's running total over the turn so far, and whether it is a heal or damage. */
		std::array<uint16, SlotCount> Totals{};
		/** The running totals over the amounts whole, and their kinds as the whole sums give them. */
		std::array<uint32, SlotCount> WholeTotals{};
		std::array<uint8, SlotCount> WholeTotalKinds{};
		/** The show's numbers draw the amounts and totals whole, as they are applied (`bWholeAmounts`); else their 16 bits. */
		bool bShownWhole = false;
		uint16 Parameter = 0;
		std::array<uint8, SlotCount> TotalKinds{};
		/** What to play: a command's number or animation, or one of the show's own codes; FFh empty. */
		uint8 Code = 0xFF;
	};

	/** What the battle has come to: still going, won, escaped or lost. */
	namespace BattleEnd
	{
		inline constexpr uint8 Running = 0;
		inline constexpr uint8 Won = 0x01;
		inline constexpr uint8 Escaped = 0x40;
		inline constexpr uint8 Lost = 0x81;
	}

	/** How the battle ended for the program after it (the game state's ProgramResult). */
	namespace BattleModeResult
	{
		inline constexpr uint8 Won = 0;
		inline constexpr uint8 Lost = 1;
		inline constexpr uint8 Escaped = 2;
		/** An event battle lost as its story wants: the story goes on. */
		inline constexpr uint8 ScriptedDefeat = 3;
	}

	/** Why the battle's rules stopped: something the original reads from memory it does not own, or data the port lacks. */
	enum class EBattleFault : uint8
	{
		None,
		/** A weapon slot the actor does not have (a command's bit 0x08, or 0x10 in a gear). */
		UnsetWeaponSlot,
		/** An attack or defense formula past the ones the rules know. */
		UnknownFormula,
		/** A drain or fixed damage whose chance names no source. */
		UnknownChanceSource,
		/** A command past its table. */
		CommandOutsideTable,
		/** A handler the on-foot or the gears' table does not have. */
		UnknownHandler,
		/** An item's stat clear past the status words. */
		StatusWordOutsideRecord,
		/** A record the rules have never pointed at. */
		UnsetRecord,
		/** A formation's group past the stage's eight. */
		GroupOutsideStage,
		/** A formation's enemy type past the scene's eight. */
		TypeOutsideScene,
		/** A party member whose character or gear has no command table. */
		NoCommandTable,
		/** A slot number past the battle's eleven (a forced actor, an enemy's saved commands). */
		SlotOutsideBattle,
		/** A value the game would read or write past its state block (an ammunition's count, a character's record). */
		OutsideGameState,
		/** An enemy script's variable past the AI's records, or over another enemy's scripts. */
		AiVariableOutsideRecords,
		/** An enemy script read past its scene's file. */
		ScriptOutsideScene,
		/** An action-queue entry past the queue's 32, or a queue full of entries carried out past its end. */
		EntryOutsideQueue,
		/** A record for the show past the 32 an action has. */
		PerformanceOutsideRecords,
		/** An index past a table the game keeps (a combatant's field by number, the scripts' globals, a command bit). */
		IndexOutsideTable,
		/** An enemy's script writes the battle event's variables in a battle without an event. */
		NoBattleEvent,
		/** Boarding finds every party gear group taken: the game places the gear by a register it never set. */
		NoGearGroup,
		/** An automatic turn finds no one to aim at but the actor itself: the game draws forever. */
		NoOtherTarget,
		/** An event reads past its script: an entry point, an instruction, an operand. */
		EventOutsideScript,
		/** An event's variable past its 512, in the rest of its work or past it. */
		EventVariableOutsideWork,
		/** An event's record (an actor, an object, a sequence's performer) past its sixteen, or a script with more actors. */
		EventRecordOutsideWork,
		/** An event's opcode past its handlers: the game adds the last handler's length. */
		UnknownEventOpcode,
		/** An event's message past its table. */
		EventMessageOutsideTable,
		/** An event's message waits for a window that never opens, or its run sweeps no actor: the game loops forever. */
		EventHangs,
		/** An enemy's script gave slots scripted turns: the game clears memory through a register it never set and gives them turns forever. */
		ScriptedTurns,
		/** The show met what it does not play (its own fault names it). */
		Show,
	};

	/** An enemy's AI: its four scripts, as offsets into the scene's file, and its variables. */
	struct FBattleAiRecord
	{
		static constexpr std::size_t VariablesSize = 0x30;

		/** The turn script, one never run, the on-hit script and the reaction script. */
		std::array<uint32, 4> Scripts{};
		/** L0-L3 (32 bits) from 0, W0-W7 (16 bits) from 10h, B0-B15 from 20h, little-endian. */
		std::array<uint8, VariablesSize> Variables{};
	};

	/** A point of the route an approach walks for the show: where, and whether it jumps there. */
	struct FBattleWaypoint
	{
		static constexpr uint16 None = 0xFFFF;

		uint16 X = None;
		uint16 Z = None;
		uint8 bJump = 0;
	};

	/**
	 * What the rules hand the show as they go, at the moments the game's presentation acts on them:
	 * the camera framing combatants, a message box's text and the name box, and the playback's start.
	 * The core's show implements it; the rules run without one (nothing shown).
	 */
	class IBattleShow
	{
	public:
		virtual ~IBattleShow() = default;

		/** The camera frames the combatants of the mask (unless a battle event holds it). */
		virtual void FrameCamera(uint16 Slots) = 0;
		/** Message box Box shows entry Entry of the scene's name table. */
		virtual void SetMessageBox(uint8 Box, uint8 Entry) = 0;
		/** The acting enemy's name box shows entry Entry of the scene's name table. */
		virtual void ShowNameBox(uint8 Entry) = 0;
		/** The playback of an action's records begins: the actor walks to the target unless it stays, and the next to act is readied. */
		virtual void BeginPlayback(bool bStays, uint8 Actor, uint8 Target, uint8 NextReady) = 0;
		/** The playback begun for an attack is dropped (the attack mode left before its first hit): the actor stands back. */
		virtual void CancelPlayback() = 0;
		/** The markers point at the combatants of the mask (none for 0). */
		virtual void SetTargetMarkers(uint16 Slots) = 0;
		/** One of the system's sound effects plays. */
		virtual void PlaySound(uint8 Effect) = 0;
		/** A party member climbs into its gear. */
		virtual void BoardGear(uint8 Slot) = 0;
		/** A turn's drains (Poison, EP loss, a fuel leak, the Booster) shown on the actor: its flinch and a number for each amount not 0 (HP, EP, fuel). */
		virtual void ShowTurnDamage(uint8 Slot, const std::array<uint32, 3>& Amounts) = 0;
		/** A battle event's load is done: its own sound effects are registered and the battle's sound bank is in memory. */
		virtual void BeginEvent() = 0;
		/** The battle's own sound bank brought back if a playback gave its room away: as a run of the event's scripts begins, and before the result's rewards. */
		virtual void LoadBattleBank() = 0;
		/** A battle event's instruction's presentation. */
		virtual void PlayEventCue(const FBattleEventCue& Cue) = 0;
		/**
		 * The show still plays what the last call the game waits on began: the playback's start, a boarding, the
		 * turn's drains, the next actor readied, the poses, an event's special attack, voice or loads. The caller
		 * takes frames until it is done.
		 */
		virtual bool IsBlocking() const = 0;
		/** A turn's own presentation: the action's name, the boxes hidden, the next actor readied, the poses. */
		virtual void PlayTurnCue(EBattleTurnCue Cue, uint8 Arg) = 0;
		/** The HUD's drawing of the party's menus for party slot Slot (the ring, the AP, the arrows, the hints), from the turn's state. */
		virtual void PlayMenuCue(EBattleMenuCue Cue, uint8 Slot, uint8 Arg) = 0;
		/** The battle's end: the loads finished, the party's pose, the dim, the music's fade. */
		virtual void PlayEndCue(EBattleEndCue Cue, uint8 Arg) = 0;
	};

	/** What a gear's turn start works out for the gear menu and its attacks. */
	struct FBattleGearTurn
	{
		/** The fuel costs of the gear's commands 0-14. */
		std::array<uint16, 15> Costs{};
		/** The fuel Charge adds. */
		uint16 Charge = 0;
		/** The attack the cockpit shows. */
		uint16 ShownAttack = 0;
		uint8 Agility = 0;
		uint8 ArmorBreak = 0;
		/** The percent chance of Infinity starting, as the cockpit shows it. */
		uint8 InfinityChance = 0;
		/** The cockpit's warning lamps: 8000h Stop Eth Engine, 4000h EP DOWN, 2000h fuel leak, 1000h ACC/EV DOWN, 800h low fuel. */
		uint16 Lamps = 0;
		bool bBooster = false;
		/** The attack level the turn began at (4 in Infinity). */
		uint8 Level = 0;
	};

	/** A party member's figures as the battle began, which the result screen compares with the end's. */
	struct FBattleStartFigures
	{
		uint32 Exp = 0;
		uint32 EtherExp = 0;
		uint16 MaxHp = 0;
		uint16 MaxEp = 0;
		/** Attack with the weapon, hit, defense with the armor, evade, ether, ether defense, agility. */
		std::array<uint8, 7> Stats{};
	};

	/**
	 * Everything the battle's rules read and change: the combatants and their command tables, the
	 * slots, the turn order, the action being carried out and the records the show plays, the game
	 * state the battle was entered from and the game's random numbers. The rules keep nothing of
	 * their own.
	 */
	struct FBattleContext
	{
		static constexpr int32 SlotCount = 11;
		static constexpr int32 PartySlotCount = 3;
		static constexpr int32 EnemySlotCount = 8;
		static constexpr int32 PerformanceCount = 32;
		static constexpr int32 ListedItemCount = 48;
		static constexpr uint8 NoCharacter = 0x7F;
		static constexpr int32 PartyTableSize = FBattleTables::CharacterCommandCount;
		static constexpr int32 GearTableSize = FBattleTables::GearCommandCount + 1;
		static constexpr int32 FirstGearCommand = PartySlotCount * PartyTableSize;
		static constexpr int32 FirstEnemyCommand = FirstGearCommand + PartySlotCount * GearTableSize;
		static constexpr int32 EnemyTableSize = 200;
		static constexpr int32 CommandRecordCount = FirstEnemyCommand + EnemyTableSize;
		static constexpr std::size_t AiQueueSize = 0x100;
		static constexpr std::size_t RouteLength = 9;
		static constexpr std::size_t EventVariableCount = 0x200;

		FGameState* GameState = nullptr;
		FPsxRandom* Random = nullptr;
		/** The show the rules hand their presentation to; none for the rules alone. */
		IBattleShow* Show = nullptr;
		/** The Battle program's tables and libgte's arctangents, which the party's commands read. */
		const FBattleProgramTables* Program = nullptr;
		const FGteArctangents* Arctangents = nullptr;

		FBattleFormation Formation;
		/** The stage's layout, whose routes the enemies' scripts change. */
		FBattleStage Stage;
		/** Slot 0 the party's leader, slots 1 and 2 the adult Emeralda in gear 17. */
		bool bSpecialParty = false;
		bool bEventBattle = false;
		/** The field's gears cannot be boarded or left where the battle began: boarding leaves the party's gear flags as they are. */
		bool bGearsLocked = false;
		/**
		 * The party takes the order its player keeps it in (FPartyOrder) where no event stages the battle and no special
		 * party is formed: the formation's slots are the members as the player shows them, their gears' bytes by member.
		 */
		bool bPlayersOrder = false;
		/** The characters in the party's slots, NoCharacter for none. */
		std::array<uint8, PartySlotCount> PartyCharacters{NoCharacter, NoCharacter, NoCharacter};
		/** How many party slots hold a member. */
		uint8 PartyCount = 0;

		std::array<FBattleCombatant, SlotCount> Combatants{};
		std::array<FBattleSlot, SlotCount> Slots{};
		/** Which slots take part: the party's filled slots and the formation's enemies. */
		std::array<bool, SlotCount> InBattle{};
		std::array<FBattleApRecord, SlotCount> ApRecords{};
		/** Place counts per stage group: party on foot 0-7, enemies on foot 8-15, party in gears 16-23, enemies in gears 24-31. */
		std::array<FBattleOccupancy, 32> Occupancy{};
		std::array<FBattleStartFigures, PartySlotCount> StartFigures{};
		/** The party's disabled commands as the battle began, which ending Play Dead gives back. */
		std::array<uint16, PartySlotCount> StartingDisabledCommands{};
		/** The enemies' AI: whether each has an on-hit and a reaction script, and whether it has left the battle. */
		std::array<bool, EnemySlotCount> HasHitScript{};
		std::array<bool, EnemySlotCount> HasReactionScript{};
		std::array<bool, EnemySlotCount> Escaped{};
		/** The enemies a party turn aimed at, whose reaction scripts run after it. */
		std::array<bool, EnemySlotCount> Targeted{};
		/** The scene's file, whose scripts the enemies run. */
		std::span<const uint8> SceneFile;
		std::array<FBattleAiRecord, EnemySlotCount> AiRecords{};
		/** The enemies' action queue: 32 entries of 8 bytes (type, command or field, code, message, value, targets). */
		std::array<uint8, AiQueueSize> AiQueue{};
		/** Where one party turn's reaction scripts add to the queue: each goes on where the one before stopped. */
		uint8 ReactionQueueNext = 0;
		/** The acting slot a party turn's reactions put back when they are done. */
		uint8 ReactionTurnActor = 0;
		/** Each slot's name in the scene's name table (an enemy slot's type + 1). */
		std::array<uint8, SlotCount> NameEntries{};
		/** The next of the eight message boxes the enemies' messages take in a turn (1 at its start). */
		uint8 NextMessageBox = 0;
		/**
		 * With `bDeusKeepsCamera` (or a program that does it, FBattleProgramTables::bQueueKeepsCamera), the last queue
		 * write's third byte (Perfect Works' rebalance: the byte after it): while it is set, the show's next taking of the
		 * camera for a model (sequence op 68) is passed over, and that op or the next damage numbers clear it.
		 */
		uint8 KeepCamera = 0;
		/** The route the next approach walks, from the actor's place (for the show). */
		std::array<FBattleWaypoint, RouteLength> Route{};
		/** The battle event's variables, which the enemies' scripts may write; there are none without an event. */
		bool bEventWork = false;
		std::array<uint16, EventVariableCount> EventVariables{};
		/** The battle's event: its machine, its message and what it asks of the battle (its variables are EventVariables). */
		FBattleEventWork Event;
		/** The fixes of the event's broken effects in use; the setup keeps them. */
		FBattleEventOptions EventOptions;
		/** The fixes of the rules' broken effects in use; the setup keeps them. */
		FBattleRuleOptions RuleOptions;
		/**
		 * The rules of Perfect Works Build's rebalance in place of the original's, which its party and items are made for
		 * (research inputs/perfect-works-build.md, "Its rebalance code"); the setup keeps them.
		 */
		bool bPerfectWorksRebalance = false;
		/** The player's changes to the balance in use (the rewards'); the setup keeps them. */
		FGameBalance Balance;
		/** What the battle's result works out (BattleResult.h), and its routine with the screen. */
		FBattleResultWork Result;
		FBattleResultRun ResultRun;
		/** The turn under way (BattleTurnMachine.h). */
		FBattleTurnWork Turn;
		/** The battle's end under way (BattleEnding.h). */
		FBattleEndingWork Ending;
		/** The party turn's current attack: 0 none, else the command number + 1 (an enemy's act sets it too). */
		uint8 CurrentAttack = 0;
		/** Each slot's current target, chosen again after every turn (FFh none). */
		std::array<uint8, SlotCount> SlotTargets{};
		/** The party's turn under way, and the target selector's candidates. */
		FBattlePartyInput Party;
		FBattleTargeting Targeting;
		/** The show has played the turn's queued actions (it ends the turn's waits). */
		bool bActionsPlayed = false;
		/** Each party member's learned Deathblows and abilities as the battle's opening ended, which the result screen compares with the end's. */
		std::array<std::array<uint16, 2>, PartySlotCount> SkillsAtStart{};

		/**
		 * Every command the battle can run, in one run as the game lays its tables out: each party
		 * slot's own table, then each one's gear's (a record longer than the gear tables are), then the
		 * enemies' (with room for more records than it has). A command number read past its own table
		 * lands in the next one, as in the game.
		 */
		std::array<FBattleCommand, CommandRecordCount> Commands{};
		std::array<FBattleItem, FBattleTables::ItemCount> Items{};
		/** The copied weapons' attacks (FBattleTables::AmmunitionAttacks). */
		std::array<uint8, FBattleTables::AmmunitionCount> AmmunitionAttacks{};
		/** The inventory's battle items as the battle lists them: the item in each of the list's cells and its count. */
		std::array<uint8, ListedItemCount> ListedItems{};
		std::array<uint8, ListedItemCount> ListedCounts{};
		std::array<uint8, ListedItemCount> ListedItemsAtStart{};
		/** Billy's ammunition in the inventory, listed for the item menu's second page. */
		std::array<uint8, ListedItemCount> ListedAmmunition{};
		std::array<uint8, ListedItemCount> ListedAmmunitionCounts{};

		/** Steps each slot has left to wait before it can act, and the wait it was last given. */
		std::array<uint16, SlotCount> Waits{};
		std::array<uint16, SlotCount> FullWaits{};
		/** 1 ready to act; FFh never again. */
		std::array<uint8, SlotCount> Ready{};
		/** Slow's toggle: a slowed slot's wait moves every other step. */
		std::array<uint16, SlotCount> SlowToggles{};
		/** The battle's random order of the slots, which settles who of the ready acts first. */
		std::array<uint8, SlotCount> TurnOrder{};
		uint8 TurnOrderNext = 0;
		/** A slot + 1 an enemy's script forces to act next; 0 none. */
		uint8 ForcedActor = 0;
		/** Slots the enemies' scripts made take scripted turns. */
		uint16 ScriptedTurns = 0;
		/** The waits run: clear during every turn. */
		bool bWaitsRunning = false;
		/** The slot picked to act + 1 (0 none) until its turn starts, then the acting slot itself. */
		uint8 TurnActor = 0;

		FBattleAction Action;
		std::array<FBattlePerformance, PerformanceCount> Performances{};
		uint8 PerformanceIndex = 0;
		/** Each slot's running total over the turn, and whether it is a heal or damage. */
		std::array<uint16, SlotCount> TurnTotals{};
		std::array<uint8, SlotCount> TurnTotalKinds{};
		/** The same over the amounts whole, for the show where they are applied whole. */
		std::array<int32, SlotCount> TurnWholeTotals{};
		std::array<uint8, SlotCount> TurnWholeTotalKinds{};
		/** Slots an action changed, and the party's panels to draw again. */
		std::array<uint8, SlotCount> ChangedSlots{};
		std::array<uint8, PartySlotCount> PanelRefresh{};
		/** An item's HP and EP heals go to the character itself, even in a gear. */
		bool bItemHealsPilot = false;
		FBattleGearTurn GearTurn;

		/** The living combatants: in the battle, not knocked out, not hidden, not escaped. */
		uint16 Living = 0;
		/** Enemies that stay counted alive while knocked out, to react. */
		uint16 ReactingWhileKnockedOut = 0;
		/** BattleEnd value. */
		uint8 End = BattleEnd::Running;

		EBattleFault Fault = EBattleFault::None;

		static constexpr bool IsParty(int32 Slot) { return Slot < PartySlotCount; }
		static constexpr uint16 PartyCommandIndex(int32 Slot, int32 Command) { return static_cast<uint16>(Slot * PartyTableSize + Command); }
		static constexpr uint16 GearCommandIndex(int32 Slot, int32 Command) { return static_cast<uint16>(FirstGearCommand + Slot * GearTableSize + Command); }
		static constexpr uint16 EnemyCommandIndex(int32 Command) { return static_cast<uint16>(FirstEnemyCommand + Command); }
		/** Whether a record holds a command the tables gave: not a gear table's last, nor past the enemies' table. */
		WELTALLCORE_API static bool IsLoadedCommand(int32 Index);
	};
}
