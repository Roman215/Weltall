#include "Menu/MenuSettingsPages.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <set>
#include <sstream>

using namespace Weltall;

namespace
{
	constexpr std::array<ESettingsPage, 8> Pages = {ESettingsPage::Display, ESettingsPage::Sound, ESettingsPage::Camera, ESettingsPage::Fixes,
		ESettingsPage::Custom, ESettingsPage::Crt, ESettingsPage::Modding, ESettingsPage::PerfectWorks};
	namespace Text = OptionsText;

	/** The settings text's lines that name a fix, as "key = value". */
	std::set<std::string> FixLines(const FPlayerSettings& Settings)
	{
		std::set<std::string> Lines;
		std::istringstream Text(WritePlayerSettings(Settings));
		for (std::string Line; std::getline(Text, Line);)
		{
			if (Line.rfind("fix.", 0) == 0)
			{
				Lines.insert(Line);
			}
		}
		return Lines;
	}

	/** A display's sizes, the widest a row need show among them, and a window of none of them; every voice and subtitle given. */
	const FSettingsContext WideDisplay{{{{1280, 720}, {1920, 1080}, {3840, 2160}, {7680, 4320}}, {1600, 900}}, {true, true, true, true}};

	/** The Sound page's rows past its volumes: the output's, then the language's. */
	namespace SoundRow
	{
		constexpr int32 Output = 7;
		constexpr int32 InBackground = 8;
		constexpr int32 Voices = 10;
		constexpr int32 Subtitles = 11;
		constexpr int32 SubtitlesFrom = 12;
	}

	int32 CustomRowOfFix(int32 Fix)
	{
		int32 Seen = 0;
		for (int32 Row = 0; Row < FSettingsPages::RowCount(ESettingsPage::Custom); ++Row)
		{
			if (!FSettingsPages::Row(ESettingsPage::Custom, Row, {}).bHeading && Seen++ == Fix)
			{
				return Row;
			}
		}
		return -1;
	}
}

TEST_CASE("The fixes' list holds every fix the settings keep, each once", "[Menu]")
{
	const FPlayerSettings Defaults;
	const std::set<std::string> Before = FixLines(Defaults);
	REQUIRE(static_cast<int32>(Before.size()) == FSettingsPages::FixCount());
	std::set<std::string> Changed;
	for (int32 Fix = 0; Fix < FSettingsPages::FixCount(); ++Fix)
	{
		FPlayerSettings Settings;
		bool bCustomPicked = false;
		const int32 Row = CustomRowOfFix(Fix);
		REQUIRE(Row >= 0);
		REQUIRE(FSettingsPages::Step(ESettingsPage::Custom, Row, 1, Settings, bCustomPicked));
		// One line of the text changes, and no other fix's.
		const std::set<std::string> After = FixLines(Settings);
		std::vector<std::string> Different;
		for (const std::string& Line : After)
		{
			if (!Before.contains(Line))
			{
				Different.push_back(Line.substr(0, Line.find(' ')));
			}
		}
		REQUIRE(Different.size() == 1);
		Changed.insert(Different.front());
	}
	CHECK(static_cast<int32>(Changed.size()) == FSettingsPages::FixCount());
}

