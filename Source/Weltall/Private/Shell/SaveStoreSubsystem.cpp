#include "Shell/SaveStoreSubsystem.h"

#include "Shell/PlayerFolders.h"

void USaveStoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Saves.emplace(std::filesystem::path(*PlayerSavesFolder()));
	StateSlots.emplace(std::filesystem::path(*PlayerStatesFolder()));
}
