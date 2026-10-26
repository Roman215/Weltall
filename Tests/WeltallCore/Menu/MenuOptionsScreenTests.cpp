#include "Game/PlayerSettings.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuPlay.h"
#include "Menu/MenuAssetPages.h"
#include "Menu/MenuDeviceFiles.h"
#include "Support/GameDiscs.h"
#include "Support/PressedKeys.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <vector>

using namespace Weltall;

// The options screen (the port's) as a menu opened from a disc opens it: from the title's first orb and the Normal
// Menu's eighth, its changes handed on as they are made and kept as it closes.
namespace
{
	class FRecordedSettings final : public IMenuSettings
	{
	public:
		const FPlayerSettings& Settings() const override { return Current; }
		void Change(const FPlayerSettings& Settings) override
		{
			Current = Settings;
			++Changes;
		}
		void Keep() override { Kept.push_back(Current); }
		FDisplayModes DisplayModes() const override { return Display; }
		IMenuAssets* Assets() const override { return AssetExport; }
		IMenuDeviceFiles* DeviceFiles() const override { return Files; }
		FMenuMods Mods() const override { return Found; }
		FSoundSources SoundSources() const override { return Sources; }

		FPlayerSettings Current;
		FDisplayModes Display;
		int32 Changes = 0;
		std::vector<FPlayerSettings> Kept;
		IMenuAssets* AssetExport = nullptr;
		IMenuDeviceFiles* Files = nullptr;
		FMenuMods Found;
		FSoundSources Sources;
	};

	FPadState Holding(EPadButton Button)
	{
		return {FPadState::Bit(Button)};
	}

	bool Drawn(const TResult<bool>& Frame)
	{
		if (!Frame)
		{
			FAIL(Frame.Error().Message);
		}
		return *Frame;
	}

	/** A button pressed for a frame and let go for a few, the menu running on. */
	void Press(FMenuPlay& Menu, EPadButton Button, int32 Frames = 8)
	{
		REQUIRE(Drawn(Menu.Frame(Holding(Button))));
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	}

	void Wait(FMenuPlay& Menu, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	}
}

TEST_CASE("The Assets page changes replacements and starts, refreshes and cancels an export", "[Menu][Content][Assets]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	class FExporter final : public IMenuAssets
	{
	public:
		FMenuAssetProgress Progress() const override { return Current; }
		bool StartExport(EAssetExportKind Kind) override
		{
			++Starts;
			Started = Kind;
			Current = {EAssetExportState::Running, Kind, 0, 100, Current.Revision + 1};
			return true;
		}
		void CancelExport() override { ++Cancels; Current.State = EAssetExportState::Cancelled; ++Current.Revision; }
		FMenuAssetProgress Current;
		EAssetExportKind Started = EAssetExportKind::Dump;
		int32 Starts = 0;
		int32 Cancels = 0;
	} Exporter;
	FRecordedSettings Settings;
	Settings.AssetExport = &Exporter;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// L1 from Display turns to the last page, Modding; past its balance rows and the mods' heading to Assets, opened.
	Press(Menu, EPadButton::L1);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.bExternalAssets);
	CHECK(Settings.Current.Balance == FGameBalance{});
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.bCaptureTextures);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	CHECK(Exporter.Starts == 1);
	CHECK(Exporter.Started == EAssetExportKind::Textures);
	Exporter.Current.Completed = 75;
	++Exporter.Current.Revision;
	Wait(Menu, 4);
	Press(Menu, EPadButton::Cross);
	CHECK(Exporter.Cancels == 1);
	// Circle back to Modding's Assets row, where Right changes nothing, and again to close.
	Press(Menu, EPadButton::Circle);
	CHECK(Settings.Kept.empty());
	const int32 Changes = Settings.Changes;
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Changes == Changes);
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().bExternalAssets);
}

TEST_CASE("The Modding page sets the balance, and without the Assets page has no rows after it", "[Menu][Content][Balance]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// L1 from Display: Modding, the cursor on Experience under the Balance heading; Right twice makes it 2x.
	Press(Menu, EPadButton::L1);
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Balance.Experience == ERewardRate::Double);
	// Down to Gold, Left: 4x; Down to Encounters, Left: half as many.
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Balance.Gold == ERewardRate::Quadruple);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Balance.Encounters == EEncounterRate::Half);
	// Nothing below it without the Assets page: Down stays, and Left steps Encounters again.
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Balance.Encounters == EEncounterRate::Quarter);
	// Confirm does nothing on a balance row; R1 turns on round to Display.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::R1);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Balance == FGameBalance{ERewardRate::Double, ERewardRate::Quadruple, EEncounterRate::Quarter});
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().Balance.Gold == ERewardRate::Quadruple);
}

