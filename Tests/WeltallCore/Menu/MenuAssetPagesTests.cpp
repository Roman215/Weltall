#include "Menu/MenuAssetPages.h"
#include "Menu/MenuSettingsPages.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <string>
#include <string_view>

using namespace Weltall;

namespace
{
	/** The Sound page's voices and subtitles rows, under its Language heading. */
	constexpr int32 VoicesRow = 10;
	constexpr int32 SubtitlesRow = 11;
}

TEST_CASE("Asset status rows preserve the selected action through progress and cancellation", "[Menu][Assets]")
{
	FPlayerSettings Settings;
	auto Rows = AssetSettingsRows(Settings, {});
	REQUIRE(Rows.size() == 13);
	CHECK(Rows[0].Value == "Off");
	CHECK(Rows[AssetRow::Capture].Label == "Capture textures");
	CHECK(Rows[AssetRow::Textures].Label == "Export textures");
	CHECK(Rows[AssetRow::Text].Label == "Export text");
	CHECK(Rows[AssetRow::Dump].Label == "Dump disc assets");
	CHECK(Rows[AssetRow::Music].Label == "Export music WAVs");
	CHECK(Rows[AssetRow::Status].Value == "Ready");
	CHECK(Rows[AssetRow::Status].bReadOnly);
	CHECK(AssetRowExport(AssetRow::Textures) == EAssetExportKind::Textures);
	CHECK(AssetRowExport(AssetRow::Text) == EAssetExportKind::Text);
	CHECK_FALSE(AssetRowExport(AssetRow::Capture));
	Settings.bExternalAssets = true;
	Rows = AssetSettingsRows(Settings, {EAssetExportState::Running, EAssetExportKind::Dump, 7, 10, 1});
	CHECK(Rows[0].Value == "On");
	CHECK(Rows[AssetRow::Dump].Label == "Cancel export");
	CHECK(Rows[AssetRow::Status].Value == "70%");
	CHECK(Rows[AssetRow::Music].bReadOnly);
	CHECK(Rows[AssetRow::Textures].bReadOnly);
	CHECK(AssetSettingsRows(Settings, {EAssetExportState::Cancelled})[AssetRow::Dump].Label == "Dump disc assets");
	CHECK(AssetSettingsRows(Settings, {EAssetExportState::Failed})[AssetRow::Status].Value == "Failed");
	Rows = AssetSettingsRows(Settings, {EAssetExportState::Running, EAssetExportKind::Music, 3, 78, 2});
	CHECK(Rows[AssetRow::Dump].bReadOnly);
	CHECK(Rows[AssetRow::Music].Label == "Cancel export");
	CHECK(Rows[AssetRow::Status].Value == "WAV 3/78");
	Rows = AssetSettingsRows(Settings, {EAssetExportState::Running, EAssetExportKind::Textures, 221, 884, 3});
	CHECK(Rows[AssetRow::Textures].Label == "Cancel export");
	CHECK(Rows[AssetRow::Status].Value == "25%");
	Rows = AssetSettingsRows(Settings, {EAssetExportState::Running, EAssetExportKind::Text, 371, 741, 4});
	CHECK(Rows[AssetRow::Text].Label == "Cancel export");
	CHECK(Rows[AssetRow::Textures].bReadOnly);
	CHECK(Rows[AssetRow::Status].Value == "50%");
}

TEST_CASE("The Assets page shows or hides each kind of texture pack picture", "[Menu][Assets]")
{
	FPlayerSettings Settings;
	CHECK(Settings.PackKinds.Shown() == 0x7F);
	const auto Rows = AssetSettingsRows(Settings, {});
	for (int32 Kind = 0; Kind < AssetRow::PackKinds; ++Kind)
	{
		const FSettingsRow& Row = Rows[static_cast<std::size_t>(AssetRow::FirstPackKind + Kind)];
		CHECK(Row.Label == OptionsText::Assets::PackKinds[static_cast<std::size_t>(Kind)].Label);
		CHECK(Row.Value == "On");
	}
	bool* Text = AssetRowPackKind(Settings.PackKinds, AssetRow::FirstPackKind);
	REQUIRE(Text == &Settings.PackKinds.bText);
	*Text = false;
	CHECK(AssetSettingsRows(Settings, {})[AssetRow::FirstPackKind].Value == "Off");
	// Other is always shown: a pack's pictures of uploads told apart from none.
	CHECK(Settings.PackKinds.Shown() == (0x7F & ~(1u << static_cast<uint32>(EVramUploadKind::Text))));
	CHECK(AssetRowPackKind(Settings.PackKinds, AssetRow::FirstPackKind + AssetRow::PackKinds - 1) == &Settings.PackKinds.bModelsAndEffects);
	CHECK(AssetRowPackKind(Settings.PackKinds, AssetRow::Capture) == nullptr);
	CHECK(AssetRowPackKind(Settings.PackKinds, AssetRow::Textures) == nullptr);
}

