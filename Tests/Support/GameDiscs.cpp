#include "Support/GameDiscs.h"

#include "Media/GameLibrary.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace Weltall::Testing
{
	const FGameLibrary& GamesLibrary()
	{
		static const FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
		return Library;
	}

	const FIso9660* FindDisc(EDiscRelease Release)
	{
		return GamesLibrary().Find(Release);
	}

	namespace
	{
		std::optional<std::filesystem::path> FolderNamedBy(const char* Variable)
		{
#ifdef _WIN32
			wchar_t* Value = nullptr;
			std::size_t Length = 0;
			const std::wstring Name(Variable, Variable + std::char_traits<char>::length(Variable));
			if (_wdupenv_s(&Value, &Length, Name.c_str()) != 0 || Value == nullptr)
			{
				return std::nullopt;
			}
			const std::filesystem::path Folder(Value);
			std::free(Value);
#else
			const char* Value = std::getenv(Variable);
			if (Value == nullptr)
			{
				return std::nullopt;
			}
			const std::filesystem::path Folder(Value);
#endif
			return Folder.empty() ? std::nullopt : std::optional<std::filesystem::path>(Folder);
		}

		std::optional<FGameLibrary> LibraryNamedBy(const char* Variable)
		{
			const std::optional<std::filesystem::path> Folder = FolderNamedBy(Variable);
			return Folder ? std::optional<FGameLibrary>(FGameLibrary::Scan(*Folder)) : std::nullopt;
		}
	}

	std::optional<std::filesystem::path> PatchedGamesFolder()
	{
		return FolderNamedBy("WELTALL_PATCHED_GAMES");
	}

	const FIso9660* FindPatchedDisc(EDiscRelease Release)
	{
		static const std::optional<FGameLibrary> Library = LibraryNamedBy("WELTALL_PATCHED_GAMES");
		return Library ? Library->Find(Release) : nullptr;
	}

	const FIso9660* FindItalianDisc(EDiscRelease Release)
	{
		static const std::optional<FGameLibrary> Library = LibraryNamedBy("WELTALL_ITALIAN_GAMES");
		return Library ? Library->Find(Release) : nullptr;
	}

	std::optional<std::filesystem::path> ItalianPatchMods()
	{
		return FolderNamedBy("WELTALL_ITALIAN_PATCH");
	}

	const FIso9660* FindItalianJapaneseCommandsDisc(EDiscRelease Release)
	{
		static const std::optional<FGameLibrary> Library = LibraryNamedBy("WELTALL_ITALIAN_JP_GAMES");
		return Library ? Library->Find(Release) : nullptr;
	}

	const FIso9660* FindBrazilianDisc(EDiscRelease Release)
	{
		static const std::optional<FGameLibrary> Library = LibraryNamedBy("WELTALL_BRAZILIAN_GAMES");
		return Library ? Library->Find(Release) : nullptr;
	}
}