TEST_CASE("The fixes' preset is read from the fixes and sets them", "[Menu]")
{
	FPlayerSettings Settings;
	CHECK(FSettingsPages::Preset(Settings.Fixes) == EFixesPreset::PortEnhancements);
	CHECK(FSettingsPages::Row(ESettingsPage::Fixes, 0, Settings).Value == Text::Fixes::PresetValues[0]);

	// Right: Original, every fix off.
	bool bCustomPicked = false;
	REQUIRE(FSettingsPages::Step(ESettingsPage::Fixes, 0, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Preset(Settings.Fixes) == EFixesPreset::Original);
	for (const std::string& Line : FixLines(Settings))
	{
		CHECK(Line.ends_with("= false"));
	}
	CHECK(FSettingsPages::Activate(ESettingsPage::Fixes, 0, Settings, bCustomPicked) == ESettingsAction::None);

	// Right again: Custom keeps the fixes as they were and opens their list.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Fixes, 0, 1, Settings, bCustomPicked));
	CHECK(bCustomPicked);
	CHECK(FSettingsPages::Preset(Settings.Fixes) == EFixesPreset::Original);
	CHECK(FSettingsPages::Row(ESettingsPage::Fixes, 0, Settings, bCustomPicked).Value == Text::Fixes::PresetValues[2]);
	CHECK(FSettingsPages::Activate(ESettingsPage::Fixes, 0, Settings, bCustomPicked) == ESettingsAction::OpenCustom);

	// And round to Port Enhancements: every fix as a new player's, the gear shop's pace among them off.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Fixes, 0, 1, Settings, bCustomPicked));
	CHECK_FALSE(bCustomPicked);
	CHECK(FixLines(Settings) == FixLines(FPlayerSettings{}));
	CHECK_FALSE(Settings.Fixes.Menu.bGearModelFieldPace);

	// One fix changed in the list: Custom, without picking it.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Custom, CustomRowOfFix(0), 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Preset(Settings.Fixes) == EFixesPreset::Custom);
	CHECK(FSettingsPages::Row(ESettingsPage::Fixes, 0, Settings).Value == Text::Fixes::PresetValues[2]);
	CHECK(FSettingsPages::Activate(ESettingsPage::Fixes, 0, Settings, false) == ESettingsAction::OpenCustom);
	// Left from Custom: Original.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Fixes, 0, -1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Preset(Settings.Fixes) == EFixesPreset::Original);
}

TEST_CASE("The fixes' list shows each fix on or as the original, under the headings of where it shows", "[Menu]")
{
	FPlayerSettings Settings;
	const FSettingsRow First = FSettingsPages::Row(ESettingsPage::Custom, 0, Settings);
	CHECK(First.bHeading);
	CHECK(First.Label == Text::FixGroups::Field);
	CHECK(FSettingsPages::Row(ESettingsPage::Custom, 1, Settings).Label == Text::Fix::HpRecovery.Label);
	bool bCustomPicked = false;
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Custom, 0, 1, Settings, bCustomPicked));
	const int32 Row = CustomRowOfFix(0);
	CHECK(FSettingsPages::Row(ESettingsPage::Custom, Row, Settings).Value == Text::Fixes::FixOn);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Custom, Row, -1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Custom, Row, Settings).Value == Text::Fixes::FixOff);
	CHECK_FALSE(Settings.Fixes.Field.bRecoverPartyHpGivesHp);
	int32 Headings = 0;
	for (int32 At = 0; At < FSettingsPages::RowCount(ESettingsPage::Custom); ++At)
	{
		Headings += FSettingsPages::Row(ESettingsPage::Custom, At, Settings).bHeading ? 1 : 0;
	}
	CHECK(Headings == 7);
	CHECK(FSettingsPages::RowCount(ESettingsPage::Custom) == FSettingsPages::FixCount() + Headings);
}

TEST_CASE("The Modding page's balance rows show the original first and step through every rate", "[Menu][Balance]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	REQUIRE(FSettingsPages::RowCount(ESettingsPage::Modding) == 4);
	const FSettingsRow Heading = FSettingsPages::Row(ESettingsPage::Modding, 0, Settings);
	CHECK(Heading.bHeading);
	CHECK(Heading.Label == Text::Modding::Balance);
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Modding, 0, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 1, Settings).Label == Text::Modding::Experience.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 2, Settings).Label == Text::Modding::Gold.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 3, Settings).Label == Text::Modding::Encounters.Label);
	for (const int32 Row : {1, 2, 3})
	{
		CHECK(FSettingsPages::Row(ESettingsPage::Modding, Row, Settings).Value == "Normal");
	}

	// Experience: Normal, then up to 4x and round; Left from Normal is 4x.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 1, 1, Settings, bCustomPicked));
	CHECK(Settings.Balance.Experience == ERewardRate::OneAndAHalf);
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 1, Settings).Value == "1.5x");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 1, -1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 1, -1, Settings, bCustomPicked));
	CHECK(Settings.Balance.Experience == ERewardRate::Quadruple);
	CHECK(Settings.Balance.Gold == ERewardRate::Original);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 2, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 2, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 2, Settings).Value == "2x");

	// Encounters: Right from Normal doubles them, Left from Normal halves them, then a quarter, then none.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 3, 1, Settings, bCustomPicked));
	CHECK(Settings.Balance.Encounters == EEncounterRate::Double);
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 3, Settings).Value == "2x");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 3, -1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 3, -1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 3, Settings).Value == "1/2");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 3, -1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, 3, -1, Settings, bCustomPicked));
	CHECK(Settings.Balance.Encounters == EEncounterRate::None);
	CHECK(FSettingsPages::Row(ESettingsPage::Modding, 3, Settings).Value == "None");
	CHECK(FSettingsPages::Activate(ESettingsPage::Modding, 3, Settings, false) == ESettingsAction::None);
	CHECK(FSettingsPages::Title(ESettingsPage::Modding) == Text::Titles::Modding);
}

