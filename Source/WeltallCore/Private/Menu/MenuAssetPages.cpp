#include "Menu/MenuAssetPages.h"

#include "Menu/OptionsText.h"

#include <algorithm>
#include <array>

namespace Weltall
{
	bool* AssetRowPackKind(FTexturePackKinds& Kinds, int32 Row)
	{
		const std::array<bool*, AssetRow::PackKinds> InOrder{
			&Kinds.bText, &Kinds.bPortraits, &Kinds.bMenusAndHud, &Kinds.bCharacters, &Kinds.bScenery, &Kinds.bModelsAndEffects};
		const int32 Kind = Row - AssetRow::FirstPackKind;
		return Kind >= 0 && Kind < AssetRow::PackKinds ? InOrder[static_cast<std::size_t>(Kind)] : nullptr;
	}

	std::optional<EAssetExportKind> AssetRowExport(int32 Row)
	{
		switch (Row)
		{
		case AssetRow::Textures: return EAssetExportKind::Textures;
		case AssetRow::Text: return EAssetExportKind::Text;
		case AssetRow::Dump: return EAssetExportKind::Dump;
		case AssetRow::Music: return EAssetExportKind::Music;
		default: return std::nullopt;
		}
	}

	std::vector<FSettingsRow> AssetSettingsRows(const FPlayerSettings& Settings, FMenuAssetProgress Progress)
	{
		namespace Text = OptionsText::Assets;
		const bool bRunning = Progress.State == EAssetExportState::Running;
		const auto Action = [&Progress, bRunning](EAssetExportKind Kind, const OptionsText::FRow& Start)
		{
			const OptionsText::FRow& Shown = bRunning && Progress.Kind == Kind ? Text::Cancel : Start;
			FSettingsRow Row{std::string(Shown.Label), {}, OptionsText::Fill(Shown.Help)};
			Row.bReadOnly = bRunning && Progress.Kind != Kind;
			return Row;
		};
		std::vector<FSettingsRow> Rows;
		const auto Switch = [&Rows](const OptionsText::FRow& Words, bool bOn)
		{
			Rows.push_back({std::string(Words.Label), std::string(bOn ? OptionsText::On : OptionsText::Off), OptionsText::Fill(Words.Help)});
		};
		Switch(Text::External, Settings.bExternalAssets);
		Switch(Text::Capture, Settings.bCaptureTextures);
		FTexturePackKinds Kinds = Settings.PackKinds;
		for (int32 Kind = 0; Kind < AssetRow::PackKinds; ++Kind)
		{
			Switch(Text::PackKinds[static_cast<std::size_t>(Kind)], *AssetRowPackKind(Kinds, AssetRow::FirstPackKind + Kind));
		}
		Rows.push_back(Action(EAssetExportKind::Textures, Text::Textures));
		Rows.push_back(Action(EAssetExportKind::Text, Text::Text));
		Rows.push_back(Action(EAssetExportKind::Dump, Text::Export));
		Rows.push_back(Action(EAssetExportKind::Music, Text::Music));
		Rows.push_back({std::string(Text::Progress.Label), std::string(Text::States[static_cast<std::size_t>(Progress.State)]), OptionsText::Fill(Text::Progress.Help)});
		Rows[AssetRow::Status].bReadOnly = true;
		if (bRunning && Progress.Total != 0)
		{
			const uint64 Done = std::min(Progress.Completed, Progress.Total);
			Rows[AssetRow::Status].Value = Progress.Kind == EAssetExportKind::Music
				? "WAV " + std::to_string(Done) + "/" + std::to_string(Progress.Total)
				: std::to_string(Done * 100 / Progress.Total) + "%";
		}
		if (Progress.State == EAssetExportState::Failed)
		{
			Rows[AssetRow::Status].Help = {"Couldn't finish the export.", "Check space and permissions.", "Details are in the game log."};
		}
		return Rows;
	}
}
