#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Graphics/Gte.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	struct FGearBone;
	struct FGearMotionPack;
	struct FGearSkeletonEntry;

	/**
	 * A bone's turn and place in its parent's frame. The turn is a unit quaternion (x, y, z, w) rather than the
	 * bone's three angles: a model's records do not all write one turn with the same angles, and two poses blend
	 * the short way only as turns.
	 */
	struct FGearBonePose
	{
		std::array<float, 4> Rotation{0.0f, 0.0f, 0.0f, 1.0f};
		std::array<float, 3> Position{};

		bool operator==(const FGearBonePose&) const = default;
	};

	/** A skeleton's pose, by bone number. The root's entry is its host's to give: a pose places the bones, not the model. */
	using FGearPose = std::vector<FGearBonePose>;

	/**
	 * An animation record played out through the engine's own steps: the pose it leaves after each of its ticks. A
	 * looping record goes round its ticks; whatever shows a model outside its scripts' pace (slower, faster, by the
	 * ground it covers) samples between them.
	 */
	struct FGearPoseTrack
	{
		/** A tick as Sample's phase counts it. */
		static constexpr int32 TickPhase = 0x1000;

		std::vector<FGearPose> Ticks;
		/** How far a cycle carries the model ahead, in model units: the root's own motion the record holds. 0: played in place. */
		int32 Stride = 0;

		/** A cycle's length as a phase. */
		int32 Cycle() const { return static_cast<int32>(Ticks.size()) * TickPhase; }

		/** The pose Phase into the cycle (it goes round), between the two ticks it lies between. */
		WELTALLCORE_API void Sample(int32 Phase, FGearPose& Pose) const;
	};

	/** Weight (0 to 1) of the way from From to To, each bone turning the short way; poses of other sizes give To. */
	WELTALLCORE_API void BlendGearPoses(const FGearPose& From, const FGearPose& To, float Weight, FGearPose& Blended);

	/** The pose a skeleton's bones hold: each one's turn from its angles in its order, and its place. */
	WELTALLCORE_API FGearPose GearPoseOf(std::span<const FGearBone> Bones);

	/** The pose that places a skeleton's bones where their world matrices put them: each one's turn and place in its parent's frame. */
	WELTALLCORE_API FGearPose GearPoseOfPlaced(std::span<const FGearBone> Bones);

	/**
	 * Record's pose snapped onto a skeleton as its load leaves it, as a script's snap leaves the bones: a record
	 * places only the bones it holds. An error for a record past the pack's bank, or one that reads past the pack.
	 */
	WELTALLCORE_API TResult<FGearPose> ReadGearPose(std::span<const FGearSkeletonEntry> Skeleton, FGearMotionPack& Motion, int32 Record);

	/**
	 * Record played as a loop from its own pose, snapped over Base's (the pose record the model stands in before,
	 * which gives the bones Record does not hold their place): one pose per tick of its length, or the one for a
	 * record that is only a pose. The root's own channels are left out: its host moves the model.
	 */
	WELTALLCORE_API TResult<FGearPoseTrack> BakeGearRecord(std::span<const FGearSkeletonEntry> Skeleton, FGearMotionPack& Motion, int32 Record, int32 Base);

	/** Gives the bones past the root a pose: their local turns and places, their matrices to be computed again. */
	WELTALLCORE_API void ApplyGearPose(const FGearPose& Pose, std::span<FGearBone> Bones);

	/** A bone's turn in the model's own space under a pose: its parents' local turns and its own, the root's left out. */
	WELTALLCORE_API std::array<float, 4> GearBoneModelTurn(const FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone);

	/** Where a bone lies in the model's own space under a pose, the root's place left out. */
	WELTALLCORE_API std::array<float, 3> GearBoneModelPlace(const FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone);

	/**
	 * Turns a bone, and the bones below it with it, by Turn (a unit quaternion) about an axis of the model's own space
	 * through the bone's place: a pose no record holds, made from one that does.
	 */
	WELTALLCORE_API void TurnGearBoneInModel(FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, const std::array<float, 4>& Turn);

	/** Whether Bone is Above or hangs below it, however far down. */
	WELTALLCORE_API bool IsGearBoneBelow(std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, int32 Above);

	/** Gives a limb (Bone and the bones below it) From's turns and places, the rest of Pose kept: one pose's arm on another. */
	WELTALLCORE_API void CopyGearLimb(const FGearPose& From, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, FGearPose& Pose);
}
