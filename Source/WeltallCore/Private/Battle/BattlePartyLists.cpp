#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleItems.h"
#include "Battle/BattleParty.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleRules.h"
#include "Game/GameStateLayout.h"

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 Open = 2;
		constexpr uint8 Chosen = 1;
		constexpr uint8 Left = 0;
		constexpr uint8 EtherWindow = 1;
		constexpr uint8 ItemWindow = 2;
		constexpr uint8 SpecialWindow = 4;
		constexpr uint8 ReactionCode = 0xF3;
		constexpr uint8 ItemCode = 0xF5;
		constexpr uint16 ShowsReactions = 0x0800;
		constexpr uint16 NoEffectInGear = 0x8000;
		/** The gear Special list's first command, and a gear Special's number as the turn keeps it. */
		constexpr uint8 FirstSpecial = 37;
		constexpr uint8 SpecialChoice = 0x10;
		constexpr uint8 FirstAbility = 22;
		constexpr uint8 FirstGearAbility = 21;
		/** The windows' frames: a list opens a window a frame (the item and Ether lists build in one more); it closes its work and each window a frame. */
		constexpr uint8 ItemBuildFrames = 1;
		/** The item list opens two windows, one a frame, and closes them after its work; the Ether list four, the Special list three. */
		constexpr uint8 ItemWindows = 2;
		constexpr uint8 EtherWindows = 4;
		constexpr uint8 SpecialWindows = 3;
		/** The item list's rows are 13 pixels high, the Ether list's 16. */
		constexpr int32 ItemRow = 13;
		constexpr int32 EtherRow = 16;

		enum EListPc : uint8
		{
			ListStart,
			ListOpening,
			ListBuilt,
			ListTop,
			ListFrame,
			ListAfterChoose,
			ListSettle,
			ListClosing,
			ListClosed,
			ListDone,
		};

		enum EPerformPc : uint8
		{
			PerformStart,
			PerformStarted,
			PerformWait,
		};

		bool HighBit(uint16 Word, uint8 Bit)
		{
			return Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		const FBattleCommand& CommandAt(FBattleContext& Context, int32 Index)
		{
			return BattleRules::CommandAt(Context, Index);
		}

		/** The Ether list's entry n: the ability's command on foot, the gear's ether command in a gear. */
		const FBattleCommand& EtherCommand(FBattleContext& Context, uint8 Slot, uint8 Entry)
		{
			if (Context.ApRecords[Slot].InGear == 0)
			{
				return CommandAt(Context, FBattleContext::PartyCommandIndex(Slot, Entry + FirstAbility));
			}
			return CommandAt(Context, FBattleContext::GearCommandIndex(Slot, Entry + FirstGearAbility));
		}

		bool EtherLearned(FBattleContext& Context, uint8 Slot, uint8 Entry)
		{
			const std::size_t Field = Context.ApRecords[Slot].InGear == 0 ? GameStateLayout::Skills::Abilities : GameStateLayout::Skills::GearAbilities;
			return HighBit(LearnedWord(Context, Context.PartyCharacters[Slot], Field), Entry);
		}

		/** The list's windows back after the target selector was left: the camera and markers on the actor and its target. */
		void ShowListWindows(FBattleContext& Context, uint8 Slot, uint8 Window)
		{
			PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
			const EBattleMenuList List = Window == ItemWindow ? EBattleMenuList::Items : Window == EtherWindow ? EBattleMenuList::Ether : EBattleMenuList::Special;
			PlayMenuCue(Context, EBattleMenuCue::ShowListWindows, Slot, static_cast<uint8>(List));
			Context.Party.ListWindow = Window;
			FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
			SetMarkers(Context, Bit(Context, Context.SlotTargets[Slot]));
		}

		/** A list's windows open one a frame (the game waits a frame after each); then the list goes on at Next. */
		EFlow OpenListWindows(FBattleContext& Context, FBattlePartyFrame& Frame, EBattleMenuList List, uint8 Windows, uint8 Next)
		{
			if (Frame.Count < Windows)
			{
				PlayMenuCue(Context, EBattleMenuCue::OpenListWindow, Context.Party.Actor, static_cast<uint8>(static_cast<uint8>(List) * 16 + Frame.Count));
				++Frame.Count;
				return CallFrames(Context, 1);
			}
			Frame.Count = 0;
			Frame.Pc = Next;
			return EFlow::Return;
		}

		/**
		 * A list closes: it hides (the command icons back when it was left); then a frame for its work, which is freed with the
		 * arrows hidden, and a frame for each of its windows as each closes.
		 */
		EFlow CloseDrawnList(FBattleContext& Context, FBattlePartyFrame& Frame, EBattleMenuList List)
		{
			FBattlePartyInput& Party = Context.Party;
			Party.ListWindow = 0;
			if (Party.List.Result == Left)
			{
				Party.bIconsShown = true;
			}
			PlayMenuCue(Context, EBattleMenuCue::HideListWindows, Party.Actor, static_cast<uint8>(List));
			Frame.Pc = ListClosing;
			Frame.Count = 0;
			return CallFrames(Context, 1);
		}

		/** The next step of a drawn list's close: the work freed and the first window closed, then each other window; Return once all have. */
		EFlow CloseListWindows(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Windows)
		{
			const uint8 Slot = Context.Party.Actor;
			if (Frame.Count == 0)
			{
				PlayMenuCue(Context, EBattleMenuCue::FreeListWork, Slot);
				PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
			}
			if (Frame.Count < Windows)
			{
				PlayMenuCue(Context, EBattleMenuCue::CloseListWindow, Slot, Frame.Count);
				++Frame.Count;
				return CallFrames(Context, 1);
			}
			Frame.Count = 0;
			Frame.Pc = ListClosed;
			return EFlow::Return;
		}

		/** A list's move by the frame's event over Cells cells shown (two to a row) and Entries entries in all; a move off the cells shown asks for a scroll. */
		void MoveInList(FBattlePartyInput& Party, uint8 Cells, uint8 Entries)
		{
			FBattleListMenu& List = Party.List;
			const uint32 Entry = List.TopRow * 2u + List.Cell;
			switch (Party.Event)
			{
			case BattleMenuEvents::Right:
				if (Entry != Entries - 1u)
				{
					if (List.Cell == Cells - 1)
					{
						Party.ListScrollWay = 1;
					}
					else
					{
						++List.Cell;
					}
				}
				break;
			case BattleMenuEvents::Down:
				if (Entry < Entries - 2u)
				{
					if (List.Cell + 2 < Cells)
					{
						List.Cell = static_cast<uint8>(List.Cell + 2);
					}
					else
					{
						Party.ListScrollWay = 3;
					}
				}
				break;
			case BattleMenuEvents::Left:
				if (Entry != 0)
				{
					if (List.Cell == 0 && List.TopRow != 0)
					{
						Party.ListScrollWay = 2;
					}
					else
					{
						--List.Cell;
					}
				}
				break;
			case BattleMenuEvents::Up:
				if (Entry > 1)
				{
					if (List.Cell < 2 && List.TopRow != 0)
					{
						Party.ListScrollWay = 4;
					}
					else
					{
						List.Cell = static_cast<uint8>(List.Cell - 2);
					}
				}
				break;
			case BattleMenuEvents::Cancel:
				List.Result = Left;
				break;
			default:
				break;
			}
		}

		/** The Ether list's scroll done once the input has moved it a whole row: the rows and the cursor step. */
		void SettleEtherScroll(FBattlePartyInput& Party)
		{
			FBattleListMenu& List = Party.List;
			switch (Party.ListScrollWay)
			{
			case 1:
			case 3:
			{
				const int32 Row = (List.TopRow + 1) * EtherRow;
				if (Row <= Party.ListScroll)
				{
					if (Party.ListScrollWay == 1)
					{
						--List.Cell;
					}
					++List.TopRow;
					Party.ListScrollWay = 0;
					Party.ListScroll = Row;
				}
				break;
			}
			case 2:
			case 4:
			{
				const int32 Row = (List.TopRow - 1) * EtherRow;
				if (Party.ListScroll < Row)
				{
					if (Party.ListScrollWay == 2)
					{
						++List.Cell;
					}
					--List.TopRow;
					Party.ListScrollWay = 0;
					Party.ListScroll = Row;
				}
				break;
			}
			default:
				break;
			}
		}

		/** The item list scrolls at once. */
		void SettleItemScroll(FBattlePartyInput& Party)
		{
			FBattleListMenu& List = Party.List;
			switch (Party.ListScrollWay)
			{
			case 1:
				--List.Cell;
				[[fallthrough]];
			case 3:
				Party.ListScroll = (List.TopRow + 1) * ItemRow;
				++List.TopRow;
				Party.ListScrollWay = 0;
				break;
			case 2:
				++List.Cell;
				[[fallthrough]];
			case 4:
				Party.ListScroll = (List.TopRow - 1) * ItemRow;
				--List.TopRow;
				Party.ListScrollWay = 0;
				break;
			default:
				break;
			}
		}

		/** Only the enemies that show their reactions keep their bit; the rest are queued as one reaction record. */
		void QueueTargetReactions(FBattleContext& Context, uint16 Targets, uint8 Actor)
		{
			uint16 Reacting = static_cast<uint16>(Targets & 0xFFF8);
			for (uint8 Enemy = FBattleContext::PartySlotCount; Enemy < FBattleContext::SlotCount; ++Enemy)
			{
				if ((Reacting & Bit(Context, Enemy)) != 0 && (Context.Combatants[Enemy].Traits & ShowsReactions) == 0)
				{
					Reacting = static_cast<uint16>(Reacting & ~Bit(Context, Enemy));
				}
			}
			if (Reacting != 0)
			{
				QueueCode(Context, Actor, ReactionCode, Reacting);
			}
		}

		/** The targets a list command hits: the selector's candidates, or its one target for a single-target word. */
		uint16 ChosenTargets(FBattleContext& Context, uint16 Word)
		{
			return (Word & 7) == 0 ? Bit(Context, Context.Targeting.Target) : Context.Targeting.Candidates;
		}

		void RecordHits(FBattleContext& Context, uint8 Actor)
		{
			for (uint8 Enemy = FBattleContext::PartySlotCount; Enemy < FBattleContext::SlotCount; ++Enemy)
			{
				if ((Context.Action.Targets & Bit(Context, Enemy)) != 0)
				{
					RecordBattleHit(Context, Actor, Enemy);
				}
			}
		}

		void EmptyCodes(FBattleContext& Context)
		{
			for (FBattlePerformance& Record : Context.Performances)
			{
				Record.Code = 0xFF;
			}
		}
	}

	EFlow RunEtherMenu(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		FBattleListMenu& List = Party.List;
		const uint8 Slot = Party.Actor;
		for (;;)
		{
			switch (Frame.Pc)
			{
			case ListStart:
				List.Cell = 0;
				List.TopRow = 0;
				List.Result = Open;
				Party.ListScroll = 0;
				Party.ListScrollWay = 0;
				Party.bIconsShown = false;
				Frame.Pc = ListOpening;
				Frame.Count = 0;
				break;
			case ListOpening:
				if (OpenListWindows(Context, Frame, EBattleMenuList::Ether, EtherWindows, ListBuilt) == EFlow::Call)
				{
					return EFlow::Call;
				}
				PlayMenuCue(Context, EBattleMenuCue::BuildEtherList, Slot);
				return CallFrames(Context, 1);
			case ListBuilt:
				PlayMenuCue(Context, EBattleMenuCue::ShowEtherCells, Slot);
				Party.ListWindow = EtherWindow;
				PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
				Frame.Pc = ListTop;
				break;
			case ListTop:
				PlayMenuCue(Context, EBattleMenuCue::EtherListFrame, Slot);
				Frame.Pc = ListFrame;
				return EFlow::Yield;
			case ListFrame:
				if (Party.Event == BattleMenuEvents::Confirm)
				{
					const uint8 Entry = static_cast<uint8>(List.Cell + List.TopRow * 2);
					const FBattleCommand& Command = EtherCommand(Context, Slot, Entry);
					if (Command.EpCost <= Context.Combatants[Slot].Ep && EtherLearned(Context, Slot, Entry))
					{
						Party.ListWindow = 0;
						PlayMenuCue(Context, EBattleMenuCue::HideListWindows, Slot, static_cast<uint8>(EBattleMenuList::Ether));
						PlayMenuCue(Context, EBattleMenuCue::ShowArrows, Slot);
						Frame.Pc = ListAfterChoose;
						return CallSelectTarget(Context, Command.Targeting, Slot, false);
					}
					PlayMenuSound(Context, ESound::Refuse);
				}
				else
				{
					MoveInList(Party, 12, 16);
				}
				Frame.Pc = ListSettle;
				break;
			case ListAfterChoose:
			{
				const uint8 Entry = static_cast<uint8>(List.Cell + List.TopRow * 2);
				if (Party.Returned == 0)
				{
					ShowListWindows(Context, Slot, EtherWindow);
				}
				else
				{
					FBattleCombatant& Member = Context.Combatants[Slot];
					Member.Ep = static_cast<uint16>(Member.Ep - EtherCommand(Context, Slot, Entry).EpCost);
					List.Result = Chosen;
					Party.Choice = Entry;
				}
				[[fallthrough]];
			}
			case ListSettle:
				SettleEtherScroll(Party);
				if (List.Result != Open)
				{
					return CloseDrawnList(Context, Frame, EBattleMenuList::Ether);
				}
				Frame.Pc = ListTop;
				break;
			case ListClosing:
				if (CloseListWindows(Context, Frame, EtherWindows) == EFlow::Call)
				{
					return EFlow::Call;
				}
				break;
			case ListClosed:
				if (List.Result == Left)
				{
					return EFlow::Return;
				}
				Frame.Pc = ListDone;
				return Call(Context, EBattlePartyRoutine::PerformAbility);
			default:
				return EFlow::Return;
			}
		}
	}

	EFlow RunItemMenu(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		FBattleListMenu& List = Party.List;
		const uint8 Slot = Party.Actor;
		for (;;)
		{
			switch (Frame.Pc)
			{
			case ListStart:
				List.Cell = 0;
				List.TopRow = 0;
				List.Result = Open;
				List.Build = 1;
				List.Page = 0;
				List.bFirstBuild = true;
				Party.ListScroll = 0;
				Party.ListScrollWay = 0;
				Party.bIconsShown = false;
				Frame.Pc = ListOpening;
				Frame.Count = 0;
				break;
			case ListOpening:
				if (OpenListWindows(Context, Frame, EBattleMenuList::Items, ItemWindows, ListTop) == EFlow::Call)
				{
					return EFlow::Call;
				}
				PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
				break;
			case ListTop:
				if (List.Build != 0)
				{
					PlayMenuCue(Context, EBattleMenuCue::BuildItemList, Slot, List.bFirstBuild ? 1 : 0);
					Frame.Pc = ListBuilt;
					return CallFrames(Context, ItemBuildFrames);
				}
				PlayMenuCue(Context, EBattleMenuCue::ItemListFrame, Slot);
				Frame.Pc = ListFrame;
				return EFlow::Yield;
			case ListBuilt:
				PlayMenuCue(Context, EBattleMenuCue::ShowListCells, Slot);
				Party.ListWindow = ItemWindow;
				List.Page = static_cast<uint8>(List.Build - 1);
				List.Build = 0;
				List.bFirstBuild = false;
				PlayMenuCue(Context, EBattleMenuCue::ItemListFrame, Slot);
				Frame.Pc = ListFrame;
				return EFlow::Yield;
			case ListFrame:
				if (Party.Event == BattleMenuEvents::Confirm)
				{
					const uint8 Item = Context.ListedItems[List.TopRow * 2u + List.Cell];
					if (Item >= Context.Items.size())
					{
						Context.Fault = EBattleFault::CommandOutsideTable;
						return EFlow::Yield;
					}
					if (Item != 0)
					{
						Party.ListWindow = 0;
						PlayMenuCue(Context, EBattleMenuCue::HideListWindows, Slot, static_cast<uint8>(EBattleMenuList::Items));
						PlayMenuCue(Context, EBattleMenuCue::ShowArrows, Slot);
						Frame.Pc = ListAfterChoose;
						return CallSelectTarget(Context, Context.Items[Item].Targeting, Slot, false);
					}
					PlayMenuSound(Context, ESound::Refuse);
				}
				else
				{
					MoveInList(Party, 16, 48);
				}
				Frame.Pc = ListSettle;
				break;
			case ListAfterChoose:
				if (Party.Returned == 0)
				{
					ShowListWindows(Context, Slot, ItemWindow);
				}
				else
				{
					Party.Choice = Context.ListedItems[List.TopRow * 2u + List.Cell];
					List.Result = Chosen;
					Party.ChosenRow = List.TopRow;
					Party.ChosenCell = List.Cell;
				}
				[[fallthrough]];
			case ListSettle:
				SettleItemScroll(Party);
				if (List.Result != Open)
				{
					return CloseDrawnList(Context, Frame, EBattleMenuList::Items);
				}
				Frame.Pc = ListTop;
				break;
			case ListClosing:
				if (CloseListWindows(Context, Frame, ItemWindows) == EFlow::Call)
				{
					return EFlow::Call;
				}
				break;
			case ListClosed:
				if (List.Result == Left)
				{
					return EFlow::Return;
				}
				Frame.Pc = ListDone;
				return Call(Context, EBattlePartyRoutine::PerformItem);
			default:
				return EFlow::Return;
			}
		}
	}

	EFlow RunSpecialMenu(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		FBattleListMenu& List = Party.List;
		const uint8 Slot = Party.Actor;
		const auto& Bits = Context.Program->CommandBits;
		const std::array<uint16, 4> Switches{Bits[ECommand::Booster], Bits[ECommand::SpecialSecond], Bits[ECommand::SpecialThird],
			Bits[ECommand::SpecialFourth]};
		for (;;)
		{
			switch (Frame.Pc)
			{
			case ListStart:
				List.Cell = 0;
				List.Result = Open;
				Party.bIconsShown = false;
				Frame.Pc = ListOpening;
				Frame.Count = 0;
				break;
			case ListOpening:
				if (OpenListWindows(Context, Frame, EBattleMenuList::Special, SpecialWindows, ListTop) == EFlow::Call)
				{
					return EFlow::Call;
				}
				PlayMenuCue(Context, EBattleMenuCue::BuildSpecialList, Slot);
				Party.ListWindow = SpecialWindow;
				PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
				break;
			case ListTop:
				PlayMenuCue(Context, EBattleMenuCue::SpecialListFrame, Slot);
				Frame.Pc = ListFrame;
				return EFlow::Yield;
			case ListFrame:
				switch (Party.Event)
				{
				case BattleMenuEvents::Up:
					List.Cell = List.Cell == 0 ? 0 : static_cast<uint8>(List.Cell - 1);
					break;
				case BattleMenuEvents::Down:
					List.Cell = List.Cell >= 3 ? 3 : static_cast<uint8>(List.Cell + 1);
					break;
				case BattleMenuEvents::Confirm:
				{
					const FBattleCommand& Command = CommandAt(Context, FBattleContext::GearCommandIndex(Slot, List.Cell + FirstSpecial));
					const FBattleCombatant& Member = Context.Combatants[Slot];
					if (Command.FuelCost <= Member.GearPart.Fuel
						&& (HighBit(LearnedWord(Context, Context.PartyCharacters[Slot], GameStateLayout::Skills::Special), List.Cell)
							|| Context.IsTrueId(Slot, List.Cell))
						&& (Member.DisabledCommands & Switches[List.Cell]) == 0)
					{
						Party.ListWindow = 0;
						PlayMenuCue(Context, EBattleMenuCue::HideListWindows, Slot, static_cast<uint8>(EBattleMenuList::Special));
						PlayMenuCue(Context, EBattleMenuCue::ShowArrows, Slot);
						Frame.Pc = ListAfterChoose;
						return CallSelectTarget(Context, Command.Targeting, Slot, false);
					}
					PlayMenuSound(Context, ESound::Refuse);
					break;
				}
				case BattleMenuEvents::Cancel:
					List.Result = Left;
					break;
				default:
					break;
				}
				Frame.Pc = ListSettle;
				break;
			case ListAfterChoose:
				if (Party.Returned == 0)
				{
					ShowListWindows(Context, Slot, SpecialWindow);
				}
				else
				{
					FBattleGearPart& Gear = Context.Combatants[Slot].GearPart;
					Gear.Fuel = static_cast<uint16>(Gear.Fuel - CommandAt(Context, FBattleContext::GearCommandIndex(Slot, List.Cell + FirstSpecial)).FuelCost);
					List.Result = Chosen;
					Party.Choice = static_cast<uint8>(List.Cell + SpecialChoice);
				}
				[[fallthrough]];
			case ListSettle:
				if (List.Result != Open)
				{
					return CloseDrawnList(Context, Frame, EBattleMenuList::Special);
				}
				Frame.Pc = ListTop;
				break;
			case ListClosing:
				if (CloseListWindows(Context, Frame, SpecialWindows) == EFlow::Call)
				{
					return EFlow::Call;
				}
				break;
			case ListClosed:
				if (List.Result == Left)
				{
					return EFlow::Return;
				}
				Frame.Pc = ListDone;
				return Call(Context, EBattlePartyRoutine::PerformAbility);
			default:
				return EFlow::Return;
			}
		}
	}

	EFlow RunPerformAbility(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		if (Frame.Pc == PerformStart)
		{
			Party.bMenuSounds = false;
			SetMarkers(Context, 0);
			EmptyCodes(Context);
			const bool bOnFoot = Context.ApRecords[Slot].InGear == 0;
			Context.CurrentAttack = static_cast<uint8>(Party.Choice + (bOnFoot ? FirstAbility + 1 : FirstGearAbility + 1));
			const int32 Index = bOnFoot ? FBattleContext::PartyCommandIndex(Slot, Party.Choice + FirstAbility)
										: FBattleContext::GearCommandIndex(Slot, Party.Choice + FirstGearAbility);
			const uint16 Targets = ChosenTargets(Context, CommandAt(Context, Index).Targeting);
			QueueTargetReactions(Context, Targets, Slot);
			if (Context.Fault != EBattleFault::None || CurrentRecord(Context) == nullptr)
			{
				return EFlow::Yield;
			}
			ClearBattlePerformance(Context);
			const uint8 Command = static_cast<uint8>(Context.CurrentAttack - 1);
			PerformBattleCommand(Context, Slot, Command, Targets, Command);
			if (Context.Fault != EBattleFault::None)
			{
				return EFlow::Yield;
			}
			CommitBattleResults(Context, Context.PerformanceIndex);
			FrameCamera(Context, static_cast<uint16>(Context.Action.Targets | Bit(Context, Slot)));
			if (Context.Show != nullptr)
			{
				Context.Show->BeginPlayback(true, Slot, Context.SlotTargets[Slot], FindNextReadySlot(Context, Slot));
			}
			Frame.Pc = PerformStarted;
		}
		if (Frame.Pc == PerformStarted)
		{
			if (ShowBlocks(Context))
			{
				return EFlow::Yield;
			}
			FBattlePerformance& Record = Context.Performances[Context.PerformanceIndex];
			Record.Code = static_cast<uint8>(Context.Action.Animation);
			Record.Actor = Slot;
			Record.Targets = Context.Action.Targets;
			RecordHits(Context, Slot);
			++Context.PerformanceIndex;
			QueueEnd(Context, Slot);
			Frame.Pc = PerformWait;
		}
		return Context.bActionsPlayed ? EFlow::Return : EFlow::Yield;
	}

	EFlow RunPerformItem(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		if (Frame.Pc == PerformStart)
		{
			Party.bMenuSounds = false;
			SetMarkers(Context, 0);
			EmptyCodes(Context);
			if (Context.Show != nullptr)
			{
				Context.Show->BeginPlayback(true, Slot, Context.SlotTargets[Slot], FindNextReadySlot(Context, Slot));
			}
			Frame.Pc = PerformStarted;
		}
		if (Frame.Pc == PerformStarted)
		{
			if (ShowBlocks(Context))
			{
				return EFlow::Yield;
			}
			const uint16 Targets = ChosenTargets(Context, Context.Items[Party.Choice].Targeting);
			Party.UsedTargets = Targets;
			QueueTargetReactions(Context, Targets, Slot);
			if (Context.Fault != EBattleFault::None || CurrentRecord(Context) == nullptr)
			{
				return EFlow::Yield;
			}
			ClearBattlePerformance(Context);
			PerformBattleItem(Context, Slot, Targets, Party.Choice);
			if (Context.Fault != EBattleFault::None)
			{
				return EFlow::Yield;
			}
			if ((Context.Action.Revived & NoEffectInGear) == 0)
			{
				const std::size_t Cell = Party.ChosenRow * 2u + Party.ChosenCell;
				if (Cell < Context.ListedCounts.size())
				{
					Context.ListedCounts[Cell] = static_cast<uint8>(Context.ListedCounts[Cell] - 1);
					if (Context.ListedCounts[Cell] == 0)
					{
						Context.ListedItems[Cell] = 0;
					}
				}
			}
			Context.bItemHealsPilot = true;
			CommitBattleResults(Context, Context.PerformanceIndex);
			Context.bItemHealsPilot = false;
			FBattlePerformance& Record = Context.Performances[Context.PerformanceIndex];
			Record.Code = ItemCode;
			Record.Parameter = Context.Action.Animation;
			Record.Actor = Slot;
			Record.Targets = Context.Action.Targets;
			RecordHits(Context, Slot);
			++Context.PerformanceIndex;
			QueueEnd(Context, Slot);
			Frame.Pc = PerformWait;
		}
		if (!Context.bActionsPlayed)
		{
			return EFlow::Yield;
		}
		for (uint8 Member = 0; Member < FBattleContext::PartySlotCount; ++Member)
		{
			if ((Party.UsedTargets & Bit(Context, Member)) != 0)
			{
				Context.PanelRefresh[Member] = 1;
			}
		}
		return EFlow::Return;
	}
}
