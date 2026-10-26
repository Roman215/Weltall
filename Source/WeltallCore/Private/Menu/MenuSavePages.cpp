#include "Menu/MenuSavePages.h"

#include "Game/SaveStore.h"
#include "Menu/MenuSaveCards.h"
#include "Menu/OptionsText.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>

namespace Weltall
{
	namespace
	{
		namespace Text = OptionsText::Saves;

		namespace SavesRow
		{
			enum : int32 { Import, Export, DeviceHeading, AddDiscs, AddDiscFolder, BackUp, Restore, SaveLog };
		}

		/** The Saves page's row of a transfer; none for the Modding page's. */
		std::optional<int32> TransferRow(EDeviceTransfer Kind)
		{
			switch (Kind)
			{
			case EDeviceTransfer::Discs: return SavesRow::AddDiscs;
			case EDeviceTransfer::DiscFolder: return SavesRow::AddDiscFolder;
			case EDeviceTransfer::BackUp: return SavesRow::BackUp;
			case EDeviceTransfer::Restore: return SavesRow::Restore;
			case EDeviceTransfer::Log: return SavesRow::SaveLog;
			case EDeviceTransfer::ImportFile: return SavesRow::Import;
			case EDeviceTransfer::CardImage: return SavesRow::Export;
			case EDeviceTransfer::ModZip:
			case EDeviceTransfer::ModFolder:
				return std::nullopt;
			}
			return std::nullopt;
		}

		namespace ImportRow
		{
			enum : int32 { Into, Copy, FirstSave };
		}

		/** A name shown in a row before its value, and a help line: in the font's 8-pixel letters. */
		constexpr std::size_t RowLetters = 25;
		constexpr std::size_t LineLetters = 30;
		constexpr std::size_t TitleLetters = 22;
		constexpr uint32 VblanksPerSecond = 60;

		std::string Shortened(const std::string& Name, std::size_t Letters)
		{
			return Name.size() <= Letters ? Name : Name.substr(0, Letters - 2) + "..";
		}

		/** A long name broken into the help's three lines, its end cut short where it does not fit. */
		std::array<std::string, 3> Wrapped(const std::string& Name)
		{
			std::array<std::string, 3> Lines;
			for (std::size_t Line = 0; Line < Lines.size() && Line * LineLetters < Name.size(); ++Line)
			{
				Lines[Line] = Name.substr(Line * LineLetters, LineLetters);
			}
			if (Name.size() > Lines.size() * LineLetters)
			{
				Lines.back() = Shortened(Name.substr((Lines.size() - 1) * LineLetters), LineLetters);
			}
			return Lines;
		}

		std::string CardName(int32 Store)
		{
			return OptionsText::Fill(Text::Card, {std::to_string(Store + 1)});
		}

		/** A row as the screen shows it: the words' label and help, and its value. */
		FSettingsRow RowOfWords(const OptionsText::FRow& Words, std::string Value = {})
		{
			return {std::string(Words.Label), std::move(Value), OptionsText::Fill(Words.Help)};
		}

		std::string PlayTime(uint32 Vblanks)
		{
			const uint32 Seconds = Vblanks / VblanksPerSecond;
			char Text[40];
			std::snprintf(Text, sizeof Text, "%u:%02u:%02u", Seconds / 3600, Seconds / 60 % 60, Seconds % 60);
			return Text;
		}

		std::string SaveNumber(const FOriginalSave& Save)
		{
			char Text[16];
			std::snprintf(Text, sizeof Text, "No.%02d", Save.CardSlot() + 1);
			return Text;
		}

		std::string SaveCount(int32 Count)
		{
			return Count == 1 ? std::string(Text::OneSave) : OptionsText::Fill(Text::ManySaves, {std::to_string(Count)});
		}

		std::string FileCount(int32 Count)
		{
			return Count == 1 ? std::string(Text::OneFile) : OptionsText::Fill(Text::ManyFiles, {std::to_string(Count)});
		}

