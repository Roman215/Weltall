#include "Battle/BattleEvent.h"

#include "Battle/BattleEventSteps.h"
#include "Battle/BattleRules.h"
#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/ProcessorDivide.h"
#include "Foundation/Sha1.h"
#include "Media/CdImage.h"
#include "Media/GameFileIndex.h"
#include "Text/MessageTableIds.h"

#include <algorithm>
#include <string_view>

namespace Weltall
{
	using namespace BattleEventSteps;

	namespace
	{
		constexpr int32 ArchiveDirectory = 32;
		constexpr int32 ArchiveFile = 2;
		/** The battle events' program beside the archive, and the French translation's version of it (its SHA-1). */
		constexpr int32 ProgramFile = 1;
		constexpr std::string_view FrenchProgram = "4965dd0b4a2e8af202cd764f09c97afadec886c3";

		/** Whether the disc's battle events' program is the French translation's. */
		bool HasFrenchProgram(const FCdImage& Image, const FGameFileIndex& Index)
		{
			const FIndexRecord* Record = Index.Find(ArchiveDirectory, ProgramFile);
			if (Record == nullptr)
			{
				return false;
			}
			const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
			return Sectors && Sectors->size() >= static_cast<std::size_t>(Record->Size)
				&& FSha1::ToHex(FSha1::Hash(std::span<const uint8>(*Sectors).first(static_cast<std::size_t>(Record->Size)))) == FrenchProgram;
		}
		constexpr uint16 NoPc = 0xFFFF;
		constexpr uint8 IdleScript = 1;
		constexpr uint8 IdlePriority = 7;
		constexpr uint8 InstructionsPerFrame = 4;
		constexpr uint8 ConfirmEvent = 4;
		constexpr uint8 NoEvent = 0xFF;
		constexpr uint8 EndOpcode = 0x00;
		constexpr std::size_t ArchivePadding = 0x800;

		/** What each instruction's handler adds to the pc: its length, SetOrder's 3 stepping over a byte after its operand. */
		constexpr std::array<uint8, BattleEventScript::OpcodeCount> Lengths = {
			1, 3, 8, 3, 3, 3, 6, 3, 3, 6, 6, 6, 6, 3, 3, 6,
			6, 6, 5, 5, 3, 5, 6, 6, 4, 5, 11, 2, 1, 1, 3, 3,
			1, 1, 1, 7, 5, 1, 9, 9, 6, 9, 5, 3, 3, 3, 3, 5,
			3, 9, 1, 1, 1, 5, 3, 1, 7, 1, 5, 3, 3, 3, 9, 9,
			3, 7, 1, 1, 1, 9, 7, 1, 5, 3, 1, 3,
		};

		enum ERunPc : uint8
		{
			RunBegin,
			RunSweep,
			RunInstruction,
			RunInHandler,
			RunSweepEnd,
		};

		enum ELoadPc : uint8
		{
			LoadFrame,
			LoadWork,
		};

		enum EShutdownPc : uint8
		{
			ShutdownBegin,
			ShutdownFreeMusic,
			ShutdownMusicFreed,
			ShutdownSoundsReleased,
		};

		void StopRoutine(FBattleEventRun& Run)
		{
			Run = FBattleEventRun{};
		}

		/** The script's entry point word at a byte offset of the script. */
		bool ScriptWord(const FBattleEventWork& Event, std::size_t Offset, uint16& Value)
		{
			if (Offset + 2 > Event.Script.size())
			{
				return false;
			}
			Value = LittleEndian::ReadU16(Event.Script, Offset);
			return true;
		}

		bool Compare(uint16 A, uint16 B, uint8 Flags)
		{
			const int16 SignedA = static_cast<int16>(A);
			const int16 SignedB = static_cast<int16>(B);
			const uint8 Bit = static_cast<uint8>(B);
			switch (Flags & 0x0F)
			{
			case 0: return A == B;
			case 1: return A != B;
			case 2: return SignedA > SignedB;
			case 3: return SignedA < SignedB;
			case 4: return SignedA >= SignedB;
			case 5: return SignedA <= SignedB;
			case 6: return (A & B) != 0;
			case 7: return A != B;
			case 8: return A != 0 || B != 0;
			case 9: return Bit < 16 && (A & (1u << Bit)) != 0;
			case 10: return !(Bit < 16 && (A & (1u << Bit)) != 0);
			default: return false;
			}
		}

