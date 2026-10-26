#include "Mods/DeviceMods.h"
#include "Mods/PerfectWorks.h"

#include "Support/AppleDouble.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

using namespace Weltall;

// Mods and packs brought onto a device whose folders only the game reaches: what the file picker copied in, told by
// what it holds and put in place beside Games, listed and removed.
namespace
{
	namespace fs = std::filesystem;

	/** A device's folder for the game (Games, Mods and Assets beside each other), empty at the start and gone at the end. */
	struct FDevice
	{
		explicit FDevice(const char* Name) : Root(fs::temp_directory_path() / Name)
		{
			fs::remove_all(Root);
			fs::create_directories(Root / "Games");
		}
		~FDevice() { fs::remove_all(Root); }

		fs::path Incoming() const { return IncomingModFolder(Root); }

		/** A file of the folder being added, its folders made. */
		void Pick(const fs::path& File, std::string_view Bytes = "x") const { Write(Incoming() / File, Bytes); }

		static void Write(const fs::path& File, std::string_view Bytes = "x")
		{
			fs::create_directories(File.parent_path());
			std::ofstream(File, std::ios::binary) << Bytes;
		}

		static std::string Read(const fs::path& File)
		{
			std::ifstream In(File, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
		}

		/** The hidden files and folders at the top: what was set aside or left half added. */
		std::vector<std::string> Hidden() const
		{
			std::vector<std::string> Names;
			for (const fs::directory_entry& Entry : fs::directory_iterator(Root))
			{
				const std::string Name = Entry.path().filename().string();
				if (Name.front() == '.')
				{
					Names.push_back(Name);
				}
			}
			return Names;
		}

		fs::path Root;
	};

	/** Perfect Works Build as its release unpacks: a README naming its version beside the folders its patcher reads. */
	void PickPerfectWorks(const FDevice& Device, const fs::path& Folder, std::string_view Version)
	{
		Device.Pick(Folder / "README.md", "# Perfect_Works_Build\n\n### Version " + std::string(Version) + "\n");
		Device.Pick(Folder / "gamefiles" / "bug_fix1" / "0610");
		Device.Pick(Folder / "gamefiles" / "script1" / "0038");
		Device.Pick(Folder / "gamefiles" / "script1" / "3087");
	}

	std::vector<std::string> Names(const std::vector<FModItem>& Items)
	{
		std::vector<std::string> Listed;
		for (const FModItem& Item : Items)
		{
			Listed.push_back(Item.Name);
		}
		return Listed;
	}
}

TEST_CASE("Perfect Works Build is found in the folder added, put in Mods, and set beside the one in use until the next start", "[Mods]")
{
	FDevice Device("weltall-device-mods-perfect-works");
	PickPerfectWorks(Device, "Xenogears_Perfect_Works_Edition_0.11.4", "0.11.4");
	Device.Pick("Xenogears_Perfect_Works_Edition_0.11.4/patcher.exe");
	FModAddition Added = AddMod(Device.Incoming(), Device.Root, "Xenogears_Perfect_Works_Edition_0.11.4");
	REQUIRE(Added.Added);
	CHECK(Added.Failure == EModAddFailure::None);
	CHECK(Added.Added->Content == EAddedContent::PerfectWorks);
	CHECK(Added.Added->Name == "0.11.4");
	CHECK(Added.Added->Files == 5);
	CHECK_FALSE(Added.Added->bOnRestart);
	CHECK(fs::is_regular_file(Device.Root / "Mods" / "PerfectWorks" / "gamefiles" / "script1" / "3087"));
	CHECK(FPerfectWorksRelease::Find(Device.Root / "Mods" / "PerfectWorks").has_value());
	CHECK_FALSE(fs::exists(Device.Incoming()));
	DeleteRemovedMods(Device.Root);
	CHECK(Device.Hidden().empty());

	// Another release while the game has one: beside it, until the next start puts it in place.
	PickPerfectWorks(Device, "PerfectWorks", "0.11.5");
	Added = AddMod(Device.Incoming(), Device.Root, "PerfectWorks");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Name == "0.11.5");
	CHECK(Added.Added->bOnRestart);
	CHECK(FPerfectWorksRelease::VersionIn(Device.Root / "Mods" / "PerfectWorks") == "0.11.4");
	std::vector<FModItem> Items = ListMods(Device.Root);
	REQUIRE(Items.size() == 1);
	CHECK(Items[0].Kind == EModItemKind::PerfectWorks);
	CHECK(Items[0].Name == "Perfect Works 0.11.4");
	CHECK(Items[0].Path == fs::path("Mods") / "PerfectWorks");
	CHECK(Items[0].bReplacedOnRestart);
	CHECK_FALSE(Items[0].bRemovedOnRestart);
	FinishModChanges(Device.Root);
	DeleteRemovedMods(Device.Root);
	CHECK(FPerfectWorksRelease::VersionIn(Device.Root / "Mods" / "PerfectWorks") == "0.11.5");
	CHECK_FALSE(fs::exists(Device.Root / "Mods" / "PerfectWorks" / "patcher.exe"));
	CHECK_FALSE(fs::exists(Device.Root / "Mods" / ".PerfectWorks.next"));
	CHECK(Device.Hidden().empty());
	Items = ListMods(Device.Root);
	REQUIRE(Items.size() == 1);
	CHECK_FALSE(Items[0].bReplacedOnRestart);
}

