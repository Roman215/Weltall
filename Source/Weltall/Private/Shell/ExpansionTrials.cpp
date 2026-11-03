#include "Shell/ExpansionTrials.h"

#include "Async/Async.h"
#include "Media/CdImage.h"
#include "Media/GameFileIndex.h"
#include "Story/StoryLibrary.h"
#include "WeltallLog.h"

#include <utility>

FExpansionTrials::~FExpansionTrials()
{
	if (Running.IsValid())
	{
		Running.Wait();
	}
}

void FExpansionTrials::Start(Weltall::FStoryLibrary& InLibrary, const Weltall::FCdImage& Image, const Weltall::FGameFileIndex& Index,
	const Weltall::FEventOptions& Options)
{
	if (TriedOn == &Image)
	{
		return;
	}
	if (Running.IsValid())
	{
		Running.Wait();
		Keep(Running.Consume());
	}
	Library = &InLibrary;
	TriedOn = &Image;
	std::vector<std::pair<std::string, const Weltall::FStoryPacks*>> Playable;
	for (const Weltall::FStoryLibrary::FExpansion& Expansion : InLibrary.Expansions())
	{
		if (Expansion.Packs)
		{
			Playable.emplace_back(Expansion.Mod.Folder, &*Expansion.Packs);
		}
	}
	if (Playable.empty())
	{
		return;
	}
	Weltall::FCdImage Read = Image;
	Read.SetAssetStore(nullptr);
	Running = Async(EAsyncExecution::ThreadPool, [Read = std::move(Read), Index, Options, Playable = std::move(Playable)]()
	{
		std::vector<FTried> Tried;
		for (const auto& [Folder, Packs] : Playable)
		{
			Tried.push_back({Folder, Weltall::TryExpansion(*Packs, Read, Index, Options)});
		}
		return Tried;
	});
}

bool FExpansionTrials::Take()
{
	if (!Running.IsValid() || !Running.IsReady())
	{
		return false;
	}
	Keep(Running.Consume());
	return true;
}

void FExpansionTrials::Keep(std::vector<FTried> Tried)
{
	for (FTried& Expansion : Tried)
	{
		const char* Folder = Expansion.Folder.c_str();
		if (!Expansion.Trial)
		{
			UE_LOG(LogWeltall, Warning, TEXT("The Disc 2 expansion in Mods/%hs could not be tried on the disc: %hs"), Folder, Expansion.Trial.Error().Message.c_str());
			continue;
		}
		for (const std::string& Document : Expansion.Trial->ForAnotherDisc)
		{
			UE_LOG(LogWeltall, Warning, TEXT("Mods/%hs is written for another disc: the game cannot use it with this one"), Document.c_str());
		}
		for (const Weltall::FError& Fault : Expansion.Trial->Faults)
		{
			UE_LOG(LogWeltall, Warning, TEXT("The Disc 2 expansion in Mods/%hs stops the game at %hs"), Folder, Fault.Message.c_str());
		}
		if (Expansion.Trial->ForAnotherDisc.empty() && Expansion.Trial->Faults.empty())
		{
			UE_LOG(LogWeltall, Log, TEXT("The Disc 2 expansion in Mods/%hs fits the disc: each of its documents was tried"), Folder);
		}
		Library->SetForAnotherDisc(Expansion.Folder, std::move(Expansion.Trial->ForAnotherDisc));
	}
}