TEST_CASE("The Perfect Works page says when its changes apply, shows the mod's parts under headings and switches each", "[Menu][Mods]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	constexpr ESettingsPage Page = ESettingsPage::PerfectWorks;
	CHECK(FSettingsPages::Title(Page) == Text::Titles::PerfectWorks);
	REQUIRE(FSettingsPages::RowCount(Page) == 15);
	const auto Value = [&](int32 Row) { return FSettingsPages::Row(Page, Row, Settings).Value; };
	const FSettingsRow Notice = FSettingsPages::Row(Page, 0, Settings);
	CHECK(Notice.bReadOnly);
	CHECK(Notice.Label == Text::PerfectWorks::Notice);
	CHECK(FSettingsPages::Row(Page, 1, Settings).Label == Text::PerfectWorks::Enabled.Label);
	CHECK(Value(1) == "Off");
	CHECK(FSettingsPages::Row(Page, 2, Settings).bHeading);
	CHECK(FSettingsPages::Row(Page, 6, Settings).bHeading);
	CHECK(FSettingsPages::Row(Page, 10, Settings).bHeading);
	CHECK(Value(3) == "Perfect Works");
	CHECK(Value(4) == "Off");
	CHECK(Value(5) == "Perfect Works");
	CHECK(FSettingsPages::Row(Page, 7, Settings).Label == Text::PerfectWorks::Party.Label);
	CHECK(Value(7) == "Original");
	CHECK(Value(8) == "Original");
	CHECK(FSettingsPages::Row(Page, 9, Settings).Label == Text::PerfectWorks::Arena.Label);
	CHECK(Value(9) == "Original");
	CHECK(Value(11) == "Fixed");
	CHECK(Value(12) == "Original");
	CHECK(Value(13) == "Fixed");
	CHECK(Value(14) == "Perfect Works");

	CHECK_FALSE(FSettingsPages::Step(Page, 0, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(Page, 1, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.bEnabled);
	REQUIRE(FSettingsPages::Step(Page, 3, -1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.PerfectWorks.bScript);
	CHECK(Value(3) == "Original");
	REQUIRE(FSettingsPages::Step(Page, 11, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.Portraits == EPerfectWorksPortraits::Resized);
	REQUIRE(FSettingsPages::Step(Page, 11, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.Portraits == EPerfectWorksPortraits::Original);
	REQUIRE(FSettingsPages::Step(Page, 12, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.bRoniPortrait);
	REQUIRE(FSettingsPages::Step(Page, 13, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.PerfectWorks.bEmeraldaCafe);
	CHECK(Value(13) == "Original");
	CHECK_FALSE(FSettingsPages::Step(Page, 2, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Activate(Page, 3, Settings, false) == ESettingsAction::None);
	// The arena's modes step round: Original, Basic, Expert.
	REQUIRE(FSettingsPages::Step(Page, 9, 1, Settings, bCustomPicked));
	CHECK(Value(9) == "Basic");
	REQUIRE(FSettingsPages::Step(Page, 9, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.Arena == EPerfectWorksArena::Expert);
	REQUIRE(FSettingsPages::Step(Page, 9, 1, Settings, bCustomPicked));
	CHECK(Value(9) == "Original");

	// Story mode and the rebalances (the arena's too) exclude each other, as the mod's patcher has them: the one chosen last stays.
	REQUIRE(FSettingsPages::Step(Page, 7, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(Page, 8, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(Page, 9, -1, Settings, bCustomPicked));
	CHECK(Value(7) == "Perfect Works");
	CHECK(Value(8) == "Perfect Works");
	CHECK(Value(9) == "Expert");
	REQUIRE(FSettingsPages::Step(Page, 4, 1, Settings, bCustomPicked));
	CHECK(Settings.PerfectWorks.bStoryMode);
	CHECK_FALSE(Settings.PerfectWorks.bRebalancedParty);
	CHECK_FALSE(Settings.PerfectWorks.bRebalancedEnemies);
	CHECK(Settings.PerfectWorks.Arena == EPerfectWorksArena::Original);
	REQUIRE(FSettingsPages::Step(Page, 8, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.PerfectWorks.bStoryMode);
	CHECK(Settings.PerfectWorks.bRebalancedEnemies);
	REQUIRE(FSettingsPages::Step(Page, 4, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(Page, 9, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.PerfectWorks.bStoryMode);
	CHECK(Settings.PerfectWorks.Arena == EPerfectWorksArena::Basic);
}

TEST_CASE("The options' choices wrap, and their numbers step within their ranges", "[Menu]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SoundRow::Output, Settings).Value == Text::Sound::OutputValues[0]);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Output, -1, Settings, bCustomPicked));
	CHECK(Settings.SoundOutput == ESoundOutput::Mono);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Output, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Output, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SoundRow::Output, Settings).Value == "Wide-Stereo");
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SoundRow::Output, Settings).Label == Text::Sound::Output.Label);

	// The scenery's and the sprites' filters, each its own: Sharp, Crisp, Smooth and the upscaler, round to Sharp.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 2, Settings).Label == Text::Display::SceneryFilter.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 2, Settings).Value == "Sharp");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 2, -1, Settings, bCustomPicked));
	CHECK(Settings.Field.Filter == EFieldTextureFilter::Upscaled);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 2, Settings).Value == "Upscaled");
	CHECK(Settings.Field.SpriteFilter == EFieldTextureFilter::Nearest);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 2, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.Filter == EFieldTextureFilter::Nearest);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 3, Settings).Label == Text::Display::SpriteFilter.Label);
	for (int32 Step = 0; Step < 3; ++Step)
	{
		REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 3, 1, Settings, bCustomPicked));
	}
	CHECK(Settings.Field.SpriteFilter == EFieldTextureFilter::Upscaled);
	CHECK(Settings.Field.Filter == EFieldTextureFilter::Nearest);
	Settings.Field.SpriteFilter = EFieldTextureFilter::Nearest;

	// The movies: today's filter, then the upscaler, round to today's.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 4, Settings).Label == Text::Display::MovieFilter.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 4, Settings).Value == "Smooth");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 4, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.MovieFilter == EMovieFilter::Upscaled);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 4, Settings).Value == "Upscaled");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 4, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.MovieFilter == EMovieFilter::Smooth);

	// The CRT filter: off at first, then the monitor's, the television's and the player's own, round to off.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 5, Settings).Label == Text::Display::Crt.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 5, Settings).Value == "Off");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 5, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.Crt == ECrtFilter::Monitor);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 5, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 5, Settings).Value == "TV");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 5, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 5, Settings).Value == "Custom");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 5, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.Crt == ECrtFilter::Off);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 5, -1, Settings, bCustomPicked));
	CHECK(Settings.Display.Crt == ECrtFilter::Custom);
	Settings.Display.Crt = ECrtFilter::Off;

	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 6, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.AntiAliasing == EFieldAntiAliasing::Supersampling);

	// The pilots in gears: the members' own sprites, or the map's.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 8, Settings).Label == Text::Display::Pilots.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 8, Settings).Value == "Detailed");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 8, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.Field.bDetailedPilots);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 8, Settings).Value == "Original");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 8, -1, Settings, bCustomPicked));
	CHECK(Settings.Field.bDetailedPilots);

	// The gears the field draws as sprites: their battle models but in cutscenes (the default), their models always, their sprites.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 9, Settings).Label == Text::Display::Gears.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 9, Settings).Value == "3D in play");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 9, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.GearModels == EFieldGearModels::Everywhere);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 9, Settings).Value == "3D always");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 9, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.GearModels == EFieldGearModels::Off);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 9, Settings).Value == "Original");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 9, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.GearModels == EFieldGearModels::OutsideCutscenes);

	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 10, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.bChapterCards);

	// The text speed: twice the original's at first; the original's, then twice, four times and a page at once, round
	// to the original.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 11, Settings).Value == "2x");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 11, -1, Settings, bCustomPicked));
	CHECK(Settings.TextSpeed == EMesTextSpeed::Original);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 11, Settings).Value == Text::Display::TextSpeedValues[0]);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 11, -1, Settings, bCustomPicked));
	CHECK(Settings.TextSpeed == EMesTextSpeed::Instant);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 11, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 11, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 11, Settings).Value == "2x");

	// The battle speed: the game's own, then twice and four times it, round to the game's own.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 12, Settings).Label == Text::Display::BattleSpeed.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 12, Settings).Value == Text::Display::BattleSpeedValues[0]);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 12, 1, Settings, bCustomPicked));
	CHECK(Settings.BattleSpeed == EBattleSpeed::Double);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 12, Settings).Value == "2x");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 12, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 12, Settings).Value == "4x");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 12, 1, Settings, bCustomPicked));
	CHECK(Settings.BattleSpeed == EBattleSpeed::Normal);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 12, -1, Settings, bCustomPicked));
	CHECK(Settings.BattleSpeed == EBattleSpeed::Quadruple);

	// The battle effects' flashes: the original's, then softer and none, round to the original's.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 13, Settings).Label == Text::Display::BattleFlashes.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 13, Settings).Value == "Original");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 13, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.BattleFlashes == EBattleFlashes::Softer);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 13, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 13, Settings).Value == "Off");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 13, 1, Settings, bCustomPicked));
	CHECK(Settings.Field.BattleFlashes == EBattleFlashes::Original);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 13, -1, Settings, bCustomPicked));
	CHECK(Settings.Field.BattleFlashes == EBattleFlashes::Off);

	// In the background the game stops and is silent, unless set otherwise.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 14, Settings).Value == Text::On);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 14, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.Display.bPauseInBackground);
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, SoundRow::InBackground, Settings).Value == Text::Off);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::InBackground, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.bSoundInBackground);
	CHECK(Settings.SoundOutput == ESoundOutput::WideStereo);

	// The picture's shape: the window's, then 16:9 and 4:3.
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings).Value == Text::Display::AspectValues[0]);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 0, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.Aspect == EDisplayAspect::Wide);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings).Value == "16:9");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 0, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings).Value == "4:3");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 0, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.Aspect == EDisplayAspect::Window);

	// The zoom from 125% by fives, to 200% and no further; a zoom between steps goes to the step on the side asked for.
	CHECK(FSettingsPages::Row(ESettingsPage::Camera, 0, Settings).Value == "125%");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Camera, 0, 1, Settings, bCustomPicked));
	CHECK(Settings.Camera.Zoom == Catch::Approx(1.3f));
	Settings.Camera.Zoom = 2.0f;
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Camera, 0, 1, Settings, bCustomPicked));
	Settings.Camera.Zoom = 1.23f;
	REQUIRE(FSettingsPages::Step(ESettingsPage::Camera, 0, -1, Settings, bCustomPicked));
	CHECK(Settings.Camera.Zoom == Catch::Approx(1.2f));
	Settings.Camera.Zoom = 1.23f;
	REQUIRE(FSettingsPages::Step(ESettingsPage::Camera, 0, 1, Settings, bCustomPicked));
	CHECK(Settings.Camera.Zoom == Catch::Approx(1.25f));

	// The tilt by degrees, to +20; shown signed.
	Settings.Camera.Tilt = 19.0f;
	REQUIRE(FSettingsPages::Step(ESettingsPage::Camera, 1, 1, Settings, bCustomPicked));
	CHECK(FSettingsPages::Row(ESettingsPage::Camera, 1, Settings).Value == "+20");
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Camera, 1, 1, Settings, bCustomPicked));

	// The reset: the framing a player starts with.
	CHECK(FSettingsPages::Row(ESettingsPage::Camera, 3, Settings).Value == Text::Camera::FollowValues[0]);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Camera, 3, 1, Settings, bCustomPicked));
	CHECK_FALSE(Settings.Camera.bSteadyFollow);
	CHECK(FSettingsPages::Row(ESettingsPage::Camera, 3, Settings).Value == Text::Camera::FollowValues[1]);
	CHECK(FSettingsPages::Activate(ESettingsPage::Camera, 4, Settings, false) == ESettingsAction::Reset);
	CHECK(Settings.Camera == FFieldCameraOffsets::Defaults());
	CHECK(FSettingsPages::Activate(ESettingsPage::Camera, 0, Settings, false) == ESettingsAction::None);
}

