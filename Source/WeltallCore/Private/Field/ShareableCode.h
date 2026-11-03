#pragma once

#include "EventScriptMatch.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <functional>
#include <set>
#include <string>

namespace Weltall
{
	/**
	 * The script lines of a shareable patch (FieldMapShareable.h): each detour Match makes, each entry point that is now
	 * other code of the disc's or none, then the edited map's steps the disc's map does not play, in the edited map's
	 * order, each going on to the disc's code by its offset where that plays what follows. Message writes a message's
	 * operand as the patch names it; the labels the lines make are none of Names. An error where the edited code goes on
	 * into what is no instruction.
	 */
	TResult<std::string> WriteShareableCode(const EventScriptMatch::FProgram& Edited, const EventScriptMatch::FProgram& Disc,
		const EventScriptMatch::FCorrespondence& Match, const std::function<void(std::string&, int32)>& Message, const std::set<std::string>& Names);
}
