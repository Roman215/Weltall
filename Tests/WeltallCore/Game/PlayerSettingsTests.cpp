#include "Game/PlayerSettings.h"

#include <catch_amalgamated.hpp>

#include <string>

using namespace Weltall;

namespace
{
	/** The value a text gives a key: what follows "key = " on its line. */
	std::string ValueOf(const std::string& Text, const std::string& Key)
	{
		const std::size_t At = Text.find("\n" + Key + " =");
		REQUIRE(At != std::string::npos);
		const std::size_t Start = At + Key.size() + 3;
		const std::size_t End = Text.find('\n', Start);
		std::string Value = Text.substr(Start, End - Start);
		return Value.empty() ? Value : Value.substr(1);
	}

	FPlayerSettings Changed()
	{
		FPlayerSettings Settings;
		Settings.Fixes.Field = FEventOptions::Original();
		Settings.Fixes.Music.bStartOnlyForItsSong = false;
		Settings.Fixes.SoundDriver.bHighNotesStayHigh = false;
		Settings.Fixes.Load = FSaveLoadOptions::Original();
		Settings.Fixes.Menu = FMenuOptions::Original();
		Settings.Fixes.BattleRules = FBattleRuleOptions::Original();
		Settings.Field.Sorting = EFieldSorting::PaintersOrder;
		Settings.Field.Filter = EFieldTextureFilter::Bilinear;
		Settings.Field.SpriteFilter = EFieldTextureFilter::Upscaled;
		Settings.Field.AntiAliasing = EFieldAntiAliasing::Supersampling;
		Settings.Field.Culling = EFieldCulling::Original;
		Settings.Field.MovieFilter = EMovieFilter::Upscaled;
		Settings.Field.bDetailedPilots = false;
		Settings.Field.GearModels = EFieldGearModels::Everywhere;
		Settings.Field.BattleFlashes = EBattleFlashes::Softer;
		Settings.SoundOutput = ESoundOutput::WideStereo;
		Settings.VoiceLanguage = EVoiceLanguage::Japanese;
		Settings.MovieSubtitles = EMovieSubtitles::On;
		Settings.bExternalAssets = true;
		Settings.bCaptureTextures = true;
		Settings.Display = {{2560, 1440}, EDisplayAspect::Wide};
		Settings.Display.Crt = ECrtFilter::Custom;
		Settings.Display.CustomCrt = FCrtLook::Television();
		Settings.Display.CustomCrt.Glow = 10;
		Settings.Camera = {1.5f, -7.25f, 135.0f};
		Settings.WorldMapCamera = {0.75f, 12.5f, -90.0f};
		Settings.Controls.Bind(EControlAction::Cross, 0, "SpaceBar");
		Settings.Controls.Bind(EControlAction::ViewTilt, 0, "-Gamepad_RightY");
		Settings.Controls.Unbind("MiddleMouseButton");
		Settings.Controls.MouseSensitivity = 8;
		Settings.Controls.StickSensitivity = 2;
		Settings.Controls.bOriginalButtonIcons = true;
		Settings.Controls.ChangeButtonLayout(EButtonLayout::Japan);
		Settings.TextSpeed = EMesTextSpeed::Quadruple;
		Settings.BattleSpeed = EBattleSpeed::Quadruple;
		Settings.Balance = {ERewardRate::OneAndAHalf, ERewardRate::Quadruple, EEncounterRate::Quarter};
		Settings.PerfectWorks = {true, false, EPerfectWorksPortraits::Resized, true, false, false, true, false, true, true};
		return Settings;
	}

	void CheckSame(const FPlayerSettings& A, const FPlayerSettings& B)
	{
		CHECK(WritePlayerSettings(A) == WritePlayerSettings(B));
		CHECK(A.Controls == B.Controls);
		CHECK(A.Camera == B.Camera);
		CHECK(A.WorldMapCamera == B.WorldMapCamera);
		CHECK(A.Field == B.Field);
		CHECK(A.Display == B.Display);
		CHECK(A.SoundOutput == B.SoundOutput);
		CHECK(A.VoiceLanguage == B.VoiceLanguage);
		CHECK(A.MovieSubtitles == B.MovieSubtitles);
		CHECK(A.bExternalAssets == B.bExternalAssets);
		CHECK(A.bCaptureTextures == B.bCaptureTextures);
		CHECK(A.TextSpeed == B.TextSpeed);
		CHECK(A.BattleSpeed == B.BattleSpeed);
		CHECK(A.Balance == B.Balance);
		CHECK(A.PerfectWorks == B.PerfectWorks);
	}
}

