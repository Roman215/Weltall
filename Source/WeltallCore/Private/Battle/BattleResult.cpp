#include "Battle/BattleResult.h"

#include "Battle/BattleGameState.h"
#include "Battle/BattleRules.h"
#include "Formats/SectionContainer.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/ProcessorDivide.h"
#include "Media/GameFileIndex.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 GrowthDirectory = 16;
		constexpr int32 GrowthFile = 15;
		constexpr uint32 GrowthSection = 1;
		constexpr uint8 NoMember = 0xFF;
		constexpr uint8 EmptyOccupant = 0x7F;
		constexpr uint16 Dead = 0x8000;
		constexpr uint16 Down = 0xC000;
		/** The story flags: bit 15 the normal EXP rules (levels stop at 99, EXP shared evenly), bit 14 the 7-AP tier and the late Deathblows. */
		constexpr uint16 NormalExpRules = 0x8000;
		constexpr uint16 LateTier = 0x4000;
		constexpr uint32 GoldCap = 9999999;
		constexpr uint8 MaxLevel = 99;
		constexpr uint8 Billy = 4;
		constexpr uint8 ChuChu = 7;
		constexpr uint8 Maria = 8;
		constexpr uint8 Bart = 3;
		constexpr uint8 Yggdrasil = 0x12;
		constexpr std::size_t YggdrasilRepairGear = 12;
		constexpr uint16 DoubleExpEquipment = 0x2000;
		constexpr uint16 EtherExpEquipment = 0x1000;
		constexpr uint16 AlwaysDropEquipment = 0x0800;
		constexpr std::size_t BillySecondGunAttack = 0x1C;

		namespace Growth
		{
			constexpr std::size_t Thresholds = 0x00;
			constexpr std::size_t HpTarget = 0xB8;
			constexpr std::size_t HpTargetHigh = 0xBA;
			constexpr std::size_t StatTargets = 0xBC;
			constexpr std::size_t EpTarget = 0xC8;
			constexpr std::size_t ApLevels = 0xCC;
			constexpr std::size_t GearDeathblowSources = 0xD0;
			constexpr std::size_t GearCommandSources = 0xE0;
			constexpr std::size_t AbilityLevels = 0xF0;
			constexpr std::size_t DeathblowLevels = 0x100;
		}

		namespace Record
		{
			constexpr std::size_t WeaponAttack = 0x04;
			constexpr std::size_t ArmorDefense = 0x2D;
			constexpr std::size_t EquipmentFlags = 0x32;
			constexpr std::size_t Exp = 0x3C;
			constexpr std::size_t EtherExp = 0x40;
			constexpr std::size_t ExpToNext = 0x44;
			constexpr std::size_t EtherExpToNext = 0x48;
			constexpr std::size_t MaxHp = 0x4E;
			constexpr std::size_t MaxEp = 0x52;
			constexpr std::size_t Id = 0x56;
			constexpr std::size_t Attack = 0x58;
			constexpr std::size_t Defense = 0x59;
			constexpr std::size_t Agility = 0x5A;
			constexpr std::size_t Ether = 0x5B;
			constexpr std::size_t EtherDefense = 0x5C;
			constexpr std::size_t Hit = 0x5E;
			constexpr std::size_t Evade = 0x5F;
			constexpr std::size_t Level = 0x62;
			constexpr std::size_t EtherLevel = 0x63;
		}

		/** The growth table and the game state as the result reads and writes them, a read or write past either faulting. */
		class FResultState
		{
		public:
			FResultState(FBattleContext& InContext, std::span<const uint8> InGrowth) : Context(InContext), GrowthBytes(InGrowth), Block(InContext.GameState->Block()) {}

			uint8 GrowthByte(uint32 Character, std::size_t Offset)
			{
				const std::size_t At = Character * FBattleGrowthTable::RecordSize + Offset;
				return At < GrowthBytes.size() ? GrowthBytes[At] : Fail(uint8{0});
			}

			uint16 GrowthWord(uint32 Character, std::size_t Offset)
			{
				const std::size_t At = Character * FBattleGrowthTable::RecordSize + Offset;
				return At + 2 <= GrowthBytes.size() ? LittleEndian::ReadU16(GrowthBytes, At) : Fail(uint16{0});
			}

			uint32 ExpToNext(uint8 Level)
			{
				const std::size_t At = FBattleGrowthTable::ExpTableOffset + 4 * static_cast<std::size_t>(Level);
				return At + 4 <= GrowthBytes.size() ? LittleEndian::ReadU32(GrowthBytes, At) : Fail(uint32{0});
			}

			uint8& Byte(std::size_t Offset)
			{
				if (Offset >= Block.size())
				{
					Context.Fault = EBattleFault::OutsideGameState;
					return Scratch;
				}
				return Block[Offset];
			}

			uint16 Word(std::size_t Offset) { return Offset + 2 <= Block.size() ? LittleEndian::ReadU16(Block, Offset) : Fail(uint16{0}); }
			uint32 Long(std::size_t Offset) { return Offset + 4 <= Block.size() ? LittleEndian::ReadU32(Block, Offset) : Fail(uint32{0}); }

			void SetWord(std::size_t Offset, uint16 Value)
			{
				if (Offset + 2 <= Block.size())
				{
					LittleEndian::WriteU16(Block, Offset, Value);
				}
				else
				{
					Context.Fault = EBattleFault::OutsideGameState;
				}
			}

			void SetLong(std::size_t Offset, uint32 Value)
			{
				if (Offset + 4 <= Block.size())
				{
					LittleEndian::WriteU32(Block, Offset, Value);
				}
				else
				{
					Context.Fault = EBattleFault::OutsideGameState;
				}
			}

			static std::size_t Character(uint32 Id) { return GameStateLayout::CharacterOffset(Id); }
			static std::size_t Skills(uint32 Id, std::size_t Field) { return GameStateLayout::LearnedSkillsOffsetOf(Id) + Field; }

			uint16 Story() { return Word(GameStateLayout::StoryFlagsOffset); }

			int32 Percent() { return Context.Random->Next() % 100; }

			FBattleContext& Context;

		private:
			template <typename T>
			T Fail(T Value)
			{
				Context.Fault = EBattleFault::OutsideGameState;
				return Value;
			}

			std::span<const uint8> GrowthBytes;
			std::span<uint8> Block;
			uint8 Scratch = 0;
		};

		bool Rises(int32 Step, int32 Roll)
		{
			return static_cast<int16>(Step - 50 + Roll) > 49;
		}

		uint8 GrowStat(FResultState& S, uint8 Current, uint8 Target, uint8 Cap, uint8 Level)
		{
			const int32 Roll = S.Percent();
			const int32 Step = ProcessorDivide(static_cast<int32>((static_cast<uint32>(Target) - Current) * 100u), static_cast<int32>(Cap) - Level);
			const uint8 Grown = static_cast<uint8>(Current + (Rises(Step, Roll) ? 1 : 0));
			return Grown > 200 ? uint8{200} : Grown;
		}

		uint16 GrowHp(FResultState& S, uint32 Id, uint16 Hp, uint8 Level)
		{
			int32 Gain = 0;
			if (Level < 100)
			{
				const int32 Roll = S.Percent();
				const uint32 Room = static_cast<uint32>(S.GrowthWord(Id, Growth::HpTarget)) - (static_cast<uint32>(Level) - 99u) - Hp;
				Gain = ProcessorDivide(static_cast<int32>(static_cast<uint32>(Roll) * Room), (100 - static_cast<int32>(Level)) * 100) + 2;
			}
			else
			{
				const int32 Roll = S.Percent();
				const int32 Room = static_cast<int32>(static_cast<uint32>(S.GrowthWord(Id, Growth::HpTargetHigh)) - Hp);
				Gain = Roll * ProcessorDivide(Room, (0xC9 - static_cast<int32>(Level)) * 100) * 2 + 2;
			}
			uint16 Grown = static_cast<uint16>(static_cast<int16>(Gain) + Hp);
			if (static_cast<int16>(Gain) < 0)
			{
				Grown = Hp;
			}
			return static_cast<int16>(Grown) > 999 ? uint16{999} : Grown;
		}

		uint8 GrowEp(FResultState& S, uint32 Id, uint8 Ep, uint8 Level)
		{
			const bool bLow = Level < 100;
			const uint8 Target = S.GrowthByte(Id, Growth::EpTarget + (bLow ? 0 : 1));
			const int32 Cap = bLow ? 99 : 200;
			const int32 Roll = S.Percent();
			// The fix divides the last level's distance by 1, where the original divides by zero.
			const int32 Levels = Cap - Level == 0 && S.Context.RuleOptions.bEpGrowsAtLastLevel ? 1 : Cap - Level;
			const int32 Step = ProcessorDivide(static_cast<int32>((static_cast<uint32>(Target) - Ep) * 100u), Levels);
			const uint8 Grown = static_cast<uint8>(Ep + (Rises(Step, Roll) ? 1 : 0));
			return Grown > 99 ? uint8{99} : Grown;
		}

		/** A level's HP, attack, defense, hit and evade, by the targets of the character the record names. */
		void GrowPhysical(FResultState& S, uint32 Character)
		{
			const std::size_t R = FResultState::Character(Character);
			const uint32 Id = S.Byte(R + Record::Id);
			const uint8 Level = S.Byte(R + Record::Level);
			const std::size_t High = Level > 99 ? 1 : 0;
			const uint8 Cap = High != 0 ? 200 : 100;
			S.SetWord(R + Record::MaxHp, GrowHp(S, Id, S.Word(R + Record::MaxHp), Level));
			for (const auto& [Field, Target] : {std::pair{Record::Attack, 0}, std::pair{Record::Defense, 1}, std::pair{Record::Hit, 2}, std::pair{Record::Evade, 3}})
			{
				S.Byte(R + Field) = GrowStat(S, S.Byte(R + Field), S.GrowthByte(Id, Growth::StatTargets + 2 * static_cast<std::size_t>(Target) + High), Cap, Level);
			}
		}

		/** An ether level's EP, ether and ether defense, by the targets of the character the record names. */
		void GrowEther(FResultState& S, uint32 Character)
		{
			const std::size_t R = FResultState::Character(Character);
			const uint32 Id = S.Byte(R + Record::Id);
			const uint8 Level = S.Byte(R + Record::EtherLevel);
			const std::size_t High = Level > 99 ? 1 : 0;
			const uint8 Cap = High != 0 ? 200 : 100;
			S.SetWord(R + Record::MaxEp, GrowEp(S, Id, static_cast<uint8>(S.Word(R + Record::MaxEp)), Level));
			for (const auto& [Field, Target] : {std::pair{Record::Ether, 4}, std::pair{Record::EtherDefense, 5}})
			{
				S.Byte(R + Field) = GrowStat(S, S.Byte(R + Field), S.GrowthByte(Id, Growth::StatTargets + 2 * static_cast<std::size_t>(Target) + High), Cap, Level);
			}
		}

		/** The EXP and ether EXP a character takes: a reserve three quarters of a third, a member its share, split by its record's shares unless the normal rules share both whole. */
		void SplitExp(FResultState& S, uint32 Id, uint32 Share, uint8 Slot, bool bReserve, uint32& Exp, uint32& EtherExp)
		{
			FBattleResultWork& Result = S.Context.Result;
			if (bReserve)
			{
				Exp = Share * 3 >> 2;
				EtherExp = Exp;
				return;
			}
			const FBattleCombatant& Member = S.Context.Combatants[Slot];
			const uint32 A = Member.ExpShare < 2 ? 1u : Member.ExpShare;
			const uint32 B = Member.EtherExpShare < 2 ? 1u : Member.EtherExpShare;
			Exp = Share * A / (A + B);
			EtherExp = Share * B / (A + B);
			if ((S.Story() & NormalExpRules) != 0)
			{
				Exp = Share;
				EtherExp = Share;
			}
			Exp = Exp == 0 ? 1 : Exp;
			EtherExp = EtherExp == 0 ? 1 : EtherExp;
			const uint16 Equipment = S.Word(FResultState::Character(Id) + Record::EquipmentFlags);
			if ((Equipment & DoubleExpEquipment) != 0)
			{
				Exp += Exp >> 1;
				EtherExp += EtherExp >> 1;
			}
			if ((Equipment & EtherExpEquipment) != 0)
			{
				EtherExp += EtherExp >> 1;
			}
			Result.ExpGains[Slot] = Exp;
			Result.EtherExpGains[Slot] = EtherExp;
		}

		/** One pool's EXP taken and its levels gained, each growing its stats. */
		void GainLevels(FResultState& S, uint32 Id, std::size_t LevelField, std::size_t ToNextField, uint32 Gain, void (*Grow)(FResultState&, uint32))
		{
			const std::size_t R = FResultState::Character(Id);
			uint32 Need = S.Long(R + ToNextField) - Gain;
			while (static_cast<int32>(Need) < 1 && S.Context.Fault == EBattleFault::None)
			{
				uint8& Level = S.Byte(R + LevelField);
				Level = static_cast<uint8>(Level + 1);
				if (Level > MaxLevel && (S.Story() & NormalExpRules) != 0)
				{
					Level = static_cast<uint8>(Level - 1);
				}
				S.SetLong(R + ToNextField, S.ExpToNext(Level));
				Grow(S, Id);
				Need += S.Long(R + ToNextField);
			}
			S.SetLong(R + ToNextField, Need);
		}

		void AddExp(FResultState& S, uint32 Id, uint32 Exp, uint32 EtherExp)
		{
			const std::size_t R = FResultState::Character(Id);
			S.SetLong(R + Record::EtherExp, S.Long(R + Record::EtherExp) + EtherExp);
			S.SetLong(R + Record::Exp, S.Long(R + Record::Exp) + Exp);
			if (S.Byte(R + Record::Level) == MaxLevel)
			{
				Exp = 0;
			}
			if (S.Byte(R + Record::EtherLevel) == MaxLevel)
			{
				EtherExp = 0;
			}
			GainLevels(S, Id, Record::Level, Record::ExpToNext, Exp, GrowPhysical);
			GainLevels(S, Id, Record::EtherLevel, Record::EtherExpToNext, EtherExp, GrowEther);
		}

		void GiveExp(FResultState& S)
		{
			FBattleContext& Context = S.Context;
			FBattleResultWork& Result = Context.Result;
			constexpr uint16 Reserve = 100;
			std::array<uint16, 12> Slots{};
			std::fill_n(Slots.begin(), FBattleGrowthTable::CharacterCount, Reserve);
			uint32 Absent = 0;
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				if (Context.PartyCharacters[Slot] == NoMember)
				{
					++Absent;
					continue;
				}
				const FBattleCombatant& Member = Context.Combatants[Slot];
				if (Member.Character >= Slots.size())
				{
					Context.Fault = EBattleFault::IndexOutsideTable;
					return;
				}
				if ((Member.Status & Down) == 0)
				{
					Slots[Member.Character] = Slot;
				}
				else
				{
					++Absent;
					Slots[Member.Character] = NoMember;
				}
				Result.StartLevels[Slot] = {Member.Level, Member.EtherLevel};
			}
			uint32 Total = Result.TotalExp;
			if (Result.ExpPenalty != 0)
			{
				Total = Total - (Total >> 2) * static_cast<uint32>(static_cast<int32>(Result.ExpPenalty));
			}
			for (uint32 Id = 0; Id < static_cast<uint32>(FBattleGrowthTable::CharacterCount) && Context.Fault == EBattleFault::None; ++Id)
			{
				if (Slots[Id] == NoMember)
				{
					continue;
				}
				const bool bReserve = static_cast<int16>(Slots[Id]) > 2;
				const uint32 Share = bReserve ? Total / 3 : ProcessorDivideUnsigned(Total, 3 - Absent);
				uint32 Exp = 0;
				uint32 EtherExp = 0;
				SplitExp(S, Id, Share, static_cast<uint8>(Slots[Id]), bReserve, Exp, EtherExp);
				AddExp(S, Id, Exp, EtherExp);
			}
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount && Context.Fault == EBattleFault::None; ++Slot)
			{
				if (Context.PartyCharacters[Slot] == NoMember)
				{
					continue;
				}
				const std::size_t R = FResultState::Character(Context.PartyCharacters[Slot]);
				Result.LevelsGained[Slot] = {static_cast<uint8>(S.Byte(R + Record::Level) - Result.StartLevels[Slot][0]),
					static_cast<uint8>(S.Byte(R + Record::EtherLevel) - Result.StartLevels[Slot][1])};
				std::array<uint8, FBattleResultWork::StatCount>& Stats = Result.NewStats[Slot];
				Stats[0] = S.Byte(R + Record::Id) == Billy ? static_cast<uint8>(S.Byte(R + Record::WeaponAttack) + S.Byte(R + BillySecondGunAttack))
														   : static_cast<uint8>(S.Byte(R + Record::Attack) + S.Byte(R + Record::WeaponAttack));
				Stats[1] = S.Byte(R + Record::Hit);
				Stats[2] = static_cast<uint8>(S.Byte(R + Record::Defense) + S.Byte(R + Record::ArmorDefense));
				Stats[3] = S.Byte(R + Record::Evade);
				Stats[4] = S.Byte(R + Record::Ether);
				Stats[5] = S.Byte(R + Record::EtherDefense);
				Stats[6] = S.Byte(R + Record::Agility);
			}
		}

		/** The first Deathblow not yet learned whose level and every threshold the member's counters reach; none past the first too high. */
		uint8 LearnDeathblow(FResultState& S, uint32 Id, const FBattleCombatant& Member)
		{
			const uint32 Limit = (S.Story() & LateTier) != 0 ? 13 : 7;
			const std::size_t Skills = FResultState::Skills(Id, GameStateLayout::Skills::Deathblows);
			for (uint32 Number = 0; Number < Limit; ++Number)
			{
				const uint16 Bit = static_cast<uint16>(0x8000u >> Number);
				if ((S.Word(Skills) & Bit) != 0)
				{
					continue;
				}
				if (S.Byte(FResultState::Character(Id) + Record::Level) < S.GrowthByte(Id, Growth::DeathblowLevels + Number))
				{
					return 0;
				}
				bool bReached = true;
				for (std::size_t Counter = 0; Counter < Member.DeathblowCounters.size(); ++Counter)
				{
					if (Member.DeathblowCounters[Counter] < S.GrowthWord(Id, Growth::Thresholds + 14 * Number + 2 * Counter))
					{
						bReached = false;
						break;
					}
				}
				if (bReached)
				{
					S.SetWord(Skills, static_cast<uint16>(S.Word(Skills) | Bit));
					return static_cast<uint8>(Number);
				}
			}
			return 0;
		}

		/** The first ability not yet learned whose level is reached, as its number + 1. */
		uint8 LearnAbility(FResultState& S, uint32 Id)
		{
			const std::size_t Skills = FResultState::Skills(Id, GameStateLayout::Skills::Abilities);
			for (uint32 Number = 0; Number < 12; ++Number)
			{
				const uint8 Level = S.GrowthByte(Id, Growth::AbilityLevels + Number);
				if (Level == 0xFF)
				{
					return 0;
				}
				const uint16 Bit = static_cast<uint16>(0x8000u >> Number);
				if (Level <= S.Byte(FResultState::Character(Id) + Record::Level) && (S.Word(Skills) & Bit) == 0)
				{
					S.SetWord(Skills, static_cast<uint16>(S.Word(Skills) | Bit));
					return static_cast<uint8>(Number + 1);
				}
			}
			return 0;
		}

		/** Each command a learned skill unlocks: Sources lists the skill (1-based) per command, ended by End. */
		void Unlock(FResultState& S, uint32 Id, std::size_t Sources, uint32 Count, uint8 End, std::size_t From, std::size_t Into, uint32 Shift)
		{
			for (uint32 Number = 0; Number < Count; ++Number)
			{
				const uint8 Source = S.GrowthByte(Id, Sources + Number);
				if (Source == End)
				{
					return;
				}
				if ((S.Word(FResultState::Skills(Id, From)) & (0x8000u >> ((Source - 1u) & 31))) != 0)
				{
					const std::size_t At = FResultState::Skills(Id, Into);
					S.SetWord(At, static_cast<uint16>(S.Word(At) | (0x8000u >> ((Number + Shift) & 31))));
				}
			}
		}

		/** Maria's gear Deathblows come by level. */
		void UnlockMariaCommands(FResultState& S)
		{
			const std::size_t At = FResultState::Skills(Maria, GameStateLayout::Skills::GearDeathblows);
			for (uint32 Number = 0; Number < 9; ++Number)
			{
				const uint8 Level = S.GrowthByte(Maria, Growth::GearDeathblowSources + Number);
				if (Level == 0xFF)
				{
					return;
				}
				if (Level <= S.Byte(FResultState::Character(Maria) + Record::Level))
				{
					const uint16 Bit = static_cast<uint16>(0x1000u >> Number);
					if ((S.Word(At) & Bit) == 0)
					{
						S.SetWord(At, static_cast<uint16>(S.Word(At) | Bit));
					}
				}
			}
		}

		/** Chu-Chu's giant form from her figures in the battle. */
		void RefreshChuChuGiant(FResultState& S, const FBattleCombatant& Member)
		{
			const std::size_t Gear = GameStateLayout::GearOffset(ChuChu);
			S.SetLong(Gear + GameStateLayout::Gear::MaxHp, static_cast<uint32>(Member.MaxHp) * 200u);
			S.Byte(Gear + 0x3C) = static_cast<uint8>(Member.Attack / 5 + 1);
			S.SetWord(Gear + 0x70, static_cast<uint16>(Member.MaxHp * 10));
			S.SetWord(Gear + 0x72, static_cast<uint16>(Member.MaxHp * 10));
		}

		void LearnSkills(FResultState& S)
		{
			FBattleContext& Context = S.Context;
			FBattleResultWork& Result = Context.Result;
			using GameStateLayout::Skills::Abilities;
			using GameStateLayout::Skills::Deathblows;
			using GameStateLayout::Skills::GearAbilities;
			using GameStateLayout::Skills::GearDeathblows;
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount && Context.Fault == EBattleFault::None; ++Slot)
			{
				const FBattleCombatant& Member = Context.Combatants[Slot];
				if ((Member.Status & Dead) != 0)
				{
					continue;
				}
				const uint8 Id = Member.Character;
				if (Id > Maria || Id < ChuChu)
				{
					if (const uint8 Learned = LearnDeathblow(S, Id, Member); Learned != 0)
					{
						Result.LearnedDeathblows[Slot] = Learned;
					}
				}
				if (Id != 10)
				{
					if (const uint8 Learned = LearnAbility(S, Id); Learned != 0)
					{
						Result.LearnedAbilities[Slot] = Learned;
					}
				}
				if (Id < ChuChu || (Id > Maria && Id != 10))
				{
					Unlock(S, Id, Growth::GearDeathblowSources, 9, 0xFF, Deathblows, GearDeathblows, 3);
				}
				if (Id > 10 || Id < Maria)
				{
					Unlock(S, Id, Growth::GearCommandSources, 13, 0, Abilities, GearAbilities, 0);
				}
				if (Id == Maria)
				{
					UnlockMariaCommands(S);
				}
				if (Id == ChuChu)
				{
					RefreshChuChuGiant(S, Member);
				}
			}
		}

		/** Each character's AP per turn rises a step at most, at its levels (the seventh with the late tier). */
		void RaiseMaxAp(FResultState& S)
		{
			for (uint32 Id = 0; Id < static_cast<uint32>(FBattleGrowthTable::CharacterCount); ++Id)
			{
				uint8& Ap = S.Byte(FResultState::Skills(Id, GameStateLayout::Skills::ApPerTurn));
				const uint8 Level = S.Byte(FResultState::Character(Id) + Record::Level);
				if (Ap >= 3 && Ap <= 5)
				{
					if (S.GrowthByte(Id, Growth::ApLevels + (Ap - 3u)) <= Level)
					{
						Ap = static_cast<uint8>(Ap + 1);
					}
				}
				else if (Ap == 6 && Level > 49 && (S.Story() & LateTier) != 0)
				{
					Ap = 7;
				}
			}
		}

		/** Levels 50, 60 and 70 unlock the three Infinity attacks of the members' characters. */
		void SetLevelMilestones(FResultState& S)
		{
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				const uint8 Id = S.Context.Combatants[Slot].Character;
				const uint8 Level = S.Byte(FResultState::Character(Id) + Record::Level);
				const std::size_t At = FResultState::Skills(Id, GameStateLayout::Skills::GearDeathblows);
				for (const auto& [Above, Bit] : {std::pair{49, 8}, std::pair{59, 4}, std::pair{69, 2}})
				{
					if (Level > Above)
					{
						S.SetWord(At, static_cast<uint16>(S.Word(At) | Bit));
					}
				}
			}
		}

		/** The members' HP, EP, counters and gears back into the game state (Chu-Chu's giant form's HP a fiftieth). */
		void WriteBackParty(FResultState& S)
		{
			using namespace GameStateLayout;
			FBattleContext& Context = S.Context;
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				if (Context.PartyCharacters[Slot] == NoMember)
				{
					continue;
				}
				FBattleCombatant& Member = Context.Combatants[Slot];
				const std::size_t R = FResultState::Character(Member.Character);
				if (Member.Character == ChuChu && Member.IsInGear())
				{
					Member.Hp = static_cast<uint16>((Member.GearPart.Hp + 1) / 50);
					if (Member.Hp == 0)
					{
						Member.Hp = 1;
					}
				}
				S.SetWord(R + Character::Hp, Member.Hp);
				S.SetWord(R + Character::Ep, Member.Ep);
				if (S.Word(R + Character::MaxHp) < S.Word(R + Character::Hp))
				{
					S.SetWord(R + Character::Hp, S.Word(R + Character::MaxHp));
				}
				if (S.Word(R + Character::MaxEp) < S.Word(R + Character::Ep))
				{
					S.SetWord(R + Character::Ep, S.Word(R + Character::MaxEp));
				}
				for (std::size_t Counter = 0; Counter < Member.DeathblowCounters.size(); ++Counter)
				{
					S.SetWord(R + Character::DeathblowCounters + 2 * Counter, Member.DeathblowCounters[Counter]);
				}
				S.SetWord(R + Character::ExactKills, Member.ExactKills);
				if ((Member.Status & Down) != 0)
				{
					S.SetWord(R + Character::Hp, 1);
				}
				const uint8 GearId = Member.Gear;
				if (GearId < ChuChu || (GearId > ChuChu && GearId < 17))
				{
					const std::size_t G = GearOffset(GearId);
					S.SetLong(G + Gear::Hp, Member.GearPart.Hp);
					S.SetWord(G + Gear::Fuel, Member.GearPart.Fuel);
					if (S.Long(G + Gear::MaxHp) < S.Long(G + Gear::Hp))
					{
						S.SetLong(G + Gear::Hp, S.Long(G + Gear::MaxHp));
					}
					if (S.Word(G + Gear::MaxFuel) < S.Word(G + Gear::Fuel))
					{
						S.SetWord(G + Gear::Fuel, S.Word(G + Gear::MaxFuel));
					}
					if ((Member.GearPart.Status & Dead) != 0)
					{
						S.SetLong(G + Gear::Hp, S.Long(G + Gear::MaxHp) / 10);
					}
				}
			}
		}

		/** Each defeated enemy drops its first item by its chance (always with a member's equipment asking), else its second by its. */
		void RollDrops(FResultState& S)
		{
			FBattleContext& Context = S.Context;
			FBattleResultWork& Result = Context.Result;
			bool bAlways = false;
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				bAlways = bAlways || (Context.Combatants[Slot].EquipmentFlags & AlwaysDropEquipment) != 0;
			}
			for (uint8 Enemy = 0; Enemy < FBattleResultWork::EnemyCount; ++Enemy)
			{
				Result.DropIds[Enemy] = 0;
				if ((Result.Defeated & (1u << Enemy)) == 0)
				{
					continue;
				}
				const FBattleCombatant& Foe = Context.Combatants[FBattleContext::PartySlotCount + Enemy];
				if (S.Percent() < Foe.DropChances[0] || bAlways)
				{
					Result.DropKinds[Enemy] = Foe.DropKinds[0];
					Result.DropIds[Enemy] = Foe.Drops[0];
				}
				else if (S.Percent() < Foe.DropChances[1])
				{
					Result.DropKinds[Enemy] = Foe.DropKinds[1];
					Result.DropIds[Enemy] = Foe.Drops[1];
				}
			}
		}

		void ApplyGrowth(FResultState& S)
		{
			FBattleContext& Context = S.Context;
			int32 Fallen = 0;
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				Fallen += (Context.Combatants[Slot].Status & Dead) != 0 ? 1 : 0;
			}
			if (Fallen == FBattleContext::PartySlotCount)
			{
				return;
			}
			GiveExp(S);
			if (Context.Fault == EBattleFault::None)
			{
				LearnSkills(S);
			}
			if (Context.Fault == EBattleFault::None)
			{
				RaiseMaxAp(S);
				SetLevelMilestones(S);
				WriteBackParty(S);
			}
			if (Context.Fault != EBattleFault::None)
			{
				return;
			}
			RollDrops(S);
			if (S.Byte(FResultState::Character(Bart) + GameStateLayout::Character::Gear) == Yggdrasil)
			{
				const bool bRepair = S.Byte(GameStateLayout::GearOffset(YggdrasilRepairGear) + 0x4F) != 0;
				S.SetWord(FResultState::Skills(Bart, GameStateLayout::Skills::Special), bRepair ? uint16{0xC000} : uint16{0x4000});
			}
		}

		/** An inventory list's count of the id raised (to 99 at most), or the first empty slot given it; a full list loses it. */
		void AddToInventory(FResultState& S, const GameStateLayout::FInventoryList& List, uint8 Id, uint8 Count)
		{
			for (std::size_t Slot = 0; Slot < List.Slots; ++Slot)
			{
				if (S.Byte(List.Items + Slot) == Id)
				{
					uint8& Held = S.Byte(List.Counts + Slot);
					Held = static_cast<uint32>(Held) + Count < 100 ? static_cast<uint8>(Held + Count) : uint8{99};
					return;
				}
			}
			for (std::size_t Slot = 0; Slot < List.Slots; ++Slot)
			{
				if (S.Byte(List.Items + Slot) == 0)
				{
					S.Byte(List.Items + Slot) = Id;
					S.Byte(List.Counts + Slot) = Count;
					return;
				}
			}
		}
	}

	TResult<FBattleGrowthTable> FBattleGrowthTable::Parse(std::vector<uint8> Section)
	{
		if (Section.size() < ExpTableOffset + 4)
		{
			return MakeError("the growth table is too short");
		}
		FBattleGrowthTable Table;
		Table.Data = std::move(Section);
		return Table;
	}

	TResult<FBattleGrowthTable> FBattleGrowthTable::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(GrowthDirectory, GrowthFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the growth table's file");
		}
		const TResult<std::vector<uint8>> File = ReadFileSectors(Image, *Record);
		if (!File)
		{
			return File.Error();
		}
		TResult<std::vector<uint8>> Section = UnpackContainerSection(*File, GrowthSection);
		if (!Section)
		{
			return Section.Error();
		}
		return Parse(std::move(*Section));
	}

	void WriteBackBattleItems(FBattleContext& Context)
	{
		const GameStateLayout::FInventoryList& Items = GameStateLayout::InventoryLists[0];
		FGameState& State = *Context.GameState;
		for (std::size_t Listed = 0; Listed < FBattleContext::ListedItemCount; ++Listed)
		{
			const uint8 Item = Context.ListedItemsAtStart[Listed];
			if (Item == 0)
			{
				continue;
			}
			for (std::size_t Slot = 0; Slot < Items.Slots; ++Slot)
			{
				if (BattleGameState::Byte(State, Items.Items + Slot) == Item)
				{
					BattleGameState::Byte(State, Items.Counts + Slot) = Context.ListedCounts[Listed];
				}
			}
		}
	}

	bool GiveBattleRewards(FBattleContext& Context, const FBattleGrowthTable& Growth)
	{
		FResultState S(Context, Growth.Bytes());
		FBattleResultWork& Result = Context.Result;
		Result.TotalExp = 0;
		Result.Defeated = 0;
		uint32 Gold = 0;
		if (!Context.Event.Requests.bEndedBattle)
		{
			for (uint8 Enemy = 0; Enemy < FBattleResultWork::EnemyCount; ++Enemy)
			{
				const uint8 Slot = static_cast<uint8>(FBattleContext::PartySlotCount + Enemy);
				const FBattleCombatant& Foe = Context.Combatants[Slot];
				if (Context.InBattle[Slot] && Context.Slots[Slot].Presence == 0 && (Foe.Status & Dead) != 0 && !Context.Escaped[Enemy])
				{
					Result.TotalExp += Foe.ExpReward;
					Gold += Foe.GoldReward;
					Result.Defeated = static_cast<uint16>(Result.Defeated | BattleRules::GameSlotBit(Context, Enemy));
				}
			}
			Result.TotalExp = RewardAt(Result.TotalExp, Context.Balance.Experience);
			Gold = RewardAt(Gold, Context.Balance.Gold);
			FGameState& State = *Context.GameState;
			State.SetGold(std::min(State.Gold() + Gold, GoldCap));
			if (Context.Show != nullptr)
			{
				Context.Show->SetTargetMarkers(0);
			}
		}
		Result.GoldGained = Gold;
		for (uint8& Character : Context.PartyCharacters)
		{
			if (Character == EmptyOccupant)
			{
				Character = NoMember;
			}
		}
		if (Context.bSpecialParty)
		{
			Context.PartyCharacters[1] = NoMember;
			Context.PartyCharacters[2] = NoMember;
			Context.Slots[1].Occupant = EmptyOccupant;
			Context.Slots[2].Occupant = EmptyOccupant;
		}
		ApplyGrowth(S);
		return !Context.Event.Requests.bNoResults && (Context.Formation.Flags & FormationFlags::NoResultScreen) == 0;
	}

	void GiveBattleDrops(FBattleContext& Context)
	{
		FBattleResultWork& Result = Context.Result;
		for (FBattleDropGroup& Group : Result.DropGroups)
		{
			Group.Id = 0;
			Group.Count = 0;
		}
		std::size_t Next = 0;
		for (std::size_t Enemy = 0; Enemy < FBattleResultWork::EnemyCount; ++Enemy)
		{
			const uint8 Id = Result.DropIds[Enemy];
			if (Id == 0)
			{
				continue;
			}
			const auto Same = std::find_if(Result.DropGroups.begin(), Result.DropGroups.end(),
				[&](const FBattleDropGroup& Group) { return Group.Kind == Result.DropKinds[Enemy] && Group.Id == Id; });
			if (Same != Result.DropGroups.end())
			{
				++Same->Count;
			}
			else
			{
				Result.DropGroups[Next] = {Result.DropKinds[Enemy], Id, 1};
				++Next;
			}
		}
		FResultState S(Context, {});
		using GameStateLayout::InventoryLists;
		for (const FBattleDropGroup& Group : Result.DropGroups)
		{
			if (Group.Id == 0)
			{
				continue;
			}
			switch (Group.Kind)
			{
			case BattleDropKinds::Weapon: AddToInventory(S, InventoryLists[1], Group.Id, Group.Count); break;
			case BattleDropKinds::Accessory: AddToInventory(S, InventoryLists[2], Group.Id, Group.Count); break;
			case BattleDropKinds::Item: AddToInventory(S, InventoryLists[0], Group.Id, Group.Count); break;
			case BattleDropKinds::GearWeapon: AddToInventory(S, InventoryLists[3], Group.Id, Group.Count); break;
			case BattleDropKinds::GearPart: AddToInventory(S, InventoryLists[4], Group.Id, Group.Count); break;
			default: break;
			}
		}
	}

	bool FinishBattleResults(FBattleContext& Context, const FBattleGrowthTable& Growth, uint8 ModeResult)
	{
		WriteBackBattleItems(Context);
		return BattleGivesRewards(Context, ModeResult) && GiveBattleRewards(Context, Growth);
	}
}
