#include "Battle/BattleRuntime.h"

#include "Battle/BattleEnd.h"
#include "Battle/BattleEnding.h"
#include "Battle/BattleEvent.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleProgramShow.h"
#include "Battle/BattleResult.h"
#include "Battle/BattleSetup.h"
#include "Battle/BattleTurnMachine.h"
#include "Battle/BattleWaits.h"
#include "Battle/Show/BattleShowTransition.h"
#include "Game/GameState.h"
#include "Input/KernelPad.h"
#include "Story/StoryPacks.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Weltall
{
	namespace
	{
		/** L1, R1, Start and Select together: the console restarts. */
		constexpr uint16 SoftResetButtons = 0x090C;
		/** The kind whose start plays no sounds, whose file's effects are then not stopped either. */
		constexpr uint8 SilentKind = 4;
		/** The setup's stages as the transitions' loops count them, one a frame from the first: then they are done. */
		enum ESetupLoopStage : int32
		{
			CombatantsStage = 1,
			StageFileStage = 2,
			PlacementStage = 3,
			OrderStage = 4,
			SetupStagesDone = 5,
		};

		bool HasSetupTransition(uint8 Kind)
		{
			return Kind >= 1 && Kind <= 4;
		}

		/** The setup's transition of each battle kind from 1 to 4. */
		constexpr std::array<EBattleScreenMesh, 4> SetupTransitions{EBattleScreenMesh::Triangles, EBattleScreenMesh::Wave,
			EBattleScreenMesh::WaveSine, EBattleScreenMesh::TrianglesStill};
	}

	TResult<std::unique_ptr<FBattleRuntime>> FBattleRuntime::Begin(const FBattleRequest& Request, const FBattleRuntimeServices& Services)
	{
		const FBattleFormation Formation = FBattleFormation::FromRecord(Request.Formation);
		TResult<FBattleScene> Scene = FBattleScene::Load(*Services.Image, *Services.Index, Formation.Scene);
		if (!Scene)
		{
			return Scene.Error();
		}
		TResult<FBattleStage> Stage = FBattleStage::Load(*Services.Image, *Services.Index, Formation.Stage);
		if (!Stage)
		{
			return Stage.Error();
		}
		if ((Formation.Flags & FormationFlags::EventBattle) != 0 && Formation.EventScript >= Services.Events->Count())
		{
			return MakeError("the battle's event script " + std::to_string(Formation.EventScript) + " is past the event archive's");
		}
		std::unique_ptr<FBattleRuntime> Runtime(new FBattleRuntime(Request, Services, std::move(*Scene), std::move(*Stage)));
		FBattleContext& Battle = *Runtime->Battle;
		Services.Show->BeginBattle(Battle);
		BeginBattleSetup(Battle, Formation, Runtime->Scene);
		Services.Show->PlayOpeningCue(EBattleOpeningCue::StartSounds, Runtime->Kind);
		if (HasSetupTransition(Runtime->Kind))
		{
			Runtime->SetupLoop.FramesLeft = SetupTransitionFrames;
			Runtime->SetupMesh = std::make_unique<FBattleScreenMesh>();
			Runtime->SetupMesh->Create(SetupTransitions[static_cast<std::size_t>(Runtime->Kind - 1)]);
		}
		else
		{
			Runtime->Transition = std::make_unique<FBattleDefaultTransition>();
			Runtime->Transition->Start(*Services.Random, *Services.Arctangents);
		}
		Runtime->TransitionFrame();
		if (Services.Show->HasStopped())
		{
			Battle.Fault = EBattleFault::Show;
			Runtime->Pc = EPc::Over;
		}
		return Runtime;
	}

	FBattleRuntime::FBattleRuntime(const FBattleRequest& Request, const FBattleRuntimeServices& InServices, FBattleScene InScene, FBattleStage InStage)
		: Services(InServices)
		, Kind(Request.Kind)
		, Scene(std::move(InScene))
		, Stage(std::move(InStage))
		, Battle(std::make_unique<FBattleContext>())
	{
		FBattleContext& Context = *Battle;
		Context.GameState = Services.Game;
		Context.Random = Services.Random;
		Context.Show = Services.Show;
		Context.Program = Services.Program;
		Context.Arctangents = Services.Arctangents;
		Context.bGearsLocked = Request.bGearsLocked;
		Context.bPlayersOrder = Request.bPlayersOrder;
		Context.EventOptions = Request.EventOptions;
		Context.RuleOptions = Request.RuleOptions;
		Context.bPerfectWorksRebalance = Request.bPerfectWorksRebalance;
		Context.Balance = Request.Balance;
		Context.AutoAdvance = Request.AutoAdvance;
	}

	FBattleRuntime::~FBattleRuntime() = default;

	const FBattleDefaultTransition* FBattleRuntime::DefaultTransition() const
	{
		return Pc == EPc::Transition ? Transition.get() : nullptr;
	}

	const FBattleScreenMesh* FBattleRuntime::SetupTransition() const
	{
		return Pc == EPc::Transition ? SetupMesh.get() : nullptr;
	}

	int32 FBattleRuntime::TransitionBuffer() const
	{
		return Transition ? Transition->Buffer() : SetupLoop.Buffer;
	}

	int32 FBattleRuntime::VblanksPerFrame() const
	{
		return Pc == EPc::Transition ? 2 : 1;
	}

	FBattleInputServices FBattleRuntime::InputServices()
	{
		FBattleInputServices Input;
		Input.Context = Battle.get();
		Input.Pad = Services.Pad;
		Input.Show = Services.Show;
		Input.Game = Services.Game;
		return Input;
	}

	void FBattleRuntime::Frame()
	{
		if (Pc == EPc::Over)
		{
			return;
		}
		Step();
		if (Pc != EPc::Over && Services.Show->HasStopped())
		{
			Battle->Fault = EBattleFault::Show;
			Pc = EPc::Over;
		}
	}

	void FBattleRuntime::Step()
	{
		if (FrameState != EFrameState::Idle)
		{
			ContinueFrame();
			return;
		}
		if (Pc == EPc::Transition)
		{
			TransitionFrame();
			return;
		}
		// A call of the show's that the rules' code is inside goes on first; the routine goes on once it has returned.
		if (bRulesResume && Services.Show->ResumeRules() && !Resume())
		{
			return;
		}
		RunFrame();
	}

	void FBattleRuntime::TransitionFrame()
	{
		int32 Number = 0;
		// The loop's check comes first: the frame after its last runs the setup's end and the opening instead.
		if (Transition)
		{
			if (Transition->IsDone())
			{
				Open();
				return;
			}
			Number = Transition->Frame();
		}
		else
		{
			if (SetupLoop.FramesLeft == 0 && SetupLoop.Stage == SetupStagesDone)
			{
				Open();
				return;
			}
			if (SetupLoop.FramesLeft > 0)
			{
				--SetupLoop.FramesLeft;
			}
			SetupLoop.Buffer = 1 - SetupLoop.Buffer;
			SetupMesh->Step(SetupLoop.Buffer);
			if (SetupLoop.Stage < SetupStagesDone)
			{
				Number = SetupLoop.Stage++;
			}
		}
		++TransitionFramesRun;
		RunSetupStage(Number);
		if (Battle->Fault != EBattleFault::None)
		{
			Pc = EPc::Over;
			return;
		}
		if (Services.Pad->Held() == SoftResetButtons)
		{
			bSoftReset = true;
			Pc = EPc::Over;
		}
	}

	void FBattleRuntime::RunSetupStage(int32 Number)
	{
		FBattleContext& Context = *Battle;
		switch (Number)
		{
		case CombatantsStage:
			RunBattleSetupStage(Context, EBattleSetupStage::Combatants, *Services.Tables, Scene);
			break;
		case PlacementStage:
			RunBattleSetupStage(Context, EBattleSetupStage::Placement, *Services.Tables, Scene);
			break;
		case OrderStage:
			RunBattleSetupStage(Context, EBattleSetupStage::Order, *Services.Tables, Scene);
			break;
		case StageFileStage:
			Context.Stage = Stage;
			break;
		default:
			break;
		}
	}

	void FBattleRuntime::Open()
	{
		FBattleContext& Context = *Battle;
		IBattleProgramShow& Show = *Services.Show;
		Show.PlayOpeningCue(EBattleOpeningCue::SetupFinished, 0);
		CheckBattleEnd(Context);
		Context.Party.InputMode = BattleInputModes::Messages;
		Context.bWaitsRunning = true;
		Show.PlayOpeningCue(EBattleOpeningCue::Presentation, Context.bEventBattle ? 1 : 0);
		Pc = EPc::AfterFirstFrame;
		RunFrame();
	}

	bool FBattleRuntime::Resume()
	{
		FBattleContext& Context = *Battle;
		IBattleProgramShow& Show = *Services.Show;
		for (;;)
		{
			if (Context.Fault != EBattleFault::None)
			{
				Pc = EPc::Over;
				return false;
			}
			switch (Pc)
			{
			case EPc::AfterFirstFrame:
				Show.PlayOpeningCue(EBattleOpeningCue::MessageBoxes, 0);
				Pc = EPc::Intro;
				break;
			case EPc::Intro:
				if (!Show.HasStarted())
				{
					return true;
				}
				Show.PlayOpeningCue(EBattleOpeningCue::StartSoundsReleased, Kind != SilentKind ? 1 : 0);
				if (Context.bEventBattle)
				{
					const uint8 Script = Context.Formation.EventScript;
					std::span<const uint8> Messages = Services.Events->Messages(Script);
					if (Services.Story != nullptr)
					{
						if (std::optional<std::vector<uint8>> Changed = Services.Story->BattleEventMessages(Script, Messages, Services.Events->DiscMessages(Script),
								Services.bExpandedStory))
						{
							StoryEventMessages = std::move(*Changed);
							Messages = StoryEventMessages;
						}
					}
					BeginBattleEvent(Context, Services.Events->Script(Script), Messages, Services.Events->Layout(), Services.Events->Replaced(Script));
				}
				Pc = EPc::EventLoad;
				break;
			case EPc::EventLoad:
				if (Context.bEventBattle)
				{
					if (StepBattleEvent(Context))
					{
						return true;
					}
					BeginBattleEventRun(Context);
				}
				Pc = EPc::EventOpening;
				break;
			case EPc::EventOpening:
				if (Context.bEventBattle && StepBattleEvent(Context))
				{
					return true;
				}
				if (!Context.Event.Requests.bEndedBattle)
				{
					Context.Party.InputMode = BattleInputModes::Commands;
				}
				FinishBattlePartySetup(Context);
				for (std::size_t Slot = 0; Slot < FBattleContext::PartySlotCount; ++Slot)
				{
					const uint8 Character = Context.PartyCharacters[Slot];
					if (Character != FBattleContext::NoCharacter)
					{
						Context.SkillsAtStart[Slot] = {
							BattleGameState::Word(*Services.Game, BattleGameState::SkillsOffset(Character, GameStateLayout::Skills::Deathblows)),
							BattleGameState::Word(*Services.Game, BattleGameState::SkillsOffset(Character, GameStateLayout::Skills::Abilities)),
						};
					}
				}
				Pc = EPc::Loop;
				break;
			case EPc::Loop:
				if (Context.End != BattleEnd::Running || Context.Event.Requests.bEndedBattle)
				{
					BeginEnd();
					break;
				}
				if (Show.HasStarted())
				{
					if (Context.ScriptedTurns != 0)
					{
						Context.Fault = EBattleFault::ScriptedTurns;
						break;
					}
					PickBattleActor(Context);
					if (Context.Fault != EBattleFault::None)
					{
						break;
					}
					BeginBattleTurn(Context, false);
					if (Context.Turn.bRunning)
					{
						Pc = EPc::Turn;
					}
				}
				return true;
			case EPc::Turn:
				if (!StepBattleTurn(Context, Context.Party.Event))
				{
					Pc = EPc::Loop;
				}
				return true;
			case EPc::Ending:
				if (StepBattleEnding(Context))
				{
					return true;
				}
				BeginBattleResult(Context);
				Pc = EPc::Result;
				return true;
			case EPc::Result:
				if (StepBattleResult(Context, *Services.Growth, Services.Game->ProgramResult))
				{
					return true;
				}
				// The battle's last clean-up takes a frame more when the frame before it drew into the first buffer.
				Pc = EPc::LastFrame;
				if (Show.DrewFirstBuffer())
				{
					return true;
				}
				break;
			case EPc::LastFrame:
				Finish();
				return false;
			default:
				return false;
			}
		}
	}

	bool FBattleRuntime::IsFought() const
	{
		return Pc < EPc::Result && !(Pc >= EPc::Loop && IsPartyDown(*Battle));
	}

	void FBattleRuntime::BeginEnd()
	{
		BeginBattleEnding(*Battle);
		Pc = EPc::Ending;
	}

	void FBattleRuntime::Finish()
	{
		FBattleContext& Context = *Battle;
		if (Context.Fault == EBattleFault::None)
		{
			BattleOutcome = TakeBattleOutcome(Context);
		}
		Pc = EPc::Over;
	}

	void FBattleRuntime::RunFrame()
	{
		if (Services.Pad->Held() == SoftResetButtons)
		{
			bSoftReset = true;
			Pc = EPc::Over;
			return;
		}
		Services.Show->BeginFrame();
		CatchUpLeft = Services.Show->CatchUpSteps();
		if (Services.Show->IsHolding())
		{
			// The show's own vertical blanks pass first, one a call.
			FrameState = EFrameState::ShowHold;
			return;
		}
		FrameState = EFrameState::Input;
		ContinueFrame();
	}

	void FBattleRuntime::ContinueFrame()
	{
		switch (FrameState)
		{
		case EFrameState::ShowHold:
			Services.Show->HoldBlank();
			if (Services.Show->IsHolding())
			{
				return;
			}
			[[fallthrough]];
		case EFrameState::Input:
			if (!InputStep.Run(InputServices()))
			{
				FrameState = EFrameState::InputHeld;
				return;
			}
			break;
		case EFrameState::InputHeld:
			if (!InputStep.Resume(InputServices()))
			{
				return;
			}
			break;
		case EFrameState::Idle:
			return;
		}
		// The input step once more for each step the frame catches up.
		while (CatchUpLeft > 0)
		{
			--CatchUpLeft;
			if (!InputStep.RunCatchUp(InputServices()))
			{
				FrameState = EFrameState::InputHeld;
				return;
			}
		}
		FrameState = EFrameState::Idle;
		FinishFrame();
	}

	void FBattleRuntime::FinishFrame()
	{
		bRulesResume = Services.Show->FinishFrame();
	}
}
