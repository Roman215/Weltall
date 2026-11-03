#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Foundation/LittleEndian.h"
#include "Game/CharacterLooks.h"
#include "Gears/GearPoseTrack.h"
#include "Gears/GearSkeleton.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

using namespace Weltall;

// Id's red gears worn over Fei's gears' moves, with the disc's own models and moves.
namespace
{
	constexpr uint8 WeltallGear = FCharacterLooks::Weltall;
	constexpr uint8 Weltall2Gear = FCharacterLooks::Weltall2;
	constexpr uint8 XenogearsGear = FCharacterLooks::Xenogears;
	constexpr std::size_t RecordRotations = 0xC;
	constexpr std::size_t RecordTranslations = 0xE;

	struct FWorn
	{
		FBattleModelFiles Gear;
		FBattleModelFiles Red;
		FBattleSequenceExtension Combos;
	};

	/** Fei's gear Gear, the red gear its pairing wears over it, and the combos its sequence 1Ah loads (Combos). */
	FWorn Read(const FIso9660& Disc, uint8 Gear, int32 Combos)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		auto Tables = FBattleObjectTables::Load(Disc.Image(), *Index);
		REQUIRE(Tables);
		const FBattleGearDress::FPairing* Pairing = FBattleGearDress::IdsPairing(Gear);
		REQUIRE(Pairing != nullptr);
		auto Own = BattleObjectFiles::ReadGear(Disc.Image(), *Index, *Tables, Gear, 0);
		auto Red = BattleObjectFiles::ReadSceneModel(Disc.Image(), *Index, Pairing->WearerScene, Pairing->WearerEntry);
		auto Extension = BattleObjectFiles::ReadExtension(Disc.Image(), *Index, Combos);
		REQUIRE(Own);
		REQUIRE(Red);
		REQUIRE(Extension);
		return FWorn{std::move(*Own), std::move(*Red), std::move(*Extension)};
	}

	FBattleGearDress Dress(FWorn& Worn, uint8 Gear)
	{
		FGearModelPack Wearer = Worn.Red.Model;
		const FGearTexturePlace Place{0x340, 0, 0, 0x1C0};
		auto Made = FBattleGearDress::Make(*FBattleGearDress::IdsPairing(Gear), Worn.Gear.Model, Worn.Gear.Motion, std::move(Wearer), Worn.Red.Motion, &Place);
		REQUIRE(Made);
		return std::move(*Made);
	}

	std::vector<FGearBone> Standing(const FGearModelPack& Pack, const FGearPose& Pose)
	{
		std::vector<FGearBone> Bones;
		MakeGearSkeleton(Pack.Skeleton, Bones);
		ApplyGearPose(Pose, Bones);
		ComputeGearBones(Bones, FGteMatrix::One);
		return Bones;
	}

	/** How low a bone's piece reaches (the model's y grows downward). */
	double Lowest(const FGearModelPack& Pack, const std::vector<FGearBone>& Bones, std::size_t Bone)
	{
		const FGteMatrix& World = Bones[Bone].World;
		double Low = -std::numeric_limits<double>::infinity();
		for (const FGteVector& Vertex : Pack.Model.Parts[Bones[Bone].Part].Vertices)
		{
			const double Y = (World.M[1][0] * static_cast<double>(Vertex[0]) + World.M[1][1] * static_cast<double>(Vertex[1]) + World.M[1][2] * static_cast<double>(Vertex[2])) / FGteMatrix::One;
			Low = std::max(Low, Y + World.T[1]);
		}
		return Low;
	}

	struct FStray
	{
		int32 Turned = 0;
		int32 Moved = 0;
	};

	/**
	 * Over the gear standing as built, how far the red gear's pieces stray from the red gear as it was built: the most a
	 * turn's entry and a place stray, in the legs the dress bends and in the rest.
	 */
	std::array<FStray, 2> StrayAsBuilt(FWorn& Worn, const FBattleGearDress& Worn2, const FBattleGearDress::FPairing& Pairing)
	{
		auto GearBuilt = ReadGearPose(Worn.Gear.Model.Skeleton, Worn.Gear.Motion, 0);
		auto RedBuilt = ReadGearPose(Worn.Red.Model.Skeleton, Worn.Red.Motion, 0);
		REQUIRE(GearBuilt);
		REQUIRE(RedBuilt);
		const std::vector<FGearBone> Performer = Standing(Worn.Gear.Model, *GearBuilt);
		const std::vector<FGearBone> Built = Standing(Worn.Red.Model, *RedBuilt);
		std::vector<FGearBone> Placed;
		Worn2.Place(Performer, Placed);
		REQUIRE(Placed.size() == Built.size());
		std::array<FStray, 2> Found{};
		for (std::size_t Bone = 1; Bone < Placed.size(); ++Bone)
		{
			CHECK(Placed[Bone].Part == Built[Bone].Part);
			bool bLeg = false;
			for (const FBattleGearDress::FLeg& Leg : Pairing.Legs)
			{
				bLeg = bLeg || std::find(Leg.Thigh.begin(), Leg.Thigh.end(), Bone) != Leg.Thigh.end() || std::find(Leg.Shin.begin(), Leg.Shin.end(), Bone) != Leg.Shin.end();
			}
			FStray& Stray = Found[bLeg ? 1 : 0];
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				for (std::size_t Column = 0; Column < 3; ++Column)
				{
					Stray.Turned = std::max(Stray.Turned, std::abs(Placed[Bone].World.M[Row][Column] - Built[Bone].World.M[Row][Column]));
				}
				// The red Weltall's open limb ends are fitted together; its spine, wings and solved feet keep their places.
				constexpr std::array<std::size_t, 14> Fitted = {8, 9, 10, 11, 16, 17, 18, 19, 44, 45, 47, 48, 56, 59};
				if (Pairing.WearerScene != 74 || std::find(Fitted.begin(), Fitted.end(), Bone) == Fitted.end())
				{
					Stray.Moved = std::max(Stray.Moved, std::abs(Placed[Bone].World.T[Row] - Built[Bone].World.T[Row]));
				}
			}
		}
		return Found;
	}

	struct FSoles
	{
		int32 Grounded = 0;
		double Furthest = 0.0;
	};

	/**
	 * Through every tick of the combos, how far the red gear's soles (RedSoles, one a side) stray from the gear's lowest
	 * point on that side (GearSoles), scaled as the pairing scales the moves, where that point is on the floor: against
	 * the height the two stood apart as built.
	 */
	FSoles Soles(FWorn& Worn, const FBattleGearDress& Worn2, std::array<std::array<std::size_t, 2>, 2> GearSoles, std::array<std::size_t, 2> RedSoles)
	{
		const FGearModelPack& Model = Worn.Gear.Model;
		auto Stance = ReadGearPose(Model.Skeleton, Worn.Gear.Motion, 0);
		auto RedBuilt = ReadGearPose(Worn.Red.Model.Skeleton, Worn.Red.Motion, 0);
		REQUIRE(Stance);
		REQUIRE(RedBuilt);
		const std::vector<FGearBone> GearStanding = Standing(Model, *Stance);
		const std::vector<FGearBone> RedStanding = Standing(Worn.Red.Model, *RedBuilt);
		const double Scale = Worn2.Pairing().bScaledMoves ? static_cast<double>(RedStanding[1].World.T[1]) / GearStanding[1].World.T[1] : 1.0;
		std::array<double, 2> BuiltGap{};
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			const double GearLow = std::max(Lowest(Model, GearStanding, GearSoles[Side][0]), Lowest(Model, GearStanding, GearSoles[Side][1]));
			BuiltGap[Side] = Lowest(Worn.Red.Model, RedStanding, RedSoles[Side]) - GearLow * Scale;
		}
		FGearMotionPack& Combos = Worn.Combos.Pack;
		FSoles Found;
		for (int32 Record = 0; Combos.Record(Record); ++Record)
		{
			INFO("record " << Record + 0x40);
			auto Track = BakeGearRecord(Model.Skeleton, Combos, Record, -1);
			REQUIRE(Track);
			const std::size_t At = *Combos.Record(Record);
			const std::size_t Turns = LittleEndian::ReadU16(Combos.Bytes, At + RecordRotations);
			const std::size_t Places = LittleEndian::ReadU16(Combos.Bytes, At + RecordTranslations);
			for (const FGearPose& Tick : Track->Ticks)
			{
				FGearPose Pose = *Stance;
				for (std::size_t Bone = 1; Bone < Pose.size(); ++Bone)
				{
					Pose[Bone].Rotation = Bone <= Turns ? Tick[Bone].Rotation : Pose[Bone].Rotation;
					Pose[Bone].Position = Bone <= Places ? Tick[Bone].Position : Pose[Bone].Position;
				}
				const std::vector<FGearBone> Performer = Standing(Model, Pose);
				std::vector<FGearBone> Placed;
				Worn2.Place(Performer, Placed);
				for (std::size_t Side = 0; Side < 2; ++Side)
				{
					const double GearLow = std::max(Lowest(Model, Performer, GearSoles[Side][0]), Lowest(Model, Performer, GearSoles[Side][1]));
					if (GearLow > -40.0)
					{
						++Found.Grounded;
						const double Gap = Lowest(Worn2.Model(), Placed, RedSoles[Side]) - GearLow * Scale;
						Found.Furthest = std::max(Found.Furthest, std::abs(Gap - BuiltGap[Side]));
					}
				}
			}
		}
		return Found;
	}

	/** Weltall's feet and toes, Weltall-2's feet and heels; the red Weltall's feet. */
	constexpr std::array<std::array<std::size_t, 2>, 2> WeltallSoles = {{{7, 8}, {14, 15}}};
	constexpr std::array<std::array<std::size_t, 2>, 2> Weltall2Soles = {{{48, 49}, {57, 58}}};
	constexpr std::array<std::size_t, 2> RedWeltallSoles = {36, 43};
	/** The combos each gear's sequence 1Ah loads. */
	constexpr int32 WeltallCombos = 2;
	constexpr int32 Weltall2Combos = 3;
	constexpr int32 XenogearsCombos = 17;
}

