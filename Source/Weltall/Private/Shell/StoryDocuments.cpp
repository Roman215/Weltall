#include "Shell/StoryDocuments.h"

#include "Foundation/HiddenFiles.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Mods/ExpansionMods.h"
#include "WeltallLog.h"

#include <filesystem>
#include <string>

namespace
{
	FString ExampleScenesFolder()
	{
		return FPaths::ProjectContentDir() / TEXT("Examples");
	}

	bool IsHidden(const FString& Name)
	{
		return Weltall::IsHiddenFile(std::filesystem::path(TCHAR_TO_UTF8(*Name)));
	}

	/** The files under Root / Relative by their paths from Root, hidden ones and those in hidden folders left out. */
	void FilesUnder(const FString& Root, const FString& Relative, TArray<FString>& Found)
	{
		const FString Folder = Relative.IsEmpty() ? Root : Root / Relative;
		for (const bool bFolders : {false, true})
		{
			TArray<FString> Names;
			IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), !bFolders, bFolders);
			for (const FString& Name : Names)
			{
				if (IsHidden(Name))
				{
					continue;
				}
				const FString Path = Relative.IsEmpty() ? Name : Relative / Name;
				if (bFolders)
				{
					FilesUnder(Root, Path, Found);
				}
				else
				{
					Found.Add(Path);
				}
			}
		}
	}
}

#if PLATFORM_ANDROID
namespace
{
	/** The names of a folder's entries (files or folders) that are not hidden, in the order the core sorts paths in. */
	TArray<FString> VisibleNames(const FString& Folder, bool bFolders)
	{
		TArray<FString> Names;
		IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), !bFolders, bFolders);
		Names.RemoveAll([](const FString& Name) { return IsHidden(Name); });
		Names.Sort([](const FString& A, const FString& B) { return FCString::Strcmp(*A, *B) < 0; });
		return Names;
	}
}
#endif

Weltall::TResult<Weltall::FStoryPacks> ReadStoryDocuments()
{
	const FString Folder = FPaths::ProjectContentDir() / TEXT("Story");
#if PLATFORM_ANDROID
	Weltall::FStoryPacks Packs;
	for (const FString& Pack : VisibleNames(Folder, true))
	{
		for (const FString& Document : VisibleNames(Folder / Pack, false))
		{
			if (!Document.EndsWith(TEXT(".txt"), ESearchCase::CaseSensitive))
			{
				continue;
			}
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *(Folder / Pack / Document)))
			{
				return Weltall::MakeError(std::string(TCHAR_TO_UTF8(*Pack)) + "/" + TCHAR_TO_UTF8(*Document) + " cannot be read");
			}
			const std::string_view Text(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<std::size_t>(Bytes.Num()));
			if (std::optional<Weltall::FError> Error = Packs.Add(TCHAR_TO_UTF8(*Pack), TCHAR_TO_UTF8(*Document), Text))
			{
				return *Error;
			}
		}
	}
	return Packs;
#else
	return Weltall::FStoryPacks::Read(std::filesystem::path(*FPaths::ConvertRelativePathToFull(Folder)));
#endif
}

bool HasExampleScenes()
{
	return IFileManager::Get().FileExists(*(ExampleScenesFolder() / TEXT("mod.txt")));
}

Weltall::EExampleScenesCopy CopyExampleScenes(const FString& ModsFolder, bool bReplace)
{
	const FString Target = ModsFolder / FString(static_cast<int32>(Weltall::ExamplesFolder.size()), Weltall::ExamplesFolder.data());
	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.DirectoryExists(*Target))
	{
		if (!bReplace)
		{
			return Weltall::EExampleScenesCopy::AlreadyThere;
		}
		if (!FileManager.DeleteDirectory(*Target, false, true))
		{
			UE_LOG(LogWeltall, Warning, TEXT("The example scenes' copy in %s cannot be removed to replace it"), *Target);
			return Weltall::EExampleScenesCopy::Failed;
		}
	}
	TArray<FString> Copied;
	FilesUnder(ExampleScenesFolder(), FString(), Copied);
	for (const FString& File : Copied)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *(ExampleScenesFolder() / File)) || !FFileHelper::SaveArrayToFile(Bytes, *(Target / File)))
		{
			UE_LOG(LogWeltall, Warning, TEXT("The example scenes cannot be copied: %s to %s"), *File, *Target);
			return Weltall::EExampleScenesCopy::Failed;
		}
	}
	if (Copied.IsEmpty())
	{
		UE_LOG(LogWeltall, Warning, TEXT("The game has no example scenes to copy (%s)"), *ExampleScenesFolder());
		return Weltall::EExampleScenesCopy::Failed;
	}
	UE_LOG(LogWeltall, Log, TEXT("The example scenes copied into %s: %d files, played from the next start"), *Target, Copied.Num());
	return Weltall::EExampleScenesCopy::Copied;
}
