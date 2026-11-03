#include "Game/BattlingGameProgram.h"

#include "Battling/BattlingFight.h"
#include "Battling/BattlingGameSound.h"
#include "Game/BattlingGameFiles.h"
#include "Game/GameSession.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Graphics/MdlLights.h"
#include "Graphics/MdlModel.h"
#include "Media/DiscModFiles.h"
#include "Sound/FieldMusic.h"
#include "Sound/GameSoundSession.h"

#include <string>
#include <vector>

namespace Weltall
{
	namespace
	{
		constexpr uint16 BattlingSoftResetButtons = 0x090C;

		/** The field's first byte: a bout the other bytes set, the arena's menus, or the story's scripted bout. */
		constexpr uint8 BattlingFromMenus = 1;
		constexpr uint8 BattlingScriptedBout = 2;
		/** The field's second byte: the kind of bout. */
		constexpr uint8 BattlingKindComputer = 0;
		constexpr uint8 BattlingKindPractice = 1;
		constexpr uint8 BattlingKindTutorial = 2;

		/** The story's progress, the event variable whose value the roster's gears are unlocked by. */
		constexpr int32 StoryProgressVariable = 0;

		constexpr uint32 SettingsValidMask = 0xF;
		constexpr uint32 SettingsValid = 1;
		constexpr uint32 SettingsArgento = 0x10000;
		constexpr uint8 DefaultWinsPerSet = 2;

		/** The story's bout against Stier is fought at sunset. */
		constexpr uint8 SunsetOpponent = 5;
		constexpr uint8 LightingSunset = 1;

		constexpr uint8 BattlingStatePause = 0;
		constexpr uint8 BattlingStateFight = 1;
		constexpr uint8 BattlingStateLeaving = 3;
		constexpr uint8 BattlingStateDemonstration = 5;

		/**
		 * The versus screen stays while the console would read the gears' files: here until a button is pressed, two seconds
		 * at most (its passes last two vertical blanks). Then one pass for each step of the load.
		 */
		constexpr int32 VersusHoldPasses = 60;
		constexpr int32 VersusLoadPasses = 4;
		/** The main loop's passes before the bout's first: the load, then the versus screen's when it shows. */
		constexpr uint32 PassesBeforeBout = 1;

		/** The scene's first light as its start leaves it, which the gears' parts are built under. */
		constexpr FGteLongVector SceneLight{0x10, 0x10, 0x10};
		constexpr int16 SceneLightStrength = 0x800;

		constexpr int8 PadDigital = 1;
		constexpr int8 PadDualShock = 4;
		constexpr int8 PadNone = 0;

		/** The practice style that hands player 2 to the second controller. */
		constexpr uint8 PracticeStyleSecondPad = 2;

		/** Every vertical blank queued since the last pass, kept in Blanks: both ports' controllers, a gamepad as a DualShock in analog mode, each with its motor or none. */
		FBattlingTickInput ReadPads(FGameSession& Session, std::vector<std::array<FBattlingPad, 2>>& Blanks)
		{
			FKernelPad& Pad = Session.Pad();
			while (const std::optional<std::array<FKernelPadWords, FKernelPad::Ports>> Words = Pad.PopBoth())
			{
				std::array<FBattlingPad, 2> Blank{};
				for (int32 Port = 0; Port < FKernelPad::Ports; ++Port)
				{
					const FKernelPadWords& Read = (*Words)[static_cast<std::size_t>(Port)];
					FBattlingPad& Made = Blank[static_cast<std::size_t>(Port)];
					Made.Type = !Pad.IsConnected(Port) ? PadNone : Read.bAnalog ? PadDualShock : PadDigital;
					Made.Held = Read.Held;
					Made.Pressed = Read.Pressed;
					Made.Repeated = Read.Repeated;
					Made.LeftX = Read.StickX;
					Made.LeftY = Read.StickY;
					Made.StickHeld = Read.StickHeld;
					Made.bMotor = Pad.IsConnected(Port) && Pad.HasMotor(Port);
				}
				Blanks.push_back(Blank);
			}
			FBattlingTickInput Input;
			Input.Blanks = Blanks;
			Input.bOverflowed = Pad.Overflowed();
			return Input;
		}