TEST_CASE("The settings' defaults are the fixes on and the sharp presentation", "[Settings]")
{
	const FPlayerSettings Settings;
	CHECK(Settings.Fixes.Field.bRecoverPartyHpGivesHp);
	CHECK(Settings.Fixes.Field.bZeroFrameFadeSnaps);
	CHECK(Settings.Fixes.Field.bFadesRiseFromBlack);
	CHECK(Settings.Fixes.Field.bChangeMapWaitsWhole);
	CHECK(Settings.Fixes.Field.bGearChannelPoolGrows);
	CHECK(Settings.Fixes.Field.bScrollsWrapWithinWidth);
	CHECK(Settings.Fixes.Field.bInitTurnSetsTurningYaw);
	CHECK(Settings.Fixes.Field.bShakeDepthFromItsOwn);
	CHECK(Settings.Fixes.Field.bLadderOnlyWithItsAnimation);
	CHECK(Settings.Fixes.Field.bHotelCountsWholeParty);
	CHECK(Settings.Fixes.Field.bActorsRunInWideView);
	CHECK(Settings.Fixes.Field.bPartySpritesFollowTheirFile);
	CHECK(Settings.Fixes.Field.bRpsHelper);
	CHECK(Settings.Fixes.Music.bStartOnlyForItsSong);
	CHECK(Settings.Fixes.SoundDriver.bPanSlidesEndOnTarget);
	CHECK(Settings.Fixes.SoundDriver.bHighNotesStayHigh);
	CHECK(Settings.Fixes.Load.bPenaltyFromSavedTolerance);
	CHECK(Settings.Fixes.Load.bDeathblowsFromGearInUse);
	CHECK(Settings.Fixes.Menu.bSaleRecomputesEquipment);
	CHECK(Settings.Fixes.Menu.bNewGameClockFromZero);
	CHECK_FALSE(Settings.Fixes.Menu.bGearModelFieldPace);
	CHECK(Settings.Fixes.BattleRules.bInfinityByDamageAlone);
	CHECK(Settings.Fixes.BattleRules.bItemBuffsByItsUser);
	CHECK(Settings.Fixes.BattleRules.bEpGrowsAtLastLevel);
	CHECK(Settings.Fixes.BattleRules.bEtherBuffsSetDirectly);
	CHECK(Settings.Fixes.BattleRules.bEveryBuffExpires);
	CHECK(Settings.Fixes.BattleRules.bDefUpOnFoot);
	CHECK(Settings.Fixes.BattleRules.bDefDownOnFoot);
	CHECK(Settings.Fixes.BattleRules.bFailedStatusKeepsCount);
	CHECK(Settings.Fixes.BattleRules.bImmunityItemsWork);
	CHECK(Settings.Fixes.BattleRules.bGearElementsExpire);
	CHECK(Settings.Fixes.BattleRules.bGearStopEnds);
	CHECK(Settings.Fixes.BattleRules.bRevealKeepsGroup);
	CHECK(Settings.Fixes.BattleRules.bGearPowerLoss);
	CHECK(Settings.Fixes.BattleRules.bWholeAmounts);
	CHECK(Settings.Fixes.BattleRules.bCounterKeepsGroupAttack);
	CHECK(Settings.Fixes.BattleRules.bDeusSecondFormHp);
	CHECK(Settings.Fixes.BattleRules.bDeusKeepsCamera);
	CHECK(Settings.Fixes.BattleRules.bEtherAmmunitionElement);
	CHECK(Settings.Fixes.Menu.bMemberChangeSaysEp);
	CHECK(Settings.Fixes.Menu.bShopPointerBesideWords);
	CHECK(Settings.Fixes.Field.bShipSoundsAtTheirVolume);
	CHECK(Settings.Fixes.Field.bMemoryAmbienceFades);
	CHECK(Settings.Fixes.BattleEvents.bWaitedStartNamesActor);
	CHECK(Settings.Fixes.WorldMap.bStoryObjectBackAfterBattle);
	CHECK(Settings.Field.Sorting == EFieldSorting::DepthBufferWithScenery);
	CHECK(Settings.Field.Filter == EFieldTextureFilter::Nearest);
	CHECK(Settings.Field.SpriteFilter == EFieldTextureFilter::Nearest);
	CHECK(Settings.Field.MovieFilter == EMovieFilter::Smooth);
	CHECK(Settings.Field.AntiAliasing == EFieldAntiAliasing::Msaa);
	CHECK(Settings.Field.Culling == EFieldCulling::InRange);
	CHECK(Settings.SoundOutput == ESoundOutput::Stereo);
	CHECK(Settings.VoiceLanguage == EVoiceLanguage::Automatic);
	CHECK(Settings.MovieSubtitles == EMovieSubtitles::Automatic);
	CHECK_FALSE(Settings.bExternalAssets);
	CHECK_FALSE(Settings.bCaptureTextures);
	CHECK(Settings.Camera == FFieldCameraOffsets::Defaults());
	CHECK(Settings.Camera.Zoom == 1.25f);
	CHECK(Settings.Controls == FControlBindings::Defaults());
	CHECK(Settings.Controls.MouseSensitivity == DefaultSensitivity);
	CHECK(Settings.Controls.StickSensitivity == DefaultSensitivity);
	CHECK(Settings.TextSpeed == EMesTextSpeed::Double);
	CHECK(Settings.BattleSpeed == EBattleSpeed::Normal);
}

