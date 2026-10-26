#include "Menu/MenuSettingsPages.h"

#include "Menu/MenuSoundPage.h"
#include "Menu/SettingsRows.h"
#include "Menu/OptionsText.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Weltall
{
	namespace
	{
		namespace FixGroup = OptionsText::FixGroups;
		namespace FixText = OptionsText::Fix;

		/** A fix the Custom page lists: the heading it shows under, its words, and its switch. */
		struct FFixEntry
		{
			std::string_view Group;
			const OptionsText::FRow& Text;
			bool& (*Switch)(FGameFixes&);
		};

		// The fixes by where they show, each group's heading the first entry's Group.
		const std::array<FFixEntry, 67> FixTable = {{
			{FixGroup::Field, FixText::HpRecovery, [](FGameFixes& F) -> bool& { return F.Field.bRecoverPartyHpGivesHp; }},
			{FixGroup::Field, FixText::InstantFades, [](FGameFixes& F) -> bool& { return F.Field.bZeroFrameFadeSnaps; }},
			{FixGroup::Field, FixText::SlowFades, [](FGameFixes& F) -> bool& { return F.Field.bFadesRiseFromBlack; }},
			{FixGroup::Field, FixText::MovieMapChange, [](FGameFixes& F) -> bool& { return F.Field.bChangeMapWaitsWhole; }},
			{FixGroup::Field, FixText::GearAnimations, [](FGameFixes& F) -> bool& { return F.Field.bGearChannelPoolGrows; }},
			{FixGroup::Field, FixText::ScrollingTextures, [](FGameFixes& F) -> bool& { return F.Field.bScrollsWrapWithinWidth; }},
			{FixGroup::Field, FixText::EntryCamera, [](FGameFixes& F) -> bool& { return F.Field.bInitTurnSetsTurningYaw; }},
			{FixGroup::Field, FixText::ScreenShakes, [](FGameFixes& F) -> bool& { return F.Field.bShakeDepthFromItsOwn; }},
			{FixGroup::Field, FixText::Ladders, [](FGameFixes& F) -> bool& { return F.Field.bLadderOnlyWithItsAnimation; }},
			{FixGroup::Field, FixText::BledavikHotel, [](FGameFixes& F) -> bool& { return F.Field.bHotelCountsWholeParty; }},
			{FixGroup::Field, FixText::RamsusShip, [](FGameFixes& F) -> bool& { return F.Field.bShipSoundsAtTheirVolume; }},
			{FixGroup::Field, FixText::LacanMemories, [](FGameFixes& F) -> bool& { return F.Field.bMemoryAmbienceFades; }},
			{FixGroup::Field, FixText::WidescreenCharacters, [](FGameFixes& F) -> bool& { return F.Field.bActorsRunInWideView; }},
			{FixGroup::Field, FixText::PartySprites, [](FGameFixes& F) -> bool& { return F.Field.bPartySpritesFollowTheirFile; }},
			{FixGroup::Field, FixText::RpsHelp, [](FGameFixes& F) -> bool& { return F.Field.bRpsHelper; }},
			{FixGroup::Field, FixText::ScenePictures, [](FGameFixes& F) -> bool& { return F.Field.bScenePictures; }},
			{FixGroup::Field, FixText::WidescreenStaging, [](FGameFixes& F) -> bool& { return F.Field.bWideViewStaging; }},
			{FixGroup::Field, FixText::SwimStroke, [](FGameFixes& F) -> bool& { return F.Field.bSwimStrokeAfterWading; }},
			{FixGroup::Field, FixText::MoonGlow, [](FGameFixes& F) -> bool& { return F.Field.bMoonGlowFades; }},
			{FixGroup::Field, FixText::SpriteFrames, [](FGameFixes& F) -> bool& { return F.Field.bSpriteFramesAligned; }},
			{FixGroup::Sound, FixText::SilentSongs, [](FGameFixes& F) -> bool& { return F.Music.bStartOnlyForItsSong; }},
			{FixGroup::Sound, FixText::SoundPanning, [](FGameFixes& F) -> bool& { return F.SoundDriver.bPanSlidesEndOnTarget; }},
			{FixGroup::Sound, FixText::HighNotes, [](FGameFixes& F) -> bool& { return F.SoundDriver.bHighNotesStayHigh; }},
			{FixGroup::Loading, FixText::GearWeight, [](FGameFixes& F) -> bool& { return F.Load.bPenaltyFromSavedTolerance; }},
			{FixGroup::Loading, FixText::GearDeathblows, [](FGameFixes& F) -> bool& { return F.Load.bDeathblowsFromGearInUse; }},
			{FixGroup::Menus, FixText::AgilityShown, [](FGameFixes& F) -> bool& { return F.Menu.bAgilityShownCapped; }},
			{FixGroup::Menus, FixText::DeathblowBar, [](FGameFixes& F) -> bool& { return F.Menu.bDeathblowBarWhole; }},
			{FixGroup::Menus, FixText::BillyAmmo, [](FGameFixes& F) -> bool& { return F.Menu.bAmmunitionKept; }},
			{FixGroup::Menus, FixText::EtherHealing, [](FGameFixes& F) -> bool& { return F.Menu.bEtherHealsAsBattle; }},
			{FixGroup::Menus, FixText::FixFrameHp, [](FGameFixes& F) -> bool& { return F.Menu.bGearRepairOwnGear; }},
			{FixGroup::Menus, FixText::NamesEndingInS, [](FGameFixes& F) -> bool& { return F.Menu.bNamesKeepTrailingS; }},
			{FixGroup::Menus, FixText::MemberChangeEp, [](FGameFixes& F) -> bool& { return F.Menu.bMemberChangeSaysEp; }},
			{FixGroup::Menus, FixText::ShopPointer, [](FGameFixes& F) -> bool& { return F.Menu.bShopPointerBesideWords; }},
			{FixGroup::Menus, FixText::GearShopParts, [](FGameFixes& F) -> bool& { return F.Menu.bGearShopPartsAsMenu; }},
			{FixGroup::Menus, FixText::GearShopPace, [](FGameFixes& F) -> bool& { return F.Menu.bGearModelFieldPace; }},
			{FixGroup::Menus, FixText::SellingEquipped, [](FGameFixes& F) -> bool& { return F.Menu.bSaleRecomputesEquipment; }},
			{FixGroup::Menus, FixText::NewGameClock, [](FGameFixes& F) -> bool& { return F.Menu.bNewGameClockFromZero; }},
			{FixGroup::Battle, FixText::InfinityChance, [](FGameFixes& F) -> bool& { return F.BattleRules.bInfinityByDamageAlone; }},
			{FixGroup::Battle, FixText::ItemBuffs, [](FGameFixes& F) -> bool& { return F.BattleRules.bItemBuffsByItsUser; }},
			{FixGroup::Battle, FixText::EpAtLevel99, [](FGameFixes& F) -> bool& { return F.BattleRules.bEpGrowsAtLastLevel; }},
			{FixGroup::Battle, FixText::EtherBuffs, [](FGameFixes& F) -> bool& { return F.BattleRules.bEtherBuffsSetDirectly; }},
			{FixGroup::Battle, FixText::BuffsWearOff, [](FGameFixes& F) -> bool& { return F.BattleRules.bEveryBuffExpires; }},
			{FixGroup::Battle, FixText::DefUpOnFoot, [](FGameFixes& F) -> bool& { return F.BattleRules.bDefUpOnFoot; }},
			{FixGroup::Battle, FixText::DefDownOnFoot, [](FGameFixes& F) -> bool& { return F.BattleRules.bDefDownOnFoot; }},
			{FixGroup::Battle, FixText::MissedStatusMoves, [](FGameFixes& F) -> bool& { return F.BattleRules.bFailedStatusKeepsCount; }},
			{FixGroup::Battle, FixText::ImmunityItems, [](FGameFixes& F) -> bool& { return F.BattleRules.bImmunityItemsWork; }},
			{FixGroup::Battle, FixText::GearElements, [](FGameFixes& F) -> bool& { return F.BattleRules.bGearElementsExpire; }},
			{FixGroup::Battle, FixText::GearStop, [](FGameFixes& F) -> bool& { return F.BattleRules.bGearStopEnds; }},
			{FixGroup::Battle, FixText::GearPowerLoss, [](FGameFixes& F) -> bool& { return F.BattleRules.bGearPowerLoss; }},
			{FixGroup::Battle, FixText::LargeGearNumbers, [](FGameFixes& F) -> bool& { return F.BattleRules.bWholeAmounts; }},
			{FixGroup::Battle, FixText::Counters, [](FGameFixes& F) -> bool& { return F.BattleRules.bCounterKeepsGroupAttack; }},
			{FixGroup::Battle, FixText::LastBossHp, [](FGameFixes& F) -> bool& { return F.BattleRules.bDeusSecondFormHp; }},
			{FixGroup::Battle, FixText::DeusFirstFight, [](FGameFixes& F) -> bool& { return F.BattleRules.bDeusKeepsCamera; }},
			{FixGroup::Battle, FixText::EtherGuns, [](FGameFixes& F) -> bool& { return F.BattleRules.bEtherAmmunitionElement; }},
			{FixGroup::Battle, FixText::HiddenEnemies, [](FGameFixes& F) -> bool& { return F.BattleRules.bRevealKeepsGroup; }},
			{FixGroup::Battle, FixText::WeaponTrails, [](FGameFixes& F) -> bool& { return F.BattleShow.bTrailsBlend; }},
			{FixGroup::Battle, FixText::SpellLighting, [](FGameFixes& F) -> bool& { return F.BattleShow.bUnlitModelsTakeStageLight; }},
			{FixGroup::Battle, FixText::StageColors, [](FGameFixes& F) -> bool& { return F.BattleShow.bTintKeepsStagePalettes; }},
			{FixGroup::Battle, FixText::BattleEvents, [](FGameFixes& F) -> bool& { return F.BattleEvents.bWaitedStartNamesActor; }},
			{FixGroup::Battle, FixText::EventDefeats, [](FGameFixes& F) -> bool& { return F.BattleEvents.bUnscriptedDefeatIsLost; }},
			{FixGroup::WorldMap, FixText::Landmarks, [](FGameFixes& F) -> bool& { return F.WorldMap.bStoryObjectBackAfterBattle; }},
			{FixGroup::WorldMap, FixText::SeaBlastSound, [](FGameFixes& F) -> bool& { return F.WorldMap.bScene14SweepHeard; }},
			{FixGroup::WorldMap, FixText::MahanonFalls, [](FGameFixes& F) -> bool& { return F.WorldMap.bScene18EffectsWait; }},
			{FixGroup::WorldMap, FixText::OverlappingTrees, [](FGameFixes& F) -> bool& { return F.WorldMap.bTreesInDepthOrder; }},
			{FixGroup::Arena, FixText::CpuRangedMoves, [](FGameFixes& F) -> bool& { return F.Battling.bRangedMovesByOwnState; }},
			{FixGroup::Arena, FixText::PracticeOpponents, [](FGameFixes& F) -> bool& { return F.Battling.bPracticeComputerAtStyleLevel; }},
			{FixGroup::Arena, FixText::PracticeFrameRate, [](FGameFixes& F) -> bool& { return F.Battling.bPracticeFrameRateAsLabelled; }},
		}};

		/** A fix's switch as the fixes have it (the table's accessors take the fixes to change). */
		bool IsOn(const FFixEntry& Fix, const FGameFixes& Of)
		{
			return Fix.Switch(const_cast<FGameFixes&>(Of));
		}

		/** The Custom page's rows: a heading before each group (-1 - its first fix), then each fix by its index. */
		const std::vector<int32>& CustomRows()
		{
			static const std::vector<int32> Rows = []
			{
				std::vector<int32> Made;
				for (int32 Fix = 0; Fix < static_cast<int32>(FixTable.size()); ++Fix)
				{
					if (Fix == 0 || FixTable[static_cast<std::size_t>(Fix)].Group != FixTable[static_cast<std::size_t>(Fix - 1)].Group)
					{
						Made.push_back(-1 - Fix);
					}
					Made.push_back(Fix);
				}
				return Made;
			}();
			return Rows;
		}

		using SettingsRows::ChoiceName;
		using SettingsRows::Heading;
		using SettingsRows::OnOff;
		using SettingsRows::Shown;
		using SettingsRows::StepChoice;

		namespace Modding
		{
			enum : int32 { Balance, Experience, Gold, Encounters, Count };
			constexpr int32 RewardRates = 5;
			constexpr int32 EncounterRates = 5;
		}

		namespace PerfectWorks
		{
			enum : int32 { Notice, Enabled, Story, Script, StoryMode, Music, Gameplay, Party, Enemies, Arena, Graphics, Portraits, Roni, Emeralda, TitleScreen, Count };
			constexpr int32 PortraitChoices = 3;
			constexpr int32 ArenaChoices = 3;

			/** A part's value: the mod's (Values' first) or the game's own. */
			std::string Taken(bool bMods, const std::array<std::string_view, 2>& Values)
			{
				return std::string(Values[bMods ? 0 : 1]);
			}
		}

		namespace Display
		{
			enum : int32 { FullScreen, WindowSize, Aspect, Sorting, SceneryFilter, SpriteFilter, MovieFilter, Crt, AntiAliasing, Culling, Pilots, Gears, ChapterCards, TextSpeed, BattleSpeed,
				BattleFlashes, BackgroundPause, Count };

			/** The window's rows (full screen, its size) are not there where the game has no window of its own: the rows after them move up. */
			int32 Hidden(const FDisplayModes& Modes)
			{
				return Modes.WindowSizes.empty() ? Aspect : 0;
			}

			/** The size shown: the player's choice, the window's own until there is one. */
			FDisplaySize ShownSize(const FPlayerSettings& Settings, const FDisplayModes& Modes)
			{
				return Settings.Display.WindowSize.IsSet() ? Settings.Display.WindowSize : Modes.Window;
			}
		}

		namespace Crt
		{
			enum : int32 { Scanlines, Mask, MaskStrength, MaskSize, Sharpness, ColorBleed, Glow, Curvature, Corners, Vignette, Count };

			const OptionsText::FRow& Words(int32 Row)
			{
				namespace Text = OptionsText::Crt;
				static constexpr std::array<const OptionsText::FRow*, Count> Rows{&Text::Scanlines, &Text::Mask, &Text::MaskStrength, &Text::MaskSize,
					&Text::Sharpness, &Text::ColorBleed, &Text::Glow, &Text::Curvature, &Text::Corners, &Text::Vignette};
				return *Rows[static_cast<std::size_t>(Row)];
			}

			/** The look's step on Row, for the parts that are steps (none for a choice's row). */
			int32* StepOf(FCrtLook& Look, int32 Row)
			{
				switch (Row)
				{
				case Scanlines:
					return &Look.Scanlines;
				case MaskStrength:
					return &Look.MaskStrength;
				case Sharpness:
					return &Look.Sharpness;
				case ColorBleed:
					return &Look.ColorBleed;
				case Glow:
					return &Look.Glow;
				case Curvature:
					return &Look.Curvature;
				case Corners:
					return &Look.Corners;
				case Vignette:
					return &Look.Vignette;
				default:
					return nullptr;
				}
			}
		}

		namespace Camera
		{
			enum : int32 { Zoom, Tilt, YieldsToScenes, Follow, Reset, Count };
			/** The zoom in whole percent, a step of 5; the tilt in whole degrees. */
			constexpr int32 ZoomStep = 5;
		}

		int32 ZoomPercent(float Zoom)
		{
			return static_cast<int32>(std::lround(Zoom * 100.0f));
		}
	}

	std::string_view FSettingsPages::Title(ESettingsPage Page)
	{
		switch (Page)
		{
		case ESettingsPage::Display:
			return OptionsText::Titles::Display;
		case ESettingsPage::Sound:
			return OptionsText::Titles::Sound;
		case ESettingsPage::Camera:
			return OptionsText::Titles::Camera;
		case ESettingsPage::Fixes:
			return OptionsText::Titles::Fixes;
		case ESettingsPage::Custom:
			return OptionsText::Titles::CustomFixes;
		case ESettingsPage::Crt:
			return OptionsText::Titles::Crt;
		case ESettingsPage::Modding:
			return OptionsText::Titles::Modding;
		case ESettingsPage::PerfectWorks:
			return OptionsText::Titles::PerfectWorks;
		}
		return {};
	}

	int32 FSettingsPages::RowCount(ESettingsPage Page, const FSettingsContext& Context)
	{
		switch (Page)
		{
		case ESettingsPage::Display:
			return Display::Count - Display::Hidden(Context.Display);
		case ESettingsPage::Sound:
			return SoundPage::RowCount(Context.Sound);
		case ESettingsPage::Camera:
			return Camera::Count;
		case ESettingsPage::Fixes:
			return 1;
		case ESettingsPage::Custom:
			return static_cast<int32>(CustomRows().size());
		case ESettingsPage::Crt:
			return Crt::Count;
		case ESettingsPage::Modding:
			return Modding::Count;
		case ESettingsPage::PerfectWorks:
			return PerfectWorks::Count;
		}
		return 0;
	}

	FSettingsRow FSettingsPages::Row(ESettingsPage Page, int32 Row, const FPlayerSettings& Settings, bool bCustomPicked, const FSettingsContext& Context)
	{
		namespace Text = OptionsText;
		const FDisplayModes& Modes = Context.Display;
		switch (Page)
		{
		case ESettingsPage::Display:
			switch (Row + Display::Hidden(Modes))
			{
			case Display::FullScreen:
				return Shown(Text::Display::FullScreen, OnOff(Settings.Display.bFullScreen));
			case Display::WindowSize:
			{
				const FDisplaySize Size = Display::ShownSize(Settings, Modes);
				return Shown(Text::Display::Resolution, Text::Fill(Text::Display::ResolutionValue, {std::to_string(Size.Width), std::to_string(Size.Height)}));
			}
			case Display::Aspect:
				return Shown(Text::Display::Aspect, ChoiceName(Settings.Display.Aspect, Text::Display::AspectValues));
			case Display::Sorting:
				return Shown(Text::Display::Sorting, ChoiceName(Settings.Field.Sorting, Text::Display::SortingValues));
			case Display::SceneryFilter:
				return Shown(Text::Display::SceneryFilter, ChoiceName(Settings.Field.Filter, Text::Display::FilterValues));
			case Display::SpriteFilter:
				return Shown(Text::Display::SpriteFilter, ChoiceName(Settings.Field.SpriteFilter, Text::Display::FilterValues));
			case Display::MovieFilter:
				return Shown(Text::Display::MovieFilter, ChoiceName(Settings.Field.MovieFilter, Text::Display::MovieFilterValues));
			case Display::Crt:
				return Shown(Text::Display::Crt, ChoiceName(Settings.Display.Crt, Text::Display::CrtValues));
			case Display::AntiAliasing:
				return Shown(Text::Display::AntiAliasing, ChoiceName(Settings.Field.AntiAliasing, Text::Display::AntiAliasingValues));
			case Display::Culling:
				return Shown(Text::Display::Culling, ChoiceName(Settings.Field.Culling, Text::Display::CullingValues));
			case Display::Pilots:
				return Shown(Text::Display::Pilots, std::string(Text::Display::PilotsValues[Settings.Field.bDetailedPilots ? 0 : 1]));
			case Display::Gears:
				return Shown(Text::Display::Gears, ChoiceName(Settings.Field.GearModels, Text::Display::GearsValues));
			case Display::ChapterCards:
				return Shown(Text::Display::ChapterCards, OnOff(Settings.bChapterCards));
			case Display::TextSpeed:
				return Shown(Text::Display::TextSpeed, ChoiceName(Settings.TextSpeed, Text::Display::TextSpeedValues));
			case Display::BattleSpeed:
				return Shown(Text::Display::BattleSpeed, ChoiceName(Settings.BattleSpeed, Text::Display::BattleSpeedValues));
			case Display::BattleFlashes:
				return Shown(Text::Display::BattleFlashes, ChoiceName(Settings.Field.BattleFlashes, Text::Display::BattleFlashesValues));
			default:
				return Shown(Text::Display::PauseInBackground, OnOff(Settings.Display.bPauseInBackground));
			}
		case ESettingsPage::Modding:
			switch (Row)
			{
			case Modding::Experience:
				return Shown(Text::Modding::Experience, ChoiceName(Settings.Balance.Experience, Text::Modding::RewardValues));
			case Modding::Gold:
				return Shown(Text::Modding::Gold, ChoiceName(Settings.Balance.Gold, Text::Modding::RewardValues));
			case Modding::Encounters:
				return Shown(Text::Modding::Encounters, ChoiceName(Settings.Balance.Encounters, Text::Modding::EncounterValues));
			default:
				return Heading(Text::Modding::Balance);
			}
		case ESettingsPage::PerfectWorks:
		{
			namespace Words = Text::PerfectWorks;
			const FPerfectWorksOptions& Mod = Settings.PerfectWorks;
			switch (Row)
			{
			case PerfectWorks::Notice:
			{
				FSettingsRow Notice;
				Notice.Label = std::string(Words::Notice);
				Notice.bReadOnly = true;
				return Notice;
			}
			case PerfectWorks::Enabled:
			{
				FSettingsRow Enabled = Shown(Words::Enabled, OnOff(Mod.bEnabled));
				if (Context.bDiscsPicked)
				{
					Enabled.Help = Text::Fill(Words::EnabledPicked);
				}
				return Enabled;
			}
			case PerfectWorks::Story:
				return Heading(Words::Story);
			case PerfectWorks::Script:
				return Shown(Words::Script, PerfectWorks::Taken(Mod.bScript, Words::ModValues));
			case PerfectWorks::StoryMode:
				return Shown(Words::StoryMode, OnOff(Mod.bStoryMode));
			case PerfectWorks::Music:
				return Shown(Words::Music, PerfectWorks::Taken(Mod.bMusic, Words::ModValues));
			case PerfectWorks::Gameplay:
				return Heading(Words::Gameplay);
			case PerfectWorks::Party:
				return Shown(Words::Party, PerfectWorks::Taken(Mod.bRebalancedParty, Words::ModValues));
			case PerfectWorks::Enemies:
				return Shown(Words::Enemies, PerfectWorks::Taken(Mod.bRebalancedEnemies, Words::ModValues));
			case PerfectWorks::Arena:
				return Shown(Words::Arena, ChoiceName(Mod.Arena, Words::ArenaValues));
			case PerfectWorks::Graphics:
				return Heading(Words::Graphics);
			case PerfectWorks::Portraits:
				return Shown(Words::Portraits, ChoiceName(Mod.Portraits, Words::PortraitValues));
			case PerfectWorks::Roni:
				return Shown(Words::Roni, PerfectWorks::Taken(Mod.bRoniPortrait, Words::ModValues));
			case PerfectWorks::Emeralda:
				return Shown(Words::Emeralda, PerfectWorks::Taken(Mod.bEmeraldaCafe, Words::FixValues));
			default:
				return Shown(Words::TitleScreen, PerfectWorks::Taken(Mod.bTitleScreen, Words::ModValues));
			}
		}
		case ESettingsPage::Sound:
			return SoundPage::Row(Row, Settings, Context.Sound, Context.bDiscsPicked);
		case ESettingsPage::Camera:
			switch (Row)
			{
			case Camera::Zoom:
				return Shown(Text::Camera::Zoom, Text::Fill(Text::Camera::ZoomValue, {std::to_string(ZoomPercent(Settings.Camera.Zoom))}));
			case Camera::Tilt:
			{
				const int32 Degrees = static_cast<int32>(std::lround(Settings.Camera.Tilt));
				return Shown(Text::Camera::Tilt, (Degrees > 0 ? "+" : "") + std::to_string(Degrees));
			}
			case Camera::YieldsToScenes:
				return Shown(Text::Camera::Scenes, std::string(Text::Camera::ScenesValues[Settings.Camera.bYieldsToScenes ? 0 : 1]));
			case Camera::Follow:
				return Shown(Text::Camera::Follow, std::string(Text::Camera::FollowValues[Settings.Camera.bSteadyFollow ? 0 : 1]));
			default:
				return Shown(Text::Camera::Reset, "");
			}
		case ESettingsPage::Crt:
		{
			FCrtLook Look = ShownCrtLook(Settings.Display);
			switch (Row)
			{
			case Crt::Mask:
				return Shown(Text::Crt::Mask, ChoiceName(Look.Mask, Text::Crt::MaskValues));
			case Crt::MaskSize:
				return Shown(Text::Crt::MaskSize, ChoiceName(Look.MaskSize, Text::Crt::MaskSizeValues));
			default:
				return Shown(Crt::Words(Row), std::to_string(*Crt::StepOf(Look, Row)));
			}
		}
		case ESettingsPage::Fixes:
		{
			const EFixesPreset Preset = bCustomPicked ? EFixesPreset::Custom : FSettingsPages::Preset(Settings.Fixes);
			const std::size_t Index = static_cast<std::size_t>(Preset);
			return {std::string(Text::Fixes::Label), std::string(Text::Fixes::PresetValues[Index]), Text::Fill(Text::Fixes::PresetHelp[Index])};
		}
		case ESettingsPage::Custom:
		{
			const int32 Entry = CustomRows()[static_cast<std::size_t>(Row)];
			if (Entry < 0)
			{
				return Heading(FixTable[static_cast<std::size_t>(-1 - Entry)].Group);
			}
			const FFixEntry& Fix = FixTable[static_cast<std::size_t>(Entry)];
			return Shown(Fix.Text, std::string(IsOn(Fix, Settings.Fixes) ? Text::Fixes::FixOn : Text::Fixes::FixOff));
		}
		}
		return {};
	}

	bool FSettingsPages::Step(ESettingsPage Page, int32 Row, int32 Direction, FPlayerSettings& Settings, bool& bCustomPicked, const FSettingsContext& Context)
	{
		const FDisplayModes& Modes = Context.Display;
		switch (Page)
		{
		case ESettingsPage::Display:
			switch (Row + Display::Hidden(Modes))
			{
			case Display::FullScreen:
				Settings.Display.bFullScreen = !Settings.Display.bFullScreen;
				return true;
			case Display::WindowSize:
			{
				// The size shown need not be one of the display's: the next one past it, round to the other end past the last.
				const std::vector<FDisplaySize>& Sizes = Modes.WindowSizes;
				const FDisplaySize Shown = Display::ShownSize(Settings, Modes);
				FDisplaySize Next = Direction > 0 ? Sizes.front() : Sizes.back();
				if (Direction > 0)
				{
					const auto Larger = std::upper_bound(Sizes.begin(), Sizes.end(), Shown);
					Next = Larger != Sizes.end() ? *Larger : Next;
				}
				else
				{
					const auto NotSmaller = std::lower_bound(Sizes.begin(), Sizes.end(), Shown);
					Next = NotSmaller != Sizes.begin() ? *(NotSmaller - 1) : Next;
				}
				if (Next == Settings.Display.WindowSize)
				{
					return false;
				}
				Settings.Display.WindowSize = Next;
				return true;
			}
			case Display::Aspect:
				return StepChoice(Settings.Display.Aspect, Direction, 3);
			case Display::Sorting:
				return StepChoice(Settings.Field.Sorting, Direction, 3);
			case Display::SceneryFilter:
				return StepChoice(Settings.Field.Filter, Direction, 4);
			case Display::SpriteFilter:
				return StepChoice(Settings.Field.SpriteFilter, Direction, 4);
			case Display::MovieFilter:
				return StepChoice(Settings.Field.MovieFilter, Direction, 2);
			case Display::Crt:
				return StepChoice(Settings.Display.Crt, Direction, 4);
			case Display::AntiAliasing:
				return StepChoice(Settings.Field.AntiAliasing, Direction, 3);
			case Display::Culling:
				return StepChoice(Settings.Field.Culling, Direction, 2);
			case Display::Pilots:
				Settings.Field.bDetailedPilots = !Settings.Field.bDetailedPilots;
				return true;
			case Display::Gears:
				return StepChoice(Settings.Field.GearModels, Direction, 3);
			case Display::ChapterCards:
				Settings.bChapterCards = !Settings.bChapterCards;
				return true;
			case Display::TextSpeed:
				return StepChoice(Settings.TextSpeed, Direction, 4);
			case Display::BattleSpeed:
				return StepChoice(Settings.BattleSpeed, Direction, BattleSpeedCount);
			case Display::BattleFlashes:
				return StepChoice(Settings.Field.BattleFlashes, Direction, 3);
			default:
				Settings.Display.bPauseInBackground = !Settings.Display.bPauseInBackground;
				return true;
			}
		case ESettingsPage::Modding:
			switch (Row)
			{
			case Modding::Experience:
				return StepChoice(Settings.Balance.Experience, Direction, Modding::RewardRates);
			case Modding::Gold:
				return StepChoice(Settings.Balance.Gold, Direction, Modding::RewardRates);
			case Modding::Encounters:
				return StepChoice(Settings.Balance.Encounters, Direction, Modding::EncounterRates);
			default:
				return false;
			}
		case ESettingsPage::PerfectWorks:
		{
			FPerfectWorksOptions& Mod = Settings.PerfectWorks;
			const auto Toggle = [](bool& bOn)
			{
				bOn = !bOn;
				return true;
			};
			switch (Row)
			{
			case PerfectWorks::Enabled:
				return Toggle(Mod.bEnabled);
			case PerfectWorks::Script:
				return Toggle(Mod.bScript);
			// Story mode and the rebalances (the arena's too) exclude each other, as in the mod's patcher: choosing one ends the other.
			case PerfectWorks::StoryMode:
				Toggle(Mod.bStoryMode);
				Mod.bRebalancedParty = Mod.bRebalancedParty && !Mod.bStoryMode;
				Mod.bRebalancedEnemies = Mod.bRebalancedEnemies && !Mod.bStoryMode;
				Mod.Arena = Mod.bStoryMode ? EPerfectWorksArena::Original : Mod.Arena;
				return true;
			case PerfectWorks::Music:
				return Toggle(Mod.bMusic);
			case PerfectWorks::Party:
				Toggle(Mod.bRebalancedParty);
				Mod.bStoryMode = Mod.bStoryMode && !Mod.bRebalancedParty;
				return true;
			case PerfectWorks::Enemies:
				Toggle(Mod.bRebalancedEnemies);
				Mod.bStoryMode = Mod.bStoryMode && !Mod.bRebalancedEnemies;
				return true;
			case PerfectWorks::Arena:
				StepChoice(Mod.Arena, Direction, PerfectWorks::ArenaChoices);
				Mod.bStoryMode = Mod.bStoryMode && Mod.Arena == EPerfectWorksArena::Original;
				return true;
			case PerfectWorks::Portraits:
				return StepChoice(Mod.Portraits, Direction, PerfectWorks::PortraitChoices);
			case PerfectWorks::Roni:
				return Toggle(Mod.bRoniPortrait);
			case PerfectWorks::Emeralda:
				return Toggle(Mod.bEmeraldaCafe);
			case PerfectWorks::TitleScreen:
				return Toggle(Mod.bTitleScreen);
			default:
				return false;
			}
		}
		case ESettingsPage::Sound:
			return SoundPage::Step(Row, Direction, Settings, Context.Sound);
		case ESettingsPage::Crt:
		{
			// The look shown becomes the player's own, the change made to it.
			FCrtLook Look = ShownCrtLook(Settings.Display);
			if (Row == Crt::Mask)
			{
				StepChoice(Look.Mask, Direction, 3);
			}
			else if (Row == Crt::MaskSize)
			{
				StepChoice(Look.MaskSize, Direction, 3);
			}
			else
			{
				int32& Step = *Crt::StepOf(Look, Row);
				const int32 Next = std::clamp(Step + Direction, 0, FCrtLook::Steps);
				if (Next == Step)
				{
					return false;
				}
				Step = Next;
			}
			Settings.Display.CustomCrt = Look;
			Settings.Display.Crt = ECrtFilter::Custom;
			return true;
		}
		case ESettingsPage::Camera:
		{
			FFieldCameraOffsets& Camera = Settings.Camera;
			switch (Row)
			{
			case Camera::Zoom:
			{
				const int32 MinPercent = ZoomPercent(FFieldCameraOffsets::MinZoom);
				const int32 MaxPercent = ZoomPercent(FFieldCameraOffsets::MaxZoom);
				// A zoom between steps (the mouse's wheel) goes to the step on the side asked for.
				const int32 Percent = ZoomPercent(Camera.Zoom);
				const int32 Below = Percent / Camera::ZoomStep * Camera::ZoomStep;
				const int32 Next = Direction > 0 ? Below + Camera::ZoomStep : (Below == Percent ? Below - Camera::ZoomStep : Below);
				const int32 Stepped = std::clamp(Next, MinPercent, MaxPercent);
				if (Stepped == Percent)
				{
					return false;
				}
				Camera.Zoom = static_cast<float>(Stepped) / 100.0f;
				return true;
			}
			case Camera::Tilt:
			{
				const float Stepped = std::clamp(std::round(Camera.Tilt) + static_cast<float>(Direction), FFieldCameraOffsets::MinTilt, FFieldCameraOffsets::MaxTilt);
				if (Stepped == Camera.Tilt)
				{
					return false;
				}
				Camera.Tilt = Stepped;
				return true;
			}
			case Camera::YieldsToScenes:
				Camera.bYieldsToScenes = !Camera.bYieldsToScenes;
				return true;
			case Camera::Follow:
				Camera.bSteadyFollow = !Camera.bSteadyFollow;
				return true;
			default:
				return false;
			}
		}
		case ESettingsPage::Fixes:
		{
			const EFixesPreset Shown = bCustomPicked ? EFixesPreset::Custom : Preset(Settings.Fixes);
			const EFixesPreset Next = static_cast<EFixesPreset>((static_cast<int32>(Shown) + Direction + 3) % 3);
			ApplyPreset(Next, Settings.Fixes);
			bCustomPicked = Next == EFixesPreset::Custom;
			return true;
		}
		case ESettingsPage::Custom:
		{
			const int32 Entry = CustomRows()[static_cast<std::size_t>(Row)];
			if (Entry < 0)
			{
				return false;
			}
			bool& Switch = FixTable[static_cast<std::size_t>(Entry)].Switch(Settings.Fixes);
			Switch = !Switch;
			return true;
		}
		}
		return false;
	}

	ESettingsAction FSettingsPages::Activate(ESettingsPage Page, int32 Row, FPlayerSettings& Settings, bool bCustomPicked, const FSettingsContext& Context)
	{
		if (Page == ESettingsPage::Display && Row + Display::Hidden(Context.Display) == Display::Crt)
		{
			return ESettingsAction::OpenCrt;
		}
		if (Page == ESettingsPage::Camera && Row == Camera::Reset)
		{
			Settings.Camera = FFieldCameraOffsets::Defaults();
			return ESettingsAction::Reset;
		}
		if (Page == ESettingsPage::Fixes && (bCustomPicked || Preset(Settings.Fixes) == EFixesPreset::Custom))
		{
			return ESettingsAction::OpenCustom;
		}
		return ESettingsAction::None;
	}

	EFixesPreset FSettingsPages::Preset(const FGameFixes& Fixes)
	{
		FGameFixes Enhanced;
		FGameFixes Original;
		ApplyPreset(EFixesPreset::Original, Original);
		const auto Matches = [&Fixes](const FGameFixes& Preset)
		{
			return std::all_of(FixTable.begin(), FixTable.end(), [&](const FFixEntry& Fix) { return IsOn(Fix, Fixes) == IsOn(Fix, Preset); });
		};
		if (Matches(Enhanced))
		{
			return EFixesPreset::PortEnhancements;
		}
		return Matches(Original) ? EFixesPreset::Original : EFixesPreset::Custom;
	}

	void FSettingsPages::ApplyPreset(EFixesPreset Preset, FGameFixes& Fixes)
	{
		if (Preset == EFixesPreset::Custom)
		{
			return;
		}
		const FGameFixes Enhanced;
		for (const FFixEntry& Fix : FixTable)
		{
			Fix.Switch(Fixes) = Preset == EFixesPreset::PortEnhancements && IsOn(Fix, Enhanced);
		}
	}

	int32 FSettingsPages::FixCount()
	{
		return static_cast<int32>(FixTable.size());
	}
}
