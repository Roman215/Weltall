#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Text/MessageTable.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	class IDiscText;

	/** A sprite slot a map of its own takes from another disc map: that map's slot's sprite file and images, in place of its base map's. */
	struct FSpriteBorrow
	{
		int32 Line = 0;
		int32 Slot = 0;
		int32 Map = 0;
		int32 SourceSlot = 0;
	};

	/** The sections a map of its own has of its own: its script section and its message table. */
	struct FFieldMapOfItsOwn
	{
		std::vector<uint8> EventScripts;
		std::vector<uint8> Messages;
	};

	/**
	 * What is added to a field map's own scripts and messages, written as text:
	 *
	 *   map 7                                       the map it is written for
	 *   option fix.rps_helper                       the player's setting it is added under, when there is one
	 *   message Offer                               a message after the map's own, by a name the scripts use
	 *   {
	 *   I could have a little talk
	 *   with him...{wait 14}
	 *   }
	 *   message Count size 30 2                     its window given where its text does not say it
	 *   { ... }
	 *   message Met from event 19 message 0 page 1  a message of the disc's own, named and not carried: a battle
	 *                                               event's message, or one page of it, in a window of its size
	 *   message Where from map 617 message 23       the same, a field map's message: the script the game plays with
	 *   expect @0x00B4 If var[2CE] & #-32768 else @0x00C4      the map's own instruction there
	 *   detour @0x024A to Hands If var[014] == #1 else @0x0329   the same, and the map goes to Hands instead
	 *   Hands:                                      every other line: event scripts (EventAssembly.h),
	 *       Message Offer 0                         assembled after the map's bytecode
	 *       Jump @0x0252                            the map's own code by its offset
	 *   actor 2 script 2:                           a script of the map's given anew
	 *
	 * Or a map of its own, numbered 730 to 1023, made of a disc map's parts with scripts and messages that are the
	 * text's alone:
	 *
	 *   map 1000 from 613                           the disc map whose archive and textures it takes
	 *   encounters from 273                         another disc map's encounters and formations, when given
	 *   sprite 9 from map 164 slot 2                another disc map's sprite in one of its slots, when given
	 *   message Ramsus                              its messages, numbered from 0 in their order
	 *   { ... }
	 *   actors 3                                    its whole script section (EventAssembly.h), from address 0
	 *   actor 0 script 0:
	 *       ...
	 *
	 * Or a map of its own that keeps its base map's scripts and messages too, and is written as a patch of them:
	 *
	 *   map 730 over 613                            the disc map it takes whole
	 *   encounters from 273
	 *   message Signal                              a message after the base map's own
	 *   { ... }
	 *   detour @0x00FD to Second SetGearModel #0    the base map's instructions, expected and detoured as a patch's
	 *   actor 8 script 1:                           a script of the base map's given anew
	 *
	 * Its script actors are the base map's first actor definitions, by number. A message's text is MesText.h's,
	 * between a line of '{' and a line of '}'. A map whose instructions are not the ones a patch expects is another
	 * map than the one it was written for, and is left as it is.
	 */
	class FFieldMapPatch
	{
	public:
		/** Reads the text; the error names the line. The scripts are assembled when the patch is applied, after a map's own. */
		WELTALLCORE_API static TResult<FFieldMapPatch> Parse(std::string_view Text);

		int32 Map() const { return MapNumber; }
		/** The player's setting the patch is added under, as the settings file names it; empty: it always is. */
		const std::string& Option() const { return OptionName; }
		/** For a map of its own, the disc map whose parts it takes; none: a patch of map Map. */
		std::optional<int32> Base() const { return BaseMap; }
		/** Whether a map of its own keeps its base map's scripts and messages, the text a patch of them ("over"). */
		bool KeepsBaseScripts() const { return bKeepsBaseScripts; }
		/** For a map of its own, the disc map whose encounter section it takes; none: its base map's. */
		std::optional<int32> EncountersFrom() const { return EncountersMap; }
		/** Which of the text's messages, by their place among them from 0, are the disc's own (named, not carried): the game's text, not the port's. */
		WELTALLCORE_API std::vector<int32> DiscMessages() const;
		/** For a map of its own, the sprite slots it takes from other disc maps, in the text's order. */
		const std::vector<FSpriteBorrow>& SpritesFrom() const { return Borrows; }

		/**
		 * Adds the patch to a map's scripts and messages, for a map over its base to the base map's: false, changing
		 * nothing, where the map's instructions are not the expected ones or the disc lacks a message the text names;
		 * an error where the patch's own scripts do not assemble or fit. Disc is where the messages it names of the
		 * disc's own are read; a text that names one cannot be added without it.
		 */
		WELTALLCORE_API TResult<bool> Apply(FEventScripts& Scripts, std::vector<uint8>& Messages, const IDiscText* Disc = nullptr) const;

		/**
		 * The script section and message table of a map of its own made from its base ("from"); an error, naming the
		 * line, where they do not assemble. Disc as for Apply.
		 */
		WELTALLCORE_API TResult<FFieldMapOfItsOwn> Build(const IDiscText* Disc = nullptr) const;

	private:
		/** One of the map's instructions the patch counts on, which a detour replaces by a jump to Label. */
		struct FSite
		{
			int32 Line = 0;
			uint16 Offset = 0;
			std::string Instruction;
			std::string Label;
		};

		/** A message of the disc's own: a battle event's or a field map's message (Map from 0; -1: the event's), or one page of it (0: all of them). */
		struct FDiscMessage
		{
			int32 Line = 0;
			int32 Event = 0;
			int32 Map = -1;
			int32 Message = 0;
			int32 Page = 0;
		};

		struct FNamedMessage
		{
			std::string Name;
			FAddedMessage Message;
			/** Set for a message the text names of the disc's own: Message is then read as the patch is added. */
			std::optional<FDiscMessage> Disc;
		};

		/** The messages' names, each the operand of its number from First on. */
		std::vector<std::pair<std::string, std::string>> MessageDefinitions(int32 First) const;

		/**
		 * The messages in their order, the disc's own read through Disc, added to Out: false where the disc lacks one,
		 * an error where one is named and there is no Disc to read it from.
		 */
		TResult<bool> AddedMessages(const IDiscText* Disc, std::vector<FAddedMessage>& Out) const;

		int32 MapNumber = 0;
		std::string OptionName;
		std::optional<int32> BaseMap;
		bool bKeepsBaseScripts = false;
		std::optional<int32> EncountersMap;
		std::vector<FSpriteBorrow> Borrows;
		std::vector<FSite> Sites;
		std::vector<FNamedMessage> Messages;
		/** The text's script lines, every other line empty so that an error's line is the text's. */
		std::string Assembly;
	};

	/**
	 * A disc map's scripts and messages written as the text of a map of its own made of it: "map Number from Base", its
	 * messages in their order, named msg0, msg1 ... as its scripts name them, each with its window, and its script
	 * section (EventAssembly.h's listing). Built, the text gives the map's script section back byte for byte and its
	 * messages as they are: where a map of one's own starts from, and what a patch of the map is written against. An
	 * error where the sections cannot be read.
	 */
	WELTALLCORE_API TResult<std::string> WriteFieldMapText(int32 Number, int32 Base, std::span<const uint8> EventScripts, std::span<const uint8> Messages);
}
