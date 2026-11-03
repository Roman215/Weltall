#include "Battle/BattlePartyStart.h"

#include "Battle/BattleContext.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleRules.h"
#include "Game/EtherHealing.h"

namespace Weltall
{
	using namespace BattleRules;

	namespace
	{
		constexpr uint8 Billy = 4;
		constexpr uint8 ChuChu = 7;
		constexpr uint8 SecondCitan = 9;
		constexpr uint8 AdultEmeralda = 10;
		constexpr uint8 Xenogears = 15;
		constexpr uint8 Yggdra = 18;
		constexpr std::size_t Bart = 3;
		/** Gear command 37, Repair, whose fuel cost follows the gear's Repair parts. */
		constexpr int32 RepairCommand = 37;
		/** The Special after Repair that Fei as Id keeps in Xenogears as True Id. */
		constexpr int32 TrueIdSpecial = 1;
		constexpr uint16 SharperEyes = 0x0100;

		uint8 AddByte(uint8 Value, uint8 Bonus)
		{
			return static_cast<uint8>(Value + Bonus);
		}

		/** What the battle starts each member with, before the equipment's bonuses: the result screen's comparison. */
		void RecordStartFigures(FBattleContext& Context, std::size_t Slot, const FBattleCombatant& Member)
		{
			FBattleStartFigures& Figures = Context.StartFigures[Slot];
			const uint8 Attack = Member.Character == Billy ? AddByte(Member.Weapons[0].Attack, Member.Weapons[3].Attack) : AddByte(Member.Attack, Member.Weapons[0].Attack);
			Figures.Stats = {Attack, Member.Hit, AddByte(Member.Defense, Member.EquipmentBonuses[5]), Member.Evade, Member.Ether, Member.EtherDefense, Member.Agility};
			Figures.Exp = Member.Exp;
			Figures.EtherExp = Member.EtherExp;
			Figures.MaxHp = Member.MaxHp;
			Figures.MaxEp = Member.MaxEp;
		}

		void AddEquipment(FBattleCombatant& Member, bool bPerfectWorksRebalance)
		{
			const std::array<uint8, 8>& Bonus = Member.EquipmentBonuses;
			// Perfect Works' rebalance adds its own bonuses too, before the agility's cap.
			const std::array<uint8, 5> Extra = bPerfectWorksRebalance ? Member.ExtraBonuses : std::array<uint8, 5>{};
			Member.Attack = AddByte(AddByte(Member.Attack, Bonus[0]), Extra[0]);
			Member.Traits = 0;
			Member.Defense = AddByte(AddByte(Member.Defense, Bonus[1]), Extra[1]);
			Member.Agility = AddByte(AddByte(Member.Agility, Bonus[2]), Extra[4]);
			Member.Ether = AddByte(AddByte(Member.Ether, Bonus[3]), Extra[2]);
			Member.EtherDefense = AddByte(AddByte(Member.EtherDefense, Bonus[4]), Extra[3]);
			Member.Hit = AddByte(Member.Hit, Bonus[6]);
			Member.Evade = AddByte(Member.Evade, Bonus[7]);
			if (Member.Agility > 16)
			{
				Member.Agility = 16;
			}
			if ((Member.EquipmentFlags & SharperEyes) != 0)
			{
				Member.Hit = AddByte(Member.Hit, static_cast<uint8>(Member.Hit >> 2));
				Member.Evade = AddByte(Member.Evade, static_cast<uint8>(Member.Evade >> 2));
			}
			const uint32 HpGain = static_cast<uint32>(Member.MaxHp) * Member.HpBonus / 20;
			const uint32 EpGain = static_cast<uint32>(Member.MaxEp) * Member.EpBonus / 20;
			Member.Hp = static_cast<uint16>(Member.Hp + HpGain);
			Member.Ep = static_cast<uint16>(Member.Ep + EpGain);
			Member.MaxHp = static_cast<uint16>(Member.MaxHp + HpGain);
			Member.MaxEp = static_cast<uint16>(Member.MaxEp + EpGain);
			Member.Hp = std::min<uint16>(Member.Hp, 999);
			Member.Ep = std::min<uint16>(Member.Ep, 99);
			Member.MaxHp = std::min<uint16>(Member.MaxHp, 999);
			Member.MaxEp = std::min<uint16>(Member.MaxEp, 99);
		}

