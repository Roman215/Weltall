#pragma once

#include "CoreMinimal.h"
#include "Game/SaveStateSlots.h"
#include "Game/SaveStore.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <optional>

#include "SaveStoreSubsystem.generated.h"

/** The player's saves: the eight stores the Menu's File screen shows, in the player's saves folder; and the save states' slots beside them. */
UCLASS()
class WELTALL_API USaveStoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	Weltall::FSaveStore& Store() { return *Saves; }
	Weltall::FSaveStateSlots& States() { return *StateSlots; }

private:
	std::optional<Weltall::FSaveStore> Saves;
	std::optional<Weltall::FSaveStateSlots> StateSlots;
};
