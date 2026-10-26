#include "Shell/GameDiscsSubsystem.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WeltallLog.h"

#if PLATFORM_ANDROID
#include <sys/stat.h>
#endif

FString UGameDiscsSubsystem::GamesFolder()
{
	FString Given;
	if (FParse::Value(FCommandLine::Get(), TEXT("-Games="), Given))
	{
		return FPaths::ConvertRelativePathToFull(Given);
	}
#if PLATFORM_ANDROID
	// Nothing lies beside an app the player can reach: its own folder on the shared storage, which a computer sees over
	// USB as Android/data/<package>/files.
	const FString Beside = FPlatformMisc::GamePersistentDownloadDir();
#else
	// A packaged game's root holds its executable; the project's folder lies a level below it.
	const FString Beside = FPlatformProperties::RequiresCookedData() ? FPaths::RootDir() : FPaths::ProjectDir();
#endif
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(Beside, TEXT("Games")));
}

void UGameDiscsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if PLATFORM_ANDROID
	// The player copies the discs in from a computer, which finds the folder made for them (Android may refuse a
	// computer a new folder inside an app's own) and open to the shared storage's group, as the app's own folders are
	// not: adb's shell, in that group, is refused a file in them.
	const FString Games = GamesFolder();
	IFileManager::Get().MakeDirectory(*Games, true);
	chmod(TCHAR_TO_UTF8(*Games), S_IRWXU | S_IRWXG | S_ISGID);
#endif
	Discs = Weltall::FGameLibrary::Scan(std::filesystem::path(*GamesFolder()));
	UE_LOG(LogWeltall, Log, TEXT("Found %d of the game's discs in %s"), static_cast<int32>(Discs.Releases().size()), *GamesFolder());
	LogNotes();
}

const Weltall::FIso9660* UGameDiscsSubsystem::Find(Weltall::EDiscRelease Release) const
{
	return Discs.Find(Release);
}

void UGameDiscsSubsystem::Rescan()
{
	const std::size_t Before = Discs.Releases().size();
	Discs.Adopt(Weltall::FGameLibrary::Scan(std::filesystem::path(*GamesFolder())));
	if (Discs.Releases().size() != Before)
	{
		UE_LOG(LogWeltall, Log, TEXT("Found %d of the game's discs in %s"), static_cast<int32>(Discs.Releases().size()), *GamesFolder());
		LogNotes();
	}
}

void UGameDiscsSubsystem::LogNotes() const
{
	for (const Weltall::FLibraryNote& Note : Discs.Notes())
	{
		const std::u8string File = std::filesystem::path(Note.File).make_preferred().u8string();
		UE_LOG(LogWeltall, Warning, TEXT("%s: %hs"), UTF8_TO_TCHAR(reinterpret_cast<const char*>(File.c_str())), Note.Text.c_str());
	}
}
