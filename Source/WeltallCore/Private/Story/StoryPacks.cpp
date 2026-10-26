#include "Story/StoryPacks.h"

#include "Foundation/HiddenFiles.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The settings a document can be added under, by the names the settings file gives them. */
		struct FStoryOption
		{
			std::string_view Name;
			bool FEventOptions::* Flag;
		};
		constexpr std::array StoryOptions = {
			FStoryOption{"fix.rps_helper", &FEventOptions::bRpsHelper},
			FStoryOption{"fix.wide_view_staging", &FEventOptions::bWideViewStaging},
			FStoryOption{"fix.swim_stroke", &FEventOptions::bSwimStrokeAfterWading},
		};

		constexpr std::string_view DocumentExtension = ".txt";
		constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
		/** The first word of a document that changes a battle event's text, and of the expanded second disc's way back. */
		constexpr std::string_view BattleEventWord = "battle";
		constexpr std::string_view WayBackWord = "way";
		/** The first word of the expanded second disc's subtitles. */
		constexpr std::string_view SubtitlesWord = "subtitles";
		/** The first word of a document that adds places to a world map. */
		constexpr std::string_view WorldMapWord = "world";

		/** A text's first word, past its blank lines and comments. */
		std::string_view FirstWord(std::string_view Text)
		{
			constexpr std::string_view Blanks = " \t\r\n";
			while (true)
			{
				Text.remove_prefix(std::min(Text.find_first_not_of(Blanks), Text.size()));
				if (!Text.starts_with(';'))
				{
					return Text.substr(0, Text.find_first_of(Blanks));
				}
				Text.remove_prefix(std::min(Text.find('\n'), Text.size()));
			}
		}

		std::string NameOf(const std::filesystem::path& Path)
		{
			const std::u8string Name = Path.filename().u8string();
			return std::string(Name.begin(), Name.end());
		}

		/** The entries of a folder that pass Wanted, in the order of their names, but the hidden ones. */
		template <typename TWanted>
		std::vector<std::filesystem::path> EntriesOf(const std::filesystem::path& Folder, TWanted Wanted)
		{
			std::vector<std::filesystem::path> Entries;
			std::error_code Error;
			for (auto Entry = std::filesystem::directory_iterator(Folder, Error); !Error && Entry != std::filesystem::directory_iterator(); Entry.increment(Error))
			{
				if (!IsHiddenFile(Entry->path()) && Wanted(*Entry))
				{
					Entries.push_back(Entry->path());
				}
			}
			std::sort(Entries.begin(), Entries.end());
			return Entries;
		}
	}

	TResult<FStoryPacks> FStoryPacks::Read(const std::filesystem::path& Folder)
	{
		FStoryPacks Packs;
		const auto IsFolder = [](const std::filesystem::directory_entry& Entry)
		{
			std::error_code Error;
			return Entry.is_directory(Error);
		};
		const auto IsDocument = [](const std::filesystem::directory_entry& Entry)
		{
			std::error_code Error;
			return Entry.is_regular_file(Error) && Entry.path().extension() == std::filesystem::path(DocumentExtension);
		};
		for (const std::filesystem::path& Pack : EntriesOf(Folder, IsFolder))
		{
			for (const std::filesystem::path& Document : EntriesOf(Pack, IsDocument))
			{
				std::ifstream File(Document, std::ios::binary);
				const std::string Text{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
				if (!File.good() && !File.eof())
				{
					return MakeError(NameOf(Pack) + "/" + NameOf(Document) + " cannot be read");
				}
				if (std::optional<FError> Error = Packs.Add(NameOf(Pack), NameOf(Document), Text))
				{
					return *Error;
				}
			}
		}
		return Packs;
	}

	std::optional<FError> FStoryPacks::Add(std::string_view Pack, std::string_view File, std::string_view Text)
	{
		const std::string Name = std::string(Pack) + "/" + std::string(File);
		if (Text.starts_with(ByteOrderMark))
		{
			Text.remove_prefix(ByteOrderMark.size());
		}
		if (FirstWord(Text) == BattleEventWord)
		{
			TResult<FBattleEventTextPatch> Event = FBattleEventTextPatch::Parse(Text);
			if (!Event)
			{
				return MakeError(Name + ": " + Event.Error().Message);
			}
			Events.push_back({Name, std::move(*Event), Pack == ExpansionPack});
			return std::nullopt;
		}
		if (FirstWord(Text) == WorldMapWord)
		{
			TResult<FWorldMapPatch> Places = FWorldMapPatch::Parse(Text);
			if (!Places)
			{
				return MakeError(Name + ": " + Places.Error().Message);
			}
			WorldMaps.push_back({Name, std::move(*Places), Pack == ExpansionPack});
			return std::nullopt;
		}
		if (FirstWord(Text) == WayBackWord)
		{
			if (Pack != ExpansionPack || Back)
			{
				return MakeError(Name + ": the way back is the expanded second disc's, one document of its pack");
			}
			TResult<FStoryWayBack> WayBack = FStoryWayBack::Parse(Text);
			if (!WayBack)
			{
				return MakeError(Name + ": " + WayBack.Error().Message);
			}
			Back = std::move(*WayBack);
			return std::nullopt;
		}
		if (FirstWord(Text) == SubtitlesWord)
		{
			if (Pack != ExpansionPack || Titles)
			{
				return MakeError(Name + ": the subtitles are the expanded second disc's, one document of its pack");
			}
			TResult<FStorySubtitles> Subtitles = FStorySubtitles::Parse(Text);
			if (!Subtitles)
			{
				return MakeError(Name + ": " + Subtitles.Error().Message);
			}
			Titles = std::move(*Subtitles);
			return std::nullopt;
		}
		TResult<FFieldMapPatch> Patch = FFieldMapPatch::Parse(Text);
		if (!Patch)
		{
			return MakeError(Name + ": " + Patch.Error().Message);
		}
		if (Patch->Base() && MakerOf(Patch->Map()) != nullptr)
		{
			return MakeError(Name + ": map " + std::to_string(Patch->Map()) + " is made by " + MakerOf(Patch->Map())->Name + " already");
		}
		bool FEventOptions::* Option = nullptr;
		if (!Patch->Option().empty())
		{
			const auto Known = std::find_if(StoryOptions.begin(), StoryOptions.end(), [&Patch](const FStoryOption& Candidate) { return Candidate.Name == Patch->Option(); });
			if (Known == StoryOptions.end())
			{
				return MakeError(Name + ": no setting is named '" + Patch->Option() + "'");
			}
			Option = Known->Flag;
		}
		All.push_back({Name, std::move(*Patch), Option, Pack == ExpansionPack});
		return std::nullopt;
	}

	bool FStoryPacks::HasExpansion() const
	{
		return std::any_of(All.begin(), All.end(), [](const FStoryDocument& Document) { return Document.bExpansion; })
			|| std::any_of(Events.begin(), Events.end(), [](const FStoryEventDocument& Document) { return Document.bExpansion; })
			|| std::any_of(WorldMaps.begin(), WorldMaps.end(), [](const FStoryWorldMapDocument& Document) { return Document.bExpansion; });
	}

	std::optional<FError> FStoryPacks::ApplyToWorldMap(int32 Era, FWorldMapScene& Scene, bool bExpandedStory) const
	{
		for (const FStoryWorldMapDocument& Document : WorldMaps)
		{
			if (Document.Patch.Era() != Era || (Document.bExpansion && !bExpandedStory))
			{
				continue;
			}
			if (std::optional<FError> Error = Document.Patch.Apply(Scene))
			{
				return MakeError(Document.Name + ": " + Error->Message);
			}
		}
		return std::nullopt;
	}

	const FStorySubtitle* FStoryPacks::SubtitleAt(uint16 Story, bool bExpandedStory) const
	{
		return bExpandedStory && Titles ? Titles->At(Story) : nullptr;
	}

	int32 FStoryPacks::SubtitlePart(uint16 First, uint16 ChapterFirst) const
	{
		return Titles ? Titles->Part(First, ChapterFirst) : 0;
	}

	bool FStoryPacks::KeepsVehicleAway(int32 Era, uint16 Story, bool bExpandedStory) const
	{
		return std::any_of(WorldMaps.begin(), WorldMaps.end(), [=](const FStoryWorldMapDocument& Document)
			{ return Document.Patch.Era() == Era && (!Document.bExpansion || bExpandedStory) && Document.Patch.KeepsVehicleAway(Story); });
	}

	std::optional<std::vector<uint8>> FStoryPacks::BattleEventMessages(int32 Event, std::span<const uint8> Messages, bool bExpandedStory) const
	{
		std::optional<std::vector<uint8>> Changed;
		for (const FStoryEventDocument& Document : Events)
		{
			if (Document.Patch.Event() != Event || (Document.bExpansion && !bExpandedStory))
			{
				continue;
			}
			if (std::optional<std::vector<uint8>> Next = Document.Patch.Apply(Changed ? std::span<const uint8>(*Changed) : Messages))
			{
				Changed = std::move(Next);
			}
		}
		return Changed;
	}

	std::optional<FError> FStoryPacks::ApplyToMap(int32 Map, FEventScripts& Scripts, std::vector<uint8>& Messages, const FEventOptions& Options,
		bool bExpandedStory, const IDiscText* Disc) const
	{
		for (const FStoryDocument& Document : All)
		{
			if (Document.Patch.Map() != Map || Document.Patch.Base() || (Document.Option != nullptr && !(Options.*Document.Option))
				|| (Document.bExpansion && !bExpandedStory))
			{
				continue;
			}
			const TResult<bool> Applied = Document.Patch.Apply(Scripts, Messages, Disc);
			if (!Applied)
			{
				return MakeError(Document.Name + ": " + Applied.Error().Message);
			}
		}
		return std::nullopt;
	}

	const FStoryDocument* FStoryPacks::MapOfItsOwn(int32 Map, bool bExpandedStory) const
	{
		const FStoryDocument* Maker = MakerOf(Map);
		return Maker != nullptr && (!Maker->bExpansion || bExpandedStory) ? Maker : nullptr;
	}

	const FStoryDocument* FStoryPacks::MakerOf(int32 Map) const
	{
		const auto Found = std::find_if(All.begin(), All.end(), [Map](const FStoryDocument& Document) { return Document.Patch.Base() && Document.Patch.Map() == Map; });
		return Found != All.end() ? &*Found : nullptr;
	}
}
