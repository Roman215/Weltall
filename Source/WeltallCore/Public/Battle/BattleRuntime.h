#pragma once

#include "Battle/BattleContext.h"
#include "Battle/BattleEndingState.h"
#include "Battle/BattleEventState.h"
#include "Battle/BattleInputStep.h"
#include "Battle/BattleScene.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace Weltall
{
	class FBattleDefaultTransition;
	class FBattleScreenMesh;
	class FBattleEventArchive;
	class FBattleGrowthTable;
	class FBattleProgramTables;
	class FBattleTables;
	class FCdImage;
	class FGameFileIndex;
	class FGameState;
	class FKernelPad;
	class FPsxRandom;
	class FStoryPacks;
	class IBattleProgramShow;
	struct FGteArctangents;

	/** A battle to fight, as the program before it hands it over. */
	struct FBattleRequest
	{
		/** The formation's record, as the kernel's table holds it. */
		std::array<uint8, FBattleFormation::RecordSize> Formation{};
		/** How the battle starts: its sounds, its transition, the party's entrance (0 to 5, and beyond as 0). */
		uint8 Kind = 0;
		/** The field's gears cannot be boarded or left where the battle began. */
		bool bGearsLocked = false;
		/** The party fights in the order its player keeps it (FPartyOrder), but in the battles an event stages. Off: the game's order. */
		bool bPlayersOrder = false;
		/** The fixes of the battle events' broken effects in use. */
		FBattleEventOptions EventOptions;
		/** The fixes of the battle rules' broken effects in use. */
		FBattleRuleOptions RuleOptions;
		/** Perfect Works Build's rebalance in play: its rules of battle in place of the original's. */
		bool bPerfectWorksRebalance = false;
		/** The player's changes to the balance: how much experience and gold the battle gives. */
		FGameBalance Balance;
		/** The player's auto-advancing text, which the battle events' messages follow. */
		EMesAutoAdvance AutoAdvance = EMesAutoAdvance::Off;
	};

	/** What a battle reads and changes but does not own; everything must outlive the battle. */
	struct FBattleRuntimeServices
	{
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		FGameState* Game = nullptr;
		FPsxRandom* Random = nullptr;
		FKernelPad* Pad = nullptr;
		IBattleProgramShow* Show = nullptr;
		const FBattleTables* Tables = nullptr;
		const FBattleProgramTables* Program = nullptr;
		const FBattleGrowthTable* Growth = nullptr;
		const FBattleEventArchive* Events = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		/** The port's story, whose documents may change an event's text, and whether the game plays the second disc expanded; none: the disc's own. */
		const FStoryPacks* Story = nullptr;
		bool bExpandedStory = false;
	};

	/**
	 * The Battle program's run of one battle, a frame at a time: its start (the setup's stages inside the
	 * transition of the battle's kind, the end check, the show begun, the intro's frames, the event's load
	 * and opening run, the party's finish), the battle's loop (the next actor picked while the battle runs,
	 * its turn frame by frame), the end (the sorting, the event's last run, the pose, the dim) and the result,
	 * each frame the show's and the input step's work in the original's order. A frame ends where the
	 * console waits for a vertical blank: the transition's frames last two, the battle's one, and a pause or
	 * the wait for a controller holds a frame one vertical blank at a time.
	 */
	class FBattleRuntime
	{
	public:
		/** The battles of kinds 1 to 4 start with the setup's own transitions, this many frames of two vertical blanks. */
		static constexpr int32 SetupTransitionFrames = 0x52;

		/**
		 * The battle begun, as the program starts: its formation's scene and stage read from the disc, the
		 * battle's memory made fresh, its start sounds, the transition begun and its first frame run.
		 */
		WELTALLCORE_API static TResult<std::unique_ptr<FBattleRuntime>> Begin(const FBattleRequest& Request, const FBattleRuntimeServices& Services);

		WELTALLCORE_API ~FBattleRuntime();
		FBattleRuntime(const FBattleRuntime&) = delete;
		FBattleRuntime& operator=(const FBattleRuntime&) = delete;

		/** The vertical blanks before the next frame: two in the transition, one after. */
		WELTALLCORE_API int32 VblanksPerFrame() const;

		/** The next frame, once its vertical blanks have passed (the kernel's pad has queued them). */
		WELTALLCORE_API void Frame();

		/** The battle is over: its result is done (Outcome), or the pad restarted the console, or a fault stopped it. */
		bool IsOver() const { return Pc == EPc::Over; }
		/**
		 * The frame begun waits on vertical blanks its show takes for itself (a screen wave's own frames): each
		 * Frame is one of them, and the frame goes on with its input step once they have passed.
		 */
		bool IsHeldByShow() const { return FrameState == EFrameState::ShowHold; }
		bool IsSoftReset() const { return bSoftReset; }
		/** What stopped the battle's rules, if anything (Show: the show's own fault names it). */
		EBattleFault Fault() const { return Battle->Fault; }
		/** What the battle leaves the programs after it, once it is over. */
		const std::optional<FBattleOutcome>& Outcome() const { return BattleOutcome; }

		const FBattleContext& Context() const { return *Battle; }
		const FBattleInputStep& Input() const { return InputStep; }
		/** The default transition while it runs (the battles of kinds 0 and 5 on), null otherwise. */
		WELTALLCORE_API const FBattleDefaultTransition* DefaultTransition() const;
		/** The mesh of the setup's own transition while it runs (the battles of kinds 1 to 4), null otherwise. */
		WELTALLCORE_API const FBattleScreenMesh* SetupTransition() const;
		/** Whether the battle is still in its transition. */
		bool IsInTransition() const { return Pc == EPc::Transition; }
		/**
		 * Whether the battle is still being fought: from its transition to its ending's last frame, before its result's
		 * routine; not once the whole party is down (its fall, the defeat's end and the game over at the game's pace).
		 */
		WELTALLCORE_API bool IsFought() const;
		/** The draw buffer the transition's last frame drew into. */
		WELTALLCORE_API int32 TransitionBuffer() const;
		/** The frames the transition has run. */
		int32 TransitionFrames() const { return TransitionFramesRun; }

	private:
		/** Where the program's routine stands between frames. */
		enum class EPc : uint8
		{
			Transition,
			/** The opening's first frame has run: the message boxes, then the intro's frames. */
			AfterFirstFrame,
			Intro,
			EventLoad,
			EventOpening,
			Loop,
			Turn,
			Ending,
			/** The result's routine, then the frame the last clean-up may take. */
			Result,
			LastFrame,
			Over,
		};

		/** The loop the setup module's own transitions run (kinds 1 to 4): its frames, the setup's stage next, the draw buffer. */
		struct FSetupLoop
		{
			int32 FramesLeft = 0;
			int32 Stage = 1;
			int32 Buffer = 0;
		};

		FBattleRuntime(const FBattleRequest& Request, const FBattleRuntimeServices& Services, FBattleScene InScene, FBattleStage InStage);

		/** The frame: a held one's input step, the transition's, or the routine to its next frame and RunFrame. */
		void Step();
		/** One frame of the transition's loop; its end runs the setup's end and the opening to its first frame. */
		void TransitionFrame();
		/** The setup's stage the transition's frame reached: 1 the combatants, 2 the stage's file, 3 the placement, 4 the order. */
		void RunSetupStage(int32 Number);
		/** The setup's end and the program's opening up to its first frame. */
		void Open();
		/** The routine from the last frame to its next; false when it ended the battle instead. */
		bool Resume();
		/** A battle frame: the soft reset's check, the show to the input step, the input steps, the show's rest. */
		void RunFrame();
		/** The frame begun from where it stands: the show's hold, its input step, its catch-up steps, its end. */
		void ContinueFrame();
		void FinishFrame();
		/** The battle's end reached: the ending's routine begun. */
		void BeginEnd();
		/** The result's rules, then what the battle leaves. */
		void Finish();
		FBattleInputServices InputServices();

		FBattleRuntimeServices Services;
		/** The event's message table where the port's story changed it: the event reads it for the whole battle. */
		std::vector<uint8> StoryEventMessages;
		uint8 Kind = 0;
		FBattleScene Scene;
		/** The stage's layout, which the setup takes as its file is read. */
		FBattleStage Stage;
		std::unique_ptr<FBattleContext> Battle;
		std::unique_ptr<FBattleDefaultTransition> Transition;
		std::unique_ptr<FBattleScreenMesh> SetupMesh;
		FSetupLoop SetupLoop;
		int32 TransitionFramesRun = 0;
		FBattleInputStep InputStep;
		/** Where a frame begun stands: none begun, held by its show, at its input step, or at a step that holds it. */
		enum class EFrameState : uint8
		{
			Idle,
			ShowHold,
			Input,
			InputHeld,
		};
		EFrameState FrameState = EFrameState::Idle;
		/** The frame's catch-up steps still to run after its own input step. */
		int32 CatchUpLeft = 0;
		EPc Pc = EPc::Transition;
		/** The last frame returned to the rules' code: the routine goes on in the next. */
		bool bRulesResume = true;
		bool bSoftReset = false;
		std::optional<FBattleOutcome> BattleOutcome;
	};
}
