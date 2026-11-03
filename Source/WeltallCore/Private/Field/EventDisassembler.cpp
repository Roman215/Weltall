#include "Field/EventAssembly.h"

#include "EventAssemblyText.h"
#include "EventOperandText.h"
#include "Field/EventInstructionSet.h"
#include "Foundation/LittleEndian.h"

#include <algorithm>
#include <map>
#include <set>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;

		enum class ELine : uint8
		{
			EntranceMark,
			Entrance,
			Instruction,
			Data,
		};

		/** A stretch of the bytecode the listing writes as one line, or as data lines. */
		struct FLine
		{
			ELine Kind = ELine::Data;
			std::size_t Offset = 0;
			std::size_t Length = 0;
			bool bReached = false;
		};

		/**
		 * The bytecode as lines in order, every byte in one: the entrance table (its length is not recorded: it
		 * ends at the first reached instruction, or before, at a record that cannot be an entrance), the reached
		 * instructions, and between them whatever reads as instructions, then data.
		 */
		std::vector<FLine> Lines(std::span<const uint8> Code, const FEventThreads& Threads)
		{
			std::vector<FLine> Result;
			std::size_t Position = 0;
			if (!Code.empty() && Code[0] == EntranceMark)
			{
				Result.push_back({ELine::EntranceMark, 0, 1});
				Position = 1;
				const std::size_t FirstReached = Threads.Reached.empty() ? 0 : Threads.Reached.front();
				const std::size_t Records = EntranceRecords(Code, FirstReached);
				for (std::size_t Record = 0; Record < Records; ++Record)
				{
					Result.push_back({ELine::Entrance, Position, EntranceBytes});
					Position += EntranceBytes;
				}
			}
			const auto Fill = [&Result, &Position, Code](std::size_t End)
			{
				while (Position < End)
				{
					const std::optional<FEventInstruction> Instruction = ReadEventInstruction(Code, Position);
					if (!Instruction || Position + Instruction->Length > End)
					{
						Result.push_back({ELine::Data, Position, End - Position});
						Position = End;
						return;
					}
					Result.push_back({ELine::Instruction, Position, Instruction->Length});
					Position += Instruction->Length;
				}
			};
			for (const uint16 Reached : Threads.Reached)
			{
				// One reached inside the instruction before it is that instruction's bytes already.
				if (Reached < Position)
				{
					continue;
				}
				Fill(Reached);
				const std::size_t Length = ReadEventInstruction(Code, Reached)->Length;
				Result.push_back({ELine::Instruction, Reached, Length, true});
				Position = Reached + Length;
			}
			Fill(Code.size());
			return Result;
		}

		std::string LabelOf(std::size_t Offset)
		{
			std::string Name = "L";
			AppendHex(Name, static_cast<uint32>(Offset), 4);
			return Name;
		}

		class FListing
		{
		public:
			explicit FListing(const FEventScripts& InScripts)
				: Scripts(InScripts), Code(InScripts.Code(), InScripts.CodeSize()), Threads(FollowEventThreads(InScripts)), Layout(Lines(Code, Threads))
			{
				FindTargets();
				FindLineStarts();
			}

			std::string Text()
			{
				WriteHeader();
				for (const FLine& Line : Layout)
				{
					switch (Line.Kind)
					{
					case ELine::EntranceMark:
						WriteLabels(Line.Offset);
						Out += "entrances\n";
						break;
					case ELine::Entrance:
						WriteLabels(Line.Offset);
						WriteEntrance(Line.Offset);
						break;
					case ELine::Instruction:
						WriteLabels(Line.Offset);
						WriteInstruction(Line);
						break;
					case ELine::Data:
						WriteData(Line);
						break;
					}
				}
				WriteLabels(Code.size());
				return std::move(Out);
			}

		private:
			void FindTargets()
			{
				for (const FLine& Line : Layout)
				{
					if (Line.Kind != ELine::Instruction)
					{
						continue;
					}
					const FEventInstruction Instruction = *ReadEventInstruction(Code, Line.Offset);
					for (int32 Operand = 0; Operand < Instruction.Operands->Count; ++Operand)
					{
						if (Instruction.Operands->Kinds[static_cast<std::size_t>(Operand)].Kind == EEventOperand::Jump)
						{
							JumpTargets.insert(static_cast<std::size_t>(Instruction.Values[static_cast<std::size_t>(Operand)]));
						}
					}
				}
				Targets = JumpTargets;
				for (int32 Actor = 0; Actor < Scripts.ActorCount(); ++Actor)
				{
					for (int32 Script = 0; Script < FEventScripts::ScriptsPerActor; ++Script)
					{
						// An entry point of 0 is an unused one, but for the first script of a bytecode that starts with it.
						const uint16 Entry = Scripts.EntryPoint(Actor, Script);
						if (Entry != 0 || (Actor == 0 && Script == 0 && !Code.empty() && Code[0] != EntranceMark))
						{
							ScriptStarts[Entry].emplace_back(Actor, Script);
							Targets.insert(Entry);
						}
					}
				}
			}

			void FindLineStarts()
			{
				for (const FLine& Line : Layout)
				{
					if (Line.Kind != ELine::Data)
					{
						LineStarts.insert(Line.Offset);
						continue;
					}
					for (std::size_t Row = Line.Offset; Row < Line.Offset + Line.Length; Row = DataRowEnd(Row, Line.Offset + Line.Length))
					{
						LineStarts.insert(Row);
					}
				}
				LineStarts.insert(Code.size());
			}

			std::size_t DataRowEnd(std::size_t Row, std::size_t End) const
			{
				const auto NextTarget = Targets.upper_bound(Row);
				return std::min({Row + DataBytesPerLine, End, NextTarget != Targets.end() ? *NextTarget : End});
			}

			void WriteHeader()
			{
				Out += "actors ";
				Append(Out, Scripts.ActorCount());
				Out += '\n';
				WriteUnsigned();
				for (const auto& [Offset, Starts] : ScriptStarts)
				{
					if (LineStarts.contains(Offset))
					{
						continue;
					}
					for (const auto& [Actor, Script] : Starts)
					{
						Out += "entry ";
						Append(Out, Actor);
						Out += ' ';
						Append(Out, Script);
						Out += " @0x";
						AppendHex(Out, static_cast<uint32>(Offset), 4);
						Out += '\n';
					}
				}
			}

			/** The unsigned variables' addresses, runs of them as ranges, a dozen to a line. */
			void WriteUnsigned()
			{
				constexpr uint32 Addresses = FEventScripts::VariableCount * 2;
				constexpr int32 PerLine = 12;
				int32 OnLine = 0;
				for (uint32 Address = 0; Address < Addresses; Address += 2)
				{
					if (!Scripts.IsVariableUnsigned(Address))
					{
						continue;
					}
					uint32 Last = Address;
					while (Last + 2 < Addresses && Scripts.IsVariableUnsigned(Last + 2))
					{
						Last += 2;
					}
					Out += OnLine == 0 ? "unsigned " : " ";
					AppendHex(Out, Address, 3);
					if (Last != Address)
					{
						Out += '-';
						AppendHex(Out, Last, 3);
					}
					Address = Last;
					if (++OnLine == PerLine)
					{
						Out += '\n';
						OnLine = 0;
					}
				}
				if (OnLine != 0)
				{
					Out += '\n';
				}
			}

			void WriteLabels(std::size_t Offset)
			{
				if (const auto Starts = ScriptStarts.find(Offset); Starts != ScriptStarts.end())
				{
					for (const auto& [Actor, Script] : Starts->second)
					{
						Out += "actor ";
						Append(Out, Actor);
						Out += " script ";
						Append(Out, Script);
						Out += ":\n";
					}
				}
				if (JumpTargets.contains(Offset))
				{
					Out += LabelOf(Offset);
					Out += ":\n";
				}
			}

			void WriteDirection(uint8 Direction)
			{
				if (Direction == KeepDirection)
				{
					Out += Keep;
					return;
				}
				Append(Out, Direction);
			}

			void WriteEntrance(std::size_t Offset)
			{
				Out += "entrance x ";
				Append(Out, LittleEndian::ReadS16(Code, Offset));
				Out += " z ";
				Append(Out, LittleEndian::ReadS16(Code, Offset + 2));
				Out += " layer ";
				Append(Out, Code[Offset + 4]);
				Out += " camera ";
				WriteDirection(Code[Offset + 5]);
				Out += " facing ";
				WriteDirection(Code[Offset + 6]);
				Out += '\n';
			}

			/** A jump names its label where a line starts at its target, and the offset itself where none does. */
			void WriteJump(std::string& Text, std::size_t Target) const
			{
				if (LineStarts.contains(Target))
				{
					Text += LabelOf(Target);
				}
				else
				{
					Text += "@0x";
					AppendHex(Text, static_cast<uint32>(Target), 4);
				}
			}

			void WriteInstruction(const FLine& Line)
			{
				const std::size_t Start = Out.size();
				AppendInstruction(Out, *ReadEventInstruction(Code, Line.Offset), [this](std::string& Text, std::size_t Target) { WriteJump(Text, Target); });
				WriteOffset(Start, Line.Offset, Line.bReached);
			}

			void WriteData(const FLine& Line)
			{
				const std::size_t End = Line.Offset + Line.Length;
				for (std::size_t Row = Line.Offset; Row < End;)
				{
					const std::size_t RowEnd = DataRowEnd(Row, End);
					WriteLabels(Row);
					const std::size_t Start = Out.size();
					Out += "    data";
					for (std::size_t Byte = Row; Byte < RowEnd; ++Byte)
					{
						Out += ' ';
						AppendHex(Out, Code[Byte], 2);
					}
					WriteOffset(Start, Row, true);
					Row = RowEnd;
				}
			}

			/** Ends a line with its offset as a comment, in a column of its own where the line leaves room. */
			void WriteOffset(std::size_t Start, std::size_t Offset, bool bReached)
			{
				constexpr std::size_t Column = 64;
				const std::size_t Width = Out.size() - Start;
				Out.append(Width < Column ? Column - Width : 1, ' ');
				Out += "; ";
				AppendHex(Out, static_cast<uint32>(Offset), 4);
				if (!bReached)
				{
					Out += " unreached";
				}
				Out += '\n';
			}

			const FEventScripts& Scripts;
			std::span<const uint8> Code;
			FEventThreads Threads;
			std::vector<FLine> Layout;
			/** Where the laid-out instructions jump: each gets a label where a line starts there. */
			std::set<std::size_t> JumpTargets;
			/** The scripts that start at each offset, by actor and number. */
			std::map<std::size_t, std::vector<std::pair<int32, int32>>> ScriptStarts;
			/** Every offset a jump or an entry point names: a data line breaks there, so that it can carry the label. */
			std::set<std::size_t> Targets;
			std::set<std::size_t> LineStarts;
			std::string Out;
		};
	}

	std::string DisassembleEventScripts(const FEventScripts& Scripts)
	{
		return FListing(Scripts).Text();
	}
}
