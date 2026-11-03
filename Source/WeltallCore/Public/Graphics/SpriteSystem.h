#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Sprite.h"
#include "Graphics/Vram.h"

#include <memory>
#include <optional>
#include <vector>

namespace Weltall
{
	class FPsxRandom;
	class FSpriteFile;
	class FSpriteSystem;
	struct FGteArctangents;
	struct FGteMatrix;
	struct FSpriteTask;

	/** Why a sprite's script could not go on: something the port does not run, or a read outside what the sprite owns. */
	enum class ESpriteFault : uint8
	{
		None,
		UnsupportedInstruction,
		PcOutsideFile,
		StackOutOfRange,
		NoVariables,
		TooManyParts,
		AnchorsHoldAModel,
		TextureSlotOutsideTable,
		/** Its saved animation is never reached again: the kernel's restore would wait for it for ever. */
		SavedStepsNeverReached,
		/** A script reaches its sprite's task through a sprite whose word for it was cleared: the kernel works on the console's lowest memory. */
		TaskUnlinked,
		/**
		 * A frame taken from a direction list that lies past its file (an animation past the file's table has a
		 * header made of whatever follows it): the kernel takes it from what its memory holds there.
		 */
		ListOutsideFile,
	};

	/**
	 * A sprite as the kernel saves it for a mode to come back to (the field before a battle): where it
	 * is, how far its animation has run, its facing, its transform's and its own scale, its weight and
	 * the preset speed and gravity its owner gave it.
	 */
	struct FSpriteSavedState
	{
		std::array<int32, 3> Position{};
		int16 Facing = 0;
		uint16 ListStep = 0;
		/** The animation and the next one, as the sprite's signed bytes hold them. */
		int16 Animation = 0;
		int16 NextAnimation = 0;
		uint16 StepsRun = 0;
		int32 PresetVerticalSpeed = 0;
		int32 PresetGravity = 0;
		std::array<int16, 3> TransformScale{};
		int16 Scale = 0;
		int16 Weight = 0;
	};

	/** An upload a sprite's frame or script asks for: units of its file, or a clear to black. */
	struct FSpriteUpload
	{
		FVramRect Rect;
		/** The units' file and offset, none for a clear. */
		const FSpriteFile* File = nullptr;
		uint32 Offset = 0;
		/** Made when asked for, before the queued ones, which the kernel makes last asked first. */
		bool bNow = false;
		/** A frame's cell, a texture of its own beside the frame's other cells (FVramImage::bSeparateTexture). */
		bool bCell = false;
	};

	/** A frame's size as the kernel measures a sprite's first frame, at the sprite's scale. */
	struct FSpriteFrameSize
	{
		int32 Right = 0;
		int32 Top = 0;
		int32 Middle = 0;
	};

	/**
	 * A mode's own routine for a sprite task in place of the kernel's: Battle's combatants, their proxies,
	 * its camera controllers and orbiting sprites have their own update and end.
	 */
	class ISpriteTaskRole
	{
	public:
		virtual ~ISpriteTaskRole() = default;

		/** The task's logic tick (its update function). */
		virtual void Update(FSpriteTask& Task) = 0;
		/** The task's end (its destroy function), which takes it out of the lists with FSpriteSystem::RemoveTask. */
		virtual void End(FSpriteTask& Task) = 0;
	};

	/**
	 * A sprite a script spawned, or one a mode runs as a task of its own: the kernel runs it as a task
	 * under the sprite that spawned it, which may end it, until its script stops.
	 */
	struct FSpriteTask
	{
		FSprite Sprite;
		/** The sprite it was spawned under; the kernel matches the two by pointer and by Marks' number. */
		const FSprite* Owner = nullptr;
		uint32 Id = 0;
		/** Bits 0-28 the owner's number when it was spawned, 29 detached with its owner, 30 outlives its owner's end. */
		uint32 Marks = 0;
		/** Its drawing, which ends apart from its update when its own children are ended. */
		bool bDrawn = true;
		/**
		 * Its drawing is filed under the task itself, so it ends with the task's children; a mode that files it
		 * under no one (Battle's combatants) keeps it drawn through that.
		 */
		bool bDrawingIsOwnChild = true;
		/** A mode's routines in place of the kernel's update and end, when set. */
		ISpriteTaskRole* UpdateRole = nullptr;
		ISpriteTaskRole* EndRole = nullptr;
		/** Each update counts itself in FSpriteSystem::CountedUpdates first (kind 7's). */
		bool bCountsUpdates = false;
		/** A mode's own drawing for the task in place of its kind's, by the mode's number; 0 its kind's. */
		uint8 DrawRole = 0;
	};