TEST_CASE("The settings' text lists every setting and reads back the same", "[Settings]")
{
	const std::string Defaults = WritePlayerSettings({});
	CHECK(ValueOf(Defaults, "fix.recover_party_hp_heals_hp") == "true");
	CHECK(ValueOf(Defaults, "fix.gear_channel_pool_grows") == "true");
	CHECK(ValueOf(Defaults, "field.sorting") == "scenery_too");
	CHECK(ValueOf(Defaults, "field.scenery_filter") == "nearest");
	CHECK(ValueOf(Defaults, "field.sprite_filter") == "nearest");
	CHECK(ValueOf(Defaults, "field.culling") == "in_range");
	CHECK(ValueOf(Defaults, "field.anti_aliasing") == "msaa");
	CHECK(ValueOf(Defaults, "field.movie_filter") == "smooth");
	CHECK(ValueOf(Defaults, "field.detailed_pilots") == "true");
	CHECK(ValueOf(Defaults, "field.gear_models") == "outside_cutscenes");
	CHECK(ValueOf(Defaults, "field.battle_flashes") == "original");
	CHECK(ValueOf(Defaults, "sound.output") == "stereo");
	CHECK(ValueOf(Defaults, "display.window_size").empty());
	CHECK(ValueOf(Defaults, "display.aspect") == "window");
	CHECK(ValueOf(Defaults, "display.crt_filter") == "off");
	CHECK(ReadPlayerSettings("display.crt_filter = tv\n").Display.Crt == ECrtFilter::Television);
	CHECK(ReadPlayerSettings("display.crt_filter = scanlines\n").Display.Crt == ECrtFilter::Off);
	CHECK(ValueOf(Defaults, "crt.scanlines") == "9");
	CHECK(ValueOf(Defaults, "crt.mask") == "aperture_grille");
	const FDisplayOptions Crt = ReadPlayerSettings("display.crt_filter = custom\ncrt.scanlines = 12\ncrt.mask = slot_mask\ncrt.vignette = -1\n").Display;
	CHECK(Crt.Crt == ECrtFilter::Custom);
	CHECK(Crt.CustomCrt.Scanlines == FCrtLook::Steps);
	CHECK(Crt.CustomCrt.Mask == ECrtMask::SlotMask);
	CHECK(Crt.CustomCrt.Vignette == 0);
	CHECK(ValueOf(Defaults, "display.pause_in_background") == "true");
	CHECK(ValueOf(Defaults, "sound.in_background") == "false");
	CHECK(ReadPlayerSettings("sound.in_background = true\ndisplay.pause_in_background = false\n").Display
		== FDisplayOptions{{}, EDisplayAspect::Window, false, true});
	CHECK(ValueOf(Defaults, "camera.zoom") == "1.25");
	CHECK(ValueOf(Defaults, "camera.world_map_zoom") == "1");
	CHECK(ValueOf(Defaults, "controls.view_zoom") == "MouseWheelAxis -Gamepad_RightY");
	CHECK(ValueOf(Defaults, "camera.steady_follow") == "true");
	CHECK(ValueOf(Defaults, "controls.always_run") == "true");
	CHECK_FALSE(ReadPlayerSettings("controls.always_run = false\n").Controls.bAlwaysRun);
	CHECK(ValueOf(Defaults, "controls.analog_movement") == "true");
	CHECK_FALSE(ReadPlayerSettings("controls.analog_movement = false\n").Controls.bAnalogMovement);
	CHECK(ValueOf(Defaults, "controls.touch_movement") == "cross");
	CHECK(ReadPlayerSettings("controls.touch_movement = stick\n").Controls.TouchMovement == ETouchMovement::Stick);
	CHECK(ValueOf(Defaults, "controls.touch_opacity") == "10");
	CHECK(ReadPlayerSettings("controls.touch_opacity = 1\n").Controls.TouchOpacity == LowestTouchOpacity);
	CHECK(ReadPlayerSettings("controls.touch_opacity = 4\n").Controls.TouchOpacity == 4);
	CHECK(ValueOf(Defaults, "controls.touch_vibration") == "true");
	CHECK_FALSE(ReadPlayerSettings("controls.touch_vibration = false\n").Controls.bTouchVibration);
	CHECK(ValueOf(Defaults, "controls.walk_into_doors") == "true");
	CHECK_FALSE(ReadPlayerSettings("controls.walk_into_doors = false\n").Controls.bWalkIntoDoors);
	CHECK(ValueOf(Defaults, "controls.original_button_icons") == "false");
	CHECK(ReadPlayerSettings("controls.original_button_icons = true\n").Controls.bOriginalButtonIcons);
	CHECK_FALSE(ReadPlayerSettings("controls.original_button_icons = invalid\n").Controls.bOriginalButtonIcons);
	CHECK(ValueOf(Defaults, "controls.button_layout") == "usa");
	CHECK(ReadPlayerSettings("controls.button_layout = circle_confirms\n").Controls.ButtonLayout == EButtonLayout::CircleConfirms);
	CHECK(ReadPlayerSettings("controls.button_layout = invalid\n").Controls.ButtonLayout == EButtonLayout::Usa);
	CHECK_FALSE(ReadPlayerSettings("camera.steady_follow = false\n").Camera.bSteadyFollow);
	CHECK(ValueOf(Defaults, "game.text_speed") == "double");
	CHECK(ReadPlayerSettings("game.text_speed = original\n").TextSpeed == EMesTextSpeed::Original);
	CHECK(ValueOf(Defaults, "controls.mouse_sensitivity") == "5");
	CHECK(ValueOf(Defaults, "controls.stick_sensitivity") == "5");
	CHECK(ReadPlayerSettings("controls.mouse_sensitivity = 12\ncontrols.stick_sensitivity = 0\n").Controls.MouseSensitivity == HighestSensitivity);
	CHECK(ReadPlayerSettings("controls.stick_sensitivity = 0\n").Controls.StickSensitivity == LowestSensitivity);
	CHECK(ReadPlayerSettings("controls.stick_sensitivity = 2.5\n").Controls.StickSensitivity == DefaultSensitivity);
	CHECK(ReadPlayerSettings("game.text_speed = instant\n").TextSpeed == EMesTextSpeed::Instant);
	CHECK(ValueOf(Defaults, "game.battle_speed") == "normal");
	CHECK(ReadPlayerSettings("game.battle_speed = 2x\n").BattleSpeed == EBattleSpeed::Double);
	CHECK(ReadPlayerSettings("game.battle_speed = 3x\n").BattleSpeed == EBattleSpeed::Normal);
	CHECK(ValueOf(Defaults, "balance.experience") == "original");
	CHECK(ValueOf(Defaults, "balance.gold") == "original");
	CHECK(ValueOf(Defaults, "balance.encounters") == "original");
	CHECK(ReadPlayerSettings("balance.experience = 3x\n").Balance.Experience == ERewardRate::Triple);
	CHECK(ReadPlayerSettings("balance.encounters = none\n").Balance.Encounters == EEncounterRate::None);
	CHECK(ReadPlayerSettings("balance.gold = 5x\n").Balance.Gold == ERewardRate::Original);
	CHECK(ValueOf(Defaults, "mods.perfect_works") == "false");
	CHECK(ValueOf(Defaults, "mods.perfect_works.script") == "true");
	CHECK(ValueOf(Defaults, "mods.perfect_works.portraits") == "fixed");
	CHECK(ValueOf(Defaults, "mods.perfect_works.arena") == "original");
	CHECK(ReadPlayerSettings("mods.perfect_works.arena = expert\n").PerfectWorks.Arena == EPerfectWorksArena::Expert);
	CHECK(ReadPlayerSettings("mods.perfect_works = true\nmods.perfect_works.portraits = resized\n").PerfectWorks
		== FPerfectWorksOptions{true, true, EPerfectWorksPortraits::Resized});
	CHECK(ReadPlayerSettings("field.sorting = per_pixel\n").Field.Sorting == EFieldSorting::DepthBuffer);
	// The value the earlier default wrote reads as the default it has become.
	CHECK(ReadPlayerSettings("field.sorting = depth_buffer\n").Field.Sorting == EFieldSorting::DepthBufferWithScenery);
	// The gears' models were once on or off: on reads as everywhere.
	CHECK(ReadPlayerSettings("field.gear_models = true\n").Field.GearModels == EFieldGearModels::Everywhere);
	CHECK(ReadPlayerSettings("field.gear_models = false\n").Field.GearModels == EFieldGearModels::Off);
	CHECK(ValueOf(Defaults, "controls.up") == "W Up Gamepad_DPad_Up Gamepad_LeftStick_Up");
	CHECK(ValueOf(Defaults, "controls.view_tilt").empty());
	CheckSame(ReadPlayerSettings(Defaults), FPlayerSettings{});

	const FPlayerSettings Settings = Changed();
	const std::string Text = WritePlayerSettings(Settings);
	CHECK(ValueOf(Text, "fix.map_change_waits_whole") == "false");
	CHECK(ValueOf(Text, "field.sorting") == "painters_order");
	CHECK(ValueOf(Text, "field.movie_filter") == "upscaled");
	CHECK(ValueOf(Text, "field.scenery_filter") == "bilinear");
	CHECK(ValueOf(Text, "field.sprite_filter") == "upscaled");
	CHECK(ValueOf(Text, "field.detailed_pilots") == "false");
	CHECK(ValueOf(Text, "field.gear_models") == "true");
	CHECK(ValueOf(Text, "field.battle_flashes") == "softer");
	CHECK(ReadPlayerSettings("field.battle_flashes = off\n").Field.BattleFlashes == EBattleFlashes::Off);
	CHECK(ValueOf(Text, "sound.output") == "wide_stereo");
	CHECK(ValueOf(Text, "display.window_size") == "2560x1440");
	CHECK(ValueOf(Text, "display.aspect") == "16:9");
	CHECK(ValueOf(Text, "display.crt_filter") == "custom");
	CHECK(ValueOf(Text, "crt.mask_size") == "medium");
	CHECK(ValueOf(Text, "crt.glow") == "10");
	CHECK(ValueOf(Text, "camera.tilt") == "-7.25");
	CHECK(ValueOf(Text, "controls.cross") == "SpaceBar Enter Gamepad_FaceButton_Right");
	CHECK(ValueOf(Text, "controls.square") == "F Gamepad_FaceButton_Top");
	CHECK(ValueOf(Text, "controls.button_layout") == "japan");
	CHECK(ValueOf(Text, "controls.view_reset") == "Gamepad_RightThumbstick");
	CHECK(ValueOf(Text, "game.battle_speed") == "4x");
	CHECK(ValueOf(Text, "balance.experience") == "1.5x");
	CHECK(ValueOf(Text, "balance.gold") == "4x");
	CHECK(ValueOf(Text, "balance.encounters") == "quarter");
	CHECK(ValueOf(Text, "mods.perfect_works") == "true");
	CHECK(ValueOf(Text, "mods.perfect_works.portraits") == "resized");
	CHECK(ValueOf(Text, "mods.perfect_works.title_screen") == "false");
	CHECK(ValueOf(Text, "controls.mouse_sensitivity") == "8");
	CheckSame(ReadPlayerSettings(Text), Settings);
}

