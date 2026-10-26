#include "Menu/MenuModFiles.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/OptionsRows.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <vector>

using namespace Weltall;

// The Modding page's rows where the device's files go through its picker: a mod or pack added from a zip or a folder,
// each one on the device listed with its size and removed.
namespace
{
	namespace Text = OptionsText::Modding;

	/** The device as the test plays it: each transfer started and mod removed recorded, its status and mods set by the test. */
	class FFakeDevice final : public IMenuDeviceFiles
	{
	public:
		FTransferStatus Shown;
		std::vector<FModItem> Listed;
		std::vector<EDeviceTransfer> Started;
		std::vector<std::filesystem::path> Removed;
		EModRemoval Removal = EModRemoval::Removed;

		FTransferStatus Status() const override { return Shown; }
		std::vector<FModItem> Mods() const override { return Listed; }

		bool Start(EDeviceTransfer Kind, const std::filesystem::path& = {}) override
		{
			if (Shown.Running())
			{
				return false;
			}
			Started.push_back(Kind);
			const uint64 Revision = Shown.Revision;
			Shown = {};
			Shown.Kind = Kind;
			Shown.State = ETransferState::Picking;
			Shown.Revision = Revision + 1;
			return true;
		}

		EModRemoval RemoveMod(const std::filesystem::path& Path) override
		{
			Removed.push_back(Path);
			if (Removal == EModRemoval::Removed)
			{
				std::erase_if(Listed, [&Path](const FModItem& Item) { return Item.Path == Path; });
			}
			return Removal;
		}

		void Set(ETransferState State, std::string Name = {}, int32 File = 0, int32 Files = 0, uint64 Copied = 0, uint64 Total = 0)
		{
			Shown.State = State;
			Shown.Name = std::move(Name);
			Shown.File = File;
			Shown.Files = Files;
			Shown.Copied = Copied;
			Shown.Total = Total;
			++Shown.Revision;
		}

		void Fail(ETransferFailure Failure)
		{
			Shown.Failure = Failure;
			Set(ETransferState::Failed, "Holiday.zip");
		}

		void Add(EAddedContent Content, std::string Name, int32 Files = 1, bool bOnRestart = false)
		{
			Shown.Added = FAddedMod{Content, std::move(Name), Files, bOnRestart};
			Set(ETransferState::Done, "picked");
		}
	};

	FModItem Item(EModItemKind Kind, std::string Path, std::string Name, uint64 Bytes)
	{
		FModItem Listed;
		Listed.Kind = Kind;
		Listed.Path = Path;
		Listed.Name = std::move(Name);
		Listed.Bytes = Bytes;
		return Listed;
	}

	namespace ModRow
	{
		enum : int32 { Heading, AddZip, AddFolder, FirstItem };
	}

	std::array<std::string, 3> Lines(std::string_view First, std::string_view Second = {}, std::string_view Third = {})
	{
		return {std::string(First), std::string(Second), std::string(Third)};
	}
}

