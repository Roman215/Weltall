#include "Story/ShareableExpansion.h"

#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Field/FieldMapShareable.h"
#include "Field/FieldScriptFixes.h"
#include "Foundation/HiddenFiles.h"
#include "Media/AssetFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Mods/ExpansionMods.h"
#include "Story/BattleEventTextPatch.h"
#include "Story/DiscIdentity.h"
#include "Story/DiscText.h"
#include "Story/StoryPacks.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** Documents a step reads: one given whole and edited takes some tens of milliseconds to share. */
		constexpr int32 FilesPerStep = 8;
		constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";

		std::string NameOf(const std::filesystem::path& Path)
		{
			const std::u8string Name = Path.generic_u8string();
			return std::string(Name.begin(), Name.end());
		}

		/** Whether a file of the expansion's folder, by its path in it, is one of its documents: a .txt file of its disc2 folder. */
		bool IsDocument(const std::filesystem::path& Relative)
		{
			std::string Folder = NameOf(Relative.parent_path());
			std::transform(Folder.begin(), Folder.end(), Folder.begin(), [](char Letter) { return static_cast<char>(std::tolower(static_cast<unsigned char>(Letter))); });
			return Folder == FStoryPacks::ExpansionPack && Relative.extension() == ".txt";
		}

		/** Every visible file under Folder, by its path in it, in the order of their paths; a hidden folder's files are left out with it. */
		TResult<std::vector<std::filesystem::path>> FilesUnder(const std::filesystem::path& Folder)
		{
			std::vector<std::filesystem::path> Files;
			std::error_code Error;
			for (auto Entry = std::filesystem::recursive_directory_iterator(Folder, Error); !Error && Entry != std::filesystem::recursive_directory_iterator();
				Entry.increment(Error))
			{
				if (IsHiddenFile(Entry->path()))
				{
					Entry.disable_recursion_pending();
					continue;
				}
				std::error_code Kind;
				if (Entry->is_regular_file(Kind))
				{
					Files.push_back(std::filesystem::relative(Entry->path(), Folder, Kind));
				}
			}
			if (Error)
			{
				return MakeError(NameOf(Folder.filename()) + " cannot be read");
			}
			std::sort(Files.begin(), Files.end());
			return Files;
		}
	}

	struct FShareableExpansion::FState
	{
		FState(FIso9660 InDisc, const FGameFileIndex& InIndex, FBattleEventArchive InEvents, const FStoryPacks& InPort, std::filesystem::path InExpansion,
			std::vector<std::filesystem::path> InFiles)
			: Disc(std::move(InDisc)), Index(InIndex), Events(std::move(InEvents)), Port(InPort), Expansion(std::move(InExpansion)), Files(std::move(InFiles)),
			  DiscText(Disc.Image(), Index)
		{
		}

		FIso9660 Disc;
		FGameFileIndex Index;
		FBattleEventArchive Events;
		FStoryPacks Port;
		std::filesystem::path Expansion;
		std::vector<std::filesystem::path> Files;
		FGameDiscText DiscText;
		std::size_t Next = 0;
		/** What the copy holds, by its path in it. */
		std::vector<std::pair<std::filesystem::path, std::string>> Copy;
		/** The disc maps the expansion gives whole and edited, and those its other documents patch, each by the document that does. */
		std::map<int32, std::string> Whole;
		std::map<int32, std::string> Patched;
		bool bWritten = false;

		/** A document's text as the copy holds it: the patch a whole map or table amounts to, none where it changes nothing, else as it is. */
		TResult<std::optional<std::string>> Shared(std::string_view Name, std::string_view Text)
		{
			const auto Failed = [Name](const FError& Error) { return MakeError(std::string(Name) + ": " + Error.Message); };
			const std::string_view Word = FStoryPacks::KindWord(Text);
			if (Word == "battle")
			{
				const TResult<FBattleEventTextPatch> Event = FBattleEventTextPatch::Parse(Text);
				if (!Event)
				{
					return Failed(Event.Error());
				}
				if (!Event->IsWhole())
				{
					return std::optional<std::string>(Text);
				}
				const TResult<std::string> Written = Event->Event() < Events.Count() ? WriteShareableBattleEventText(*Event, Events.DiscMessages(Event->Event()))
					: TResult<std::string>(MakeError("the disc has no battle event " + std::to_string(Event->Event())));
				return Written ? (Written->empty() ? std::optional<std::string>() : std::optional<std::string>(*Written)) : TResult<std::optional<std::string>>(Failed(Written.Error()));
			}
			if (Word == "world" || Word == "way" || Word == "subtitles")
			{
				return std::optional<std::string>(Text);
			}
			const TResult<FFieldMapPatch> Map = FFieldMapPatch::Parse(Text);
			if (!Map)
			{
				return Failed(Map.Error());
			}
			if (!Map->IsWhole())
			{
				if (!Map->Base() && Map->Map() < FieldMapCount)
				{
					Patched.emplace(Map->Map(), std::string(Name));
				}
				return std::optional<std::string>(Text);
			}
			const TResult<std::string> Written = SharedMap(*Map);
			if (!Written)
			{
				return Failed(Written.Error());
			}
			if (Written->empty())
			{
				return std::optional<std::string>();
			}
			Whole.emplace(Map->Map(), std::string(Name));
			return std::optional<std::string>(*Written);
		}

		/** The patch a whole map amounts to on the disc's map, kept clear of the port's own changes to it. */
		TResult<std::string> SharedMap(const FFieldMapPatch& Map)
		{
			const TResult<FFieldArchive> Archive = LoadFieldArchive(Disc.Image(), Index, Map.Map());
			if (!Archive)
			{
				return MakeError("the disc holds no map " + std::to_string(Map.Map()) + ": it was written for another disc");
			}
			const std::span<const uint8> Scripts = Archive->Section(EFieldSection::EventScripts);
			const std::span<const uint8> Messages = Archive->Section(EFieldSection::Messages);
			TResult<FEventScripts> Fixed = FEventScripts::Parse(Scripts);
			const std::optional<FSha1::FDigest> Identity = FieldMapIdentity(Scripts, Messages);
			if (!Fixed || !Identity)
			{
				return MakeError("the disc's map " + std::to_string(Map.Map()) + " cannot be read");
			}
			const FEventOptions Every;
			ApplyFieldScriptFixes(Map.Map(), *Fixed, Every);
			std::vector<uint8> FixedMessages(Messages.begin(), Messages.end());
			if (std::optional<FError> Unfit = Port.ApplyToMap(Map.Map(), *Fixed, FixedMessages, Every, false, &DiscText, &*Identity))
			{
				return *Unfit;
			}
			return WriteShareableFieldMapText(Map, Scripts, Messages, DiscText, *Fixed);
		}

		/** The copy, in place of one there: an error where a document patches a map the expansion gives whole, or a file cannot be written. */
		TResult<bool> Write()
		{
			for (const auto& [Map, Name] : Patched)
			{
				if (const auto Given = Whole.find(Map); Given != Whole.end())
				{
					return MakeError(Name + ": it patches map " + std::to_string(Map) + ", which " + Given->second
						+ " gives whole, so it cannot be shared: make its change in that one");
				}
			}
			const std::filesystem::path Folder = CopyFolder(Expansion);
			std::error_code Error;
			std::filesystem::remove_all(Folder, Error);
			if (Error)
			{
				return MakeError(NameOf(Folder.filename()) + " cannot be replaced");
			}
			for (const auto& [Relative, Text] : Copy)
			{
				const TResult<bool> Published = PublishAssetFile(Folder, Relative, std::span<const uint8>(reinterpret_cast<const uint8*>(Text.data()), Text.size()));
				if (!Published)
				{
					return Published.Error();
				}
			}
			bWritten = true;
			return true;
		}
	};

	FShareableExpansion::FShareableExpansion(std::unique_ptr<FState> InState) : State(std::move(InState)) {}

	FShareableExpansion::~FShareableExpansion() = default;

	std::filesystem::path FShareableExpansion::CopyFolder(const std::filesystem::path& Expansion)
	{
		std::filesystem::path Folder = Expansion;
		Folder += std::filesystem::path(std::u8string(Suffix.begin(), Suffix.end()));
		return Folder;
	}

	TResult<std::unique_ptr<FShareableExpansion>> FShareableExpansion::Open(const FIso9660& Disc, const FGameFileIndex& Index, const FStoryPacks& Port,
		std::filesystem::path Expansion)
	{
		const std::string Copy = NameOf(CopyFolder(Expansion).filename());
		if (Copy.size() > ExpansionFolderBytes)
		{
			return MakeError("the copy's folder, " + Copy + ", is longer than a save can keep (" + std::to_string(ExpansionFolderBytes)
				+ " bytes): shorten the expansion's folder name");
		}
		// The mods' files are the game's as it plays; the player's Assets (and its store, which is the game thread's) are not read.
		FIso9660 Read = Disc;
		Read.Image().SetAssetStore(nullptr);
		TResult<FBattleEventArchive> Events = FBattleEventArchive::Load(Read.Image(), Index);
		if (!Events)
		{
			return Events.Error();
		}
		TResult<std::vector<std::filesystem::path>> Files = FilesUnder(Expansion);
		if (!Files)
		{
			return Files.Error();
		}
		auto State = std::make_unique<FState>(std::move(Read), Index, std::move(*Events), Port, std::move(Expansion), std::move(*Files));
		return std::unique_ptr<FShareableExpansion>(new FShareableExpansion(std::move(State)));
	}

	TResult<bool> FShareableExpansion::Step()
	{
		FState& Making = *State;
		if (Making.Next == Making.Files.size())
		{
			return Making.bWritten ? TResult<bool>(true) : Making.Write();
		}
		for (int32 Read = 0; Read < FilesPerStep && Making.Next < Making.Files.size(); ++Read, ++Making.Next)
		{
			const std::filesystem::path& Relative = Making.Files[Making.Next];
			const std::string Name = NameOf(Making.Expansion.filename() / Relative);
			std::ifstream File(Making.Expansion / Relative, std::ios::binary);
			std::string Bytes{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
			if (!File.good() && !File.eof())
			{
				return MakeError(Name + " cannot be read");
			}
			if (!IsDocument(Relative))
			{
				Making.Copy.emplace_back(Relative, std::move(Bytes));
				continue;
			}
			std::string_view Text = Bytes;
			if (Text.starts_with(ByteOrderMark))
			{
				Text.remove_prefix(ByteOrderMark.size());
			}
			if (ExportMark::LeftAsWritten(Text))
			{
				continue;
			}
			TResult<std::optional<std::string>> Shared = Making.Shared(Name, Text);
			if (!Shared)
			{
				return Shared.Error();
			}
			if (*Shared)
			{
				Making.Copy.emplace_back(Relative, std::move(**Shared));
			}
		}
		return false;
	}

	int32 FShareableExpansion::CompletedFiles() const
	{
		return static_cast<int32>(State->Next) + (State->bWritten ? 1 : 0);
	}

	int32 FShareableExpansion::TotalFiles() const
	{
		return static_cast<int32>(State->Files.size()) + 1;
	}
}