TEST_CASE("Settings a text lacks or cannot read take their defaults, and unknown lines are passed over", "[Settings]")
{
	const FPlayerSettings Settings = ReadPlayerSettings(
		"# a comment\n"
		"not a setting\n"
		"future.option = 12\n"
		"  field.sorting   =   painters_order  \r\n"
		"field.scenery_filter = sharp\n"
		"fix.zero_frame_fades_snap = off\n"
		"fix.slow_fades_rise_from_black = false\n"
		"camera.zoom = 5\n"
		"camera.tilt = lots\n"
		"camera.turn = -180\n"
		"display.window_size = 1920x\n"
		"display.aspect = 21:9\n"
		"controls.select =\n");
	CHECK(Settings.Field.Sorting == EFieldSorting::PaintersOrder);
	CHECK(Settings.Field.Filter == EFieldTextureFilter::Nearest);
	CHECK(Settings.Fixes.Field.bZeroFrameFadeSnaps);
	CHECK_FALSE(Settings.Fixes.Field.bFadesRiseFromBlack);
	CHECK(Settings.Camera.Zoom == FFieldCameraOffsets::MaxZoom);
	CHECK(Settings.Camera.Tilt == 0.0f);
	CHECK(Settings.Camera.Turn == 180.0f);
	CHECK_FALSE(Settings.Display.WindowSize.IsSet());
	CHECK(Settings.Display.Aspect == EDisplayAspect::Window);
	CHECK_FALSE(Settings.Display.bFullScreen);
	CHECK(ReadPlayerSettings("display.full_screen = true\n").Display.bFullScreen);
	CHECK_FALSE(ReadPlayerSettings("display.window_size = 0x600\n").Display.WindowSize.IsSet());
	CHECK_FALSE(ReadPlayerSettings("display.window_size = 800x600 \t px\n").Display.WindowSize.IsSet());
	CHECK(ReadPlayerSettings("display.window_size = 800x600\ndisplay.aspect = 4:3\n").Display == FDisplayOptions{{800, 600}, EDisplayAspect::Standard});
	CHECK(Settings.Controls.Keys(EControlAction::Select).empty());
	CHECK(Settings.Controls.Keys(EControlAction::Start) == FControlBindings::Defaults().Keys(EControlAction::Start));
	CheckSame(ReadPlayerSettings(""), FPlayerSettings{});
}