		/** The highest thread slot taken runs. */
		bool PickThread(FBattleEventRecord& Record)
		{
			bool bFound = false;
			for (uint8 Slot = 0; Slot < FBattleEventRecord::ThreadCount; ++Slot)
			{
				if (Record.Priorities[Slot] != Free)
				{
					Record.Running = Slot;
					bFound = true;
				}
			}
			if (bFound)
			{
				Record.RunningScript = Record.Scripts[Record.Running];
			}
			return bFound;
		}

		FBattleEventRecord* Record(FBattleContext& Context, uint32 Number)
		{
			if (Number >= FBattleEventWork::RecordCount)
			{
				Context.Fault = EBattleFault::EventRecordOutsideWork;
				return nullptr;
			}
			return &Context.Event.Records[Number];
		}

		/** Start (03h): the first free thread slot 1-7 of the actor named takes the script; none free waits. */
		int32 StartThread(FBattleContext& Context, uint8 Actor, std::size_t Pc)
		{
			FBattleEventRecord* Target = Record(Context, CodeByte(Context, Pc + 1));
			const uint8 Byte = CodeByte(Context, Pc + 2);
			if (Target == nullptr || Context.Fault != EBattleFault::None)
			{
				return 0;
			}
			uint8 Slot = 1;
			while (Slot < FBattleEventRecord::ThreadCount && Target->Priorities[Slot] != Free)
			{
				++Slot;
			}
			if (Slot == FBattleEventRecord::ThreadCount)
			{
				return 0;
			}
			FBattleEventRecord& Self = Context.Event.Records[Actor];
			Self.StartedScript = Byte & 0x1F;
			Self.StartedActor = CodeByte(Context, Pc + 1);
			Target->Priorities[Slot] = static_cast<uint8>(Byte >> 5);
			Target->Pcs[Slot] = Entry(Context, CodeByte(Context, Pc + 1), Self.StartedScript);
			Target->Scripts[Slot] = Self.StartedScript;
			return 3;
		}

		int32 StartWaiting(FBattleContext& Context, uint8 Actor, std::size_t Pc, bool bUntilDone)
		{
			FBattleEventRecord& Self = Context.Event.Records[Actor];
			const uint8 Script = CodeByte(Context, Pc + 2) & 0x1F;
			const bool bOtherActor = Context.EventOptions.bWaitedStartNamesActor && Self.StartedActor != CodeByte(Context, Pc + 1);
			if (Self.StartedScript != Script || bOtherActor)
			{
				StartThread(Context, Actor, Pc);
				return 0;
			}
			const FBattleEventRecord* Target = Record(Context, CodeByte(Context, Pc + 1));
			if (Target == nullptr)
			{
				return 0;
			}
			if (!bUntilDone)
			{
				if (Script != Target->RunningScript)
				{
					return 0;
				}
			}
			else if (std::find(Target->Scripts.begin(), Target->Scripts.end(), Script) != Target->Scripts.end() || Self.StartedScript == Target->RunningScript)
			{
				return 0;
			}
			Self.StartedScript = Free;
			return 3;
		}

