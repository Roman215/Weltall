
#include "Battle/BattleAiMemory.h"
#include "Battle/BattleAiSteps.h"
#include "Battle/BattleProgram.h"
#include "Battle/BattleRules.h"
#include "Battle/BattleSteps.h"
#include "Battle/BattleWaits.h"
#include "Foundation/ProcessorDivide.h"
#include "Game/GameState.h"

#include <algorithm>

namespace Weltall
{
	namespace BattleAiSteps
	{
		namespace
		{
			using namespace BattleAiMemory;

			constexpr std::size_t EntrySize = 8;
			/** Where the words and the bytes begin among a record's variables. */
			constexpr std::size_t WordVariables = 0x10;
			constexpr std::size_t ByteVariables = 0x20;
			/** The type a guarded write gives the queue's first entry, and the one a raw command queues: both bad types. */
			constexpr uint8 PoisonedType = 0x20;
			constexpr uint8 RawType = 0x80;
			constexpr uint8 NoSlot = FBattleContext::SlotCount;

			/**
			 * What an empty mask's slot names as a combatant's gear HP and maximum: the game's combatants are followed
			 * by the party's command tables, and those two long words lie in the fourth command of the first member's.
			 */
			FBattleCommand& PastCombatants(FBattleContext& Context)
			{
				return Context.Commands[FBattleContext::PartyCommandIndex(0, 3)];
			}

			/** The last boss's second form: scene 63's enemy type 1, whose first turn sets its HP through a mask it never set. */
			constexpr uint8 DeusScene = 63;
			constexpr uint8 DeusSecondForm = 1;

			/** Whether a write through an empty mask is Deus's second form setting its own HP, as `bDeusSecondFormHp` takes it. */
			bool IsDeusOwnHp(const FBattleContext& Context, uint8 Slot, uint8 Field)
			{
				return Context.RuleOptions.bDeusSecondFormHp && Field == 1 && Context.Formation.Scene == DeusScene
					&& Context.Slots[Slot].Occupant == DeusSecondForm;
			}

			uint8 SaturatedByte(int32 Value)
			{
				return static_cast<uint8>(Value > 0xFF ? 0xFF : (Value < 0 ? 0 : Value));
			}

			uint16 SaturatedWord(int32 Value)
			{
				return static_cast<uint16>(Value > 0xFFFF ? 0xFFFF : (Value < 0 ? 0 : Value));
			}

			/** A byte product as the game saturates it: tested as a signed 16-bit number, so the largest keep their low byte. */
			uint8 ByteProduct(uint8 X, uint8 Y)
			{
				const int16 Product = static_cast<int16>(static_cast<uint16>(X * Y));
				return static_cast<uint8>(Product > 0xFF ? 0xFF : Product);
			}

			/** A word product as the game saturates it: tested as a signed 32-bit number. */
			uint16 WordProduct(uint16 X, uint16 Y)
			{
				const int32 Product = static_cast<int32>(static_cast<uint32>(X) * Y);
				return static_cast<uint16>(Product > 0xFFFF ? 0xFFFF : Product);
			}

			/** The byte operations of 08h-0Fh and 18h-1Fh by their order: add, subtract, multiply, divide, remainder, and, or, xor. */
			uint8 ByteOperation(int32 Operation, uint8 X, uint8 Y)
			{
				switch (Operation)
				{
				case 0: return SaturatedByte(X + Y);
				case 1: return SaturatedByte(X - Y);
				case 2: return ByteProduct(X, Y);
				case 3: return static_cast<uint8>(ProcessorDivideUnsigned(X, Y));
				case 4: return static_cast<uint8>(ProcessorRemainderUnsigned(X, Y));
				case 5: return static_cast<uint8>(X & Y);
				case 6: return static_cast<uint8>(X | Y);
				default: return static_cast<uint8>(X ^ Y);
				}
			}

			uint16 WordOperation(int32 Operation, uint16 X, uint16 Y)
			{
				switch (Operation)
				{
				case 0: return SaturatedWord(X + Y);
				case 1: return SaturatedWord(X - Y);
				case 2: return WordProduct(X, Y);
				case 3: return static_cast<uint16>(ProcessorDivideUnsigned(X, Y));
				case 4: return static_cast<uint16>(ProcessorRemainderUnsigned(X, Y));
				case 5: return static_cast<uint16>(X & Y);
				case 6: return static_cast<uint16>(X | Y);
				default: return static_cast<uint16>(X ^ Y);
				}
			}

