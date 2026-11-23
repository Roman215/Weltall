#pragma once

#include "CoreMinimal.h"

/**
 * Weltall's settings folder. A desktop package with a Portable folder imports its user profile on first use.
 */
WELTALL_API FString PlayerSettingsFolder();

/** Weltall's folder for the player's saves, under Portable when using a portable profile. */
WELTALL_API FString PlayerSavesFolder();

/** Weltall's folder for the player's save states: "States", beside the saves' folder. */
WELTALL_API FString PlayerStatesFolder();

/**
 * Name's folder inside Parent as the player made it, whatever the case of its letters (Weltall::FolderIgnoringCase):
 * on Linux a player's "Textures" is the game's "textures" too. A full path with forward slashes.
 */
WELTALL_API FString PlayerMadeFolder(const FString& Parent, const TCHAR* Name);