	/** An instruction's operand bytes: the script's own, or a variable's for WithVariable. */
	struct FSpriteOperands
	{
		std::array<uint8, 3> Bytes{};
		/** Where they are in the script's file, which some instructions address their data from. */
		bool bInScript = true;
		uint32 At = 0;

		uint8 U8(int32 Index) const { return Bytes[static_cast<std::size_t>(Index)]; }
		int32 S8(int32 Index) const { return static_cast<int8>(Bytes[static_cast<std::size_t>(Index)]); }
		int32 S16() const { return static_cast<int16>(Bytes[0] | Bytes[1] << 8); }
		int32 S24() const { return Bytes[0] | Bytes[1] << 8 | static_cast<int8>(Bytes[2]) * 0x10000; }
	};

	/** A point one of BC's battle modes reads. */
	struct FSpriteBattlePoint
	{
		std::array<int16, 3> Point{};
		/** False where the battle cannot give it (the kernel would read what the memory holds). */
		bool bValid = false;
	};

	/**
	 * The battle program's side of the kernel's sprite code while the kernel runs in battle mode (its mode
	 * flag): the interpreter that takes every script step, the floor the sprites land on,
	 * the resources its effect kinds draw from, the camera controllers, and what the kernel's own
	 * instructions read of the battle (the actor, the points of BC's battle modes, the sounds, the
	 * files in memory the scripts write their variables into).
	 */
	class ISpriteBattleMode
	{
	public:
		virtual ~ISpriteBattleMode() = default;

		/** The sprite's script from its next instruction (Battle's interpreter, which the kernel hands every step). */
		virtual void Step(FSprite& Sprite) = 0;
		/** Puts the floor's height under the sprite into its FloorY (for sprites without simple physics). */
		virtual void SnapToFloor(FSprite& Sprite) = 0;
		/** The resource block kind 5 (the common effects) or 6 (the effect's own) draws from. */
		virtual FSpriteResource& KindResource(int32 Kind) = 0;
		/** A camera controller was spawned (kinds 10 to 13): it takes the eye or the target and runs as one. */
		virtual void SetUpController(FSpriteTask& Task) = 0;
		/** The sprite performing the action, which some instructions and SpawnEffect read; none between actions. */
		virtual FSprite* Actor() = 0;
		/** A point of BC's battle modes 2, 4, 5 (the targets' centre), 6, 7 (the camera controllers), 0x17 (the screen's centre) and 0x26 (the sprite's slot). */
		virtual FSpriteBattlePoint Point(uint32 Mode, const FSprite& Sprite) = 0;
		/** The view a screen-space sprite's points go through (the kernel's sprite view matrix). */
		virtual const FGteMatrix& View() = 0;
		/** Plays a sound effect of the battle's own set (bSystem) or of the sprite's. */
		virtual void PlaySound(const FSprite& Sprite, uint8 Effect, bool bSystem) = 0;
		/** The file in memory a script writes its variables into, which every sprite of the file reads. */
		virtual FSpriteFile* WritableFile(const FSpriteFile& File) = 0;
		/** A model sprite's packets took its added colour (FSprite::ModelPackets' tint): those a frame has linked already show it. */
		virtual void ModelTinted(const FSprite& Sprite) = 0;
	};

	/**
	 * A task of a mode's own in the kernel's update list, beside the sprites the system spawns: the
	 * kernel runs them all in one list, newest first, so their order decides whose random numbers come
	 * first. Its owner links it and unlinks it.
	 */
	class IKernelTask
	{
	public:
		virtual ~IKernelTask() = default;

		/** One logic tick; it may unlink tasks, itself included. */
		virtual void Update() = 0;
		/** Its owner's children were ended and it is out of the list: the kernel then runs its destroy function. */
		virtual void OnKilled() {}
	};

