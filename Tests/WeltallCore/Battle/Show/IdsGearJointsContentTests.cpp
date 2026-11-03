#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Foundation/LittleEndian.h"
#include "Game/CharacterLooks.h"
#include "Gears/GearSkeleton.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>

using namespace Weltall;

namespace
{
	using FPoint = std::array<double, 3>;

	struct FEnd
	{
		uint8 Bone;
		std::vector<uint16> Vertices;
	};

	const std::array<std::pair<FEnd, FEnd>, 8> Ends = {{
		{{12, {13, 14, 15, 16}}, {45, {5, 6, 7, 8, 9}}},
		{{45, {0, 1, 2, 3, 4}}, {44, {1, 5, 8}}},
		{{44, {2, 4, 11, 13, 14}}, {11, {0, 9, 10, 11, 12}}},
		{{20, {13, 14, 15, 16}}, {48, {5, 6, 7, 8, 9}}},
		{{48, {0, 1, 2, 3, 4}}, {47, {1, 5, 8}}},
		{{47, {2, 4, 11, 13, 14}}, {19, {0, 9, 10, 11, 12}}},
		{{56, {4, 5, 8, 10}}, {34, {14, 17, 20, 21}}},
		{{59, {4, 5, 8, 9}}, {41, {14, 17, 20, 21}}},
	}};

	double Distance(const FPoint& A, const FPoint& B)
	{
		double Squared = 0.0;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Squared += (A[Axis] - B[Axis]) * (A[Axis] - B[Axis]);
		}
		return std::sqrt(Squared);
	}

	std::pair<FPoint, double> Rim(const FGearModelPack& Model, const FEnd& End)
	{
		const auto& Vertices = Model.Model.Parts[Model.Skeleton[End.Bone - 1].Part].Vertices;
		FPoint Center{};
		for (const uint16 Index : End.Vertices)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Center[Axis] += static_cast<double>(Vertices[Index][Axis]) / static_cast<double>(End.Vertices.size());
			}
		}
		double Radius = 0.0;
		for (const uint16 Index : End.Vertices)
		{
			const FPoint Vertex = {double(Vertices[Index][0]), double(Vertices[Index][1]), double(Vertices[Index][2])};
			Radius = std::max(Radius, Distance(Vertex, Center));
		}
		return {Center, Radius};
	}

	FPoint InModel(const FPoint& Point, const FGteMatrix& Matrix)
	{
		FPoint Result{};
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Result[Row] = Matrix.T[Row];
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Result[Row] += Matrix.M[Row][Column] * Point[Column] / FGteMatrix::One;
			}
		}
		return Result;
	}
}

TEST_CASE("The red Weltall's shoulder, elbow, wrist and knee ends overlap through both Weltalls' moves", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FBattleObjectTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	for (const uint8 Gear : {FCharacterLooks::Weltall, FCharacterLooks::Weltall2})
	{
		INFO("gear " << int32(Gear));
		auto Own = BattleObjectFiles::ReadGear(Disc->Image(), *Index, *Tables, Gear, 0);
		auto Red = BattleObjectFiles::ReadSceneModel(Disc->Image(), *Index, 74, 0);
		auto Combos = BattleObjectFiles::ReadExtension(Disc->Image(), *Index, Gear == FCharacterLooks::Weltall ? 2 : 3);
		REQUIRE(Own);
		REQUIRE(Red);
		REQUIRE(Combos);
		FGearModelPack Model = Red->Model;
		auto Dress = FBattleGearDress::Make(*FBattleGearDress::IdsPairing(Gear), Own->Model, Own->Motion, std::move(Model), Red->Motion, nullptr);
		REQUIRE(Dress);
		auto Built = ReadGearPose(Own->Model.Skeleton, Own->Motion, 0);
		REQUIRE(Built);
		std::vector<FGearBone> Performer;
		MakeGearSkeleton(Own->Model.Skeleton, Performer);
		std::vector<FGearBone> Placed;
		std::array<double, Ends.size()> Furthest{};
		int32 Ticks = 0;
		for (FGearMotionPack* Motion : {&Own->Motion, &Combos->Pack})
		{
			for (int32 Record = 0; Motion->Record(Record); ++Record)
			{
				auto Track = BakeGearRecord(Own->Model.Skeleton, *Motion, Record, -1);
				REQUIRE(Track);
				const std::size_t At = *Motion->Record(Record);
				const uint16 Rotations = LittleEndian::ReadU16(Motion->Bytes, At + 0xC);
				const uint16 Translations = LittleEndian::ReadU16(Motion->Bytes, At + 0xE);
				for (const FGearPose& Tick : Track->Ticks)
				{
					FGearPose Pose = *Built;
					for (std::size_t Bone = 1; Bone < Pose.size(); ++Bone)
					{
						Pose[Bone].Rotation = Bone <= Rotations ? Tick[Bone].Rotation : Pose[Bone].Rotation;
						Pose[Bone].Position = Bone <= Translations ? Tick[Bone].Position : Pose[Bone].Position;
					}
					ApplyGearPose(Pose, Performer);
					ComputeGearBones(Performer, FGteMatrix::One);
					Dress->Place(Performer, Placed);
					++Ticks;
					for (std::size_t Joint = 0; Joint < Ends.size(); ++Joint)
					{
						const auto& [Upper, Lower] = Ends[Joint];
						const auto [A, RadiusA] = Rim(Dress->Model(), Upper);
						const auto [B, RadiusB] = Rim(Dress->Model(), Lower);
						const double Gap = Distance(InModel(A, Placed[Upper.Bone].World), InModel(B, Placed[Lower.Bone].World));
						REQUIRE(std::isfinite(Gap));
						Furthest[Joint] = std::max(Furthest[Joint], Gap / std::min(RadiusA, RadiusB));
					}
				}
			}
		}
		CHECK(Ticks > 500);
		for (std::size_t Joint = 0; Joint < Ends.size(); ++Joint)
		{
			INFO("pieces " << int32(Ends[Joint].first.Bone) << ", " << int32(Ends[Joint].second.Bone));
			// Even the most bent ends meet within their narrower rim's radius; a few units allow matrix rounding.
			CHECK(Furthest[Joint] < 1.02);
		}
	}
}
