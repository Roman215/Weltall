#pragma once

#include "CoreMinimal.h"
#include "Menu/MenuDeviceFiles.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <filesystem>
#include <vector>

#include "DeviceFilesSubsystem.generated.h"

/**
 * The player's files brought into the game and out of it through Android's own file picker, where nothing outside the
 * game reaches its folder (computers and file managers cannot reach Android/data): disc images, or every one in a
 * folder, copied into the Games folder, a card image or save's file into a folder to import from, every save into one zip and back from one, the logs
 * into another zip, a card image to a file the player names, and a mod or pack from a zip or a folder into Mods or
 * Assets. One transfer at a time, copied on a thread of Android's while the game runs on; nothing asks for a
 * permission. Elsewhere the game reads and writes the computer's folders itself, and Files() is none.
 */
UCLASS()
class WELTALL_API UDeviceFilesSubsystem : public UGameInstanceSubsystem, public Weltall::IMenuDeviceFiles
{
	GENERATED_BODY()

public:
	/** Whether the player's files go through the device's picker here. */
	static constexpr bool UsesPicker() { return PLATFORM_ANDROID != 0; }
	/** The transfers as the menus and the game's notices take them, none where the picker is not used. */
	Weltall::IMenuDeviceFiles* Files() { return UsesPicker() ? this : nullptr; }
	/** The folder holding Games, Mods and Assets. */
	static std::filesystem::path PlayerFoldersRoot();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual Weltall::FTransferStatus Status() const override;
	virtual bool Start(Weltall::EDeviceTransfer Kind, const std::filesystem::path& File = {}) override;
	virtual std::vector<Weltall::FModItem> Mods() const override;
	virtual Weltall::EModRemoval RemoveMod(const std::filesystem::path& Path) override;

	/**
	 * The game takes what a transfer added, which the player added to use: Perfect Works offered at once and switched on
	 * from the next start, External assets switched on for a pack (its pictures and songs found again).
	 */
	void FollowAddedMod(Weltall::EAddedContent Content);
};