	/** An entry of the kernel's update list: a sprite the system spawned, or a mode's own task. */
	struct FKernelTaskEntry
	{
		std::unique_ptr<FSpriteTask> Sprite;
		IKernelTask* Task = nullptr;
		/** Linked while the kernel counted the tasks made (a battle's playback waits for them). */
		bool bCounted = false;
		/** A mode task's owner, whose children's end ends it too, and the owner's number when it was linked. */
		const FSprite* Owner = nullptr;
		uint32 OwnerNumber = 0;
	};

	/**
	 * The kernel's sprite system: the state its sprites share (the ticks per logic tick, the hit-stop,
	 * the frames waiting to be built, the uploads waiting for VRAM, the spawned children) and every
	 * operation on a sprite, each as the kernel's routine of the same purpose does it.
	 */
	class FSpriteSystem
	{
	public:
		/** Where battle mode streams the frames of a file it takes that is not resident. */
		static constexpr int16 BattleStreamX = 0x300;
		static constexpr int16 BattleStreamY = 0x100;

		/** Display frames per logic tick less one: 1 in the field and on the world map, 0 in battle. */
		int32 TickExtraFrames = 1;
		/** Logic ticks the spawned children stand still for. */
		int32 HitStop = 0;
		/**
		 * What the loop ticking the sprites holds in the register that script steps 40-7F wait by: the
		 * kernel never sets it for them. Unknown where no such loop is known, and such a step faults.
		 */
		std::optional<int32> StrayWait;
		/** The game's rand(). */
		FPsxRandom* Random = nullptr;
		/** libgte's arctangents, for turning toward a target. */
		const FGteArctangents* Arctangents = nullptr;
		/**
		 * The place a script reads through a sprite pointer nothing has set (a target, a parent or Battle's actor it
		 * does not have): the three words at the bottom of the console's memory, which the BIOS leaves holding the
		 * tail of its exception vector's copy. As a place that is far below any floor.
		 */
		std::array<int32, 3> UnsetSpritePlace{0x00000003, 0x275A0C80, 0x03400008};
		/** Where the VRAM ring puts the next streamed child's frame: a column and one of three rows. The kernel keeps it from map to map. */
		struct FRingPlace
		{
			int32 X = 0;
			int32 Row = 0;
		};
		FRingPlace RingPlace() const { return {RingCursorX, RingRow}; }
		void SetRingPlace(const FRingPlace& Place)
		{
			RingCursorX = Place.X;
			RingRow = Place.Row;
		}

		/**
		 * The battle program, while the kernel runs in battle mode: its interpreter
		 * runs the scripts and the kernel's own routines take its paths. None in the field.
		 */
		ISpriteBattleMode* BattleMode = nullptr;
		/** The scale a new animation gives its sprite in battle mode. */
		int16 AnimationScale = 0x2000;
		/** The animation extension module the last resource taken in battle mode asks for, 0 none. */
		int32 RequestedExtension = 0;
		/** The updates of counting tasks (kind 7) so far. */
		int32 CountedUpdates = 0;

		/**
		 * A streamed frame's part mirrored left to right is drawn where its frame was made to put it: moved left by the
		 * empty columns that pad its cell's art to the width VRAM takes. The tool that made the frames placed a mirrored
		 * cell by its art's width; the kernel mirrors the whole cell, which leaves the art that many texels to the right
		 * (Fei's arm off his shoulder as he runs away from the view, a lock of Billy's hair off his head). Off: as the
		 * original draws.
		 */
		bool bMirroredPartsPlacedByArt = false;

		/** The texture page scripts give the models they attach (the model builder's override). */
		bool bModelTextureOverride = false;
		int16 ModelTextureX = 0;
		int16 ModelTextureY = 0;

		/**
		 * Makes a sprite for a file, as the kernel creates one: its textures and palettes at the given
		 * places in VRAM, palette set PaletteSet, animation 0 started and facing 0.
		 */
		WELTALLCORE_API void Create(FSprite& Sprite, const FSpriteFile& File, int32 ClutX, int32 ClutY, int32 TextureX, int32 TextureY, int32 PaletteSet = 0);
		/** Gives the sprite Count part records, as the modes do for sprites whose files change. */
		WELTALLCORE_API void SetPartCapacity(FSprite& Sprite, int32 Count);
		/** Part coordinates count in texels << Shift. */
		WELTALLCORE_API void SetSizeShift(FSprite& Sprite, int32 Shift);
		/** The size of the first frame of animation 0's first facing, as the modes measure a new sprite. */
		WELTALLCORE_API FSpriteFrameSize FirstFrameSize(const FSprite& Sprite) const;
		/** Takes a sprite out of the queue of frames to build, as the kernel frees one. */
		WELTALLCORE_API void Free(FSprite& Sprite);
		/**
		 * Gives a sprite Count texture slots in place of any it had, the first its own texture place;
		 * its owner sets the others.
		 */
		WELTALLCORE_API void AllocTextureSlots(FSprite& Sprite, int32 Count);

