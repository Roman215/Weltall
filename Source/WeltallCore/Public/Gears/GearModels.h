#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearChannels.h"
#include "Gears/GearCloth.h"
#include "Gears/GearDress.h"
#include "Gears/GearEngine.h"
#include "Gears/GearFiles.h"
#include "Gears/GearFrame.h"
#include "Gears/GearPackets.h"
#include "Gears/GearSkeleton.h"
#include "Graphics/AffineBlend.h"
#include "Graphics/Gte.h"
#include "Graphics/TexturePictures.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace Weltall
{
	class FPsxRandom;
	struct FGteArctangents;

	/**
	 * A loaded gear model: its skeleton, its script's state, its motion and what the field or the menu
	 * writes into it each tick. The script's places (its instruction, its handlers, its timeline's
	 * events) are places in a motion pack: a model can run another model's scripts.
	 */
	struct FGearModel : FGearBody
	{
		static constexpr uint8 NoHost = 0xFF;

		std::unique_ptr<FGearMotionPack> Motion;
		/** The parts the bones draw. */
		std::shared_ptr<const FMdlModel> Parts;
		uint8 Slot = 0;
		int16 ShadowHalfWidth = 0;
		int16 ShadowHalfDepth = 0;
		uint8 DefaultRecord = 0;
		/** The start queue: 0 off, 1 holding nothing, 2-5 holding one start fewer. */
		uint8 Queue = 0;
		std::array<uint8, 4> QueuedOwners{};
		std::array<uint8, 4> QueuedAnimations{};
		uint8 bShown = 1;
		/** 0: the root's height is pinned to the floor every tick. */
		uint8 FreeHeight = 0;
		uint8 ScaledBones = 0;
		uint8 Signal = 0;
		int16 Mark = -1;
		/** The cloth's vertical wind. */
		int16 WindY = 0;
		uint16 WaitTicks = 0;
		uint16 Loops = 0;
		uint16 Timer = 0;
		uint16 TimerEnd = 0;
		int16 NearDistance = 0;
		uint16 Flags = 0;
		FGearCodeRef Pc;
		FGearCodeRef OnNear;
		FGearCodeRef OnTimer;
		FGearCodeRef OnLanding;
		uint8 HostSlot = NoHost;
		/** Bit 1: the host's bone turns the model too. */
		uint8 HostTurns = 0;
		uint16 HostBone = 0;
		int16 FloorY = 0;
		std::array<int16, 3> AttachOffset{};
		/** The root's angular velocity (eight times its step), and its acceleration. */
		std::array<int16, 3> Spin{};
		std::array<int16, 3> SpinAcceleration{};
		/** In the root's frame. */
		std::array<int16, 3> Velocity{};
		std::array<int16, 3> Acceleration{};
		int16 Speed = 1;
		/** Where the load put the texture blocks (TextureX -1: nowhere) and the CLUTs. */
		int16 TextureX = -1;
		int16 TextureY = 0;
		int16 ClutX = 0;
		int16 ClutY = 0;
		/**
		 * The texture regions its parts and cloths read where the load put the textures, with the colours the load gave
		 * them (Weltall::GearTextureRegions): what names its pictures, whatever later changes their colours.
		 */
		std::shared_ptr<const std::vector<FTexturePictureSeed>> TextureSeeds;
		/** The timeline: its tick (-1 off), its loop length (-1 none), the events done and in it. */
		int16 TimelineTick = -1;
		int16 TimelineLoop = 0;
		int16 TimelineDone = 0;
		int16 TimelineEvents = 0;
		FGearCodeRef NextEvent;
		FGearCodeRef FirstEvent;
		/** The play mask when the running script started. */
		uint16 PlayMask = 0;
		uint8 Trails = 0;
		uint8 TextureAnimations = 0;
		std::vector<FGearCloth> Cloths;
		/** Each bone's part as the load built it, in bone order. */
		std::vector<FGearBonePackets> BonePackets;
		/** The root's position before this tick's update, and the tick's root motion (before - after). */
		std::array<int32, 3> RootBefore{};
		std::array<int32, 3> RootMotion{};
		/**
		 * Where its host draws the model, when not where its matrices have it: the model's own space placed in the
		 * world by a scale and a translation, as real numbers. For a model kept at its files' scale in a space of its
		 * own (its floor at 0) and drawn elsewhere at another size, finer than a matrix's 4.12 entries would hold it
		 * there. None: its matrices are the world's.
		 */
		std::optional<FAffine> Placement;
		/** Another model drawn over its bones in its stead (FGearModels::Wear), and that model's bones as last placed. */
		std::shared_ptr<const IGearDress> Dress;
		std::vector<FGearBone> DressBones;
	};

	/** A texture block a load placed in VRAM, which the renderer uploads. */
	struct FGearUpload
	{
		int16 X = 0;
		int16 Y = 0;
		int16 Width = 0;
		int16 Height = 0;
		std::vector<uint16> Pixels;
	};

	/** A VRAM rectangle a script copies (the original's MoveImage); the GPU refuses one without width or height. */
	struct FGearVramMove
	{
		int16 X = 0;
		int16 Y = 0;
		int16 Width = 0;
		int16 Height = 0;
		int16 ToX = 0;
		int16 ToY = 0;
	};

	struct FGearModelOptions
	{
		/** A full channel pool hands out new channels instead of leaving bones still. */
		bool bChannelPoolGrows = false;
	};

	/**
	 * The gears' skeletal models of a screen (the field's map, the gear shop): ten slots (8 and 9 for
	 * clones, which the port does not make), the channel pool they share, the vblank counter that turns
	 * frames into ticks, the cloth's wind, and the play mask and slot of the last animation played.
	 * What a call cannot do as the original does (a clone, a sound, a trail or light, a read past a
	 * gear's files, a division by zero) stops the module with an error, which every later call returns.
	 */
	class FGearModels final : public FGearEngine
	{
	public:
		static constexpr int32 Slots = 10;
		static constexpr int32 PartTables = 8;

		WELTALLCORE_API FGearModels();
		WELTALLCORE_API ~FGearModels() override;

		/** A fresh module: the counter and the wind at rest, Pool channels, no models; an error for a pool of none. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Init(int32 Pool, const FGearModelOptions& Options = {});

		/**
		 * Loads a gear into an empty slot below 10 (nothing otherwise, as the original), its textures
		 * placed at Place (unless its header says absolute) and its root at Position, and runs its
		 * script 0. The model pack is the load's alone; the model keeps the motion pack.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Load(int32 Slot, FGearModelPack&& ModelPack, FGearMotionPack&& MotionPack, const FGearTexturePlace& Place, const FGteVector& Position);

		/**
		 * Slot's model drawn as Dress from now on: Dress's textures go up at the model's place over its own, its parts
		 * and texture regions are the drawing's; the model goes on as itself. False, changing nothing, for an empty slot.
		 */
		WELTALLCORE_API bool Wear(int32 Slot, std::shared_ptr<const IGearDress> Dress);

		/**
		 * Loads a model as Load does and leaves it still: no script starts, and its motion pack's sound effects are
		 * nobody's. For a model its host poses (DrawPosed) rather than its scripts.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> LoadStill(int32 Slot, FGearModelPack&& ModelPack, FGearMotionPack&& MotionPack, const FGearTexturePlace& Place);

		/** Frees a slot's model and every channel its bones hold. */
		WELTALLCORE_API void Free(int32 Slot);
		/** Every model freed and the pool given back, as a screen releases the module. */
		WELTALLCORE_API void FreeAll();

		/** Starts the slot's script Animation on itself, the play mask Mask; nothing for an empty slot. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> PlayAnimation(int32 Slot, uint16 Mask, int32 Animation);
		/** The same with the owner's scripts, unless the slot is the mask's first. */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> PlayAnimationFrom(int32 Owner, int32 Slot, uint16 Mask, int32 Animation);

		/** While on, the scripts' blends to a pose snap to it (a screen's first ticks). */
		void SetSnapPoses(bool bOn) { bSnapPoses = bOn; }

		/**
		 * One frame: the vblank counter advanced by 1 + ExtraVblanks (it wraps as the original's word
		 * does) turns into ticks, two vblanks a tick, at most three; the wind; every model's update,
		 * its attachment, its root motion; then what each shown model's drawing keeps (its cloth), and
		 * the frame for the renderer.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Tick(uint32 ExtraVblanks);

		/**
		 * A frame of models their host poses: the wind's tick, then each shown model's bones' matrices from the
		 * angles and places its host gave them, its cloth and the frame for the renderer. No channel is stepped and
		 * no script run.
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> DrawPosed();

		/**
		 * The slot's root local matrix (bone 0) or bone's world matrix placed by it; nothing for an empty
		 * slot (the original writes nothing) or for a bone the skeleton does not have (the original reads
		 * past its bones; the port writes nothing, as for an empty slot).
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FGteMatrix> BoneMatrix(int32 Slot, int32 Bone);

		FGearModel* Model(int32 Slot) { return Slot >= 0 && Slot < Slots ? Models[static_cast<std::size_t>(Slot)].get() : nullptr; }
		const FGearModel* Model(int32 Slot) const { return Slot >= 0 && Slot < Slots ? Models[static_cast<std::size_t>(Slot)].get() : nullptr; }
		const FGearChannelPool& Pool() const { return Channels; }
		const FGearFrame& Frame() const { return DrawnFrame; }

		/** The texture blocks and scripted VRAM copies since they were last taken, in order. */
		const std::vector<FGearUpload>& Uploads() const { return PendingUploads; }
		const std::vector<FGearVramMove>& VramMoves() const { return PendingMoves; }
		WELTALLCORE_API void TakeVramChanges();

		// The module's globals, as the original keeps them.
		/** The vblanks not yet a tick (the original's signed word). */
		uint32 VblankCounter = 0;
		uint16 WindPhase = 0;
		int16 Wind = 0;
		bool bSnapPoses = false;
		uint16 PlayMask = 0;
		uint16 PlaySlot = 0;
		/**
		 * The GTE's lighting a load builds its parts' packets with, as the screen left it; nothing
		 * where the previous frame's drawing left it, which a part lit at the build and never again
		 * cannot be loaded under.
		 */
		std::optional<FGte> BuildLighting;
		/** Counts the loads and frees: what the models draw changes with it. */
		uint32 Revision = 0;

		// For the module's own code (Private/Gears).
		std::array<std::unique_ptr<FGearModel>, Slots> Models;
		FGearFrame DrawnFrame;
		std::vector<FGearUpload> PendingUploads;
		std::vector<FGearVramMove> PendingMoves;
		int32 PartTablesUsed = 0;
		FGearModelOptions Options;

		/** A motion pack a loaded model keeps. */
		bool HoldsPack(const FGearMotionPack* Pack) const override;
	};
}
