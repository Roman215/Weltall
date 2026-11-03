#include "Story/Disc2Export.h"

#include "Battle/BattleEvent.h"
#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Foundation/NumberText.h"
#include "Media/AssetFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/Iso9660.h"
#include "Story/BattleEventTextPatch.h"
#include "Text/MessageTable.h"

#include <span>
#include <string>

namespace Weltall
{
	namespace
	{
		/** Documents a step writes: a map's listing takes a few milliseconds, so a step stays well under a frame of a worker's time. */
		constexpr int32 FilesPerStep = 8;
		constexpr std::string_view DocumentsFolder = "disc2";
		constexpr std::string_view InfoFile = "mod.txt";
		constexpr std::string_view Info =
			"; Disc 2 as your disc holds it, written by Weltall's Export Disc 2: every map the\n"
			"; second disc plays and every battle event's text, in the disc2 folder. As it is,\n"
			"; it plays as the original Disc 2; edit a file and it is a Disc 2 expansion\n"
			"; (Docs/Modding.md, \"Disc 2 expansions\"). Rename the folder and the name below to\n"
			"; make it your own, and add \"author\" and \"version\" lines.\n"
			"name Disc 2 (export)\n";

		std::string DocumentName(std::string_view Kind, int32 Number)
		{
			std::string Name(Kind);
			NumberText::Append(Name, Number, 10, 3);
			Name += ".txt";
			return Name;
		}

		TResult<bool> Publish(const std::filesystem::path& Folder, std::string_view Relative, std::string_view Text)
		{
			return PublishAssetFile(Folder, std::filesystem::path(std::u8string(Relative.begin(), Relative.end())),
				std::span<const uint8>(reinterpret_cast<const uint8*>(Text.data()), Text.size()));
		}
	}

	struct FDisc2Export::FState
	{
		FState(FIso9660 InDisc, FGameFileIndex InIndex, FBattleEventArchive InEvents, std::filesystem::path InFolder)
			: Disc(std::move(InDisc)), Index(std::move(InIndex)), Events(std::move(InEvents)), Folder(std::move(InFolder))
		{
		}

		FIso9660 Disc;
		FGameFileIndex Index;
		FBattleEventArchive Events;
		std::filesystem::path Folder;
		/** The next of the maps, then of the events after them. */
		int32 Next = 0;
		bool bInfoWritten = false;
	};

	FDisc2Export::FDisc2Export(std::unique_ptr<FState> InState) : State(std::move(InState)) {}

	FDisc2Export::~FDisc2Export() = default;

	TResult<std::unique_ptr<FDisc2Export>> FDisc2Export::Open(const FIso9660& Disc, const FGameFileIndex& Index, std::filesystem::path ModFolder)
	{
		// The mods' files are the game's as it plays; the player's Assets (and its store, which is the game thread's) are not read.
		FIso9660 Read = Disc;
		Read.Image().SetAssetStore(nullptr);
		TResult<FBattleEventArchive> Events = FBattleEventArchive::Load(Read.Image(), Index);
		if (!Events)
		{
			return Events.Error();
		}
		auto State = std::make_unique<FState>(std::move(Read), Index, std::move(*Events), std::move(ModFolder));
		return std::unique_ptr<FDisc2Export>(new FDisc2Export(std::move(State)));
	}

	TResult<bool> FDisc2Export::Step()
	{
		if (!State->bInfoWritten)
		{
			if (const TResult<bool> Published = Publish(State->Folder, InfoFile, Info); !Published)
			{
				return Published.Error();
			}
			State->bInfoWritten = true;
		}
		const std::string Documents = std::string(DocumentsFolder) + "/";
		for (int32 Written = 0; Written < FilesPerStep && State->Next < TotalFiles(); ++Written, ++State->Next)
		{
			const FCdImage& Image = State->Disc.Image();
			TResult<std::string> Text = MakeError("");
			std::string Name;
			if (State->Next < FieldMapCount)
			{
				const int32 Map = State->Next;
				const TResult<FFieldArchive> Archive = LoadFieldArchive(Image, State->Index, Map);
				if (!Archive && !HoldsFieldMap(Image, State->Index, Map))
				{
					continue;
				}
				if (!Archive)
				{
					return Archive.Error();
				}
				Text = WriteWholeFieldMapText(Map, Archive->Section(EFieldSection::EventScripts), Archive->Section(EFieldSection::Messages));
				Name = DocumentName("map-", Map);
			}
			else
			{
				const int32 Event = State->Next - FieldMapCount;
				const std::span<const uint8> Messages = State->Events.DiscMessages(Event);
				const TResult<FMessageTable> Table = FMessageTable::Parse(Messages);
				if (Table && Table->Count() == 0)
				{
					continue;
				}
				Text = WriteWholeBattleEventText(Event, Messages);
				Name = DocumentName("battle-event-", Event);
			}
			if (!Text)
			{
				return MakeError(Name + ": " + Text.Error().Message);
			}
			if (const TResult<bool> Published = Publish(State->Folder, Documents + Name, *Text); !Published)
			{
				return Published.Error();
			}
		}
		return State->Next >= TotalFiles();
	}

	int32 FDisc2Export::CompletedFiles() const
	{
		return State->Next;
	}

	int32 FDisc2Export::TotalFiles() const
	{
		return FieldMapCount + State->Events.Count();
	}
}