TEST_CASE("The Sound page's volumes step by ten within 0 and 100, each of its own", "[Menu][Sound]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	CHECK(FSettingsPages::Row(ESettingsPage::Sound, 0, Settings).bHeading);
	const std::array<std::pair<std::string_view, int32 FSoundVolumes::*>, 5> Volumes{{{Text::Sound::Master.Label, &FSoundVolumes::Master},
		{Text::Sound::Music.Label, &FSoundVolumes::Music}, {Text::Sound::Effects.Label, &FSoundVolumes::Effects},
		{Text::Sound::VoiceVolume.Label, &FSoundVolumes::Voices}, {Text::Sound::Movies.Label, &FSoundVolumes::Movies}}};
	for (std::size_t Index = 0; Index < Volumes.size(); ++Index)
	{
		const int32 Row = static_cast<int32>(Index) + 1;
		const auto [Label, Volume] = Volumes[Index];
		INFO(Label);
		CHECK(FSettingsPages::Row(ESettingsPage::Sound, Row, Settings).Label == Label);
		CHECK(FSettingsPages::Row(ESettingsPage::Sound, Row, Settings).Value == "100%");
		CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, Row, 1, Settings, bCustomPicked));
		REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, Row, -1, Settings, bCustomPicked));
		CHECK(Settings.Volumes.*Volume == 90);
		CHECK(FSettingsPages::Row(ESettingsPage::Sound, Row, Settings).Value == "90%");
		// One set between the steps (in the settings file) goes to the step past it either way.
		Settings.Volumes.*Volume = 55;
		REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, Row, 1, Settings, bCustomPicked));
		CHECK(Settings.Volumes.*Volume == 60);
		Settings.Volumes.*Volume = 55;
		REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, Row, -1, Settings, bCustomPicked));
		CHECK(Settings.Volumes.*Volume == 50);
		Settings.Volumes.*Volume = 10;
		REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, Row, -1, Settings, bCustomPicked));
		CHECK(Settings.Volumes.*Volume == 0);
		CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, Row, -1, Settings, bCustomPicked));
	}
	// Each volume is its own: the others stayed at 0 after their own rows.
	CHECK(Settings.Volumes == FSoundVolumes{0, 0, 0, 0, 0});
}