		/** What a transfer's row tells while it runs, and at its end; nothing once cancelled. */
		std::optional<std::array<std::string, 3>> TransferLines(const FTransferStatus& Status)
		{
			const std::string Name = Shortened(FFileBrowser::DisplayName(Status.Name), LineLetters);
			switch (Status.State)
			{
			case ETransferState::Picking:
				return OptionsText::Fill(Text::Picking);
			case ETransferState::Copying:
				if (Status.Kind == EDeviceTransfer::DiscFolder && Status.Files == 0)
				{
					// The folder's disc images are counted before any is copied.
					return OptionsText::Fill(Text::ScanningDiscs, {Name, std::to_string(Status.File)});
				}
				return OptionsText::Fill(Text::Copying, {std::to_string(std::max(Status.File, 1)), std::to_string(std::max(Status.Files, 1)), Name, std::to_string(Status.Percent())});
			case ETransferState::Done:
				switch (Status.Kind)
				{
				case EDeviceTransfer::Discs:
				case EDeviceTransfer::DiscFolder:
					return OptionsText::Fill(Text::DiscsCopied, {FileCount(Status.Files)});
				case EDeviceTransfer::BackUp: return OptionsText::Fill(Text::BackedUp, {FileCount(Status.Files)});
				case EDeviceTransfer::Restore: return OptionsText::Fill(Text::Restored, {FileCount(Status.Files)});
				case EDeviceTransfer::Log: return OptionsText::Fill(Text::LogSaved);
				case EDeviceTransfer::CardImage: return OptionsText::Fill(Text::CardSaved, {Name});
				case EDeviceTransfer::ImportFile:
				case EDeviceTransfer::ModZip:
				case EDeviceTransfer::ModFolder:
					return std::nullopt;
				}
				return std::nullopt;
			case ETransferState::Failed:
			{
				const std::size_t Failure = static_cast<std::size_t>(Status.Failure);
				if (Failure == 0 || Failure > Text::Failures.size() || Text::Failures[Failure - 1][0].empty())
				{
					return std::nullopt;
				}
				return OptionsText::Fill(Text::Failures[Failure - 1], {Name});
			}
			case ETransferState::Idle:
			case ETransferState::Cancelled:
				return std::nullopt;
			}
			return std::nullopt;
		}

		/** The user's home: where browsing starts the first time. */
		std::filesystem::path Home()
		{
#ifdef _WIN32
			wchar_t* Value = nullptr;
			std::size_t Length = 0;
			if (_wdupenv_s(&Value, &Length, L"USERPROFILE") == 0 && Value != nullptr)
			{
				const std::filesystem::path Folder(Value);
				std::free(Value);
				return Folder;
			}
#else
			if (const char* Value = std::getenv("HOME"))
			{
				return Value;
			}
#endif
			std::error_code Error;
			return std::filesystem::current_path(Error);
		}