TEST_CASE("Only Fei's three gears are drawn as Id's, each with the red gear of its time", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	for (uint8 Gear = 0; Gear < 20; ++Gear)
	{
		INFO("gear " << int32(Gear));
		const FBattleGearDress::FPairing* Pairing = FBattleGearDress::IdsPairing(Gear);
		CHECK((Pairing != nullptr) == (Gear == WeltallGear || Gear == Weltall2Gear || Gear == XenogearsGear));
		if (Pairing != nullptr)
		{
			// The red Weltall of the first disc's fights (Elly's against it) while Fei has Weltall or Weltall-2, the red
			// gear of the second's once he has Xenogears.
			CHECK(Pairing->WearerScene == (Gear == XenogearsGear ? 70 : 74));
			CHECK(Pairing->WearerEntry == 0);
		}
	}
	// The pairings are no gear's own model: the gears' lookup has none of them.
	CHECK(FBattleGearDress::FindPairing(WeltallGear, WeltallGear) == nullptr);
	CHECK(FBattleGearDress::FindPairing(XenogearsGear, XenogearsGear) == nullptr);
	// A skeleton the pairing was not drawn up for is refused: Weltall's red gear over Weltall-2.
	FWorn Worn = Read(*Disc, WeltallGear, WeltallCombos);
	FWorn Other = Read(*Disc, Weltall2Gear, Weltall2Combos);
	FGearModelPack Wearer = Worn.Red.Model;
	CHECK_FALSE(FBattleGearDress::Make(*FBattleGearDress::IdsPairing(WeltallGear), Other.Gear.Model, Other.Gear.Motion, std::move(Wearer), Worn.Red.Motion, nullptr));
}