TEST_CASE("The Sound page offers the voices and subtitles the game was given, and says what is missing", "[Menu][Sound]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	const auto Context = [](bool bEnglish, bool bUndub, bool bPerfectWorks)
	{
		return FSettingsContext{{}, {bEnglish, bUndub, bPerfectWorks, false}};
	};
	const auto Row = [&Settings](int32 At, const FSettingsContext& Given) { return FSettingsPages::Row(ESettingsPage::Sound, At, Settings, false, Given); };

	// The USA discs alone: English voices only, no subtitles, so no choice of whose. Rows with nothing to choose say why,
	// where the cursor still stops.
	const FSettingsContext Usa = Context(true, false, false);
	CHECK(Row(SoundRow::Voices, Usa).Value == "English");
	CHECK(Row(SoundRow::Voices, Usa).Help[0] == Text::Sound::OnlyEnglish[0]);
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Voices, 1, Settings, bCustomPicked, Usa));
	CHECK(Row(SoundRow::Subtitles, Usa).Value == Text::Sound::NoSubtitles);
	CHECK(Row(SoundRow::Subtitles, Usa).Help[0] == Text::Sound::NoSubtitlesHelp[0]);
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Subtitles, 1, Settings, bCustomPicked, Usa));
	CHECK(FSettingsPages::RowCount(ESettingsPage::Sound, Usa) == SoundRow::SubtitlesFrom);
	// Where discs come in through the device's file picker, the help names the Saves page rather than the folders.
	FSettingsContext Picked = Usa;
	Picked.bDiscsPicked = true;
	CHECK(Row(SoundRow::Voices, Picked).Help == OptionsText::Fill(Text::Sound::OnlyEnglishPicked));
	CHECK(Row(SoundRow::Subtitles, Picked).Help == OptionsText::Fill(Text::Sound::NoSubtitlesPicked));
	CHECK(Settings.VoiceLanguage == EVoiceLanguage::Automatic);
	CHECK(Settings.MovieSubtitles == EMovieSubtitles::Automatic);

	// The undub alone: Japanese voices only, and its subtitles.
	const FSettingsContext Undub = Context(false, true, false);
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Voices, 1, Settings, bCustomPicked, Undub));
	CHECK(Row(SoundRow::Voices, Undub).Value == "Japanese");
	CHECK(Row(SoundRow::Voices, Undub).Help[0] == Text::Sound::OnlyJapanese[0]);
	CHECK(Row(SoundRow::Subtitles, Undub).Help[0] == Text::Sound::Subtitles.Help[0]);
	CHECK(Row(SoundRow::SubtitlesFrom, Undub).Value == "Undub");
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::SubtitlesFrom, 1, Settings, bCustomPicked, Undub));

	// The USA discs and Perfect Works: its subtitles shown always on On, Auto waiting for Japanese voices there are none of.
	const FSettingsContext Mod = Context(true, false, true);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Subtitles, 1, Settings, bCustomPicked, Mod));
	CHECK(Settings.MovieSubtitles == EMovieSubtitles::On);
	CHECK(Row(SoundRow::Subtitles, Mod).Help[0] == Text::Sound::SubtitlesWithoutUndub[0]);
	CHECK(Row(SoundRow::SubtitlesFrom, Mod).Value == "Perfect Works");
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::SubtitlesFrom, 1, Settings, bCustomPicked, Mod));

	// Both languages and both subtitles: every choice.
	const FSettingsContext Every = Context(true, true, true);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::Voices, 1, Settings, bCustomPicked, Every));
	CHECK(Settings.VoiceLanguage == EVoiceLanguage::English);
	CHECK(Row(SoundRow::SubtitlesFrom, Every).Value == "Auto");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::SubtitlesFrom, 1, Settings, bCustomPicked, Every));
	CHECK(Row(SoundRow::SubtitlesFrom, Every).Value == "Undub");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Sound, SoundRow::SubtitlesFrom, 1, Settings, bCustomPicked, Every));
	CHECK(Settings.MovieSubtitleSource == EMovieSubtitleSource::PerfectWorks);
	CHECK(FSettingsPages::RowCount(ESettingsPage::Sound, Every) == SoundRow::SubtitlesFrom + 1);
}

