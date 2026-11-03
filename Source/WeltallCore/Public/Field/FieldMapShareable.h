#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <span>
#include <string>

namespace Weltall
{
	class FFieldMapPatch;
	class IDiscText;

	/**
	 * A disc map given whole (FFieldMapPatch::IsWhole), as an author edits Export Disc 2's, written as the patch of the
	 * disc's map that plays as it does, which carries only what the author changed or added (Make shareable):
	 *
	 *   map 617
	 *   disc 3f0a...                              the disc's map the whole map was written for
	 *   message msg16 anew size 30 2              each message the author changed, the others named nowhere
	 *   { ... }
	 *   message Hall from map 617 message 3       an added message that is one of the disc's, named
	 *   entrance 2 x 120 z -40 layer 0 camera 2 facing keep
	 *   detour @0x0A12 to Part1 Message msg9 0    the disc's code that leads into what the author changed
	 *   entry 3 1 @0x0C40                         a script that is now another of the disc's code
	 *   actor 4 script 2:                         a script that begins with what the author changed
	 *   Part1:
	 *       ...                                   only the code that differs from the disc's
	 *       Jump @0x0A1C                          and back to the disc's, where it is the same again
	 *
	 * Each of the whole map's steps that the disc's map has with the same steps after it is left where the disc has it;
	 * the rest is the patch's code, reached by a detour at the disc's instruction it stands for (one of three bytes or
	 * more whose bytes the port's fixes and documents leave as they are) or by an entry point. Such a path runs a plain
	 * jump or two more than the whole map's, which the field counts in an actor's eight instructions a tick.
	 *
	 * DiscScripts and DiscMessages are the disc's map as the game reads it, before the player's messages; Disc reads the
	 * messages the whole map names of the disc's own; Fixed is the disc map's scripts as the port's fixes and own
	 * documents leave them. Before it is given back, the patch is added to the disc's map and checked to play as the
	 * whole map does: every script, message, entrance and unsigned variable. Empty where the whole map plays as the
	 * disc's. An error where the disc's map is not the one the whole map was written for, or where the whole map changes
	 * what a patch cannot: its actor count, the number of its entrances, a variable the disc's reads unsigned.
	 */
	WELTALLCORE_API TResult<std::string> WriteShareableFieldMapText(const FFieldMapPatch& Whole, std::span<const uint8> DiscScripts,
		std::span<const uint8> DiscMessages, const IDiscText& Disc, const FEventScripts& Fixed);
}
