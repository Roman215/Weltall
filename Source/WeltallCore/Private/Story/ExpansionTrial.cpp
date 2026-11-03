#include "Story/ExpansionTrial.h"

#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapStory.h"
#include "Story/StoryPacks.h"

#include <set>
#include <string>

namespace Weltall
{
	namespace
	{
		/** Field map Number's sections the story is put in, from its archive alone: nothing of drawing it is built. */
		TResult<FFieldMap> LoadStorySections(const FCdImage& Image, const FGameFileIndex& Index, int32 Number)
		{
			TResult<FFieldArchive> Archive = LoadFieldArchive(Image, Index, Number);
			if (!Archive)
			{
				return Archive.Error();
			}
			FFieldMap Map;
			Map.EventScripts = std::move(Archive->Section(EFieldSection::EventScripts));
			Map.Messages = std::move(Archive->Section(EFieldSection::Messages));
			Map.Encounters = std::move(Archive->Section(EFieldSection::Encounters));
			return Map;
		}

		/** The numbers the expansion's documents of Documents are written for, each once and in order. */
		template <typename TDocument, typename TNumber>
		std::set<int32> WrittenFor(const std::vector<TDocument>& Documents, TNumber Number)
		{
			std::set<int32> Numbers;
			for (const TDocument& Document : Documents)
			{
				if (Document.bExpansion)
				{
					Numbers.insert(Number(Document));
				}
			}
			return Numbers;
		}

		/** Adds the names of the expansion's documents of Documents that pass Named to Names. */
		template <typename TDocument, typename TNamed>
		void AddNames(std::vector<std::string>& Names, const std::vector<TDocument>& Documents, TNamed Named)
		{
			for (const TDocument& Document : Documents)
			{
				if (Document.bExpansion && Named(Document))
				{
					Names.push_back(Document.Name);
				}
			}
		}
	}

	TResult<FExpansionTrial> TryExpansion(const FStoryPacks& Story, const FCdImage& Image, const FGameFileIndex& Index, const FEventOptions& Options)
	{
		const TResult<FBattleEventArchive> Events = FBattleEventArchive::Load(Image, Index);
		if (!Events)
		{
			return Events.Error();
		}
		FExpansionTrial Trial;
		const FFieldMapStory MapStory{&Story, Options, true};
		for (const int32 Map : WrittenFor(Story.Documents(), [](const FStoryDocument& Document) { return Document.Patch.Map(); }))
		{
			const int32 From = FieldMapReadFrom(Map, MapStory);
			if (!HoldsFieldMap(Image, Index, From))
			{
				AddNames(Trial.ForAnotherDisc, Story.Documents(), [Map](const FStoryDocument& Document) { return Document.Patch.Map() == Map; });
				continue;
			}
			TResult<FFieldMap> Sections = LoadStorySections(Image, Index, From);
			std::vector<const FStoryDocument*> LeftOut;
			const TResult<FEventScripts> Put = Sections ? PutStoryInFieldMap(Map, *Sections, Image, Index, MapStory, LeftOut) : TResult<FEventScripts>(Sections.Error());
			if (!Put)
			{
				Trial.Faults.push_back(MakeError("field map " + std::to_string(Map) + ": " + Put.Error().Message));
			}
			for (const FStoryDocument* Document : LeftOut)
			{
				if (Document->bExpansion)
				{
					Trial.ForAnotherDisc.push_back(Document->Name);
				}
			}
		}
		for (const int32 Event : WrittenFor(Story.EventDocuments(), [](const FStoryEventDocument& Document) { return Document.Patch.Event(); }))
		{
			if (Event < 0 || Event >= Events->Count())
			{
				AddNames(Trial.ForAnotherDisc, Story.EventDocuments(), [Event](const FStoryEventDocument& Document) { return Document.Patch.Event() == Event; });
				continue;
			}
			std::vector<const FStoryEventDocument*> LeftOut;
			Story.BattleEventMessages(Event, Events->Messages(Event), Events->DiscMessages(Event), true, &LeftOut);
			for (const FStoryEventDocument* Document : LeftOut)
			{
				if (Document->bExpansion)
				{
					Trial.ForAnotherDisc.push_back(Document->Name);
				}
			}
		}
		return Trial;
	}
}
