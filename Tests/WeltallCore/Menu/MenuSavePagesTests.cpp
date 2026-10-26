#include "Menu/MenuSavePages.h"
#include "Game/GameState.h"
#include "Game/SaveStore.h"
#include "Game/SaveWrite.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuFileBrowser.h"
#include "Menu/MenuResources.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Support/OptionsRows.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>

using namespace Weltall;

// The options screen's saves pages: browsing for a card image, its saves copied into a card, a card written out.
namespace
{
	/** A folder of the test's own under the system's temporary folder, empty at the start and gone at the end. */
	struct FScratchFolder
	{
		std::filesystem::path Path;

		explicit FScratchFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
			std::filesystem::create_directories(Path);
		}

		~FScratchFolder()
		{
			std::error_code Error;
			std::filesystem::remove_all(Path, Error);
		}
	};

	void Touch(const std::filesystem::path& File, std::string_view Bytes = "x")
	{
		std::ofstream(File, std::ios::binary) << Bytes;
	}

	std::vector<std::string> Names(const FFileBrowser& Browser)
	{
		std::vector<std::string> Listed;
		for (const FBrowserEntry& Entry : Browser.Entries())
		{
			Listed.push_back(Entry.Name);
		}
		return Listed;
	}

	namespace Text = OptionsText::Saves;

	int32 RowNamed(const std::vector<FSettingsRow>& Rows, std::string_view Label)
	{
		for (std::size_t Row = 0; Row < Rows.size(); ++Row)
		{
			if (Rows[Row].Label == Label)
			{
				return static_cast<int32>(Row);
			}
		}
		return -1;
	}

	/** The device's file picker as the test plays it: each transfer started is recorded, its status set by the test. */
	class FFakeDeviceFiles final : public IMenuDeviceFiles
	{
	public:
		FTransferStatus Shown;
		std::vector<std::pair<EDeviceTransfer, std::filesystem::path>> Started;

		FTransferStatus Status() const override { return Shown; }

		bool Start(EDeviceTransfer Kind, const std::filesystem::path& File = {}) override
		{
			if (Shown.Running())
			{
				return false;
			}
			Started.emplace_back(Kind, File);
			Shown = {};
			Shown.Kind = Kind;
			Shown.State = ETransferState::Picking;
			Shown.Revision = Started.size() * 100;
			return true;
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
	};

	namespace DeviceRow
	{
		enum : int32 { Import, Export, Heading, AddDiscs, AddDiscFolder, BackUp, Restore, SaveLog };
	}
}

TEST_CASE("The file browser lists a folder's folders, then the card images and saves in it, by name", "[Menu]")
{
	FScratchFolder Scratch("WeltallFileBrowser");
	std::filesystem::create_directories(Scratch.Path / "b");
	std::filesystem::create_directories(Scratch.Path / "A");
	std::filesystem::create_directories(Scratch.Path / ".hidden");
	Touch(Scratch.Path / "zz.MCR");
	Touch(Scratch.Path / "notes.txt");
	Touch(Scratch.Path / "Slot.mcs");
	Touch(Scratch.Path / "b" / "epsxe.mcd");

	FFileBrowser Browser(Scratch.Path / "no such folder" / "deeper");
	CHECK(Browser.Folder() == Scratch.Path);
	CHECK(Browser.FolderName() == "WeltallFileBrowser");
	CHECK(Names(Browser) == std::vector<std::string>{"..", "A", "b", "Slot.mcs", "zz.MCR"});

	// Into b, a file picked there, and back up.
	CHECK_FALSE(Browser.Open(2));
	CHECK(Browser.Folder() == Scratch.Path / "b");
	CHECK(Names(Browser) == std::vector<std::string>{"..", "epsxe.mcd"});
	const std::optional<std::filesystem::path> Picked = Browser.Open(1);
	REQUIRE(Picked);
	CHECK(*Picked == Scratch.Path / "b" / "epsxe.mcd");
	CHECK_FALSE(Browser.Open(0));
	CHECK(Browser.Folder() == Scratch.Path);
	CHECK(Browser.Up());
	CHECK(Browser.Folder() == Scratch.Path.parent_path());

	CHECK(FFileBrowser::DisplayName(std::filesystem::path(u8"Caérd{1}")) == "Ca?rd?1?");
	CHECK(FFileBrowser::DisplayName("Xenogears_Perfect_Works_Edition_0.11.4") == "Xenogears Perfect Works Edition 0.11.4");
	CHECK(FFileBrowser::IsOffered("card.Gme"));
	CHECK_FALSE(FFileBrowser::IsOffered("card.zip"));
}

