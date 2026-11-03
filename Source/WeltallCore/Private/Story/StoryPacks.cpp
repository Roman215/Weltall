#include "Story/StoryPacks.h"

#include "Foundation/HiddenFiles.h"
#include "Story/DiscIdentity.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
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

	std::string_view FStoryPacks::KindWord(std::string_view Text)
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

	TResult<FStoryPacks> FStoryPacks::Read(const std::filesystem::path& Folder)
	{
		FStoryPacks Packs;
		const auto IsFolder = [](const std::filesystem::directory_entry& Entry)
		{
			std::error_code Error;
			return Entry.is_directory(Error);
		};
		for (const std::filesystem::path& Pack : EntriesOf(Folder, IsFolder))
		{
			const std::string Name = NameOf(Pack);
			const std::optional<FError> Error = ReadDocuments(Pack, Name, [&Packs, &Name](std::string_view File, std::string_view Text)
				{ return Packs.Add(Name, File, Text); });
			if (Error)
			{
				return *Error;
			}
		}
		return Packs;
	}

	std::optional<FError> FStoryPacks::ReadExpansion(const std::filesystem::path& Folder, std::string_view Mod)
	{
		const std::string Pack = std::string(Mod) + "/" + std::string(ExpansionPack);
		return ReadDocuments(Folder, Pack, [this, &Pack](std::string_view File, std::string_view Text) { return AddNamed(Pack + "/" + std::string(File), true, Text); });
	}

	std::optional<FError> FStoryPacks::ReadDocuments(const std::filesystem::path& Folder, std::string_view Pack,
		const std::function<std::optional<FError>(std::string_view, std::string_view)>& Added)
	{
		const auto IsDocument = [](const std::filesystem::directory_entry& Entry)
		{
			std::error_code Error;
			return Entry.is_regular_file(Error) && Entry.path().extension() == std::filesystem::path(DocumentExtension);
		};
		for (const std::filesystem::path& Document : EntriesOf(Folder, IsDocument))
		{
			std::ifstream File(Document, std::ios::binary);
			const std::string Text{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
			if (!File.good() && !File.eof())
			{
				return MakeError(std::string(Pack) + "/" + NameOf(Document) + " cannot be read");
			}
			if (std::optional<FError> Error = Added(NameOf(Document), Text))
			{
				return Error;
			}
		}
		return std::nullopt;
	}

	std::optional<FError> FStoryPacks::Add(std::string_view Pack, std::string_view File, std::string_view Text)
	{
		return AddNamed(std::string(Pack) + "/" + std::string(File), Pack == ExpansionPack, Text);
	}

	std::optional<FError> FStoryPacks::AddNamed(const std::string& Name, bool bExpansionDocument, std::string_view Text)
	{
		if (Text.starts_with(ByteOrderMark))
		{
			Text.remove_prefix(ByteOrderMark.size());
		}
		// What Export Disc 2 wrote and nobody changed is the disc's own: it is not read, which an export's size makes worth it.
		if (ExportMark::LeftAsWritten(Text))
		{
			++Unchanged;
			return std::nullopt;
		}
		if (KindWord(Text) == BattleEventWord)
		{
			TResult<FBattleEventTextPatch> Event = FBattleEventTextPatch::Parse(Text);
			if (!Event)
			{
				return MakeError(Name + ": " + Event.Error().Message);
			}
			const auto Whole = [&Event](const FStoryEventDocument& Other) { return Other.Patch.IsWhole() && Other.Patch.Event() == Event->Event(); };
			if (const auto Given = std::find_if(Events.begin(), Events.end(), Whole); Event->IsWhole() && Given != Events.end())
			{
				return MakeError(Name + ": battle event " + std::to_string(Event->Event()) + " is given whole by " + Given->Name + " already");
			}
			Events.push_back({Name, std::move(*Event), bExpansionDocument});
			return std::nullopt;
		}
		if (KindWord(Text) == WorldMapWord)
		{
			TResult<FWorldMapPatch> Places = FWorldMapPatch::Parse(Text);
			if (!Places)
			{
				return MakeError(Name + ": " + Places.Error().Message);
			}
			WorldMaps.push_back({Name, std::move(*Places), bExpansionDocument});
			return std::nullopt;
		}
		if (KindWord(Text) == WayBackWord)
		{
			if (!bExpansionDocument || Back)
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
		if (KindWord(Text) == SubtitlesWord)
		{
			if (!bExpansionDocument || Titles)
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
		const auto Whole = [&Patch](const FStoryDocument& Other) { return Other.Patch.IsWhole() && Other.Patch.Map() == Patch->Map(); };
		if (const auto Given = std::find_if(All.begin(), All.end(), Whole); Patch->IsWhole() && Given != All.end())
		{
			return MakeError(Name + ": map " + std::to_string(Patch->Map()) + " is given whole by " + Given->Name + " already");
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
		All.push_back({Name, std::move(*Patch), Option, bExpansionDocument});
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

	std::optional<std::vector<uint8>> FStoryPacks::BattleEventMessages(int32 Event, std::span<const uint8> Messages, std::span<const uint8> DiscMessages,
		bool bExpandedStory, std::vector<const FStoryEventDocument*>* LeftOut) const
	{
		const auto Unfit = [LeftOut](const FStoryEventDocument& Document)
		{
			if (LeftOut != nullptr)
			{
				LeftOut->push_back(&Document);
			}
		};
		std::optional<std::vector<uint8>> Changed;
		const std::optional<FSha1::FDigest> Identity = MessageTableIdentity(DiscMessages);
		// A whole table stands for the disc's first, and the beginnings are made in the table it gives.
		for (const FStoryEventDocument& Document : Events)
		{
			if (Document.Patch.IsWhole() && Document.Patch.Event() == Event && (!Document.bExpansion || bExpandedStory))
			{
				Changed = Document.Patch.Replace(DiscMessages);
				if (!Changed)
				{
					Unfit(Document);
				}
			}
		}
		for (const FStoryEventDocument& Document : Events)
		{
			if (Document.Patch.Event() != Event || Document.Patch.IsWhole() || (Document.bExpansion && !bExpandedStory))
			{
				continue;
			}
			if (std::optional<std::vector<uint8>> Next = Document.Patch.Apply(Changed ? std::span<const uint8>(*Changed) : Messages, Identity ? &*Identity : nullptr))
			{
				Changed = std::move(Next);
			}
			else
			{
				Unfit(Document);
			}
		}
		return Changed;
	}

	std::optional<FError> FStoryPacks::ApplyToMap(int32 Map, FEventScripts& Scripts, std::vector<uint8>& Messages, const FEventOptions& Options,
		bool bExpandedStory, const IDiscText* Disc, const FSha1::FDigest* Identity, std::vector<const FStoryDocument*>* LeftOut) const
	{
		for (const FStoryDocument& Document : All)
		{
			if (Document.Patch.Map() != Map || Document.Patch.Base() || Document.Patch.IsWhole() || (Document.Option != nullptr && !(Options.*Document.Option))
				|| (Document.bExpansion && !bExpandedStory))
			{
				continue;
			}
			const TResult<bool> Applied = Document.Patch.Apply(Scripts, Messages, Disc, Identity);
			if (!Applied)
			{
				return MakeError(Document.Name + ": " + Applied.Error().Message);
			}
			if (!*Applied && LeftOut != nullptr)
			{
				LeftOut->push_back(&Document);
			}
		}
		return std::nullopt;
	}

	const FStoryDocument* FStoryPacks::MapOfItsOwn(int32 Map, bool bExpandedStory) const
	{
		const FStoryDocument* Maker = MakerOf(Map);
		return Maker != nullptr && (!Maker->bExpansion || bExpandedStory) ? Maker : nullptr;
	}

	const FStoryDocument* FStoryPacks::WholeMap(int32 Map, bool bExpandedStory) const
	{
		const auto Found = std::find_if(All.begin(), All.end(), [=](const FStoryDocument& Document)
			{ return Document.Patch.IsWhole() && Document.Patch.Map() == Map && (!Document.bExpansion || bExpandedStory); });
		return Found != All.end() ? &*Found : nullptr;
	}

	const FStoryDocument* FStoryPacks::MakerOf(int32 Map) const
	{
		const auto Found = std::find_if(All.begin(), All.end(), [Map](const FStoryDocument& Document) { return Document.Patch.Base() && Document.Patch.Map() == Map; });
		return Found != All.end() ? &*Found : nullptr;
	}
}