		void FitGearParts(FBattleContext& Context, int32 Slot, FBattleCombatant& Member)
		{
			FBattleGearPart& Gear = Member.GearPart;
			Gear.Defense = static_cast<uint16>(Gear.Defense + Gear.PartsDefense);
			Gear.EtherDefense = static_cast<uint16>(Gear.EtherDefense + Gear.PartsEtherDefense);
			Gear.Guard = AddByte(Gear.Guard, Gear.PartsGuard);
			Gear.WeightCapacity = static_cast<uint16>(Gear.WeightCapacity + Gear.PartsWeight + Gear.Word46);
			Gear.HitBonus = static_cast<int8>(Gear.HitBonus + Gear.PartsHit);
			Gear.Agility = static_cast<uint8>(Gear.Agility + static_cast<uint8>(Gear.PartsAgility - Gear.OverweightPenalty));
			Gear.EtherAmplifier = AddByte(Gear.EtherAmplifier, Gear.PartsEtherAmplifier);
			if (Gear.RepairLevel != 0 && Member.Gear != Yggdra)
			{
				uint16 Cost = static_cast<uint16>(Gear.MaxHp / 10 * Gear.RepairLevel * 2 / 9);
				Cost = static_cast<uint16>(Cost / 10);
				Cost = static_cast<uint16>(Cost * 10);
				Context.Commands[FBattleContext::GearCommandIndex(Slot, RepairCommand)].FuelCost = Cost;
			}
		}

		/** The highest attack level whose gear Deathblows the pilot knows. */
		uint8 MaxAttackLevel(uint16 GearDeathblows)
		{
			uint8 Level = 0;
			if ((GearDeathblows & 0x1C00) != 0)
			{
				Level = 1;
			}
			if ((GearDeathblows & 0x0380) != 0)
			{
				Level = 2;
			}
			if ((GearDeathblows & 0x0070) != 0)
			{
				Level = 3;
			}
			return Level;
		}

		/** Ether Doubler doubles every EP cost of the member's and its gear's tables; Economether then halves them, rounding up. */
		void ScaleEtherCosts(FBattleContext& Context, int32 Slot, const FBattleCombatant& Member)
		{
			for (int32 Command = 0; Command < FBattleContext::PartyTableSize; ++Command)
			{
				uint8& Cost = Context.Commands[FBattleContext::PartyCommandIndex(Slot, Command)].EpCost;
				Cost = EtherCost(Cost, Member.PermanentEtherBuffs, Member.EquipmentFlags);
			}
			for (int32 Command = 0; Command < FBattleContext::GearTableSize; ++Command)
			{
				uint8& Cost = Context.Commands[FBattleContext::GearCommandIndex(Slot, Command)].EpCost;
				Cost = EtherCost(Cost, Member.PermanentEtherBuffs, Member.EquipmentFlags);
			}
		}

		/** Chu-Chu fights as her giant self: her gear part is made from her own figures. */
		void MakeGiantChuChu(FBattleCombatant& Member)
		{
			FBattleGearPart& Gear = Member.GearPart;
			Gear.Hp = static_cast<uint32>(Member.Hp) * 50;
			Gear.MaxHp = static_cast<uint32>(Member.MaxHp) * 50;
			Gear.Output = Member.Attack;
			Gear.Defense = static_cast<uint16>(Member.Defense * 12);
			Gear.EtherDefense = static_cast<uint16>(Member.EtherDefense * 6);
			Gear.Agility = Member.Agility;
			const uint16 Immunity = Gear.StatusImmunity;
			Gear.StatusImmunity = static_cast<uint16>(Immunity | 0x3C4);
			if ((Member.MentalImmunity & 0x2000) != 0)
			{
				Gear.StatusImmunity = static_cast<uint16>(Immunity | 0x13C4);
			}
		}

