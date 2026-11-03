#include "Foundation/FolderNames.h"

#include "Foundation/HiddenFiles.h"

#include <algorithm>
#include <optional>
#include <string>

namespace Weltall
{
	namespace
	{
		char8_t Lower(char8_t Letter)
		{
			return Letter >= u8'A' && Letter <= u8'Z' ? static_cast<char8_t>(Letter - u8'A' + u8'a') : Letter;
		}

		bool SameIgnoringCase(std::u8string_view Found, std::string_view Name)
		{
			return std::equal(Found.begin(), Found.end(), Name.begin(), Name.end(),
				[](char8_t A, char B) { return Lower(A) == Lower(static_cast<char8_t>(B)); });
		}
	}

	std::filesystem::path FolderIgnoringCase(const std::filesystem::path& Parent, std::string_view Name)
	{
		namespace fs = std::filesystem;
		const fs::path Written = Parent / fs::path(std::u8string(Name.begin(), Name.end()));
		std::error_code Error;
		if (fs::is_directory(Written, Error))
		{
			return Written;
		}
		std::optional<fs::path> First;
		for (fs::directory_iterator Entry(Parent, Error), End; !Error && Entry != End; Entry.increment(Error))
		{
			std::error_code Kind;
			if (IsHiddenFile(Entry->path()) || !Entry->is_directory(Kind) || !SameIgnoringCase(Entry->path().filename().u8string(), Name))
			{
				continue;
			}
			if (!First || Entry->path().filename() < First->filename())
			{
				First = Entry->path();
			}
		}
		return First.value_or(Written);
	}
}