TEST_CASE("The CRT filter's parts show the look in use, and changing one makes it the player's own", "[Menu]")
{
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	// Confirm on the CRT filter's row opens its parts, wherever the window's rows put it.
	CHECK(FSettingsPages::Activate(ESettingsPage::Display, 5, Settings, false) == ESettingsAction::OpenCrt);
	CHECK(FSettingsPages::Activate(ESettingsPage::Display, 7, Settings, false, WideDisplay) == ESettingsAction::OpenCrt);
	CHECK(FSettingsPages::Activate(ESettingsPage::Display, 4, Settings, false) == ESettingsAction::None);
	REQUIRE(FSettingsPages::RowCount(ESettingsPage::Crt) == 10);

	// The television's look, part by part.
	Settings.Display.Crt = ECrtFilter::Television;
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 0, Settings).Label == Text::Crt::Scanlines.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 0, Settings).Value == "8");
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 1, Settings).Value == "Slots");
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 7, Settings).Label == Text::Crt::Curvature.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 7, Settings).Value == "3");

	// Its glass a step rounder: the rest of the television's look kept, as the player's own.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Crt, 7, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.Crt == ECrtFilter::Custom);
	FCrtLook Rounder = FCrtLook::Television();
	Rounder.Curvature = 4;
	CHECK(Settings.Display.CustomCrt == Rounder);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 5, Settings).Value == "Custom");

	// A part's steps stop at their ends; the mask's kinds wrap.
	Settings.Display.CustomCrt.Scanlines = FCrtLook::Steps;
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Crt, 0, 1, Settings, bCustomPicked));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Crt, 1, 1, Settings, bCustomPicked));
	CHECK(Settings.Display.CustomCrt.Mask == ECrtMask::None);

	// A look chosen again shows its own parts; the player's stays for Custom.
	Settings.Display.Crt = ECrtFilter::Monitor;
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 1, Settings).Value == "Stripes");
	Settings.Display.Crt = ECrtFilter::Custom;
	CHECK(FSettingsPages::Row(ESettingsPage::Crt, 1, Settings).Value == "None");
}

