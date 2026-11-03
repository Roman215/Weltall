#include "Field/FieldMapStory.h"

#include "Field/FieldMap.h"
#include "Field/FieldScriptFixes.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/**
		 * The document's built script section and message table in place of the disc map Base's, whose actor definitions
		 * its script actors are: it may have no more of them than the disc map's scripts have.
		 */
		std::optional<FError> PutBuiltSections(const FStoryDocument& Document, int32 Base, FFieldMap& Map, const IDiscText& DiscText)
		{
			TResult<FFieldMapOfItsOwn> Built = Document.Patch.Build(&DiscText);
			if (!Built)
			{
				return MakeError(Document.Name + ": " + Built.Error().Message);
			}
			const TResult<FEventScripts> Disc = FEventScripts::Parse(Map.EventScripts);
			const TResult<FEventScripts> Own = FEventScripts::Parse(Built->EventScripts);
			if (!Disc || !Own)
			{
				return MakeError(Document.Name + ": its scripts or field map " + std::to_string(Base) + "'s cannot be read");
			}
			if (Own->ActorCount() > Disc->ActorCount())
			{
				return MakeError(Document.Name + ": " + std::to_string(Own->ActorCount()) + " script actors, more than the " + std::to_string(Disc->ActorCount())
					+ " of field map " + std::to_string(Base));
			}
			Map.EventScripts = std::move(Built->EventScripts);
			Map.Messages = std::move(Built->Messages);
			Map.ReplacedMessages = {};
			Map.DiscMessages.clear();
			return std::nullopt;
		}

		/**
		 * A disc map given whole: the document's scripts and messages in place of the disc's, where the disc's map, as the
		 * game reads it but for the player's messages, is the one it was written for; elsewhere the disc's own stays.
		 */
		std::optional<FError> PutWholeMap(const FStoryDocument& Document, FFieldMap& Map, const IDiscText& DiscText, std::vector<const FStoryDocument*>& LeftOut)
		{
			const std::vector<uint8>& DiscMessages = Map.DiscMessages.empty() ? Map.Messages : Map.DiscMessages;
			if (FieldMapIdentity(Map.EventScripts, DiscMessages) != Document.Patch.WrittenFor())
			{
				LeftOut.push_back(&Document);
				return std::nullopt;
			}
			return PutBuiltSections(Document, Document.Patch.Map(), Map, DiscText);
		}

		/**
		 * A map of its own over its base map's parts: another map's encounter section when the document names one, and,
		 * for a map made from its base, the document's scripts and messages in place of the base's (a map over its base
		 * keeps them, and the document is added to them once they are read: AddToBaseScripts).
		 */
		std::optional<FError> BuildMapOfItsOwn(const FStoryDocument& Document, FFieldMap& Map, const FCdImage& Image, const FGameFileIndex& Index,
			const IDiscText& DiscText)
		{
			if (!Document.Patch.KeepsBaseScripts())
			{
				if (std::optional<FError> Unbuilt = PutBuiltSections(Document, *Document.Patch.Base(), Map, DiscText))
				{
					return Unbuilt;
				}
			}
			if (const std::optional<int32> From = Document.Patch.EncountersFrom())
			{
				TResult<FFieldArchive> Other = LoadFieldArchive(Image, Index, *From);
				if (!Other)
				{
					return Other.Error();
				}
				Map.Encounters = std::move(Other->Section(EFieldSection::Encounters));
			}
			return std::nullopt;
		}

		/** A map over its base: the document added to the base map's scripts and messages, left out where they are not the ones it was written for. */
		std::optional<FError> AddToBaseScripts(const FStoryDocument& Document, FEventScripts& Scripts, std::vector<uint8>& Messages, const IDiscText& DiscText,
			std::vector<const FStoryDocument*>& LeftOut)
		{
			const TResult<bool> Added = Document.Patch.Apply(Scripts, Messages, &DiscText);
			if (!Added)
			{
				return MakeError(Document.Name + ": " + Added.Error().Message);
			}
			if (!*Added)
			{
				LeftOut.push_back(&Document);
			}
			return std::nullopt;
		}
	}

	int32 FieldMapReadFrom(int32 Number, const FFieldMapStory& Story)
	{
		const FStoryDocument* Own = Story.Packs != nullptr ? Story.Packs->MapOfItsOwn(Number, Story.bExpandedStory) : nullptr;
		return Own != nullptr ? *Own->Patch.Base() : Number;
	}

	TResult<FEventScripts> PutStoryInFieldMap(int32 Number, FFieldMap& Map, const FCdImage& Image, const FGameFileIndex& Index, const FFieldMapStory& Story,
		std::vector<const FStoryDocument*>& LeftOut)
	{
		const FStoryDocument* Own = Story.Packs != nullptr ? Story.Packs->MapOfItsOwn(Number, Story.bExpandedStory) : nullptr;
		// The disc's own text, for the documents that name a message of it.
		const FGameDiscText DiscText(Image, Index);
		if (Own != nullptr)
		{
			if (std::optional<FError> Unbuilt = BuildMapOfItsOwn(*Own, Map, Image, Index, DiscText))
			{
				return *Unbuilt;
			}
		}
		else if (const FStoryDocument* Whole = Story.Packs != nullptr ? Story.Packs->WholeMap(Number, Story.bExpandedStory) : nullptr)
		{
			if (std::optional<FError> Unbuilt = PutWholeMap(*Whole, Map, DiscText, LeftOut))
			{
				return *Unbuilt;
			}
		}
		TResult<FEventScripts> Parsed = FEventScripts::Parse(Map.EventScripts);
		if (!Parsed)
		{
			return MakeError("its scripts, sprite files or messages cannot be read");
		}
		// What a patch that names the disc's map checks: the map as the disc gives it, before the fixes and the player's messages.
		const std::optional<FSha1::FDigest> Identity = FieldMapIdentity(Map.EventScripts, Map.DiscMessages.empty() ? Map.Messages : Map.DiscMessages);
		// A map over its base runs the base map's scripts, so it takes that map's fixes before the document is added.
		const bool bOverBase = Own != nullptr && Own->Patch.KeepsBaseScripts();
		ApplyFieldScriptFixes(bOverBase ? *Own->Patch.Base() : Number, *Parsed, Story.Options);
		if (bOverBase)
		{
			if (std::optional<FError> Unfit = AddToBaseScripts(*Own, *Parsed, Map.Messages, DiscText, LeftOut))
			{
				return *Unfit;
			}
		}
		if (Story.Packs != nullptr)
		{
			if (std::optional<FError> Unfit = Story.Packs->ApplyToMap(Number, *Parsed, Map.Messages, Story.Options, Story.bExpandedStory, &DiscText,
				Identity ? &*Identity : nullptr, &LeftOut))
			{
				return MakeError("an addition to it: " + Unfit->Message);
			}
		}
		return Parsed;
	}
}
