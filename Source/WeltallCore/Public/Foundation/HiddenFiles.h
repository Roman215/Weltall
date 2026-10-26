#pragma once

#include <filesystem>
#include <string>

namespace Weltall
{
	/**
	 * Whether a file or folder is hidden as Linux and macOS hide them: its name begins with a dot. Such a file is the
	 * system's or another program's, never one the player gives the game. macOS writes one beside every file it copies
	 * to a drive formatted for Windows, "._" and that file's name, which would otherwise pass for the file itself.
	 */
	inline bool IsHiddenFile(const std::filesystem::path& Path)
	{
		const std::u8string Name = Path.filename().u8string();
		return !Name.empty() && Name.front() == u8'.';
	}
}
