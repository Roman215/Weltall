#include "Gears/GearPoseTrack.h"

#include "GearEngineSteps.h"
#include "Gears/GearFiles.h"
#include "Gears/GearSkeleton.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Weltall
{
	namespace
	{
		using FQuaternion = std::array<float, 4>;

		/** The engine a record is played out on: it holds the one pack, and hands out as many channels as the bones ask for. */
		class FTrackEngine final : public FGearEngine
		{
		public:
			FTrackEngine(const FGearMotionPack& InPack, std::size_t Bones) : FGearEngine("a gear's pose track"), Pack(&InPack)
			{
				constexpr int32 ChannelsPerBone = 3;
				Channels.Init(static_cast<int32>(Bones) * ChannelsPerBone, true);
			}

			bool HoldsPack(const FGearMotionPack* Asked) const override { return Asked == Pack; }

		private:
			const FGearMotionPack* Pack;
		};

		FQuaternion Normalized(const FQuaternion& Turn)
		{
			const float Length = std::sqrt(Turn[0] * Turn[0] + Turn[1] * Turn[1] + Turn[2] * Turn[2] + Turn[3] * Turn[3]);
			if (Length <= 0.0f)
			{
				return {0.0f, 0.0f, 0.0f, 1.0f};
			}
			return {Turn[0] / Length, Turn[1] / Length, Turn[2] / Length, Turn[3] / Length};
		}

		/** The turn B then A. */
		FQuaternion Product(const FQuaternion& A, const FQuaternion& B)
		{
			return {A[3] * B[0] + A[0] * B[3] + A[1] * B[2] - A[2] * B[1], A[3] * B[1] - A[0] * B[2] + A[1] * B[3] + A[2] * B[0],
				A[3] * B[2] + A[0] * B[1] - A[1] * B[0] + A[2] * B[3], A[3] * B[3] - A[0] * B[0] - A[1] * B[1] - A[2] * B[2]};
		}

		FQuaternion Inverse(const FQuaternion& Turn)
		{
			return {-Turn[0], -Turn[1], -Turn[2], Turn[3]};
		}

		std::array<float, 3> Rotated(const FQuaternion& Turn, const std::array<float, 3>& Vector)
		{
			const FQuaternion Moved = Product(Product(Turn, {Vector[0], Vector[1], Vector[2], 0.0f}), Inverse(Turn));
			return {Moved[0], Moved[1], Moved[2]};
		}

		/** A bone's parent past the root, none for a bone the skeleton hangs from the model itself. */
		int32 ParentOf(std::span<const FGearSkeletonEntry> Skeleton, int32 Bone)
		{
			const uint16 Parent = Skeleton[static_cast<std::size_t>(Bone - 1)].Parent;
			return Parent == 0xFFFF ? 0 : Parent + 1;
		}

		bool InPose(const FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone)
		{
			return Bone > 0 && static_cast<std::size_t>(Bone) < Pose.size() && static_cast<std::size_t>(Bone) <= Skeleton.size();
		}

		/** A rotation matrix's turn, by the largest of the quaternion's four parts. */
		FQuaternion TurnOf(const FGteMatrix& Matrix)
		{
			constexpr float One = static_cast<float>(FGteMatrix::One);
			float M[3][3];
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					M[Row][Column] = static_cast<float>(Matrix.M[Row][Column]) / One;
				}
			}
			const float Trace = M[0][0] + M[1][1] + M[2][2];
			FQuaternion Turn;
			if (Trace > 0.0f)
			{
				const float S = std::sqrt(Trace + 1.0f) * 2.0f;
				Turn = {(M[2][1] - M[1][2]) / S, (M[0][2] - M[2][0]) / S, (M[1][0] - M[0][1]) / S, S / 4.0f};
			}
			else if (M[0][0] > M[1][1] && M[0][0] > M[2][2])
			{
				const float S = std::sqrt(std::max(1.0f + M[0][0] - M[1][1] - M[2][2], 0.0f)) * 2.0f;
				Turn = {S / 4.0f, (M[0][1] + M[1][0]) / S, (M[0][2] + M[2][0]) / S, (M[2][1] - M[1][2]) / S};
			}
			else if (M[1][1] > M[2][2])
			{
				const float S = std::sqrt(std::max(1.0f + M[1][1] - M[0][0] - M[2][2], 0.0f)) * 2.0f;
				Turn = {(M[0][1] + M[1][0]) / S, S / 4.0f, (M[1][2] + M[2][1]) / S, (M[0][2] - M[2][0]) / S};
			}
			else
			{
				const float S = std::sqrt(std::max(1.0f + M[2][2] - M[0][0] - M[1][1], 0.0f)) * 2.0f;
				Turn = {(M[0][2] + M[2][0]) / S, (M[1][2] + M[2][1]) / S, S / 4.0f, (M[1][0] - M[0][1]) / S};
			}
			return Normalized(Turn);
		}

		/** A turn as the rotation part of a GTE matrix, each entry rounded. */
		void SetTurn(const FQuaternion& Turn, FGteMatrix& Matrix)
		{
			const float X = Turn[0], Y = Turn[1], Z = Turn[2], W = Turn[3];
			const float M[3][3] = {
				{1.0f - 2.0f * (Y * Y + Z * Z), 2.0f * (X * Y - Z * W), 2.0f * (X * Z + Y * W)},
				{2.0f * (X * Y + Z * W), 1.0f - 2.0f * (X * X + Z * Z), 2.0f * (Y * Z - X * W)},
				{2.0f * (X * Z - Y * W), 2.0f * (Y * Z + X * W), 1.0f - 2.0f * (X * X + Y * Y)}};
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Matrix.M[Row][Column] = static_cast<int16>(std::lround(M[Row][Column] * static_cast<float>(FGteMatrix::One)));
				}
			}
		}

		/** A body of the skeleton standing in the pose of record Base (none below 0), and Record read. */
		std::optional<GearMachine::FGearRecord> Start(FTrackEngine& Engine, FGearBody& Body, std::span<const FGearSkeletonEntry> Skeleton, FGearMotionPack& Motion,
			int32 Record, int32 Base)
		{
			MakeGearSkeleton(Skeleton, Body.Bones);
			const auto Read = [&Engine, &Motion](int32 Number) -> std::optional<GearMachine::FGearRecord>
			{
				const std::optional<uint32> At = Motion.Record(Number);
				if (!At)
				{
					Engine.Fail("record " + std::to_string(Number) + " is past the pack's bank");
					return std::nullopt;
				}
				return GearMachine::ReadRecord(Engine, {&Motion, *At});
			};
			if (Base >= 0)
			{
				if (const std::optional<GearMachine::FGearRecord> Stood = Read(Base))
				{
					GearMachine::SnapPose(Engine, Body, *Stood);
				}
			}
			return Engine.Faulted() ? std::nullopt : Read(Record);
		}
	}

	void BlendGearPoses(const FGearPose& From, const FGearPose& To, float Weight, FGearPose& Blended)
	{
		if (From.size() != To.size() || Weight >= 1.0f)
		{
			Blended = To;
			return;
		}
		Weight = std::max(Weight, 0.0f);
		Blended.resize(To.size());
		for (std::size_t Bone = 0; Bone < To.size(); ++Bone)
		{
			const FQuaternion& A = From[Bone].Rotation;
			const FQuaternion& B = To[Bone].Rotation;
			// The same turn has two quaternions: the one on From's side is the short way.
			const float Side = A[0] * B[0] + A[1] * B[1] + A[2] * B[2] + A[3] * B[3] < 0.0f ? -1.0f : 1.0f;
			FQuaternion Turn;
			for (std::size_t Part = 0; Part < 4; ++Part)
			{
				Turn[Part] = A[Part] + (Side * B[Part] - A[Part]) * Weight;
			}
			Blended[Bone].Rotation = Normalized(Turn);
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Blended[Bone].Position[Axis] = From[Bone].Position[Axis] + (To[Bone].Position[Axis] - From[Bone].Position[Axis]) * Weight;
			}
		}
	}

	void FGearPoseTrack::Sample(int32 Phase, FGearPose& Pose) const
	{
		if (Ticks.empty())
		{
			Pose.clear();
			return;
		}
		const int32 Round = Cycle();
		Phase %= Round;
		if (Phase < 0)
		{
			Phase += Round;
		}
		const std::size_t First = static_cast<std::size_t>(Phase / TickPhase);
		const std::size_t Second = (First + 1) % Ticks.size();
		BlendGearPoses(Ticks[First], Ticks[Second], static_cast<float>(Phase % TickPhase) / static_cast<float>(TickPhase), Pose);
	}

	FGearPose GearPoseOf(std::span<const FGearBone> Bones)
	{
		FGearPose Pose(Bones.size());
		for (std::size_t Index = 1; Index < Bones.size(); ++Index)
		{
			const FGearBone& Bone = Bones[Index];
			Pose[Index].Rotation = TurnOf(GearBoneRotation(Bone.Angles, Bone.RotationOrder));
			Pose[Index].Position = {static_cast<float>(Bone.Position[0]), static_cast<float>(Bone.Position[1]), static_cast<float>(Bone.Position[2])};
		}
		return Pose;
	}

	FGearPose GearPoseOfPlaced(std::span<const FGearBone> Bones)
	{
		FGearPose Pose(Bones.size());
		for (std::size_t Index = 1; Index < Bones.size(); ++Index)
		{
			const FGearBone& Bone = Bones[Index];
			const FQuaternion Turn = TurnOf(Bone.World);
			std::array<float, 3> Place = {static_cast<float>(Bone.World.T[0]), static_cast<float>(Bone.World.T[1]), static_cast<float>(Bone.World.T[2])};
			if (Bone.Parent == FGearBone::NoParent)
			{
				Pose[Index] = {Turn, Place};
				continue;
			}
			const FGteMatrix& Above = Bones[static_cast<std::size_t>(Bone.Parent)].World;
			const FQuaternion Back = Inverse(TurnOf(Above));
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Place[Axis] -= static_cast<float>(Above.T[Axis]);
			}
			Pose[Index] = {Normalized(Product(Back, Turn)), Rotated(Back, Place)};
		}
		return Pose;
	}

	TResult<FGearPose> ReadGearPose(std::span<const FGearSkeletonEntry> Skeleton, FGearMotionPack& Motion, int32 Record)
	{
		FTrackEngine Engine(Motion, Skeleton.size() + 1);
		FGearBody Body;
		if (const std::optional<GearMachine::FGearRecord> Read = Start(Engine, Body, Skeleton, Motion, Record, -1))
		{
			GearMachine::SnapPose(Engine, Body, *Read);
		}
		if (Engine.Fault)
		{
			return *Engine.Fault;
		}
		return GearPoseOf(Body.Bones);
	}

	TResult<FGearPoseTrack> BakeGearRecord(std::span<const FGearSkeletonEntry> Skeleton, FGearMotionPack& Motion, int32 Record, int32 Base)
	{
		FTrackEngine Engine(Motion, Skeleton.size() + 1);
		FGearBody Body;
		FGearPoseTrack Track;
		if (const std::optional<GearMachine::FGearRecord> Read = Start(Engine, Body, Skeleton, Motion, Record, Base))
		{
			GearMachine::SnapPose(Engine, Body, *Read);
			if (Read->bPoseOnly)
			{
				Track.Ticks.push_back(GearPoseOf(Body.Bones));
			}
			else
			{
				Track.Stride = Read->Stride < 0 ? -Read->Stride : Read->Stride;
				GearMachine::PlayRecord(Engine, Body, *Read, 1, 0);
				// The root's channels carry the model, which its host does.
				GearMachine::FreeBoneChannels(Engine, Body, 0, 7);
				for (int32 Tick = 0; Tick < Read->Length && !Engine.Faulted(); ++Tick)
				{
					GearMachine::StepChannels(Engine, Body);
					Track.Ticks.push_back(GearPoseOf(Body.Bones));
				}
			}
		}
		if (Engine.Fault)
		{
			return *Engine.Fault;
		}
		return Track;
	}

	void ApplyGearPose(const FGearPose& Pose, std::span<FGearBone> Bones)
	{
		for (std::size_t Index = 1; Index < Bones.size() && Index < Pose.size(); ++Index)
		{
			FGearBone& Bone = Bones[Index];
			SetTurn(Pose[Index].Rotation, Bone.Local);
			Bone.Position = {static_cast<int32>(std::lround(Pose[Index].Position[0])), static_cast<int32>(std::lround(Pose[Index].Position[1])),
				static_cast<int32>(std::lround(Pose[Index].Position[2]))};
			// The local turn is given whole: its angles are not what it is computed from.
			Bone.bRotationDirty = 0;
			Bone.bWorldDirty = 1;
		}
	}

	std::array<float, 4> GearBoneModelTurn(const FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone)
	{
		FQuaternion Turn{0.0f, 0.0f, 0.0f, 1.0f};
		for (int32 At = Bone; InPose(Pose, Skeleton, At); At = ParentOf(Skeleton, At))
		{
			Turn = Product(Pose[static_cast<std::size_t>(At)].Rotation, Turn);
		}
		return Normalized(Turn);
	}

	std::array<float, 3> GearBoneModelPlace(const FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone)
	{
		if (!InPose(Pose, Skeleton, Bone))
		{
			return {};
		}
		const int32 Parent = ParentOf(Skeleton, Bone);
		const std::array<float, 3> Own = Rotated(GearBoneModelTurn(Pose, Skeleton, Parent), Pose[static_cast<std::size_t>(Bone)].Position);
		const std::array<float, 3> Above = GearBoneModelPlace(Pose, Skeleton, Parent);
		return {Above[0] + Own[0], Above[1] + Own[1], Above[2] + Own[2]};
	}

	void TurnGearBoneInModel(FGearPose& Pose, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, const std::array<float, 4>& Turn)
	{
		if (!InPose(Pose, Skeleton, Bone))
		{
			return;
		}
		const FQuaternion Parent = GearBoneModelTurn(Pose, Skeleton, ParentOf(Skeleton, Bone));
		FQuaternion& Local = Pose[static_cast<std::size_t>(Bone)].Rotation;
		Local = Normalized(Product(Product(Inverse(Parent), Product(Turn, Parent)), Local));
	}

	bool IsGearBoneBelow(std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, int32 Above)
	{
		for (int32 At = Bone; At > 0 && static_cast<std::size_t>(At) <= Skeleton.size(); At = ParentOf(Skeleton, At))
		{
			if (At == Above)
			{
				return true;
			}
		}
		return false;
	}

	void CopyGearLimb(const FGearPose& From, std::span<const FGearSkeletonEntry> Skeleton, int32 Bone, FGearPose& Pose)
	{
		if (!InPose(Pose, Skeleton, Bone) || From.size() != Pose.size())
		{
			return;
		}
		for (int32 Other = 1; InPose(Pose, Skeleton, Other); ++Other)
		{
			if (IsGearBoneBelow(Skeleton, Other, Bone))
			{
				Pose[static_cast<std::size_t>(Other)] = From[static_cast<std::size_t>(Other)];
			}
		}
	}
}