TEST_CASE("A mod found has a row on the Modding page that switches it and opens its page", "[Menu][Content][Mods]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	Settings.Found = {true, "0.11.4", {}};
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// L1 to Modding; past the balance rows and the mods' heading to Perfect Works, which Right switches on.
	Press(Menu, EPadButton::L1);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.PerfectWorks.bEnabled);
	CHECK(Settings.Current.Balance == FGameBalance{});
	// Cross opens its page on its switch; Down past the Story heading to Script, which Left sets to the original's.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Left);
	CHECK_FALSE(Settings.Current.PerfectWorks.bScript);
	// Circle back to the mod's row, where Right switches it off; again to close.
	Press(Menu, EPadButton::Circle);
	Press(Menu, EPadButton::Right);
	CHECK_FALSE(Settings.Current.PerfectWorks.bEnabled);
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK_FALSE(Settings.Kept.front().PerfectWorks.bScript);
}

TEST_CASE("On a device, the Modding page has External assets in the Assets row's place, then the device's mods, the cursor kept as rows come above", "[Menu][Content][Mods]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	class FNoExports final : public IMenuAssets
	{
	public:
		FMenuAssetProgress Progress() const override { return {}; }
		bool StartExport(EAssetExportKind) override { return false; }
		void CancelExport() override {}
	} Exports;
	class FDevice final : public IMenuDeviceFiles
	{
	public:
		FTransferStatus Shown;
		std::vector<EDeviceTransfer> Started;
		std::vector<std::filesystem::path> Removed;
		FTransferStatus Status() const override { return Shown; }
		bool Start(EDeviceTransfer Kind, const std::filesystem::path& = {}) override
		{
			Started.push_back(Kind);
			Shown.Kind = Kind;
			Shown.State = ETransferState::Picking;
			++Shown.Revision;
			return true;
		}
		std::vector<FModItem> Mods() const override
		{
			FModItem Pack;
			Pack.Kind = EModItemKind::Textures;
			Pack.Path = "Assets/textures/HD";
			Pack.Name = "HD";
			return {Pack};
		}
		EModRemoval RemoveMod(const std::filesystem::path& Path) override
		{
			Removed.push_back(Path);
			return EModRemoval::Removed;
		}
	} Device;
	FRecordedSettings Settings;
	Settings.AssetExport = &Exports;
	Settings.Files = &Device;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// L1 to Modding; past the balance rows and the mods' heading to External assets, which Right and Confirm switch.
	Press(Menu, EPadButton::L1);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.bExternalAssets);
	Press(Menu, EPadButton::Cross);
	CHECK_FALSE(Settings.Current.bExternalAssets);
	// Down past the device's heading to Add from a zip.
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	CHECK(Device.Started == std::vector<EDeviceTransfer>{EDeviceTransfer::ModZip});
	// Perfect Works added: its row comes above, the cursor stays on Add from a zip.
	Device.Shown.State = ETransferState::Done;
	Device.Shown.Added = FAddedMod{EAddedContent::PerfectWorks, "0.11.4", 4428, false};
	++Device.Shown.Revision;
	Settings.Found = {true, "0.11.4", {}};
	Wait(Menu, 4);
	Press(Menu, EPadButton::Cross);
	CHECK(Device.Started == std::vector<EDeviceTransfer>{EDeviceTransfer::ModZip, EDeviceTransfer::ModZip});
	// Added to be used: Perfect Works switched on (from the next start).
	CHECK(Settings.Current.PerfectWorks.bEnabled);
	CHECK_FALSE(Settings.Current.bExternalAssets);
	// Down to Add a folder, then the pack, removed on a second Confirm.
	Device.Shown.State = ETransferState::Cancelled;
	++Device.Shown.Revision;
	Wait(Menu, 4);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	CHECK(Device.Started.back() == EDeviceTransfer::ModFolder);
	// A texture pack added: External assets switched on at once.
	Device.Shown.State = ETransferState::Done;
	Device.Shown.Added = FAddedMod{EAddedContent::Textures, "HD", 600, false};
	++Device.Shown.Revision;
	Wait(Menu, 4);
	CHECK(Settings.Current.bExternalAssets);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	CHECK(Device.Removed.empty());
	Press(Menu, EPadButton::Cross);
	CHECK(Device.Removed == std::vector<std::filesystem::path>{"Assets/textures/HD"});
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().PerfectWorks.bEnabled);
	CHECK(Settings.Kept.front().bExternalAssets);
}