			uint8 Self(uint8 Enemy)
			{
				return static_cast<uint8>(Enemy + FBattleContext::PartySlotCount);
			}

			/** A write to a combatant the scripts guard: a party slot poisons the queue's first entry instead. */
			bool GuardsParty(FBattleContext& Context, uint8 Slot, uint8& Next)
			{
				if (Slot < FBattleContext::PartySlotCount)
				{
					Context.AiQueue[0] = PoisonedType;
					Next = static_cast<uint8>(Next + 1);
					return true;
				}
				return false;
			}

			uint8 QueueCommand(FBattleContext& Context, uint8 Enemy, const FWord& Word, uint8 Next)
			{
				const std::size_t Entry = static_cast<std::size_t>(Next) * EntrySize;
				switch (Word.Op)
				{
				case 0x01:
					SetQueueByte(Context, Entry + Word.A, Word.B);
					if (Context.RuleOptions.bDeusKeepsCamera || Context.bPerfectWorksRebalance || (Context.Program != nullptr && Context.Program->bQueueKeepsCamera))
					{
						Context.KeepCamera = Word.C;
					}
					return Word.A == 0 ? static_cast<uint8>(Next + 1) : Next;
				case 0x02:
					SetQueueByte(Context, Entry + Word.A, B(Context, Enemy, Word.B));
					return Next;
				case 0x03:
					for (std::size_t Byte = 0; Byte < EntrySize; ++Byte)
					{
						SetQueueByte(Context, Word.B * EntrySize + Byte, QueueByte(Context, Word.A * EntrySize + Byte));
					}
					return Next;
				case 0x3D:
					SetQueueByte(Context, Entry + Word.A, Word.B);
					SetQueueByte(Context, Entry + Word.A + 1, Word.C);
					return Next;
				case 0x52:
				{
					const uint16 Value = W(Context, Enemy, Word.B);
					SetQueueByte(Context, Entry + Word.A, static_cast<uint8>(Value));
					SetQueueByte(Context, Entry + Word.A + 1, static_cast<uint8>(Value >> 8));
					return Next;
				}
				default:
					SetQueueByte(Context, Entry, RawType);
					SetQueueByte(Context, Entry + 1, Word.Op);
					SetQueueByte(Context, Entry + 2, Word.A);
					SetQueueByte(Context, Entry + 3, Word.B);
					SetQueueByte(Context, Entry + 4, Word.C);
					return static_cast<uint8>(Next + 1);
				}
			}

