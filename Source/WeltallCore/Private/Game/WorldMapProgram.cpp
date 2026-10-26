#include "Game/WorldMapProgram.h"

#include "Field/FieldRequests.h"
#include "Game/GameMenus.h"
#include "Game/GameSession.h"
#include "Sound/GameSoundSession.h"
#include "Text/MesWindow.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapScene.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		/** The menu the world map opens: the normal menu, with saving allowed. */
		constexpr uint8 MenuMode = 0;
		constexpr uint8 MenuParam = 1;
		/** The result the Menu leaves when it loaded a save. */
		constexpr uint8 SaveLoadedResult = 2;
		/** The kind of start a random encounter's battle makes. */
		constexpr uint8 EncounterBattleKind = 0;

		/** A fade of the screen: its steps, the first step's level and how far each step moves it. */
		struct FFadeSteps
		{
			int32 Steps = 0;
			int32 First = 0;
			int32 Step = 0;
		};
		constexpr FFadeSteps EntryFade{64, 0, 4};
		constexpr FFadeSteps MenuFadeOut{16, 0, 8};
		constexpr FFadeSteps MenuFadeIn{16, 0x80, -8};

		/**
		 * The level a fade's frame shows: the first only the display's picture again, then a step a frame; its steps alternate
		 * between the two draw buffers, so the frames after the last show the step before it.
		 */
		int32 FadeLevel(int32 Frame, const FFadeSteps& Fade)
		{
			if (Frame == 0)
			{
				return -1;
			}
			return Fade.First + Fade.Step * (Frame <= Fade.Steps ? Frame - 1 : Fade.Steps - 2);
		}
	}

	static_assert(FWorldMapProgram::EntryFadeFrames == EntryFade.Steps + 2);
	static_assert(FWorldMapProgram::MenuFadeOutFrames == MenuFadeOut.Steps + 2);
	static_assert(FWorldMapProgram::MenuFadeInFrames == MenuFadeIn.Steps + 3);

	FWorldMapProgram::FWorldMapProgram(FGameSession& InSession, FWorldMapTables InTables, FWorldMapDrawingTables InDrawingTables)
		: Session(InSession), Tables(std::move(InTables)), DrawingTables(std::move(InDrawingTables))
	{
	}

	FWorldMapProgram::~FWorldMapProgram() = default;

	TResult<std::unique_ptr<FWorldMapProgram>> FWorldMapProgram::Start(FGameSession& Session)
	{
		TResult<FWorldMapTables> Tables = FWorldMapTables::Read(Session.Image(), Session.Index(), GameplayRelease(Session.Release()));
		if (!Tables)
		{
			return Tables.Error();
		}
		TResult<FWorldMapDrawingTables> DrawingTables = FWorldMapDrawingTables::Read(Session.Image(), Session.Index(), GameplayRelease(Session.Release()));
		if (!DrawingTables)
		{
			return DrawingTables.Error();
		}
		std::unique_ptr<FWorldMapProgram> Program(new FWorldMapProgram(Session, std::move(*Tables), std::move(*DrawingTables)));
		Program->Keep = Session.Kernel().WorldMapKeep;
		// Every scene's start loads the place models' light, and nothing the world map draws loads another.
		Session.Kernel().LightMatrix = Program->DrawingTables.LightMatrix;

		FWorldMapServices Services;
		Services.Image = &Session.Image();
		Services.Index = &Session.Index();
		Services.Tables = &Program->Tables;
		Services.Arctangents = &Session.Arctangents();
		Services.Game = &Session.Game();
		Services.Random = &Session.Random();
		Services.Pad = &Session.Pad();
		Services.Clock = &Session.Clock();
		Services.Text.Font = &Session.Text().Font;
		Services.Text.Tables = &Session.Text().NameTables;
		Services.Text.State = &Session.Names();
		Services.Text.ButtonMap = MesKernelButtonMap;
		Services.Text.Speed = Session.Settings().TextSpeed;
		Services.BattleKeep = Program->Keep.get();
		Services.Sound = &Session.Sound().WorldMapMusic();
		Services.Options = Session.Settings().Fixes.WorldMap;
		Services.bMirroredSpritePartsPlacedByArt = Session.Settings().Fixes.Field.bSpriteFramesAligned;
		Services.EncounterRate = Session.Settings().Balance.Encounters;
		Program->SettingsSeen = Session.SettingsRevision();
		Services.GearLock = Session.Kernel().Field.bGearsLocked ? 1 : 0;
		Services.Story = Session.Story();
		Program->MapRuntime = std::make_unique<FWorldMapRuntime>(Services);
		if (auto Error = Program->MapRuntime->Enter())
		{
			return *Error;
		}
		if (auto Error = Session.Sound().WorldMapMusic().TakeError())
		{
			return *Error;
		}
		Program->Presenter.emplace(Program->DrawingTables, Session.Image(), Session.Index(), Session.FieldTables().TextClut);
		Program->Presenter->SetGearStandIns(Session.Settings().Field.GearModels);
		if (auto Error = Program->Presenter->EnterScene(*Program->MapRuntime))
		{
			return *Error;
		}
		Program->Phase = EPhase::EntryFade;
		Program->FramesLeft = EntryFadeFrames;
		return Program;
	}

	int32 FWorldMapProgram::VblanksPerFrame() const
	{
		switch (Phase)
		{
		case EPhase::Loop:
			return MapRuntime->VblanksPerTick();
		case EPhase::Menu:
			return Menu->VblanksPerFrame();
		default:
			return 1;
		}
	}

	void FWorldMapProgram::RunVblanks(std::span<const FPadState> HeldPerVblank)
	{
		for (const FPadState Held : HeldPerVblank)
		{
			Session.Vblank(Held);
		}
	}

	void FWorldMapProgram::AddViewInput(const FFieldViewInput& Input)
	{
		if (Phase == EPhase::Loop && bViewMoves)
		{
			PendingView.Add(Input);
		}
	}

	FWorldMapCameraOffsets FWorldMapProgram::LiveViewOffsets() const
	{
		if (Phase == EPhase::Loop && bViewMoves)
		{
			return MoveWorldMapCameraOffsets(Session.Settings().WorldMapCamera, PendingView);
		}
		const FWorldMapFrame* Shown = LatestFrame();
		return Shown != nullptr ? Shown->ShownOffsets : FWorldMapCameraOffsets{};
	}

	std::optional<FError> FWorldMapProgram::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (ProgramState != EGameProgramState::Running)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::WorldMap)) + " has ended");
		}
		if (Session.SettingsRevision() != SettingsSeen)
		{
			// A fix the player changed in the menu holds from the map's next tick, as do how the gears are drawn and how
			// often battles come.
			MapRuntime->Context().Options = Session.Settings().Fixes.WorldMap;
			MapRuntime->Context().Sprites.bMirroredPartsPlacedByArt = Session.Settings().Fixes.Field.bSpriteFramesAligned;
			MapRuntime->Context().Encounters.Rate = Session.Settings().Balance.Encounters;
			Presenter->SetGearStandIns(Session.Settings().Field.GearModels);
			SettingsSeen = Session.SettingsRevision();
		}
		const bool bMenu = Phase == EPhase::Menu;
		std::optional<FError> Error = RunFrame(HeldPerVblank);
		// The card's time stands still with the game's: in the menu, and in the pause's frames.
		const bool bPaused = Phase == EPhase::Loop && MapRuntime->LastFrame() == EWorldMapFrame::Paused;
		if (!Error && !bMenu && !bPaused)
		{
			AdvanceChapterCard(static_cast<int32>(HeldPerVblank.size()));
		}
		return Error;
	}

	void FWorldMapProgram::AdvanceChapterCard(int32 Vblanks)
	{
		// In control: a tick of the map past its fade, the party the pad's, nothing asked of the program.
		const FWorldMapContext& Context = MapRuntime->Context();
		const bool bInControl = Session.Settings().bChapterCards && Phase == EPhase::Loop && ProgramState == EGameProgramState::Running && !Context.bCutscene
			&& Context.Fading == 0 && Context.Running != 0 && Context.MenuRequested == 0 && MapRuntime->Request().Kind == EWorldMapRequest::None;
		Session.ChapterCard().Advance(Session.Chapter(), Session.Subtitle(), bInControl, Vblanks);
	}

	std::optional<FError> FWorldMapProgram::RunFrame(std::span<const FPadState> HeldPerVblank)
	{
		if (Phase != EPhase::Loop)
		{
			bPreviousFollows = false;
		}
		LastFade = FadeShown();
		switch (Phase)
		{
		case EPhase::EntryFade:
			RunVblanks(HeldPerVblank);
			if (--FramesLeft == 0)
			{
				// The scene's start empties the pad's queue after its fade, before the loop's first pass.
				Session.Pad().Reset();
				Phase = EPhase::Loop;
			}
			return std::nullopt;
		case EPhase::Loop:
		{
			const int32 Next = Latest < 0 ? 0 : 1 - Latest;
			// The tick's frame is drawn under the player's view, and its d-pad and sprites follow the view's turn.
			FWorldMapCameraOffsets& Offsets = Session.WorldMapCameraOffsets();
			const FWorldMapCameraOffsets Kept = Offsets;
			const bool bPlayerView = !MapRuntime->Context().bCutscene;
			if (bPlayerView)
			{
				Offsets = MoveWorldMapCameraOffsets(Offsets, PendingView);
			}
			PendingView = {};
			Presenter->SetPlayerView(bPlayerView ? Offsets : FWorldMapCameraOffsets{});
			MapRuntime->Context().ViewTurn = bPlayerView ? WorldMapViewTurn(Offsets) : int16{0};
			MapRuntime->Context().bAnalogMovement = Session.Settings().Controls.bAnalogMovement;
			if (auto Error = Presenter->Tick(*MapRuntime, HeldPerVblank, Culling, Frames[static_cast<std::size_t>(Next)]))
			{
				return Error;
			}
			if (auto Error = Session.Sound().WorldMapMusic().TakeError())
			{
				return Error;
			}
			if (!Presenter->DrewFrame())
			{
				// A frame of the pause draws nothing: the view stays as the last frame shows it.
				Offsets = Kept;
			}
			bViewMoves = bPlayerView && Presenter->DrewFrame();
			bPreviousFollows = Presenter->DrewFrame() && Latest >= 0;
			if (Presenter->DrewFrame())
			{
				Latest = Next;
			}
			return TakeRequest();
		}
		case EPhase::MenuFadeOut:
			RunVblanks(HeldPerVblank);
			if (--FramesLeft == 0)
			{
				return OpenMenu();
			}
			return std::nullopt;
		case EPhase::Menu:
			if (auto Error = Menu->Frame(HeldPerVblank))
			{
				return Error;
			}
			if (Menu->State() == EGameProgramState::SoftReset)
			{
				ProgramState = EGameProgramState::SoftReset;
				return std::nullopt;
			}
			if (Menu->State() == EGameProgramState::Ended)
			{
				Menu.reset();
				if (Session.Game().ProgramResult == SaveLoadedResult)
				{
					return GoOnWithLoadedGame();
				}
				Session.Game().ProgramResult = ResultBeforeMenu;
				Phase = EPhase::MenuFadeIn;
				FramesLeft = MenuFadeInFrames;
			}
			return std::nullopt;
		case EPhase::MenuFadeIn:
			RunVblanks(HeldPerVblank);
			if (--FramesLeft == 0)
			{
				Phase = EPhase::Loop;
				return MapRuntime->ResumeAfterMenu();
			}
			return std::nullopt;
		}
		return std::nullopt;
	}

	FWorldMapScreenFade FWorldMapProgram::FadeShown() const
	{
		using EPicture = FWorldMapScreenFade::EPicture;
		switch (Phase)
		{
		case EPhase::EntryFade:
			return {EPicture::ScreenBefore, FadeLevel(EntryFadeFrames - FramesLeft, EntryFade), MapRuntime->EntryFadeBlend()};
		case EPhase::MenuFadeOut:
			return {EPicture::LatestFrame, FadeLevel(MenuFadeOutFrames - FramesLeft, MenuFadeOut)};
		case EPhase::MenuFadeIn:
			return {EPicture::LatestFrame, FadeLevel(MenuFadeInFrames - FramesLeft, MenuFadeIn)};
		default:
			return {};
		}
	}

	const FWorldMapFrame* FWorldMapProgram::LatestFrame() const
	{
		return Latest < 0 ? nullptr : &Frames[static_cast<std::size_t>(Latest)];
	}

	const FWorldMapFrame* FWorldMapProgram::PreviousFrame() const
	{
		return bPreviousFollows ? &Frames[static_cast<std::size_t>(1 - Latest)] : nullptr;
	}

	std::optional<FError> FWorldMapProgram::TakeRequest()
	{
		const FWorldMapRequest& Request = MapRuntime->Request();
		switch (Request.Kind)
		{
		case EWorldMapRequest::None:
			return std::nullopt;
		case EWorldMapRequest::Menu:
			Phase = EPhase::MenuFadeOut;
			FramesLeft = MenuFadeOutFrames;
			return std::nullopt;
		case EWorldMapRequest::SoftReset:
			ProgramState = EGameProgramState::SoftReset;
			return std::nullopt;
		case EWorldMapRequest::Field:
		case EWorldMapRequest::Battle:
			return Leave(Request);
		}
		return std::nullopt;
	}

	std::optional<FError> FWorldMapProgram::OpenMenu()
	{
		// What the world map hands the menu. The original's reads no result of it and leaves the one before it standing,
		// a battle's among them, whose "run from" is the number of a game loaded: the menu starts from none, and
		// the one before it is put back unless a game was loaded.
		ResultBeforeMenu = Session.Game().ProgramResult;
		Session.Game().ProgramResult = 0;
		FKernelState& Kernel = Session.Kernel();
		Kernel.bMenuStandalone = false;
		Kernel.MenuMode = MenuMode;
		Kernel.Field.MenuParam = MenuParam;
		FFieldRequest Request;
		Request.Kind = EFieldRequest::Menu;
		Request.MenuMode = MenuMode;
		Request.MenuParam = MenuParam;
		Request.bMenuByButton = true;
		Request.bGearBoardingLocked = MapRuntime->Context().GearLock != 0;
		if (Session.Menus() == nullptr)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Menu)) + " (mode " + std::to_string(MenuMode) + ", parameter " +
				std::to_string(MenuParam) + ", from the world map) is not run yet");
		}
		TResult<std::unique_ptr<IGameProgram>> Opened = Session.Menus()->Open(Session, Request);
		if (!Opened)
		{
			return Opened.Error();
		}
		Menu = std::move(*Opened);
		Phase = EPhase::Menu;
		return std::nullopt;
	}

	std::optional<FError> FWorldMapProgram::GoOnWithLoadedGame()
	{
		Session.Sound().WorldMapMusic().StopAllSongs();
		FWorldMapRequest Request;
		Request.NextProgram = static_cast<uint8>(LoadedGameProgram(Session.Game()));
		return Leave(Request);
	}

	std::optional<FError> FWorldMapProgram::Leave(const FWorldMapRequest& Request)
	{
		FKernelState& Kernel = Session.Kernel();
		Kernel.NextProgram = static_cast<EGameProgram>(Request.NextProgram);
		Kernel.Field.bGearsLocked = MapRuntime->Context().GearLock != 0;
		if (Request.Kind == EWorldMapRequest::Battle)
		{
			// The region's formations, the one drawn among them, and what the world map keeps for its return.
			const FWorldMapRegion& Region = MapRuntime->Scene().Regions[static_cast<std::size_t>(Request.Region)];
			auto Out = Kernel.Formations.begin();
			for (const auto& Record : Region.Formations)
			{
				Out = std::copy(Record.begin(), Record.end(), Out);
			}
			Kernel.Field.BattleFormation = Request.Formation;
			Kernel.BattleKind = EncounterBattleKind;
			Kernel.bBattleDebug = false;
			Kernel.Music = EKernelMusic::WorldMap;
			Kernel.WorldMapKeep = Request.Keep;
		}
		ProgramState = EGameProgramState::Ended;
		return std::nullopt;
	}
}
