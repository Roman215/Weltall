#pragma once

#include "Field/EventContext.h"
#include "Field/FieldMapPatch.h"
#include "Foundation/Result.h"
#include "Story/BattleEventTextPatch.h"
#include "Story/StoryWayBack.h"
#include "Story/StorySubtitles.h"
#include "Story/WorldMapPatch.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** A document of the port's story: where it came from ("pack/file.txt") and the map patch it is. */
	struct FStoryDocument
	{
		std::string Name;
		FFieldMapPatch Patch;
		/** The player's setting it is added under (the patch's option), none: it always is. */
		bool FEventOptions::* Option = nullptr;
		/** Of the expanded second disc's pack: a game's only when it plays the second disc expanded. */
		bool bExpansion = false;
	};

	/** A document that changes a battle event's text (BattleEventTextPatch.h): where it came from and what it changes. */
	struct FStoryEventDocument
	{
		std::string Name;
		FBattleEventTextPatch Patch;
		/** Of the expanded second disc's pack: a game's only when it plays the second disc expanded. */
		bool bExpansion = false;
	};

	/** A document that adds places to a world map's era (WorldMapPatch.h): where it came from and what it adds. */
	struct FStoryWorldMapDocument
	{
		std::string Name;
		FWorldMapPatch Patch;
		/** Of the expanded second disc's pack: a game's only when it plays the second disc expanded. */
		bool bExpansion = false;
	};

	/**
	 * The port's own content: text the game reads as it starts, in packs, a folder of documents each. A document
	 * is a map patch (FieldMapPatch.h), added to its map's scripts and messages as the map loads, under the
	 * player's setting it names if it names one (the additions pack's RPS helper: `option fix.rps_helper`), or a
	 * map of its own, which the field loads under its number as it would a disc's, or, beginning "battle event", a
	 * change to a battle event's text, made as the battle loads its event, or, beginning "world map", places added
	 * to a world map's era as the world map loads its scene. One pack is the expanded second disc:
	 * its documents are there for a game that plays the second disc expanded (EStoryMode) and for no other; one
	 * of them, beginning "way back", may say where such a game goes on in the original's (StoryWayBack.h), and one,
	 * beginning "subtitles", the titles its stretches take inside the original's chapters (StorySubtitles.h).
	 */
	class FStoryPacks
	{
	public:
		/** The pack that is the expanded second disc. */
		static constexpr std::string_view ExpansionPack = "disc2";

		/**
		 * Every pack in Folder: each folder in it a pack of its name, each .txt file there one of its documents, the
		 * packs and their documents in the order of their names, hidden ones left out (IsHiddenFile). None at all
		 * where there is no such folder; an error naming the document where one cannot be read.
		 */
		WELTALLCORE_API static TResult<FStoryPacks> Read(const std::filesystem::path& Folder);

		/** Adds a document of a pack after the others; the error names it and its line where it cannot be read, or where it makes a map another document made. */
		WELTALLCORE_API std::optional<FError> Add(std::string_view Pack, std::string_view File, std::string_view Text);

		/** The documents of field maps: the patches and the maps of their own. */
		const std::vector<FStoryDocument>& Documents() const { return All; }
		/** The documents of battle events' text. */
		const std::vector<FStoryEventDocument>& EventDocuments() const { return Events; }
		/** How many documents were read, of every kind. */
		int32 DocumentCount() const { return static_cast<int32>(All.size() + Events.size() + WorldMaps.size() + (Back ? 1 : 0) + (Titles ? 1 : 0)); }

		/** The documents of the world map's places. */
		const std::vector<FStoryWorldMapDocument>& WorldMapDocuments() const { return WorldMaps; }

		/** Where a game of the expanded second disc goes on in the original's; null where the pack does not say. */
		const FStoryWayBack* WayBack() const { return Back ? &*Back : nullptr; }

		/** The subtitle a game of the expanded second disc is under with its story at Story; null in any other game, and before the first. */
		WELTALLCORE_API const FStorySubtitle* SubtitleAt(uint16 Story, bool bExpandedStory) const;
		/** The subtitle beginning at First as FStorySubtitles::Part numbers it inside a chapter from ChapterFirst. */
		WELTALLCORE_API int32 SubtitlePart(uint16 First, uint16 ChapterFirst) const;

		/** Whether the port has the expanded second disc: any document of its pack. */
		WELTALLCORE_API bool HasExpansion() const;

		/**
		 * Adds the documents written for field map Map that Options leave on to its scripts and messages, in their
		 * order, the expanded second disc's only with bExpandedStory; one the map's instructions do not fit is left
		 * out. An error naming the document where one cannot be assembled or the map cannot take it. Disc is where
		 * the messages a document names of the disc's own are read.
		 */
		WELTALLCORE_API std::optional<FError> ApplyToMap(int32 Map, FEventScripts& Scripts, std::vector<uint8>& Messages, const FEventOptions& Options,
			bool bExpandedStory, const IDiscText* Disc = nullptr) const;

		/**
		 * Battle event Event's message table as the documents written for it leave it, in their order, the expanded
		 * second disc's only with bExpandedStory; none where no document changes it (one the table does not fit is
		 * left out).
		 */
		WELTALLCORE_API std::optional<std::vector<uint8>> BattleEventMessages(int32 Event, std::span<const uint8> Messages, bool bExpandedStory) const;

		/**
		 * Adds the places the documents written for era Era's world map give to Scene, in their order, the expanded
		 * second disc's only with bExpandedStory; an error naming the document where the scene cannot take one.
		 */
		WELTALLCORE_API std::optional<FError> ApplyToWorldMap(int32 Era, FWorldMapScene& Scene, bool bExpandedStory) const;

		/** Whether a document for era Era keeps the party's ship away from the world map while the story is at Story (WorldMapPatch.h), the expansion's only in a game of it. */
		WELTALLCORE_API bool KeepsVehicleAway(int32 Era, uint16 Story, bool bExpandedStory) const;

		/**
		 * The document that makes field map Map, a map of its own, the expanded second disc's only with bExpandedStory;
		 * null when none does (the disc's maps).
		 */
		WELTALLCORE_API const FStoryDocument* MapOfItsOwn(int32 Map, bool bExpandedStory) const;

	private:
		/** The document that makes field map Map in any game, null when none does. */
		const FStoryDocument* MakerOf(int32 Map) const;

		std::vector<FStoryDocument> All;
		std::vector<FStoryEventDocument> Events;
		std::vector<FStoryWorldMapDocument> WorldMaps;
		std::optional<FStoryWayBack> Back;
		std::optional<FStorySubtitles> Titles;
	};
}