			/** The variable commands of 04h-3Fh but the queue's: sets, arithmetic, fields, globals, copies, rewards, random numbers. */
			uint8 VariableCommand(FBattleContext& Context, uint8 Enemy, const FWord& Word, uint8 Next)
			{
				const uint8 Op = Word.Op;
				if (Op >= 0x08 && Op <= 0x0F)
				{
					SetB(Context, Enemy, Word.A, ByteOperation(Op - 0x08, B(Context, Enemy, Word.A), Word.B));
					return Next;
				}
				if (Op >= 0x10 && Op <= 0x17)
				{
					SetW(Context, Enemy, Word.A, WordOperation(Op - 0x10, W(Context, Enemy, Word.A), Word.Immediate()));
					return Next;
				}
				if (Op >= 0x18 && Op <= 0x1F)
				{
					const uint8 X = B(Context, Enemy, Word.A);
					const uint8 Y = B(Context, Enemy, Word.B);
					SetB(Context, Enemy, Word.C, ByteOperation(Op - 0x18, X, Y));
					return Next;
				}
				if (Op >= 0x20 && Op <= 0x27)
				{
					const uint16 X = W(Context, Enemy, Word.A);
					const uint16 Y = W(Context, Enemy, Word.B);
					SetW(Context, Enemy, Word.C, WordOperation(Op - 0x20, X, Y));
					return Next;
				}
				FBattleCombatant& Own = Context.Combatants[Self(Enemy)];
				switch (Op)
				{
				case 0x04: SetB(Context, Enemy, Word.A, Word.B); break;
				case 0x05: SetW(Context, Enemy, Word.A, Word.Immediate()); break;
				case 0x06: SetL(Context, Enemy, Word.A, static_cast<uint32>(Word.Immediate()) * 0x10); break;
				case 0x07: SetGlobal(Context, Word.A, Word.B); break;
				case 0x28: SetByteField(Context, Self(Enemy), Word.A, Word.B); break;
				case 0x29: SetWordField(Context, Self(Enemy), Word.A, Word.Immediate()); break;
				case 0x2A: SetB(Context, Enemy, Word.A, ByteField(Context, FirstSlot(W(Context, Enemy, Word.C)), Word.B)); break;
				case 0x2B:
				{
					const uint8 Slot = FirstSlot(W(Context, Enemy, Word.C));
					if (!GuardsParty(Context, Slot, Next))
					{
						SetByteField(Context, Slot, Word.B, B(Context, Enemy, Word.A));
					}
					break;
				}
				case 0x2C: SetW(Context, Enemy, Word.A, WordField(Context, FirstSlot(W(Context, Enemy, Word.C)), Word.B)); break;
				case 0x2D:
				{
					const uint8 Slot = FirstSlot(W(Context, Enemy, Word.C));
					if (!GuardsParty(Context, Slot, Next))
					{
						SetWordField(Context, Slot, Word.B, W(Context, Enemy, Word.A));
					}
					break;
				}
				case 0x2E:
					if (Word.B == 1)
					{
						const uint8 Slot = FirstSlot(W(Context, Enemy, Word.C));
						SetL(Context, Enemy, Word.A, Slot < NoSlot ? Context.Combatants[Slot].GearPart.Hp : PastCombatants(Context).Bytes04);
					}
					else if (Word.B == 2)
					{
						SetL(Context, Enemy, Word.A, Context.GameState->Gold());
					}
					break;
				case 0x2F:
				{
					const uint8 Slot = FirstSlot(W(Context, Enemy, Word.C));
					if (GuardsParty(Context, Slot, Next))
					{
						break;
					}
					const uint32 Value = L(Context, Enemy, Word.A);
					if (Slot >= NoSlot && IsDeusOwnHp(Context, Self(Enemy), Word.B))
					{
						Own.GearPart.Hp = Value;
						break;
					}
					if (Slot >= NoSlot)
					{
						FBattleCommand& Command = PastCombatants(Context);
						if (Word.B == 0)
						{
							Command.DamageKind = static_cast<uint16>(Value);
							Command.Flags = static_cast<uint16>(Value >> 16);
						}
						else
						{
							Command.Bytes04 = Value;
						}
						break;
					}
					FBattleGearPart& Gear = Context.Combatants[Slot].GearPart;
					(Word.B == 0 ? Gear.MaxHp : Gear.Hp) = Value;
					break;
				}
				case 0x30: SetW(Context, Enemy, Word.A, Global(Context, Word.B)); break;
				case 0x31: SetGlobal(Context, Word.B, W(Context, Enemy, Word.A)); break;
				case 0x32: SetB(Context, Enemy, Word.B, B(Context, Enemy, Word.A)); break;
				case 0x33: SetW(Context, Enemy, Word.B, W(Context, Enemy, Word.A)); break;
				case 0x34: SetL(Context, Enemy, Word.B, L(Context, Enemy, Word.A)); break;
				case 0x35: SetW(Context, Enemy, Word.B, B(Context, Enemy, Word.A)); break;
				case 0x36: SetL(Context, Enemy, Word.B, W(Context, Enemy, Word.A)); break;
				case 0x37: std::fill_n(Context.AiRecords[Enemy].Variables.begin() + ByteVariables, 16, uint8{0}); break;
				case 0x38: std::fill_n(Context.AiRecords[Enemy].Variables.begin() + WordVariables, 16, uint8{0}); break;
				case 0x39: Own.ExpReward = static_cast<uint32>(Word.A | Word.B << 8); break;
				case 0x3A: Own.GoldReward = static_cast<uint16>(Word.A | Word.B << 8); break;
				case 0x3B:
					Own.DropKinds[1] = Word.A;
					Own.Drops[1] = Word.B;
					Own.DropChances[1] = Word.C;
					break;
				case 0x3C:
					Own.DropKinds[0] = Word.A;
					Own.Drops[0] = Word.B;
					Own.DropChances[0] = Word.C;
					break;
				case 0x3E: SetB(Context, Enemy, Word.A, RandomByteBetween(*Context.Random, 0, Word.B)); break;
				case 0x3F: SetW(Context, Enemy, Word.A, BattleRules::RandomU16Between(*Context.Random, 0, Word.Immediate())); break;
				default: break;
				}
				return Next;
			}