		void InitMember(FBattleContext& Context, int32 Slot)
		{
			using namespace GameStateLayout;
			FGameState& State = *Context.GameState;
			const std::size_t Index = static_cast<std::size_t>(Slot);
			FBattleCombatant& Member = Context.Combatants[Index];
			PointActorAt(Context, Slot);
			const uint8 Character = Context.PartyCharacters[Index];
			if (Character == SecondCitan || Character == AdultEmeralda)
			{
				BattleGameState::Byte(State, CharacterOffset(Character) + 0x56) = Character;
				Member.Character = Character;
			}
			if (!BattleGameState::HasCharacter(Member.Character))
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return;
			}
			RecordStartFigures(Context, Index, Member);
			AddEquipment(Member, Context.bPerfectWorksRebalance);
			Member.ExpShare = 5;
			Member.EtherExpShare = 5;
			Member.AttackLevel = 0;
			FitGearParts(Context, Slot, Member);

			const std::size_t GearDeathblows = BattleGameState::SkillsOffset(Member.Character, Skills::GearDeathblows);
			Member.MaxAttackLevel = MaxAttackLevel(BattleGameState::Word(State, GearDeathblows));
			FBattleGearPart& Gear = Member.GearPart;
			Gear.AttackMultiplier = AddByte(Gear.BaseMultiplier, Gear.MultiplierBonus);
			Gear.Byte3E = AddByte(Gear.BaseMultiplier, Gear.MultiplierBonus);
			ScaleEtherCosts(Context, Slot, Member);
			if (Member.Level > 49)
			{
				BattleGameState::SetWord(State, GearDeathblows, static_cast<uint16>(BattleGameState::Word(State, GearDeathblows) | 8));
			}
			if (Member.Character == ChuChu)
			{
				MakeGiantChuChu(Member);
			}
			if (Member.Gear == Xenogears)
			{
				const std::size_t Special = BattleGameState::SkillsOffset(Member.Character, Skills::Special);
				BattleGameState::SetWord(State, Special, static_cast<uint16>(BattleGameState::Word(State, Special) & 0x8FFF));
				if (Context.IsTrueId(Slot, TrueIdSpecial))
				{
					Context.Commands[FBattleContext::GearCommandIndex(Slot, RepairCommand + TrueIdSpecial)].FuelCost = 0;
				}
			}
			if (Member.Gear == Yggdra)
			{
				Member.DisabledCommands = 0x238;
				Gear.Fuel = 9900;
				Gear.MaxFuel = 9900;
				Member.MaxAttackLevel = 0;
				BattleGameState::SetWord(State, BattleGameState::SkillsOffset(Bart, Skills::Special), 0x8000);
			}
			uint8& Once = BattleGameState::Byte(State, BattleOnceFlagsOffset);
			if (BattleGameState::Story(State) > 0xE6 && (Once & 0x80) == 0)
			{
				BattleGameState::Byte(State, CharacterOffset(SecondCitan) + Character::Weapon) = 39;
				BattleGameState::Byte(State, CharacterOffset(SecondCitan) + Character::WeaponAttack) = 30;
				Once = static_cast<uint8>(Once | 0x80);
			}
		}
	}

	void InitPartyCombatants(FBattleContext& Context)
	{
		for (int32 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			if (Context.PartyCharacters[static_cast<std::size_t>(Slot)] != FBattleContext::NoCharacter)
			{
				InitMember(Context, Slot);
			}
		}
		Context.PartyCount = 3;
		for (const uint8 Character : Context.PartyCharacters)
		{
			if (Character == FBattleContext::NoCharacter)
			{
				--Context.PartyCount;
			}
		}
	}

	void InitSpecialPartyMembers(FBattleContext& Context)
	{
		for (std::size_t Slot = 1; Slot < FBattleContext::PartySlotCount; ++Slot)
		{
			FBattleCombatant& Member = Context.Combatants[Slot];
			Member.Hp = 100;
			Member.MaxHp = 100;
			Member.Hit = 20;
			Member.Evade = 15;
			Member.DisabledCommands = 0x1FBF;
			Member.MaxAttackLevel = 0;
		}
	}
}
