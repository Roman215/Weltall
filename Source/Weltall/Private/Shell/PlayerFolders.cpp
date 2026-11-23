#include "Shell/PlayerFolders.h"

#include "Foundation/FolderNames.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "Misc/Paths.h"
#include "Shell/PortableProfile.h"
#include "WeltallLog.h"

namespace
{
	/** An XDG base directory: its variable, or its default under the user's home. */
	FString XdgFolder(const TCHAR* Variable, const TCHAR* Default)
	{
		const FString Folder = FPlatformMisc::GetEnvironmentVariable(Variable);
		return Folder.IsEmpty() ? FPaths::Combine(FPlatformProcess::UserHomeDir(), Default) : Folder;
	}

	FString UserSettingsFolder()
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

	FString UserSavesFolder()
	{
#if PLATFORM_WINDOWS || PLATFORM_MAC
		return FPaths::Combine(UserSettingsFolder(), TEXT("Saves"));
#elif PLATFORM_ANDROID
		return FPaths::Combine(FPlatformMisc::GamePersistentDownloadDir(), TEXT("Saves"));
#else
		return FPaths::Combine(XdgFolder(TEXT("XDG_DATA_HOME"), TEXT(".local/share")), TEXT("Weltall"), TEXT("Saves"));
#endif
	}

#if PLATFORM_WINDOWS || PLATFORM_LINUX
	const FString& PortableFolder()
	{
		static const FString Folder = []
		{
			if (!FPlatformProperties::RequiresCookedData())
			{
				return FString();
			}
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::RootDir(), TEXT("Portable")));
			if (!IFileManager::Get().DirectoryExists(*Path))
			{
				return FString();
			}
			const FString Settings = FPaths::Combine(UserSettingsFolder(), TEXT("Settings.txt"));
			if (const auto Failed = ImportPortableProfile(std::filesystem::path(*Path), std::filesystem::path(*Settings), std::filesystem::path(*UserSavesFolder())))
			{
				UE_LOG(LogWeltall, Warning, TEXT("Portable profile import failed. Using the user profile for this run: %hs"), Failed->Message.c_str());
				return FString();
			}
			return Path;
		}();
		return Folder;
	}
#endif
}

FString PlayerSettingsFolder()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	if (const FString& Portable = PortableFolder(); !Portable.IsEmpty())
	{
		return Portable;
	}
#endif
	return UserSettingsFolder();
}

FString PlayerSavesFolder()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	if (const FString& Portable = PortableFolder(); !Portable.IsEmpty())
	{
		return FPaths::Combine(Portable, TEXT("Saves"));
	}
#endif
	return UserSavesFolder();
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