TEST_CASE("A card written out is read back by browsing to it, its saves copied into a card at their numbers", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	auto NewGame = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(NewGame);

	FScratchFolder Scratch("WeltallSavePages");
	FSaveStore Store(Scratch.Path / "Saves");
	const FOriginalSave Third = WriteOriginalSave(*NewGame, 2, 0, Resident.NameTables, Resident.Font.LeadByte());
	const FOriginalSave Last = WriteOriginalSave(*NewGame, 14, 0, Resident.NameTables, Resident.Font.LeadByte());
	constexpr int32 SecondCard = FSaveStore::SlotsPerStore;
	REQUIRE_FALSE(Store.Write(SecondCard + 2, Third));
	REQUIRE_FALSE(Store.Write(SecondCard + 14, Last));
	std::filesystem::create_directories(Scratch.Path / "Saves" / "Cards");
	Touch(Scratch.Path / "Saves" / "Cards" / "broken.mcr", "not a card");

	FMenuSavePages Pages(Store, *Resources);
	CHECK(Pages.Title() == OptionsText::Titles::Saves);
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	// Export: Right to Card 2, Confirm.
	REQUIRE(Pages.Step(1, 1));
	CHECK(Pages.Rows()[1].Value == "Card 2");
	REQUIRE(Pages.Activate(1));
	CHECK(Pages.Rows()[1].Help == OptionsText::Fill(Text::Exported, {"Card 2"}));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	CHECK(std::filesystem::file_size(Scratch.Path / "Saves" / "Cards" / "Card 2.mcr") == 0x20000);
	Pages.CursorMoved();
	CHECK(Pages.Rows()[1].Help == OptionsText::Fill(Text::Export.Help));

	// Import: browsing opens where the last import was picked from.
	Store.KeepImportFolder(Scratch.Path / "Saves" / "Cards");
	REQUIRE(Pages.Activate(0));
	REQUIRE(Pages.Page() == ESavesPage::Browse);
	CHECK(Pages.Title() == "Cards");
	std::vector<FSettingsRow> Rows = Pages.Rows();
	CHECK(Testing::OptionsRowsFit(Rows, Resident.Font));
	const int32 Broken = RowNamed(Rows, "broken.mcr");
	REQUIRE(Broken >= 0);
	REQUIRE(Pages.Activate(Broken));
	CHECK(Pages.Page() == ESavesPage::Browse);
	CHECK(Pages.Rows()[static_cast<std::size_t>(Broken)].Help == OptionsText::Fill(Text::NotACard));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	REQUIRE(Pages.Activate(RowNamed(Rows, "Card 2.mcr")));
	REQUIRE(Pages.Page() == ESavesPage::Import);

	// Into the first card with room for both, at their own numbers.
	Rows = Pages.Rows();
	CHECK(Rows[0].Value == "Card 1");
	CHECK(Rows[1].Label == OptionsText::Fill(Text::Copy.Label, {OptionsText::Fill(Text::ManySaves, {"2"})}));
	CHECK(Rows[2].Label == "No.03");
	CHECK(Rows[3].Label == "No.15");
	CHECK(Testing::OptionsRowsFit(Rows, Resident.Font));
	REQUIRE(Pages.Activate(1));
	CHECK(Pages.Rows()[1].Help == OptionsText::Fill(Text::Copied, {OptionsText::Fill(Text::ManySaves, {"2"}), "Card 1"}));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	for (const auto& [Slot, Save] : {std::pair{2, &Third}, std::pair{14, &Last}})
	{
		const auto Copied = Store.ReadBytes(Slot);
		REQUIRE(Copied);
		REQUIRE(*Copied);
		CHECK(std::equal((*Copied)->begin(), (*Copied)->end(), Save->Data().begin(), Save->Data().end()));
	}

	// Into Card 2, where both numbers hold saves: a second Confirm replaces them.
	REQUIRE(Pages.Step(0, 1));
	CHECK(Pages.Rows()[1].Value == OptionsText::Fill(Text::Overwrites, {"2"}));
	CHECK(Pages.Rows()[1].Help[0] == OptionsText::Fill(Text::OverwriteHelp[0], {"Card 2"}));
	CHECK(Pages.Rows()[1].Help[1] == "No.03 No.15");
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	REQUIRE_FALSE(Store.Remove(SecondCard + 2));
	REQUIRE(Pages.Activate(1));
	CHECK(Pages.Rows()[1].Value == OptionsText::Fill(Text::Overwrites, {"1"}));
	CHECK(Pages.Rows()[1].Help[0] == Text::OverwriteAsked[0]);
	CHECK(Pages.Rows()[1].Help[1] == "No.15");
	CHECK(Pages.Rows()[1].Help[2] == OptionsText::Fill(Text::OverwriteAsked[2], {"Card 2"}));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	const auto Untouched = Store.ReadBytes(SecondCard + 2);
	REQUIRE(Untouched);
	CHECK_FALSE(*Untouched);
	REQUIRE(Pages.Activate(1));
	const auto Replaced = Store.ReadBytes(SecondCard + 2);
	REQUIRE(Replaced);
	CHECK(*Replaced);

	// Back to the folder, then to the saves page; no further.
	REQUIRE(Pages.Back());
	CHECK(Pages.Page() == ESavesPage::Browse);
	REQUIRE(Pages.Back());
	CHECK(Pages.Page() == ESavesPage::Saves);
	CHECK_FALSE(Pages.Back());
}

