#include "Support/GameDiscs.h"

#include "Media/GameLibrary.h"

#include <cstdlib>
#include <optional>

namespace Weltall::Testing
{
	const FIso9660* FindDisc(EDiscRelease Release)
	{
		static const FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
		return Library.Find(Release);
	}

	std::optional<std::filesystem::path> PatchedGamesFolder()
	{
#ifdef _WIN32
		wchar_t* Value = nullptr;
		std::size_t Length = 0;
		if (_wdupenv_s(&Value, &Length, L"WELTALL_PATCHED_GAMES") != 0 || Value == nullptr)
		{
			return std::nullopt;
		}
		const std::filesystem::path Folder(Value);
		std::free(Value);
#else
		const char* Value = std::getenv("WELTALL_PATCHED_GAMES");
		if (Value == nullptr)
		{
			return std::nullopt;
		}
		const std::filesystem::path Folder(Value);
#endif
		return Folder.empty() ? std::nullopt : std::optional<std::filesystem::path>(Folder);
	}

	const FIso9660* FindPatchedDisc(EDiscRelease Release)
	{
		static const std::optional<FGameLibrary> Library = []() -> std::optional<FGameLibrary> {
			const std::optional<std::filesystem::path> Folder = PatchedGamesFolder();
			return Folder ? std::optional<FGameLibrary>(FGameLibrary::Scan(*Folder)) : std::nullopt;
		}();
		return Library ? Library->Find(Release) : nullptr;
	}
}
