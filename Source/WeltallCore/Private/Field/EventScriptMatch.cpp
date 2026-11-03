#include "EventScriptMatch.h"

#include "EventAssemblyText.h"
#include "Field/EventAssembly.h"
#include "Field/EventInstructionSet.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

namespace Weltall::EventScriptMatch
{
	namespace
	{
		using namespace EventAssemblyText;

		constexpr std::size_t JumpBytes = 3;
		/** Past this many differences, an edited map is matched by the steps alike at its start and its end alone. */
		constexpr int32 MostDifferences = 2000;
		/** Plain jumps followed in a row before the last is taken for a step of its own (a jump to itself). */
		constexpr int32 MostHops = 64;

		void AppendWord(std::string& Bytes, uint32 Value)
		{
			for (int32 Byte = 0; Byte < 4; ++Byte)
			{
				Bytes += static_cast<char>(Value >> (8 * Byte));
			}
		}

		/** The pairs of A's and B's indices a shortest edit of A into B keeps, ascending: Myers' greedy diff, past its common start and end. */
		std::vector<std::pair<int32, int32>> Alike(std::span<const int32> A, std::span<const int32> B)
		{
			std::vector<std::pair<int32, int32>> Pairs;
			int32 Start = 0;
			const int32 SizeA = static_cast<int32>(A.size());
			const int32 SizeB = static_cast<int32>(B.size());
			while (Start < SizeA && Start < SizeB && A[static_cast<std::size_t>(Start)] == B[static_cast<std::size_t>(Start)])
			{
				Pairs.emplace_back(Start, Start);
				++Start;
			}
			int32 EndA = SizeA;
			int32 EndB = SizeB;
			std::vector<std::pair<int32, int32>> Ends;
			while (EndA > Start && EndB > Start && A[static_cast<std::size_t>(EndA - 1)] == B[static_cast<std::size_t>(EndB - 1)])
			{
				--EndA;
				--EndB;
				Ends.emplace_back(EndA, EndB);
			}
			const int32 N = EndA - Start;
			const int32 M = EndB - Start;
			const auto Same = [&](int32 X, int32 Y) { return A[static_cast<std::size_t>(Start + X)] == B[static_cast<std::size_t>(Start + Y)]; };
			// Each round's furthest reach on each diagonal k = x - y, kept from -d to d for the walk back.
			const int32 Most = std::min(N + M, MostDifferences);
			std::vector<int32> Reach(static_cast<std::size_t>(2 * Most + 3), 0);
			const int32 Middle = Most + 1;
			std::vector<std::vector<int32>> Rounds;
			int32 Found = -1;
			for (int32 D = 0; D <= Most && Found < 0; ++D)
			{
				for (int32 K = -D; K <= D; K += 2)
				{
					const bool bDown = K == -D || (K != D && Reach[static_cast<std::size_t>(Middle + K - 1)] < Reach[static_cast<std::size_t>(Middle + K + 1)]);
					int32 X = bDown ? Reach[static_cast<std::size_t>(Middle + K + 1)] : Reach[static_cast<std::size_t>(Middle + K - 1)] + 1;
					int32 Y = X - K;
					while (X < N && Y < M && Same(X, Y))
					{
						++X;
						++Y;
					}
					Reach[static_cast<std::size_t>(Middle + K)] = X;
					if (X >= N && Y >= M)
					{
						Found = D;
						break;
					}
				}
				Rounds.emplace_back(Reach.begin() + (Middle - D), Reach.begin() + (Middle + D + 1));
			}
			std::vector<std::pair<int32, int32>> Kept;
			if (Found >= 0)
			{
				int32 X = N;
				int32 Y = M;
				for (int32 D = Found; D > 0; --D)
				{
					const std::vector<int32>& Before = Rounds[static_cast<std::size_t>(D - 1)];
					const auto Previous = [&Before, D](int32 K) { return Before[static_cast<std::size_t>(K + D - 1)]; };
					const int32 K = X - Y;
					const int32 FromK = K == -D || (K != D && Previous(K - 1) < Previous(K + 1)) ? K + 1 : K - 1;
					const int32 FromX = Previous(FromK);
					const int32 FromY = FromX - FromK;
					while (X > FromX && Y > FromY)
					{
						Kept.emplace_back(Start + X - 1, Start + Y - 1);
						--X;
						--Y;
					}
					X = FromX;
					Y = FromY;
				}
				while (X > 0 && Y > 0)
				{
					Kept.emplace_back(Start + X - 1, Start + Y - 1);
					--X;
					--Y;
				}
			}
			Pairs.insert(Pairs.end(), Kept.rbegin(), Kept.rend());
			Pairs.insert(Pairs.end(), Ends.rbegin(), Ends.rend());
			return Pairs;
		}

