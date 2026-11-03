#include "Menu/MenuMain.h"

#include "Menu/MenuAssetPages.h"
#include "Menu/MenuFileBrowser.h"
#include "Menu/OptionsText.h"
#include "Story/ShareableExpansion.h"

#include <algorithm>
#include <functional>
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
			Italian,
			/**
			 * The second disc's heading, over its expansions' switch, the one offered, each that cannot be played or has
			 * documents for another disc, the disc's export, the offered expansion's shareable copy and the example scenes' copy.
			 */
			Disc2Heading,
			Expansions,
			Expansion,
			Unplayable,
			ForAnotherDisc,
			ExportDisc2,
			MakeShareable,
			ExampleScenes,
		};

		struct FModdingRow
		{
			EModdingRow Kind;
			/** For an expansion that cannot be played or has documents for another disc, its place among the mods' expansions. */
			std::size_t Expansion = 0;
		};

		std::string Shown(std::string_view Name, std::size_t Letters)
		{
			return FFileBrowser::ShortDisplayName(Name, Letters);
		}

		/** Whether the game exports the second disc: where it has the Assets page's exports and the player reaches the Mods folder. */
		bool ExportsDisc2(const IMenuSettings& Settings)
		{
			return Settings.Assets() != nullptr && Settings.DeviceFiles() == nullptr;
		}

		/** The playable expansion the second disc's question offers with Options, as FStoryLibrary::Offered finds it; null for none. */
		const FMenuExpansion* Offered(const std::vector<FMenuExpansion>& Expansions, const FExpansionOptions& Options)
		{
			const auto Chosen = std::find_if(Expansions.begin(), Expansions.end(),
				[&Options](const FMenuExpansion& Expansion) { return Expansion.bPlayable && Expansion.Folder == Options.Folder; });
			const auto First = std::find_if(Expansions.begin(), Expansions.end(), [](const FMenuExpansion& Expansion) { return Expansion.bPlayable; });
			return Chosen != Expansions.end() ? &*Chosen : First != Expansions.end() ? &*First : nullptr;
		}

		/**
		 * The added rows: the mods' heading over the Assets page's row and each mod's, those the game has; then the second
		 * disc's, where it has its example scenes, exports the disc or an expansion is found: the expansion offered, and
		 * its shareable copy where the game exports the disc, only while they are on.
		 */
		std::vector<FModdingRow> ModdingRows(const IMenuSettings& Settings)
		{
			std::vector<FModdingRow> Rows;
			const FMenuMods Mods = Settings.Mods();
			const bool bAssets = Settings.Assets() != nullptr;
			if (bAssets || Mods.bPerfectWorks || Mods.bItalian)
			{
				Rows.push_back({EModdingRow::Heading});
			}
			if (bAssets)
			{
				Rows.push_back({Settings.DeviceFiles() != nullptr ? EModdingRow::External : EModdingRow::Assets});
			}
			if (Mods.bPerfectWorks)
			{
				Rows.push_back({EModdingRow::PerfectWorks});
			}
			if (Mods.bItalian)
			{
				Rows.push_back({EModdingRow::Italian});
			}
			if (!Settings.HasExampleScenes() && !ExportsDisc2(Settings) && Mods.Expansions.empty())
			{
				return Rows;
			}
			Rows.push_back({EModdingRow::Disc2Heading});
			Rows.push_back({EModdingRow::Expansions});
			const bool bOn = Settings.Settings().Expansions.bEnabled;
			const auto Playable = [](const FMenuExpansion& Expansion) { return Expansion.bPlayable; };
			if (bOn && std::any_of(Mods.Expansions.begin(), Mods.Expansions.end(), Playable))
			{
				Rows.push_back({EModdingRow::Expansion});
			}
			for (std::size_t Expansion = 0; bOn && Expansion < Mods.Expansions.size(); ++Expansion)
			{
				const FMenuExpansion& Found = Mods.Expansions[Expansion];
				if (!Found.bPlayable || Found.ForAnotherDisc > 0)
				{
					Rows.push_back({Found.bPlayable ? EModdingRow::ForAnotherDisc : EModdingRow::Unplayable, Expansion});
				}
			}
			if (ExportsDisc2(Settings))
			{
				Rows.push_back({EModdingRow::ExportDisc2});
			}
			if (ExportsDisc2(Settings) && bOn && Offered(Mods.Expansions, Settings.Settings().Expansions) != nullptr)
			{
				Rows.push_back({EModdingRow::MakeShareable});
			}
			if (Settings.HasExampleScenes())
			{
				Rows.push_back({EModdingRow::ExampleScenes});
			}
			return Rows;
		}

		/**
		 * The row of an export of Kind as it stands: its progress as the value, Help's help for where the export stands (Idle
		 * where another runs or ran); read-only while another export runs.
		 */
		FSettingsRow ExportRow(const FMenuAssetProgress& Progress, EAssetExportKind Kind, std::string_view Label,
			const std::function<std::array<std::string, 3>(EAssetExportState)>& Help)
		{
			const bool bOurs = Progress.Kind == Kind;
			const EAssetExportState State = bOurs ? Progress.State : EAssetExportState::Idle;
			FSettingsRow Row{std::string(Label), {}, Help(State)};
			if (State != EAssetExportState::Idle)
			{
				Row.Value = std::string(OptionsText::Assets::States[static_cast<std::size_t>(State)]);
			}
			if (State == EAssetExportState::Running && Progress.Total != 0)
			{
				Row.Value = std::to_string(std::min(Progress.Completed, Progress.Total) * 100 / Progress.Total) + "%";
			}
			Row.bReadOnly = Progress.State == EAssetExportState::Running && !bOurs;
			return Row;
		}

		FSettingsRow ExportDisc2Row(const FMenuAssetProgress& Progress)
		{
			namespace Text = OptionsText::Modding;
			return ExportRow(Progress, EAssetExportKind::Disc2, Text::ExportDisc2.Label, [](EAssetExportState State)
				{
					return OptionsText::Fill(State == EAssetExportState::Running ? Text::ExportDisc2Working
						: State == EAssetExportState::Complete ? Text::ExportDisc2Done
						: State == EAssetExportState::Failed ? Text::ExportDisc2Failed
						: State == EAssetExportState::Unavailable ? Text::ExportDisc2Unavailable : Text::ExportDisc2.Help);
				});
		}

		/** Make shareable's row for the expansion Shares, as its copy stands; bThere where it told that the copy is there already. */
		FSettingsRow MakeShareableRow(const FMenuAssetProgress& Progress, const FMenuExpansion& Shares, bool bThere)
		{
			namespace Text = OptionsText::Modding;
			constexpr std::size_t LineLetters = 30;
			const std::string Name = Shown(Shares.Info.Name, LineLetters);
			const std::string Copy = Shown(Shares.Folder + std::string(FShareableExpansion::Suffix), LineLetters);
			return ExportRow(Progress, EAssetExportKind::Shareable, Text::MakeShareable.Label, [&](EAssetExportState State)
				{
					return bThere ? OptionsText::Fill(Text::MakeShareableThere)
						: State == EAssetExportState::Running ? OptionsText::Fill(Text::MakeShareableWorking)
						: State == EAssetExportState::Complete ? OptionsText::Fill(Text::MakeShareableDone, {Copy})
						: State == EAssetExportState::Failed ? OptionsText::Fill(Text::MakeShareableFailed)
						: State == EAssetExportState::Unavailable ? OptionsText::Fill(Text::MakeShareableUnavailable) : OptionsText::Fill(Text::MakeShareable.Help, {Name});
				});
		}

		/** Who made an expansion and its version, as its mod.txt gives them, on a line. */
		std::string MadeBy(const FModInfo& Info)
		{
			constexpr std::size_t LineLetters = 30;
			const std::string Author = Info.Author.empty() ? std::string() : "by " + Info.Author;
			const std::string Version = Info.Version.empty() ? std::string() : "version " + Info.Version;
			return Shown(Author + (Author.empty() || Version.empty() ? "" : ", ") + Version, LineLetters);
		}
	}

	bool FMenuMain::OnModdingTurned() const
	{
		return OptionsState.Turned == TurnedPages() - 1;
	}

	int32 FMenuMain::ExpansionRow() const
	{
		const std::vector<FModdingRow> Rows = ModdingRows(*Services.Settings);
		const auto Found = std::find_if(Rows.begin(), Rows.end(), [](const FModdingRow& Row) { return Row.Kind == EModdingRow::Expansion; });
		const auto Switch = std::find_if(Rows.begin(), Rows.end(), [](const FModdingRow& Row) { return Row.Kind == EModdingRow::Expansions; });
		const auto Shown = Found != Rows.end() ? Found : Switch;
		return Shown != Rows.end() ? FSettingsPages::RowCount(ESettingsPage::Modding) + static_cast<int32>(Shown - Rows.begin()) : 0;
	}

	int32 FMenuMain::ModFilesRow() const
	{
		return FSettingsPages::RowCount(ESettingsPage::Modding) + static_cast<int32>(ModdingRows(*Services.Settings).size());
	}

	void FMenuMain::AddModdingRows()
	{
		namespace Text = OptionsText::Modding;
		constexpr std::size_t ValueLetters = 16;
		constexpr std::size_t LineLetters = 30;
		const FMenuMods Mods = Services.Settings->Mods();
		for (const FModdingRow& Row : ModdingRows(*Services.Settings))
		{
			switch (Row.Kind)
			{
			case EModdingRow::Heading:
			case EModdingRow::Disc2Heading:
			{
				FSettingsRow Heading;
				Heading.Label = std::string(Row.Kind == EModdingRow::Heading ? Text::Mods : Text::Disc2);
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
			case EModdingRow::Italian:
			{
				const bool bOn = OptionsState.Settings.Italian.bEnabled;
				OptionsState.Rows.push_back({std::string(Text::Italian.Label), std::string(bOn ? OptionsText::On : OptionsText::Off), OptionsText::Fill(Text::Italian.Help)});
				break;
			}
			case EModdingRow::Expansions:
			{
				const bool bOn = OptionsState.Settings.Expansions.bEnabled;
				OptionsState.Rows.push_back({std::string(Text::Expansions.Label), std::string(bOn ? OptionsText::On : OptionsText::Off), OptionsText::Fill(Text::Expansions.Help)});
				break;
			}
			case EModdingRow::Expansion:
			{
				const FMenuExpansion* Shows = Offered(Mods.Expansions, OptionsState.Settings.Expansions);
				const std::string Name = Shown(Shows->Info.Name, LineLetters);
				OptionsState.Rows.push_back({std::string(Text::Expansion), Shown(Shows->Info.Name, ValueLetters), OptionsText::Fill(Text::ExpansionHelp, {Name, MadeBy(Shows->Info)})});
				break;
			}
			case EModdingRow::Unplayable:
			{
				const FMenuExpansion& Unplayable = Mods.Expansions[Row.Expansion];
				OptionsState.Rows.push_back({Shown(Unplayable.Info.Name, ValueLetters), std::string(Unplayable.bUnchanged ? Text::Unchanged : Text::Unplayable),
					OptionsText::Fill(Unplayable.bUnchanged ? Text::UnchangedHelp : Text::UnplayableHelp, {Shown(Unplayable.Info.Name, LineLetters)})});
				break;
			}
			case EModdingRow::ForAnotherDisc:
			{
				const FMenuExpansion& Partial = Mods.Expansions[Row.Expansion];
				const std::string HowMany = std::to_string(Partial.ForAnotherDisc) + (Partial.ForAnotherDisc == 1 ? " file" : " files");
				OptionsState.Rows.push_back({Shown(Partial.Info.Name, ValueLetters), std::string(Text::ForAnotherDisc),
					OptionsText::Fill(Text::ForAnotherDiscHelp, {Shown(Partial.Info.Name, LineLetters), HowMany})});
				break;
			}
			case EModdingRow::ExportDisc2:
			{
				const FMenuAssetProgress Progress = Services.Settings->Assets()->Progress();
				OptionsState.AssetRevision = Progress.Revision;
				OptionsState.Rows.push_back(ExportDisc2Row(Progress));
				break;
			}
			case EModdingRow::MakeShareable:
			{
				const FMenuAssetProgress Progress = Services.Settings->Assets()->Progress();
				OptionsState.AssetRevision = Progress.Revision;
				OptionsState.Rows.push_back(MakeShareableRow(Progress, *Offered(Mods.Expansions, OptionsState.Settings.Expansions), OptionsState.bShareableThere));
				break;
			}
			case EModdingRow::ExampleScenes:
			{
				const std::optional<EExampleScenesCopy> Told = OptionsState.ExampleScenesTold;
				const std::array<std::string_view, 3>& Help = !Told ? Text::ExampleScenes.Help
					: *Told == EExampleScenesCopy::Copied ? Text::ExampleScenesCopied
					: *Told == EExampleScenesCopy::AlreadyThere ? Text::ExampleScenesThere : Text::ExampleScenesFailed;
				OptionsState.Rows.push_back({std::string(Text::ExampleScenes.Label), {}, OptionsText::Fill(Help)});
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

	bool FMenuMain::StepModdingRow(int32 Direction)
	{
		FMenuOptionsState& State = OptionsState;
		const std::vector<FModdingRow> Rows = ModdingRows(*Services.Settings);
		const std::size_t Added = static_cast<std::size_t>(State.Row - FSettingsPages::RowCount(ESettingsPage::Modding));
		if (Added >= Rows.size())
		{
			return false;
		}
		switch (Rows[Added].Kind)
		{
		case EModdingRow::External:
			State.Settings.bExternalAssets = !State.Settings.bExternalAssets;
			break;
		case EModdingRow::PerfectWorks:
			State.Settings.PerfectWorks.bEnabled = !State.Settings.PerfectWorks.bEnabled;
			break;
		case EModdingRow::Italian:
			State.Settings.Italian.bEnabled = !State.Settings.Italian.bEnabled;
			break;
		case EModdingRow::Expansions:
			State.Settings.Expansions.bEnabled = !State.Settings.Expansions.bEnabled;
			break;
		case EModdingRow::Expansion:
		{
			// The playable expansions in turn, from the one offered now, round from the last to the first.
			std::vector<const FMenuExpansion*> Playable;
			const FMenuMods Mods = Services.Settings->Mods();
			for (const FMenuExpansion& Expansion : Mods.Expansions)
			{
				if (Expansion.bPlayable)
				{
					Playable.push_back(&Expansion);
				}
			}
			const FMenuExpansion* Now = Offered(Mods.Expansions, State.Settings.Expansions);
			const int32 Count = static_cast<int32>(Playable.size());
			if (Count < 2)
			{
				return false;
			}
			const int32 At = static_cast<int32>(std::find(Playable.begin(), Playable.end(), Now) - Playable.begin());
			State.Settings.Expansions.Folder = Playable[static_cast<std::size_t>((At + Direction % Count + Count) % Count)]->Folder;
			break;
		}
		default:
			return false;
		}
		Services.Settings->Change(State.Settings);
		FillOptionsRows();
		return true;
	}

	bool FMenuMain::ActivateModdingRow()
	{
		FMenuOptionsState& State = OptionsState;
		const std::vector<FModdingRow> Rows = ModdingRows(*Services.Settings);
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
		switch (Rows[Added].Kind)
		{
		case EModdingRow::Heading:
		case EModdingRow::Disc2Heading:
		case EModdingRow::Expansion:
		case EModdingRow::Unplayable:
		case EModdingRow::ForAnotherDisc:
			return false;
		case EModdingRow::External:
		case EModdingRow::Expansions:
			return StepModdingRow(1);
		case EModdingRow::ExportDisc2:
		{
			// The exports run one at a time: Confirm starts this one, or stops it after its step while it runs.
			IMenuAssets& Assets = *Services.Settings->Assets();
			const FMenuAssetProgress Progress = Assets.Progress();
			const bool bRunning = Progress.State == EAssetExportState::Running;
			if (bRunning && Progress.Kind != EAssetExportKind::Disc2)
			{
				return false;
			}
			if (bRunning)
			{
				Assets.CancelExport();
			}
			else
			{
				Assets.StartExport(EAssetExportKind::Disc2);
			}
			FillOptionsRows();
			return true;
		}
		case EModdingRow::MakeShareable:
		{
			// As Export Disc 2's, and a copy there already is replaced only on a second Confirm.
			IMenuAssets& Assets = *Services.Settings->Assets();
			const FMenuAssetProgress Progress = Assets.Progress();
			const bool bRunning = Progress.State == EAssetExportState::Running;
			if (bRunning && Progress.Kind != EAssetExportKind::Shareable)
			{
				return false;
			}
			if (bRunning)
			{
				Assets.CancelExport();
			}
			else
			{
				const FMenuMods Mods = Services.Settings->Mods();
				const FMenuExpansion* Shares = Offered(Mods.Expansions, State.Settings.Expansions);
				State.bShareableThere = Shares != nullptr && Assets.StartShareable(Shares->Folder, State.bShareableThere) == EShareableStart::CopyThere;
			}
			FillOptionsRows();
			return true;
		}
		case EModdingRow::ExampleScenes:
			// A copy in the Mods folder already is replaced only on a second Confirm: the player may have changed it.
			State.ExampleScenesTold = Services.Settings->CopyExampleScenes(State.ExampleScenesTold == EExampleScenesCopy::AlreadyThere);
			FillOptionsRows();
			return true;
		case EModdingRow::Assets:
			State.bAssetsOpened = true;
			break;
		case EModdingRow::PerfectWorks:
			State.Opened = ESettingsPage::PerfectWorks;
			break;
		case EModdingRow::Italian:
			State.Opened = ESettingsPage::Italian;
			break;
		}
		State.OpenedFrom = State.Row;
		ShowOptionsPage(true);
		return true;
	}

	bool FMenuMain::RefreshDisc2Export()
	{
		const FMenuOptionsState& State = OptionsState;
		if (State.Opened || State.bAssetsOpened || !OnModdingTurned() || !ExportsDisc2(*Services.Settings)
			|| State.AssetRevision == Services.Settings->Assets()->Progress().Revision)
		{
			return false;
		}
		ShowOptionsPage(false);
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