		/** The flow, thread, variable and arithmetic instructions (00h-17h), the rest by ExecuteBattleOp. */
		int32 Execute(FBattleContext& Context, uint8 Actor, std::size_t Pc)
		{
			FBattleEventWork& Event = Context.Event;
			FBattleEventRecord& Self = Event.Records[Actor];
			const uint8 Opcode = CodeByte(Context, Pc);
			if (Context.Fault != EBattleFault::None)
			{
				return 0;
			}
			const auto Destination = [&]() { return Variable(Context, CodeWord(Context, Pc + 1)); };
			const auto Source = [&]() { return Event.Operands[1]; };
			switch (Opcode)
			{
			case 0x00:
				Self.Scripts[Self.Running] = Free;
				Self.Priorities[Self.Running] = Free;
				Self.Pcs[Self.Running] = NoPc;
				Self.Scripts[0] = IdleScript;
				Self.Running = 0;
				Self.Priorities[0] = IdlePriority;
				Self.StartedScript = Free;
				Self.Pcs[0] = Entry(Context, Actor, IdleScript);
				return 0;
			case 0x01:
				Self.Pcs[Self.Running] = CodeWord(Context, Pc + 1);
				return 0;
			case 0x02:
			{
				const uint8 Flags = CodeByte(Context, Pc + 5);
				FetchFlagged(Context, Pc, 2, Flags);
				if (Compare(Event.Operands[0], Event.Operands[1], Flags))
				{
					return 8;
				}
				Self.Pcs[Self.Running] = CodeWord(Context, Pc + 6);
				return 0;
			}
			case 0x03:
				return StartThread(Context, Actor, Pc);
			case 0x04:
				return StartWaiting(Context, Actor, Pc, false);
			case 0x05:
				return StartWaiting(Context, Actor, Pc, true);
			case 0x07:
			case 0x08:
			case 0x0D:
			case 0x0E:
				if (uint16* Value = Destination())
				{
					*Value = Opcode == 0x07 ? uint16{1} : Opcode == 0x08 ? uint16{0} : static_cast<uint16>(*Value + (Opcode == 0x0D ? 1 : 0xFFFF));
				}
				return 3;
			case 0x06:
			case 0x09:
			case 0x0A:
			case 0x0B:
			case 0x0C:
			case 0x0F:
			case 0x10:
			case 0x11:
			{
				FetchFlagged(Context, Pc, 2, CodeByte(Context, Pc + 5));
				uint16* Value = Destination();
				if (Value == nullptr)
				{
					return 6;
				}
				switch (Opcode)
				{
				case 0x06: *Value = Source(); break;
				case 0x09: *Value = static_cast<uint16>(*Value + Source()); break;
				case 0x0A: *Value = static_cast<uint16>(*Value - Source()); break;
				case 0x0C: *Value = static_cast<uint16>(*Value & ~Source()); break;
				case 0x0F: *Value = static_cast<uint16>(*Value & Source()); break;
				case 0x11: *Value = static_cast<uint16>(*Value ^ Source()); break;
				default: *Value = static_cast<uint16>(*Value | Source()); break;
				}
				return 6;
			}
			case 0x12:
			case 0x13:
			{
				FetchFlagged(Context, Pc, 2, 0);
				uint16* Value = Destination();
				if (Value != nullptr)
				{
					const uint32 Count = Source() & 0x1Fu;
					*Value = static_cast<uint16>(Opcode == 0x12 ? static_cast<uint32>(*Value) << Count : static_cast<uint32>(*Value) >> Count);
				}
				return 5;
			}
			case 0x14:
				if (uint16* Value = Destination())
				{
					*Value = BattleRules::RandomU16Between(*Context.Random, 0, 0x7FFF);
				}
				return 3;
			case 0x15:
			{
				const uint16 High = CodeWord(Context, Pc + 1);
				if (uint16* Value = Variable(Context, CodeWord(Context, Pc + 3)))
				{
					*Value = BattleRules::RandomU16Between(*Context.Random, 0, High);
				}
				return 5;
			}
			case 0x16:
			case 0x17:
			{
				FetchFlagged(Context, Pc, 2, CodeByte(Context, Pc + 5));
				if (uint16* Value = Destination())
				{
					const uint16 A = Event.Operands[0];
					const uint16 B = Event.Operands[1];
					*Value = Opcode == 0x16 ? static_cast<uint16>(static_cast<uint32>(A) * B) : static_cast<uint16>(ProcessorDivide(A, B));
				}
				return 6;
			}
			default:
				return ExecuteBattleOp(Context, Actor, Opcode, Pc);
			}
		}

