#pragma once

#include "Foundation/Types.h"

#include <filesystem>
#include <string_view>

namespace Weltall
{
	/**
	 * The folder Name inside Parent as the player made it. Windows and macOS find a folder whatever the case of its
	 * letters, Linux and Android only as it is written, so a player's "Textures" stands for the game's "textures"
	 * there too. Parent / Name where that folder exists as written or no folder of Parent matches it; of several that
	 * match, the first by name. Hidden folders (IsHiddenFile) never match.
	 */
	WELTALLCORE_API std::filesystem::path FolderIgnoringCase(const std::filesystem::path& Parent, std::string_view Name);
}
