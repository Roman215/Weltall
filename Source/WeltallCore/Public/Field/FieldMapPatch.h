#pragma once

#include "Field/EventScripts.h"
#include "Foundation/Result.h"
#include "Foundation/Sha1.h"
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
	 *   message Hall from map 617 message 16        a field map's message, in the window its table gives it; one
	 *                                               page of it, with 'page P', in a window of the page's size
	 *   expect @0x00B4 If var[2CE] & #-32768 else @0x00C4      the map's own instruction there
	 *   detour @0x024A to Hands If var[014] == #1 else @0x0329   the same, and the map goes to Hands instead
	 *   Hands:                                      every other line: event scripts (EventAssembly.h),
	 *       Message Offer 0                         assembled after the map's bytecode
	 *       Jump @0x0252                            the map's own code by its offset
	 *   actor 2 script 2:                           a script of the map's given anew
	 *
	 * A patch may also name the disc's map it was written for, which lets it give the map's own messages and entrances
	 * anew (Make shareable writes such patches):
	 *
	 *   map 617
	 *   disc 3f0a...                                the disc's map (DiscIdentity.h): on any other the patch is left out
	 *   message msg16 anew size 30 2                the map's message 16 given anew, where its scripts name it
	 *   { ... }
	 *   message msg4 anew from map 617 message 9    or as another of the disc's own
	 *   entrance 2 x 120 z -40 layer 0 camera 2 facing keep   the map's entrance 2 given anew
	 *
	 * Or a map of its own, numbered 730 to 1023, made of a disc map's parts with scripts and messages that are the
	 * text's alone:
	 *
	 *   map 1000 from 613                           the disc map whose archive and textures it takes
	 *   encounters from 273                         another disc map's encounters and formations, when given
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
	 *
	 * Or a disc map given whole, its script section and messages in place of the disc's, which Export Disc 2 writes:
	 *
	 *   map 617 whole                               the disc map it stands for
	 *   disc 3f0a...                                the disc's map it was written for (DiscIdentity.h): on any other
	 *                                               the disc's own plays
	 *   exported 91c2...                            the export's mark: left as written, the text changes nothing
	 *   message msg0 size 30 2                      every message, numbered from 0 in their order
	 *   { ... }
	 *   actors 12                                   the whole script section, from address 0, as "from" gives it
	 *   ...
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
		/** Whether the text gives disc map Map whole, built in place of the disc's scripts and messages. */
		bool IsWhole() const { return bWhole; }
		/** For a whole map, or a patch that names it, the identity (FieldMapIdentity) of the disc's map it was written for. */
		const std::optional<FSha1::FDigest>& WrittenFor() const { return WrittenForMap; }

		/**
		 * Adds the patch to a map's scripts and messages, for a map over its base to the base map's: false, changing
		 * nothing, where the map is not the one the patch was written for (its identity, Identity, is not the one the
		 * patch names, its instructions are not the expected ones, or it lacks a message or an entrance the patch gives
		 * anew) or the disc lacks a message the text names; an error where the patch's own scripts do not assemble or
		 * fit. Disc is where the messages it names of the disc's own are read; a text that names one cannot be added
		 * without it. Identity is the map's as the disc gives it (DiscIdentity.h), before any fix or patch.
		 */
		WELTALLCORE_API TResult<bool> Apply(FEventScripts& Scripts, std::vector<uint8>& Messages, const IDiscText* Disc = nullptr,
			const FSha1::FDigest* Identity = nullptr) const;

		/**
		 * The script section and message table of a map of its own made from its base ("from"), or of a whole map; an
		 * error, naming the line, where they do not assemble. Disc as for Apply.
		 */
		WELTALLCORE_API TResult<FFieldMapOfItsOwn> Build(const IDiscText* Disc = nullptr) const;

		/** A message of the disc's own: a battle event's or a field map's message, or one page of it (0: all of them). */
		struct FDiscMessage
		{
			int32 Line = 0;
			bool bFieldMap = false;
			/** The event, or the map, whose table holds it. */
			int32 Table = 0;
			int32 Message = 0;
			int32 Page = 0;
		};

		struct FNamedMessage
		{
			std::string Name;
			FAddedMessage Message;
			/** Set for a message the text names of the disc's own: Message is then read as the patch is added. */
			std::optional<FDiscMessage> Disc;
			/** Set for one of the map's own messages given anew, by its number: the others follow the map's own. */
			std::optional<int32> Anew;
		};

		/** The text's messages in its order. */
		const std::vector<FNamedMessage>& NamedMessages() const { return Messages; }

	private:
		/** One of the map's instructions the patch counts on, which a detour replaces by a jump to Label. */
		struct FSite
		{
			int32 Line = 0;
			uint16 Offset = 0;
			std::string Instruction;
			std::string Label;
		};

		/** One of the map's entrance records given anew: its number and its bytes. */
		struct FEntrance
		{
			int32 Line = 0;
			int32 Index = 0;
			std::vector<uint8> Bytes;
		};

		/** The names of the messages that follow the map's own, each the operand of its number from First on. */
		std::vector<std::pair<std::string, std::string>> MessageDefinitions(int32 First) const;

		/** Gives the map's entrances anew in Scripts: false where the map lacks one of them. */
		bool PutEntrances(FEventScripts& Scripts) const;

		/**
		 * The messages in their order, the disc's own read through Disc, added to Out: false where the disc lacks one,
		 * an error where one is named and there is no Disc to read it from.
		 */
		TResult<bool> AddedMessages(const IDiscText* Disc, std::vector<FAddedMessage>& Out) const;

		int32 MapNumber = 0;
		std::string OptionName;
		std::optional<int32> BaseMap;
		bool bKeepsBaseScripts = false;
		bool bWhole = false;
		std::optional<FSha1::FDigest> WrittenForMap;
		std::optional<int32> EncountersMap;
		std::vector<FSite> Sites;
		std::vector<FEntrance> Entrances;
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

	/**
	 * A disc map's scripts and messages written as the text that gives map Number whole ("map Number whole"), naming
	 * them as the disc holds them and with the export's mark (DiscIdentity.h): built, the text gives them back.
	 */
	WELTALLCORE_API TResult<std::string> WriteWholeFieldMapText(int32 Number, std::span<const uint8> EventScripts, std::span<const uint8> Messages);
}