		/** The rules the arena's fight plays by: the game's, or those of the mod's arena whose files the discs give. */
		EBattlingRules BattlingRulesOf(EPerfectWorksArena Arena)
		{
			switch (Arena)
			{
			case EPerfectWorksArena::Basic:
				return EBattlingRules::PerfectWorksBasic;
			case EPerfectWorksArena::Expert:
				return EBattlingRules::PerfectWorksExpert;
			default:
				return EBattlingRules::Original;
			}
		}

		std::string BattlingParameterList(const std::array<uint8, 6>& Parameters)
		{
			std::string List;
			for (const uint8 Parameter : Parameters)
			{
				List += " " + std::to_string(Parameter);
			}
			return List;
		}
	}

	FBattlingSavedSettings FBattlingSavedSettings::FromWord(uint32 Word)
	{
		FBattlingSavedSettings Settings;
		if ((Word & SettingsValidMask) != SettingsValid)
		{
			return Settings;
		}
		Settings.Vibration = {(Word >> 4 & 1) != 0, (Word >> 5 & 1) != 0};
		Settings.WinsPerSet = static_cast<uint8>(Word >> 6 & 0x7F);
		Settings.Level = static_cast<uint8>(Word >> 13 & 7);
		return Settings;
	}

	void FBattlingSavedSettings::Save(FGameState& Game) const
	{
		const uint32 Word = SettingsValid | (Vibration[0] ? 1u : 0u) << 4 | (Vibration[1] ? 1u : 0u) << 5 | static_cast<uint32>(WinsPerSet & 0x7F) << 6
			| static_cast<uint32>(Level & 7) << 13;
		Game.SetArenaSettings((Game.ArenaSettings() & 0xFFFF0000u) | Word);
	}

	FBattlingSavedSettings FBattlingSavedSettings::Load(FGameState& Game)
	{
		const uint32 Word = Game.ArenaSettings();
		if ((Word & SettingsValidMask) != SettingsValid)
		{
			Game.SetArenaSettings((Word & 0xFFFF0000u) | SettingsValid | static_cast<uint32>(DefaultWinsPerSet) << 6);
		}
		else if ((Word & SettingsArgento) != 0)
		{
			Game.SetArenaDefeatedGears({});
		}
		return FromWord(Word);
	}

	FBattlingGameProgram::FBattlingGameProgram(FGameSession& InSession) : Session(InSession)
	{
		// The gears, their trails and the fire, but for what each upload below names.
		Screen.SetUploadKind(EVramUploadKind::ModelsAndEffects);
	}

	FBattlingGameProgram::~FBattlingGameProgram() = default;