TEST_CASE("Perfect Works' gamefiles folder picked alone is put in a folder of its own", "[Mods]")
{
	FDevice Device("weltall-device-mods-gamefiles");
	Device.Pick("bug_fix1/0610");
	Device.Pick("script1/0038");
	const FModAddition Added = AddMod(Device.Incoming(), Device.Root, "gamefiles");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::PerfectWorks);
	CHECK(Added.Added->Name.empty());
	CHECK(FPerfectWorksRelease::IsRelease(Device.Root / "Mods" / "PerfectWorks"));
	CHECK(Names(ListMods(Device.Root)) == std::vector<std::string>{"Perfect Works"});
}

TEST_CASE("A Beetle PSX HW texture pack is put in Assets/textures under the zip's name, macOS's companions left out", "[Mods]")
{
	FDevice Device("weltall-device-mods-beetle");
	const fs::path Pack = "[your .cue or .m3u file name here]-texture-replacements";
	Device.Pick(Pack / "49b52d64-1a585016.png", "picture");
	Device.Pick(Pack / "alternate portraits" / "1664bf35-109c6b6f.png");
	Device.Pick(Pack / "0 all portrait sprites.bmp");
	Testing::WriteAppleDoubleCompanion(Device.Incoming() / Pack / "49b52d64-1a585016.png");
	Device.Pick("__MACOSX" / Pack / "._49b52d64-1a585016.png");
	FModAddition Added = AddMod(Device.Incoming(), Device.Root, "Perfect ART Works 1.8.zip");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::Textures);
	CHECK(Added.Added->Name == "Perfect ART Works 1.8");
	CHECK(Added.Added->Files == 3);
	const fs::path Placed = Device.Root / "Assets" / "textures" / "Perfect ART Works 1.8";
	CHECK(FDevice::Read(Placed / "49b52d64-1a585016.png") == "picture");
	CHECK(fs::is_regular_file(Placed / "alternate portraits" / "1664bf35-109c6b6f.png"));

	// Added again, the pack is replaced whole: a picture the new one lacks is gone.
	Device.Pick(Pack / "49b52d64-1a585016.png", "picture 2");
	Added = AddMod(Device.Incoming(), Device.Root, "Perfect ART Works 1.8.zip");
	REQUIRE(Added.Added);
	CHECK(FDevice::Read(Placed / "49b52d64-1a585016.png") == "picture 2");
	CHECK_FALSE(fs::exists(Placed / "alternate portraits"));
	DeleteRemovedMods(Device.Root);
	CHECK(Device.Hidden().empty());
	const std::vector<FModItem> Items = ListMods(Device.Root);
	REQUIRE(Items.size() == 1);
	CHECK(Items[0].Kind == EModItemKind::Textures);
	CHECK(Items[0].Path == fs::path("Assets") / "textures" / "Perfect ART Works 1.8");
	CHECK(Items[0].Bytes == 9);
}

TEST_CASE("Pictures named by Weltall's keys, sheets and keyed songs are packs of their own kinds", "[Mods]")
{
	FDevice Device("weltall-device-mods-weltall-packs");
	Device.Pick("village-door-0d381f34099245c6.png");
	FModAddition Added = AddMod(Device.Incoming(), Device.Root, "Lahan");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::Textures);
	CHECK(fs::is_regular_file(Device.Root / "Assets" / "textures" / "Lahan" / "village-door-0d381f34099245c6.png"));

	Device.Pick("cells/cells-0.png");
	Device.Pick("cells/cells-0.tsv");
	Added = AddMod(Device.Incoming(), Device.Root, "Fei's sprites");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::Textures);
	CHECK(fs::is_regular_file(Device.Root / "Assets" / "textures" / "Fei's sprites" / "cells-0.tsv"));

	Device.Pick("Songs/song-07-3f2a9c41d0e8b6a1.wav");
	Added = AddMod(Device.Incoming(), Device.Root, "Orchestral: songs?.zip");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::Music);
	CHECK(Added.Added->Name == "Orchestral_ songs_");
	CHECK(fs::is_regular_file(Device.Root / "Assets" / "music" / "Orchestral_ songs_" / "song-07-3f2a9c41d0e8b6a1.wav"));
	CHECK(Names(ListMods(Device.Root)) == std::vector<std::string>{"Fei's sprites", "Lahan", "Orchestral_ songs_"});
}

