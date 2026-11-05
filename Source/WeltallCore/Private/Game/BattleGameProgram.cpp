#include "Game/BattleGameProgram.h"

#include "Battle/BattleEndingState.h"
#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattleGameSound.h"
#include "Battle/Show/BattlePresentation.h"
#include "Game/BattleGameFiles.h"
#include "Game/BattleSpeed.h"
#include "Game/GameSession.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Sound/GameSoundSession.h"

#include <string>

namespace Weltall
{
	namespace
	{
		/** The music's volume a chained battle sets after a result screen's fanfare, and the steps it takes to it. */
		constexpr int32 ChainedMusicVolume = 0x7F;
		constexpr int32 ChainedMusicSteps = 0x3C;

		std::string BattleProgramShowStop(const FBattleShow& Show, EBattleShowFault Fault, EBattleHudFault HudFault)
		{
			switch (Fault)
			{
			case EBattleShowFault::Sprites:
			{
				const std::optional<FError> Error = Show.Sprites.FaultError();
				return "the battle's show stopped at a " + (Error ? Error->Message : std::string("sprite"));
			}
			case EBattleShowFault::Objects:
				return "the battle's 3D objects stopped: " + (Show.Objects.Fault ? Show.Objects.Fault->Message : std::string("no reason kept"));
			case EBattleShowFault::EventMusic:
				return "the battle's event plays music of its own, which no script of the game does";
			case EBattleShowFault::Hud:
				return "the battle's HUD stopped (its fault " + std::to_string(static_cast<int32>(HudFault)) + ")";
			default:
				return "the battle's show stopped (its fault " + std::to_string(static_cast<int32>(Fault)) + (Show.FaultReason() != nullptr ? std::string(": ") + Show.FaultReason() : std::string()) + ")";
			}
		}
	}

	void EndBattleProgram(const FBattleOutcome& Outcome, FKernelState& Kernel, FGameState& Game)
	{
		if (Outcome.Chain)
		{
			Kernel.ChainedBattle = Outcome.Chain->Formation;
			Kernel.BattleKind = Outcome.Chain->Kind;
		}
		if (Outcome.Movie)
		{
			Kernel.Movie.Type = Outcome.Movie->Type;
			Kernel.Movie.Number = Outcome.Movie->Number;
			Kernel.Movie.NextProgram = EGameProgram::Field;
			Kernel.Movie.NoSkip = Outcome.Movie->NoSkip;
			Kernel.Movie.EndFrame = Outcome.Movie->EndFrame;
		}
		if (Outcome.bFieldStartsFresh)
		{
			Kernel.FieldTrips = 0;
		}
		if (Outcome.FieldReturn)
		{
			Kernel.bEventFadeIn = *Outcome.FieldReturn == 1;
		}
		if (Outcome.bFanfarePlayed)
		{
			Kernel.bResultFanfare = true;
		}
		Kernel.EndBattle(FBattleProgramEnd{Outcome.Result, Outcome.Movie.has_value()}, Game);
	}

	FBattleGameProgram::FBattleGameProgram(FGameSession& InSession) : Session(InSession)
	{
	}

	FBattleGameProgram::~FBattleGameProgram() = default;

