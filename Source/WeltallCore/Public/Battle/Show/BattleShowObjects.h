#pragma once

#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleObjectParts.h"
#include "Battle/Show/BattleObjectsFrame.h"
#include "Battle/Show/BattleShowOptions.h"
#include "Field/FieldSceneFrame.h"
#include "Foundation/Types.h"
#include "Gears/GearChannels.h"
#include "Gears/GearCloth.h"
#include "Gears/GearEngine.h"
#include "Gears/GearFiles.h"
#include "Gears/GearPackets.h"
#include "Graphics/Gte.h"
#include "Graphics/Vram.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FBattleFloor;
	class FBattleStageRecord;
	class FSpriteSystem;
	class IKernelTask;
	struct FBattleCamera;
	struct FBattleContext;
	struct FBattleStageFiles;

	/** What an object's flags ask as it is made (the original's bits). */
	namespace BattleObjectCreateFlags
	{
		/** The model is the last one made: no textures uploaded, no part list of its own, no texture place. */
		inline constexpr uint32 SharesModel = 0x01;
		/** The model is kept whole for the next object of it. */
		inline constexpr uint32 KeepsModel = 0x02;
		/** The sequence archive is already in place (an earlier object of the same archive). */
		inline constexpr uint32 SharesSequences = 0x04;
		/** Built still: no sequence started, the model's own scale, its parts unlit (the stage). */
		inline constexpr uint32 Still = 0x40;
		/** The sequence archive is not the object's to free. */
		inline constexpr uint32 BorrowsSequences = 0x80;
	}

	/** The header flags an object keeps (a sequence can set them all, op 5F). */
	namespace BattleObjectFlags
	{
		inline constexpr uint16 NoShadow = 0x0001;
		/** Key event 8 and op 3E's class 8 ask whether an object reacts so. */
		inline constexpr uint16 Reacts = 0x0002;
		inline constexpr uint16 AbsoluteTextures = 0x0004;
		inline constexpr uint16 RadiusFromX = 0x0008;
		/** An attached object keeps its own visibility. */
		inline constexpr uint16 OwnVisibility = 0x0010;
		/** The performer that has it is not pushed out of the others. */
		inline constexpr uint16 Immovable = 0x0020;
		/** Its parts are drawn without the kernel's culling of a part by its bounding box. */
		inline constexpr uint16 NoBoxCulling = 0x0040;
		/** The stances FFh picks: 80h the weak one, 100h the broken one, 400h a third. */
		inline constexpr uint16 WeakStance = 0x0080;
		inline constexpr uint16 BrokenStance = 0x0100;
		inline constexpr uint16 EnvironmentMapped = 0x0200;
		inline constexpr uint16 OtherStance = 0x0400;
	}

	/**
	 * One of Battle's 3D objects (0x11C bytes in the original): a skeleton the engine animates, its
	 * sequence and the handlers a sequence set, the queue of sequences started while one runs, its
	 * motion, the point it aims at and whose it is, the object it rides, its key events' cursor, and the
	 * trails, cloths and CLUT animations its header gives it. Places in code are places in the packs the
	 * objects hold.
	 */
	struct FBattleObject : FGearBody
	{
		static constexpr uint8 NoHost = 0xFF;

		/** The object's own sequence archive (its sequences below 50h and its animations below 40h). */
		FGearMotionPack* Motion = nullptr;
		/** The object frees its archive with itself. */
		bool bOwnsMotion = false;
		/** Its archive's sound container is known (every object but the stage's). */
		bool bHasSoundTable = false;
		/** The sequence extension op 04 read (sequences from 50h), and whether op 05 installed it (animations from 40h). */
		FBattleSequenceExtension* Extension = nullptr;
		bool bExtensionInstalled = false;
		/** Op 05 installed it and waits a step for the reads to end. */
		bool bExtensionPending = false;
		FGearCodeRef Pc;
		FGearCodeRef OnNear;
		FGearCodeRef OnTimer;
		FGearCodeRef OnLanding;
		/** The floor triangle it last stood over, -1 none. */
		int16 FloorTriangle = -1;
		uint8 Slot = 0;
		/** A clone's owner slot. */
		uint8 Owner = 0;
		/** Made from a gear's or a special model's files (op 34 tests it, op 3A asks it). */
		bool bFromFiles = false;
		int16 Height = 0;
		int16 RadiusX = 0;
		int16 RadiusZ = 0;
		/** The animation FEh plays, FFh sets it from the combatant's state (bit 7: mirrored). */
		uint8 Stance = 0;
		/** The sequence queue: 0 off, 1 holding nothing, 2-5 holding one start fewer. */
		uint8 Queue = 0;
		std::array<uint8, 4> QueuedOwners{};
		std::array<uint8, 4> QueuedSequences{};
		uint8 bVisible = 1;
		/** Its sequence signals the battle event at its end (ops 02, 03). */
		uint8 Signal = 0;
		/** 0: the root's height follows the floor. */
		uint8 NoFloorSnap = 0;
		uint8 ScaledBones = 0;
		/** Busy (op 6D), which op 6E, the playback and the idle wait watch. */
		uint8 Playing = 0;
		/** The sound volume the shadow's depth gives it. */
		uint8 Loudness = 0x6B;
		/** Op 6F's mark: the play mask, or -1. */
		int16 Mark = -1;
		int16 WindY = 0;
		uint16 WaitTicks = 0;
		uint16 Loops = 0;
		uint16 Timer = 0;
		uint16 TimerEnd = 0;
		int16 NearDistance = 0;
		uint16 Flags = 0;
		/** Whose point the target follows (0 none; FFh the first target, FEh the performer, FDh itself, FCh its owner, FAh the stage, else a slot + 1), the bone and the offset. */
		int16 Who = 0;
		int16 WhoBone = 0;
		std::array<int16, 3> WhoOffset{};
		/** The object it rides (FFh none), whether the ride turns it too, the bone and the offset. */
		uint8 Host = NoHost;
		uint8 HostTurns = 0;
		uint16 HostBone = 0;
		std::array<int16, 3> AttachOffset{};
		int16 FloorY = 0;
		bool bSoundEffectsRegistered = false;
		bool bExtensionSoundEffectsRegistered = false;
		/**
		 * The set id of the sound effects of the last extension installed, which its sounds are played by;
		 * the original reads it from the extension's block, freed or not, which nothing reuses before.
		 */
		std::optional<uint16> ExtensionSoundSet;
		std::array<int16, 3> Spin{};
		std::array<int16, 3> SpinAcceleration{};
		std::array<int16, 3> Velocity{};
		std::array<int16, 3> Acceleration{};
		/** The approach distance the waits and handlers measure against, 1 from the start. */
		int16 Speed = 1;
		/** Where its textures and CLUTs went; TextureX -1 none. */
		int16 TextureX = -1;
		int16 TextureY = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
		/** The key events: the frame (-1 off), the loop's length (-1 none), the events done and in all, the next and the first. */
		int16 KeyFrame = -1;
		int16 KeyLoop = 0;
		int16 KeyIndex = 0;
		int16 KeyCount = 0;
		FGearCodeRef KeyEvents;
		FGearCodeRef KeyEventsLoop;
		/** The play's targets when its sequence started. */
		uint16 Targets = 0;
		/** Its trails' emitters, cloths and CLUT animations, as its header gives their counts. */
		std::vector<FBattleTrailEmitter> Trails;
		std::vector<FGearCloth> Cloths;
		std::vector<FBattleClutAnimation> ClutAnimations;
		/** The model its bones' parts are in. */
		std::shared_ptr<const FGearModelPack> Model;
		/** Each bone's part as its making built it, in bone order; a clone draws its original's. */
		std::shared_ptr<const std::vector<FGearBonePackets>> Packets;
		/**
		 * The texture regions its model's polygons read where its making put the textures (Weltall::GearTextureRegions),
		 * each with the colours its CLUT held once the making had uploaded them.
		 */
		std::shared_ptr<const std::vector<FTexturePictureSeed>> TextureSeeds;
		/** The model drawn over its bones in place of its own, and that model's bones as last placed over them; none drawn for most. */
		std::shared_ptr<const FBattleGearDress> Dress;
		std::vector<FGearBone> DressBones;
	};

	/** Key event 1's spawn of an effect sprite at a bone: what the sprite follows when it follows the bone. */
	struct FBattleBoneEffect
	{
		/** From the effect's own resource rather than the common effects'. */
		bool bEffectResource = false;
		int32 Animation = 0;
		FGteVector Point{};
		int16 Direction = 0;
		int16 Scale = 0;
		/**
		 * The event's place, the slot the object lies in (which a swap of objects moves with it), the bone the sprite follows
		 * (when it follows) and its offset, and whether it stays on the floor.
		 */
		FGearCodeRef Event;
		uint8 Slot = 0;
		uint8 Bone = 0;
		bool bFollows = false;
		FGteVector FollowOffset{};
		bool bOnFloor = false;
	};

	/**
	 * What the objects reach outside the object system: the playback, the event, the sprites, the
	 * sounds, VRAM and the disc. Each call is where the original makes it.
	 */
	class IBattleObjectServices
	{
	public:
		virtual ~IBattleObjectServices() = default;

		/** The record the playback shows, counted from 1: the result classes are the previous one's. */
		virtual int32 PlayRecord() = 0;
		/** The damage numbers of the action shown, and whether any the playback waits for remain. */
		virtual void ShowNumbers() = 0;
		virtual bool NumbersPending() = 0;
		/** Ops 02 and 03: the kernel stops counting the tasks made. */
		virtual void StopCountingTasks() = 0;
		/** The battle event's performer in Slot has ended its sequence. */
		virtual void SignalEventPerform(int32 Slot) = 0;
		/** A sequence ended a model record. */
		virtual void CountModelRecord() = 0;
		/**
		 * A signalled sequence begins: the kernel counts the tasks made from
		 * none and the event's performer faces its target.
		 */
		virtual void BeginEventPerformance(int32 Slot, int32 Target) = 0;
		/** Key event 1: an effect sprite at a bone. */
		virtual void SpawnEffect(const FBattleBoneEffect& Effect) = 0;
		/** Two objects swapped slots: what follows an object's bone follows it to its new slot. */
		virtual void ObjectsSwapped(int32 First, int32 Second) = 0;
		/** Op 73: the effect a cast asks for; whether the one asked is in. */
		virtual void RequestEffect(int16 Effect) = 0;
		virtual bool EffectLoaded() = 0;
		/** Op 74: the special model's steps counted; true when it is the one that frees the model. */
		virtual bool CountSpecialModel() = 0;
		/** Op 05: an extension's sprite resource parsed into the effect's. */
		virtual void TakeEffectResource(const FBattleSequenceExtension& Extension) = 0;
		/** Sound effect files (`seds`, their id at +14h): whether one of the id is registered, registering and unregistering one. */
		virtual bool SoundEffectsRegistered(uint16 FileId) = 0;
		virtual void RegisterSoundEffects(std::span<const uint8> Seds) = 0;
		virtual void UnregisterSoundEffects(std::span<const uint8> Seds) = 0;
		/** The battle's own sound effects and the battle event's (op 3C and key event 5's bases 0 and 3), their id in the high half. */
		virtual uint32 SystemSoundBase() = 0;
		virtual uint32 EventSoundBase() = 0;
		/** A sound effect played, its volume set, its volume faded. */
		virtual void PlaySound(uint32 Sound) = 0;
		virtual void SetSoundVolume(uint32 Sound, int32 Volume) = 0;
		virtual void FadeSound(uint32 Sound, int32 Volume, int32 Steps) = 0;
		/** VRAM: a block uploaded at its place, a copy (MoveImage), a rectangle read and written (StoreImage, LoadImage). */
		virtual void UploadImage(const FGearTextureBlock& Block, int16 X, int16 Y) = 0;
		/** A block of the port's own at its place in the port's rows (Y counted from the console's top): nothing of the game's. */
		virtual void UploadToPortRows(const FGearTextureBlock& Block, int16 X, int16 Y) = 0;
		virtual void MoveImage(const FVramRect& Rect, int32 X, int32 Y) = 0;
		virtual void StoreImage(const FVramRect& Rect, std::span<uint16> Into) = 0;
		virtual void LoadImage(const FVramRect& Rect, std::span<const uint16> From) = 0;
		/** The files the objects read (whole sectors, as the game reads them): a gear's, a special model's, directory 42's File. */
		virtual TResult<FBattleModelFiles> ReadGear(int32 Gear, int32 Variant) = 0;
		virtual TResult<FBattleModelFiles> ReadSpecialModel(int32 Model) = 0;
		virtual TResult<FBattleSequenceExtension> ReadExtension(int32 File) = 0;
		/** A battle scene's model entry (the port's: a model worn by a look, as Id's red gears are). */
		virtual TResult<FBattleModelFiles> ReadSceneModel(int32 Scene, int32 Entry) = 0;
		/** Op 07: the reads under way are abandoned. */
		virtual void AbortReads() = 0;
		/** The sprite that stands in for slot Slot's object in the sprites' code. */
		virtual void CreateProxy(int32 Slot) = 0;
	};

	/**
	 * Battle's 3D objects: 32 slots of objects on the gears'
	 * engine, the track pool the stage record sizes, the trails' pool, the model camera's tracks, the
	 * point lights and the words the sequences share, stepped every other frame and drawn every frame
	 * as the objects' drawing runs them.
	 */
	class FBattleObjects final : public FGearEngine
	{
	public:
		static constexpr int32 SlotCount = 32;
		static constexpr int32 CombatantSlots = 11;
		static constexpr int32 SpecialSlot = 11;
		static constexpr int32 FirstSubObject = 13;
		static constexpr int32 FirstClone = 19;
		static constexpr int32 StageSlot = 31;
		static constexpr int32 PartyTextureRows = 0x1C0;
		static constexpr int32 GearPlaces = 3;

		WELTALLCORE_API FBattleObjects();
		WELTALLCORE_API ~FBattleObjects() override;

		/** What it works with, which its owner sets before Init. */
		FBattleContext* Context = nullptr;
		FBattleCamera* Camera = nullptr;
		FBattleFloor* Floor = nullptr;
		const FBattleObjectTables* Tables = nullptr;
		/** The stage's record, which Init takes. */
		const FBattleStageRecord* Stage = nullptr;
		/** The GTE's state the owner keeps: its offset and projection place the trails and the shadows. */
		FGte* Gte = nullptr;
		IBattleObjectServices* Services = nullptr;
		/** The show's fixes in use, which the owner keeps; none: every fix on. */
		const FBattleShowOptions* ShowOptions = nullptr;
		/** The kernel's task list, which the gears' model loads run in. */
		FSpriteSystem* Tasks = nullptr;
		/** The battle's kind (the setup's argument), which op 33 tests. */
		uint8 BattleKind = 0;
		/**
		 * Which polygons and cloth triangles the drawing keeps: the original's rules, made for its 320-pixel screen, or
		 * everything a view of any width shows (only the polygons facing away are left out; the display clips the rest).
		 */
		EFieldCulling Culling = EFieldCulling::InRange;

		// The show's calls, each where the original makes it.
		/** Every slot empty, the pools of tracks and trails the stage's record sizes, the model camera's tracks cleared. */
		WELTALLCORE_API void Init(const FBattleStageRecord& Record);
		/**
		 * The stage's object: slot 31 made from the stage's model, its bones placed from the model's fifth
		 * section, the stage's sequences taken and the first started. Its texture regions are its model's and its
		 * backdrop's (BattleBackdropTextureSeeds).
		 */
		WELTALLCORE_API void CreateStage(const FBattleStageFiles& StageFiles);
		/**
		 * A scene's model enemy made: slot Slot, its flags, its
		 * motion pack and model pack, its textures at (TextureX, TextureY) and its CLUT row.
		 */
		WELTALLCORE_API void CreateModel(int32 Slot, uint32 Flags, std::span<const uint8> Motion, std::span<const uint8> Model, int32 TextureX,
			int32 TextureY, int32 ClutRow);
		/** The objects' shadows are drawn from now on: their texture, the common effects' images, is in VRAM. */
		void EnableShadows() { bShadows = true; }
		/** A party member's gear model read and made by a kernel task. */
		WELTALLCORE_API void QueueGearModel(int32 Slot);
		/** Gear models still being read and made: the setup's loads wait for none. */
		bool GearModelsPending() const { return PendingGearModels != 0; }
		/** The step a gear model's load task has reached (0 waiting to read, 1 read and waiting to be made); none for another task. */
		WELTALLCORE_API std::optional<int32> GearLoadStep(const IKernelTask& Task) const;
		/** Where slot Slot's object stands (its skeleton's translation); false when the slot has none. */
		WELTALLCORE_API bool Root(int32 Slot, std::array<int32, 3>& Translation);
		/** The stage object's CLUT animations step (as the stage's drawing steps them), by 1 + the catch-up. */
		WELTALLCORE_API void StepStageClutAnimations(int32 CatchUp);
		/**
		 * The tint's start: the stage's running CLUT animations feed the tint's palettes from now on in place
		 * of VRAM, and the tint's palettes start, the rectangle Palettes read from VRAM (nothing when they run already).
		 */
		WELTALLCORE_API void BeginStageTint(const FVramRect& Palettes);
		/**
		 * The tint's end: when the tint's palettes run, the stage's running CLUT animations feed nothing (whatever
		 * they fed before the tint), and the palettes as last fed go back to VRAM as the tint's palettes stop.
		 */
		WELTALLCORE_API void EndStageTint();
		/**
		 * The frame's drawing begun (the last frame's links let go), then, when the stage is shown, its model as
		 * the stage's drawing draws it under View before the objects: each shown bone's part by
		 * its draw kind, the kind-0 bones' into the table's farthest entry.
		 */
		WELTALLCORE_API void DrawStage(const FGteMatrix& View, bool bShown);
		/**
		 * Every object's steps for the frame (one for every two frames at full speed) and
		 * its drawing under View; Buffer is the frame's draw buffer (0 or 1), by which the model camera looks for the
		 * floor under its eye once a frame.
		 */
		WELTALLCORE_API void UpdateDraw(const FGteMatrix& View, int32 CatchUp, int32 Buffer);
		/** What the stage and the objects drew in the frame, which the show's frame hands the renderer. */
		const FBattleObjectsFrame& Frame() const { return Drawn; }
		/** The frame drawn handed over for Kept's memory, which the next frame's drawing fills. */
		/** The frame drawn handed over for Kept, with the texture seeds of the objects present. */
		WELTALLCORE_API void ExchangeFrame(FBattleObjectsFrame& Kept);
		/** An object still plays a sequence, or holds a sequence extension (the idle wait's tests). */
		WELTALLCORE_API bool IsPlaying();
		WELTALLCORE_API bool HoldsExtension();
		/** Every object's sequence extension freed. */
		WELTALLCORE_API void FreeExtensions();
		/** The objects asked to be waited for once the disc's reads are done; the ask is taken. */
		WELTALLCORE_API bool TakeIdleWait();
		/** A 3D target reacts: slot Slot's object runs sequence Sequence, hit by the slots of FromSlots. */
		WELTALLCORE_API void React(int32 Slot, uint16 FromSlots, int32 Sequence);
		/** The objects shown or hidden. */
		void SetVisible(bool bVisible) { bObjectsVisible = bVisible; }
		/** Special model Model of directory 40 read (46), made in slot 11 and run as sequence Sequence on Targets (47), freed (51). */
		WELTALLCORE_API void LoadSpecialModel(int32 Model);
		WELTALLCORE_API void RunSpecialModel(int32 Sequence, uint16 Targets);
		WELTALLCORE_API void FreeSpecialModel();

		// Making and freeing objects.
		/** Slot Slot's object from a model and a sequence archive, as Flags ask. */
		WELTALLCORE_API void Create(int32 Slot, uint32 Flags, FGearMotionPack* Motion, std::shared_ptr<const FGearModelPack> Model, const FGearTexturePlace& Place);
		/** A gear's or a special model's object, busy, and a gear variant's sub-objects or images. */
		WELTALLCORE_API void CreateFromFiles(int32 Slot, FBattleModelFiles&& Files, const FGearTexturePlace& Place);
		/** The steps of a gear model's load, which its task runs a frame apart: its files read, its model made. */
		WELTALLCORE_API void ReadGearModelFiles(int32 Slot);
		WELTALLCORE_API void CreateGearModel(int32 Slot);
		/** An object freed (a party slot's sub-objects too), and every slot but the stage's with the pools. */
		WELTALLCORE_API void Free(int32 Slot);
		WELTALLCORE_API void FreeAll();

		// What the show, the playback and the rules ask of them.
		/** Slot plays sequence Sequence on the targets, signalled or not. */
		WELTALLCORE_API void Perform(int32 Slot, uint16 Targets, int32 Sequence);
		WELTALLCORE_API void PerformSignalled(int32 Slot, uint16 Targets, int32 Sequence);
		/** A target runs another object's sequence as its reaction, unless it is the first target. */
		WELTALLCORE_API void ReactFrom(int32 OwnerSlot, int32 Slot, uint16 From, int32 Sequence);
		/** Owner's sequence Sequence on Slot's object, queued while its queue holds. */
		WELTALLCORE_API void StartSequence(int32 Slot, int32 OwnerSlot, int32 Sequence);
		/** An object's radius: 0 for an empty slot. */
		WELTALLCORE_API int32 Radius(int32 Slot) const;
		/** An object's stance set; two objects swapped. */
		WELTALLCORE_API void SetStance(int32 Slot, uint8 NewStance);
		WELTALLCORE_API void Swap(int32 Hidden, int32 Shown);
		/** The other party's removal: every party gear but Kept's plays its leaving sequence (5); whether any is still shown or busy. */
		WELTALLCORE_API void StartRemovingOtherParty(int32 Kept);
		WELTALLCORE_API bool OtherPartyLeaving(int32 Kept) const;
		/** Its end, a frame after none is: the other party gears freed. */
		WELTALLCORE_API void FreeOtherParty(int32 Kept);

		/**
		 * The slot an object sits in now. A swap leaves each object the number of the slot it was made in, which
		 * its sequences go on naming others by; the object itself is still the one running.
		 */
		WELTALLCORE_API int32 PlaceOf(const FBattleObject& Object) const;
		FBattleObject* Object(int32 Slot) { return Slot >= 0 && Slot < SlotCount && Slots[static_cast<std::size_t>(Slot)].bPresent ? &Slots[static_cast<std::size_t>(Slot)].Object : nullptr; }
		const FBattleObject* Object(int32 Slot) const { return Slot >= 0 && Slot < SlotCount && Slots[static_cast<std::size_t>(Slot)].bPresent ? &Slots[static_cast<std::size_t>(Slot)].Object : nullptr; }
		/** How tall slot Slot's object stands, which its proxy sprite takes as its frame's height (0 for none). */
		WELTALLCORE_API int32 Height(int32 Slot) const;

		/** A pack the objects hold from now on, numbered in the order they are taken (the oracle's numbering). */
		WELTALLCORE_API FGearMotionPack* HoldPack(FGearMotionPack Pack);
		WELTALLCORE_API FBattleSequenceExtension* HoldExtension(FBattleSequenceExtension Extension);
		/**
		 * A pack let go. The code places left in an object's own fault from now on; an extension's read on, as the
		 * original reads the block it freed while a bone still streams one of its animations.
		 */
		WELTALLCORE_API void ReleasePack(const FGearMotionPack* Pack);
		/** Whether the pack was let go. */
		WELTALLCORE_API bool IsReleased(const FGearMotionPack* Pack) const;
		bool HoldsPack(const FGearMotionPack* Pack) const override;

		// The module's words, as the original keeps them.
		/** The frames not yet a step (1 + the catch-up each frame, two a step, at most six). */
		int32 Accumulator = 0;
		uint16 WindPhase = 0;
		int16 ClothWind = 0;
		/** The pulse of the objects the scene flags: its phase, its level and the ambient colour it gives. */
		uint16 PulsePhase = 0;
		int16 PulseLevel = 0;
		std::array<uint8, 3> PulsedAmbient{};
		/** The objects the ambient pulses on: the target markers' mask, which the show sets with the markers. */
		uint16 PulseMask = 0;
		bool bObjectsVisible = true;
		bool bShadows = false;
		/** A signalled sequence runs. */
		bool bSignalled = false;
		/** The performer and the play's targets the sequences read. */
		uint16 Performer = 0;
		uint16 PlayTargets = 0;
		/** Op 71's effect number. */
		int16 RequestedEffect = 0;
		/** The shift from a depth to its ordering table entry. */
		int32 OrderingShift = 2;
		/**
		 * The stage's tint's palettes, a CLUT animation that never steps: its first source the palettes kept aside (which
		 * the stage's CLUT animations feed while it runs), its second the tint's recolouring of them. Its fed flag is the
		 * tint layers' changed flag too (one byte in the original).
		 */
		FBattleClutAnimation StageTintPalette;
		FBattleModelCamera ModelCamera;
		std::array<FBattlePointLight, 2> PointLights{};
		/** The stage's light matrix (its direction in row 0) and colour matrix, whose columns 1 and 2 the point lights set. */
		FGteMatrix LightMatrix;
		FGteMatrix ColourMatrix;
		/** The trails' pool (its last quad is where a full pool's requests draw), and its lowest free. */
		std::vector<FBattleTrailSegment> TrailSegments;
		int16 TrailLowest = 0;
		/** The gears' model loads: queued and not made yet, reading, the VRAM places taken, and the idle wait asked once they are made. */
		int32 PendingGearModels = 0;
		int32 ReadingGearModels = 0;
		uint16 GearPlacesTaken = 0;
		bool bIdleWaitAsked = false;

		struct FSlot
		{
			bool bPresent = false;
			FBattleObject Object;
		};
		std::array<FSlot, SlotCount> Slots{};

	private:
		/** The point lights: each at its bone. */
		void UpdatePointLights();
		/** Slot Slot's object drawn: its shadow, its shown bones' parts and its cloths into the frame, and the drawing's logic. */
		void DrawObject(int32 Slot, const FGteMatrix& View, int32 CatchUp);
		/** The trails: the pool's quads into the frame under View, aged by the step's ticks. */
		void DrawTrails(const FGteMatrix& View, int32 Ticks);
		/** The model a gear's files have it wear over the performer's moves, made for Place; none where they ask for none or it cannot be made. */
		static std::shared_ptr<const FBattleGearDress> MakeDress(FBattleModelFiles& Files, FGearMotionPack& Motion, const FGearTexturePlace& Place);
		/** The gear model files read and waiting to be made, by party slot. */
		std::array<std::optional<FBattleModelFiles>, 3> GearFiles;
		std::optional<FBattleModelFiles> SpecialFiles;
		/** The model a create keeps for the next object that shares it, and the last motion pack a create made. */
		std::shared_ptr<const FGearModelPack> KeptModel;
		std::span<const uint8> LastMotionBytes;
		FGearMotionPack* LastMotion = nullptr;
		std::vector<std::unique_ptr<IKernelTask>> GearLoadTasks;
		std::vector<std::unique_ptr<FGearMotionPack>> HeldPacks;
		std::vector<std::unique_ptr<FBattleSequenceExtension>> HeldExtensions;
		std::vector<const FGearMotionPack*> ReleasedPacks;
		FBattleObjectsFrame Drawn;
		/** Scratch: a part's vertices projected. */
		std::vector<FGteProjection> Projected;
	};
}