TEST_CASE("The window's size is chosen among the display's, from the window's own", "[Menu]")
{
	// No window's rows where the display offers no sizes: the page begins with the picture's shape.
	CHECK(FSettingsPages::RowCount(ESettingsPage::Display, WideDisplay) == FSettingsPages::RowCount(ESettingsPage::Display) + 2);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, {}).Label == Text::Display::Aspect.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 2, {}, false, WideDisplay).Label == Text::Display::Aspect.Label);

	// Full screen, off at first, toggles.
	FPlayerSettings Settings;
	bool bCustomPicked = false;
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings, false, WideDisplay).Label == Text::Display::FullScreen.Label);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings, false, WideDisplay).Value == Text::Off);
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 0, 1, Settings, bCustomPicked, WideDisplay));
	CHECK(Settings.Display.bFullScreen);
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 0, Settings, false, WideDisplay).Value == Text::On);
	Settings.Display.bFullScreen = false;

	// Until one is chosen the size's row shows the window as it is, whichever size that is, and steps from it.
	const FSettingsRow Unchosen = FSettingsPages::Row(ESettingsPage::Display, 1, Settings, false, WideDisplay);
	CHECK(Unchosen.Label == Text::Display::Resolution.Label);
	CHECK(Unchosen.Value == "1600 x 900");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 1, 1, Settings, bCustomPicked, WideDisplay));
	CHECK(Settings.Display.WindowSize == FDisplaySize{1920, 1080});
	Settings.Display.WindowSize = {};
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 1, -1, Settings, bCustomPicked, WideDisplay));
	CHECK(Settings.Display.WindowSize == FDisplaySize{1280, 720});

	// Past either end it goes round to the other.
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 1, -1, Settings, bCustomPicked, WideDisplay));
	CHECK(Settings.Display.WindowSize == FDisplaySize{7680, 4320});
	CHECK(FSettingsPages::Row(ESettingsPage::Display, 1, Settings, false, WideDisplay).Value == "7680 x 4320");
	REQUIRE(FSettingsPages::Step(ESettingsPage::Display, 1, 1, Settings, bCustomPicked, WideDisplay));
	CHECK(Settings.Display.WindowSize == FDisplaySize{1280, 720});

	// A display with the one size, already chosen, has nothing to change to.
	const FSettingsContext Single{{{{1280, 720}}, {1280, 720}}, {}};
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Display, 1, 1, Settings, bCustomPicked, Single));
	CHECK_FALSE(FSettingsPages::Step(ESettingsPage::Display, 1, -1, Settings, bCustomPicked, Single));
}