TEST_CASE("A setting's number is read whole as std::to_chars writes it, in any locale", "[Settings]")
{
	const float Default = FPlayerSettings{}.Camera.Zoom;
	const auto ZoomOf = [](const char* Text) { return ReadPlayerSettings(std::string("camera.zoom = ") + Text + "\n").Camera.Zoom; };
	CHECK(ZoomOf("1.5") == 1.5f);
	CHECK(ZoomOf("0.75") == 0.75f);
	CHECK(ZoomOf("1e0") == 1.0f);
	CHECK(ZoomOf("1e+00") == 1.0f);
	CHECK(ZoomOf("-0") == FFieldCameraOffsets::MinZoom);
	for (const char* Unread : {"+1.5", "1,5", "1.5x", "x1.5", "inf", "nan", "0x1p0", ".", "-"})
	{
		INFO(Unread);
		CHECK(ZoomOf(Unread) == Default);
	}
}

TEST_CASE("A key the text binds to two actions stays with the last, and over a default", "[Settings]")
{
	const FPlayerSettings Settings = ReadPlayerSettings("controls.cross = K SpaceBar\ncontrols.circle = SpaceBar L\n");
	CHECK(Settings.Controls.Keys(EControlAction::Cross) == std::vector<std::string>{"K"});
	CHECK(Settings.Controls.Keys(EControlAction::Circle) == std::vector<std::string>{"SpaceBar", "L"});

	const FPlayerSettings Taken = ReadPlayerSettings("controls.cross = Q\n");
	CHECK(Taken.Controls.Keys(EControlAction::Cross) == std::vector<std::string>{"Q"});
	CHECK(Taken.Controls.Keys(EControlAction::Circle) == std::vector<std::string>{"Escape", "Gamepad_FaceButton_Right"});
}

TEST_CASE("The sound's output choices are the driver's modes", "[Settings]")
{
	CHECK(SoundDriverOutputMode(ESoundOutput::Mono) == 0);
	CHECK(SoundDriverOutputMode(ESoundOutput::Stereo) == 1);
	CHECK(SoundDriverOutputMode(ESoundOutput::WideStereo) == 2);
}
