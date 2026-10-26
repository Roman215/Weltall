#include "Game/PlayerSettings.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr std::string_view Header = "# The game's settings: one \"key = value\" a line. A key left out takes its default.\n";

		std::string_view Trimmed(std::string_view Text)
		{
			constexpr std::string_view Blanks = " \t\r";
			const std::size_t First = Text.find_first_not_of(Blanks);
			if (First == std::string_view::npos)
			{
				return {};
			}
			return Text.substr(First, Text.find_last_not_of(Blanks) - First + 1);
		}

		std::vector<std::string> Words(std::string_view Text)
		{
			std::vector<std::string> Result;
			std::size_t At = 0;
			while (At < Text.size())
			{
				const std::size_t Start = Text.find_first_not_of(" \t", At);
				if (Start == std::string_view::npos)
				{
					break;
				}
				const std::size_t End = std::min(Text.find_first_of(" \t", Start), Text.size());
				Result.emplace_back(Text.substr(Start, End - Start));
				At = End;
			}
			return Result;
		}

		/** Writes each setting as a line. */
		class FWriter
		{
		public:
			void Flag(std::string_view Key, const bool& Value) { Line(Key, Value ? "true" : "false"); }

			template <typename TEnum>
			void Choice(std::string_view Key, const TEnum& Value, std::initializer_list<std::string_view> Names)
			{
				Line(Key, *(Names.begin() + static_cast<std::ptrdiff_t>(Value)));
			}

			void Number(std::string_view Key, const float& Value, float, float)
			{
				char Digits[32];
				const std::to_chars_result Written = std::to_chars(Digits, Digits + sizeof(Digits), Value);
				Line(Key, std::string_view(Digits, static_cast<std::size_t>(Written.ptr - Digits)));
			}

			void Level(std::string_view Key, const int32& Value, int32, int32) { Line(Key, std::to_string(Value)); }

			void Size(std::string_view Key, const FDisplaySize& Value)
			{
				Line(Key, Value.IsSet() ? std::to_string(Value.Width) + "x" + std::to_string(Value.Height) : std::string());
			}

			void Keys(std::string_view Key, std::optional<std::vector<std::string>>& Bound)
			{
				std::string Joined;
				for (const std::string& Name : *Bound)
				{
					Joined += Joined.empty() ? "" : " ";
					Joined += Name;
				}
				Line(Key, Joined);
			}

			std::string Text{Header};

		private:
			void Line(std::string_view Key, std::string_view Value)
			{
				Text.append(Key).append(Value.empty() ? " =" : " = ").append(Value).push_back('\n');
			}
		};

		/** Takes each setting from the text's lines, leaving those it lacks or cannot read. */
		class FReader
		{
		public:
			explicit FReader(std::string_view Text)
			{
				while (!Text.empty())
				{
					const std::size_t End = std::min(Text.find('\n'), Text.size());
					const std::string_view Line = Trimmed(Text.substr(0, End));
					Text.remove_prefix(std::min(End + 1, Text.size()));
					const std::size_t Equals = Line.find('=');
					if (Line.empty() || Line.front() == '#' || Equals == std::string_view::npos)
					{
						continue;
					}
					Values[std::string(Trimmed(Line.substr(0, Equals)))] = std::string(Trimmed(Line.substr(Equals + 1)));
				}
			}

			void Flag(std::string_view Key, bool& Value)
			{
				if (const std::string* Text = Find(Key))
				{
					if (*Text == "true" || *Text == "false")
					{
						Value = *Text == "true";
					}
				}
			}

			template <typename TEnum>
			void Choice(std::string_view Key, TEnum& Value, std::initializer_list<std::string_view> Names)
			{
				if (const std::string* Text = Find(Key))
				{
					const auto Found = std::find(Names.begin(), Names.end(), std::string_view(*Text));
					if (Found != Names.end())
					{
						Value = static_cast<TEnum>(Found - Names.begin());
					}
				}
			}

			void Number(std::string_view Key, float& Value, float Min, float Max)
			{
				const std::string* Text = Find(Key);
				if (Text == nullptr || Text->empty() || Text->front() == '+' || Text->find_first_not_of("0123456789.eE+-") != std::string::npos)
				{
					return;
				}
				// Read in the C locale, as std::to_chars wrote it: Android's C++ library has no floating-point from_chars.
				std::istringstream Stream(*Text);
				Stream.imbue(std::locale::classic());
				float Read = 0.0f;
				Stream >> std::noskipws >> Read;
				if (!Stream.fail() && Stream.peek() == std::char_traits<char>::eof() && std::isfinite(Read))
				{
					Value = std::clamp(Read, Min, Max);
				}
			}

			void Level(std::string_view Key, int32& Value, int32 Min, int32 Max)
			{
				const std::string* Text = Find(Key);
				int32 Read = 0;
				if (Text != nullptr)
				{
					const std::from_chars_result Parsed = std::from_chars(Text->data(), Text->data() + Text->size(), Read);
					if (Parsed.ec == std::errc() && Parsed.ptr == Text->data() + Text->size())
					{
						Value = std::clamp(Read, Min, Max);
					}
				}
			}

			void Size(std::string_view Key, FDisplaySize& Value)
			{
				const std::string* Text = Find(Key);
				const std::size_t By = Text != nullptr ? Text->find('x') : std::string::npos;
				if (By == std::string::npos)
				{
					return;
				}
				FDisplaySize Read;
				const char* const Middle = Text->data() + By;
				const char* const End = Text->data() + Text->size();
				const std::from_chars_result Width = std::from_chars(Text->data(), Middle, Read.Width);
				const std::from_chars_result Height = std::from_chars(Middle + 1, End, Read.Height);
				if (Width.ec == std::errc() && Width.ptr == Middle && Height.ec == std::errc() && Height.ptr == End && Read.IsSet())
				{
					Value = Read;
				}
			}

			void Keys(std::string_view Key, std::optional<std::vector<std::string>>& Bound)
			{
				if (const std::string* Text = Find(Key))
				{
					Bound = Words(*Text);
				}
			}

		private:
			const std::string* Find(std::string_view Key) const
			{
				const auto Found = Values.find(std::string(Key));
				return Found != Values.end() ? &Found->second : nullptr;
			}

			std::map<std::string, std::string> Values;
		};

		/** Each action's keys as the text has them: none for an action it does not name. */
		using FBoundKeys = std::array<std::optional<std::vector<std::string>>, ControlActionCount>;

		/** Every setting by its key, in the text's order. The controls' keys go through Bindings. */
		template <typename TSettings, typename TVisitor>
		void VisitSettings(TSettings& Settings, TVisitor& Visitor, FBoundKeys& Bindings)
		{
			auto& Fixes = Settings.Fixes;
			Visitor.Flag("fix.recover_party_hp_heals_hp", Fixes.Field.bRecoverPartyHpGivesHp);
			Visitor.Flag("fix.zero_frame_fades_snap", Fixes.Field.bZeroFrameFadeSnaps);
			Visitor.Flag("fix.slow_fades_rise_from_black", Fixes.Field.bFadesRiseFromBlack);
			Visitor.Flag("fix.map_change_waits_whole", Fixes.Field.bChangeMapWaitsWhole);
			Visitor.Flag("fix.gear_channel_pool_grows", Fixes.Field.bGearChannelPoolGrows);
			Visitor.Flag("fix.scrollers_wrap_within_width", Fixes.Field.bScrollsWrapWithinWidth);
			Visitor.Flag("fix.init_turn_turns_whole", Fixes.Field.bInitTurnSetsTurningYaw);
			Visitor.Flag("fix.shake_depth_from_its_own", Fixes.Field.bShakeDepthFromItsOwn);
			Visitor.Flag("fix.ladder_only_with_its_animation", Fixes.Field.bLadderOnlyWithItsAnimation);
			Visitor.Flag("fix.hotel_counts_whole_party", Fixes.Field.bHotelCountsWholeParty);
			Visitor.Flag("fix.ship_sounds_at_their_volume", Fixes.Field.bShipSoundsAtTheirVolume);
			Visitor.Flag("fix.memory_ambience_fades", Fixes.Field.bMemoryAmbienceFades);
			Visitor.Flag("fix.actors_run_in_wide_view", Fixes.Field.bActorsRunInWideView);
			Visitor.Flag("fix.party_sprites_follow_their_file", Fixes.Field.bPartySpritesFollowTheirFile);
			Visitor.Flag("fix.rps_helper", Fixes.Field.bRpsHelper);
			Visitor.Flag("fix.scene_pictures", Fixes.Field.bScenePictures);
			Visitor.Flag("fix.wide_view_staging", Fixes.Field.bWideViewStaging);
			Visitor.Flag("fix.swim_stroke", Fixes.Field.bSwimStrokeAfterWading);
			Visitor.Flag("fix.moon_glow", Fixes.Field.bMoonGlowFades);
			Visitor.Flag("fix.sprite_frames", Fixes.Field.bSpriteFramesAligned);
			Visitor.Flag("fix.silent_start_only_for_its_song", Fixes.Music.bStartOnlyForItsSong);
			Visitor.Flag("fix.pan_slides_end_on_target", Fixes.SoundDriver.bPanSlidesEndOnTarget);
			Visitor.Flag("fix.high_notes_stay_high", Fixes.SoundDriver.bHighNotesStayHigh);
			Visitor.Flag("fix.load_penalty_from_saved_tolerance", Fixes.Load.bPenaltyFromSavedTolerance);
			Visitor.Flag("fix.load_deathblows_from_gear_in_use", Fixes.Load.bDeathblowsFromGearInUse);
			Visitor.Flag("fix.menu_agility_shown_capped", Fixes.Menu.bAgilityShownCapped);
			Visitor.Flag("fix.menu_deathblow_bar_whole", Fixes.Menu.bDeathblowBarWhole);
			Visitor.Flag("fix.menu_ammunition_kept", Fixes.Menu.bAmmunitionKept);
			Visitor.Flag("fix.menu_ether_heals_as_battle", Fixes.Menu.bEtherHealsAsBattle);
			Visitor.Flag("fix.menu_gear_repair_own_gear", Fixes.Menu.bGearRepairOwnGear);
			Visitor.Flag("fix.menu_names_keep_trailing_s", Fixes.Menu.bNamesKeepTrailingS);
			Visitor.Flag("fix.member_change_says_ep", Fixes.Menu.bMemberChangeSaysEp);
			Visitor.Flag("fix.shop_pointer_beside_words", Fixes.Menu.bShopPointerBesideWords);
			Visitor.Flag("fix.gear_shop_parts_as_menu", Fixes.Menu.bGearShopPartsAsMenu);
			Visitor.Flag("fix.gear_shop_model_field_pace", Fixes.Menu.bGearModelFieldPace);
			Visitor.Flag("fix.shop_sale_recomputes_equipment", Fixes.Menu.bSaleRecomputesEquipment);
			Visitor.Flag("fix.new_game_clock_from_zero", Fixes.Menu.bNewGameClockFromZero);
			Visitor.Flag("fix.battle_infinity_by_damage_alone", Fixes.BattleRules.bInfinityByDamageAlone);
			Visitor.Flag("fix.battle_item_buffs_by_its_user", Fixes.BattleRules.bItemBuffsByItsUser);
			Visitor.Flag("fix.battle_ep_grows_at_last_level", Fixes.BattleRules.bEpGrowsAtLastLevel);
			Visitor.Flag("fix.battle_ether_buffs_set_directly", Fixes.BattleRules.bEtherBuffsSetDirectly);
			Visitor.Flag("fix.battle_every_buff_expires", Fixes.BattleRules.bEveryBuffExpires);
			Visitor.Flag("fix.battle_def_up_on_foot", Fixes.BattleRules.bDefUpOnFoot);
			Visitor.Flag("fix.battle_def_down_on_foot", Fixes.BattleRules.bDefDownOnFoot);
			Visitor.Flag("fix.battle_failed_status_keeps_count", Fixes.BattleRules.bFailedStatusKeepsCount);
			Visitor.Flag("fix.battle_immunity_items_work", Fixes.BattleRules.bImmunityItemsWork);
			Visitor.Flag("fix.battle_gear_elements_expire", Fixes.BattleRules.bGearElementsExpire);
			Visitor.Flag("fix.battle_gear_stop_ends", Fixes.BattleRules.bGearStopEnds);
			Visitor.Flag("fix.battle_reveal_keeps_group", Fixes.BattleRules.bRevealKeepsGroup);
			Visitor.Flag("fix.battle_gear_power_loss", Fixes.BattleRules.bGearPowerLoss);
			Visitor.Flag("fix.battle_whole_amounts", Fixes.BattleRules.bWholeAmounts);
			Visitor.Flag("fix.battle_counter_keeps_group_attack", Fixes.BattleRules.bCounterKeepsGroupAttack);
			Visitor.Flag("fix.battle_deus_second_form_hp", Fixes.BattleRules.bDeusSecondFormHp);
			Visitor.Flag("fix.battle_deus_keeps_camera", Fixes.BattleRules.bDeusKeepsCamera);
			Visitor.Flag("fix.battle_ether_ammunition_element", Fixes.BattleRules.bEtherAmmunitionElement);
			Visitor.Flag("fix.battle_trails_blend", Fixes.BattleShow.bTrailsBlend);
			Visitor.Flag("fix.battle_unlit_models_take_stage_light", Fixes.BattleShow.bUnlitModelsTakeStageLight);
			Visitor.Flag("fix.battle_tint_keeps_stage_palettes", Fixes.BattleShow.bTintKeepsStagePalettes);
			Visitor.Flag("fix.battle_event_waited_start_names_actor", Fixes.BattleEvents.bWaitedStartNamesActor);
			Visitor.Flag("fix.battle_event_unscripted_defeat_is_lost", Fixes.BattleEvents.bUnscriptedDefeatIsLost);
			Visitor.Flag("fix.worldmap_landmark_back_after_battle", Fixes.WorldMap.bStoryObjectBackAfterBattle);
			Visitor.Flag("fix.worldmap_scene14_sweep_heard", Fixes.WorldMap.bScene14SweepHeard);
			Visitor.Flag("fix.worldmap_scene18_effects_wait", Fixes.WorldMap.bScene18EffectsWait);
			Visitor.Flag("fix.worldmap_trees_in_depth_order", Fixes.WorldMap.bTreesInDepthOrder);
			Visitor.Flag("fix.arena_ranged_moves_by_own_state", Fixes.Battling.bRangedMovesByOwnState);
			Visitor.Flag("fix.arena_practice_computer_at_style_level", Fixes.Battling.bPracticeComputerAtStyleLevel);
			Visitor.Flag("fix.arena_practice_frame_rate_as_labelled", Fixes.Battling.bPracticeFrameRateAsLabelled);

			Visitor.Flag("display.full_screen", Settings.Display.bFullScreen);
			Visitor.Size("display.window_size", Settings.Display.WindowSize);
			Visitor.Choice("display.aspect", Settings.Display.Aspect, {"window", "16:9", "4:3"});
			Visitor.Choice("display.crt_filter", Settings.Display.Crt, {"off", "monitor", "tv", "custom"});
			auto& Crt = Settings.Display.CustomCrt;
			Visitor.Level("crt.scanlines", Crt.Scanlines, 0, FCrtLook::Steps);
			Visitor.Choice("crt.mask", Crt.Mask, {"none", "aperture_grille", "slot_mask"});
			Visitor.Level("crt.mask_strength", Crt.MaskStrength, 0, FCrtLook::Steps);
			Visitor.Choice("crt.mask_size", Crt.MaskSize, {"fine", "medium", "coarse"});
			Visitor.Level("crt.sharpness", Crt.Sharpness, 0, FCrtLook::Steps);
			Visitor.Level("crt.color_bleed", Crt.ColorBleed, 0, FCrtLook::Steps);
			Visitor.Level("crt.glow", Crt.Glow, 0, FCrtLook::Steps);
			Visitor.Level("crt.curvature", Crt.Curvature, 0, FCrtLook::Steps);
			Visitor.Level("crt.corners", Crt.Corners, 0, FCrtLook::Steps);
			Visitor.Level("crt.vignette", Crt.Vignette, 0, FCrtLook::Steps);
			Visitor.Flag("display.pause_in_background", Settings.Display.bPauseInBackground);

			auto& Field = Settings.Field;
			Visitor.Choice("field.sorting", Field.Sorting, {"painters_order", "per_pixel", "scenery_too"});
			Visitor.Choice("field.scenery_filter", Field.Filter, {"nearest", "crisp", "bilinear", "upscaled"});
			Visitor.Choice("field.sprite_filter", Field.SpriteFilter, {"nearest", "crisp", "bilinear", "upscaled"});
			Visitor.Choice("field.movie_filter", Field.MovieFilter, {"smooth", "upscaled"});
			Visitor.Choice("field.anti_aliasing", Field.AntiAliasing, {"none", "msaa", "supersampling"});
			Visitor.Choice("field.culling", Field.Culling, {"original", "in_range"});
			Visitor.Flag("field.detailed_pilots", Field.bDetailedPilots);
			// Once a flag: its true and false read as they did.
			Visitor.Choice("field.gear_models", Field.GearModels, {"false", "outside_cutscenes", "true"});
			Visitor.Choice("field.battle_flashes", Field.BattleFlashes, {"original", "softer", "off"});

			Visitor.Level("sound.master_volume", Settings.Volumes.Master, 0, FSoundVolumes::Full);
			Visitor.Level("sound.music_volume", Settings.Volumes.Music, 0, FSoundVolumes::Full);
			Visitor.Level("sound.effects_volume", Settings.Volumes.Effects, 0, FSoundVolumes::Full);
			Visitor.Level("sound.voices_volume", Settings.Volumes.Voices, 0, FSoundVolumes::Full);
			Visitor.Level("sound.movies_volume", Settings.Volumes.Movies, 0, FSoundVolumes::Full);
			Visitor.Choice("sound.output", Settings.SoundOutput, {"stereo", "wide_stereo", "mono"});
			Visitor.Flag("sound.in_background", Settings.Display.bSoundInBackground);
			Visitor.Choice("sound.voices", Settings.VoiceLanguage, {"automatic", "english", "japanese"});
			Visitor.Choice("sound.subtitles", Settings.MovieSubtitles, {"automatic", "on", "off"});
			Visitor.Choice("sound.subtitles_from", Settings.MovieSubtitleSource, {"automatic", "undub", "perfect_works"});
			Visitor.Flag("assets.external", Settings.bExternalAssets);
			Visitor.Flag("assets.capture_textures", Settings.bCaptureTextures);

			Visitor.Flag("game.chapter_cards", Settings.bChapterCards);
			Visitor.Choice("game.text_speed", Settings.TextSpeed, {"original", "double", "quadruple", "instant"});
			Visitor.Choice("game.battle_speed", Settings.BattleSpeed, {"normal", "2x", "4x"});

			auto& Balance = Settings.Balance;
			Visitor.Choice("balance.experience", Balance.Experience, {"original", "1.5x", "2x", "3x", "4x"});
			Visitor.Choice("balance.gold", Balance.Gold, {"original", "1.5x", "2x", "3x", "4x"});
			Visitor.Choice("balance.encounters", Balance.Encounters, {"none", "quarter", "half", "original", "double"});

			auto& PerfectWorks = Settings.PerfectWorks;
			Visitor.Flag("mods.perfect_works", PerfectWorks.bEnabled);
			Visitor.Flag("mods.perfect_works.script", PerfectWorks.bScript);
			Visitor.Flag("mods.perfect_works.story_mode", PerfectWorks.bStoryMode);
			Visitor.Flag("mods.perfect_works.music", PerfectWorks.bMusic);
			Visitor.Flag("mods.perfect_works.rebalanced_party", PerfectWorks.bRebalancedParty);
			Visitor.Flag("mods.perfect_works.rebalanced_enemies", PerfectWorks.bRebalancedEnemies);
			Visitor.Choice("mods.perfect_works.arena", PerfectWorks.Arena, {"original", "basic", "expert"});
			Visitor.Choice("mods.perfect_works.portraits", PerfectWorks.Portraits, {"original", "fixed", "resized"});
			Visitor.Flag("mods.perfect_works.roni_portrait", PerfectWorks.bRoniPortrait);
			Visitor.Flag("mods.perfect_works.emeralda_cafe", PerfectWorks.bEmeraldaCafe);
			Visitor.Flag("mods.perfect_works.title_screen", PerfectWorks.bTitleScreen);

			auto& Camera = Settings.Camera;
			Visitor.Number("camera.zoom", Camera.Zoom, FFieldCameraOffsets::MinZoom, FFieldCameraOffsets::MaxZoom);
			Visitor.Number("camera.tilt", Camera.Tilt, FFieldCameraOffsets::MinTilt, FFieldCameraOffsets::MaxTilt);
			Visitor.Number("camera.turn", Camera.Turn, -180.0f, 180.0f);
			Visitor.Flag("camera.yields_to_scenes", Camera.bYieldsToScenes);
			Visitor.Flag("camera.steady_follow", Camera.bSteadyFollow);
			auto& WorldMapCamera = Settings.WorldMapCamera;
			Visitor.Number("camera.world_map_zoom", WorldMapCamera.Zoom, FWorldMapCameraOffsets::MinZoom, FWorldMapCameraOffsets::MaxZoom);
			Visitor.Number("camera.world_map_tilt", WorldMapCamera.Tilt, FWorldMapCameraOffsets::MinTilt, FWorldMapCameraOffsets::MaxTilt);
			Visitor.Number("camera.world_map_turn", WorldMapCamera.Turn, -180.0f, 180.0f);

			Visitor.Flag("controls.always_run", Settings.Controls.bAlwaysRun);
			Visitor.Flag("controls.analog_movement", Settings.Controls.bAnalogMovement);
			Visitor.Flag("controls.walk_into_doors", Settings.Controls.bWalkIntoDoors);
			Visitor.Flag("controls.original_button_icons", Settings.Controls.bOriginalButtonIcons);
			Visitor.Choice("controls.button_layout", Settings.Controls.ButtonLayout, {"usa", "circle_confirms", "japan"});
			Visitor.Level("controls.mouse_sensitivity", Settings.Controls.MouseSensitivity, LowestSensitivity, HighestSensitivity);
			Visitor.Level("controls.stick_sensitivity", Settings.Controls.StickSensitivity, LowestSensitivity, HighestSensitivity);
			Visitor.Choice("controls.touch_movement", Settings.Controls.TouchMovement, {"cross", "stick"});
			Visitor.Level("controls.touch_opacity", Settings.Controls.TouchOpacity, LowestTouchOpacity, HighestTouchOpacity);
			Visitor.Flag("controls.touch_vibration", Settings.Controls.bTouchVibration);
			constexpr std::array<std::string_view, ControlActionCount> ActionKeys{"controls.up", "controls.down", "controls.left", "controls.right",
				"controls.cross", "controls.circle", "controls.square", "controls.triangle", "controls.l1", "controls.r1", "controls.l2", "controls.r2",
				"controls.start", "controls.select", "controls.view_turn", "controls.view_tilt", "controls.view_zoom", "controls.view_reset"};
			for (std::size_t Action = 0; Action < ActionKeys.size(); ++Action)
			{
				Visitor.Keys(ActionKeys[Action], Bindings[Action]);
			}
		}
	}

	std::string WritePlayerSettings(const FPlayerSettings& Settings)
	{
		FBoundKeys Bindings;
		for (int32 Action = 0; Action < ControlActionCount; ++Action)
		{
			Bindings[static_cast<std::size_t>(Action)] = Settings.Controls.Keys(static_cast<EControlAction>(Action));
		}
		FPlayerSettings Copy = Settings;
		FWriter Writer;
		VisitSettings(Copy, Writer, Bindings);
		return std::move(Writer.Text);
	}

	FPlayerSettings ReadPlayerSettings(std::string_view Text)
	{
		FPlayerSettings Settings;
		FBoundKeys Bindings;
		FReader Reader(Text);
		VisitSettings(Settings, Reader, Bindings);
		// The actions the text names take their keys after the defaults, a key named twice going to the
		// last action that names it, as binding them one by one would.
		for (int32 Action = 0; Action < ControlActionCount; ++Action)
		{
			if (const std::optional<std::vector<std::string>>& Keys = Bindings[static_cast<std::size_t>(Action)])
			{
				Settings.Controls.SetKeys(static_cast<EControlAction>(Action), *Keys);
			}
		}
		if (Settings.Camera.Turn <= -180.0f)
		{
			Settings.Camera.Turn = 180.0f;
		}
		if (Settings.WorldMapCamera.Turn <= -180.0f)
		{
			Settings.WorldMapCamera.Turn = 180.0f;
		}
		return Settings;
	}
}