		/** The work as the load leaves it, from the script and messages BeginBattleEvent took. */
		void Initialize(FBattleContext& Context)
		{
			FBattleEventWork& Event = Context.Event;
			const std::span<const uint8> Script = Event.Script;
			const FMessageTable Messages = Event.Messages;
			const FReplacedMessages* const Replaced = Event.Replaced;
			const EBattleMessageLayout Layout = Event.Layout;
			Event = FBattleEventWork{};
			Event.Script = Script;
			Event.Messages = Messages;
			Event.Replaced = Replaced;
			Event.Layout = Layout;
			Context.EventVariables.fill(0);
			Context.bEventWork = true;
			if (Script.size() < BattleEventScript::EntriesOffset)
			{
				Context.Fault = EBattleFault::EventOutsideScript;
				return;
			}
			Event.ActorCount = LittleEndian::ReadU32(Script, BattleEventScript::ActorCountOffset);
			if (Event.ActorCount > FBattleEventWork::RecordCount)
			{
				Context.Fault = EBattleFault::EventRecordOutsideWork;
				return;
			}
			Event.CodeOffset = BattleEventScript::EntriesOffset + 0x10 * static_cast<std::size_t>(Event.ActorCount);
			if (Event.CodeOffset > Script.size())
			{
				Context.Fault = EBattleFault::EventOutsideScript;
				return;
			}
			Event.Order.fill(Free);
			for (uint32 Actor = 0; Actor < Event.ActorCount; ++Actor)
			{
				FBattleEventRecord& Self = Event.Records[Actor];
				Self.Pcs.fill(NoPc);
				Self.Priorities.fill(Free);
				Self.Scripts.fill(Free);
				Event.Order[Actor] = static_cast<uint8>(Actor);
				Self.Pcs[0] = Entry(Context, Actor, 0);
				Self.Priorities[0] = 0;
				Self.OrderArgument = Free;
				Self.Scripts[0] = 0;
				Self.Running = 0;
				Self.StartedScript = Free;
				Self.Portrait = Free;
			}
			Event.ArrowFrame = 4;
			Event.bWaitCounters = true;
			Event.bKeyWait = false;
			Event.bSoundsRegistered = true;
		}

		bool StepLoad(FBattleContext& Context)
		{
			FBattleEventRun& Run = Context.Event.Run;
			if (Run.Pc == LoadFrame)
			{
				Run.Pc = LoadWork;
				return true;
			}
			Initialize(Context);
			if (Context.Show != nullptr)
			{
				Context.Show->BeginEvent();
			}
			StopRoutine(Context.Event.Run);
			return false;
		}

		/** The instruction's length added to the running thread's pc, and the actor's turn goes on or ends. */
		void Advance(FBattleEventWork& Event, uint8 Opcode, int32 Length)
		{
			FBattleEventRun& Run = Event.Run;
			FBattleEventRecord& Self = Event.Records[Run.Actor];
			Self.Pcs[Self.Running] = static_cast<uint16>(Self.Pcs[Self.Running] + Length);
			Run.Budget = Opcode == EndOpcode ? uint8{0} : static_cast<uint8>(Run.Budget - 1);
			Run.Step = 0;
			if (Run.Budget == 0)
			{
				++Run.Sweep;
				Run.Pc = RunSweep;
			}
			else
			{
				Run.Pc = RunInstruction;
			}
		}