	TResult<std::unique_ptr<FBattlingGameProgram>> FBattlingGameProgram::Start(FGameSession& Session)
	{
		FKernelState& Kernel = Session.Kernel();
		const std::array<uint8, 6> Parameters = Kernel.BattlingParameters;
		const uint8 From = Parameters[0];
		const uint8 Kind = Parameters[1];
		const bool bFromMenus = From == BattlingFromMenus;
		if (!bFromMenus && (Kind > BattlingKindTutorial || Parameters[2] >= BattlingGears::Count || Parameters[3] >= BattlingGears::Count))
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Battling)) + " was asked for a match it has none of:" + BattlingParameterList(Parameters));
		}

		std::unique_ptr<FBattlingGameProgram> Program(new FBattlingGameProgram(Session));
		const FBattlingGameFiles& Files = Session.BattlingFiles();
		FGameSoundSession& GameSound = Session.Sound();
		Program->Sound = std::make_unique<FBattlingGameSound>(*GameSound.Sound(), Files.Effects, GameSound.FieldMusic().Sequence());
		if (!Program->Sound->HasEffects())
		{
			return MakeError("the sound driver has no room for the arena's effects");
		}
		Program->Text.Font = &Session.Text().Font;
		Program->Text.Tables = &Session.Text().NameTables;
		Program->Text.State = &Session.Names();
		Program->Text.ButtonMap = MesKernelButtonMap;
		Program->Text.Speed = Session.Settings().TextSpeed;
		Program->Text.AutoAdvance = Session.Settings().AutoAdvance;
		Program->Text.Log = Session.MessageLog();

		Program->Bout = std::make_unique<FBattlingFight>(Files.Tables, Files.Terrain, Session.Random(), *Program->Sound);
		FBattlingFight& Fight = *Program->Bout;
		Fight.InitSparkTiles();
		Fight.InitSplashTiles();
		const FBattlingOptions& Options = Session.Settings().Fixes.Battling;
		Fight.Settings.bRangedMovesByOwnState = Options.bRangedMovesByOwnState;
		Fight.Settings.bPracticeComputerAtStyleLevel = Options.bPracticeComputerAtStyleLevel;
		Fight.Settings.bPracticeFrameRateAsLabelled = Options.bPracticeFrameRateAsLabelled;
		Fight.Settings.Rules = BattlingRulesOf(Session.Image().ModChanges().PerfectWorksArena);
		// Only a start from the menus or the scripted bout loads the saved settings; the other bouts' vibration is whatever the
		// memory held, which the port takes as the switches the player last saved. The menus take their set's wins and the
		// computer's level from them too.
		Fight.Settings.WinsPerSet = Parameters[4];
		Fight.Settings.Level = Parameters[5];
		Fight.Settings.Vibration = FBattlingSavedSettings::FromWord(Session.Game().ArenaSettings()).Vibration;
		if (From != 0)
		{
			const FBattlingSavedSettings Saved = FBattlingSavedSettings::Load(Session.Game());
			Fight.Settings.Vibration = Saved.Vibration;
			if (bFromMenus)
			{
				Fight.Settings.WinsPerSet = Saved.WinsPerSet;
				Fight.Settings.Level = Saved.Level;
			}
		}
		Fight.Settings.bScripted = From == BattlingScriptedBout;
		Fight.Text = &Program->Text;
		Fight.Messages = &Files.Messages.Table();
		Fight.DefeatedGears = Session.Game().ArenaDefeatedGears();
		Fight.bArgentoUnlocked = (Session.Game().ArenaSettings() & SettingsArgento) != 0;

		{
			const FVram::FKindScope SceneryKind(Program->Screen, EVramUploadKind::Scenery);
			for (const FVramImage& Upload : Files.ArenaTextures.Uploads)
			{
				Program->Screen.Upload(Upload);
			}
		}
		{
			const FVram::FKindScope TextKind(Program->Screen, EVramUploadKind::Text);
			Program->Screen.Upload(BattlingNamePaletteUpload(Files.ArenaTables));
		}
		Program->Screen.Upload(BattlingFirePaletteUpload(Files.ArenaTables));
		// The kernel's text palettes, which the program loads for its message windows.
		const std::array<uint16, 32>& TextPalettes = Session.FieldTables().TextClut;
		const FVram::FKindScope TextKind(Program->Screen, EVramUploadKind::Text);
		Program->Screen.Upload({{FBattlingTutorial::PalettesX, FBattlingTutorial::PalettesY, static_cast<int16>(TextPalettes.size()), 1},
			std::vector<uint16>(TextPalettes.begin(), TextPalettes.end())});
		Program->Drawing = std::make_unique<FBattlingDrawing>(Files.ArenaTables, Files.ArenaTextures, Files.Terrain, Session.Arctangents());

		if (bFromMenus)
		{
			// The menus as the program starts them: both sides' kinds as its settings start them, the roster the story has
			// reached, Argento once unlocked.
			Program->bFromMenus = true;
			Fight.Settings.bFromMenus = true;
			Fight.Settings.Computer = {true, false};
			Fight.Menus.Roster = BattlingRoster(Files.GearTable, Session.Game().SavedEventVariable(StoryProgressVariable));
			if (Fight.bArgentoUnlocked)
			{
				Fight.Menus.ListArgento();
			}
			std::array<uint8, BattlingGears::Count> Every{};
			for (std::size_t Gear = 0; Gear < Every.size(); ++Gear)
			{
				Every[Gear] = static_cast<uint8>(Gear);
			}
			TResult<std::vector<FVramImage>> Portraits = ReadBattlingPortraitUploads(Session.Image(), Session.Index(), Every);
			if (!Portraits)
			{
				return Portraits.Error();
			}
			Program->MenuPortraits = std::move(*Portraits);
			Fight.StartMenus();
			Fight.VBlank = Session.Game().PlayTime();
			Program->GameStage = EBattlingGameStage::Menus;
			Program->NextBlanks = 1;
			return Program;
		}

		// The program takes the first byte as read: its exit then leaves by the field's way.
		Kernel.BattlingParameters[0] = 0;
		Program->GearNumbers = {Parameters[2], Parameters[3]};
		Fight.LightFire();
		switch (Kind)
		{
		case BattlingKindComputer:
			Fight.Settings.Mode = EBattlingMode::VersusComputer;
			Fight.Settings.Computer = {false, true};
			Fight.Lighting = Parameters[3] == SunsetOpponent ? LightingSunset : 0;
			Program->bVersusShown = true;
			break;
		case BattlingKindPractice:
			Fight.Settings.Mode = EBattlingMode::Practice;
			Fight.Settings.Computer = {false, true};
			break;
		default:
			Fight.Settings.Mode = EBattlingMode::Tutorial;
			Fight.Settings.Computer = {false, false};
			Fight.StartTutorial();
			break;
		}
		if (auto Error = Program->LoadGears())
		{
			return *Error;
		}
		if (Program->bVersusShown)
		{
			TResult<std::vector<FVramImage>> Portraits = ReadBattlingPortraitUploads(Session.Image(), Session.Index(), Program->GearNumbers);
			if (!Portraits)
			{
				return Portraits.Error();
			}
			const FVram::FKindScope FacesKind(Program->Screen, EVramUploadKind::Portraits);
			for (const FVramImage& Upload : *Portraits)
			{
				Program->Screen.Upload(Upload);
			}
			Program->GameStage = EBattlingGameStage::Versus;
			Program->LoadPasses = VersusLoadPasses;
		}
		else
		{
			Fight.PutOutFire();
		}
		return Program;
	}

	std::optional<FError> FBattlingGameProgram::LoadGears()
	{
		const FBattlingGameFiles& Files = Session.BattlingFiles();
		FBattlingFight& Fight = *Bout;
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			const int32 Gear = GearNumbers[Side];
			TResult<FBattlingGearFile> File = FBattlingGearFile::Read(Session.Image(), Session.Index(), Gear);
			if (!File)
			{
				return File.Error();
			}
			GearFiles[Side] = std::move(*File);
			const FBattlingGearFile& Kept = GearFiles[Side];
			TResult<FMdlModel> Model = ParseMdlModel(std::span<const uint8>(Kept.Bytes).subspan(Kept.Model));
			if (!Model)
			{
				return Model.Error();
			}
			Fight.LoadFighter(static_cast<int32>(Side), Gear, Kept, Files.GearTable.Gears[static_cast<std::size_t>(Gear)], std::move(*Model));
		}
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			TResult<std::vector<FVramImage>> Uploads = ReadBattlingGearTextures(GearFiles[Side], static_cast<int32>(Side));
			if (!Uploads)
			{
				return Uploads.Error();
			}
			for (const FVramImage& Upload : *Uploads)
			{
				Screen.Upload(Upload);
			}
			const FVram::FKindScope HudKind(Screen, EVramUploadKind::MenusAndHud);
			for (const FVramImage& Upload : BattlingGearHudUploads(GearFiles[Side], static_cast<int32>(Side)))
			{
				Screen.Upload(Upload);
			}
		}
		Screen.Upload(BattlingTrailPaletteUpload(Files.ArenaTextures, Fight.Fighters[0].Colour, Fight.Fighters[1].Colour));
		// The gears' parts are lit as they are built by what the GTE holds then: the light matrix the program before (or the
		// last frame drawn) left, under the scene's own colours.
		FMdlLights SceneLights;
		SceneLights.SetDirectional(0, SceneLight, {SceneLightStrength, SceneLightStrength, SceneLightStrength});
		FGte BuildLighting;
		BuildLighting.LightMatrix = Session.Kernel().LightMatrix;
		BuildLighting.ColorMatrix = SceneLights.Colors;
		return Drawing->LoadGears(Fight, BuildLighting);
	}

	std::optional<FError> FBattlingGameProgram::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (ProgramState != EGameProgramState::Running)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Battling)) + " has ended");
		}
		for (const FPadState Held : HeldPerVblank)
		{
			Session.Vblank(Held);
		}
		if (Session.Pad().Held() == BattlingSoftResetButtons)
		{
			ProgramState = EGameProgramState::SoftReset;
			return std::nullopt;
		}
		if (GameStage == EBattlingGameStage::Menus)
		{
			return RunMenus();
		}
		if (GameStage == EBattlingGameStage::Versus)
		{
			if (bFromMenus)
			{
				RunMenusLoad();
				return std::nullopt;
			}
			Bout->Fire.Step(Session.Random());
			if (!bGearsRead)
			{
				// The pass the console repeats while it reads: it merges the pad's queue, which here ends the wait.
				const bool bPressed = Session.Pad().TakeMerged().Pressed != 0;
				++HeldPasses;
				bGearsRead = bPressed || HeldPasses >= VersusHoldPasses;
				DrawVersus(false);
				return std::nullopt;
			}
			// The load's steps, a vertical blank each.
			DrawVersus(true);
			NextBlanks = 1;
			if (--LoadPasses == 0)
			{
				GameStage = EBattlingGameStage::Bout;
			}
			return std::nullopt;
		}
		if (!bBoutBegun)
		{
			BeginBout();
		}
		std::vector<std::array<FBattlingPad, 2>> Blanks;
		const FBattlingTickInput Input = ReadPads(Session, Blanks);
		Bout->RunPass(Input);
		StartVibrations();
		NextBlanks = Bout->PassBlanks;
		if (Bout->Fault != EBattlingFault::None)
		{
			return MakeError("the arena's bout stopped (its fault " + std::to_string(static_cast<int32>(Bout->Fault)) + ")");
		}
		if (bFromMenus && Bout->LatchedState == BattlingStateLeaving)
		{
			// The set or the demonstration is over: the pass went on into the menus.
			bBoutBegun = false;
			return AfterMenusPass(Input);
		}
		PublishPass(false);
		if (Bout->Tutorial.bLeft || (!bFromMenus && Bout->State == BattlingStateLeaving))
		{
			Leave();
		}
		return std::nullopt;
	}

	std::optional<FError> FBattlingGameProgram::RunMenus()
	{
		std::vector<std::array<FBattlingPad, 2>> Blanks;
		const FBattlingTickInput Input = ReadPads(Session, Blanks);
		Bout->RunMenusPass(Input);
		NextBlanks = Bout->PassBlanks;
		return AfterMenusPass(Input);
	}

	std::optional<FError> FBattlingGameProgram::AfterMenusPass(const FBattlingTickInput& Input)
	{
		FBattlingFight& Fight = *Bout;
		GameStage = EBattlingGameStage::Menus;
		if (Fight.Menus.bExited)
		{
			FBattlingSavedSettings{Fight.Settings.Vibration, Fight.Settings.WinsPerSet, Fight.Settings.Level}.Save(Session.Game());
			Leave();
			return std::nullopt;
		}
		if (Fight.Menus.bDemonstrationChosen)
		{
			// Its gears loaded at once, no fire, its first pass in this one.
			GearNumbers = {static_cast<uint8>(Fight.Menus.GearsRead[0]), static_cast<uint8>(Fight.Menus.GearsRead[1])};
			if (auto Error = LoadGears())
			{
				return Error;
			}
			Fight.PutOutFire();
			GameStage = EBattlingGameStage::Bout;
			BeginBout();
			Fight.RunPass(Input);
			StartVibrations();
			NextBlanks = Fight.PassBlanks;
			PublishPass(false);
			return std::nullopt;
		}
		PublishPass(false);
		if (!Fight.Menus.bBoutChosen)
		{
			return std::nullopt;
		}
		GearNumbers = {static_cast<uint8>(Fight.Menus.GearsRead[0]), static_cast<uint8>(Fight.Menus.GearsRead[1])};
		if (auto Error = LoadGears())
		{
			return Error;
		}
		GameStage = EBattlingGameStage::Versus;
		HeldPasses = 0;
		bGearsRead = false;
		LoadPasses = Fight.Settings.Mode == EBattlingMode::Tutorial ? 0 : VersusLoadPasses;
		return std::nullopt;
	}

	void FBattlingGameProgram::RunMenusLoad()
	{
		// While the gears are read, until a button or two seconds as the story's versus screen; then the load's steps. The
		// tutorial shows the fire a pass and goes on.
		FBattlingFight& Fight = *Bout;
		const bool bLoading = bGearsRead;
		if (!bLoading)
		{
			const bool bPressed = Session.Pad().TakeMerged().Pressed != 0;
			++HeldPasses;
			bGearsRead = Fight.Settings.Mode == EBattlingMode::Tutorial || bPressed || HeldPasses >= VersusHoldPasses;
		}
		else
		{
			--LoadPasses;
		}
		Fight.RunLoadPass(bLoading);
		NextBlanks = Fight.PassBlanks;
		PublishPass(bLoading);
		if (bGearsRead && LoadPasses == 0)
		{
			GameStage = EBattlingGameStage::Bout;
		}
	}

	void FBattlingGameProgram::BeginBout()
	{
		bBoutBegun = true;
		FBattlingFight& Fight = *Bout;
		if (bFromMenus && Fight.Settings.Mode == EBattlingMode::Tutorial)
		{
			Fight.PutOutFire();
		}
		Fight.StartFadeIn();
		Fight.StartRound();
		if (Fight.Settings.Mode != EBattlingMode::Tutorial && Fight.Settings.Mode != EBattlingMode::Demonstration)
		{
			Fight.State = BattlingStateFight;
		}
		// From the menus the main loop's counts run on from theirs.
		if (!bFromMenus)
		{
			Fight.Pass = PassesBeforeBout + (bVersusShown ? static_cast<uint32>(HeldPasses + VersusLoadPasses) : 0);
			Fight.VBlank = Session.Game().PlayTime();
		}
	}

	void FBattlingGameProgram::DrawVersus(bool bLoading)
	{
		// A frame of no scene: the screen black under the fire, the portraits and the names.
		const int32 Next = Latest < 0 ? 0 : 1 - Latest;
		FBattlingFrame& Frame = Frames[static_cast<std::size_t>(Next)];
		Frame.Pass = 0;
		Frame.Columns = FBattlingFrame::ScreenWidth;
		Frame.PictureBlurs = 0;
		Frame.bTrails = false;
		Frame.Scene = {};
		Frame.Clear = {0, 0, 0};
		Frame.Screen.clear();
		Frame.TextButtons.clear();
		Drawing->DrawVersus(*Bout, {GearNumbers[0], GearNumbers[1]}, bLoading, Frame.Screen);
		bPreviousFollows = false;
		Latest = Next;
		Screen.Upload(BattlingFirePictureUpload(Bout->Fire.Picture()));
	}

	void FBattlingGameProgram::PublishPass(bool bLoading)
	{
		FBattlingFight& Fight = *Bout;
		// A pause opened in the fight's tick captures the screen as it stands: the frame of the pass before, before this one's.
		if (Fight.State == BattlingStatePause && Fight.LatchedState != BattlingStatePause && Latest >= 0)
		{
			const FBattlingFrame& Shown = Frames[static_cast<std::size_t>(Latest)];
			PictureBlursBefore = Shown.PictureBlurs;
			if (Shown.PictureBlurs == 0)
			{
				Picture = Shown;
			}
		}
		Drawing->TypeWindows(Fight, TypedLines);
		{
			const FVram::FKindScope TextKind(Screen, EVramUploadKind::Text);
			for (const FVramImage& Typed : TypedLines)
			{
				Screen.Upload(Typed);
			}
		}
		if (Fight.Menus.Gears.bPortraitsUploadPending)
		{
			// The gear choice's portraits, every gear's, as it first opens after the menus start.
			Fight.Menus.Gears.bPortraitsUploadPending = false;
			const FVram::FKindScope FacesKind(Screen, EVramUploadKind::Portraits);
			for (const FVramImage& Upload : MenuPortraits)
			{
				Screen.Upload(Upload);
			}
		}
		if (Fight.bFireDrawn)
		{
			Screen.Upload(BattlingFirePictureUpload(Fight.FireDrawn));
		}
		if (Fight.bFrameDrawn || Fight.bWinnerDrawn)
		{
			// A view of any width but the original's draws everything in reach.
			Drawing->Culling = Session.Settings().Field.Culling == EFieldCulling::Original ? EBattlingCulling::Original : EBattlingCulling::InRange;
			const int32 Next = Latest < 0 ? 0 : 1 - Latest;
			bPreviousFollows = Latest >= 0 && Frames[static_cast<std::size_t>(Latest)].Pass + 1 == Fight.Pass;
			if (Fight.bWinnerDrawn)
			{
				Drawing->DrawWinner(Fight, Frames[static_cast<std::size_t>(Next)]);
			}
			else if (Fight.LatchedState == BattlingStateDemonstration)
			{
				Drawing->DrawDemonstration(Fight, Frames[static_cast<std::size_t>(Next)]);
			}
			else
			{
				Drawing->Draw(Fight, Frames[static_cast<std::size_t>(Next)]);
			}
			Latest = Next;
			Session.Kernel().LightMatrix = Drawing->LightLeft();
		}
		else if (Fight.bPauseDrawn || Fight.bMenusDrawn)
		{
			// A frame of no scene: a paused pass's layer over the picture, which each pass of the pause blurs as many times as it
			// has been; or the menus' over the fire.
			const int32 Next = Latest < 0 ? 0 : 1 - Latest;
			FBattlingFrame& Frame = Frames[static_cast<std::size_t>(Next)];
			Frame.Pass = Fight.Pass;
			Frame.Columns = FBattlingFrame::ScreenWidth;
			Frame.Scene = {};
			Frame.Clear = {0, 0, 0};
			Frame.Screen.clear();
			Frame.TextButtons.clear();
			Frame.PictureBlurs = 0;
			Frame.bTrails = false;
			if (Fight.bPauseDrawn)
			{
				const bool bPicture = Drawing->DrawPause(Fight, Frame.Screen, &Frame.TextButtons) && Picture.has_value();
				Frame.PictureBlurs = bPicture ? PictureBlursBefore + 1 + Fight.Menus.BlurPresses : 0;
			}
			else
			{
				Drawing->DrawMenus(Fight, bLoading, Frame.Screen, &Frame.TextButtons);
				Screen.Upload(BattlingFirePictureUpload(Fight.Fire.Picture()));
			}
			bPreviousFollows = false;
			Latest = Next;
		}

		// The program writes these where the field and the saves read them, as it goes.
		FGameState& Game = Session.Game();
		Game.SetArenaDefeatedGears(Fight.DefeatedGears);
		if (Fight.bArgentoUnlocked)
		{
			Game.SetArenaSettings(Game.ArenaSettings() | SettingsArgento);
		}
		Session.Kernel().Field.BattlingResult = Fight.Result;
	}

	bool FBattlingGameProgram::ReadsSecondPad() const
	{
		const FBattlingSettings& Settings = Bout->Settings;
		return Settings.Mode == EBattlingMode::TwoPlayers || (Settings.Mode == EBattlingMode::Practice && Settings.PracticeStyle == PracticeStyleSecondPad);
	}

	void FBattlingGameProgram::StartVibrations()
	{
		for (int32 Index = 0; Index < Bout->VibrationCount; ++Index)
		{
			const std::array<int32, 2>& Started = Bout->Vibrations[static_cast<std::size_t>(Index)];
			Session.Vibration().Start(Started[0], Started[1]);
		}
	}

	void FBattlingGameProgram::Leave()
	{
		FKernelState& Kernel = Session.Kernel();
		Kernel.NextProgram = EGameProgram::Field;
		Kernel.BattlingParameters[0] = BattlingFromMenus;
		ProgramState = EGameProgramState::Ended;
	}
}
