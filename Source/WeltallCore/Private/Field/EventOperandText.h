#pragma once

#include "Field/EventInstructionSet.h"
#include "Foundation/Types.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

/** An event instruction's operand as the scripts' text writes it, and back. A jump is a label, which only the text's reader and writer know. */
namespace Weltall::EventAssemblyText
{
	/** An operand as stored, and whether it is a constant where its kind can be one or a variable. */
	struct FOperandWord
	{
		uint32 Stored = 0;
		bool bConstant = false;
	};

	/** Adds an operand other than a jump or the flags byte: Stored as its instruction holds it, Flags the instruction's flags byte. */
	void AppendOperand(std::string& Out, FEventOperandKind Kind, int32 Stored, uint8 Flags);

	/** The operand Word spells for Kind (not a jump, not the flags byte); none where it spells none. */
	std::optional<FOperandWord> OperandOfWord(FEventOperandKind Kind, std::string_view Word);

	/**
	 * Adds an instruction as the scripts' text writes it, indented: its name, then its operands, an If's as a sentence and
	 * the flags byte's other bits last; Jump adds a jump's target (a label or an offset), Message, where given, a
	 * message's operand (else msgN).
	 */
	void AppendInstruction(std::string& Out, const FEventInstruction& Instruction, const std::function<void(std::string&, std::size_t)>& Jump,
		const std::function<void(std::string&, int32)>& Message = {});
}
