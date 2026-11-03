#include "Foundation/FolderNames.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>

using namespace Weltall;

namespace
{
	namespace fs = std::filesystem;

	/** A fresh, empty folder of the test's own. */
	fs::path FreshFolder(const char* Name)
	{
		const fs::path Folder = fs::temp_directory_path() / Name;
		fs::remove_all(Folder);
		fs::create_directories(Folder);
		return Folder;
	}
}

TEST_CASE("A folder the player named in other letters' case is the game's folder", "[Foundation][FolderNames]")
{
	const fs::path Root = FreshFolder("weltall-folder-names");
	fs::create_directories(Root / "Assets" / "Textures");

	// On Windows the folder is found as written; on Linux only through its other spelling. Either way it is the player's.
	const fs::path Textures = FolderIgnoringCase(Root / "Assets", "textures");
	CHECK(fs::is_directory(Textures));
	CHECK(fs::equivalent(Textures, Root / "Assets" / "Textures"));
	CHECK(fs::equivalent(FolderIgnoringCase(Root, "assets"), Root / "Assets"));
	fs::remove_all(Root);
}

TEST_CASE("A folder as written, or one that is not there, is named as the game writes it", "[Foundation][FolderNames]")
{
	const fs::path Root = FreshFolder("weltall-folder-names-written");
	fs::create_directories(Root / "Mods");
	fs::create_directories(Root / ".music");
	{
		std::ofstream File(Root / "TEXT");
	}

	CHECK(FolderIgnoringCase(Root, "Mods") == Root / "Mods");
	// Neither a hidden folder nor a file of the name stands for a folder that is missing.
	CHECK(FolderIgnoringCase(Root, "music") == Root / "music");
	CHECK(FolderIgnoringCase(Root, ".MUSIC") == Root / ".MUSIC");
	CHECK(FolderIgnoringCase(Root, "text") == Root / "text");
	CHECK(FolderIgnoringCase(Root / "missing", "font") == Root / "missing" / "font");
	fs::remove_all(Root);
}