		/**
		 * Plays animation Animation of the primary file, or ~Animation of the alternate one for a negative number, as a start
		 * from outside the sprite's script (counted in FSprite::Starts).
		 */
		WELTALLCORE_API void SetAnimation(FSprite& Sprite, int32 Animation);
		/** Faces the sprite Angle (4096 a turn) relative to the view: picks its frame lists. */
		WELTALLCORE_API void SetFacing(FSprite& Sprite, int32 Angle);
		/** Sets the direction it moves in, and its velocity along it. */
		WELTALLCORE_API void SetDirection(FSprite& Sprite, int32 Direction);
		WELTALLCORE_API void SetScale(FSprite& Sprite, int32 Scale);
		/** Modulates every part by the colour (128 unchanged). */
		WELTALLCORE_API void SetTint(FSprite& Sprite, int32 Red, int32 Green, int32 Blue);
		/** 0 opaque, 1-4 semi-transparent with GPU blend mode Mode - 1. */
		WELTALLCORE_API void SetBlend(FSprite& Sprite, int32 Mode);

		/** Runs the animation for one logic tick. */
		WELTALLCORE_API void TickAnimation(FSprite& Sprite);
		/** What the kernel keeps of the sprite to put it back later. */
		WELTALLCORE_API static FSpriteSavedState SaveState(const FSprite& Sprite);
		/**
		 * Puts a saved sprite back: its facing, animations, scales and weight, the animation started
		 * again and run (the sprite moving raw by its velocity and gravity) until it has run as many steps
		 * as when it was saved, then its position and preset speed and gravity. A fault when it stops or
		 * starts again short of them, where the kernel would wait for ever.
		 */
		WELTALLCORE_API void RestoreState(FSprite& Sprite, const FSpriteSavedState& Saved);
		/** Moves by the velocity, then falls by the gravity. */
		WELTALLCORE_API void Move(FSprite& Sprite);
		/** One logic tick of every task, newest first, unless a hit-stop holds them. */
		WELTALLCORE_API void RunTasks();
		/** Ends every spawned child and forgets the modes' tasks, as a mode does when it ends. */
		WELTALLCORE_API void EndTasks();
		const std::vector<FKernelTaskEntry>& Tasks() const { return TaskList; }

		/**
		 * Links a mode's own task at the head of the update list; the next run reaches
		 * it. With an owner, it is one of the owner's children, which the owner's end of its children ends.
		 */
		WELTALLCORE_API void LinkTask(IKernelTask& Task, const FSprite* Owner = nullptr);
		/** Unlinks a mode's own task: a run under way skips it if it was still to come. */
		WELTALLCORE_API void UnlinkTask(IKernelTask& Task);
		/** Whether the task is linked. */
		WELTALLCORE_API bool IsLinked(const IKernelTask& Task) const;
		/** Takes a linked task out of the count of the tasks made (a task that clears its own counted bit). */
		WELTALLCORE_API void Uncount(const IKernelTask& Task);

		/** While set, every task linked is counted until it is unlinked. */
		bool bCountingTasks = false;
		int32 CountedTasks = 0;

