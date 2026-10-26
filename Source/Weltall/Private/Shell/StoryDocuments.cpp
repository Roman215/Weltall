#include "Shell/StoryDocuments.h"

#include "Misc/Paths.h"

#include <filesystem>

#if PLATFORM_ANDROID
#include "Foundation/HiddenFiles.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

#include <string>

namespace
{
	/** The names of a folder's entries (files or folders) that are not hidden, in the order the core sorts paths in. */
	TArray<FString> VisibleNames(const FString& Folder, bool bFolders)
	{
		TArray<FString> Names;
		IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), !bFolders, bFolders);
		Names.RemoveAll([](const FString& Name) { return Weltall::IsHiddenFile(std::filesystem::path(TCHAR_TO_UTF8(*Name))); });
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