TEST_CASE("The Modding page's device rows add a mod from a zip or a folder, its progress and what it added told on its row", "[Menu]")
{
	FFakeDevice Device;
	FMenuModFiles Rows(Device);
	std::vector<FSettingsRow> Shown = Rows.Rows();
	REQUIRE(Shown.size() == 3);
	CHECK(Shown[ModRow::Heading].bHeading);
	CHECK(Shown[ModRow::Heading].Label == "This device");
	CHECK(Shown[ModRow::AddZip].Label == "Add zip file");
	CHECK(Shown[ModRow::AddFolder].Label == "Add folder");
	CHECK(Shown[ModRow::AddFolder].Help == Lines("Adds a mod or texture pack", "from a folder. Unpack .7z and", ".rar files first."));
	CHECK_FALSE(Rows.Activate(ModRow::Heading));

	// A zip: picked, unpacked with its files uncounted, a texture pack added while External assets is off.
	REQUIRE(Rows.Activate(ModRow::AddZip));
	CHECK(Device.Started == std::vector<EDeviceTransfer>{EDeviceTransfer::ModZip});
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("Waiting for the file picker."));
	Device.Set(ETransferState::Copying, "49b52d64-1a585016.png", 12, 0, 45, 100);
	CHECK(Rows.Refresh());
	CHECK_FALSE(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("Extracting:", "49b52d64-1a585016.png", "45% done."));
	CHECK_FALSE(Rows.Activate(ModRow::AddFolder));
	Device.Add(EAddedContent::Textures, "Perfect ART Works 1.8", 549);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("Added texture pack", "Perfect ART Works 1.8.", ""));
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == OptionsText::Fill(Text::AddFolder.Help));
	// What was added, taken once for the screen to switch it on.
	const std::optional<FAddedMod> Added = Rows.TakeAdded();
	REQUIRE(Added);
	CHECK(Added->Content == EAddedContent::Textures);
	CHECK_FALSE(Rows.TakeAdded());
	Device.Listed.push_back(Item(EModItemKind::Textures, "Assets/textures/Perfect ART Works 1.8", "Perfect ART Works 1.8", 1));
	++Device.Shown.Revision;
	CHECK(Rows.Refresh());
	CHECK_FALSE(Rows.TakeAdded());

	// A folder: its files counted, Perfect Works added, then one beside the one in use.
	REQUIRE(Rows.Activate(ModRow::AddFolder));
	Device.Set(ETransferState::Copying, "PerfectWorks", 0, 0);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("Scanning folder:", "PerfectWorks", "0 files found."));
	Device.Set(ETransferState::Copying, "PerfectWorks", 1250, 0);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("Scanning folder:", "PerfectWorks", "1250 files found."));
	Device.Set(ETransferState::Copying, "3087", 2, 4428, 10, 1000);
	CHECK(Rows.Refresh());
	std::vector<FSettingsRow> Copying = Rows.Rows();
	CHECK(Copying[ModRow::AddFolder].Help == Lines("Copying file 2 of 4428:", "3087", "1% done."));
	CHECK(Copying[ModRow::AddZip].Help == OptionsText::Fill(Text::AddZip.Help));
	Device.Add(EAddedContent::PerfectWorks, "0.11.4", 4428);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("Added Perfect Works 0.11.4.", "Restart Weltall to use it.", ""));
	CHECK(Rows.TakeAdded()->Content == EAddedContent::PerfectWorks);
	REQUIRE(Rows.Activate(ModRow::AddFolder));
	Device.Add(EAddedContent::PerfectWorks, "", 4428, true);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("Added Perfect Works.", "It replaces the current one", "when you restart Weltall."));
	REQUIRE(Rows.Activate(ModRow::AddZip));
	Device.Add(EAddedContent::Folders, "Weltall", 128);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("Added 128 files from", "Weltall.", "Restart Weltall to use them."));
	REQUIRE(Rows.Activate(ModRow::AddZip));
	Device.Add(EAddedContent::Music, "Orchestral", 73);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("Added music pack", "Orchestral.", ""));

	// Why nothing was added; a cancel tells nothing.
	REQUIRE(Rows.Activate(ModRow::AddZip));
	Device.Fail(ETransferFailure::NotAZip);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddZip].Help == Lines("That's not a zip file. Unpack", ".7z and .rar files first, then", "use Add folder."));
	REQUIRE(Rows.Activate(ModRow::AddFolder));
	Device.Fail(ETransferFailure::NotAMod);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("No mod or texture pack found", "that Weltall can use.", ""));
	REQUIRE(Rows.Activate(ModRow::AddFolder));
	Device.Fail(ETransferFailure::NoRoom);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == Lines("Not enough storage space.", "", ""));
	REQUIRE(Rows.Activate(ModRow::AddFolder));
	Device.Set(ETransferState::Cancelled);
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows()[ModRow::AddFolder].Help == OptionsText::Fill(Text::AddFolder.Help));
	CHECK_FALSE(Rows.TakeAdded());

	// Rows made after a transfer ended take nothing from it.
	Device.Add(EAddedContent::Textures, "Earlier");
	FMenuModFiles Later(Device);
	++Device.Shown.Revision;
	CHECK(Later.Refresh());
	CHECK_FALSE(Later.TakeAdded());

	// The Saves page's transfers are not told here.
	REQUIRE(Device.Start(EDeviceTransfer::Discs));
	Device.Set(ETransferState::Copying, "Xenogears (USA) (Disc 2).bin", 1, 2, 5, 10);
	CHECK(Rows.Refresh());
	Shown = Rows.Rows();
	CHECK(Shown[ModRow::AddZip].Help == OptionsText::Fill(Text::AddZip.Help));
	CHECK(Shown[ModRow::AddFolder].Help == OptionsText::Fill(Text::AddFolder.Help));
	CHECK_FALSE(Rows.Activate(ModRow::AddZip));
}