TEST_CASE("The title's first orb opens the options screen, whose changes are made at once and kept as it closes", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	// Down from Continue to the first orb, and Cross (the US discs' Confirm): the layer flies in.
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// It opens on Display; R1 turns to Sound, on its first volume past the heading, which Left lowers. Down past the
	// other volumes and the next heading to the output, which Right sets to Wide-Stereo.
	Press(Menu, EPadButton::R1);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Volumes.Master == 90);
	for (int32 Row = 0; Row < 5; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.SoundOutput == ESoundOutput::WideStereo);
	CHECK(Settings.Changes == 2);
	// L1 turns back to Display, round to Modding and on to Fixes; Right picks Original.
	Press(Menu, EPadButton::L1);
	Press(Menu, EPadButton::L1);
	Press(Menu, EPadButton::L1);
	Press(Menu, EPadButton::Right);
	CHECK_FALSE(Settings.Current.Fixes.Field.bRecoverPartyHpGivesHp);
	CHECK_FALSE(Settings.Current.Fixes.BattleRules.bDefUpOnFoot);
	// Right again: Custom, and Cross opens the fixes' list on the first fix; Left turns it back on.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Fixes.Field.bRecoverPartyHpGivesHp);
	CHECK_FALSE(Settings.Current.Fixes.Field.bZeroFrameFadeSnaps);
	CHECK(Settings.Kept.empty());
	// Circle back to Fixes, and again to close: the settings are kept once, the title's arc back.
	Press(Menu, EPadButton::Circle);
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().SoundOutput == ESoundOutput::WideStereo);
	CHECK(Settings.Kept.front().Volumes.Master == 90);
	CHECK(Settings.Kept.front().Fixes.Field.bRecoverPartyHpGivesHp);
	CHECK_FALSE(Menu.Menu().HasEnded());
	// The title goes on: Up twice to New Game, which ends it.
	Press(Menu, EPadButton::Up);
	Press(Menu, EPadButton::Up);
	REQUIRE(Drawn(Menu.Frame(Holding(EPadButton::Cross))));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 200 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	CHECK(bEnded);
}

TEST_CASE("Discs that give voices while the options screen is open change the Sound page's choices at once", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	Settings.Sources.bEnglish = true;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// Sound, down from the first volume past the others, the output's rows and the heading to the voices: English
	// alone, which Right does not change.
	Press(Menu, EPadButton::R1);
	for (int32 Row = 0; Row < 7; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.VoiceLanguage == EVoiceLanguage::Automatic);
	CHECK(Settings.Changes == 0);
	// The undub's discs added meanwhile (the host looked for voices again): Japanese is there to pick.
	Settings.Sources.bUndub = true;
	Wait(Menu, 4);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.VoiceLanguage != EVoiceLanguage::Automatic);
	CHECK(Settings.Changes == 1);
}

TEST_CASE("The CRT filter's row opens its parts, each change handed on at once, and Cancel goes back to the row", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// Display without the window's rows: down five to the CRT filter, and Right: the monitor's look.
	for (int32 Row = 0; Row < 5; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Display.Crt == ECrtFilter::Monitor);
	// Cross opens its parts; down seven to Curvature, and Right: the monitor's look a step bulging, the player's own,
	// handed on as it is made.
	Press(Menu, EPadButton::Cross);
	for (int32 Row = 0; Row < 7; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	const int32 Before = Settings.Changes;
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Changes == Before + 1);
	CHECK(Settings.Current.Display.Crt == ECrtFilter::Custom);
	FCrtLook Bulging = FCrtLook::Monitor();
	Bulging.Curvature = 1;
	CHECK(Settings.Current.Display.CustomCrt == Bulging);
	// Circle goes back to the CRT filter's row: Right steps it on from Custom, round to off.
	Press(Menu, EPadButton::Circle);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Display.Crt == ECrtFilter::Off);
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().Display.CustomCrt == Bulging);
}

TEST_CASE("The Normal Menu's eighth orb, over Status, opens the options screen and is not remembered", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	Settings.Display = {{{1280, 720}, {1920, 1080}}, {1280, 720}};
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::Normal, 0, true}, nullptr, &Settings);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 60);
	// A new game's menu opens on Status; Up is the eighth orb.
	Press(Menu, EPadButton::Up);
	Press(Menu, EPadButton::Cross, 40);
	// It opens on Display, on full screen; Down to the window's size, Right: the display's next size up from the window's;
	// Down, Left: the picture 4:3; Down twice to the scenery's texels, Right: crisp.
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Display.WindowSize == FDisplaySize{1920, 1080});
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Left);
	CHECK(Settings.Current.Display.Aspect == EDisplayAspect::Standard);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Right);
	CHECK(Settings.Current.Field.Filter == EFieldTextureFilter::Crisp);
	// R1 past Sound to Camera, Down twice and Right: the view no longer steps aside for scenes; then the reset puts it back.
	Press(Menu, EPadButton::R1);
	Press(Menu, EPadButton::R1);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Right);
	CHECK_FALSE(Settings.Current.Camera.bYieldsToScenes);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Right);
	CHECK_FALSE(Settings.Current.Camera.bSteadyFollow);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	CHECK(Settings.Current.Camera == FFieldCameraOffsets::Defaults());
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().Field.Filter == EFieldTextureFilter::Crisp);
	CHECK(Settings.Kept.front().Display == FDisplayOptions{{1920, 1080}, EDisplayAspect::Standard});
	// The menu closes on the eighth orb; a save keeps the command the original's arc has.
	REQUIRE(Drawn(Menu.Frame(Holding(EPadButton::Circle))));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 200 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	CHECK(bEnded);
	CHECK(Menu.Game().MenuLastCommand == 6);
}