TEST_CASE("Asset page words fit the game's font and menu strips", "[Menu][Assets][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
	const auto Width = [&Text](std::string_view Words)
	{
		int32 Units = 0;
		for (char Character : Words)
		{
			const auto Code = MesCodeOfCharacter(Character);
			REQUIRE(Code);
			Units += Text.Font.WidthUnits(*Code);
		}
		return Units * 4;
	};
	for (int32 State = 0; State <= static_cast<int32>(EAssetExportState::Unavailable); ++State)
	{
		for (const auto& Row : AssetSettingsRows({}, {static_cast<EAssetExportState>(State), EAssetExportKind::Music, 99, 100, 1}))
		{
			INFO(Row.Label);
			CHECK(Width(Row.Label) + 8 + Width(Row.Value) <= 256);
			for (const auto& Line : Row.Help) { CHECK(Width(Line) <= 244); }
		}
	}
	// The Modding page's rows that open this page and the mod's, and the heading over them.
	CHECK(Width(OptionsText::Modding::Assets.Label) + 8 <= 256);
	CHECK(Width(OptionsText::Modding::Mods) <= 256);
	for (const std::string_view Line : OptionsText::Modding::Assets.Help) { CHECK(Width(Line) <= 244); }
	CHECK(Width(OptionsText::Modding::PerfectWorks.Label) + 8 + Width(OptionsText::Off) <= 256);
	for (const std::string& Line : OptionsText::Fill(OptionsText::Modding::PerfectWorks.Help, {"0.11.10"})) { CHECK(Width(Line) <= 244); }
	CHECK(Width(OptionsText::Modding::PerfectWorksUnversioned) <= 244);
}

TEST_CASE("Voice language choices cycle independently of sound output and persist", "[Menu][Assets][Settings]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	const FSettingsContext Both{{}, {true, true, false, false}};
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, VoicesRow, -1, Settings, bCustomPicked, Both));
	CHECK(Settings.VoiceLanguage == EVoiceLanguage::Japanese);
	CHECK(Settings.SoundOutput == ESoundOutput::Stereo);
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, VoicesRow, Settings, false, Both).Value == "Japanese");
	Settings.bExternalAssets = true;
	const auto Read = ReadPlayerSettings(WritePlayerSettings(Settings));
	CHECK(Read.VoiceLanguage == EVoiceLanguage::Japanese);
	CHECK(Read.bExternalAssets);
	const auto Invalid = ReadPlayerSettings("sound.voices = invalid\nassets.external = invalid\n");
	CHECK(Invalid.VoiceLanguage == EVoiceLanguage::Automatic);
	CHECK_FALSE(Invalid.bExternalAssets);
}

TEST_CASE("Movie subtitles step through Auto, On and Off beside the voices and persist", "[Menu][Settings]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	const FSettingsContext Undub{{}, {true, true, false, false}};
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SubtitlesRow, Settings, false, Undub).Label == OptionsText::Sound::Subtitles.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SubtitlesRow, Settings, false, Undub).Value == "Auto");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SubtitlesRow, 1, Settings, bCustomPicked, Undub));
	CHECK(Settings.MovieSubtitles == EMovieSubtitles::On);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SubtitlesRow, 1, Settings, bCustomPicked, Undub));
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SubtitlesRow, Settings, false, Undub).Value == "Off");
	CHECK(Settings.VoiceLanguage == EVoiceLanguage::Automatic);
	CHECK(ReadPlayerSettings(WritePlayerSettings(Settings)).MovieSubtitles == EMovieSubtitles::Off);
	CHECK(ReadPlayerSettings("sound.subtitles = on\n").MovieSubtitles == EMovieSubtitles::On);
	CHECK(ReadPlayerSettings("sound.subtitles = invalid\n").MovieSubtitles == EMovieSubtitles::Automatic);
}

TEST_CASE("The volumes and the subtitles' source persist, and a value out of range keeps the default", "[Menu][Settings]")
{
	FPlayerSettings Settings;
	Settings.Volumes = {90, 40, 70, 0, 100};
	Settings.MovieSubtitleSource = EMovieSubtitleSource::PerfectWorks;
	const FPlayerSettings Read = ReadPlayerSettings(WritePlayerSettings(Settings));
	CHECK(Read.Volumes == Settings.Volumes);
	CHECK(Read.MovieSubtitleSource == EMovieSubtitleSource::PerfectWorks);
	const std::string Written = WritePlayerSettings(Settings);
	for (const std::string_view Key : {"sound.master_volume = 90", "sound.music_volume = 40", "sound.effects_volume = 70", "sound.voices_volume = 0",
			 "sound.movies_volume = 100", "sound.subtitles_from = perfect_works"})
	{
		CHECK(Written.find(Key) != std::string::npos);
	}
	CHECK(ReadPlayerSettings("sound.subtitles_from = undub\n").MovieSubtitleSource == EMovieSubtitleSource::Undub);
	const FPlayerSettings Invalid = ReadPlayerSettings("sound.music_volume = 101\nsound.subtitles_from = elsewhere\n");
	CHECK(Invalid.Volumes.Music == FSoundVolumes::Full);
	CHECK(Invalid.MovieSubtitleSource == EMovieSubtitleSource::Automatic);
}