TEST_CASE("Where files go through the device's picker, the Saves page adds discs, backs up, restores and saves the log, each told on its row", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	auto NewGame = LoadNewGameState(Disc->Image(), *Index, Resident.NameTables);
	REQUIRE(NewGame);

	FScratchFolder Scratch("WeltallDeviceSaves");
	FSaveStore Store(Scratch.Path / "Saves");
	REQUIRE_FALSE(Store.Write(4, WriteOriginalSave(*NewGame, 4, 0, Resident.NameTables, Resident.Font.LeadByte())));

	// The computer's own folders: the two rows alone.
	CHECK(FMenuSavePages(Store, *Resources).Rows().size() == 2);

	FFakeDeviceFiles Files;
	FMenuSavePages Pages(Store, *Resources, &Files);
	std::vector<FSettingsRow> Rows = Pages.Rows();
	REQUIRE(Rows.size() == 8);
	CHECK(Rows[DeviceRow::Import].Help == OptionsText::Fill(Text::ImportPickedHelp));
	CHECK(Rows[DeviceRow::Export].Help == OptionsText::Fill(Text::ExportPickedHelp));
	CHECK(Rows[DeviceRow::Heading].bHeading);
	CHECK(Rows[DeviceRow::Heading].Label == Text::DeviceHeading);
	CHECK(Rows[DeviceRow::AddDiscs].Label == Text::AddDiscs.Label);
	CHECK(Rows[DeviceRow::AddDiscFolder].Label == Text::AddDiscFolder.Label);
	CHECK(Rows[DeviceRow::SaveLog].Label == Text::SaveLog.Label);
	CHECK(Testing::OptionsRowsFit(Rows, Resident.Font));
	CHECK_FALSE(Pages.Refresh());

	// Disc images: picked, copied a file at a time, then told until another transfer starts.
	REQUIRE(Pages.Activate(DeviceRow::AddDiscs));
	REQUIRE(Files.Started.back().first == EDeviceTransfer::Discs);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscs].Help == OptionsText::Fill(Text::Picking));
	Files.Set(ETransferState::Copying, "Xenogears (USA) (Disc 1).bin", 1, 2, 45, 100);
	CHECK(Pages.Refresh());
	CHECK_FALSE(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscs].Help == OptionsText::Fill(Text::Copying, {"1", "2", "Xenogears (USA) (Disc 1).bin", "45"}));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	// One transfer at a time.
	CHECK_FALSE(Pages.Activate(DeviceRow::SaveLog));
	Files.Set(ETransferState::Done, "Xenogears (USA) (Disc 1).cue", 2, 2, 100, 100);
	CHECK(Pages.Refresh());
	Pages.CursorMoved();
	CHECK(Pages.Rows()[DeviceRow::AddDiscs].Help == OptionsText::Fill(Text::DiscsCopied, {"2 files"}));
	CHECK(Pages.Rows()[DeviceRow::BackUp].Help == OptionsText::Fill(Text::BackUp.Help));

	// A folder of disc images: its disc files counted, then copied; a folder with none is told so.
	REQUIRE(Pages.Activate(DeviceRow::AddDiscFolder));
	REQUIRE(Files.Started.back().first == EDeviceTransfer::DiscFolder);
	Files.Set(ETransferState::Copying, "Xenogears", 3, 0);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscFolder].Help == OptionsText::Fill(Text::ScanningDiscs, {"Xenogears", "3"}));
	CHECK(Pages.Rows()[DeviceRow::AddDiscs].Help == OptionsText::Fill(Text::AddDiscs.Help));
	Files.Set(ETransferState::Copying, "Xenogears (USA) (Disc 2).bin", 4, 4, 50, 100);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscFolder].Help == OptionsText::Fill(Text::Copying, {"4", "4", "Xenogears (USA) (Disc 2).bin", "50"}));
	Files.Set(ETransferState::Done, "Xenogears", 4, 4, 100, 100);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscFolder].Help == OptionsText::Fill(Text::DiscsCopied, {"4 files"}));
	REQUIRE(Pages.Activate(DeviceRow::AddDiscFolder));
	Files.Shown.Failure = ETransferFailure::NoDiscImages;
	Files.Set(ETransferState::Failed, "Photos");
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::AddDiscFolder].Help == OptionsText::Fill(Text::Failures[static_cast<std::size_t>(ETransferFailure::NoDiscImages) - 1]));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));

	// A backup.
	REQUIRE(Pages.Activate(DeviceRow::BackUp));
	CHECK(Pages.Rows()[DeviceRow::AddDiscs].Help == OptionsText::Fill(Text::AddDiscs.Help));
	Files.Set(ETransferState::Done, "Weltall saves 2026-10-26.zip", 23, 23, 2000, 2000);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::BackUp].Help == OptionsText::Fill(Text::BackedUp, {"23 files"}));

	// A restore asks a second Confirm first, which moving away forgets; a file that holds no saves is told so.
	REQUIRE(Pages.Activate(DeviceRow::Restore));
	CHECK(Files.Started.size() == 4);
	CHECK(Pages.Rows()[DeviceRow::Restore].Help == OptionsText::Fill(Text::RestoreAsked));
	CHECK(Testing::OptionsRowsFit(Pages.Rows(), Resident.Font));
	Pages.CursorMoved();
	CHECK(Pages.Rows()[DeviceRow::Restore].Help == OptionsText::Fill(Text::Restore.Help));
	REQUIRE(Pages.Activate(DeviceRow::Restore));
	REQUIRE(Pages.Activate(DeviceRow::Restore));
	REQUIRE(Files.Started.back().first == EDeviceTransfer::Restore);
	Files.Shown.Failure = ETransferFailure::NotABackup;
	Files.Set(ETransferState::Failed, "photo.jpg");
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::Restore].Help == OptionsText::Fill(Text::Failures[static_cast<std::size_t>(ETransferFailure::NotABackup) - 1]));

	// The log; a picker closed with nothing picked tells nothing.
	REQUIRE(Pages.Activate(DeviceRow::SaveLog));
	Files.Set(ETransferState::Cancelled);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::SaveLog].Help == OptionsText::Fill(Text::SaveLog.Help));
	REQUIRE(Pages.Activate(DeviceRow::SaveLog));
	Files.Set(ETransferState::Done, "Weltall.log", 1, 1, 10, 10);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::SaveLog].Help == OptionsText::Fill(Text::LogSaved));

	// The Modding page's transfers are not told here, running or done.
	REQUIRE(Files.Start(EDeviceTransfer::ModZip));
	Files.Set(ETransferState::Copying, "Perfect ART Works 1.8.zip", 1, 0, 5, 10);
	CHECK(Pages.Refresh());
	std::vector<FSettingsRow> Unchanged = Pages.Rows();
	CHECK(Unchanged[DeviceRow::Import].Help == OptionsText::Fill(Text::ImportPickedHelp));
	CHECK(Unchanged[DeviceRow::SaveLog].Help == OptionsText::Fill(Text::SaveLog.Help));
	Files.Set(ETransferState::Done, "Perfect ART Works 1.8.zip", 1, 0, 10, 10);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::Import].Help == OptionsText::Fill(Text::ImportPickedHelp));

	// A card exported is written into the Cards folder, then copied out where the player picks.
	REQUIRE(Pages.Activate(DeviceRow::Export));
	REQUIRE(Files.Started.back().first == EDeviceTransfer::CardImage);
	const std::filesystem::path Card = Files.Started.back().second;
	CHECK(Card == Scratch.Path / "Saves" / "Cards" / "Card 1.mcr");
	CHECK(std::filesystem::file_size(Card) == 0x20000);
	Files.Set(ETransferState::Done, "Card 1.mcr", 1, 1, 0x20000, 0x20000);
	CHECK(Pages.Refresh());
	CHECK(Pages.Rows()[DeviceRow::Export].Help == OptionsText::Fill(Text::CardSaved, {"Card 1.mcr"}));

	// A file to import: one that is no card is told on the row; a card image opens its saves, Cancel going back to the page.
	const std::filesystem::path Picked = Scratch.Path / "Import" / "notes.txt";
	std::filesystem::create_directories(Picked.parent_path());
	Touch(Picked, "not a card");
	REQUIRE(Pages.Activate(DeviceRow::Import));
	REQUIRE(Files.Started.back().first == EDeviceTransfer::ImportFile);
	Files.Shown.Picked = Picked;
	Files.Set(ETransferState::Done, "notes.txt", 1, 1, 10, 10);
	CHECK(Pages.Refresh());
	CHECK(Pages.Page() == ESavesPage::Saves);
	CHECK(Pages.Rows()[DeviceRow::Import].Help == OptionsText::Fill(Text::NotACard));
	REQUIRE(Pages.Activate(DeviceRow::Import));
	Files.Shown.Picked = Card;
	const uint32 Views = Pages.View();
	Files.Set(ETransferState::Done, "Card 1.mcr", 1, 1, 0x20000, 0x20000);
	CHECK(Pages.Refresh());
	CHECK(Pages.View() != Views);
	REQUIRE(Pages.Page() == ESavesPage::Import);
	CHECK(Pages.Rows()[2].Label == "No.05");
	REQUIRE(Pages.Back());
	CHECK(Pages.Page() == ESavesPage::Saves);
	CHECK_FALSE(Pages.Back());
}

