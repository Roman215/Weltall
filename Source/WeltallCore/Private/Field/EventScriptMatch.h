#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Types.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * How the scripts of an edited map and of the disc's map correspond, which Make shareable writes a patch from: the
 * instructions the edited map's threads reach that the disc's map has as they are, and where the disc's code must
 * jump to the edited code instead.
 */
namespace Weltall::EventScriptMatch
{
	/** Gives the steps that do the same one number, across every program read with it. */
	class FKinds
	{
	public:
		int32 Of(std::string Bytes);

	private:
		std::unordered_map<std::string, int32> Numbers;
	};

	/** One instruction a map's threads reach. */
	struct FStep
	{
		uint16 Offset = 0;
		uint16 Length = 0;
		/** Whether it is a plain jump, which goes on elsewhere and does nothing else. */
		bool bJump = false;
		/** Where it goes on, in a fixed order: the next instruction unless it ends the thread, its jumps' targets, a jump table's entries. */
		std::vector<uint16> Successors;
		/** For a jump table, how many entries (plain jumps) follow it, the last of its successors. */
		uint16 Entries = 0;
		/** Its bytes but for its jumps' targets, each message as the program's message numbering gives it: steps of one kind do the same. */
		int32 Kind = 0;
	};

	/** A map's reached instructions in the order of their offsets. */
	struct FProgram
	{
		const FEventScripts* Scripts = nullptr;
		std::vector<FStep> Steps;
		std::unordered_map<uint16, int32> Index;

		/** The step at Offset, -1 where none starts there. */
		int32 StepAt(std::size_t Offset) const;
	};

	/**
	 * Scripts' reached instructions, their kinds from Kinds; Message gives the number a message operand is compared as
	 * (its number in the table the program is compared in).
	 */
	FProgram ReadProgram(const FEventScripts& Scripts, FKinds& Kinds, const std::function<int32(int32)>& Message);

	/** Whether an actor's script is used: an entry point other than 0, or 0 for a bytecode that starts with a script. */
	bool IsScriptUsed(const FEventScripts& Scripts, int32 Actor, int32 Script);

	/** Where each step of the edited map plays in the disc's map once patched. */
	struct FCorrespondence
	{
		/** For each edited step, the disc's step that plays it with its own bytes; -1 for none. */
		std::vector<int32> Kept;
		/** For each disc step, the edited step a detour there jumps to; -1 for none. */
		std::vector<int32> Detours;
	};

	/**
	 * The edited program's steps the disc's has as they are, found in the order both lay their code out, each kept where
	 * every step it goes on to plays at the disc step's own successor; where it does not, a disc step of three bytes or
	 * more may become a detour to the edited step instead, unless one of its bytes is one the port changes in the map
	 * (Fixed's, by offset, not zero).
	 */
	FCorrespondence Correspond(const FProgram& Edited, const FProgram& Disc, std::span<const uint8> Fixed);

	/**
	 * Whether the code from Edited's From plays as the code from Patched's To does, a plain jump being only the way to
	 * where it goes; none, or where they part (an offset of Edited's).
	 */
	std::optional<uint16> PartsFrom(const FProgram& Edited, uint16 From, const FProgram& Patched, uint16 To);
}