TEST_CASE("Over Fei's gears standing as built, Id's red gears keep their built turns, spine and feet", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	for (const auto [Gear, Combos] : {std::pair{WeltallGear, WeltallCombos}, std::pair{Weltall2Gear, Weltall2Combos}, std::pair{XenogearsGear, XenogearsCombos}})
	{
		INFO("gear " << int32(Gear));
		FWorn Worn = Read(*Disc, Gear, Combos);
		const FBattleGearDress Made = Dress(Worn, Gear);
		const std::array<FStray, 2> Stray = StrayAsBuilt(Worn, Made, *FBattleGearDress::IdsPairing(Gear));
		// The rest within a few 4096ths of a turn and a few units, the built poses' own rounding; a leg as near straight as
		// built turns its knee a long way for the unit or two the matrices round apart: within a degree and a half.
		CHECK(Stray[0].Turned <= 8);
		CHECK(Stray[0].Moved <= 3);
		CHECK(Stray[1].Turned <= 96);
		CHECK(Stray[1].Moved <= 40);
	}
}

TEST_CASE("The red Weltall's soles stand where Weltall's and Weltall-2's do through their combos, at its own size over Weltall", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	{
		FWorn Worn = Read(*Disc, WeltallGear, WeltallCombos);
		const FBattleGearDress Made = Dress(Worn, WeltallGear);
		const FSoles Found = Soles(Worn, Made, WeltallSoles, RedWeltallSoles);
		CHECK(Found.Grounded > 300);
		CHECK(Found.Furthest < 40.0);
	}
	{
		FWorn Worn = Read(*Disc, Weltall2Gear, Weltall2Combos);
		const FBattleGearDress Made = Dress(Worn, Weltall2Gear);
		const FSoles Found = Soles(Worn, Made, Weltall2Soles, RedWeltallSoles);
		CHECK(Found.Grounded > 300);
		CHECK(Found.Furthest < 40.0);
	}
}

