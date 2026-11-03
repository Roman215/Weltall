#pragma once

#include "Battle/Show/BattleSpriteFiles.h"
#include "Battle/Show/BattleShowRoutines.h"
#include "Foundation/Types.h"
#include "Graphics/Sprite.h"

#include <array>
#include <memory>
#include <vector>

namespace Weltall
{
	class FBattleEventFiles;
	class FBattleShow;
	struct FBattleEventCue;
	struct FBattlePerformance;
	struct FBattleWaypoint;

	/** The routines of the show's playback, as the original's routines that wait for frames. */
	enum class EPlayRoutine : uint8
	{
		/** The work after a frame's display that the rules' code ran (depth 1): the controller's step, the objects' idle wait. */
		Tail,
		/** The playback's step, a stand then the next record, the next record, the next model record. */
		Step,
		StandAndNext,
		NextRecord,
		NextModelRecord,
		/** Frames while the countered target plays its reaction (animation 1Bh). */
		ReactionPose,
		/** The collapse (its count left in Returned), the wait for the poses, the finish, the next attack readied. */
		Collapse,
		WaitPoses,
		Finish,
		PrepareNext,
		/** The wait for the loads (aborting them is it with the drive stopped first), the effects' images uploaded again. */
		WaitLoads,
		ReloadImages,
		/** Frames until no counted task but the total's panel runs. */
		WaitCounted,
		/** A party slot's attack file read. */
		ReadAttackFile,
		/** The wait for the objects to be idle. */
		IdleWait,
		/** The playback's start, the turn's damage, the poses refreshed and the end pose, which the rules call. */
		Begin,
		TurnDamage,
		RefreshPoses,
		EndPose,
		/** Records F4 and F5's part of the control records: the effect cast from the performer. */
		CastRecord,
		/** An effect's cast and load, the other party's removal, the gear models loaded again. */
		EffectCast,
		EffectLoad,
		RemoveOtherParty,
		ReloadGearModels,
		/** The frame's tail loads and starts the effect the objects asked for. */
		LoadRequested,
		/** A member boarding its gear, which the rules call. */
		BoardGear,
		/**
		 * A battle event's instructions that take frames: a signalled sequence's end (the loads waited for, then the
		 * battle's own bank), an attack (the attacker turned to its target with a party member's attack file read,
		 * then its animation), a special attack (the cast, then the effect) and a voice (played to its end).
		 */
		EventFinishPerform,
		EventAttack,
		EventSpecialAttack,
		EventVoice,
	};

	/**
	 * The playback controller (the battle program's block the playback's start makes): the performer of the record
	 * played and the action's actor, the state its step works by, the route the approach walks, and the flags
	 * the records and the sprites' animations hand each other.
	 */
	struct FBattlePlayController
	{
		/** The performer's slot (+24h); the action's actor (+20h). */
		int32 PerformerSlot = 0;
		int32 Actor = 0;
		/** The step's state (+1Ch): 2 the last leg, 4 an animation, 5 the walk home, 6 a leg of the route, 7 the numbers, 8 an effect, 9 a model record, 10 the end. */
		int32 State = 0;
		/** The first target's slot (+28h). */
		int32 FirstTargetSlot = 0;
		/** The next waypoint of the route (+2Ch), 1 as it starts: waypoint 0 is the actor's own place. */
		int32 RouteIndex = 1;
		/** Model records the objects asked the playback to take (+34h). */
		int32 PendingModelRecords = 0;
		/** The next ready slot, whose attack file the end reads (+40h). */
		int32 NextReady = 0;
		/** The distance to the leg's goal last measured (+44h). */
		int32 LastDistance = 0;
		/** Ready for the next record (+48h); the state an animation's end came in (+49h, 0 none); a model record's sequence ran (+4Ah). */
		bool bReady = true;
		int32 Event = 0;
		bool bModelRecordRan = false;
	};

