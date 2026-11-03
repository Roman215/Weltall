#include "Battle/BattleAction.h"
#include "Battle/BattleAi.h"
#include "Battle/BattleParty.h"
#include "Battle/BattlePartySteps.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleSteps.h"
#include "Game/CharacterLooks.h"
#include "Game/GameStateLayout.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall::BattleParty
{
	namespace
	{
		constexpr uint8 ReactionCode = 0xF3;
		constexpr uint8 ApproachCode = 0xFD;
		constexpr uint16 ShowsReactions = 0x0800;
		/** The AP bar's full height in its table's units, as a percent. */
		constexpr uint32 ApBarFull = 0x38;
		/** The Fierce basic attack's command + 1, which a Deathblow no longer learned falls back to. */
		constexpr uint8 FierceAttack = 7;
		constexpr uint8 FirstDeathblowAttack = 8;
		constexpr uint8 NoPattern = FBattleProgramTables::PatternCount;
		constexpr uint8 AmmunitionWindow = 5;
		/** Id's two quick combos and his big one, his own sprite's animations. */
		constexpr uint8 IdsQuickCombo = 0x1D;
		constexpr uint8 IdsSecondQuickCombo = 0x1E;
		constexpr uint8 IdsBigCombo = 0x1F;
		/** The turn's AP an Id press is worked out for: Fei's AP per turn, 3 to 7. */
		constexpr uint8 IdsLeastAp = 3;
		constexpr uint8 IdsMostAp = 7;

		enum EAttackPc : uint8
		{
			AttackStart,
			AttackShown,
			AttackAfterCancel,
			AttackAfterButton,
		};

		/** Bit n of a learned-skills word, as the game's table of the high bits gives it (none from 16). */
		bool HighBit(uint16 Word, uint8 Bit)
		{
			return Bit < 16 && (Word & (0x8000u >> Bit)) != 0;
		}

		/** The pattern the buttons pressed so far make (NoPattern for none). */
		uint8 MatchPattern(const FBattleContext& Context)
		{
			const auto& Patterns = Context.Program->Patterns;
			for (uint8 Pattern = 0; Pattern < Patterns.size(); ++Pattern)
			{
				if (Patterns[Pattern] == Context.Party.Buttons)
				{
					return Pattern;
				}
			}
			return NoPattern;
		}

		/** The Deathblow pattern Pattern completes for the slot's character; a character past the lists faults. */
		uint8 PatternDeathblow(FBattleContext& Context, uint8 Slot, uint8 Pattern)
		{
			const uint8 Character = Context.Combatants[Slot].Character;
			if (Character >= FBattleProgramTables::DeathblowListCount)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return Context.Program->Deathblows[Character][Pattern];
		}

		uint16 LearnedDeathblows(FBattleContext& Context, uint8 Slot)
		{
			return LearnedWord(Context, Context.PartyCharacters[Slot], GameStateLayout::Skills::Deathblows);
		}

		/** A Fierce press finishes a learned Deathblow's pattern. */
		bool IsDeathblowFinisher(FBattleContext& Context, uint8 Event, uint8 Slot)
		{
			if (Context.Party.ButtonCount == 0 || Event != BattleMenuEvents::Confirm)
			{
				return false;
			}
			const uint8 Pattern = MatchPattern(Context);
			return Pattern != NoPattern && HighBit(LearnedDeathblows(Context, Slot), PatternDeathblow(Context, Slot, Pattern));
		}

		/** The press joins the turn's buttons; when it makes a Deathblow, the attack becomes that Deathblow. */
		void RecordButton(FBattleContext& Context, uint8 Event, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			if (Party.ButtonCount >= Party.Buttons.size())
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return;
			}
			const uint8 Button = static_cast<uint8>(Event - BattleMenuEvents::Confirm);
			Party.Buttons[Party.ButtonCount] = Button;
			++Party.ButtonCount;
			if (Party.Deathblow == 0)
			{
				return;
			}
			Party.Buttons[Party.ButtonCount - 1] = 0xFF;
			const uint8 Pattern = MatchPattern(Context);
			Party.Buttons[Party.ButtonCount - 1] = Button;
			if (Pattern == NoPattern)
			{
				return;
			}
			const uint8 Deathblow = PatternDeathblow(Context, Slot, Pattern);
			if (HighBit(LearnedDeathblows(Context, Slot), Deathblow) && !IsDisabled(Context, Slot, ECommand::FierceButton))
			{
				Context.CurrentAttack = static_cast<uint8>(Deathblow + FirstDeathblowAttack);
			}
		}

		/** Fei drawn as Id on foot, whose one press performs one of Id's whole combos. */
		bool IsDrawnAsId(const FBattleContext& Context, uint8 Slot)
		{
			return Slot < FBattleContext::PartySlotCount && Context.ApRecords[Slot].InGear == 0 && Context.PartyLooks().IsId(Context.PartyCharacters[Slot]);
		}

		/** The presses of one of Id's combos, as Fei would press them. */
		struct FIdsCombo
		{
			std::array<uint8, 5> Presses;
			uint8 Count;
		};

		/**
		 * Id's combos by the turn's AP (3 to 7) and the button pressed (Weak, Strong, Fierce): the presses of a combo of Fei's
		 * of that many AP. At 3 AP his basic attacks; from 4 on the build-up and the Fierce press finishing a Deathblow of
		 * that AP, the lightest for Weak and the strongest for Fierce (4: Deathblow 0 for all; 5: 1, 2, 2; 6: 3, 5, 6; 7: 7,
		 * 11, 12; research battle.md "Deathblows: the combo patterns").
		 */
		const FIdsCombo& IdsCombo(uint8 Ap, uint8 Event)
		{
			using namespace BattleMenuEvents;
			static constexpr std::array<std::array<FIdsCombo, 3>, IdsMostAp - IdsLeastAp + 1> Combos{{
				{{{{Weak, Weak, Weak}, 3}, {{Strong, Weak}, 2}, {{Confirm}, 1}}},
				{{{{Weak, Confirm}, 2}, {{Weak, Confirm}, 2}, {{Weak, Confirm}, 2}}},
				{{{{Weak, Weak, Confirm}, 3}, {{Strong, Confirm}, 2}, {{Strong, Confirm}, 2}}},
				{{{{Weak, Weak, Weak, Confirm}, 4}, {{Strong, Weak, Confirm}, 3}, {{Confirm, Confirm}, 2}}},
				{{{{Weak, Weak, Weak, Weak, Confirm}, 5}, {{Strong, Strong, Confirm}, 3}, {{Confirm, Weak, Confirm}, 3}}},
			}};
			const std::size_t Button = Event == Weak ? 0 : Event == Strong ? 1 : 2;
			return Combos[static_cast<std::size_t>(std::clamp(Ap, IdsLeastAp, IdsMostAp) - IdsLeastAp)][Button];
		}

		/** The animation of Id's own sprite a press plays, at any AP: one of his combos by the button. */
		uint8 IdsAnimation(uint8 Event)
		{
			return Event == BattleMenuEvents::Weak ? IdsQuickCombo : Event == BattleMenuEvents::Strong ? IdsSecondQuickCombo : IdsBigCombo;
		}

		/** A press's AP by its event (0 for none). */
		uint8 PressCost(uint8 Event)
		{
			switch (Event)
			{
			case BattleMenuEvents::Confirm: return 3;
			case BattleMenuEvents::Strong: return 2;
			case BattleMenuEvents::Weak: return 1;
			default: return 0;
			}
		}

		int32 ButtonCommand(uint8 Event)
		{
			switch (Event)
			{
			case BattleMenuEvents::Confirm: return ECommand::FierceButton;
			case BattleMenuEvents::Weak: return ECommand::WeakButton;
			default: return ECommand::StrongButton;
			}
		}

		/** The AP bar's height (in percent) once the press is paid; a turn's AP past the table faults. */
		uint16 ApBarHeight(FBattleContext& Context)
		{
			const FBattlePartyInput& Party = Context.Party;
			if (Party.ApAtStart >= FBattleProgramTables::ApBarRows || Party.ApLeft >= FBattleProgramTables::ApBarColumns)
			{
				Context.Fault = EBattleFault::IndexOutsideTable;
				return 0;
			}
			return static_cast<uint16>(Context.Program->ApBar[Party.ApAtStart][Party.ApLeft] * 100u / ApBarFull);
		}

		/** The press accepted: the target locked, the AP paid, the button recorded (whose hints take a frame, unless bHints is false). */
		EFlow TakePress(FBattleContext& Context, FBattlePartyFrame& Frame, uint8 Slot, uint8 Event, bool bHints = true)
		{
			FBattlePartyInput& Party = Context.Party;
			PlaySound(Context, ESound::Accept);
			LockTarget(Context, Slot);
			if (!Party.bIconsShown)
			{
				Party.MenuState = EState::AttackMode;
				Party.ShownState = EState::AttackMode;
			}
			else
			{
				Party.MenuState = EState::FadingAttackMode;
				Party.ShownState = 0xFF;
				Party.bMenuFading = true;
			}
			Party.ApLeft = static_cast<uint8>(Party.ApLeft - Party.Cost);
			PlayMenuCue(Context, EBattleMenuCue::ApNumbers, Slot);
			RecordButton(Context, Event, Slot);
			if (bHints)
			{
				PlayMenuCue(Context, EBattleMenuCue::DeathblowHints, Slot);
			}
			Frame.Pc = AttackAfterButton;
			return EFlow::Yield;
		}

		/** A result that did something to its slot: not a miss, nor no effect, nor no target. */
		bool Landed(uint8 Result)
		{
			return Result != BattleResults::Miss && Result != BattleResults::NoEffect && Result != BattleResults::NoResult;
		}

		/**
		 * The records from First on made one where the hits were: the reaction before the first hit kept, then the last hit's
		 * record showing what the hits did together (each slot's amounts summed, the turn's running total where its results
		 * are HP's, and the last result that landed), played by Animation of Id's own sprite; any other record after it.
		 */
		void JoinHits(FBattleContext& Context, uint8 First, std::span<const uint8> Hits, uint8 Animation)
		{
			if (Hits.empty())
			{
				return;
			}
			FBattlePerformance Joined = Context.Performances[Hits.back()];
			Joined.Targets = 0;
			for (std::size_t Slot = 0; Slot < FBattlePerformance::SlotCount; ++Slot)
			{
				uint32 Sum = 0;
				for (const uint8 Hit : Hits)
				{
					const FBattlePerformance& Performance = Context.Performances[Hit];
					if (Landed(Performance.Results[Slot]))
					{
						Joined.Results[Slot] = Performance.Results[Slot];
						Sum += Performance.WholeAmounts[Slot];
					}
				}
				const uint8 Kind = Joined.TotalKinds[Slot];
				const bool bHp = Kind == BattleResults::HpDamage || Kind == BattleResults::HalvedDamage || Kind == BattleResults::HpHeal;
				Joined.WholeAmounts[Slot] = bHp ? Joined.WholeTotals[Slot] : Sum;
				Joined.Amounts[Slot] = bHp ? Joined.Totals[Slot] : static_cast<uint16>(Sum);
			}
			std::optional<FBattlePerformance> Reaction;
			std::vector<FBattlePerformance> Others;
			for (uint8 Record = First; Record < Context.PerformanceIndex; ++Record)
			{
				const FBattlePerformance& Performance = Context.Performances[Record];
				if (std::find(Hits.begin(), Hits.end(), Record) != Hits.end())
				{
					Joined.Targets = static_cast<uint16>(Joined.Targets | Performance.Targets);
				}
				else if (Performance.Code != ReactionCode)
				{
					Others.push_back(Performance);
				}
				else if (Record < Hits.front() && !Reaction)
				{
					Reaction = Performance;
				}
			}
			Joined.IdsAnimation = Animation;
			uint8 At = First;
			if (Reaction)
			{
				Context.Performances[At++] = *Reaction;
			}
			Context.Performances[At++] = Joined;
			for (const FBattlePerformance& Other : Others)
			{
				Context.Performances[At++] = Other;
			}
			Context.PerformanceIndex = At;
		}

		/**
		 * Id's combo for the turn's AP: Fei's presses, each hit worked out, applied and queued as his would be, a Fierce press
		 * finishing a pattern the pattern's Deathblow whether learned or not; a hit whose target's script ends the combo ends
		 * it there. The hits' records then become one.
		 */
		void PerformIdsCombo(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			const uint8 First = Context.PerformanceIndex;
			const FIdsCombo& Combo = IdsCombo(Party.Cost, Party.IdsPress);
			const std::span<const uint8> Presses = std::span<const uint8>(Combo.Presses).first(Combo.Count);
			std::array<uint8, FBattlePartyInput::MaxButtons> Hits{};
			std::size_t HitCount = 0;
			Party.Buttons.fill(0xFF);
			Party.ButtonCount = 0;
			for (std::size_t Press = 0; Press < Presses.size(); ++Press)
			{
				const uint8 Event = Presses[Press];
				if (Press + 1 == Presses.size() && Event == BattleMenuEvents::Confirm)
				{
					const uint8 Pattern = MatchPattern(Context);
					if (Pattern != NoPattern)
					{
						Context.CurrentAttack = static_cast<uint8>(PatternDeathblow(Context, Slot, Pattern) + FirstDeathblowAttack);
					}
				}
				Party.Buttons[Party.ButtonCount] = static_cast<uint8>(Event - BattleMenuEvents::Confirm);
				++Party.ButtonCount;
				const uint8 Before = Context.PerformanceIndex;
				const bool bEnds = PerformHit(Context, Slot, PressCost(Event), true);
				if (Context.Fault != EBattleFault::None)
				{
					return;
				}
				// The hit's record follows the reaction queued before it, where its target shows reactions.
				if (Context.PerformanceIndex > Before)
				{
					const bool bAfterReaction = Context.Performances[Before].Code == ReactionCode && Context.PerformanceIndex > Before + 1;
					Hits[HitCount++] = static_cast<uint8>(bAfterReaction ? Before + 1 : Before);
				}
				if (bEnds)
				{
					break;
				}
			}
			JoinHits(Context, First, std::span<const uint8>(Hits).first(HitCount), IdsAnimation(Party.IdsPress));
		}

		/** The combo's end queued when the AP is spent or the hit ends it. */
		EFlow CloseIfDone(FBattleContext& Context, uint8 Slot)
		{
			FBattlePartyInput& Party = Context.Party;
			if (Party.ApLeft == 0)
			{
				QueueEnd(Context, Slot);
				Party.bComboClosed = true;
			}
			if (Party.bHitEndsCombo)
			{
				QueueEnd(Context, Slot);
				Party.bComboClosed = true;
			}
			return EFlow::Return;
		}
	}

	void BeginAttack(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		PlayMenuCue(Context, EBattleMenuCue::ApNumbers, Slot);
		const uint8 Target = Context.SlotTargets[Slot];
		ResetQueue(Context, Slot, Target);
		Party.bMenuSounds = false;
		const bool bStays = BattleSteps::PlanApproachPath(Context, Slot, Target);
		const uint8 Next = FindNextReadySlot(Context, Slot);
		if (Context.Show != nullptr)
		{
			Context.Show->BeginPlayback(bStays, Slot, Target, Next);
		}
	}

	void FinishAttackStart(FBattleContext& Context)
	{
		Context.Party.bMenuSounds = true;
		Context.Party.bApproached = false;
	}

	EFlow RunStartAttack(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		const bool bInGear = Party.MenuState == EState::GearAttack;
		if (Frame.Pc == 0)
		{
			if (!bInGear)
			{
				Party.bMenuSounds = false;
			}
			BeginAttack(Context, Slot);
			Frame.Pc = 1;
		}
		if (ShowBlocks(Context))
		{
			return EFlow::Yield;
		}
		FinishAttackStart(Context);
		InitTargetCursor(Context, Slot);
		PlayMenuCue(Context, EBattleMenuCue::InitArrows, Slot);
		Party.MenuState = bInGear ? EState::GearAttackMode : EState::AttackMode;
		if (bInGear)
		{
			Party.bMenuSounds = false;
		}
		return EFlow::Return;
	}

	void InitTargetCursor(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		Party.Cursor = Context.SlotTargets[Slot];
		PickBattleDefaultTarget(Context, Slot);
		const FBattleTargeting& Targeting = Context.Targeting;
		for (uint8 Index = 0; Index < Targeting.Count && Index < Targeting.List.size(); ++Index)
		{
			if (Targeting.List[Index] == Context.SlotTargets[Slot])
			{
				return;
			}
		}
		Party.Cursor = Targeting.List[0];
	}

	void LockTarget(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Party.bTargetLocked)
		{
			return;
		}
		Party.bTargetLocked = true;
		SetMarkers(Context, 0);
		Context.SlotTargets[Slot] = Party.Cursor;
		PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
		FrameCamera(Context, static_cast<uint16>(Bit(Context, Slot) | Bit(Context, Context.SlotTargets[Slot])));
		if (Context.PartyCharacters[Slot] != Billy)
		{
			const uint16 Targets = Bit(Context, Context.SlotTargets[Slot]);
			if (FBattlePerformance* Record = QueueCode(Context, Slot, ApproachCode, 0))
			{
				Record->Targets = Targets;
			}
		}
		CloseAmmunition(Context, Slot, false);
	}

	bool PerformHit(FBattleContext& Context, uint8 Slot, uint8 Cost, bool bAnyDeathblow)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Target = Context.SlotTargets[Slot];
		const bool bOnFoot = Context.ApRecords[Slot].InGear == 0;
		bool bEnds = false;
		if (Target >= FBattleContext::SlotCount)
		{
			Context.Fault = EBattleFault::SlotOutsideBattle;
			return false;
		}
		if (!Party.bApproached)
		{
			if (!bOnFoot)
			{
				BattleSteps::MoveGearToTargetGroup(Context, Slot, Target);
			}
			else if (Context.PartyCharacters[Slot] != Billy)
			{
				BattleSteps::MoveToTargetGroup(Context, Slot, Target);
			}
			Party.bApproached = true;
		}
		if (CurrentRecord(Context) == nullptr)
		{
			return false;
		}
		ClearBattlePerformance(Context);
		if (bOnFoot)
		{
			if (Context.CurrentAttack < FirstDeathblowAttack)
			{
				Context.CurrentAttack = Context.Program->BasicAttacks[Context.CurrentAttack * 3u + Cost - 1u];
			}
			else if (bAnyDeathblow || HighBit(LearnedDeathblows(Context, Slot), static_cast<uint8>(Context.CurrentAttack - FirstDeathblowAttack)))
			{
				bEnds = true;
			}
			else
			{
				Context.CurrentAttack = FierceAttack;
			}
		}
		else
		{
			Context.CurrentAttack = static_cast<uint8>(Context.CurrentAttack + 1);
		}
		if ((Context.Combatants[Target].Traits & ShowsReactions) != 0)
		{
			QueueCode(Context, Slot, ReactionCode, Bit(Context, Target));
		}
		const uint8 Command = static_cast<uint8>(Context.CurrentAttack - 1);
		PerformBattleCommand(Context, Slot, Command, Bit(Context, Target), Command);
		FBattlePerformance* Record = CurrentRecord(Context);
		if (Context.Fault != EBattleFault::None || Record == nullptr)
		{
			return false;
		}
		const uint8 Committed = Context.PerformanceIndex;
		Record->Actor = Slot;
		Record->Targets = Context.Action.Targets;
		Record->Code = static_cast<uint8>(Context.Action.Animation);
		++Context.PerformanceIndex;
		RecordBattleHit(Context, Slot, Target);
		if (Target >= FBattleContext::PartySlotCount && RunBattleHitScript(Context, Target))
		{
			bEnds = true;
		}
		if (Context.Fault == EBattleFault::None)
		{
			CommitBattleResults(Context, Committed);
		}
		return bEnds;
	}

	EFlow ShowTarget(FBattleContext& Context, uint8 Slot)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Party.bTargetLocked)
		{
			return EFlow::Return;
		}
		FrameCamera(Context, Bit(Context, Party.Cursor));
		SetMarkers(Context, Bit(Context, Party.Cursor));
		uint8 Ways = 0;
		for (uint8 Way = 0; Way < 4; ++Way)
		{
			if (FindTargetInDirection(Context, Party.Cursor, Way) != Party.Cursor)
			{
				Ways = static_cast<uint8>(Ways | 1u << Way);
			}
		}
		PlayMenuCue(Context, EBattleMenuCue::ArrowTargets, Slot, Ways);
		if (Context.GearTurn.Level != InfinityLevel && Context.PartyCharacters[Slot] == Billy && Party.ListWindow == 0)
		{
			return Call(Context, EBattlePartyRoutine::OpenAmmunition);
		}
		return EFlow::Return;
	}

	EFlow CloseAmmunition(FBattleContext& Context, uint8 Slot, bool bClose)
	{
		FBattlePartyInput& Party = Context.Party;
		if (Context.PartyCharacters[Slot] != Billy)
		{
			return EFlow::Return;
		}
		Party.ListWindow = 0;
		PlayMenuCue(Context, EBattleMenuCue::HideAmmunition, Slot);
		if (bClose && Party.bAmmunitionShown)
		{
			return Call(Context, EBattlePartyRoutine::CloseAmmunition);
		}
		return EFlow::Return;
	}

	EFlow RunOpenAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		if (Frame.Pc == 0)
		{
			PlayMenuCue(Context, EBattleMenuCue::BuildAmmunition, Context.Party.Actor);
			Frame.Pc = 1;
			return EFlow::Yield;
		}
		Context.Party.ListWindow = AmmunitionWindow;
		Context.Party.bAmmunitionShown = true;
		return EFlow::Return;
	}

	EFlow RunCloseAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		if (Frame.Pc < 2)
		{
			if (Frame.Pc == 0)
			{
				PlayMenuCue(Context, EBattleMenuCue::CloseListWindow, Context.Party.Actor, 0);
			}
			++Frame.Pc;
			return EFlow::Yield;
		}
		CloseBattleAmmunitionWindow(Context);
		return EFlow::Return;
	}

	EFlow RunAttackMode(FBattleContext& Context, FBattlePartyFrame& Frame)
	{
		FBattlePartyInput& Party = Context.Party;
		const uint8 Slot = Party.Actor;
		switch (Frame.Pc)
		{
		case AttackStart:
			Party.bMenuSounds = false;
			if (Party.bComboClosed)
			{
				return EFlow::Return;
			}
			Party.Cost = 0;
			Frame.Pc = AttackShown;
			if (ShowTarget(Context, Slot) == EFlow::Call)
			{
				return EFlow::Call;
			}
			[[fallthrough]];
		case AttackShown:
		{
			const uint8 Event = Party.Event;
			Frame.Pc = AttackAfterCancel;
			if (Event == BattleMenuEvents::Cancel)
			{
				if (Party.ApLeft == Party.ApAtStart)
				{
					if (Context.Show != nullptr)
					{
						Context.Show->CancelPlayback();
					}
					Party.bMenuSounds = true;
					PlayMenuCue(Context, EBattleMenuCue::CancelApNumbers, Slot);
					Party.MenuState = EState::Attack;
					PlayMenuCue(Context, EBattleMenuCue::HideArrows, Slot);
					if (CloseAmmunition(Context, Slot, true) == EFlow::Call)
					{
						return EFlow::Call;
					}
				}
				else
				{
					QueueEnd(Context, Slot);
					Party.bComboClosed = true;
				}
			}
			else if (PressCost(Event) != 0 && IsDisabled(Context, Slot, ButtonCommand(Event)))
			{
				Party.Event = BattleMenuEvents::Cancel;
				PlaySound(Context, ESound::Refuse);
			}
			[[fallthrough]];
		}
		case AttackAfterCancel:
		{
			const uint8 Event = Party.Event;
			if (!Party.bTargetLocked)
			{
				if (Event <= BattleMenuEvents::Up)
				{
					Party.Cursor = FindTargetInDirection(Context, Party.Cursor, Event);
					ResetQueue(Context, Context.TurnActor, Party.Cursor);
					PlaySound(Context, ESound::Move);
				}
				if (Party.Cursor >= FBattleContext::SlotCount)
				{
					Context.Fault = EBattleFault::SlotOutsideBattle;
					return EFlow::Yield;
				}
				BattleSteps::PlanApproachPath(Context, Slot, Party.Cursor);
			}
			PlayMenuCue(Context, EBattleMenuCue::ShowApNumbers, Slot, 1);
			if (Event == BattleMenuEvents::Cancel || PressCost(Event) == 0)
			{
				return EFlow::Return;
			}
			if (IsDrawnAsId(Context, Slot))
			{
				// The press takes the turn's AP, Fei's AP per turn, for one of Id's combos of that many; no further press to hint at.
				Party.IdsPress = Event;
				Party.Cost = Party.ApLeft;
				return TakePress(Context, Frame, Slot, Event, false);
			}
			if (Event == BattleMenuEvents::Confirm && IsDeathblowFinisher(Context, Event, Slot) && Party.ApLeft >= 3)
			{
				Party.Deathblow = static_cast<uint8>(Party.Deathblow + 1);
			}
			Party.Cost = PressCost(Event);
			if (Party.ApLeft < Party.Cost)
			{
				PlaySound(Context, ESound::Refuse);
				return CloseIfDone(Context, Slot);
			}
			return TakePress(Context, Frame, Slot, Event);
		}
		case AttackAfterButton:
			if (Party.IdsPress != BattleMenuEvents::Nothing)
			{
				PerformIdsCombo(Context, Slot);
				Party.bHitEndsCombo = false;
			}
			else
			{
				Party.bHitEndsCombo = PerformHit(Context, Slot, Party.Cost);
			}
			if (Context.Fault != EBattleFault::None)
			{
				return EFlow::Yield;
			}
			Context.Waits[Slot] = ApBarHeight(Context);
			Party.bStoresAp = true;
			return CloseIfDone(Context, Slot);
		default:
			return EFlow::Return;
		}
	}
}