TEST_CASE("A computer's Weltall folder zipped puts its Mods' folders whole and its Assets' files in their places", "[Mods]")
{
	FDevice Device("weltall-device-mods-folders");
	FDevice::Write(Device.Root / "Mods" / "Mine" / "old.bin");
	FDevice::Write(Device.Root / "Assets" / "text" / "lahan.txt", "old");
	FDevice::Write(Device.Root / "Assets" / "text" / "kept.txt", "kept");
	Device.Pick("Weltall/Games/Xenogears (USA) (Disc 1).cue");
	Device.Pick("Weltall/Mods/Mine/new.bin");
	PickPerfectWorks(Device, "Weltall/Mods/PerfectWorks", "0.11.4");
	Device.Pick("Weltall/Assets/text/lahan.txt", "new");
	Device.Pick("Weltall/Assets/textures/HD/46c8e27-bc2f6e29.png");
	Device.Pick("Weltall/Assets/font/font.tsv");
	const FModAddition Added = AddMod(Device.Incoming(), Device.Root, "Weltall.zip");
	REQUIRE(Added.Added);
	CHECK(Added.Added->Content == EAddedContent::Folders);
	CHECK(Added.Added->Name == "Weltall");
	CHECK(Added.Added->Files == 8);
	CHECK_FALSE(Added.Added->bOnRestart);
	CHECK(fs::is_regular_file(Device.Root / "Mods" / "Mine" / "new.bin"));
	CHECK_FALSE(fs::exists(Device.Root / "Mods" / "Mine" / "old.bin"));
	CHECK(FPerfectWorksRelease::IsRelease(Device.Root / "Mods" / "PerfectWorks"));
	CHECK(FDevice::Read(Device.Root / "Assets" / "text" / "lahan.txt") == "new");
	CHECK(FDevice::Read(Device.Root / "Assets" / "text" / "kept.txt") == "kept");
	CHECK_FALSE(fs::exists(Device.Root / "Games" / "Xenogears (USA) (Disc 1).cue"));
	CHECK(Names(ListMods(Device.Root)) == std::vector<std::string>{"Perfect Works 0.11.4", "Mods/Mine", "HD", "Assets/font", "Assets/text"});
	DeleteRemovedMods(Device.Root);
	CHECK(Device.Hidden().empty());
}

TEST_CASE("A folder holding nothing the game reads is refused and nothing of it kept", "[Mods]")
{
	FDevice Device("weltall-device-mods-refused");
	Device.Pick("Holiday/photo.jpg");
	Device.Pick("Holiday/notes.txt");
	const FModAddition Added = AddMod(Device.Incoming(), Device.Root, "Holiday");
	CHECK_FALSE(Added.Added);
	CHECK(Added.Failure == EModAddFailure::NotAMod);
	CHECK(ListMods(Device.Root).empty());
	CHECK_FALSE(fs::exists(Device.Incoming()));
	DeleteRemovedMods(Device.Root);
	CHECK(Device.Hidden().empty());
}

TEST_CASE("A pack is removed at once, Perfect Works when the game next starts, and anything not listed never", "[Mods]")
{
	FDevice Device("weltall-device-mods-remove");
	PickPerfectWorks(Device, "PerfectWorks", "0.11.4");
	REQUIRE(AddMod(Device.Incoming(), Device.Root, "PerfectWorks").Added);
	Device.Pick("pack/49b52d64-1a585016.png");
	REQUIRE(AddMod(Device.Incoming(), Device.Root, "HD Portraits").Added);
	const fs::path Pack = fs::path("Assets") / "textures" / "HD Portraits";

	CHECK(RemoveMod(Device.Root, "Games") == EModRemoval::Failed);
	CHECK(RemoveMod(Device.Root, fs::path("Assets") / "textures") == EModRemoval::Failed);
	CHECK(RemoveMod(Device.Root, Pack) == EModRemoval::Removed);
	CHECK_FALSE(fs::exists(Device.Root / Pack));
	CHECK(Names(ListMods(Device.Root)) == std::vector<std::string>{"Perfect Works 0.11.4"});

	// A release added to replace the one in use is dropped with it.
	PickPerfectWorks(Device, "PerfectWorks", "0.11.5");
	const FModAddition Replacing = AddMod(Device.Incoming(), Device.Root, "PerfectWorks");
	REQUIRE(Replacing.Added);
	CHECK(Replacing.Added->bOnRestart);
	CHECK(RemoveMod(Device.Root, fs::path("Mods") / "PerfectWorks") == EModRemoval::OnRestart);
	CHECK(FPerfectWorksRelease::IsRelease(Device.Root / "Mods" / "PerfectWorks"));
	std::vector<FModItem> Items = ListMods(Device.Root);
	REQUIRE(Items.size() == 1);
	CHECK(Items[0].bRemovedOnRestart);
	CHECK_FALSE(Items[0].bReplacedOnRestart);

	FinishModChanges(Device.Root);
	DeleteRemovedMods(Device.Root);
	CHECK(ListMods(Device.Root).empty());
	CHECK(Device.Hidden().empty());
	CHECK(fs::is_directory(Device.Root / "Games"));
}

TEST_CASE("The next start clears a folder an earlier session left half added", "[Mods]")
{
	FDevice Device("weltall-device-mods-leftover");
	Device.Pick("pack/49b52d64-1a585016.png");
	FinishModChanges(Device.Root);
	CHECK_FALSE(fs::exists(Device.Incoming()));
	DeleteRemovedMods(Device.Root);
	CHECK(Device.Hidden().empty());
	CHECK(ListMods(Device.Root).empty());
}