	/**
	 * The show's playback of the rules' records: the controller, its step and each record's show, the walk out and home, the end and the cancel, the collapse and the poses it waits
	 * for, the action's attack file. Its routines take frames as the original's do: those the frame's tail
	 * runs take nested frames (depth 2), those the rules call hold the rules' code (HoldsRules).
	 */
	class FBattlePlayback
	{
	public:
		/** The animations the playback and the collapse start. */
		static constexpr int32 RunAnimation = 2;
		static constexpr int32 JumpAnimation = 3;
		static constexpr int32 WalkHomeAnimation = 4;
		static constexpr int32 EscapeAnimation = 5;
		static constexpr int32 FlinchAnimation = 10;
		static constexpr int32 RefreshAnimation = 0x10;
		/**
		 * The casts' poses: the plain one (records F4, F5 and a character's effects from its limit) and a character's own; once a
		 * resident effect's sprite runs, the party performer's wait, or an item's.
		 */
		static constexpr int32 CastAnimation = 0x11;
		static constexpr int32 OwnCastAnimation = 0x1C;
		static constexpr int32 EffectWaitAnimation = 0x12;
		static constexpr int32 ItemAnimation = 0x13;
		static constexpr int32 CollapseAnimation = 0x15;
		static constexpr int32 BoardAnimation = 0x16;
		static constexpr int32 VictoryAnimation = 0x18;
		static constexpr int32 ReactionAnimation = 0x1B;
		/** Each pose's animation (1: the sprite's own stance); pose 8 is out of the battle. */
		static constexpr std::array<int32, 9> PoseAnimations{1, 0x15, 0, 7, 1, 0x14, 1, 1, 0x16};
		static constexpr int32 OutPose = 8;

		explicit FBattlePlayback(FBattleShow& InShow);

		/** A new battle: no controller, the flags the battle's memory starts with. */
		void Reset();

		/** The frame's work after its display; false while the show still runs frames of its own. */
		bool FinishFrame();
		/**
		 * The next frame's start, before the rules' code: the rules' call under way goes on from its last frame (the code the
		 * original runs once that frame has returned). True once the call has returned to the rules' code.
		 */
		bool ResumeRules();
		/** 2 while the frames are the show's own (inside a routine the frame's tail ran), 1 otherwise. */
		int32 Depth() const { return Nested.IsEmpty() ? 1 : 2; }
		/** The rules' call under way still takes frames. */
		bool HoldsRules() const { return !Rules.IsEmpty(); }

		/** The playback's start from the rules' code: two frames, then the controller made and the actor started. */
		void Begin(bool bStays, int32 Actor, int32 Target, int32 NextReady);
		/** The actor back at its place in its stance, the controller gone. */
		void Cancel();
		/** From the rules' code at a turn's end: the objects' idle wait, then the next ready slot's attack readied. */
		void PrepareNext(int32 Slot);
		/**
		 * The slot's flinch and a number for each drain not 0 (HP, EP, fuel), then frames
		 * until the numbers have gone and the flinch has ended.
		 */
		void ShowTurnDamage(int32 Slot, const std::array<uint32, 3>& Amounts);
		/**
		 * Every sprite combatant's animation made to follow its state, those away from
		 * home walked back, each facing home, the knocked-out collapsed, the pose its state gives it and its status
		 * marks, with the frames each waits for.
		 */
		void RefreshPoses();
		/**
		 * The camera on the member, then on nobody; the loads let go; the member's boarding
		 * animation, with frames until it has ended and every task made since has gone; the member's sprite removed;
		 * two frames; its gear's model queued, with frames until it is in.
		 */
		void BoardGear(int32 Slot);
		/** From the rules' code (the battle's end): the drive stopped, then the wait for the loads. */
		void FinishLoads();

		/** The show's share of a battle event's load: the event's files taken, its own sound effects registered, the battle's own bank loaded. */
		void BeginEvent(const FBattleEventFiles& Files);
		/**
		 * A battle event's instruction's show (the event module's calls of the presentation): the screen's fade and shake,
		 * the 3D combatants' sequences, the event's objects and the camera they hold, its sounds and the music's
		 * volume, the combatants' sprites' animations, walks, jumps and attacks, an effect, a voice. The instructions
		 * the game waits in hold the rules' code (HoldsRules); the HUD's (the message's window and portrait, the gear
		 * cockpit) are not the playback's. The event's own music, which no script of the disc starts, stops the show.
		 */
		void PlayEventCue(const FBattleEventCue& Cue);
		/** The event's own sound effects' set, which the 3D objects' sounds of the event's kind play from; -1 without an event's. */
		WELTALLCORE_API int32 EventSoundSet() const;
		/** The event's record of the object a sprite file is the data of; -1 for any other file. */
		WELTALLCORE_API int32 EventObjectOf(const FSpriteFile* File) const;
		/**
		 * The end pose (Pose as EBattleEndPose numbers it): the victory (everyone framed, the standing members'
		 * marks ended, their victory animation) or the escape (the camera still, the screen fading out over 40 steps,
		 * the standing members running, 40 frames), then the enemies released, the battle's own sound bank loaded
		 * again and the battle over.
		 */
		void PlayEndPose(int32 Pose);