TEST_CASE("Every row's words are in the game's font and fit their places on the options screen", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Text = Testing::LoadMesResident(*Disc);
	// In pixels: a row's label from its place, a gap, then its value ending at the list's inner edge; a help line in
	// its strip's typing.
	constexpr int32 LabelX = 0x28;
	constexpr int32 ValueRight = 0x128;
	constexpr int32 Gap = 8;
	constexpr int32 HelpWidth = 0x3D * 4;
	const auto Width = [&Text](std::string_view Words)
	{
		int32 Units = 0;
		for (const char Character : Words)
		{
			const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
			CHECK(Code.has_value());
			Units += Code ? Text.Font.WidthUnits(*Code) : 0;
		}
		return Units * 4;
	};
	FPlayerSettings Settings;
	FPlayerSettings Original;
	FSettingsPages::ApplyPreset(EFixesPreset::Original, Original.Fixes);
	// Every page with everything given; the Sound page also with each set of voices and subtitles it words otherwise.
	std::vector<std::pair<ESettingsPage, FSettingsContext>> Shown;
	for (const ESettingsPage Page : Pages)
	{
		Shown.push_back({Page, WideDisplay});
	}
	for (const FSoundSources Sources : {FSoundSources{}, FSoundSources{true, false, false, false}, FSoundSources{false, true, false, false},
			 FSoundSources{true, false, true, false}})
	{
		Shown.push_back({ESettingsPage::Sound, {WideDisplay.Display, Sources}});
		Shown.push_back({ESettingsPage::Sound, {WideDisplay.Display, Sources, true}});
	}
	for (const auto& [Page, Context] : Shown)
	{
		INFO(FSettingsPages::Title(Page));
		CHECK(Width(FSettingsPages::Title(Page)) <= ValueRight - LabelX);
		for (int32 Row = 0; Row < FSettingsPages::RowCount(Page, Context); ++Row)
		{
			for (const FPlayerSettings* From : {&Settings, &Original})
			{
				// Each of the row's choices: four steps reach every one.
				FPlayerSettings Stepped = *From;
				for (int32 Step = 0; Step < 5; ++Step)
				{
					bool bCustomPicked = false;
					if (Step > 0 && !FSettingsPages::Step(Page, Row, 1, Stepped, bCustomPicked, Context))
					{
						break;
					}
					const FSettingsRow Words = FSettingsPages::Row(Page, Row, Stepped, bCustomPicked, Context);
					INFO(std::string(Words.Label) + " = " + Words.Value);
					CHECK(LabelX + Width(Words.Label) + Gap <= ValueRight - Width(Words.Value));
					for (const std::string_view Line : Words.Help)
					{
						CHECK(Width(Line) <= HelpWidth);
					}
				}
			}
		}
	}
}