		bool StepRun(FBattleContext& Context)
		{
			FBattleEventWork& Event = Context.Event;
			FBattleEventRun& Run = Event.Run;
			while (Context.Fault == EBattleFault::None)
			{
				switch (Run.Pc)
				{
				case RunBegin:
					if (Context.Show != nullptr)
					{
						Context.Show->LoadBattleBank();
						Context.Show->SetTargetMarkers(0);
					}
					if (Event.bDone)
					{
						StopRoutine(Run);
						return false;
					}
					Run.Sweep = 0;
					Run.Pc = RunSweep;
					break;
				case RunSweep:
					if (Run.Sweep >= Event.ActorCount)
					{
						Run.Pc = RunSweepEnd;
						break;
					}
					Run.Actor = Event.Order[Run.Sweep];
					if (Record(Context, Run.Actor) == nullptr)
					{
						return false;
					}
					Run.Budget = InstructionsPerFrame;
					Run.Pc = RunInstruction;
					return true;
				case RunInstruction:
				case RunInHandler:
				{
					FBattleEventRecord& Self = Event.Records[Run.Actor];
					if (Run.Pc == RunInstruction)
					{
						if (!PickThread(Self))
						{
							Context.Fault = EBattleFault::EventHangs;
							return false;
						}
						Run.Step = 0;
					}
					const std::size_t Pc = Self.Pcs[Self.Running];
					const uint8 Opcode = CodeByte(Context, Pc);
					const int32 Length = Execute(Context, Run.Actor, Pc);
					if (Context.Fault != EBattleFault::None)
					{
						return false;
					}
					if (Length == TakeFrame)
					{
						Run.Pc = RunInHandler;
						return true;
					}
					Advance(Event, Opcode, Length);
					break;
				}
				case RunSweepEnd:
					if (Event.ReturnCountdown != 0)
					{
						--Event.ReturnCountdown;
						if (Event.ReturnCountdown == 1)
						{
							StopRoutine(Run);
							return false;
						}
					}
					if (Event.ActorCount == 0 && Event.ReturnCountdown < 2)
					{
						Context.Fault = EBattleFault::EventHangs;
						return false;
					}
					Run.Sweep = 0;
					Run.Pc = RunSweep;
					break;
				default:
					StopRoutine(Run);
					return false;
				}
			}
			return false;
		}

		bool StepShutdown(FBattleContext& Context)
		{
			FBattleEventWork& Event = Context.Event;
			FBattleEventRun& Run = Event.Run;
			switch (Run.Pc)
			{
			case ShutdownBegin:
				if (Event.bMusicLoaded)
				{
					Cue(Context, EBattleEventCue::StopMusic, {});
					Run.Pc = ShutdownFreeMusic;
					return true;
				}
				[[fallthrough]];
			case ShutdownMusicFreed:
				if (Event.bSoundsRegistered)
				{
					Cue(Context, EBattleEventCue::ReleaseSounds, {});
					Run.Pc = ShutdownSoundsReleased;
					return true;
				}
				break;
			case ShutdownFreeMusic:
				Cue(Context, EBattleEventCue::FreeMusic, {});
				Run.Pc = ShutdownMusicFreed;
				return true;
			case ShutdownSoundsReleased:
				Event.bSoundsRegistered = false;
				break;
			default:
				break;
			}
			StopRoutine(Run);
			return false;
		}
	}

	namespace BattleEventSteps
	{
		uint8 CodeByte(FBattleContext& Context, std::size_t Offset)
		{
			const FBattleEventWork& Event = Context.Event;
			const std::size_t At = Event.CodeOffset + Offset;
			if (At >= Event.Script.size())
			{
				Context.Fault = EBattleFault::EventOutsideScript;
				return 0;
			}
			return Event.Script[At];
		}

		uint16 CodeWord(FBattleContext& Context, std::size_t Offset)
		{
			const uint8 Low = CodeByte(Context, Offset);
			return static_cast<uint16>(Low | CodeByte(Context, Offset + 1) << 8);
		}

		uint16 Entry(FBattleContext& Context, uint32 Actor, uint32 Script)
		{
			uint16 Value = 0;
			if (!ScriptWord(Context.Event, BattleEventScript::EntriesOffset + 0x10 * static_cast<std::size_t>(Actor) + 2 * static_cast<std::size_t>(Script), Value))
			{
				Context.Fault = EBattleFault::EventOutsideScript;
			}
			return Value;
		}

		uint16* Variable(FBattleContext& Context, uint32 Address)
		{
			const std::size_t Index = (Address & 0xFFFEu) / 2;
			if (Index >= Context.EventVariables.size())
			{
				Context.Fault = EBattleFault::EventVariableOutsideWork;
				return nullptr;
			}
			return &Context.EventVariables[Index];
		}

		void FetchWords(FBattleContext& Context, std::size_t Pc, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const uint16 Raw = CodeWord(Context, Pc + 1 + 2 * static_cast<std::size_t>(Index));
				uint16 Value = static_cast<uint16>(Raw & 0x7FFF);
				if ((Raw & 0x8000) == 0)
				{
					const uint16* Held = Variable(Context, Raw & 0x7FFEu);
					Value = Held != nullptr ? *Held : 0;
				}
				Context.Event.Operands[static_cast<std::size_t>(Index)] = Value;
			}
		}

