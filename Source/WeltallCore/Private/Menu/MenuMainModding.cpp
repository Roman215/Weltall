#include "Menu/MenuMain.h"

#include "Menu/OptionsText.h"

#include <vector>

namespace Weltall
{
	namespace
	{
		/** The rows the screen adds after the Modding page's own, before the device's. */
		enum class EModdingRow : uint8
		{
			Heading,
			Assets,
			/** External assets itself, in the Assets page's place where the device's files go through its picker. */
			External,
			PerfectWorks,
		};

		/** The added rows: the mods' heading over the Assets page's row and each mod's, those the game has. */
		std::vector<EModdingRow> ModdingRows(const IMenuSettings& Settings)
		{
			std::vector<EModdingRow> Rows;
			const bool bAssets = Settings.Assets() != nullptr;
			const bool bPerfectWorks = Settings.Mods().bPerfectWorks;
			if (bAssets || bPerfectWorks)
			{
				Rows.push_back(EModdingRow::Heading);
			}
			if (bAssets)
			{
				Rows.push_back(Settings.DeviceFiles() != nullptr ? EModdingRow::External : EModdingRow::Assets);
			}
			if (bPerfectWorks)
			{
				Rows.push_back(EModdingRow::PerfectWorks);
			}
			return Rows;
		}
	}

	bool FMenuMain::OnModdingTurned() const
	{
		return OptionsState.Turned == TurnedPages() - 1;
	}

	int32 FMenuMain::ModFilesRow() const
	{
		return FSettingsPages::RowCount(ESettingsPage::Modding) + static_cast<int32>(ModdingRows(*Services.Settings).size());
	}

	void FMenuMain::AddModdingRows()
	{
		namespace Text = OptionsText::Modding;
		for (const EModdingRow Row : ModdingRows(*Services.Settings))
		{
			switch (Row)
			{
			case EModdingRow::Heading:
			{
				FSettingsRow Heading;
				Heading.Label = std::string(Text::Mods);
				Heading.bHeading = true;
				OptionsState.Rows.push_back(std::move(Heading));
				break;
			}
			case EModdingRow::Assets:
				OptionsState.Rows.push_back({std::string(Text::Assets.Label), {}, OptionsText::Fill(Text::Assets.Help)});
				break;
			case EModdingRow::External:
			{
				const bool bOn = OptionsState.Settings.bExternalAssets;
				OptionsState.Rows.push_back({std::string(Text::External.Label), std::string(bOn ? OptionsText::On : OptionsText::Off), OptionsText::Fill(Text::External.Help)});
				break;
			}
			case EModdingRow::PerfectWorks:
			{
				const std::string Version = Services.Settings->Mods().PerfectWorksVersion;
				const bool bPicked = OptionsState.ModFiles != nullptr;
				std::array<std::string, 3> Help = OptionsText::Fill(bPicked ? Text::PerfectWorksPicked : Text::PerfectWorks.Help, {Version});
				if (Version.empty())
				{
					Help[0] = std::string(Text::PerfectWorksUnversioned);
				}
				const bool bOn = OptionsState.Settings.PerfectWorks.bEnabled;
				OptionsState.Rows.push_back({std::string(Text::PerfectWorks.Label), std::string(bOn ? OptionsText::On : OptionsText::Off), Help});
				break;
			}
			}
		}
		if (OptionsState.ModFiles)
		{
			OptionsState.ModFilesShownAt = static_cast<int32>(OptionsState.Rows.size());
			for (FSettingsRow& Row : OptionsState.ModFiles->Rows())
			{
				OptionsState.Rows.push_back(std::move(Row));
			}
		}
	}

	bool FMenuMain::StepModdingRow()
	{
		FMenuOptionsState& State = OptionsState;
		const std::vector<EModdingRow> Rows = ModdingRows(*Services.Settings);
		const std::size_t Added = static_cast<std::size_t>(State.Row - FSettingsPages::RowCount(ESettingsPage::Modding));
		if (Added >= Rows.size() || (Rows[Added] != EModdingRow::PerfectWorks && Rows[Added] != EModdingRow::External))
		{
			return false;
		}
		if (Rows[Added] == EModdingRow::External)
		{
			State.Settings.bExternalAssets = !State.Settings.bExternalAssets;
		}
		else
		{
			State.Settings.PerfectWorks.bEnabled = !State.Settings.PerfectWorks.bEnabled;
		}
		Services.Settings->Change(State.Settings);
		FillOptionsRows();
		return true;
	}

	bool FMenuMain::ActivateModdingRow()
	{
		FMenuOptionsState& State = OptionsState;
		const std::vector<EModdingRow> Rows = ModdingRows(*Services.Settings);
		const std::size_t Added = static_cast<std::size_t>(State.Row - FSettingsPages::RowCount(ESettingsPage::Modding));
		if (Added >= Rows.size())
		{
			if (!State.ModFiles || !State.ModFiles->Activate(State.Row - ModFilesRow()))
			{
				return false;
			}
			FillOptionsRows();
			return true;
		}
		switch (Rows[Added])
		{
		case EModdingRow::Heading:
			return false;
		case EModdingRow::External:
			return StepModdingRow();
		case EModdingRow::Assets:
			State.bAssetsOpened = true;
			break;
		case EModdingRow::PerfectWorks:
			State.Opened = ESettingsPage::PerfectWorks;
			break;
		}
		State.OpenedFrom = State.Row;
		ShowOptionsPage(true);
		return true;
	}

	bool FMenuMain::RefreshModFiles()
	{
		FMenuOptionsState& State = OptionsState;
		if (!State.ModFiles || State.Opened || State.bAssetsOpened || !OnModdingTurned())
		{
			return false;
		}
		// Perfect Works' row comes as the release added is found: the rows under it move down a row, the cursor with them.
		const int32 Was = State.ModFilesShownAt;
		if (!State.ModFiles->Refresh() && ModFilesRow() == Was)
		{
			return false;
		}
		if (const std::optional<FAddedMod> Added = State.ModFiles->TakeAdded())
		{
			// Added to be used: Perfect Works on from the next start, a pack's External assets at once.
			bool& Switch = Added->Content == EAddedContent::PerfectWorks ? State.Settings.PerfectWorks.bEnabled : State.Settings.bExternalAssets;
			if (!Switch)
			{
				Switch = true;
				Services.Settings->Change(State.Settings);
			}
		}
		if (State.Row >= Was)
		{
			State.Row += ModFilesRow() - Was;
		}
		ShowOptionsPage(false);
		return true;
	}
}