		/** Sequence op 73: the effect the frame's tail loads and starts, not loaded yet. */
		void RequestEffect(int16 Effect)
		{
			AskedEffect = Effect;
			bAskedEffectLoaded = false;
		}
		/** Whether the effect asked for is in (op 72's wait). */
		bool IsAskedEffectLoaded() const { return bAskedEffectLoaded; }

		/** A sequence ended a model record, which the controller (when there is one) takes next. */
		void CountModelRecord()
		{
			if (bController)
			{
				++Play.PendingModelRecords;
			}
		}

		bool HasController() const { return bController; }
		const FBattlePlayController& Controller() const { return Play; }
		/** The action's file in memory (the party member's attack file) and how many the playback has read. */
		const FBattleSpriteFile* ActionFile() const { return ActionFileData.get(); }
		/**
		 * Whether File is (or is a section of) an action file freed since: the original frees it while sprites may
		 * still point into it (the last actor's alternate file); the port keeps it until none does.
		 */
		bool IsFreed(const FSpriteFile* File) const;
		int32 ActionFileSlot() const { return ActionFileOwner; }
		int32 ActionFileReads() const { return FileReads; }
		bool IsAttackFileInstalled() const { return bAttackFileInstalled; }
		/** Records the next record waits (F7). */
		int32 WaitingRecords() const { return WaitRecords; }
		/**
		 * The slots record F3 set aside to react while knocked out, which the rules' end check counts alive (the rules' word,
		 * which the playback writes), and those that collapsed.
		 */
		WELTALLCORE_API uint16 Stayed() const;
		uint16 Collapsed() const { return CollapsedSlots; }
		/** The listener a sprite's animation calls back: the playback's own (1), the cast pose's (2), another's (3), none (0). */
		int32 ListenerKind(const FSprite& Sprite) const;
		/** An item's cast (record F5) waits for its effect; the party gears an effect's load dropped (1 all, 2 all but the actor's). */
		bool IsItemCast() const { return bItemCast; }
		uint8 DroppedGears() const { return GearDrop; }
		static constexpr uint8 DropsEveryGear = 1;

	private:
		/** Ready, and the state the animation ended in. */
		class FAnimationEnd final : public ISpriteListener
		{
		public:
			explicit FAnimationEnd(FBattlePlayback& InOwner) : Owner(InOwner) {}
			void OnAnimationEvent(FSprite&) override;

		private:
			FBattlePlayback& Owner;
		};

		/** The cast pose's ends counted. */
		class FCastPoseEnd final : public ISpriteListener
		{
		public:
			explicit FCastPoseEnd(FBattlePlayback& InOwner) : Owner(InOwner) {}
			void OnAnimationEvent(FSprite&) override { ++Owner.CastPoseEnds; }

		private:
			FBattlePlayback& Owner;
		};

		/** A motion the event started has ended: the event's instruction waiting on the sprite's motion goes on. */
		class FEventMotionEnd final : public ISpriteListener
		{
		public:
			explicit FEventMotionEnd(FBattlePlayback& InOwner) : Owner(InOwner) {}
			void OnAnimationEvent(FSprite& Sprite) override;

		private:
			FBattlePlayback& Owner;
		};

		/** An event object's sprite data, kept for the battle, and the event's record it was loaded for. */
		struct FEventObjectFile
		{
			std::unique_ptr<FSpriteFile> File;
			int32 Record = 0;
		};

		EShowFlow Dispatch(FShowRoutine& Routine);
		/** Runs the stack's routines to their end (true) or to a frame they wait for (false). */
		bool Run(FShowRoutineStack& Stack);
		/** The rules' word of the slots record F3 set aside. */
		uint16& StayedWord();
		/** Calls a routine from the one running, with its two locals: it goes on at Resume once the callee is done. */
		EShowFlow Call(FShowRoutine& Caller, uint8 Resume, EPlayRoutine Kind, int32 Value = 0, int32 Other = 0);