	TResult<std::unique_ptr<FBattleGameProgram>> FBattleGameProgram::Start(FGameSession& Session)
	{
		FKernelState& Kernel = Session.Kernel();
		if (Kernel.bBattleDebug)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Battle)) + "'s scene select, which the debug menu reaches, is not run");
		}
		// A chained battle's formation is the one its event named; the battle clears it as it starts.
		const bool bChained = Kernel.ChainedBattle != 0;
		const int32 Formation = bChained ? Kernel.ChainedBattle - 1 : Kernel.Field.BattleFormation;
		Kernel.ChainedBattle = 0;
		const std::optional<std::array<uint8, FKernelState::FormationBytes>> Record = Kernel.Formation(Formation);
		if (!Record)
		{
			return MakeError("the battle's formation " + std::to_string(Formation) + " is past the kernel's table");
		}
		// The setup's reset clears the field's return fade: only the battle's event sets it again.
		Kernel.bEventFadeIn = false;

		FBattleRequest Request;
		Request.Formation = *Record;
		Request.Kind = Kernel.BattleKind;
		Request.bGearsLocked = Kernel.Field.bGearsLocked;
		Request.bPlayersOrder = true;
		Request.EventOptions = Session.Settings().Fixes.BattleEvents;
		Request.RuleOptions = Session.Settings().Fixes.BattleRules;
		Request.bPerfectWorksRebalance = Session.Image().ModChanges().bPerfectWorksRebalance;
		Request.Balance = Session.Settings().Balance;
		Request.AutoAdvance = Session.Settings().AutoAdvance;

		std::unique_ptr<FBattleGameProgram> Program(new FBattleGameProgram(Session));
		FGameSoundSession& Sound = Session.Sound();
		FSoundSequence* Music = Kernel.Music == EKernelMusic::WorldMap ? Sound.WorldMapMusic().CurrentSequence() : Sound.FieldMusic().Sequence();
		Program->Sound = std::make_unique<FBattleGameSound>(*Sound.Sound(), Sound.Resident(), Music);
		if (bChained && Kernel.bResultFanfare)
		{
			Kernel.bResultFanfare = false;
			Program->Sound->SetMusicVolume(ChainedMusicVolume, ChainedMusicSteps);
		}
		Program->Text.Font = &Session.Text().Font;
		Program->Text.Tables = &Session.Text().NameTables;
		Program->Text.State = &Session.Names();
		Program->Text.ButtonMap = MesKernelButtonMap;
		Program->Text.Speed = Session.Settings().TextSpeed;
		Program->Text.Log = Session.MessageLog();
		const FBattleGameFiles& Files = Session.BattleFiles();
		FBattleHudSettings HudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend};
		HudSettings.bJapaneseCommands = Session.Pad().HasJapaneseCommands();
		Program->Hud = std::make_unique<FBattleGameHud>(Files.HudResources, Program->Text, HudSettings, &Files.EventFiles, &Session.Unreached());
		Program->Show = std::make_unique<FBattlePresentation>(
			Files.PresentationFiles(Session.Image(), Session.Index(), Session.Arctangents()), Program->Sound.get(), Program->Hud.get());
		Program->Show->Show.Objects.Culling = Session.Settings().Field.Culling;
		Program->Show->Show.Gte.LightMatrix = Kernel.LightMatrix;
		Program->Show->Show.Options = Session.Settings().Fixes.BattleShow;
		Program->Show->Show.Sprites.bMirroredPartsPlacedByArt = Session.Settings().Fixes.Field.bSpriteFramesAligned;

		FBattleRuntimeServices Services;
		Services.Image = &Session.Image();
		Services.Index = &Session.Index();
		Services.Game = &Session.Game();
		Services.Unreached = &Session.Unreached();
		Services.Random = &Session.Random();
		Services.Pad = &Session.Pad();
		Services.Show = Program->Show.get();
		Services.Tables = &Files.Tables;
		Services.Program = &Files.Program;
		Services.Growth = &Files.Growth;
		Services.Events = &Files.Events;
		Services.Arctangents = &Session.Arctangents();
		Services.Story = Session.Story();
		Services.bExpandedStory = Session.Game().StoryMode() == EStoryMode::Expanded;
		TResult<std::unique_ptr<FBattleRuntime>> Runtime = FBattleRuntime::Begin(Request, Services);
		if (!Runtime)
		{
			return Runtime.Error();
		}
		Program->BattleRuntime = std::move(*Runtime);
		if (auto Error = Program->Sound->TakeError())
		{
			return *Error;
		}
		if (Program->BattleRuntime->Fault() != EBattleFault::None)
		{
			return Program->StopError();
		}
		return Program;
	}

	int32 FBattleGameProgram::VblanksPerFrame() const
	{
		return BattleRuntime->VblanksPerFrame();
	}

	std::optional<FError> FBattleGameProgram::Frame(std::span<const FPadState> HeldPerVblank)
	{
		if (ProgramState != EGameProgramState::Running)
		{
			return MakeError(std::string(GameProgramName(EGameProgram::Battle)) + " has ended");
		}
		for (const FPadState Held : HeldPerVblank)
		{
			Session.Vblank(Held);
			StepSpeed(Held);
		}
		Show->Show.Objects.Culling = Session.Settings().Field.Culling;
		Show->Show.Options = Session.Settings().Fixes.BattleShow;
		Show->Show.Sprites.bMirroredPartsPlacedByArt = Session.Settings().Fixes.Field.bSpriteFramesAligned;
		BattleRuntime->Frame();
		Session.Kernel().LightMatrix = Show->Show.Gte.LightMatrix;
		if (auto Error = Sound->TakeError())
		{
			return Error;
		}
		if (BattleRuntime->IsSoftReset())
		{
			ProgramState = EGameProgramState::SoftReset;
			return std::nullopt;
		}
		if (BattleRuntime->Fault() != EBattleFault::None)
		{
			return StopError();
		}
		if (BattleRuntime->IsOver())
		{
			EndBattleProgram(*BattleRuntime->Outcome(), Session.Kernel(), Session.Game());
			ProgramState = EGameProgramState::Ended;
		}
		return std::nullopt;
	}

	void FBattleGameProgram::StepSpeed(FPadState Held)
	{
		const FPadState Pressed{static_cast<uint16>(Held.Buttons & ~HeldBefore.Buttons)};
		HeldBefore = Held;
		if (BattleRuntime->IsFought())
		{
			Session.BattleSpeed() = StepBattleSpeed(Session.BattleSpeed(), Pressed);
		}
	}

	const FMenuSpriteSheet& FBattleGameProgram::HudSheet() const
	{
		return Session.BattleFiles().HudResources.Sheet;
	}

	std::span<const FTextureRegion> FBattleGameProgram::HudTextureRegions() const
	{
		return Hud ? Hud->TextureRegions() : std::span<const FTextureRegion>();
	}

	FError FBattleGameProgram::StopError() const
	{
		if (BattleRuntime->Fault() == EBattleFault::Show)
		{
			return MakeError(BattleProgramShowStop(Show->Show, Show->Fault(), Hud->Fault()));
		}
		return MakeError("the battle's rules stopped (their fault " + std::to_string(static_cast<int32>(BattleRuntime->Fault())) + ")");
	}
}