		void FetchFlagged(FBattleContext& Context, std::size_t Pc, int32 Count, uint8 Flags)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				uint16 Value = CodeWord(Context, Pc + 1 + 2 * static_cast<std::size_t>(Index));
				if ((Flags & (0x80u >> Index)) == 0)
				{
					const uint16* Held = Variable(Context, Value);
					Value = Held != nullptr ? *Held : 0;
				}
				Context.Event.Operands[static_cast<std::size_t>(Index)] = Value;
			}
		}

		uint8 CombatantSlot(const FBattleContext& Context, uint16 Value)
		{
			const uint8 Named = static_cast<uint8>(Value);
			if (Named >= 0x10)
			{
				return static_cast<uint8>(Value - 0x0D);
			}
			for (uint8 Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
			{
				if (Context.PartyCharacters[Slot] != 0xFF && Context.PartyCharacters[Slot] == Named)
				{
					return Slot;
				}
			}
			return 0;
		}

		bool IsShownSlot(FBattleContext& Context, uint32 Slot)
		{
			if (Slot >= FBattleContext::SlotCount)
			{
				Context.Fault = EBattleFault::SlotOutsideBattle;
				return false;
			}
			return true;
		}

		void Cue(FBattleContext& Context, EBattleEventCue Kind, std::initializer_list<uint16> Args)
		{
			if (Context.Show == nullptr)
			{
				return;
			}
			FBattleEventCue Cue;
			Cue.Cue = Kind;
			std::copy_n(Args.begin(), std::min(Args.size(), Cue.Args.size()), Cue.Args.begin());
			Context.Show->PlayEventCue(Cue);
		}

		bool ShowBlocks(const FBattleContext& Context)
		{
			return Context.Show != nullptr && Context.Show->IsBlocking();
		}
	}

	namespace BattleEventScript
	{
		std::size_t InstructionLength(uint8 Opcode)
		{
			return Opcode < Lengths.size() ? Lengths[Opcode] : 0;
		}

		bool JumpTarget(std::span<const uint8> Code, std::size_t Offset, uint16& Target)
		{
			if (Offset >= Code.size())
			{
				return false;
			}
			const uint8 Opcode = Code[Offset];
			const std::size_t At = Opcode == 0x01 ? Offset + 1 : Opcode == 0x02 ? Offset + 6 : 0;
			if (At == 0 || At + 2 > Code.size())
			{
				return false;
			}
			Target = LittleEndian::ReadU16(Code, At);
			return true;
		}
	}

	TResult<FBattleEventArchive> FBattleEventArchive::Parse(std::span<const uint8> File)
	{
		if (File.size() < 4)
		{
			return MakeError("the battle events' archive is empty");
		}
		const uint32 Count = LittleEndian::ReadU32(File, 0);
		if (Count % 2 != 0 || 4 + 4 * static_cast<std::size_t>(Count) > File.size())
		{
			return MakeError("the battle events' archive does not hold pairs of scripts and messages");
		}
		std::vector<uint8> Padded(File.begin(), File.end());
		Padded.resize(Padded.size() + ArchivePadding, 0);
		FBattleEventArchive Archive;
		for (uint32 Entry = 0; Entry < Count; ++Entry)
		{
			const uint32 Offset = LittleEndian::ReadU32(File, 4 + 4 * static_cast<std::size_t>(Entry));
			if (Offset >= File.size())
			{
				return MakeError("a battle event's entry starts past its archive");
			}
			TResult<std::vector<uint8>> Unpacked = UnpackLzss(std::span<const uint8>(Padded).subspan(Offset));
			if (!Unpacked)
			{
				return Unpacked.Error();
			}
			(Entry % 2 == 0 ? Archive.Scripts : Archive.MessageTables).push_back(std::move(*Unpacked));
		}
		return Archive;
	}

	TResult<FBattleEventArchive> FBattleEventArchive::Load(const FCdImage& Image, const FGameFileIndex& Index)
	{
		const FIndexRecord* Record = Index.Find(ArchiveDirectory, ArchiveFile);
		if (Record == nullptr)
		{
			return MakeError("the disc lacks the battle events' archive");
		}
		const TResult<std::vector<uint8>> Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		TResult<FBattleEventArchive> Archive = Parse(*Sectors);
		if (!Archive)
		{
			return Archive;
		}
		if (Image.ModChanges().bPerfectWorksBattleMessages)
		{
			Archive->MessageLayout = EBattleMessageLayout::PerfectWorks;
		}
		else if (HasFrenchProgram(Image, Index))
		{
			Archive->MessageLayout = EBattleMessageLayout::French;
		}
		Archive->ReplacedMessages.resize(Archive->MessageTables.size());
		for (std::size_t Event = 0; Event < Archive->MessageTables.size(); ++Event)
		{
			const std::string Table = MessageTables::BattleEvent(static_cast<int32>(Event));
			if (std::optional<FAppliedMessages> Applied = ApplyMessageOverrides(Image, Table, Archive->MessageTables[Event], EMessageWindows::Script))
			{
				Archive->MessageTables[Event] = std::move(Applied->Table);
				Archive->ReplacedMessages[Event] = std::move(Applied->Replaced);
			}
		}
		return Archive;
	}

	void BeginBattleEvent(FBattleContext& Context, std::span<const uint8> Script, std::span<const uint8> Messages, EBattleMessageLayout Layout,
		const FReplacedMessages* Replaced)
	{
		FBattleEventWork& Event = Context.Event;
		Event = FBattleEventWork{};
		Event.Run.Routine = EBattleEventRoutine::Load;
		Event.Script = Script;
		Event.Replaced = Replaced;
		Event.Layout = Layout;
		if (TResult<FMessageTable> Table = FMessageTable::Parse(Messages))
		{
			Event.Messages = *Table;
		}
		Context.bEventWork = false;
	}

	void BeginBattleEventRun(FBattleContext& Context)
	{
		FBattleEventRun& Run = Context.Event.Run;
		Run = FBattleEventRun{};
		Run.Routine = EBattleEventRoutine::Run;
	}

	void BeginBattleEventShutdown(FBattleContext& Context)
	{
		FBattleEventRun& Run = Context.Event.Run;
		Run = FBattleEventRun{};
		Run.Routine = EBattleEventRoutine::Shutdown;
	}

	bool StepBattleEvent(FBattleContext& Context)
	{
		FBattleEventWork& Event = Context.Event;
		if (Context.Fault != EBattleFault::None)
		{
			return false;
		}
		switch (Event.Run.Routine)
		{
		case EBattleEventRoutine::Load:
			return StepLoad(Context);
		case EBattleEventRoutine::Run:
			return StepRun(Context);
		case EBattleEventRoutine::Shutdown:
			return StepShutdown(Context);
		case EBattleEventRoutine::None:
			break;
		}
		return false;
	}

	void TakeBattleMessageInput(FBattleContext& Context, bool bConfirm)
	{
		FBattleEventWork& Event = Context.Event;
		if (Event.bWaitCounters)
		{
			for (FBattleEventRecord& Record : Event.Records)
			{
				if (Record.bWaiting)
				{
					Record.WaitCount = static_cast<int16>(Record.WaitCount - 1);
					if (Record.WaitCount < 0)
					{
						Record.WaitCount = 0;
					}
				}
			}
		}
		if (!Event.bKeyWait)
		{
			Context.Party.Event = NoEvent;
		}
		if (bConfirm)
		{
			Context.Party.Event = ConfirmEvent;
		}
	}

	void TickBattleEventMessage(FBattleContext& Context, FMesContext& Text)
	{
		FBattleEventWork& Event = Context.Event;
		if (Event.bMessageShown && Event.Text)
		{
			Event.Text->Update(Text);
		}
	}

	void FinishBattleEventPerform(FBattleContext& Context, uint8 Slot)
	{
		if (Slot < FBattleEventWork::RecordCount)
		{
			Context.Event.Records[Slot].PerformState = 1;
		}
	}

	void FinishBattleEventMotion(FBattleContext& Context, uint8 Slot)
	{
		if (Slot < FBattleEventWork::RecordCount)
		{
			Context.Event.Moving[Slot] = false;
		}
	}
}