		EShowFlow RunTail(FShowRoutine& Routine);
		EShowFlow RunStep(FShowRoutine& Routine);
		EShowFlow RunStandAndNext(FShowRoutine& Routine);
		EShowFlow RunNextRecord(FShowRoutine& Routine);
		/** A 3D performer's record: its object runs the record's code as a sequence on the record's targets. */
		EShowFlow RunNextModelRecord(FShowRoutine& Routine);
		EShowFlow RunReactionPose(FShowRoutine& Routine);
		EShowFlow RunCollapse(FShowRoutine& Routine);
		EShowFlow RunWaitPoses(FShowRoutine& Routine);
		EShowFlow RunFinish(FShowRoutine& Routine);
		EShowFlow RunPrepareNext(FShowRoutine& Routine);
		EShowFlow RunWaitLoads(FShowRoutine& Routine);
		EShowFlow RunReloadImages(FShowRoutine& Routine);
		EShowFlow RunWaitCounted(FShowRoutine& Routine);
		EShowFlow RunReadAttackFile(FShowRoutine& Routine);
		EShowFlow RunIdleWait(FShowRoutine& Routine);
		EShowFlow RunBegin(FShowRoutine& Routine);
		EShowFlow RunTurnDamage(FShowRoutine& Routine);
		EShowFlow RunRefreshPoses(FShowRoutine& Routine);
		EShowFlow RunEndPose(FShowRoutine& Routine);
		EShowFlow RunCastRecord(FShowRoutine& Routine);
		EShowFlow RunEffectCast(FShowRoutine& Routine);
		EShowFlow RunEffectLoad(FShowRoutine& Routine);
		EShowFlow RunRemoveOtherParty(FShowRoutine& Routine);
		EShowFlow RunReloadGearModels(FShowRoutine& Routine);
		EShowFlow RunLoadRequested(FShowRoutine& Routine);
		EShowFlow RunBoardGear(FShowRoutine& Routine);
		EShowFlow RunEventFinishPerform(FShowRoutine& Routine);
		EShowFlow RunEventAttack(FShowRoutine& Routine);
		EShowFlow RunEventSpecialAttack(FShowRoutine& Routine);
		EShowFlow RunEventVoice(FShowRoutine& Routine);
		/** A combatant's sprite an event's instruction names; none (a fault) for an empty slot. */
		FSprite* EventSprite(uint16 Slot);
		/** An effect's number in the driver: of the event's own set, or of the battle's common sounds. */
		uint32 EventSound(uint16 Effect, bool bOwn);
		/** The event's object of a record made from an entry of its archive, and ended. */
		void LoadEventObject(uint16 Record, uint16 Entry);
		void FreeEventObject(uint16 Record);
		/** The camera still and held while an object is up, framing again and free once it is gone. */
		void LockEventCamera(bool bLocked);
		/** The slot's sprite plays an animation whose end the event hears of. */
		void SetEventAnimation(uint16 Slot, int32 Animation);		/**
		 * The loaded effect's resource taken, then the actor plays its first animation from the file, or (a
		 * resident effect) a child of the actor plays one of the resource's; its sections put to use, the music lowered. Whether
		 * the actor plays it itself.
		 */
		bool StartEffect();
		/** Effect E3h's load first moves the three gear places' columns back where the effects' sprites keep theirs. */
		void BringBackGearColumns();
		/** A routine the rules' code calls, run to its first frame. */
		void CallFromRules(EPlayRoutine Kind, int32 Value = 0, int32 Other = 0);
		/** The slot's third pass of RefreshPoses once its collapse is done: the pose, then the status marks. */
		void SettlePose(int32 Slot);
		/** 8000h Sleep, 4000h Confusion, 2000h Poison, the element and buff nibbles >> 3. */
		uint16 StatusMarks(int32 Slot) const;
		/** The marks that appeared since the sprite last showed its word spawned, those that went ended. */
		void RefreshMarks(FSprite& Sprite, int32 Slot);