			/** 63h and 69h-74h: showing or hiding the enemy, its result, long arithmetic, the event's variables, the party's commands, routes, the next actor, the waits. */
			void BattleCommand(FBattleContext& Context, uint8 Enemy, const FWord& Word)
			{
				const uint8 Slot = Self(Enemy);
				switch (Word.Op)
				{
				case 0x63:
					Context.Slots[Slot].Presence = Word.A;
					if ((Word.A & 0x80) != 0)
					{
						BattleSteps::MoveToTargetGroup(Context, Slot, Slot);
					}
					else if (Word.A != 0 || !Context.RuleOptions.bRevealKeepsGroup)
					{
						// Gone from the battle; the original takes a revealed enemy (0) out of its group too.
						BattleSteps::LeaveGroup(Context, Slot);
					}
					break;
				case 0x69:
					Context.Action.Amounts[Slot] = 0;
					Context.Action.Results[Slot] = BattleResults::Miss;
					break;
				case 0x6A: SetL(Context, Enemy, Word.C, L(Context, Enemy, Word.A) + L(Context, Enemy, Word.B)); break;
				case 0x6B: SetL(Context, Enemy, Word.C, L(Context, Enemy, Word.A) - L(Context, Enemy, Word.B)); break;
				case 0x6C: SetL(Context, Enemy, Word.A, Word.B * L(Context, Enemy, Word.A)); break;
				case 0x6D: SetL(Context, Enemy, Word.A, ProcessorDivideUnsigned(L(Context, Enemy, Word.A), Word.B)); break;
				case 0x70:
					if (!Context.bEventWork)
					{
						Context.Fault = EBattleFault::NoBattleEvent;
						break;
					}
					Context.EventVariables[Word.A] = Word.B;
					break;
				case 0x71:
				{
					const uint32 Command = static_cast<uint8>(Word.A + 7);
					if (Command >= 16)
					{
						Context.Fault = EBattleFault::IndexOutsideTable;
						break;
					}
					const uint16 Bit = static_cast<uint16>(0x8000u >> Command);
					for (std::size_t Member = 0; Member < FBattleContext::PartySlotCount; ++Member)
					{
						uint16& Disabled = Context.Combatants[Member].DisabledCommands;
						Disabled = Word.B == 0 ? static_cast<uint16>(Disabled & ~Bit) : static_cast<uint16>(Disabled | Bit);
					}
					break;
				}
				case 0x72:
					if (Word.A >= FBattleStage::GroupCount || Word.B >= FBattleStage::GroupCount)
					{
						Context.Fault = EBattleFault::GroupOutsideStage;
						break;
					}
					Context.Stage.Route(Word.A, Word.B, 0) = Word.C;
					break;
				case 0x73: Context.ForcedActor = static_cast<uint8>(FirstSlot(W(Context, Enemy, Word.A)) + 1); break;
				case 0x74: InitBattleWaits(Context); break;
				default: break;
				}
			}
		}

		uint8 RunCommand(FBattleContext& Context, uint8 Enemy, const FWord& Word, uint8 Next)
		{
			const uint8 Op = Word.Op;
			if (Op == 0x01 || Op == 0x02 || Op == 0x03 || Op == 0x3D || Op == 0x52)
			{
				return QueueCommand(Context, Enemy, Word, Next);
			}
			if (Op >= 0x04 && Op <= 0x3F)
			{
				return VariableCommand(Context, Enemy, Word, Next);
			}
			if (RunPicker(Context, Enemy, Word))
			{
				return Next;
			}
			if (Op == 0x62)
			{
				return Next;
			}
			if (Op == 0x63 || (Op >= 0x69 && Op <= 0x6D) || (Op >= 0x70 && Op <= 0x74))
			{
				BattleCommand(Context, Enemy, Word);
				return Next;
			}
			return QueueCommand(Context, Enemy, Word, Next);
		}
	}
}