		/**
		 * A task with a sprite of a mode's own, owned by nobody, at the head of the list with its drawing
		 * as well: the mode makes its sprite and may give it roles.
		 */
		WELTALLCORE_API FSpriteTask& CreateTask();
		/** The kernel's update of a sprite task: the animation and the motion, twice if it asks, and its end when its script stopped. */
		WELTALLCORE_API void UpdateSpriteTask(FSpriteTask& Task);
		/** Ends a task: its end role's way, or the kernel's. */
		WELTALLCORE_API void EndTask(FSpriteTask& Task);
		/** The kernel's end of a sprite task: its children as its flags say, then the task. */
		WELTALLCORE_API void DestroyTask(FSpriteTask& Task);
		/** Takes the task out of the lists and frees it; an end role's last step. */
		WELTALLCORE_API void RemoveTask(FSpriteTask& Task);
		/**
		 * Takes the task out of the lists but keeps its memory as it was, as the kernel's heap keeps a freed block until
		 * the block is taken again: the game's pointers to the sprite still read it, unchanged. Kept until EndTasks.
		 */
		WELTALLCORE_API void RetireTask(FSpriteTask& Task);
		/** Ends the tasks spawned or linked under the sprite, and its own drawing. */
		WELTALLCORE_API void KillChildren(const FSprite& Owner);
		/** Tells the sprite's detachable children it is gone: their parent is none. */
		WELTALLCORE_API void DetachChildren(const FSprite& Owner);
		/**
		 * The detachable children spawned under Owner that draw from Resource with Animation as their animation's
		 * number (any, for none), in the list's order: the first (none: nullptr), or every one ended.
		 */
		WELTALLCORE_API FSprite* FindChild(const FSprite& Owner, const FSpriteResource& Resource, int32 Animation);
		WELTALLCORE_API void EndChildren(const FSprite& Owner, const FSpriteResource& Resource, std::optional<int32> Animation);

		// What a mode's interpreter shares with the kernel's own.

		/** Shows frame Frame: queues it to be built. */
		WELTALLCORE_API void SetFrame(FSprite& Sprite, int32 Frame);
		/** Shows the frame of the facing's list at its step. */
		WELTALLCORE_API void ApplyDirectionFrame(FSprite& Sprite);
		/** Starts the animation whose header is at Header in File. */
		WELTALLCORE_API void Start(FSprite& Sprite, const FSpriteFile& File, uint32 Header);
		/** Takes the file as the sprite's resource. */
		WELTALLCORE_API void SetResource(FSprite& Sprite, const FSpriteFile* File);
		/** Fills a resource block from a file, its cells and palettes at the places given. */
		WELTALLCORE_API void ParseResource(FSpriteResource& Resource, const FSpriteFile& File, int16 TextureX, int16 TextureY, int16 ClutX, int16 ClutY);
		/** The velocity along the heading at the speed. */
		WELTALLCORE_API void UpdateVelocity(FSprite& Sprite);
		/** The fall and, without simple physics, the landing. */
		WELTALLCORE_API void ApplyGravity(FSprite& Sprite);
		/** The colour and blend into the built parts. */
		WELTALLCORE_API void ApplyColorBlend(FSprite& Sprite);
		/** Spawns a child running the animation at Header of File, drawing from Resource. */
		WELTALLCORE_API FSprite* SpawnChild(FSprite& Parent, const FSpriteFile& File, uint32 Header, FSpriteResource* Resource);
		/**
		 * Spawns a sprite of no owner running animation Animation of the resource at Point; in battle mode it
		 * takes the actor's slot, target, motion, look, time scale, extra block and sounds, but none of its files. None after a
		 * fault.
		 */
		WELTALLCORE_API FSpriteTask* SpawnEffect(FSpriteResource& Resource, int32 Animation, const std::array<int16, 3>& Point);
		/** Runs one of the kernel's own instructions. */
		WELTALLCORE_API void ExecuteKernelInstruction(FSprite& Sprite, uint8 Opcode, const FSpriteOperands& Args);
		/** A variable's three bytes, as WithVariable hands them to an instruction; none after a fault. */
		WELTALLCORE_API std::optional<FSpriteOperands> VariableOperands(FSprite& Sprite, uint8 Operand);
		WELTALLCORE_API bool PushByte(FSprite& Sprite, uint8 Value);
		WELTALLCORE_API bool PopByte(FSprite& Sprite, uint8& Value);
		/** A script address: its three low bytes, the lowest on top. */
		WELTALLCORE_API bool PushAddress(FSprite& Sprite, uint32 Address);
		WELTALLCORE_API bool PopAddress(FSprite& Sprite, uint32& Address);
		/** Records a fault: the first one stands. */
		WELTALLCORE_API void Report(ESpriteFault Fault, uint32 Value);
		/** Whether an instruction may reach the sprite's task through the sprite; one whose word for it was cleared reports the fault. */
		WELTALLCORE_API bool ReachesTask(const FSprite& Sprite, uint32 Opcode);
		/** The game's next rand(), 0 without one. */
		WELTALLCORE_API int32 NextRandom();