TEST_CASE("Everything the saves pages tell is in the game's font and fits the help", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	std::vector<FSettingsRow> Told;
	for (const std::array<std::string_view, 3>& Lines : {Text::Unreadable, Text::NotACard, Text::NoSaves, Text::CopyFailed, Text::DamagedCard, Text::WriteFailed})
	{
		Told.push_back({"", "", OptionsText::Fill(Lines)});
	}
	Told.push_back({"", "", OptionsText::Fill(Text::Copied, {OptionsText::Fill(Text::ManySaves, {"15"}), "Card 4"})});
	Told.push_back({"", "", OptionsText::Fill(Text::Exported, {"Card 4"})});
	Told.push_back({"", "", OptionsText::Fill(Text::SaveHelp, {"No.15", "99:59:59"})});
	Told.push_back({OptionsText::Fill(Text::Copy.Label, {OptionsText::Fill(Text::ManySaves, {"15"})}), OptionsText::Fill(Text::Overwrites, {"15"}),
		OptionsText::Fill(Text::OverwriteHelp, {"Card 4"})});
	Told.push_back({std::string(Text::NoFolders), std::string(Text::Drive), OptionsText::Fill(Text::OverwriteAsked, {"Card 4"})});

	// The device's transfers, a name as long as the page shows one.
	const std::string Name = "Xenogears (USA undub) (Disc..";
	for (const std::array<std::string_view, 3>& Lines : Text::Failures)
	{
		Told.push_back({"", "", OptionsText::Fill(Lines, {Name})});
	}
	for (const std::array<std::string_view, 3>& Lines : {Text::ImportPickedHelp, Text::ExportPickedHelp, Text::RestoreAsked, Text::Picking, Text::LogSaved})
	{
		Told.push_back({"", "", OptionsText::Fill(Lines)});
	}
	Told.push_back({"", "", OptionsText::Fill(Text::Copying, {"12", "12", Name, "100"})});
	Told.push_back({"", "", OptionsText::Fill(Text::ScanningDiscs, {Name, "1234"})});
	Told.push_back({std::string(Text::AddDiscFolder.Label), "", OptionsText::Fill(Text::AddDiscFolder.Help)});
	Told.push_back({"", "", OptionsText::Fill(Text::DiscsCopied, {OptionsText::Fill(Text::ManyFiles, {"12"})})});
	Told.push_back({"", "", OptionsText::Fill(Text::BackedUp, {OptionsText::Fill(Text::ManyFiles, {"128"})})});
	Told.push_back({"", "", OptionsText::Fill(Text::Restored, {OptionsText::Fill(Text::ManyFiles, {"128"})})});
	Told.push_back({"", "", OptionsText::Fill(Text::CardSaved, {"Card 8.mcr"})});
	CHECK(Testing::OptionsRowsFit(Told, Resident.Font));
}