		// The controller's helpers, each one of the original's routines.
		void CreateController();
		void FreeController();
		void SetState(int32 State) { Play.State = State; }
		/** The performer before stands in its stance unless hidden; the slot's sprite performs. */
		FSprite* SetPerformer(int32 Slot);
		/** The record's targets listed, the first facing the sprite and the sprite it. */
		void AimAtTargets(FSprite& Sprite);
		/** The sprites of the mask's slots, each aimed at Aimer; how many. */
		int32 ListTargetSprites(uint16 Mask, std::array<FSprite*, 12>& Listed, FSprite* Aimer);
		void FaceEachOther(FSprite& First, FSprite& Second);
		void Arrive(FSprite& Sprite, FSprite* Target);
		void Approach(FSprite& Sprite, FSprite* Target);
		/** The route's waypoint the walk is at; none (a fault) past the route. */
		const FBattleWaypoint* Waypoint();
		void NextWaypoint(FSprite& Sprite);
		void WalkTo(FSprite& Sprite, int32 Animation);
		/** The show's own codes F3 and F6-F8, FA (F4 and F5 are the routine CastRecord). */
		void ControlRecord(FSprite& Sprite, uint8 Code);
		/** Its sections put to use once; the sound set it registered. */
		int32 InstallAttackFile();
		void UninstallAttackFile();
		/** The action's file becomes one the scripts write their variables into: its sprite and its sections' sprites. */
		void AddWritable(FBattleSpriteFile& File);
		/** The action's file freed (and no longer one the scripts write into): kept while a sprite points into it. */
		void ReleaseActionFile();
		/** The freed files no sprite points into any more let go. */
		void DropUnreferencedFiles();
		/** Facing its default target's side, unless collapsed. */
		void FaceHome(int32 Slot);
		/** The record being played. */
		FBattlePerformance& Record();
		FSprite* SlotSprite(int32 Slot) const;
		bool IsModel(int32 Slot) const;
		bool IsHidden(int32 Slot) const;
		static bool IsResident(const FSprite& Sprite);
		/** Whether the sprite is a party member's drawn with Id's own battle sprite (Fei as Id). */
		bool IsDrawnAsId(const FSprite& Sprite) const;
		/** The heading from Sprite toward Other, or toward its goal (+A0h). */
		int32 HeadingTo(const FSprite& Sprite, const FSprite& Other) const;
		int32 HeadingToGoal(const FSprite& Sprite) const;
		/** The distance across the floor from the sprite's place to its goal. */
		int32 DistanceToGoal(const FSprite& Sprite) const;
		/** The pose a combatant's state gives it (8 out of the battle, 1 knocked out, 0 standing). */
		int32 Pose(int32 Slot) const;
		/** A fault of the show's, which ends every routine under way. */
		void Fail(uint8 Fault, const char* Reason = nullptr);

		FBattleShow& Show;
		FAnimationEnd AnimationEnd;
		FCastPoseEnd CastPoseEnd;
		FEventMotionEnd EventMotionEnd;
		FShowRoutineStack Nested;
		FShowRoutineStack Rules;
		/** What the last routine done left its caller (the collapse's count). */
		int32 Returned = 0;
		bool bController = false;
		FBattlePlayController Play;
		/** The collapse's list of the knocked-out it collapses, by slot (-1 for one set aside). */
		std::array<int32, 12> CollapseSlots{};
		/** The step's performer and first target as it began (its locals), and the sprite the next record plays on. */
		FSprite* StepPerformer = nullptr;
		FSprite* StepTarget = nullptr;
		FSprite* RecordSprite = nullptr;
		/** The target whose reaction the step waits for. */
		int32 ReactionSlot = 0;
		/** Records the next record waits (record F7). */
		int32 WaitRecords = 0;
		/** The drains ShowTurnDamage pops as it begins. */
		std::array<uint32, 3> Drains{};
		/**
		 * The action's file in memory (an attack file or an effect), its party slot, whether its sections are installed.
		 * An effect read waits in LoadingEffect for its load's frames. The effect's start parses the last one loaded,
		 * which is the action's file until that is released (bEffectInMemory).
		 */
		std::unique_ptr<FBattleSpriteFile> ActionFileData;
		std::unique_ptr<FBattleSpriteFile> LoadingEffect;
		bool bEffectInMemory = false;
		std::vector<std::unique_ptr<FBattleSpriteFile>> FreedFiles;
		int32 ActionFileOwner = 0;
		int32 FileReads = 0;
		bool bAttackFileInstalled = false;
		/** The slots that collapsed. */
		uint16 CollapsedSlots = 0;
		/** The cast pose's ends so far, the item cast's flag, the gears an effect dropped. */
		int32 CastPoseEnds = 0;
		bool bItemCast = false;
		uint8 GearDrop = 0;
		/** The effect the objects asked for, 0 none, and whether it is in. */
		int16 AskedEffect = 0;
		bool bAskedEffectLoaded = false;
		/** A battle event's files, whether its own sound effects are registered, its objects by its records and their data. */
		const FBattleEventFiles* EventFiles = nullptr;
		bool bEventSounds = false;
		std::array<FSpriteTask*, 16> EventObjects{};
		std::vector<FEventObjectFile> EventObjectFiles;
		/** The voice playing: its sound effects (registered while it plays) and its effect. */
		std::vector<uint8> VoiceSounds;
		uint32 VoiceSound = 0;
	};
}
