#include "Shell/GameModsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Mods/DeviceMods.h"
#include "Shell/DeviceFilesSubsystem.h"
#include "Shell/GameDiscsSubsystem.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "WeltallLog.h"

FString UGameModsSubsystem::ModsFolder()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GetPath(UGameDiscsSubsystem::GamesFolder()), TEXT("Mods")));
}

void UGameModsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGameDiscsSubsystem>();
	Collection.InitializeDependency<UPlayerSettingsSubsystem>();
	if (UDeviceFilesSubsystem::UsesPicker())
	{
		// A release added or removed while the game read the one in use takes effect now, before mods are looked for.
		Weltall::FinishModChanges(UDeviceFilesSubsystem::PlayerFoldersRoot());
	}
	Found = Weltall::FindMods(std::filesystem::path(*ModsFolder()));
	InUse = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings().PerfectWorks;
	if (Found.PerfectWorks)
	{
		UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build %hs found in %s: %s"), Found.PerfectWorks->Version().c_str(), *ModsFolder(),
			InUse.bEnabled ? TEXT("in use") : TEXT("not in use"));
		if (Found.PerfectWorks->MovieSubtitles(1) == nullptr && Found.PerfectWorks->MovieSubtitles(2) == nullptr)
		{
			UE_LOG(LogWeltall, Warning, TEXT("Perfect Works Build %hs gives no movie subtitles Weltall can read"), Found.PerfectWorks->Version().c_str());
		}
		if (!Found.PerfectWorks->KnowsBattleEventCode())
		{
			UE_LOG(LogWeltall, Warning, TEXT("Perfect Works Build %hs changes the battle dialogue's windows in a way Weltall doesn't know: the battles keep the original dialogue"),
				Found.PerfectWorks->Version().c_str());
		}
		if (InUse.bRebalancedParty && !Found.PerfectWorks->KnowsRebalanceCode())
		{
			UE_LOG(LogWeltall, Warning, TEXT("Perfect Works Build %hs rebalances the party and items with code Weltall doesn't know: the game keeps the original party, items and rules"),
				Found.PerfectWorks->Version().c_str());
		}
		if (InUse.bRebalancedEnemies && !Found.PerfectWorks->KnowsEnemiesCode())
		{
			UE_LOG(LogWeltall, Warning, TEXT("Perfect Works Build %hs rebalances the enemies with code Weltall doesn't know: the game keeps the original enemies"),
				Found.PerfectWorks->Version().c_str());
		}
		if (InUse.Arena != Weltall::EPerfectWorksArena::Original && !Found.PerfectWorks->KnowsArenaCode(InUse.Arena, InUse.bScript))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Perfect Works Build %hs rebalances the Battling arena with code Weltall doesn't know: the arena keeps its original gears and rules"),
				Found.PerfectWorks->Version().c_str());
		}
		if (Found.PerfectWorks->UsesRebalancedParty(InUse))
		{
			UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build's rebalanced party and items in use, with its rules"));
		}
		if (Found.PerfectWorks->UsesArena(InUse) != Weltall::EPerfectWorksArena::Original)
		{
			UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build's arena (%s mode) in use, with its rules"),
				Found.PerfectWorks->UsesArena(InUse) == Weltall::EPerfectWorksArena::Expert ? TEXT("Expert") : TEXT("Basic"));
		}
	}
	else
	{
		InUse.bEnabled = false;
	}
	Refresh();
}

void UGameModsSubsystem::Refresh()
{
	const Weltall::FGameLibrary& Library = GetGameInstance()->GetSubsystem<UGameDiscsSubsystem>()->Library();
	if (const std::optional<Weltall::FError> Error = Weltall::ApplyMods(Library, Found, InUse))
	{
		UE_LOG(LogWeltall, Warning, TEXT("The mods' files are not used: %hs"), Error->Message.c_str());
	}
}

void UGameModsSubsystem::FindAgain()
{
	if (Found.PerfectWorks)
	{
		return;
	}
	Found = Weltall::FindMods(std::filesystem::path(*ModsFolder()));
	if (Found.PerfectWorks)
	{
		UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build %hs added in %s: offered now, in use from the next start"), Found.PerfectWorks->Version().c_str(), *ModsFolder());
	}
}

std::array<const Weltall::FPerfectWorksSubtitles*, 2> UGameModsSubsystem::PerfectWorksSubtitles() const
{
	if (!Found.PerfectWorks)
	{
		return {};
	}
	return {Found.PerfectWorks->MovieSubtitles(1), Found.PerfectWorks->MovieSubtitles(2)};
}

Weltall::FMenuMods UGameModsSubsystem::MenuMods() const
{
	return {Found.PerfectWorks.has_value(), Found.PerfectWorks ? Found.PerfectWorks->Version() : std::string(), InUse};
}
