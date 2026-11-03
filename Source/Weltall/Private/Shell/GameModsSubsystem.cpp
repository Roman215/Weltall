#include "Shell/GameModsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Mods/DeviceMods.h"
#include "Shell/DeviceFilesSubsystem.h"
#include "Shell/GameDiscsSubsystem.h"
#include "Shell/PlayerFolders.h"
#include "Shell/PlayerSettingsSubsystem.h"
#include "WeltallLog.h"

FString UGameModsSubsystem::ModsFolder()
{
	return PlayerMadeFolder(FPaths::GetPath(UGameDiscsSubsystem::GamesFolder()), TEXT("Mods"));
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
	const Weltall::FPlayerSettings& Settings = GetGameInstance()->GetSubsystem<UPlayerSettingsSubsystem>()->Settings();
	InUse = Settings.PerfectWorks;
	ItalianInUse = Found.Italian ? Settings.Italian : Weltall::FItalianOptions{};
	if (Found.Italian)
	{
		const auto Movies = [this](bool bJapanese)
		{
			return Found.Italian->MoviePatch(1, bJapanese) || Found.Italian->MoviePatch(2, bJapanese);
		};
		UE_LOG(LogWeltall, Log, TEXT("The Italian translation's patch found: %hs (movie patches: English sound %s, Japanese sound %s): %s"),
			Found.Italian->Path().filename().string().c_str(), Movies(false) ? TEXT("yes") : TEXT("no"), Movies(true) ? TEXT("yes") : TEXT("no"),
			ItalianInUse.bEnabled ? TEXT("in use") : TEXT("not in use"));
		if (ItalianInUse.bEnabled && InUse.bEnabled)
		{
			UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build is not used with the Italian translation"));
		}
	}
	if (ItalianInUse.bEnabled)
	{
		InUse.bEnabled = false;
	}
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
	if (const std::optional<Weltall::FError> Error = Weltall::ApplyMods(Library, Found, InUse, ItalianInUse))
	{
		UE_LOG(LogWeltall, Warning, TEXT("The mods' files are not used: %hs"), Error->Message.c_str());
	}
}

void UGameModsSubsystem::FindAgain()
{
	const Weltall::FFoundMods Again = Weltall::FindMods(std::filesystem::path(*ModsFolder()));
	if (!Found.PerfectWorks && Again.PerfectWorks)
	{
		Found.PerfectWorks = Again.PerfectWorks;
		UE_LOG(LogWeltall, Log, TEXT("Perfect Works Build %hs added in %s: offered now, in use from the next start"), Found.PerfectWorks->Version().c_str(), *ModsFolder());
	}
	if (!Found.Italian && Again.Italian)
	{
		Found.Italian = Again.Italian;
		UE_LOG(LogWeltall, Log, TEXT("The Italian translation's patch added in %s: offered now, in use from the next start"), *ModsFolder());
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
	return {Found.PerfectWorks.has_value(), Found.PerfectWorks ? Found.PerfectWorks->Version() : std::string(), InUse, Found.Italian.has_value(), ItalianInUse};
}
