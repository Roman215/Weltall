#include "Game/FieldProgram.h"

#include "Game/ChapterCues.h"

#include "Battle/BattleScene.h"
#include "Field/FieldMap.h"
#include "Game/FieldMovie.h"
#include "Game/GameMenus.h"
#include "Game/GameSession.h"
#include "Sound/GameSoundSession.h"
#include "Text/MesWindow.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The saved variables the field's start writes: 0x002 the entrance, 0x008 the camera's direction, 0x050 set on the retail discs. */
		constexpr int32 SavedEntrance = 1;
		constexpr int32 SavedCameraDirection = 4;
		constexpr int32 SavedRetailVariable = 0x28;
		/** A direction in eighths of a turn, as the game state keeps the camera's yaw in 4096ths. */
		constexpr int32 DirectionShift = 9;
		/** The result a lost battle leaves the field. */
		constexpr uint8 BattleLostResult = 1;
		/** The result the Menu leaves when it loaded a save, and the title's mode, whose map sends the loaded game on itself. */
		constexpr uint8 SaveLoadedResult = 2;
		constexpr uint8 LoadGameMenu = 2;
		/** The event variable that counts the story's progress, by its address. */
		constexpr uint32 StoryProgressVariable = 0;
		/** A formation record's flags. */
		constexpr std::size_t FormationFlagsByte = 1;

		/** A battle of the story: one a script starts that cannot be escaped or runs a battle event. */
		bool IsStoryBattle(const FFieldRequest& Request, const FKernelState& Kernel)
		{
			const auto Formation = Kernel.Formation(Request.Formation);
			return !Request.bEncounter && Formation && ((*Formation)[FormationFlagsByte] & (FormationFlags::NoEscape | FormationFlags::EventBattle)) != 0;
		}
	}

	FFieldProgram::FFieldProgram(FGameSession& InSession)
		: Session(InSession), PlayerCamera(InSession.CameraOffsets()), PartySprites(InSession.Image(), InSession.Index())
	{
	}

	FFieldProgram::~FFieldProgram()
	{
		// The movie's and the menu's sounds go before effect file 0.
		FieldMovie.reset();
		Menu.reset();
		ReleaseSound();
	}

	TResult<std::unique_ptr<FFieldProgram>> FFieldProgram::Start(FGameSession& Session)
	{
		std::unique_ptr<FFieldProgram> Program(new FFieldProgram(Session));
		TResult<FFieldPresenter> Presenter = FFieldPresenter::Start(Session.FieldTables(), Session.Image(), Session.Index());
		if (!Presenter)
		{
			return Presenter.Error();
		}
		Program->Presenter.emplace(std::move(*Presenter));
		FGameSoundSession& Sound = Session.Sound();
		Sound.Sound()->Lock()->RegisterEffects(Sound.Resident().MainEffects);
		Program->bMainEffectsRegistered = true;

		FFieldServices Services;
		Services.Image = &Session.Image();
		Services.Index = &Session.Index();
		Services.Arctangents = &Session.Arctangents();
		Services.Game = &Session.Game();
		Services.Random = &Session.Random();
		Services.Songs = &Sound.FieldSongs();
		Services.SoundEffects = &Sound.FieldSoundEffects();
		Services.SoundBanks = &Sound.FieldSoundEffects();
		Services.SoundPause = &Sound.FieldSoundEffects();
		Services.PartySprites = &Program->PartySprites;
		Services.Pad = &Session.Pad();
		Services.Clock = &Session.Clock();
		Services.Vibration = &Session.Vibration();
		Services.Text.Font = &Session.Text().Font;
		Services.Text.Tables = &Session.Text().NameTables;
		Services.Text.State = &Session.Names();
		Services.Text.ButtonMap = MesKernelButtonMap;
		Services.Disc = static_cast<uint8>(Session.DiscNumber() + 1);
		Services.Options = Session.Settings().Fixes.Field;
		Services.EncounterRate = Session.Settings().Balance.Encounters;
		Services.Story = Session.Story();
		Services.Unreached = &Session.Unreached();
		Services.TranslationCreditsCue = FBootNotice::TranslationCreditsCue(Session.Index());
		Program->FieldRuntime = std::make_unique<FFieldRuntime>(Services);
		Program->FollowSettings();
		if (auto Error = Program->EnterFirstMap())
		{
			return *Error;
		}
		return Program;
	}

	void FFieldProgram::FollowSettings()
	{
		const FPlayerSettings& Settings = Session.Settings();
		FieldRuntime->SetOptions(Settings.Fixes.Field);
		FieldRuntime->SetAlwaysRun(Settings.Controls.bAlwaysRun);
		FieldRuntime->SetAnalogMovement(Settings.Controls.bAnalogMovement);
		FieldRuntime->SetWalkIntoDoors(Settings.Controls.bWalkIntoDoors);
		FieldRuntime->SetTextSpeed(Settings.TextSpeed);
		FieldRuntime->SetEncounterRate(Settings.Balance.Encounters);
		Culling = Settings.Field.Culling;
		Presenter->SetPilotStandIns(Settings.Field.bDetailedPilots);
		Presenter->SetGearStandIns(Settings.Field.GearModels);
		SettingsSeen = Session.SettingsRevision();
	}

	std::optional<FError> FFieldProgram::EnterFirstMap()
	{
		FKernelState& Kernel = Session.Kernel();
		FGameState& Game = Session.Game();
		// Where the state says the field goes on: the map, the camera's direction and the entrance.
		Game.SetSavedEventVariable(SavedCameraDirection, static_cast<uint16>(Game.CameraYaw() >> DirectionShift));
		Game.SetSavedEventVariable(SavedEntrance, Game.Scene());
		int32 Song = FFieldMusic::NoSong;
		if (!Kernel.bFieldEntered)
		{
			Game.ProgramResult = 0;
		}
		else
		{
			Song = Game.FieldSong();
		}
		Session.Sound().FieldMusic().SetSong(Song);
		Game.SetSavedEventVariable(SavedRetailVariable, 1);
		// After a lost battle, and after a battle event that asked for its own fade, the map comes in their ways.
		FFieldEntry Entry;
		Entry.bAfterDefeat = Game.ProgramResult == BattleLostResult;
		Entry.bHoldsGameOver = true;
		Entry.bBattleEventFade = Kernel.bEventFadeIn;
		// Back from a battle or the arena it saved itself for, the field comes back to that state.
		if (auto Error = Kernel.FieldTrips != 0 ? FieldRuntime->EnterMapAfterBattle(Game.Place(), Kernel.Field, Entry)
												: FieldRuntime->EnterMap(Game.Place(), Kernel.Field, Entry))
		{
			return Error;
		}
		Kernel.FieldTrips = 0;
		Kernel.ArenaTrips = 0;
		if (auto Error = Presenter->EnterMap(*FieldRuntime))
		{
			return Error;
		}
		Kernel.bFieldEntered = true;
		MapLoadsSeen = FieldRuntime->MapLoads();
		KeepFormations();
		return std::nullopt;
	}

	int32 FFieldProgram::VblanksPerFrame() const
	{
		if (TranslationCredits)
		{
			return 1;
		}
		if (Menu)
		{
			return Menu->VblanksPerFrame();
		}
		return FieldRuntime->VblanksPerTick();
	}

	std::optional<FError> FFieldProgram::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (ProgramState != EGameProgramState::Running)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Field)) + " has ended");
		}
		if (Session.SettingsRevision() != SettingsSeen)
		{
			FollowSettings();
		}
		if (TranslationCredits)
		{
			return AdvanceCredits(HeldPerVblank[0]);
		}
		if (Menu)
		{
			if (auto Error = Menu->Frame(HeldPerVblank))
			{
				return Error;
			}
			if (Menu->State() == EGameProgramState::SoftReset)
			{
				ProgramState = EGameProgramState::SoftReset;
				return std::nullopt;
			}
			if (Menu->State() == EGameProgramState::Running)
			{
				return std::nullopt;
			}
			Menu.reset();
			// The disc change leaves the other disc in the drive.
			FieldRuntime->SetDisc(static_cast<uint8>(Session.DiscNumber() + 1));
			// The menu's gears, when it drew any, left the GTE their light.
			FieldRuntime->SetLightLeft(Session.Kernel().LightMatrix);
			if (Session.Game().ProgramResult == SaveLoadedResult)
			{
				// A loaded game goes on in its chapter without the chapter's card.
				Session.ChapterCard().Know(Session.Chapter(), Session.Subtitle());
			}
			if (auto Error = FieldRuntime->ResumeAfterMenu(Session.Game().ProgramResult))
			{
				return Error;
			}
			if (Session.Game().ProgramResult == SaveLoadedResult && Session.Kernel().MenuMode != LoadGameMenu)
			{
				return GoOnWithLoadedGame();
			}
			return TakeRequest();
		}
		if (auto Error = Tick(HeldPerVblank))
		{
			return Error;
		}
		if (FieldRuntime->TakeTranslationCredits())
		{
			return StartCredits();
		}
		return TakeRequest();
	}

	std::optional<FError> FFieldProgram::StartCredits()
	{
		TResult<FBootNotice> Loaded = FBootNotice::LoadTranslationCredits(Session.Image(), Session.Index());
		if (!Loaded)
		{
			return Loaded.Error();
		}
		// The kernel waits for the song's load before the screen fades in, which the port's load does at once.
		if (const TResult<bool> Started = Session.Sound().FieldMusic().PlayAnew(FBootNotice::TranslationCreditsSong); !Started)
		{
			return Started.Error();
		}
		TranslationCredits.emplace(std::move(*Loaded));
		return std::nullopt;
	}

	std::optional<FError> FFieldProgram::AdvanceCredits(FPadState Held)
	{
		Session.Vblank(Held);
		TranslationCredits->Tick(Session.Pad().Held());
		if (!TranslationCredits->IsOver())
		{
			return std::nullopt;
		}
		TranslationCredits.reset();
		return TakeRequest();
	}

	std::optional<FError> FFieldProgram::Tick(std::span<const FPadState> HeldPerVblank)
	{
		if (FieldMovie)
		{
			if (auto Error = AdvanceMovie(static_cast<int32>(HeldPerVblank.size())))
			{
				return Error;
			}
		}
		const int32 Next = Latest < 0 ? 0 : 1 - Latest;
		if (auto Error = Presenter->Tick(*FieldRuntime, HeldPerVblank, Culling, Frames[static_cast<std::size_t>(Next)], &PlayerCamera))
		{
			return Error;
		}
		FieldRuntime->SetViewTurn(PlayerCamera.ViewTurn());
		FieldRuntime->SetShownView(PlayerCamera.ShownView().View.View);
		// Nothing blends across a map change.
		const bool bMapChanged = FieldRuntime->MapLoads() != MapLoadsSeen;
		bPreviousFollows = Latest >= 0 && !bMapChanged;
		bFrameBefore = Latest >= 0;
		Latest = Next;
		if (bMapChanged)
		{
			MapLoadsSeen = FieldRuntime->MapLoads();
			KeepFormations();
		}
		AdvanceChapterCard(static_cast<int32>(HeldPerVblank.size()));
		if (FieldMovie)
		{
			if (FieldRuntime->IsMovieSkipped())
			{
				FieldMovie->Skip();
			}
			else if (FieldRuntime->Request().Kind != EFieldRequest::Movie)
			{
				FieldMovie.reset();
			}
		}
		return std::nullopt;
	}

	void FFieldProgram::AdvanceChapterCard(int32 Vblanks)
	{
		const FEventContext& Context = FieldRuntime->Context();
		const uint16 Story = Context.Variables.Raw(StoryProgressVariable);
		const FChapter* Chapter = Session.Chapters().Find(Story);
		// In control: the pad moves the leader and the menu opens, which no scene and not the title's map allow.
		const bool bInControl = Session.Settings().bChapterCards && Context.bFieldRunning && Context.bPlayerControlled && !Context.bMenuLocked
			&& !FieldMovie && FieldRuntime->Request().Kind == EFieldRequest::None;
		const int32 Number = Chapter != nullptr ? Chapter->Number : 0;
		const bool bMovie = FieldMovie || FieldRuntime->Request().Kind == EFieldRequest::Movie;
		if (Session.Settings().bChapterCards && Context.bFieldRunning && !bMovie)
		{
			// A chapter with no control of its own: a scene's place, or the story entering it. None begins over a movie.
			if (const int32 Asked = ChapterCues::AskedByScene(Context); Asked != 0)
			{
				Session.ChapterCard().Ask(Asked);
			}
			if (ChapterCues::BeginsWithStory(Number))
			{
				Session.ChapterCard().Ask(Number);
			}
		}
		Session.ChapterCard().Advance(Number, Session.SubtitleAt(Story), bInControl, Vblanks);
	}

	void FFieldProgram::AddViewInput(const FFieldViewInput& Input)
	{
		if (!Menu && !FieldMovie && !TranslationCredits)
		{
			PlayerCamera.AddInput(Input);
		}
	}

	std::optional<FError> FFieldProgram::AdvanceMovie(int32 Vblanks)
	{
		const uint32 Shown = FieldMovie->Player().PictureNumber();
		const TResult<int32> Frame = FieldMovie->Advance(Vblanks);
		if (!Frame)
		{
			return Frame.Error();
		}
		FieldRuntime->SetMovieFrame(*Frame);
		// A movie in the scene decodes its pictures into VRAM, where the map's models show them.
		const FMoviePlayer& Player = FieldMovie->Player();
		if (FieldMovie->View() == EFieldMovieView::InScene && Player.PictureNumber() != Shown && Player.Picture())
		{
			// A picture written over the picture of the screen's area leaves the area VRAM's, not the screen's.
			FieldRuntime->Context().Render.WroteVram(Presenter->UploadMoviePicture(*Player.Picture(), FieldMovie->Request(), Player.PictureNumber()));
		}
		return std::nullopt;
	}

	const FFieldFrame* FFieldProgram::LatestFrame() const
	{
		return Latest < 0 ? nullptr : &Frames[static_cast<std::size_t>(Latest)];
	}

	const FFieldFrame* FFieldProgram::PreviousFrame() const
	{
		return bPreviousFollows ? &Frames[static_cast<std::size_t>(1 - Latest)] : nullptr;
	}

	const FFieldFrame* FFieldProgram::FrameBeforeLatest() const
	{
		return bFrameBefore ? &Frames[static_cast<std::size_t>(1 - Latest)] : nullptr;
	}

	std::optional<FError> FFieldProgram::TakeRequest()
	{
		const FFieldRequest& Request = FieldRuntime->Request();
		switch (Request.Kind)
		{
		case EFieldRequest::None:
			return std::nullopt;
		case EFieldRequest::Battle:
		case EFieldRequest::WorldMap:
		case EFieldRequest::Battling:
		case EFieldRequest::Program:
			return Leave(Request);
		case EFieldRequest::Menu:
			return StartMenu(Request);
		case EFieldRequest::Movie:
			return FieldMovie ? std::nullopt : StartMovie();
		case EFieldRequest::SoftReset:
			ProgramState = EGameProgramState::SoftReset;
			return std::nullopt;
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldProgram::StartMovie()
	{
		TResult<std::unique_ptr<FFieldMovie>> Started = FFieldMovie::Start(Session, FieldRuntime->Request());
		if (!Started)
		{
			return Started.Error();
		}
		FieldMovie = std::move(*Started);
		const FMoviePlayer& Player = FieldMovie->Player();
		if (FieldMovie->View() == EFieldMovieView::InScene && Player.Picture())
		{
			FieldRuntime->Context().Render.WroteVram(Presenter->UploadMoviePicture(*Player.Picture(), FieldMovie->Request(), Player.PictureNumber()));
		}
		return std::nullopt;
	}

	std::optional<FError> FFieldProgram::StartMenu(const FFieldRequest& Request)
	{
		// What the field hands the menu, which leaves its result in the game state's ProgramResult.
		FKernelState& Kernel = Session.Kernel();
		Session.Game().ProgramResult = 0;
		Kernel.bMenuStandalone = false;
		Kernel.MenuMode = Request.MenuMode;
		Kernel.LightMatrix = FieldRuntime->LightLeft();
		Kernel.Field.MenuParam = Request.MenuParam;
		if (Session.Menus() == nullptr)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Menu)) + " (mode " + std::to_string(Request.MenuMode) + ", parameter " +
							 std::to_string(Request.MenuParam) + (Request.bMenuByButton ? ", the player's button" : "") + ") is not run yet");
		}
		TResult<std::unique_ptr<IGameProgram>> Opened = Session.Menus()->Open(Session, Request);
		if (!Opened)
		{
			return Opened.Error();
		}
		Menu = std::move(*Opened);
		return std::nullopt;
	}

	std::optional<FError> FFieldProgram::Leave(const FFieldRequest& Request)
	{
		FKernelState& Kernel = Session.Kernel();
		FGameSoundSession& Sound = Session.Sound();
		Kernel.Field = FieldRuntime->KernelState();
		Kernel.LightMatrix = FieldRuntime->LightLeft();
		Kernel.bEventFadeIn = false;
		Kernel.NextProgram = static_cast<EGameProgram>(Request.NextProgram);
		switch (Request.Kind)
		{
		case EFieldRequest::Battle:
			Kernel.BattleKind = Request.BattleKind;
			Kernel.bBattleDebug = false;
			Kernel.Music = EKernelMusic::Field;
			if (IsStoryBattle(Request, Kernel))
			{
				// However fast the last fight was left, the story's battles begin at the game's pace: R1 steps them up again.
				Session.BattleSpeed() = EBattleSpeed::Normal;
			}
			if (Request.bFieldKept)
			{
				++Kernel.FieldTrips;
			}
			if (Request.bEncounter)
			{
				// The encounter's battle plays its own song, the map's kept for the field's return, which the
				// field names as the song it goes on with.
				FFieldMusic& Music = Sound.FieldMusic();
				const int32 MapSong = Music.Song();
				if (const TResult<bool> Started = Music.StartBattleSong(Request.BattleSong); !Started)
				{
					return Started.Error();
				}
				Music.SetSong(MapSong);
			}
			break;
		case EFieldRequest::WorldMap:
			if (const TResult<bool> Restored = Sound.FieldMusic().RestoreBattleThemes(); !Restored)
			{
				return Restored.Error();
			}
			break;
		case EFieldRequest::Battling:
			Kernel.BattlingParameters = Request.BattlingParameters;
			Kernel.BattlingTask = 0;
			++Kernel.ArenaTrips;
			++Kernel.FieldTrips;
			break;
		case EFieldRequest::Program:
			Sound.FieldMusic().Stop();
			Kernel.FieldTrips = 0;
			Kernel.ArenaTrips = 0;
			if (Request.bNewGame)
			{
				if (auto Error = Session.InitNewGame())
				{
					return Error;
				}
			}
			break;
		default:
			break;
		}
		ReleaseSound();
		ProgramState = EGameProgramState::Ended;
		return std::nullopt;
	}

	std::optional<FError> FFieldProgram::GoOnWithLoadedGame()
	{
		FFieldRequest Request;
		Request.Kind = EFieldRequest::Program;
		Request.NextProgram = static_cast<uint8>(LoadedGameProgram(Session.Game()));
		return Leave(Request);
	}

	void FFieldProgram::KeepFormations()
	{
		// Every map with encounter data unpacks it over the kernel's formation table.
		const std::vector<uint8>& Encounters = FieldRuntime->Map().Encounters;
		FKernelState& Kernel = Session.Kernel();
		std::copy_n(Encounters.begin(), std::min(Encounters.size(), Kernel.Formations.size()), Kernel.Formations.begin());
	}

	void FFieldProgram::ReleaseSound()
	{
		if (!bMainEffectsRegistered)
		{
			return;
		}
		FGameSoundSession& Sound = Session.Sound();
		Sound.Sound()->Lock()->UnregisterEffects(Sound.Resident().MainEffects);
		bMainEffectsRegistered = false;
	}
}