TEST_CASE("Over Xenogears' combos the red gear hovers as built, its hips raised by the height they stand apart, its whole legs as Xenogears' own", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	// Xenogears hovers on whole legs (bones 42 and 65), as the red gear does on its own (bones 10 and 9); the floor is
	// no measure of either, whose legs go through it in some moves.
	constexpr std::array<std::size_t, 2> XenogearsLegs = {42, 65};
	constexpr std::array<std::size_t, 2> RedGearLegs = {10, 9};
	FWorn Worn = Read(*Disc, XenogearsGear, XenogearsCombos);
	const FBattleGearDress Made = Dress(Worn, XenogearsGear);
	const FGearModelPack& Model = Worn.Gear.Model;
	auto Stance = ReadGearPose(Model.Skeleton, Worn.Gear.Motion, 0);
	auto RedBuilt = ReadGearPose(Worn.Red.Model.Skeleton, Worn.Red.Motion, 0);
	REQUIRE(Stance);
	REQUIRE(RedBuilt);
	const std::vector<FGearBone> GearStanding = Standing(Model, *Stance);
	const std::vector<FGearBone> RedStanding = Standing(Worn.Red.Model, *RedBuilt);
	const int32 Raise = RedStanding[1].World.T[1] - GearStanding[1].World.T[1];
	CHECK(Raise < -900);
	// A leg's turn from the pose it was built in.
	const auto SinceBuilt = [](const FGteMatrix& Now, const FGteMatrix& Built, std::size_t Row, std::size_t Column) {
		double Sum = 0.0;
		for (std::size_t Inner = 0; Inner < 3; ++Inner)
		{
			Sum += static_cast<double>(Now.M[Row][Inner]) * Built.M[Column][Inner];
		}
		return Sum / FGteMatrix::One;
	};
	FGearMotionPack& Combos = Worn.Combos.Pack;
	int32 Ticks = 0;
	int32 HipsAstray = 0;
	double LegsAstray = 0.0;
	for (int32 Record = 0; Combos.Record(Record); ++Record)
	{
		auto Track = BakeGearRecord(Model.Skeleton, Combos, Record, -1);
		REQUIRE(Track);
		const std::size_t At = *Combos.Record(Record);
		const std::size_t Turns = LittleEndian::ReadU16(Combos.Bytes, At + RecordRotations);
		const std::size_t Places = LittleEndian::ReadU16(Combos.Bytes, At + RecordTranslations);
		for (const FGearPose& Tick : Track->Ticks)
		{
			FGearPose Pose = *Stance;
			for (std::size_t Bone = 1; Bone < Pose.size(); ++Bone)
			{
				Pose[Bone].Rotation = Bone <= Turns ? Tick[Bone].Rotation : Pose[Bone].Rotation;
				Pose[Bone].Position = Bone <= Places ? Tick[Bone].Position : Pose[Bone].Position;
			}
			const std::vector<FGearBone> Performer = Standing(Model, Pose);
			std::vector<FGearBone> Placed;
			Made.Place(Performer, Placed);
			++Ticks;
			HipsAstray = std::max({HipsAstray, std::abs(Placed[1].World.T[0] - Performer[1].World.T[0]), std::abs(Placed[1].World.T[1] - Performer[1].World.T[1] - Raise),
				std::abs(Placed[1].World.T[2] - Performer[1].World.T[2])});
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				for (std::size_t Row = 0; Row < 3; ++Row)
				{
					for (std::size_t Column = 0; Column < 3; ++Column)
					{
						const double Red = SinceBuilt(Placed[RedGearLegs[Side]].World, RedStanding[RedGearLegs[Side]].World, Row, Column);
						const double Own = SinceBuilt(Performer[XenogearsLegs[Side]].World, GearStanding[XenogearsLegs[Side]].World, Row, Column);
						LegsAstray = std::max(LegsAstray, std::abs(Red - Own));
					}
				}
			}
		}
	}
	CHECK(Ticks > 500);
	CHECK(HipsAstray <= 2);
	// Within a few 4096ths of a turn: the matrices' rounding.
	CHECK(LegsAstray <= 16.0);
}
