#include "Shell/PlayerFolders.h"

#include "Foundation/FolderNames.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
	/** An XDG base directory: its variable, or its default under the user's home. */
	FString XdgFolder(const TCHAR* Variable, const TCHAR* Default)
	{
		const FString Folder = FPlatformMisc::GetEnvironmentVariable(Variable);
		return Folder.IsEmpty() ? FPaths::Combine(FPlatformProcess::UserHomeDir(), Default) : Folder;
	}
}

FString PlayerSettingsFolder()
{
#if PLATFORM_WINDOWS
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("Weltall"));
#elif PLATFORM_MAC
	return FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Library/Application Support/Weltall"));
#elif PLATFORM_ANDROID
	return FPaths::Combine(FPlatformMisc::GamePersistentDownloadDir(), TEXT("Settings"));
#else
	return FPaths::Combine(XdgFolder(TEXT("XDG_CONFIG_HOME"), TEXT(".config")), TEXT("Weltall"));
#endif
}

FString PlayerSavesFolder()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return FPaths::Combine(PlayerSettingsFolder(), TEXT("Saves"));
#elif PLATFORM_ANDROID
	return FPaths::Combine(FPlatformMisc::GamePersistentDownloadDir(), TEXT("Saves"));
#else
	return FPaths::Combine(XdgFolder(TEXT("XDG_DATA_HOME"), TEXT(".local/share")), TEXT("Weltall"), TEXT("Saves"));
#endif
}

FString PlayerStatesFolder()
{
	return FPaths::Combine(FPaths::GetPath(PlayerSavesFolder()), TEXT("States"));
}

FString PlayerMadeFolder(const FString& Parent, const TCHAR* Name)
{
	const std::filesystem::path Found = Weltall::FolderIgnoringCase(std::filesystem::path(*Parent), TCHAR_TO_UTF8(Name));
	return FPaths::ConvertRelativePathToFull(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Found.generic_u8string().c_str())));
}
