#pragma once

#include "CoreMinimal.h"
#include "Menu/MenuProgram.h"
#include "Mods/GameMods.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <array>

#include "GameModsSubsystem.generated.h"

/**
 * The player's mods, found in the Mods folder beside Games as the game starts, their files given to the discs as the
 * settings chose then: a change in the options applies from the next start.
 */
UCLASS()
class WELTALL_API UGameModsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static FString ModsFolder();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Every disc found so far given the mods' files the game started with: after the Games folder is looked in again. */
	void Refresh();
	/**
	 * Perfect Works looked for again after one was added during play, where none was found before: the options offer it
	 * at once, the game takes it from the next start. A release found stays the one the game reads until then.
	 */
	void FindAgain();

	/** What the options screen shows of the mods. */
	Weltall::FMenuMods MenuMods() const;
	/** Perfect Works Build's movie subtitles of each disc, null where the mod gives none. */
	std::array<const Weltall::FPerfectWorksSubtitles*, 2> PerfectWorksSubtitles() const;

private:
	Weltall::FFoundMods Found;
	Weltall::FPerfectWorksOptions InUse;
};
