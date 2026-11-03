#pragma once

#include "CoreMinimal.h"

#include <filesystem>

class IImageWrapperModule;

/**
 * Whether File lies below Root, which is canonical already, with no link between Root and it: a mod folder cannot
 * redirect a write or a read. Root itself may be reached through links.
 */
WELTALL_API bool IsSafeTexturePath(const FString& File, const std::filesystem::path& Root);

/** Root as IsSafeTexturePath expects it, empty when it cannot be resolved. */
WELTALL_API std::filesystem::path CanonicalTextureRoot(const FString& Root);

/**
 * Saves RGBA pixels as a PNG beside a partial file of its own, published under File only when no file has that
 * name: an existing original or edit is never replaced. With bFolderSafe, the caller has checked File's folder with
 * IsSafeTexturePath, and only the two names are checked not to be links. Any thread.
 */
WELTALL_API bool WriteTexturePng(IImageWrapperModule& Module, const FString& File, const std::filesystem::path& Root, const uint8* Rgba, int32 Width,
	int32 Height, bool bFolderSafe = false);
