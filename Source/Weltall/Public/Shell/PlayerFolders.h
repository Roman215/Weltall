#pragma once

#include "CoreMinimal.h"

/**
 * Weltall's folder among the user's settings: Windows' local application data, macOS's application support, the app's
 * own folder on Android's shared storage, XDG's config home elsewhere.
 */
WELTALL_API FString PlayerSettingsFolder();

/** Weltall's folder for the player's saves: the settings' folder's "Saves" on Windows and macOS, the app's own folder's on Android, XDG's data home elsewhere. */
WELTALL_API FString PlayerSavesFolder();
