#include "Battle/BattleParty.h"

#include "Battle/BattleGameState.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleTurn.h"

namespace Weltall
{
	using namespace BattleParty;

	namespace
	{
		constexpr uint8 MaxStoredAp = 28;
		constexpr uint8 IconFlying = 3;
		constexpr uint8 IconArrived = 1;
		constexpr uint8 BoardedInBattle = 2;
		constexpr uint16 PlayingDead = 0x0002;
		enum ETurnPc : uint8
		{
			TurnStart,
			TurnIcon,
			TurnMenuOpen,
			TurnLoopTop,
			TurnAfterFrame,
			TurnAfterHandler,
			TurnEpilogue,
			TurnAfterClose,
			TurnCockpitFuel,
			TurnCockpitShown,
		};

		/** A Billy's ammunition count in the game state, by the ammunition's kind in the counts at Offset; none past the state faults. */
		uint8 AmmunitionLeft(FBattleContext& Context, std::size_t Offset, uint8 Kind)
		{
			const std::size_t At = Offset + Kind;
			if (At >= FGameState::BlockSize)
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return 0;
			}
			return BattleGameState::Byte(*Context.GameState, At);
		}

		/** A byte of a game-state record; past the state faults. */
		uint8 RecordByte(FBattleContext& Context, std::size_t Offset)
		{
			if (Offset >= FGameState::BlockSize)
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return 0;
			}
			return BattleGameState::Byte(*Context.GameState, Offset);
		}

		/** Billy's buttons whose guns are out of ammunition are disabled: on foot his pistol's and big gun's, in his gear its two guns'. */
		void DisableEmptyGuns(FBattleContext& Context, uint8 Slot, bool bInGear)
		{
			using namespace GameStateLayout;
			const FBattleProgramTables& Program = *Context.Program;
			auto& Disabled = Context.Party.Disabled[Slot];
			const std::size_t Record = CharacterOffset(Billy);
			uint8 SecondLeft = 0;
			if (!bInGear)
			{
				if (AmmunitionLeft(Context, AmmunitionCountsOffset, RecordByte(Context, Record + 0x6F)) == 0)
				{
					Disabled[ECommand::WeakButton] = Program.CommandBits[ECommand::WeakButton];
				}
				SecondLeft = AmmunitionLeft(Context, AmmunitionCountsOffset, RecordByte(Context, Record + 0x72));
			}
			else
			{
				const std::size_t Gear = GearOffset(RecordByte(Context, Record + Character::Gear));
				if (AmmunitionLeft(Context, StoryFlagsOffset, RecordByte(Context, Gear + 0x04)) == 0)
				{
					Disabled[ECommand::WeakButton] = Program.CommandBits[ECommand::WeakButton];
				}
				SecondLeft = AmmunitionLeft(Context, StoryFlagsOffset, RecordByte(Context, Gear + 0x07));
			}
			if (SecondLeft == 0)
			{
				Disabled[ECommand::FierceButton] = Program.CommandBits[ECommand::FierceButton];
			}
		}

		void StartTurn(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			Party.bActed = true;
			Party.bTargetLocked = false;
			Party.bTurnOver = false;
			Party.bMenuFading = false;
			Party.bComboClosed = false;
			Party.ShownState = 0xFF;
			Party.bStoresAp = false;
			Party.Deathblow = 0;
			Party.IdsPress = BattleMenuEvents::Nothing;
			Context.CurrentAttack = 0;
			Party.ApAtStart = Context.ApRecords[Slot].ApPerTurn;
			Party.ApLeft = Party.ApAtStart;
			Party.TurnIcons[Slot] = IconFlying;
			Party.GearHits = 0;
			Party.LastDirection = 0xFF;
			Party.bGearPressed = false;
		}

		/** The command window opens: the turn's sound, the disabled commands, the state the menu opens on; a gear's turn start. */
		void OpenMenu(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			const FBattleProgramTables& Program = *Context.Program;
			Party.bMenuSounds = true;
			PlayMenuSound(Context, ESound::TurnStart);
			Party.bMenuSounds = false;
			auto& Disabled = Party.Disabled[Slot];
			for (std::size_t Command = 0; Command < Disabled.size(); ++Command)
			{
				Disabled[Command] = Context.Combatants[Slot].DisabledCommands & Program.CommandBits[Command];
			}
			const uint8 Character = Context.PartyCharacters[Slot];
			const bool bInGear = Context.ApRecords[Slot].InGear != 0;
			if (Character == ChuChu && bInGear)
			{
				Disabled[ECommand::Booster] = Program.CommandBits[ECommand::Booster];
				Disabled[ECommand::Special] = Program.CommandBits[ECommand::Special];
			}
			if (Character == Billy)
			{
				DisableEmptyGuns(Context, Slot, bInGear);
			}
			const bool bAttack = Disabled[ECommand::Attack] == 0;
			if (!bInGear)
			{
				Party.MenuState = bAttack ? EState::Attack : EState::Ether;
				Party.PreviousDirection = bAttack ? BattleMenuEvents::Right : BattleMenuEvents::Up;
				if ((Context.Combatants[Slot].Status & PlayingDead) != 0)
				{
					Party.MenuState = EState::Item;
					Party.PreviousDirection = BattleMenuEvents::Down;
				}
				PlayMenuCue(Context, EBattleMenuCue::MakeCommandHud, Slot);
			}
			else
			{
				Party.MenuState = bAttack ? EState::GearAttack : EState::GearEther;
				Party.PreviousDirection = bAttack ? BattleMenuEvents::Right : BattleMenuEvents::Up;
				PrepareGearTurn(Context, Slot);
				PlayMenuCue(Context, EBattleMenuCue::OpenCockpit, Slot);
			}
		}

		void CloseCommandWindow(FBattleContext& Context, uint8 Slot)
		{
			PlayMenuCue(Context, EBattleMenuCue::CloseCommandHud, Slot);
			if (Context.ApRecords[Slot].InGear == BoardedInBattle)
			{
				Context.ApRecords[Slot].InGear = 1;
			}
		}

		EFlow RunTurn(FBattleContext& Context, FBattlePartyFrame& Frame)
		{
			FBattlePartyInput& Party = Context.Party;
			const uint8 Slot = Party.Actor;
			if (Slot >= FBattleContext::PartySlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return EFlow::Yield;
			}
			while (Context.Fault == EBattleFault::None)
			{
				switch (Frame.Pc)
				{
				case TurnStart:
					StartTurn(Context, Slot);
					Frame.Pc = TurnIcon;
					return EFlow::Yield;
				case TurnIcon:
					if (Party.TurnIcons[Slot] != IconArrived)
					{
						return EFlow::Yield;
					}
					OpenMenu(Context, Slot);
					Frame.Pc = TurnMenuOpen;
					// A gear's cockpit takes two frames to open: one after its instruments, one after its fuel.
					if (Context.ApRecords[Slot].InGear != 0 && Context.PartyCharacters[Slot] != ChuChu)
					{
						Frame.Pc = TurnCockpitFuel;
						return CallFrames(Context, 1);
					}
					break;
				case TurnCockpitFuel:
					PlayMenuCue(Context, EBattleMenuCue::CockpitFuel, Slot);
					Frame.Pc = TurnCockpitShown;
					return CallFrames(Context, 1);
				case TurnCockpitShown:
					PlayMenuCue(Context, EBattleMenuCue::ShowCockpit, Slot);
					Frame.Pc = TurnMenuOpen;
					break;
				case TurnMenuOpen:
					Party.Buttons.fill(0xFF);
					Party.ButtonCount = 0;
					EndBattleDefence(Context, Slot);
					FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
					SetMarkers(Context, Bit(Context, Context.SlotTargets[Slot] == 0xFF ? 0 : Context.SlotTargets[Slot]));
					Frame.Pc = Party.bTurnOver || Context.End != BattleEnd::Running || Context.bActionsPlayed ? TurnEpilogue : TurnLoopTop;
					break;
				case TurnLoopTop:
					if (Context.ApRecords[Slot].InGear == 0)
					{
						PlayMenuCue(Context, EBattleMenuCue::ApTicks, Slot);
					}
					if (Party.ShownState != Party.MenuState)
					{
						Party.bBlockedOnce = false;
						Party.ShownState = Party.MenuState;
						Party.bIconsShown = false;
						PlayMenuCue(Context, EBattleMenuCue::CommandIcons, Slot);
						Party.bIconsShown = true;
					}
					if (Party.Event == BattleMenuEvents::Over)
					{
						Frame.Pc = TurnEpilogue;
						break;
					}
					Frame.Pc = TurnAfterFrame;
					return EFlow::Yield;
				case TurnAfterFrame:
					if (Context.bActionsPlayed)
					{
						Frame.Pc = TurnEpilogue;
						break;
					}
					if (!Party.bMenuFading)
					{
						Party.bMenuSounds = true;
					}
					Frame.Pc = TurnAfterHandler;
					if (RunMenuState(Context, Slot) == EFlow::Call)
					{
						return EFlow::Call;
					}
					break;
				case TurnAfterHandler:
					Frame.Pc = Party.bTurnOver || Context.End != BattleEnd::Running || Context.bActionsPlayed ? TurnEpilogue : TurnLoopTop;
					break;
				case TurnEpilogue:
					Party.bIconsShown = false;
					PlayMenuCue(Context, EBattleMenuCue::ShowApNumbers, Slot, 0);
					Party.TurnIcons[Slot] = 0;
					if (Party.bStoresAp)
					{
						uint8& Stored = Context.ApRecords[Slot].StoredAp;
						Stored = static_cast<uint8>(Stored + Party.ApLeft);
						if (Stored > MaxStoredAp)
						{
							Stored = MaxStoredAp;
						}
					}
					Party.bMenuSounds = false;
					Frame.Pc = TurnAfterClose;
					if (Party.bTurnOver || Context.End != BattleEnd::Running)
					{
						CloseCommandWindow(Context, Slot);
						if (Context.PartyCharacters[Slot] == Billy && Party.bAmmunitionShown)
						{
							return Call(Context, EBattlePartyRoutine::CloseAmmunition);
						}
					}
					break;
				case TurnAfterClose:
					Context.Waits[Slot] = Context.FullWaits[Slot];
					Context.PanelRefresh[Slot] = 1;
					Party.bStoredCombo = false;
					return EFlow::Return;
				default:
					return EFlow::Return;
				}
			}
			return EFlow::Yield;
		}

		EFlow RunFrames(FBattlePartyFrame& Frame)
		{
			if (Frame.Count == 0)
			{
				return EFlow::Return;
			}
			--Frame.Count;
			return EFlow::Yield;
		}

		EFlow RunRoutine(FBattleContext& Context, FBattlePartyFrame& Frame)
		{
			switch (Frame.Routine)
			{
			case EBattlePartyRoutine::Turn: return RunTurn(Context, Frame);
			case EBattlePartyRoutine::AutoTurn: return RunAutoTurn(Context, Frame);
			case EBattlePartyRoutine::AttackMode: return RunAttackMode(Context, Frame);
			case EBattlePartyRoutine::GearAttackMode: return RunGearAttackMode(Context, Frame);
			case EBattlePartyRoutine::ItemMenu: return RunItemMenu(Context, Frame);
			case EBattlePartyRoutine::EtherMenu: return RunEtherMenu(Context, Frame);
			case EBattlePartyRoutine::SpecialMenu: return RunSpecialMenu(Context, Frame);
			case EBattlePartyRoutine::ComboMenu: return RunComboMenu(Context, Frame);
			case EBattlePartyRoutine::SelectTarget: return RunSelectTarget(Context, Frame);
			case EBattlePartyRoutine::PerformAbility: return RunPerformAbility(Context, Frame);
			case EBattlePartyRoutine::PerformItem: return RunPerformItem(Context, Frame);
			case EBattlePartyRoutine::OpenAmmunition: return RunOpenAmmunition(Context, Frame);
			case EBattlePartyRoutine::CloseAmmunition: return RunCloseAmmunition(Context, Frame);
			case EBattlePartyRoutine::StartAttack: return RunStartAttack(Context, Frame);
			case EBattlePartyRoutine::Board: return RunBoard(Context, Frame);
			case EBattlePartyRoutine::Frames: return RunFrames(Frame);
			case EBattlePartyRoutine::None: break;
			}
			return EFlow::Return;
		}

		void Resume(FBattleContext& Context)
		{
			FBattlePartyInput& Party = Context.Party;
			while (Party.Depth > 0 && Context.Fault == EBattleFault::None)
			{
				const EFlow Flow = RunRoutine(Context, Party.Routines[Party.Depth - 1]);
				if (Flow == EFlow::Yield)
				{
					return;
				}
				if (Flow == EFlow::Return)
				{
					--Party.Depth;
				}
			}
		}

		void Begin(FBattleContext& Context, uint8 Slot, EBattlePartyRoutine Routine)
		{
			Context.Party.Actor = Slot;
			Context.Party.Depth = 0;
			Call(Context, Routine);
			Resume(Context);
		}
	}

	namespace BattleParty
	{
		FBattlePartyFrame& Current(FBattleContext& Context)
		{
			return Context.Party.Routines[Context.Party.Depth - 1];
		}

		EFlow Call(FBattleContext& Context, EBattlePartyRoutine Routine)
		{
			FBattlePartyInput& Party = Context.Party;
			if (Party.Depth >= Party.Routines.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return EFlow::Yield;
			}
			Party.Routines[Party.Depth] = {Routine, 0, 0};
			++Party.Depth;
			return EFlow::Call;
		}

		EFlow CallFrames(FBattleContext& Context, uint8 Count)
		{
			const EFlow Flow = Call(Context, EBattlePartyRoutine::Frames);
			if (Flow == EFlow::Call)
			{
				Current(Context).Count = Count;
			}
			return Flow;
		}

		EFlow ReturnValue(FBattleContext& Context, uint8 Value)
		{
			Context.Party.Returned = Value;
			return EFlow::Return;
		}

		bool ShowBlocks(const FBattleContext& Context)
		{
			return Context.Show != nullptr && Context.Show->IsBlocking();
		}

		void PlaySound(FBattleContext& Context, uint8 Effect)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlaySound(Effect);
			}
		}

		void PlayMenuSound(FBattleContext& Context, uint8 Effect)
		{
			if (Context.Party.bMenuSounds)
			{
				PlaySound(Context, Effect);
			}
		}

		void FrameCamera(FBattleContext& Context, uint16 Slots)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->FrameCamera(Slots);
			}
		}

		void SetMarkers(FBattleContext& Context, uint16 Slots)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->SetTargetMarkers(Slots);
			}
		}

		void PlayMenuCue(FBattleContext& Context, EBattleMenuCue Cue, uint8 Slot, uint8 Arg)
		{
			if (Context.Show != nullptr)
			{
				Context.Show->PlayMenuCue(Cue, Slot, Arg);
			}
		}

		uint16 Bit(FBattleContext& Context, uint8 Slot)
		{
			return BattleRules::GameSlotBit(Context, Slot);
		}

		bool IsDisabled(const FBattleContext& Context, uint8 Slot, int32 Command)
		{
			return Context.Party.Disabled[Slot][static_cast<std::size_t>(Command)] != 0;
		}

		FBattlePerformance* CurrentRecord(FBattleContext& Context)
		{
			if (Context.PerformanceIndex >= FBattleContext::PerformanceCount)
			{
				Context.Fault = EBattleFault::PerformanceOutsideRecords;
				return nullptr;
			}
			return &Context.Performances[Context.PerformanceIndex];
		}

		FBattlePerformance* QueueCode(FBattleContext& Context, uint8 Actor, uint8 Code, uint16 Parameter)
		{
			FBattlePerformance* Record = CurrentRecord(Context);
			if (Record != nullptr)
			{
				Record->Actor = Actor;
				Record->Code = Code;
				Record->Parameter = Parameter;
				++Context.PerformanceIndex;
			}
			return Record;
		}

		void QueueEnd(FBattleContext& Context, uint8 Actor)
		{
			if (FBattlePerformance* Record = CurrentRecord(Context))
			{
				Record->Code = 0xFE;
				Record->Actor = Actor;
			}
			Context.Party.bMenuSounds = false;
		}

		void ResetQueue(FBattleContext& Context, uint8 Actor, uint8 Target)
		{
			for (FBattlePerformance& Record : Context.Performances)
			{
				Record.Code = 0xFF;
				Record.IdsAnimation = 0xFF;
				Record.Actor = Actor;
				Record.Targets = Bit(Context, Target);
			}
		}

		uint16 LearnedWord(FBattleContext& Context, uint8 Character, std::size_t Field)
		{
			const std::size_t Offset = GameStateLayout::LearnedSkillsOffsetOf(Character) + Field;
			if (Offset + 2 > FGameState::BlockSize)
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return 0;
			}
			return BattleGameState::Word(*Context.GameState, Offset);
		}
	}

	void BeginBattlePartyTurn(FBattleContext& Context, uint8 Slot)
	{
		Begin(Context, Slot, EBattlePartyRoutine::Turn);
	}

	void BeginBattleAutoTurn(FBattleContext& Context, uint8 Slot)
	{
		Begin(Context, Slot, EBattlePartyRoutine::AutoTurn);
	}

	void ScrollBattleList(FBattleContext& Context)
	{
		FBattlePartyInput& Party = Context.Party;
		switch (Party.ListScrollWay)
		{
		case 1:
		case 3:
			++Party.ListScroll;
			break;
		case 2:
		case 4:
			--Party.ListScroll;
			break;
		default:
			break;
		}
	}

	bool ResumeBattlePartyTurn(FBattleContext& Context, uint8 Event)
	{
		FBattlePartyInput& Party = Context.Party;
		Party.Event = Event;
		Resume(Context);
		return Party.Depth > 0 && Context.Fault == EBattleFault::None;
	}

	bool StepBattlePartyTurn(FBattleContext& Context, uint8 Event)
	{
		ScrollBattleList(Context);
		return ResumeBattlePartyTurn(Context, Event);
	}

	uint8 TakeBattleMenuPress(FBattleContext& Context, uint8 Press)
	{
		if (Press == BattleMenuEvents::Nothing)
		{
			return Press;
		}
		if (Context.End != BattleEnd::Running || Context.bActionsPlayed)
		{
			return BattleMenuEvents::Over;
		}
		FBattlePartyInput& Party = Context.Party;
		switch (Press)
		{
		case BattleMenuEvents::Right:
		case BattleMenuEvents::Down:
		case BattleMenuEvents::Left:
		case BattleMenuEvents::Up:
			PlayMenuSound(Context, ESound::Move);
			Party.PreviousDirection = Party.LastDirection;
			Party.LastDirection = Press;
			break;
		case BattleMenuEvents::Confirm:
		case BattleMenuEvents::Weak:
		case BattleMenuEvents::Strong:
			PlayMenuSound(Context, ESound::Accept);
			break;
		case BattleMenuEvents::Cancel:
			PlayMenuSound(Context, ESound::Back);
			break;
		default:
			break;
		}
		return Press;
	}

	void FinishBattleActionPlayback(FBattleContext& Context)
	{
		if (EndBattleActionPlayback(Context))
		{
			CloseBattleAmmunitionWindow(Context);
		}
	}

	bool EndBattleActionPlayback(FBattleContext& Context)
	{
		Context.bActionsPlayed = true;
		const uint8 Actor = Context.TurnActor;
		if (Actor >= FBattleContext::PartySlotCount || Context.Party.bAutoTurn)
		{
			return false;
		}
		CloseCommandWindow(Context, Actor);
		if (Context.PartyCharacters[Actor] != Billy || !Context.Party.bAmmunitionShown)
		{
			return false;
		}
		PlayMenuCue(Context, EBattleMenuCue::CloseListWindow, Actor, 0);
		return true;
	}

	void CloseBattleAmmunitionWindow(FBattleContext& Context)
	{
		if (Context.Party.bAmmunitionShown)
		{
			PlayMenuCue(Context, EBattleMenuCue::FreeListWork, Context.Party.Actor);
		}
		Context.Party.bAmmunitionShown = false;
	}
}