TEST_CASE("The options screen's Controls page binds the key the player presses, at once, its keys drawn as their pictures", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FRecordedSettings Settings;
	Testing::FPressedKeys Keys;
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::LoadGame, 0, false}, nullptr, &Settings, &Keys);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	Wait(Menu, 30);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 40);
	// L1 from Display turns past the last page, Modding, to Controls: the device, a heading and five buttons' two keys each, the cursor on
	// Up; on to Cross.
	Press(Menu, EPadButton::L1);
	Press(Menu, EPadButton::L1);
	const auto ShownKeys = [&Menu]() { return Menu.Menu().Snapshot().Keys; };
	REQUIRE(ShownKeys().size() == 10);
	CHECK(ShownKeys()[0] == FButtonGlyph{EButtonIconSet::Keyboard, "W", "W"});
	const std::vector<FMenuPrimitive>& Primitives = Menu.Menu().Snapshot().Primitives;
	CHECK(std::count_if(Primitives.begin(), Primitives.end(), [](const FMenuPrimitive& Primitive) { return Primitive.ButtonRole >= MenuKeyRole; }) == 10);
	// The player touches a controller: its buttons are shown; back on the keyboard, its keys.
	Keys.Family = EInputFamily::Gamepad;
	Wait(Menu, 2);
	CHECK(ShownKeys()[0] == FButtonGlyph{EButtonIconSet::Xbox, "Gamepad_DPad_Up", "DPad_Up"});
	Keys.Family = EInputFamily::KeyboardAndMouse;
	Wait(Menu, 2);
	CHECK(ShownKeys()[0].Key == "W");
	for (int32 Row = 0; Row < 4; ++Row)
	{
		Press(Menu, EPadButton::Down);
	}
	// Confirm listens: the pad's buttons do nothing until a key is heard (Circle would close the screen).
	Press(Menu, EPadButton::Cross);
	CHECK(Keys.bListening);
	Press(Menu, EPadButton::Circle);
	CHECK(Keys.bListening);
	Keys.Press("Q");
	Wait(Menu, 4);
	CHECK_FALSE(Keys.bListening);
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 0) == "Q");
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Circle, EInputFamily::KeyboardAndMouse, 0) == "E");
	CHECK(Settings.Kept.empty());
	// Triangle (the menus' Square) takes Enter off Cross's second place, then cannot take off its only key.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Triangle);
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 1).empty());
	const FControlBindings Bound = Settings.Current.Controls;
	Press(Menu, EPadButton::Left);
	Press(Menu, EPadButton::Triangle);
	CHECK(Settings.Current.Controls == Bound);
	// Down to Circle, its second place: a key there in Escape's.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross);
	Keys.Press("O");
	Wait(Menu, 4);
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Circle, EInputFamily::KeyboardAndMouse, 1) == "O");
	CHECK(std::find(ShownKeys().begin(), ShownKeys().end(), FButtonGlyph{EButtonIconSet::Keyboard, "O", "O"}) != ShownKeys().end());
	// Square (the menus' Triangle) swaps the row's two keys; Triangle then clears the second, E.
	Press(Menu, EPadButton::Square);
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Circle, EInputFamily::KeyboardAndMouse, 0) == "O");
	Press(Menu, EPadButton::Triangle);
	CHECK(Settings.Current.Controls.KeyAt(EControlAction::Circle, EInputFamily::KeyboardAndMouse, 1).empty());
	// Circle closes the screen and keeps the bindings.
	Press(Menu, EPadButton::Circle, 40);
	REQUIRE(Settings.Kept.size() == 1);
	CHECK(Settings.Kept.front().Controls.KeyAt(EControlAction::Cross, EInputFamily::KeyboardAndMouse, 0) == "Q");
	CHECK(Keys.Listens == 2);
}

