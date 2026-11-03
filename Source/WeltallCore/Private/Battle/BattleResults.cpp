#include "Battle/BattleAction.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"
#include "Battle/BattleTurn.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		/** A slot's running total over the turn, which nets heals and damage against each other. */
		template <typename TAmount>
		void Accumulate(uint8 Result, TAmount Amount, TAmount& Total, uint8& Kind)
		{
			const bool bDamage = Result == BattleResults::HpDamage || Result == BattleResults::HalvedDamage;
			if (Result != BattleResults::HpHeal && !bDamage)
			{
				return;
			}
			const bool bKindDamage = Kind == BattleResults::HpDamage || Kind == BattleResults::HalvedDamage;
			const bool bSame = Result == BattleResults::HpHeal ? Kind == BattleResults::HpHeal : bKindDamage;
			const bool bOpposite = Result == BattleResults::HpHeal ? bKindDamage : Kind == BattleResults::HpHeal;
			if (bSame)
			{
				Total = static_cast<TAmount>(Total + Amount);
			}
			else if (bOpposite)
			{
				if (static_cast<int64>(Amount) - static_cast<int64>(Total) >= 0)
				{
					Total = static_cast<TAmount>(Amount - Total);
					Kind = Result == BattleResults::HpHeal ? BattleResults::HpHeal : BattleResults::HpDamage;
				}
				else
				{
					Total = static_cast<TAmount>(Total - Amount);
				}
			}
			else
			{
				Total = Amount;
				Kind = Result;
			}
		}

		bool Drops(uint16 Value, uint16 Amount)
		{
			return static_cast<int32>(static_cast<int16>(Value)) - static_cast<int32>(static_cast<int16>(Amount)) < 1;
		}

		void KnockOut(FBattleContext& Context, std::size_t Slot)
		{
			Context.Action.KnockedOut |= SlotBit(static_cast<int32>(Slot));
			if (Slot >= FBattleContext::PartySlotCount)
			{
				BattleSteps::LeaveGroup(Context, static_cast<uint8>(Slot));
			}
		}
	}

	void CommitBattleResults(FBattleContext& Context, uint8 Record)
	{
		FBattlePerformance& Performance = Context.Performances[Record];
		for (std::size_t Slot = 0; Slot < FBattlePerformance::SlotCount; ++Slot)
		{
			const uint16 Amount = static_cast<uint16>(Context.Action.Amounts[Slot]);
			const uint8 Result = Context.Action.Results[Slot];
			Performance.Amounts[Slot] = Amount;
			Performance.WholeAmounts[Slot] = Context.Action.Amounts[Slot];
			Performance.Results[Slot] = Result;
			int16 Total = static_cast<int16>(Context.TurnTotals[Slot]);
			Accumulate(Result, static_cast<int16>(Amount), Total, Context.TurnTotalKinds[Slot]);
			Context.TurnTotals[Slot] = static_cast<uint16>(Total);
			Performance.Totals[Slot] = static_cast<uint16>(Total);
			Performance.TotalKinds[Slot] = Context.TurnTotalKinds[Slot];
			Accumulate(Result, static_cast<int32>(Context.Action.Amounts[Slot]), Context.TurnWholeTotals[Slot], Context.TurnWholeTotalKinds[Slot]);
			Performance.WholeTotals[Slot] = static_cast<uint32>(Context.TurnWholeTotals[Slot]);
			Performance.WholeTotalKinds[Slot] = Context.TurnWholeTotalKinds[Slot];
		}
		Performance.bShownWhole = Context.RuleOptions.bWholeAmounts;
		ApplyBattleResults(Context, Record);
		if (Context.Show != nullptr)
		{
			Context.Show->PlayMenuCue(EBattleMenuCue::HideCockpit, 0, 0);
		}
	}

	void ApplyBattleResults(FBattleContext& Context, uint8 Record)
	{
		const FBattlePerformance& Performance = Context.Performances[Record];
		for (std::size_t Slot = 0; Slot < FBattleContext::SlotCount; ++Slot)
		{
			if (!Context.InBattle[Slot])
			{
				continue;
			}
			FBattleCombatant& Combatant = Context.Combatants[Slot];
			if ((Combatant.Status & 0x8000) != 0)
			{
				Context.Action.KnockedOut |= SlotBit(static_cast<int32>(Slot));
				continue;
			}
			const uint16 Amount = Performance.Amounts[Slot];
			const bool bGear = Context.Slots[Slot].InGear != 0;
			FBattleGearPart& Gear = Combatant.GearPart;
			const uint32 GearAmount = Context.RuleOptions.bWholeAmounts ? Performance.WholeAmounts[Slot] : Amount;
			switch (Performance.Results[Slot])
			{
			case BattleResults::HpDamage:
			case BattleResults::HalvedDamage:
			case BattleResults::Counterattack:
			case BattleResults::TurnHpLoss:
				if (!bGear)
				{
					if (Drops(Combatant.Hp, Amount))
					{
						Combatant.Hp = 0;
						Combatant.Status |= 0x8000;
						KnockOut(Context, Slot);
					}
					else
					{
						Combatant.Hp = static_cast<uint16>(Combatant.Hp - Amount);
					}
				}
				else
				{
					const uint32 Left = Gear.Hp - GearAmount;
					if (static_cast<int32>(Left) < 1)
					{
						Gear.Hp = 0;
						Gear.Status |= 0x8000;
						Combatant.Status |= 0x8000;
						KnockOut(Context, Slot);
					}
					else
					{
						Gear.Hp = Left;
					}
				}
				break;
			case BattleResults::EpDamage:
			case BattleResults::TurnEpLoss:
				Combatant.Ep = Drops(Combatant.Ep, Amount) ? 0 : static_cast<uint16>(Combatant.Ep - Amount);
				continue;
			case BattleResults::HpHeal:
				if (!bGear || Context.bItemHealsPilot)
				{
					Combatant.Hp = static_cast<uint16>(Combatant.Hp + Amount);
					if (Combatant.MaxHp < Combatant.Hp)
					{
						Combatant.Hp = Combatant.MaxHp;
					}
				}
				else
				{
					Gear.Hp = GearAmount + Gear.Hp;
					if (Gear.MaxHp < Gear.Hp)
					{
						Gear.Hp = Gear.MaxHp;
					}
				}
				break;
			case BattleResults::EpHeal:
				if (!bGear || Context.bItemHealsPilot)
				{
					Combatant.Ep = static_cast<uint16>(Combatant.Ep + Amount);
					if (Combatant.MaxEp < Combatant.Ep)
					{
						Combatant.Ep = Combatant.MaxEp;
					}
				}
				continue;
			case BattleResults::FuelLoss:
				Gear.Fuel = static_cast<int32>(Gear.Fuel) - static_cast<int32>(Amount) < 1 ? 0 : static_cast<uint16>(Gear.Fuel - Amount);
				break;
			case BattleResults::FuelGain:
				Gear.Fuel = static_cast<uint16>(Gear.Fuel + Amount);
				if (Gear.MaxFuel < Gear.Fuel)
				{
					Gear.Fuel = Gear.MaxFuel;
				}
				break;
			default:
				continue;
			}
			Context.ChangedSlots[Slot] = 1;
		}
	}

	void ClearBattlePerformance(FBattleContext& Context)
	{
		FBattlePerformance& Performance = Context.Performances[Context.PerformanceIndex];
		Performance.Amounts = {};
		Performance.WholeAmounts = {};
		Performance.Results.fill(BattleResults::NoResult);
		Performance.Totals = {};
		Performance.TotalKinds.fill(0xFF);
		Performance.WholeTotals = {};
		Performance.WholeTotalKinds.fill(0xFF);
		Performance.bShownWhole = false;
		Performance.IdsAnimation = 0xFF;
	}

	void ResetBattleTurnTotals(FBattleContext& Context)
	{
		Context.TurnTotalKinds.fill(0xFF);
		Context.TurnTotals = {};
		Context.TurnWholeTotalKinds.fill(0xFF);
		Context.TurnWholeTotals = {};
	}

	void ApplyTurnDrains(FBattleContext& Context, uint8 Slot)
	{
		std::array<uint32, 3> Drains{};
		const FBattleCombatant& Combatant = Context.Combatants[Slot];
		if ((Combatant.Status & 0x8000) != 0)
		{
			return;
		}
		const FBattleGearPart& Gear = Combatant.GearPart;
		const bool bPoisoned = (Combatant.Status & 0x800) != 0;
		const bool bLosesEp = (Combatant.Mental & 0x200) != 0 || (Gear.Status & 0x200) != 0;
		const bool bLeaks = (Gear.Status & 0x80) != 0;
		// Perfect Works' rebalance drains fuel for any of the gear's permanent buffs, its booster and its engine parts',
		// while the pilot is boarded.
		const bool bRebalance = Context.bPerfectWorksRebalance;
		const bool bBoosted = bRebalance ? Gear.PermanentBuffs != 0 && Combatant.IsInGear() : (Gear.Buffs & 0x8000) != 0;
		if (!bPoisoned && !bLosesEp && !bLeaks && !bBoosted)
		{
			return;
		}
		if (bPoisoned)
		{
			Drains[0] = Combatant.MaxHp / 20u;
		}
		if (bLosesEp)
		{
			Drains[1] = Combatant.MaxEp / 20u;
		}
		if (bLeaks)
		{
			Drains[2] = Gear.MaxFuel / 50u;
		}
		if (bBoosted)
		{
			Drains[2] += Gear.MaxFuel / 50u;
		}
		if (bBoosted && bRebalance)
		{
			// Its engine parts' bits: 4 no fuel, 1 a quarter less, 2 half.
			const uint32 Fuel = Drains[2];
			Drains[2] = (Gear.PermanentBuffs & 4) != 0 ? 0 : (Gear.PermanentBuffs & 1) != 0 ? Fuel - (Fuel >> 2) : (Gear.PermanentBuffs & 2) != 0 ? Fuel - (Fuel >> 1) : Fuel;
		}
		for (std::size_t Kind = 0; Kind < Drains.size(); ++Kind)
		{
			if (Drains[Kind] != 0)
			{
				Context.PerformanceIndex = 0;
				ClearBattlePerformance(Context);
				Context.Performances[0].Results[Slot] = static_cast<uint8>(BattleResults::TurnHpLoss + Kind);
				Context.Performances[0].Amounts[Slot] = static_cast<uint16>(Drains[Kind]);
				Context.Performances[0].WholeAmounts[Slot] = Drains[Kind];
				ApplyBattleResults(Context, 0);
			}
		}
		if (Context.Show != nullptr)
		{
			std::array<uint32, 3> Shown = Drains;
			if (!Context.RuleOptions.bWholeAmounts)
			{
				for (uint32& Amount : Shown)
				{
					Amount &= 0xFFFF;
				}
			}
			Context.Show->ShowTurnDamage(Slot, Shown);
		}
	}
}