TEST_CASE("Each mod on the device is listed with its size and removed on a second Confirm, Perfect Works when the game restarts", "[Menu]")
{
	FFakeDevice Device;
	Device.Listed = {Item(EModItemKind::PerfectWorks, "Mods/PerfectWorks", "Perfect Works 0.11.4", 321'456'789),
		Item(EModItemKind::Textures, "Assets/textures/Perfect ART Works 1.8", "Perfect ART Works 1.8", 104'813'032),
		Item(EModItemKind::Music, "Assets/music/Orchestral", "Orchestral", 1'500), Item(EModItemKind::Assets, "Assets/text", "Assets/text", 2'345'678'901)};
	Device.Listed[0].bReplacedOnRestart = true;
	FMenuModFiles Rows(Device);
	std::vector<FSettingsRow> Shown = Rows.Rows();
	REQUIRE(Shown.size() == 7);
	const int32 PerfectWorks = ModRow::FirstItem, Pack = ModRow::FirstItem + 1, Music = ModRow::FirstItem + 2, OwnFiles = ModRow::FirstItem + 3;
	CHECK(Shown[PerfectWorks].Label == "Perfect Works 0.11.4");
	CHECK(Shown[PerfectWorks].Value == "321 MB");
	CHECK(Shown[PerfectWorks].Help == Lines("Perfect Works 0.11.4", "The new version replaces it", "when you restart Weltall."));
	CHECK(Shown[Pack].Label == "Perfect ART Works 1.8");
	CHECK(Shown[Pack].Value == "105 MB");
	CHECK(Shown[Pack].Help == Lines("Perfect ART Works 1.8", "Texture pack, 105 MB.", "Press Confirm to remove it."));
	CHECK(Shown[Music].Value == "2 KB");
	CHECK(Shown[Music].Help == Lines("Orchestral", "Music pack, 2 KB.", "Press Confirm to remove it."));
	CHECK(Shown[OwnFiles].Value == "2.3 GB");
	CHECK(Shown[OwnFiles].Help[1] == "Asset files, 2.3 GB.");

	// Asked first; the cursor leaving forgets it.
	REQUIRE(Rows.Activate(Pack));
	CHECK(Device.Removed.empty());
	CHECK(Rows.Rows()[Pack].Help == Lines("Perfect ART Works 1.8", "Press Confirm again to", "remove it from this device."));
	Rows.CursorMoved();
	CHECK(Rows.Rows()[Pack].Help[1] == "Texture pack, 105 MB.");
	REQUIRE(Rows.Activate(Pack));
	REQUIRE(Rows.Activate(Pack));
	CHECK(Device.Removed == std::vector<std::filesystem::path>{"Assets/textures/Perfect ART Works 1.8"});
	Shown = Rows.Rows();
	REQUIRE(Shown.size() == 6);
	CHECK(Shown[Pack].Label == "Orchestral");
	CHECK(Shown[Pack].Help == Lines("Removed Perfect ART Works 1.8.", "", ""));
	Rows.CursorMoved();

	// The last one: told on the one before it.
	const int32 Last = Pack + 1;
	REQUIRE(Rows.Activate(Last));
	REQUIRE(Rows.Activate(Last));
	Shown = Rows.Rows();
	REQUIRE(Shown.size() == 5);
	CHECK(Shown[Pack].Help == Lines("Removed Assets/text.", "", ""));
	Rows.CursorMoved();

	// Perfect Works: in the game's use, it goes when the game restarts, and Confirm does nothing more.
	Device.Removal = EModRemoval::OnRestart;
	REQUIRE(Rows.Activate(PerfectWorks));
	REQUIRE(Rows.Activate(PerfectWorks));
	Shown = Rows.Rows();
	CHECK(Shown[PerfectWorks].Value == "Pending");
	CHECK(Shown[PerfectWorks].Help == Lines("Perfect Works 0.11.4", "Will be removed when you", "restart Weltall."));
	CHECK_FALSE(Rows.Activate(PerfectWorks));

	// One that could not be removed says so; nothing is removed while a transfer runs.
	Device.Removal = EModRemoval::Failed;
	REQUIRE(Rows.Activate(Pack));
	REQUIRE(Rows.Activate(Pack));
	CHECK(Rows.Rows()[Pack].Help == Lines("Couldn't remove it.", "", ""));
	Rows.CursorMoved();
	REQUIRE(Rows.Activate(ModRow::AddZip));
	CHECK_FALSE(Rows.Activate(Pack));

	// The device's mods listed again: the rows follow.
	Device.Set(ETransferState::Cancelled);
	Device.Listed.push_back(Item(EModItemKind::Mod, "Mods/Mine", "Mods/Mine", 10));
	CHECK(Rows.Refresh());
	CHECK(Rows.Rows().back().Label == "Mods/Mine");
}