		/** Builds the frames the sprites were given since the last build, newest first. */
		WELTALLCORE_API void BuildPendingFrames();
		/** The uploads asked for since the last flush, in the order they were asked for. */
		const std::vector<FSpriteUpload>& Uploads() const { return UploadList; }
		/** Makes the uploads the frames asked for, in the order the kernel makes them; one at a place in the port's rows goes there. */
		WELTALLCORE_API void FlushUploads(FVram& Vram);
		/** The same into Into, which puts them where it keeps them (a sprite system of the port's own: VRAM's port rows). */
		WELTALLCORE_API void FlushUploads(IVramUploads& Into);

		/** The number a spawned child records for the sprite it was spawned under, as the kernel reads it. */
		WELTALLCORE_API static uint32 OwnerNumber(const FSprite& Owner);

		/** The first fault since the last ClearFault, with what it concerns: an opcode or an offset. */
		ESpriteFault Fault() const { return FirstFault; }
		uint32 FaultValue() const { return FirstFaultValue; }
		void ClearFault() { FirstFault = ESpriteFault::None; }
		WELTALLCORE_API std::optional<FError> FaultError() const;

	private:
		friend class FSpriteOperations;
		friend class FPlacementWalker;

		void Step(FSprite& Sprite);
		/** SetAnimation's work, which the script's own loops and chains and a restore do without counting a start. */
		void PlayAnimation(FSprite& Sprite, int32 Animation);
		void ReadAnchors(FSprite& Sprite, int32 Frame);
		void CopyParentAnchors(FSprite& Sprite);
		/** An upload made at once, not queued with the frames'. */
		void UploadNow(const FVramRect& Rect, const FSpriteFile& File, uint32 Offset);
		void Unqueue(FSprite& Sprite);
		/**
		 * A byte of the sprite as the kernel's stack pointer reaches it, At counted from the stack's first
		 * byte: the stack, and past its ends the words beside it (before: the pointer and the parent's
		 * animation; after: the wait and the goal). A fault further away.
		 */
		bool ReadStackByte(FSprite& Sprite, int32 At, uint8& Value);
		bool WriteStackByte(FSprite& Sprite, int32 At, uint8 Value);
		void Seek(FSprite& Sprite, uint32 Pc, uint32 Steps);
		void BuildFrame(FSprite& Sprite, int32 Frame);
		void BuildResidentFrame(FSprite& Sprite, int32 Frame);
		void QueueUpload(const FVramRect& Rect, const FSpriteFile* File, uint32 Offset, bool bCell = false);
		void UpdateTask(FSpriteTask& Task);
		/** Links a new sprite task's update and drawing under Owner. */
		FSpriteTask& NewTask(const FSprite* Owner);
		/** The kernel's set-up of a spawned task by its kind. */
		void SetUpKind(FSpriteTask& Task);
		bool IsDrawingChild(const FKernelTaskEntry& Entry, const FSprite& Owner, const FSpriteResource& Resource, std::optional<int32> Animation) const;
		/** The texture place battle mode gives a sprite's resource as it takes a file: the block's, or the streaming place. */
		void ApplyBattleTexture(FSprite& Sprite, bool bFromBlock);

		std::vector<FSprite*> Pending;
		std::vector<FSpriteUpload> UploadList;
		FVramImage UploadScratch;
		void Insert(FKernelTaskEntry Entry);
		void Erase(std::size_t Index);

		std::vector<FKernelTaskEntry> TaskList;
		/** Ended tasks kept for the next ones, so that spawning allocates only past the most alive at once. */
		std::vector<std::unique_ptr<FSpriteTask>> FreeTasks;
		/** Tasks taken out of the lists whose sprites the game still reads (RetireTask). */
		std::vector<std::unique_ptr<FSpriteTask>> RetiredTasks;
		/** The VRAM ring's place for the next streamed child's frame: a column and one of three rows. */
		int32 RingCursorX = 0;
		int32 RingRow = 0;
		/** Where the task run is: the task after the running one, which ending a task may move. */
		std::size_t RunCursor = 0;
		bool bRunning = false;
		uint32 NextTaskId = 0;
		ESpriteFault FirstFault = ESpriteFault::None;
		uint32 FirstFaultValue = 0;
	};
}
