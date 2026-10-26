#include "Menu/MenuModFiles.h"

#include "Menu/MenuFileBrowser.h"
#include "Menu/OptionsText.h"

#include <algorithm>
#include <cstdio>

namespace Weltall
{
	namespace
	{
		namespace Text = OptionsText::Modding;

		namespace ModRow
		{
			enum : int32 { Heading, AddZip, AddFolder, FirstItem };
		}

		/** A name beside a value, and one on a help line or after "Removed", in the font's 8-pixel letters. */
		constexpr std::size_t LabelLetters = 22;
		constexpr std::size_t LineLetters = 30;
		constexpr std::size_t RemovedLetters = 21;

		/** A name in the font's characters, its end cut short where it is longer than Letters. */
		std::string Shown(std::string_view Name, std::size_t Letters)
		{
			const std::string Characters = FFileBrowser::DisplayName(std::filesystem::path(std::u8string(Name.begin(), Name.end())));
			return Characters.size() <= Letters ? Characters : Characters.substr(0, Letters - 2) + "..";
		}

		/** Bytes as the device's own file manager counts them, in thousands. */
		std::string Size(uint64 Bytes)
		{
			char Text[24];
			if (Bytes >= 1'000'000'000)
			{
				std::snprintf(Text, sizeof Text, "%.1f GB", static_cast<double>(Bytes) / 1e9);
			}
			else if (Bytes >= 1'000'000)
			{
				std::snprintf(Text, sizeof Text, "%llu MB", static_cast<unsigned long long>((Bytes + 500'000) / 1'000'000));
			}
			else
			{
				std::snprintf(Text, sizeof Text, "%llu KB", static_cast<unsigned long long>(std::max<uint64>((Bytes + 500) / 1000, 1)));
			}
			return Text;
		}

		std::string FileCount(int32 Count)
		{
			namespace Saves = OptionsText::Saves;
			return Count == 1 ? std::string(Saves::OneFile) : OptionsText::Fill(Saves::ManyFiles, {std::to_string(Count)});
		}

		/** Perfect Works' version as the lines that add it take it: after a space, nothing where it names none. */
		std::string VersionAfterSpace(const std::string& Version)
		{
			return Version.empty() ? std::string() : " " + Shown(Version, 8);
		}

		/** What a transfer's row tells while it runs, and at its end; nothing once cancelled. */
		std::optional<std::array<std::string, 3>> TransferLines(const FTransferStatus& Status)
		{
			namespace Saves = OptionsText::Saves;
			const std::string Name = Shown(Status.Name, LineLetters);
			const std::string Percent = std::to_string(Status.Percent());
			switch (Status.State)
			{
			case ETransferState::Picking:
				return OptionsText::Fill(Saves::Picking);
			case ETransferState::Copying:
				if (Status.Kind == EDeviceTransfer::ModZip)
				{
					return OptionsText::Fill(Text::Extracting, {Name, Percent});
				}
				if (Status.Files == 0)
				{
					// A folder's files are counted before any is copied.
					return OptionsText::Fill(Text::Scanning, {Name, std::to_string(Status.File)});
				}
				return OptionsText::Fill(Saves::Copying, {std::to_string(std::max(Status.File, 1)), std::to_string(std::max(Status.Files, 1)), Name, Percent});
			case ETransferState::Done:
			{
				if (!Status.Added)
				{
					return std::nullopt;
				}
				const FAddedMod& Added = *Status.Added;
				const std::string Pack = Shown(Added.Name, LineLetters - 1);
				switch (Added.Content)
				{
				case EAddedContent::PerfectWorks:
					return OptionsText::Fill(Added.bOnRestart ? Text::PerfectWorksNext : Text::PerfectWorksAdded, {VersionAfterSpace(Added.Name)});
				case EAddedContent::Textures:
					return OptionsText::Fill(Text::TexturesAdded, {Pack});
				case EAddedContent::Music:
					return OptionsText::Fill(Text::MusicAdded, {Pack});
				case EAddedContent::Folders:
					return OptionsText::Fill(Text::FoldersAdded, {FileCount(Added.Files), Pack});
				}
				return std::nullopt;
			}
			case ETransferState::Failed:
				switch (Status.Failure)
				{
				case ETransferFailure::NotAMod:
					return OptionsText::Fill(Text::NotAMod);
				case ETransferFailure::NotAZip:
					return OptionsText::Fill(Text::NotAZip);
				case ETransferFailure::NoRoom:
				case ETransferFailure::Unreadable:
				case ETransferFailure::Unwritable:
				case ETransferFailure::NoPicker:
					return OptionsText::Fill(Saves::Failures[static_cast<std::size_t>(Status.Failure) - 1], {Name});
				case ETransferFailure::None:
				case ETransferFailure::NotABackup:
				case ETransferFailure::NoDiscImages:
					return std::nullopt;
				}
				return std::nullopt;
			case ETransferState::Idle:
			case ETransferState::Cancelled:
				return std::nullopt;
			}
			return std::nullopt;
		}

		bool IsModTransfer(EDeviceTransfer Kind)
		{
			return Kind == EDeviceTransfer::ModZip || Kind == EDeviceTransfer::ModFolder;
		}
	}

	FMenuModFiles::FMenuModFiles(IMenuDeviceFiles& InFiles) : Files(InFiles), Items(InFiles.Mods()), SeenRevision(InFiles.Status().Revision)
	{
	}

	std::vector<FSettingsRow> FMenuModFiles::Rows() const
	{
		std::vector<FSettingsRow> Rows;
		FSettingsRow Heading;
		Heading.Label = std::string(OptionsText::Saves::DeviceHeading);
		Heading.bHeading = true;
		Rows.push_back(std::move(Heading));
		Rows.push_back({std::string(Text::AddZip.Label), {}, OptionsText::Fill(Text::AddZip.Help)});
		Rows.push_back({std::string(Text::AddFolder.Label), {}, OptionsText::Fill(Text::AddFolder.Help)});
		for (const FModItem& Item : Items)
		{
			const std::string Name = Shown(Item.Name, LineLetters);
			FSettingsRow Row{Shown(Item.Name, LabelLetters), Size(Item.Bytes), {}};
			Row.Help = OptionsText::Fill(Text::Item, {Name, Text::ItemKinds[static_cast<std::size_t>(Item.Kind)], Row.Value});
			if (Item.bRemovedOnRestart)
			{
				Row.Value = std::string(Text::OnRestart);
				Row.Help = OptionsText::Fill(Text::RemovedOnRestart, {Name});
			}
			else if (static_cast<int32>(Rows.size()) == AskedRow)
			{
				Row.Help = OptionsText::Fill(Text::RemoveAsked, {Name});
			}
			else if (Item.bReplacedOnRestart)
			{
				Row.Help = OptionsText::Fill(Text::ReplacedOnRestart, {Name});
			}
			Rows.push_back(std::move(Row));
		}
		TellTransfer(Rows);
		if (ToldRow >= 0 && ToldRow < static_cast<int32>(Rows.size()))
		{
			Rows[static_cast<std::size_t>(ToldRow)].Help = Told;
		}
		return Rows;
	}

	bool FMenuModFiles::Activate(int32 Row)
	{
		switch (Row)
		{
		case ModRow::AddZip:
			return StartTransfer(EDeviceTransfer::ModZip);
		case ModRow::AddFolder:
			return StartTransfer(EDeviceTransfer::ModFolder);
		default:
			break;
		}
		const int32 Item = Row - ModRow::FirstItem;
		if (Item < 0 || Item >= static_cast<int32>(Items.size()) || Items[static_cast<std::size_t>(Item)].bRemovedOnRestart || Files.Status().Running())
		{
			return false;
		}
		if (AskedRow != Row)
		{
			AskedRow = Row;
			ToldRow = -1;
			return true;
		}
		AskedRow = -1;
		const FModItem Removed = Items[static_cast<std::size_t>(Item)];
		switch (Files.RemoveMod(Removed.Path))
		{
		case EModRemoval::Removed:
			Items.erase(Items.begin() + Item);
			// Told on the row the cursor is left on: the next one, or the one before where it was the last.
			ToldRow = std::min(Row, static_cast<int32>(Items.size()) + ModRow::FirstItem - 1);
			Told = OptionsText::Fill(Text::Removed, {Shown(Removed.Name, RemovedLetters)});
			return true;
		case EModRemoval::OnRestart:
			Items[static_cast<std::size_t>(Item)].bRemovedOnRestart = true;
			Items[static_cast<std::size_t>(Item)].bReplacedOnRestart = false;
			return true;
		case EModRemoval::Failed:
			ToldRow = Row;
			Told = OptionsText::Fill(Text::RemoveFailed);
			return true;
		}
		return false;
	}

	void FMenuModFiles::CursorMoved()
	{
		AskedRow = -1;
		ToldRow = -1;
	}

	bool FMenuModFiles::Refresh()
	{
		const FTransferStatus Status = Files.Status();
		if (Status.Revision == SeenRevision)
		{
			return false;
		}
		SeenRevision = Status.Revision;
		if (Status.Running())
		{
			bEndSeen = false;
			Followed = IsModTransfer(Status.Kind) ? std::optional(Status.Kind) : Followed;
		}
		else if (!bEndSeen)
		{
			bEndSeen = true;
			JustAdded = IsModTransfer(Status.Kind) && Status.State == ETransferState::Done ? Status.Added : std::nullopt;
		}
		std::vector<FModItem> Listed = Files.Mods();
		if (Listed != Items)
		{
			Items = std::move(Listed);
			AskedRow = -1;
		}
		return true;
	}

	bool FMenuModFiles::StartTransfer(EDeviceTransfer Kind)
	{
		if (!Files.Start(Kind))
		{
			return false;
		}
		Followed = Kind;
		bEndSeen = false;
		AskedRow = -1;
		ToldRow = -1;
		return true;
	}

	void FMenuModFiles::TellTransfer(std::vector<FSettingsRow>& Rows) const
	{
		const FTransferStatus Status = Files.Status();
		if (!IsModTransfer(Status.Kind) || (!Status.Running() && Followed != Status.Kind))
		{
			return;
		}
		if (std::optional<std::array<std::string, 3>> Lines = TransferLines(Status))
		{
			Rows[Status.Kind == EDeviceTransfer::ModZip ? ModRow::AddZip : ModRow::AddFolder].Help = std::move(*Lines);
		}
	}
}
