#include "Battle/BattleSetup.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleEnd.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleParty.h"
#include "Battle/BattlePartyStart.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleScene.h"
#include "Battle/BattleTables.h"
#include "Battle/BattleWaits.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		constexpr uint8 SpecialPartyGear = 17;
		constexpr uint8 SpecialPartyCharacter = 10;
		constexpr uint8 NoGear = 0xFF;
		/** The menu's commands the formation or a missing gear switch off: boarding, and escaping. */
		/** The menu's Gear and Escape commands, by number. */
		constexpr std::size_t BoardCommandNumber = 7;
		constexpr std::size_t EscapeCommandNumber = 8;
		/** The items numbered from 1 to this one are the battle's. */
		constexpr uint8 LastBattleItem = 0x30;
		/** Billy's ammunition among the weapons: 50 to 72. */
		constexpr uint8 FirstAmmunition = 50;
		constexpr uint8 AmmunitionKinds = 23;
		/** Enemies whose traits have this bit act first. */
		constexpr uint16 ActsFirst = 0x0200;
		/** A script offset of a type that has no such script. */
		constexpr uint16 NoScript = 0xFFFF;

		/** Whether the battle takes the party in the order its player has set: none an event stages, nor a special party. */
		bool InPlayersOrder(const FBattleContext& Context)
		{
			return Context.bPlayersOrder && Context.GameState->PartyOrder().IsSet()
				&& (Context.Formation.Flags & (FormationFlags::EventBattle | FormationFlags::SpecialParty)) == 0;
		}

		/**
		 * The party the battle takes: those of the game's party who have joined, closed up (in the order its player keeps it
		 * where the battle does: InPlayersOrder); or the special party.
		 */
		void ChooseParty(FBattleContext& Context)
		{
			const FGameState& State = *Context.GameState;
			const std::array<uint8, 3> Party = InPlayersOrder(Context) ? State.PartyOrder().Shown(State.Party()) : State.Party();
			Context.bSpecialParty = (Context.Formation.Flags & FormationFlags::SpecialParty) != 0;
			if (Context.bSpecialParty)
			{
				Context.PartyCharacters = {static_cast<uint8>(Party[0] & 0x7F), SpecialPartyCharacter, SpecialPartyCharacter};
				return;
			}
			const uint16 Joined = State.JoinedCharacters() & State.MenuCharacters() & 0x7FF;
			std::size_t Count = 0;
			for (const uint8 Character : Party)
			{
				if (Character < 16 && (Joined & SlotBit(Character)) != 0)
				{
					Context.PartyCharacters[Count++] = static_cast<uint8>(Character & 0x7F);
				}
			}
			for (; Count < Context.PartyCharacters.size(); ++Count)
			{
				Context.PartyCharacters[Count] = FBattleContext::NoCharacter;
			}
		}

		/** The code a command plays: its own animation where it names one, else its number. */
		uint8 PlayedCode(const FBattleCommand& Command, int32 Number)
		{
			return static_cast<uint8>((Command.Flags & BattleCommandFlags::OwnAnimation) != 0 ? Command.Animation : Number);
		}

		/**
		 * For each code Own's commands play, the code Drawn's same command plays. Where several of Own's play one code, the
		 * lowest command's stands: the later ones (the counterattack, Deathblows the second Citan and the grown Emeralda never
		 * learn) are never played as their member's own.
		 */
		std::array<uint8, 256> DrawnCodes(const FBattleTables::FCharacterCommands& Own, const FBattleTables::FCharacterCommands& Drawn)
		{
			std::array<uint8, 256> Codes{};
			for (std::size_t Code = 0; Code < Codes.size(); ++Code)
			{
				Codes[Code] = static_cast<uint8>(Code);
			}
			for (int32 Command = FBattleTables::CharacterCommandCount - 1; Command >= 0; --Command)
			{
				Codes[PlayedCode(Own[static_cast<std::size_t>(Command)], Command)] = PlayedCode(Drawn[static_cast<std::size_t>(Command)], Command);
			}
			return Codes;
		}

		/** Each party member's combatant from its character and gear records, and the command tables. */
		void LoadCombatants(FBattleContext& Context, const FBattleTables& Tables)
		{
			ChooseParty(Context);
			for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				const uint8 Character = Context.PartyCharacters[static_cast<std::size_t>(Slot)];
				if (Character == FBattleContext::NoCharacter)
				{
					continue;
				}
				if (Character >= FBattleTables::CharacterCount)
				{
					Context.Fault = EBattleFault::NoCommandTable;
					return;
				}
				std::array<uint8, FBattleCombatant::RecordSize> Record{};
				const std::span<const uint8> CharacterRecord = BattleGameState::CharacterRecord(*Context.GameState, Character);
				std::copy(CharacterRecord.begin(), CharacterRecord.end(), Record.begin());
				if (Context.bSpecialParty && Slot > 0)
				{
					Record[0xA0] = SpecialPartyGear;
				}
				const uint8 Gear = Record[0xA0] == NoGear ? 0 : Record[0xA0];
				if (Gear >= FBattleTables::GearCount)
				{
					Context.Fault = EBattleFault::NoCommandTable;
					return;
				}
				const std::span<const uint8> GearRecord = BattleGameState::GearRecord(*Context.GameState, Gear);
				std::copy(GearRecord.begin(), GearRecord.end(), Record.begin() + FBattleCombatant::CharacterRecordSize);
				Context.Combatants[static_cast<std::size_t>(Slot)] = FBattleCombatant::FromRecord(Record);
				const FBattleTables::FCharacterCommands& Own = Tables.CharacterCommands(Character);
				std::copy(Own.begin(), Own.end(), Context.Commands.begin() + FBattleContext::PartyCommandIndex(Slot, 0));
				const uint8 Drawn = Context.PartyLooks().Drawn(Character);
				Context.DrawnCodes[static_cast<std::size_t>(Slot)] =
					Drawn != Character ? std::optional(DrawnCodes(Own, Tables.CharacterCommands(Drawn))) : std::nullopt;
				const FBattleTables::FGearCommands& GearOwn = Tables.GearCommands(Gear);
				std::copy(GearOwn.begin(), GearOwn.end(), Context.Commands.begin() + FBattleContext::GearCommandIndex(Slot, 0));
			}
			const auto& Enemies = Tables.EnemyCommands();
			std::copy(Enemies.begin(), Enemies.end(), Context.Commands.begin() + FBattleContext::EnemyCommandIndex(0));
			Context.Items = Tables.Items();
			Context.AmmunitionAttacks = Tables.AmmunitionAttacks();
		}

		/**
		 * The game's party slot whose riding byte a battle slot reads: the slot itself, as the original reads it even
		 * past a member it closed up; in the player's order, the slot of the member the battle slot holds.
		 */
		std::size_t GameSlotOf(const FBattleContext& Context, std::size_t Slot)
		{
			if (!InPlayersOrder(Context) || Slot >= FBattleContext::PartySlotCount)
			{
				return Slot;
			}
			const std::array<uint8, 3> Party = Context.GameState->Party();
			for (std::size_t Member = 0; Member < Party.size(); ++Member)
			{
				if (Party[Member] != FGameState::EmptySlot && (Party[Member] & 0x7F) == Context.PartyCharacters[Slot])
				{
					return Member;
				}
			}
			return Slot;
		}

		void ResetSlots(FBattleContext& Context)
		{
			Context.GameState->BattleActingTurns = 0;
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				Context.Slots[Slot].Occupant = Context.PartyCharacters[Slot];
			}
			for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				FBattleSlot& Target = Context.Slots[Slot];
				Target.Presence = 0;
				Target.InGear = Context.bSpecialParty ? 1 : BattleGameState::Byte(*Context.GameState, GameStateLayout::PartyInGearOffset + GameSlotOf(Context, Slot));
				Target.Byte5 = 0;
			}
			Context.End = BattleEnd::Running;
			Context.ForcedActor = 0;
		}

		/** Which slots fight, where each stands in its group and on the stage, and how full each group is. */
		void PlaceFormation(FBattleContext& Context)
		{
			const FBattleFormation& Formation = Context.Formation;
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				FBattleSlot& Target = Context.Slots[Slot];
				Context.InBattle[Slot] = (Target.Occupant & 0x7F) != FBattleSlot::Empty;
				Target.Group = Target.InGear == 0 ? static_cast<uint8>(Formation.PartyGroups[Slot] & 0x7F) : static_cast<uint8>(Slot);
			}
			for (std::size_t Enemy = 0; Enemy < FBattleContext::EnemySlotCount; ++Enemy)
			{
				const std::size_t Slot = Enemy + FBattleContext::PartySlotCount;
				FBattleSlot& Target = Context.Slots[Slot];
				const uint8 Type = Formation.EnemyTypes[Enemy];
				if ((Type & 0x7F) == FBattleFormation::EmptyType)
				{
					Context.Combatants[Slot].Hp = 0;
					Context.Combatants[Slot].MaxHp = 0;
					Target.Occupant = FBattleSlot::Empty;
					Target.Presence = 0;
					Target.InGear = 0;
					Context.InBattle[Slot] = false;
				}
				else
				{
					Target.Occupant = static_cast<uint8>(Type & 0x7F);
					Target.Presence = static_cast<uint8>(Formation.EnemyFlags[Enemy] & 0x80);
					Target.InGear = static_cast<uint8>(Type & 0x80);
					Target.Byte5 = static_cast<uint8>(Formation.EnemyFlags[Enemy] & 1);
					Context.InBattle[Slot] = true;
					Target.Group = static_cast<uint8>(Formation.EnemyGroups[Enemy] & 0x7F);
				}
				Context.NameEntries[Slot] = static_cast<uint8>(Target.Occupant + 1);
			}
			Context.Occupancy = {};
			for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				FBattleSlot& Target = Context.Slots[Slot];
				if (Target.Occupant == FBattleSlot::Empty)
				{
					continue;
				}
				if (Target.Group >= FBattleStage::GroupCount)
				{
					Context.Fault = EBattleFault::GroupOutsideStage;
					return;
				}
				const bool bEnemy = Slot >= FBattleContext::PartySlotCount;
				if (Target.InGear == 0)
				{
					FBattleOccupancy& Entry = Context.Occupancy[Target.Group + (bEnemy ? 8u : 0u)];
					Target.Place = Entry.Count;
					Entry.Places = static_cast<uint8>(Entry.Places | SlotBit(Target.Place));
					Entry.Count = static_cast<uint8>(Entry.Count + 1);
					Target.Position = bEnemy ? Context.Stage.EnemyPlace(Target.Group, Target.Place) : Context.Stage.PartyPlace(Target.Group, Target.Place);
				}
				else
				{
					Target.Place = 0;
					Context.Occupancy[Target.Group + (bEnemy ? 24u : 16u)] = {1, 1};
					Target.Position = bEnemy ? Context.Stage.EnemyGearPlace(Target.Group) : Context.Stage.PartyGearPlace(Target.Group);
				}
			}
		}

		/** The enemies' combatants from the scene, and what their scripts start from. */
		void LoadEnemies(FBattleContext& Context, const FBattleScene& Scene)
		{
			Context.Action.KnockedOut = 0;
			Context.ScriptedTurns = 0;
			for (std::size_t Enemy = 0; Enemy < FBattleContext::EnemySlotCount; ++Enemy)
			{
				const std::size_t Slot = Enemy + FBattleContext::PartySlotCount;
				Context.Escaped[Enemy] = false;
				const uint8 Type = Context.Slots[Slot].Occupant;
				if (Type == FBattleSlot::Empty)
				{
					Context.Combatants[Slot] = FBattleCombatant{};
					Context.HasHitScript[Enemy] = false;
					Context.HasReactionScript[Enemy] = false;
					continue;
				}
				if (Type >= FBattleScene::TypeCount)
				{
					Context.Fault = EBattleFault::TypeOutsideScene;
					return;
				}
				Context.Combatants[Slot] = Scene.Enemy(Type);
				FBattleAiRecord& Ai = Context.AiRecords[Enemy];
				const uint32 Block = Scene.ScriptBlock(Type);
				for (int32 Script = 0; Script < 4; ++Script)
				{
					const uint16 Offset = Scene.ScriptOffset(Type, Script);
					if (Script < 2 || Offset != NoScript)
					{
						Ai.Scripts[static_cast<std::size_t>(Script)] = Block + Offset;
					}
				}
				Ai.Variables = {};
				Context.HasHitScript[Enemy] = Scene.ScriptOffset(Type, 2) != NoScript;
				Context.HasReactionScript[Enemy] = Scene.ScriptOffset(Type, 3) != NoScript;
			}
		}

		/** Who fights in a gear, and the party's levels for the turn-start panel. */
		void InitParty(FBattleContext& Context)
		{
			InitPartyCombatants(Context);
			if (Context.bSpecialParty)
			{
				InitSpecialPartyMembers(Context);
			}
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				FBattleCombatant& Member = Context.Combatants[Slot];
				const bool bGear = Context.Slots[Slot].Occupant != FBattleSlot::Empty && Context.Slots[Slot].InGear != 0;
				Context.ApRecords[Slot].InGear = bGear ? 1 : 0;
				Member.BattleFlags = static_cast<uint8>(bGear ? Member.BattleFlags | BattleFlags::InGear : Member.BattleFlags & ~BattleFlags::InGear);
			}
			for (std::size_t Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
			{
				const FBattleSlot& Enemy = Context.Slots[Slot];
				Context.ApRecords[Slot].InGear = Enemy.Occupant != FBattleSlot::Empty && Enemy.InGear != 0 ? 1 : 0;
			}
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				Context.ApRecords[Slot].Level = Context.Combatants[Slot].Level;
				Context.ApRecords[Slot].EtherLevel = Context.Combatants[Slot].EtherLevel;
			}
		}

		/** The battle's item list from the inventory, which the battle also tidies (counts capped at 99, empty slots cleared). */
		void ListItems(FBattleContext& Context)
		{
			using namespace GameStateLayout;
			FGameState& State = *Context.GameState;
			Context.ListedItems = {};
			Context.ListedCounts = {};
			Context.ListedItemsAtStart = {};
			const FInventoryList& Items = InventoryLists[0];
			for (std::size_t Slot = 0; Slot < Items.Slots; ++Slot)
			{
				uint8& Count = BattleGameState::Byte(State, Items.Counts + Slot);
				Count = std::min<uint8>(Count, 99);
				if (Count == 0)
				{
					BattleGameState::Byte(State, Items.Items + Slot) = 0;
				}
			}
			std::size_t Listed = 0;
			for (std::size_t Slot = 0; Slot < Items.Slots && Listed < FBattleContext::ListedItemCount; ++Slot)
			{
				const uint8 Item = BattleGameState::Byte(State, Items.Items + Slot);
				if (Item != 0 && Item <= LastBattleItem)
				{
					Context.ListedItems[Listed] = Item;
					Context.ListedCounts[Listed] = BattleGameState::Byte(State, Items.Counts + Slot);
					Context.ListedItemsAtStart[Listed] = Item;
					++Listed;
				}
			}
			const FInventoryList& Weapons = InventoryLists[1];
			Context.ListedAmmunition = {};
			Context.ListedAmmunitionCounts = {};
			Listed = 0;
			for (std::size_t Slot = 0; Slot < Weapons.Slots; ++Slot)
			{
				const uint8 Weapon = BattleGameState::Byte(State, Weapons.Items + Slot);
				if (static_cast<uint8>(Weapon - FirstAmmunition) < AmmunitionKinds && Listed < FBattleContext::ListedItemCount)
				{
					Context.ListedAmmunition[Listed] = Weapon;
					Context.ListedAmmunitionCounts[Listed] = BattleGameState::Byte(State, Weapons.Counts + Slot);
					++Listed;
				}
			}
		}

		/** The first waits, the battle's random order of the slots, and the first actor ready after one step. */
		void InitTurnOrder(FBattleContext& Context)
		{
			Context.Action.Command = 0;
			InitBattleWaits(Context);
			std::array<bool, FBattleContext::SlotCount> Taken{};
			for (std::size_t Count = 0; Count < FBattleContext::SlotCount;)
			{
				const uint8 Slot = RandomByteBetween(*Context.Random, 0, 10);
				if (!Taken[Slot])
				{
					Taken[Slot] = true;
					Context.TurnOrder[Count++] = Slot;
				}
			}
			Context.TurnOrderNext = 0;
			for (std::size_t Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if (Context.InBattle[Slot] && (Context.Combatants[Slot].Traits & ActsFirst) != 0)
				{
					Context.Waits[Slot] = 1;
					Context.FullWaits[Slot] = 1;
				}
			}
			int32 Soonest = 0xFFFF;
			for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if (Context.InBattle[Slot] && static_cast<int16>(Context.Waits[Slot]) < Soonest)
				{
					Soonest = static_cast<int16>(Context.Waits[Slot]);
				}
			}
			for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
			{
				if (Context.InBattle[Slot])
				{
					Context.Waits[Slot] = static_cast<uint16>(Context.Waits[Slot] - static_cast<uint16>(Soonest - 1));
				}
			}
		}

		/**
		 * The party's AP, every slot's default target, the party's disabled commands for the menu and the
		 * commands the formation or a missing gear switch off.
		 */
		void InitCommands(FBattleContext& Context)
		{
			if ((Context.Formation.Flags & FormationFlags::EventBattle) != 0)
			{
				Context.bEventBattle = true;
			}
			const auto& Bits = Context.Program->CommandBits;
			for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				const uint8 Character = Context.PartyCharacters[Slot];
				if (Character < FBattleTables::CharacterCount)
				{
					Context.ApRecords[Slot].ApPerTurn = BattleGameState::Byte(*Context.GameState, BattleGameState::SkillsOffset(Character, GameStateLayout::Skills::ApPerTurn));
				}
				AimBattleSlot(Context, static_cast<uint8>(Slot));
				FBattleCombatant& Member = Context.Combatants[Slot];
				auto& Disabled = Context.Party.Disabled[Slot];
				for (std::size_t Command = 0; Command < Disabled.size(); ++Command)
				{
					Disabled[Command] = Member.DisabledCommands & Bits[Command];
				}
				if (Member.Gear == NoGear || (Context.Formation.Flags & FormationFlags::NoBoarding) != 0)
				{
					Disabled[BoardCommandNumber] = Bits[BoardCommandNumber];
					Member.DisabledCommands |= Bits[BoardCommandNumber];
				}
				if ((Context.Formation.Flags & FormationFlags::NoEscape) != 0)
				{
					Disabled[EscapeCommandNumber] = Bits[EscapeCommandNumber];
					Member.DisabledCommands |= Bits[EscapeCommandNumber];
				}
			}
			for (uint8 Slot = FBattleContext::PartySlotCount; Slot < FBattleContext::SlotCount; ++Slot)
			{
				AimBattleSlot(Context, Slot);
			}
		}
	}

	void BeginBattleSetup(FBattleContext& Context, const FBattleFormation& Formation, const FBattleScene& Scene)
	{
		FGameState* const State = Context.GameState;
		FUnreachedCases* const Unreached = Context.Unreached;
		FPsxRandom* const Random = Context.Random;
		IBattleShow* const Show = Context.Show;
		const FBattleProgramTables* const Program = Context.Program;
		const FGteArctangents* const Arctangents = Context.Arctangents;
		const bool bGearsLocked = Context.bGearsLocked;
		const bool bPlayersOrder = Context.bPlayersOrder;
		const FBattleEventOptions EventOptions = Context.EventOptions;
		const FBattleRuleOptions RuleOptions = Context.RuleOptions;
		const bool bPerfectWorksRebalance = Context.bPerfectWorksRebalance;
		const FGameBalance Balance = Context.Balance;
		const EMesAutoAdvance AutoAdvance = Context.AutoAdvance;
		Context = FBattleContext{};
		Context.GameState = State;
		Context.Unreached = Unreached;
		Context.Random = Random;
		Context.Show = Show;
		Context.Program = Program;
		Context.Arctangents = Arctangents;
		Context.bGearsLocked = bGearsLocked;
		Context.bPlayersOrder = bPlayersOrder;
		Context.EventOptions = EventOptions;
		Context.RuleOptions = RuleOptions;
		Context.bPerfectWorksRebalance = bPerfectWorksRebalance;
		Context.Balance = Balance;
		Context.AutoAdvance = AutoAdvance;
		Context.Formation = Formation;
		Context.SceneFile = Scene.Bytes();
		// The battle's memory starts cleared: zeros where the context names "none" until the setup and the
		// turns write it.
		for (FBattleSlot& Slot : Context.Slots)
		{
			Slot.Occupant = 0;
		}
		for (FBattlePerformance& Performance : Context.Performances)
		{
			Performance.Code = 0;
		}
		for (FBattleWaypoint& Waypoint : Context.Route)
		{
			Waypoint.X = 0;
			Waypoint.Z = 0;
		}
		Context.Party.ShownState = 0;
		Context.Party.Event = 0;
		Context.Targeting.Target = 0;
	}

	void RunBattleSetupStage(FBattleContext& Context, EBattleSetupStage Stage, const FBattleTables& Tables, const FBattleScene& Scene)
	{
		if (Context.Fault != EBattleFault::None)
		{
			return;
		}
		switch (Stage)
		{
		case EBattleSetupStage::Combatants:
			LoadCombatants(Context, Tables);
			break;
		case EBattleSetupStage::Placement:
			ResetSlots(Context);
			PlaceFormation(Context);
			if (Context.Fault != EBattleFault::None)
			{
				return;
			}
			LoadEnemies(Context, Scene);
			if (Context.Fault != EBattleFault::None)
			{
				return;
			}
			InitParty(Context);
			break;
		case EBattleSetupStage::Order:
			ListItems(Context);
			InitTurnOrder(Context);
			InitCommands(Context);
			break;
		}
	}

	void SetUpBattle(FBattleContext& Context, const FBattleFormation& Formation, const FBattleTables& Tables, const FBattleScene& Scene,
		const FBattleStage& Stage)
	{
		BeginBattleSetup(Context, Formation, Scene);
		Context.Stage = Stage;
		Context.bWaitsRunning = true;
		for (const EBattleSetupStage Next : {EBattleSetupStage::Combatants, EBattleSetupStage::Placement, EBattleSetupStage::Order})
		{
			RunBattleSetupStage(Context, Next, Tables, Scene);
		}
		if (Context.Fault == EBattleFault::None)
		{
			CheckBattleEnd(Context);
		}
	}

	void FinishBattlePartySetup(FBattleContext& Context)
	{
		using namespace GameStateLayout;
		FGameState& State = *Context.GameState;
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Context.PartyCharacters[static_cast<std::size_t>(Slot)] == FBattleContext::NoCharacter)
			{
				continue;
			}
			PointActorAt(Context, Slot);
			FBattleCombatant& Member = Context.Combatants[static_cast<std::size_t>(Slot)];
			Context.StartingDisabledCommands[static_cast<std::size_t>(Slot)] = Member.DisabledCommands;
			FBattleGearPart& Gear = Member.GearPart;
			for (int32 Kind = 0; Kind < 4; ++Kind)
			{
				const uint8 Ground = Context.Stage.Terrain(Kind);
				if (Ground != 0)
				{
					Gear.Agility = static_cast<uint8>(Gear.Agility - Ground);
					Gear.Agility = static_cast<uint8>(Gear.Agility + Gear.TerrainAgility[static_cast<std::size_t>(Kind)]);
				}
			}
			Gear.Agility = std::min<uint8>(Gear.Agility, 16);
		}
		constexpr std::size_t Maria = 8;
		BattleGameState::Byte(State, CharacterOffset(Maria) + Character::Agility) = 7;
		if ((BattleGameState::Word(State, BattleGameState::SkillsOffset(Maria, Skills::Special)) & 0x2000) != 0)
		{
			const std::size_t Abilities = BattleGameState::SkillsOffset(Maria, Skills::Abilities);
			BattleGameState::SetWord(State, Abilities, static_cast<uint16>(BattleGameState::Word(State, Abilities) | 0x800));
		}
		if (BattleGameState::Story(State) < 0xBB)
		{
			struct FForcedMultiplier
			{
				std::size_t Gear;
				uint8 Multiplier;
			};
			constexpr std::array<FForcedMultiplier, 7> Multipliers{{{0, 10}, {1, 10}, {11, 9}, {12, 9}, {13, 8}, {14, 12}, {15, 12}}};
			for (const FForcedMultiplier& Forced : Multipliers)
			{
				BattleGameState::Byte(State, GearOffset(Forced.Gear) + Gear::BaseMultiplier) = Forced.Multiplier;
			}
			// Perfect Works' rebalance gives Weltall's two records 21.
			if (Context.bPerfectWorksRebalance)
			{
				for (const std::size_t Weltall : {std::size_t{0}, std::size_t{1}})
				{
					BattleGameState::Byte(State, GearOffset(Weltall) + Gear::BaseMultiplier) = 21;
				}
			}
			BattleGameState::Byte(State, GearOffset(13) + Gear::Frame) = 88;
			BattleGameState::Byte(State, GearOffset(7) + Gear::Engine) = 0;
			BattleGameState::Byte(State, GearOffset(15) + Gear::Armor) = 40;
		}
	}
}