		/** The step a program's code at Offset plays as, past plain jumps; -1 where it is no step. */
		int32 Played(const FProgram& Program, std::size_t Offset)
		{
			int32 Step = Program.StepAt(Offset);
			for (int32 Hop = 0; Hop < MostHops && Step >= 0 && Program.Steps[static_cast<std::size_t>(Step)].bJump; ++Hop)
			{
				Step = Program.StepAt(Program.Steps[static_cast<std::size_t>(Step)].Successors.front());
			}
			return Step;
		}
	}

	int32 FKinds::Of(std::string Bytes)
	{
		const int32 Next = static_cast<int32>(Numbers.size());
		return Numbers.try_emplace(std::move(Bytes), Next).first->second;
	}

	int32 FProgram::StepAt(std::size_t Offset) const
	{
		const auto Found = Index.find(static_cast<uint16>(Offset));
		return Offset <= 0xFFFF && Found != Index.end() ? Found->second : -1;
	}

	FProgram ReadProgram(const FEventScripts& Scripts, FKinds& Kinds, const std::function<int32(int32)>& Message)
	{
		FProgram Program;
		Program.Scripts = &Scripts;
		const std::span<const uint8> Code(Scripts.Code(), Scripts.CodeSize());
		for (const uint16 Offset : FollowEventThreads(Scripts).Reached)
		{
			const FEventInstruction Instruction = *ReadEventInstruction(Code, Offset);
			FStep Step;
			Step.Offset = Offset;
			Step.Length = static_cast<uint16>(Instruction.Length);
			Step.bJump = Instruction.Shape->Opcode == EventOpcode::Jump;
			if (!Instruction.Shape->bEndsThread)
			{
				Step.Successors.push_back(static_cast<uint16>(Offset + Instruction.Length));
			}
			std::string Bytes(reinterpret_cast<const char*>(Code.data() + Offset), Instruction.Length);
			std::size_t At = static_cast<std::size_t>(Instruction.Shape->OpcodeBytes());
			for (int32 Operand = 0; Operand < Instruction.Operands->Count; ++Operand)
			{
				const FEventOperandKind Kind = Instruction.Operands->Kinds[static_cast<std::size_t>(Operand)];
				const int32 Value = Instruction.Values[static_cast<std::size_t>(Operand)];
				if (Kind.Kind == EEventOperand::Jump || Kind.Kind == EEventOperand::Message)
				{
					Bytes[At] = 0;
					Bytes[At + 1] = 0;
				}
				if (Kind.Kind == EEventOperand::Jump)
				{
					Step.Successors.push_back(static_cast<uint16>(Value));
				}
				else if (Kind.Kind == EEventOperand::Message)
				{
					AppendWord(Bytes, static_cast<uint32>(Message(Value)));
				}
				At += static_cast<std::size_t>(Kind.Size());
			}
			if (Instruction.Shape->Opcode == EventOpcode::JumpTable)
			{
				uint32 Entries = 0;
				for (std::size_t Entry = Offset + Instruction.Length; Entry + JumpBytes <= Code.size() && Code[Entry] == EventOpcode::Jump; Entry += JumpBytes)
				{
					Step.Successors.push_back(static_cast<uint16>(Entry));
					++Entries;
				}
				AppendWord(Bytes, Entries);
				Step.Entries = static_cast<uint16>(Entries);
			}
			Step.Kind = Kinds.Of(std::move(Bytes));
			Program.Index.emplace(Offset, static_cast<int32>(Program.Steps.size()));
			Program.Steps.push_back(std::move(Step));
		}
		return Program;
	}

