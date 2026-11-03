#pragma once

#include "CoreMinimal.h"

/**
 * Weltall's folder among the user's settings: Windows' local application data, macOS's application support, the app's
 * own folder on Android's shared storage, XDG's config home elsewhere.
 */
WELTALL_API FString PlayerSettingsFolder();

/** Weltall's folder for the player's saves: the settings' folder's "Saves" on Windows and macOS, the app's own folder's on Android, XDG's data home elsewhere. */
WELTALL_API FString PlayerSavesFolder();

/** Weltall's folder for the player's save states: "States", beside the saves' folder. */
WELTALL_API FString PlayerStatesFolder();

/**
 * Name's folder inside Parent as the player made it, whatever the case of its letters (Weltall::FolderIgnoringCase):
 * on Linux a player's "Textures" is the game's "textures" too. A full path with forward slashes.
 */
WELTALL_API FString PlayerMadeFolder(const FString& Parent, const TCHAR* Name);
