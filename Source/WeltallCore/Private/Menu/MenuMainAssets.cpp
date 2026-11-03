#include "Menu/MenuMain.h"

#include "Menu/MenuAssetPages.h"
#include "Menu/OptionsText.h"

namespace Weltall
{
	bool FMenuMain::OnAssetsPage() const
	{
		return OptionsState.bAssetsOpened;
	}

	void FMenuMain::FillAssetRows()
	{
		const FMenuAssetProgress Progress = Services.Settings->Assets()->Progress();
		OptionsState.AssetRevision = Progress.Revision;
		OptionsState.Title = std::string(OptionsText::Titles::Assets);
		OptionsState.Rows = AssetSettingsRows(OptionsState.Settings, Progress);
	}

	bool FMenuMain::RefreshAssetProgress()
	{
		if (OptionsState.AssetRevision == Services.Settings->Assets()->Progress().Revision) { return false; }
		FillAssetRows();
		return true;
	}

	bool FMenuMain::StepAssetOption()
	{
		FMenuOptionsState& State = OptionsState;
		if (State.Row == AssetRow::External) { State.Settings.bExternalAssets = !State.Settings.bExternalAssets; }
		else if (State.Row == AssetRow::Capture) { State.Settings.bCaptureTextures = !State.Settings.bCaptureTextures; }
		else if (bool* Shown = AssetRowPackKind(State.Settings.PackKinds, State.Row)) { *Shown = !*Shown; }
		else { return false; }
		Services.Settings->Change(State.Settings);
		FillAssetRows();
		return true;
	}

	bool FMenuMain::ActivateAssetOption()
	{
		const int32 Row = OptionsState.Row;
		if (Row == AssetRow::External || Row == AssetRow::Capture || AssetRowPackKind(OptionsState.Settings.PackKinds, Row) != nullptr) { return StepAssetOption(); }
		const std::optional<EAssetExportKind> Kind = AssetRowExport(Row);
		if (!Kind) { return false; }
		IMenuAssets& Assets = *Services.Settings->Assets();
		const FMenuAssetProgress Progress = Assets.Progress();
		if (Progress.State == EAssetExportState::Running)
		{
			if (Progress.Kind != *Kind) { return false; }
			Assets.CancelExport();
			FillAssetRows();
			return true;
		}
		const bool bStarted = Assets.StartExport(*Kind);
		FillAssetRows();
		return bStarted;
	}
}