	bool IsScriptUsed(const FEventScripts& Scripts, int32 Actor, int32 Script)
	{
		return Scripts.EntryPoint(Actor, Script) != 0 || (Actor == 0 && Script == 0 && Scripts.CodeSize() > 0 && Scripts.Code()[0] != EntranceMark);
	}

	FCorrespondence Correspond(const FProgram& Edited, const FProgram& Disc, std::span<const uint8> Fixed)
	{
		std::vector<int32> EditedKinds;
		std::vector<int32> DiscKinds;
		std::transform(Edited.Steps.begin(), Edited.Steps.end(), std::back_inserter(EditedKinds), [](const FStep& Step) { return Step.Kind; });
		std::transform(Disc.Steps.begin(), Disc.Steps.end(), std::back_inserter(DiscKinds), [](const FStep& Step) { return Step.Kind; });
		FCorrespondence Result;
		Result.Kept.assign(Edited.Steps.size(), -1);
		Result.Detours.assign(Disc.Steps.size(), -1);
		// Which edited step each disc step plays with its own bytes.
		std::vector<int32> Host(Disc.Steps.size(), -1);
		for (const auto& [Step, Site] : Alike(EditedKinds, DiscKinds))
		{
			Result.Kept[static_cast<std::size_t>(Step)] = Site;
			Host[static_cast<std::size_t>(Site)] = Step;
		}
		// Every offset of the disc's code a script begins at or a step goes on to, instruction or not: a detour's jump may not cover one.
		std::vector<bool> Pointed(Disc.Scripts->CodeSize() + JumpBytes, false);
		for (int32 Actor = 0; Actor < Disc.Scripts->ActorCount(); ++Actor)
		{
			for (int32 Script = 0; Script < FEventScripts::ScriptsPerActor; ++Script)
			{
				if (const uint16 Entry = Disc.Scripts->EntryPoint(Actor, Script); IsScriptUsed(*Disc.Scripts, Actor, Script) && Entry < Pointed.size())
				{
					Pointed[Entry] = true;
				}
			}
		}
		for (const FStep& Step : Disc.Steps)
		{
			for (const uint16 Next : Step.Successors)
			{
				if (Next < Pointed.size())
				{
					Pointed[Next] = true;
				}
			}
		}
		const auto Taken = [&](int32 Site) { return Site >= 0 && (Host[static_cast<std::size_t>(Site)] >= 0 || Result.Detours[static_cast<std::size_t>(Site)] >= 0); };
		const auto CanDetour = [&](int32 Site)
		{
			const FStep& At = Disc.Steps[static_cast<std::size_t>(Site)];
			if (Taken(Site) || At.Length < JumpBytes)
			{
				return false;
			}
			for (std::size_t Byte = At.Offset; Byte < static_cast<std::size_t>(At.Offset + At.Length); ++Byte)
			{
				if (Byte < Fixed.size() && Fixed[Byte] != 0)
				{
					return false;
				}
			}
			// A step that starts inside the jump's bytes, or a detour whose jump covers this step's start (instructions that overlap).
			for (std::size_t Near = 1; Near < JumpBytes; ++Near)
			{
				const int32 Before = At.Offset >= Near ? Disc.StepAt(At.Offset - Near) : -1;
				if (Pointed[At.Offset + Near] || Taken(Disc.StepAt(At.Offset + Near)) || (Before >= 0 && Result.Detours[static_cast<std::size_t>(Before)] >= 0))
				{
					return false;
				}
			}
			return true;
		};
		// Whether every step the edited step goes on to plays where the disc step goes on, a detour there made where one can be.
		const auto Plays = [&](int32 Step, int32 Site)
		{
			const FStep& Mine = Edited.Steps[static_cast<std::size_t>(Step)];
			const FStep& Theirs = Disc.Steps[static_cast<std::size_t>(Site)];
			if (Mine.Successors.size() != Theirs.Successors.size())
			{
				return false;
			}
			for (std::size_t Each = 0; Each < Mine.Successors.size(); ++Each)
			{
				const int32 Next = Edited.StepAt(Mine.Successors[Each]);
				const int32 There = Disc.StepAt(Theirs.Successors[Each]);
				if (Next < 0 || There < 0)
				{
					if (Next < 0 && There < 0)
					{
						continue;
					}
					return false;
				}
				if (Result.Kept[static_cast<std::size_t>(Next)] == There || Result.Detours[static_cast<std::size_t>(There)] == Next)
				{
					continue;
				}
				if (!CanDetour(There))
				{
					return false;
				}
				Result.Detours[static_cast<std::size_t>(There)] = Next;
			}
			return true;
		};
		for (bool bChanged = true; bChanged;)
		{
			bChanged = false;
			for (std::size_t Step = 0; Step < Edited.Steps.size(); ++Step)
			{
				const int32 Site = Result.Kept[Step];
				if (Site >= 0 && !Plays(static_cast<int32>(Step), Site))
				{
					Result.Kept[Step] = -1;
					Host[static_cast<std::size_t>(Site)] = -1;
					bChanged = true;
				}
			}
		}
		// A detour made for a step that was not kept after all is left out.
		std::vector<bool> Needed(Disc.Steps.size(), false);
		for (std::size_t Step = 0; Step < Edited.Steps.size(); ++Step)
		{
			const int32 Site = Result.Kept[Step];
			for (std::size_t Each = 0; Site >= 0 && Each < Edited.Steps[Step].Successors.size(); ++Each)
			{
				const int32 Next = Edited.StepAt(Edited.Steps[Step].Successors[Each]);
				const int32 There = Disc.StepAt(Disc.Steps[static_cast<std::size_t>(Site)].Successors[Each]);
				if (Next >= 0 && There >= 0 && Result.Kept[static_cast<std::size_t>(Next)] != There && Result.Detours[static_cast<std::size_t>(There)] == Next)
				{
					Needed[static_cast<std::size_t>(There)] = true;
				}
			}
		}
		for (std::size_t Site = 0; Site < Disc.Steps.size(); ++Site)
		{
			if (!Needed[Site])
			{
				Result.Detours[Site] = -1;
			}
		}
		return Result;
	}

	std::optional<uint16> PartsFrom(const FProgram& Edited, uint16 From, const FProgram& Patched, uint16 To)
	{
		std::set<std::pair<int32, int32>> Seen;
		std::vector<std::pair<std::size_t, std::size_t>> Pending = {{From, To}};
		while (!Pending.empty())
		{
			const auto [Mine, Theirs] = Pending.back();
			Pending.pop_back();
			const int32 Left = Played(Edited, Mine);
			const int32 Right = Played(Patched, Theirs);
			if (Left < 0 || Right < 0)
			{
				if (Left < 0 && Right < 0)
				{
					continue;
				}
				return static_cast<uint16>(Mine);
			}
			if (!Seen.emplace(Left, Right).second)
			{
				continue;
			}
			const FStep& A = Edited.Steps[static_cast<std::size_t>(Left)];
			const FStep& B = Patched.Steps[static_cast<std::size_t>(Right)];
			if (A.Kind != B.Kind || A.Successors.size() != B.Successors.size())
			{
				return A.Offset;
			}
			for (std::size_t Each = 0; Each < A.Successors.size(); ++Each)
			{
				Pending.emplace_back(A.Successors[Each], B.Successors[Each]);
			}
		}
		return std::nullopt;
	}
}