		std::optional<std::vector<uint8>> ReadFile(const std::filesystem::path& Path)
		{
			std::ifstream File(Path, std::ios::binary);
			if (!File)
			{
				return std::nullopt;
			}
			return std::vector<uint8>((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		}
	}

	FMenuSavePages::FMenuSavePages(FSaveStore& InSaves, const FMenuResources& InResources, IMenuDeviceFiles* InFiles)
		: Saves(InSaves), Resources(InResources), Files(InFiles)
	{
	}

	std::string FMenuSavePages::Title() const
	{
		switch (Shown)
		{
		case ESavesPage::Saves:
			return std::string(OptionsText::Titles::Saves);
		case ESavesPage::Browse:
			return Shortened(Browser->FolderName(), TitleLetters);
		case ESavesPage::Import:
			return std::string(OptionsText::Titles::Import);
		}
		return {};
	}

	std::vector<FSettingsRow> FMenuSavePages::Rows() const
	{
		std::vector<FSettingsRow> Rows;
		switch (Shown)
		{
		case ESavesPage::Saves:
			Rows.push_back(RowOfWords(Text::Import));
			Rows.push_back(RowOfWords(Text::Export, CardName(ExportStore)));
			if (Files != nullptr)
			{
				Rows[SavesRow::Import].Help = OptionsText::Fill(Text::ImportPickedHelp);
				Rows[SavesRow::Export].Help = OptionsText::Fill(Text::ExportPickedHelp);
				FSettingsRow Heading;
				Heading.Label = std::string(Text::DeviceHeading);
				Heading.bHeading = true;
				Rows.push_back(std::move(Heading));
				Rows.push_back(RowOfWords(Text::AddDiscs));
				Rows.push_back(RowOfWords(Text::AddDiscFolder));
				Rows.push_back(RowOfWords(Text::BackUp));
				Rows.push_back(RowOfWords(Text::Restore));
				Rows.push_back(RowOfWords(Text::SaveLog));
				if (bRestoreAsked)
				{
					Rows[SavesRow::Restore].Help = OptionsText::Fill(Text::RestoreAsked);
				}
				TellTransfer(Rows);
			}
			break;
		case ESavesPage::Browse:
			for (const FBrowserEntry& Entry : Browser->Entries())
			{
				switch (Entry.Kind)
				{
				case FBrowserEntry::EKind::Up:
					Rows.push_back(RowOfWords(Text::Up));
					break;
				case FBrowserEntry::EKind::Folder:
					Rows.push_back({Shortened(Entry.Name, RowLetters), std::string(Browser->Folder().empty() ? Text::Drive : Text::Folder), Wrapped(Entry.Name)});
					break;
				case FBrowserEntry::EKind::File:
					Rows.push_back({Shortened(Entry.Name, RowLetters), "", Wrapped(Entry.Name)});
					break;
				}
			}
			if (Rows.empty())
			{
				Rows.push_back({std::string(Text::NoFolders), "", {"", "", ""}});
			}
			break;
		case ESavesPage::Import:
		{
			Rows.push_back(RowOfWords(Text::Into, CardName(IntoStore)));
			const int32 Replacing = Replaced(IntoStore);
			FSettingsRow Copy = RowOfWords(Text::Copy);
			Copy.Label = OptionsText::Fill(Text::Copy.Label, {SaveCount(static_cast<int32>(Picked.size()))});
			if (Replacing > 0)
			{
				Copy.Value = OptionsText::Fill(Text::Overwrites, {std::to_string(Replacing)});
				Copy.Help = OptionsText::Fill(Text::OverwriteHelp, {CardName(IntoStore)});
				Copy.Help[1] = ReplacedNumbers(IntoStore);
			}
			if (bReplaceAsked)
			{
				Copy.Help = OptionsText::Fill(Text::OverwriteAsked, {CardName(IntoStore)});
				Copy.Help[1] = ReplacedNumbers(IntoStore);
			}
			Rows.push_back(std::move(Copy));
			for (const FOriginalSave& Save : Picked)
			{
				Rows.push_back({SaveNumber(Save), PlayTime(Save.PlayTime()), OptionsText::Fill(Text::SaveHelp, {SaveNumber(Save), PlayTime(Save.PlayTime())})});
			}
			break;
		}
		}
		if (ToldRow >= 0 && ToldRow < static_cast<int32>(Rows.size()))
		{
			Rows[static_cast<std::size_t>(ToldRow)].Help = Told;
		}
		return Rows;
	}

	bool FMenuSavePages::Step(int32 Row, int32 Direction)
	{
		int32* Store = nullptr;
		if (Shown == ESavesPage::Saves && Row == SavesRow::Export)
		{
			Store = &ExportStore;
		}
		else if (Shown == ESavesPage::Import && Row == ImportRow::Into)
		{
			Store = &IntoStore;
		}
		if (Store == nullptr)
		{
			return false;
		}
		*Store = (*Store + Direction + FSaveStore::StoreCount) % FSaveStore::StoreCount;
		bReplaceAsked = false;
		ToldRow = -1;
		return true;
	}

	bool FMenuSavePages::Activate(int32 Row)
	{
		switch (Shown)
		{
		case ESavesPage::Saves:
			switch (Row)
			{
			case SavesRow::Import:
				if (Files != nullptr)
				{
					return StartTransfer(EDeviceTransfer::ImportFile);
				}
				Browse();
				return true;
			case SavesRow::Export:
				return Export();
			case SavesRow::AddDiscs:
				return StartTransfer(EDeviceTransfer::Discs);
			case SavesRow::AddDiscFolder:
				return StartTransfer(EDeviceTransfer::DiscFolder);
			case SavesRow::BackUp:
				return StartTransfer(EDeviceTransfer::BackUp);
			case SavesRow::Restore:
				if (Files != nullptr && !bRestoreAsked)
				{
					bRestoreAsked = true;
					return true;
				}
				bRestoreAsked = false;
				return StartTransfer(EDeviceTransfer::Restore);
			case SavesRow::SaveLog:
				return StartTransfer(EDeviceTransfer::Log);
			default:
				return false;
			}
		case ESavesPage::Browse:
			if (Row < 0 || Row >= static_cast<int32>(Browser->Entries().size()))
			{
				return false;
			}
			if (const std::optional<std::filesystem::path> File = Browser->Open(Row))
			{
				if (Pick(*File, Row))
				{
					Saves.KeepImportFolder(Browser->Folder());
				}
			}
			else
			{
				++Views;
			}
			return true;
		case ESavesPage::Import:
			if (Row != ImportRow::Copy)
			{
				return false;
			}
			if (Replaced(IntoStore) > 0 && !bReplaceAsked)
			{
				bReplaceAsked = true;
				return true;
			}
			Import();
			return true;
		}
		return false;
	}

	bool FMenuSavePages::Back()
	{
		ToldRow = -1;
		bReplaceAsked = false;
		bRestoreAsked = false;
		switch (Shown)
		{
		case ESavesPage::Saves:
			return false;
		case ESavesPage::Browse:
			Shown = ESavesPage::Saves;
			++Views;
			return true;
		case ESavesPage::Import:
			// The file picked through the device's picker was never browsed to.
			Shown = Browser ? ESavesPage::Browse : ESavesPage::Saves;
			++Views;
			return true;
		}
		return false;
	}

	void FMenuSavePages::CursorMoved()
	{
		ToldRow = -1;
		bReplaceAsked = false;
		bRestoreAsked = false;
	}

	bool FMenuSavePages::Refresh()
	{
		if (Files == nullptr)
		{
			return false;
		}
		const FTransferStatus Status = Files->Status();
		if (Status.Revision == SeenRevision)
		{
			return false;
		}
		SeenRevision = Status.Revision;
		if (Status.Running())
		{
			Followed = Status.Kind;
		}
		else if (Followed == EDeviceTransfer::ImportFile && Status.Kind == EDeviceTransfer::ImportFile && Status.State == ETransferState::Done)
		{
			Followed.reset();
			if (Shown == ESavesPage::Saves)
			{
				Pick(Status.Picked, SavesRow::Import);
			}
		}
		return true;
	}

	bool FMenuSavePages::StartTransfer(EDeviceTransfer Kind, const std::filesystem::path& File)
	{
		if (Files == nullptr || !Files->Start(Kind, File))
		{
			return false;
		}
		Followed = Kind;
		ToldRow = -1;
		return true;
	}

	void FMenuSavePages::TellTransfer(std::vector<FSettingsRow>& Rows) const
	{
		const FTransferStatus Status = Files->Status();
		const std::optional<int32> Row = TransferRow(Status.Kind);
		if (!Row || (!Status.Running() && Followed != Status.Kind))
		{
			return;
		}
		if (std::optional<std::array<std::string, 3>> Lines = TransferLines(Status))
		{
			Rows[static_cast<std::size_t>(*Row)].Help = std::move(*Lines);
		}
	}

	void FMenuSavePages::Browse()
	{
		Browser.emplace(Saves.ImportFolder().value_or(Home()));
		Shown = ESavesPage::Browse;
		++Views;
		ToldRow = -1;
	}

	bool FMenuSavePages::Pick(const std::filesystem::path& File, int32 Row)
	{
		const std::optional<std::vector<uint8>> Bytes = ReadFile(File);
		if (!Bytes)
		{
			Tell(Row, OptionsText::Fill(Text::Unreadable));
			return false;
		}
		TResult<std::vector<FOriginalSave>> Read = ReadCardImageSaves(*Bytes);
		if (!Read)
		{
			Tell(Row, OptionsText::Fill(Text::NotACard));
			return false;
		}
		if (Read->empty())
		{
			Tell(Row, OptionsText::Fill(Text::NoSaves));
			return false;
		}
		Picked = std::move(*Read);
		// The first card with room for every save at its number, else the first.
		IntoStore = 0;
		for (int32 Store = 0; Store < FSaveStore::StoreCount; ++Store)
		{
			if (Replaced(Store) == 0)
			{
				IntoStore = Store;
				break;
			}
		}
		bReplaceAsked = false;
		ToldRow = -1;
		Shown = ESavesPage::Import;
		++Views;
		return true;
	}

	void FMenuSavePages::Import()
	{
		bReplaceAsked = false;
		for (const FOriginalSave& Save : Picked)
		{
			if (Saves.Copy(IntoStore * FSaveStore::SlotsPerStore + Save.CardSlot(), Save.Data()))
			{
				Tell(ImportRow::Copy, OptionsText::Fill(Text::CopyFailed));
				return;
			}
		}
		Tell(ImportRow::Copy, OptionsText::Fill(Text::Copied, {SaveCount(static_cast<int32>(Picked.size())), CardName(IntoStore)}));
	}

	bool FMenuSavePages::Export()
	{
		TResult<std::vector<uint8>> Image = ExportStoreCard(Saves, ExportStore, Resources);
		if (!Image)
		{
			Tell(SavesRow::Export, OptionsText::Fill(Text::DamagedCard));
			return true;
		}
		const TResult<std::filesystem::path> Written = Saves.WriteCardImage(ExportStore, *Image);
		if (!Written)
		{
			Tell(SavesRow::Export, OptionsText::Fill(Text::WriteFailed));
			return true;
		}
		if (Files != nullptr)
		{
			// Saved where the player chooses, the Cards folder being out of their reach.
			return StartTransfer(EDeviceTransfer::CardImage, *Written);
		}
		Tell(SavesRow::Export, OptionsText::Fill(Text::Exported, {CardName(ExportStore)}));
		return true;
	}

	bool FMenuSavePages::HoldsSave(int32 Store, const FOriginalSave& Save) const
	{
		// A slot whose file cannot even be read counts as holding a save: it is never replaced unasked.
		const TResult<std::optional<std::vector<uint8>>> There = Saves.ReadBytes(Store * FSaveStore::SlotsPerStore + Save.CardSlot());
		return !There || *There;
	}

	int32 FMenuSavePages::Replaced(int32 Store) const
	{
		int32 Count = 0;
		for (const FOriginalSave& Save : Picked)
		{
			Count += HoldsSave(Store, Save) ? 1 : 0;
		}
		return Count;
	}

	std::string FMenuSavePages::ReplacedNumbers(int32 Store) const
	{
		// Five fit a help line; past five, four and how many more.
		constexpr std::size_t Listed = 5;
		std::vector<std::string> Numbers;
		for (const FOriginalSave& Save : Picked)
		{
			if (HoldsSave(Store, Save))
			{
				Numbers.push_back(SaveNumber(Save));
			}
		}
		std::string Line;
		const std::size_t Named = Numbers.size() <= Listed ? Numbers.size() : Listed - 1;
		for (std::size_t Number = 0; Number < Named; ++Number)
		{
			Line += (Line.empty() ? "" : " ") + Numbers[Number];
		}
		if (Named < Numbers.size())
		{
			Line += " +" + std::to_string(Numbers.size() - Named);
		}
		return Line;
	}

	void FMenuSavePages::Tell(int32 Row, std::array<std::string, 3> Lines)
	{
		ToldRow = Row;
		Told = std::move(Lines);
	}
}