TEST_CASE("Everything the Modding page's device rows tell is in the game's font and fits", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	FFakeDevice Device;
	// Names as long as the rows show them, with the font's widest letters.
	const std::string Long = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
	for (const EModItemKind Kind : {EModItemKind::PerfectWorks, EModItemKind::Mod, EModItemKind::Textures, EModItemKind::Music, EModItemKind::Assets})
	{
		Device.Listed.push_back(Item(Kind, "Mods/" + std::to_string(Device.Listed.size()), Long, 888'888'888));
	}
	Device.Listed.push_back(Item(EModItemKind::Textures, "Assets/textures/x", "[your .cue or .m3u file name here]-texture-replacements", 9'999'999'999));
	Device.Listed[0].bRemovedOnRestart = true;
	FMenuModFiles Rows(Device);
	std::vector<FSettingsRow> Told = Rows.Rows();
	REQUIRE(Rows.Activate(ModRow::FirstItem + 1));
	Told.push_back(Rows.Rows()[ModRow::FirstItem + 1]);
	Device.Removal = EModRemoval::Removed;
	REQUIRE(Rows.Activate(ModRow::FirstItem + 1));
	Told.push_back(Rows.Rows()[ModRow::FirstItem + 1]);
	Device.Listed[1].bReplacedOnRestart = true;
	REQUIRE(Device.Start(EDeviceTransfer::ModFolder));
	Device.Set(ETransferState::Cancelled);
	CHECK(Rows.Refresh());
	Told.push_back(Rows.Rows()[ModRow::FirstItem + 1]);

	const auto Transfer = [&](EDeviceTransfer Kind, const auto& Finish)
	{
		Device.Shown.State = ETransferState::Idle;
		REQUIRE(Rows.Activate(Kind == EDeviceTransfer::ModZip ? ModRow::AddZip : ModRow::AddFolder));
		Finish();
		REQUIRE(Rows.Refresh());
		Told.push_back(Rows.Rows()[Kind == EDeviceTransfer::ModZip ? ModRow::AddZip : ModRow::AddFolder]);
	};
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Set(ETransferState::Copying, Long, 1, 0, 99, 100); });
	Transfer(EDeviceTransfer::ModFolder, [&] { Device.Set(ETransferState::Copying, Long, 4428, 4428, 99, 100); });
	Transfer(EDeviceTransfer::ModFolder, [&] { Device.Set(ETransferState::Copying, Long, 44280, 0); });
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Add(EAddedContent::Textures, Long); });
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Add(EAddedContent::Music, Long); });
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Add(EAddedContent::Folders, Long, 12345); });
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Add(EAddedContent::PerfectWorks, "0.11.10"); });
	Transfer(EDeviceTransfer::ModZip, [&] { Device.Add(EAddedContent::PerfectWorks, "0.11.10", 1, true); });
	for (const ETransferFailure Failure : {ETransferFailure::NoRoom, ETransferFailure::Unreadable, ETransferFailure::Unwritable, ETransferFailure::NoPicker,
			 ETransferFailure::NotAMod, ETransferFailure::NotAZip})
	{
		Transfer(EDeviceTransfer::ModFolder, [&] { Device.Shown.Failure = Failure; Device.Set(ETransferState::Failed, Long); });
	}
	Told.push_back({std::string(Text::External.Label), "Off", OptionsText::Fill(Text::External.Help)});
	Told.push_back({std::string(Text::PerfectWorks.Label), "Off", OptionsText::Fill(Text::PerfectWorksPicked, {"0.11.10"})});
	Told.push_back({"", "", OptionsText::Fill(OptionsText::PerfectWorks::EnabledPicked)});
	Told.push_back({"", "", OptionsText::Fill(OptionsText::Sound::NoSubtitlesPicked)});
	CHECK(Testing::OptionsRowsFit(Told, Resident.Font));
}
