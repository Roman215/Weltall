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

// Fenrir's pieces worn over the first Heimdal's moving skeleton, with the disc's own models and moves.
namespace
{
	constexpr uint8 Heimdal = FCharacterLooks::Heimdal;
	constexpr uint8 Fenrir = FCharacterLooks::Fenrir;
	/** Heimdal's combos: the extension its sequence 1Ah loads. */
	constexpr int32 HeimdalCombos = 5;
	constexpr std::size_t RecordRotations = 0xC;
	constexpr std::size_t RecordTranslations = 0xE;
	/** Heimdal's hands on Fenrir's grip's side (a fist, an open hand), and on the other side; Fenrir's grip, sword strip, fist, open hand. */
	constexpr std::size_t HeimdalFist = 23;
	constexpr std::size_t HeimdalOpenHand = 24;
	constexpr std::size_t HeimdalOtherFist = 29;
	constexpr std::size_t HeimdalOtherOpenHand = 30;
	constexpr std::size_t FenrirGrip = 12;
	constexpr std::size_t FenrirSwordStrip = 46;
	constexpr std::size_t FenrirFist = 50;
	constexpr std::size_t FenrirOpenHand = 51;
	/** Each side's soles: Heimdal's foot and toe, Fenrir's foot. */
	constexpr std::array<std::array<std::size_t, 2>, 2> HeimdalSoles = {{{6, 7}, {12, 13}}};
	constexpr std::array<std::size_t, 2> FenrirSoles = {54, 55};

	struct FGears
	{
		FBattleModelFiles Heimdal;
		FBattleModelFiles Fenrir;
		FBattleSequenceExtension Combos;
	};

	std::optional<FGears> ReadGears(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		auto Tables = FBattleObjectTables::Load(Disc.Image(), *Index);
		REQUIRE(Tables);
		auto First = BattleObjectFiles::ReadGear(Disc.Image(), *Index, *Tables, Heimdal, 0);
		auto Second = BattleObjectFiles::ReadGear(Disc.Image(), *Index, *Tables, Fenrir, 0);
		auto Combos = BattleObjectFiles::ReadExtension(Disc.Image(), *Index, HeimdalCombos);
		REQUIRE(First);
		REQUIRE(Second);
		REQUIRE(Combos);
		return FGears{std::move(*First), std::move(*Second), std::move(*Combos)};
	}

	FBattleGearDress MakeDress(FGears& Gears)
	{
		const FBattleGearDress::FPairing* Pairing = FBattleGearDress::FindPairing(Heimdal, Fenrir);
		REQUIRE(Pairing != nullptr);
		FGearModelPack Wearer = Gears.Fenrir.Model;
		const FGearTexturePlace Place{0x340, 0, 0, 0x1C1};
		auto Dress = FBattleGearDress::Make(*Pairing, Gears.Heimdal.Model, Gears.Heimdal.Motion, std::move(Wearer), Gears.Fenrir.Motion, &Place);
		REQUIRE(Dress);
		return std::move(*Dress);
	}

	/** A skeleton's bones standing in Pose, their matrices computed. */
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
}

TEST_CASE("Over the first Heimdal standing as it was built, Fenrir's pieces stand as Fenrir was built", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	const FBattleGearDress Dress = MakeDress(*Gears);
	auto HeimdalBuilt = ReadGearPose(Gears->Heimdal.Model.Skeleton, Gears->Heimdal.Motion, 0);
	auto FenrirBuilt = ReadGearPose(Gears->Fenrir.Model.Skeleton, Gears->Fenrir.Motion, 0);
	REQUIRE(HeimdalBuilt);
	REQUIRE(FenrirBuilt);
	const std::vector<FGearBone> Performer = Standing(Gears->Heimdal.Model, *HeimdalBuilt);
	const std::vector<FGearBone> Built = Standing(Gears->Fenrir.Model, *FenrirBuilt);
	std::vector<FGearBone> Worn;
	Dress.Place(Performer, Worn);
	REQUIRE(Worn.size() == Built.size());
	// The legs' bones the solve bends: a leg as near straight as built turns its knee a long way for the unit or two the
	// game's matrices and the built pose round apart.
	constexpr std::array<std::size_t, 16> LegBones = {19, 20, 21, 22, 23, 26, 27, 28, 29, 30, 42, 43, 44, 45, 54, 55};
	std::array<int32, 2> Turned{};
	std::array<int32, 2> Moved{};
	for (std::size_t Bone = 1; Bone < Worn.size(); ++Bone)
	{
		INFO("bone " << Bone);
		CHECK(Worn[Bone].Part == Built[Bone].Part);
		const std::size_t Kind = std::find(LegBones.begin(), LegBones.end(), Bone) != LegBones.end() ? 1 : 0;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Turned[Kind] = std::max(Turned[Kind], std::abs(Worn[Bone].World.M[Row][Column] - Built[Bone].World.M[Row][Column]));
			}
			Moved[Kind] = std::max(Moved[Kind], std::abs(Worn[Bone].World.T[Row] - Built[Bone].World.T[Row]));
		}
	}
	// The rest within a few 4096ths of a turn and a few units, the built pose's own rounding; the legs within a degree.
	CHECK(Turned[0] <= 8);
	CHECK(Moved[0] <= 3);
	CHECK(Turned[1] <= 72);
	CHECK(Moved[1] <= 30);
}

TEST_CASE("Fenrir's soles stand where the first Heimdal's do through every move of its combos", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	const FBattleGearDress Dress = MakeDress(*Gears);
	const FGearModelPack& Model = Gears->Heimdal.Model;
	auto Stance = ReadGearPose(Model.Skeleton, Gears->Heimdal.Motion, 0);
	REQUIRE(Stance);
	FGearMotionPack& Combos = Gears->Combos.Pack;
	int32 Records = 0;
	int32 Grounded = 0;
	double Furthest = 0.0;
	for (int32 Record = 0; Combos.Record(Record); ++Record)
	{
		INFO("record " << Record + 0x40);
		auto Track = BakeGearRecord(Model.Skeleton, Combos, Record, -1);
		REQUIRE(Track);
		// A combo's record holds the turns and places of its first bones; the rest keep the stance's.
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
			std::vector<FGearBone> Worn;
			Dress.Place(Performer, Worn);
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				const double HeimdalLow = std::max(Lowest(Model, Performer, HeimdalSoles[Side][0]), Lowest(Model, Performer, HeimdalSoles[Side][1]));
				const double FenrirLow = Lowest(Dress.Model(), Worn, FenrirSoles[Side]);
				if (HeimdalLow > -40.0)
				{
					++Grounded;
					Furthest = std::max(Furthest, std::abs(FenrirLow - HeimdalLow));
				}
			}
		}
		++Records;
	}
	CHECK(Records == 22);
	CHECK(Grounded > 400);
	// Where a leg at its full length falls short, the hips come down until it reaches.
	CHECK(Furthest < 40.0);
}

TEST_CASE("Fenrir's hands follow the first Heimdal's fists and open hands, the strip its sword lies along never shown", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	const FBattleGearDress Dress = MakeDress(*Gears);
	auto Built = ReadGearPose(Gears->Heimdal.Model.Skeleton, Gears->Heimdal.Motion, 0);
	REQUIRE(Built);
	std::vector<FGearBone> Performer = Standing(Gears->Heimdal.Model, *Built);
	std::vector<FGearBone> Worn;
	const auto Shown = [&](bool bFist, bool bOpenHand, bool bOtherFist, bool bOtherOpenHand) {
		Performer[HeimdalFist].bShown = bFist ? 1 : 0;
		Performer[HeimdalOpenHand].bShown = bOpenHand ? 1 : 0;
		Performer[HeimdalOtherFist].bShown = bOtherFist ? 1 : 0;
		Performer[HeimdalOtherOpenHand].bShown = bOtherOpenHand ? 1 : 0;
		Dress.Place(Performer, Worn);
		return std::array<bool, 4>{Worn[FenrirGrip].bShown != 0, Worn[FenrirSwordStrip].bShown != 0, Worn[FenrirFist].bShown != 0, Worn[FenrirOpenHand].bShown != 0};
	};
	// As Heimdal's opening sequence leaves them: an open hand on the grip's side, a fist on the other.
	CHECK(Shown(false, true, true, false) == std::array<bool, 4>{true, false, true, false});
	CHECK(Shown(true, false, false, true) == std::array<bool, 4>{true, false, false, true});
	CHECK(Shown(false, false, false, false) == std::array<bool, 4>{false, false, false, false});
	// Every other piece is shown while its counterpart is.
	int32 Others = 0;
	for (std::size_t Bone = 1; Bone < Worn.size(); ++Bone)
	{
		if (Worn[Bone].Part != FGearBone::NoPart && Bone != FenrirGrip && Bone != FenrirSwordStrip && Bone != FenrirFist && Bone != FenrirOpenHand)
		{
			Others += Worn[Bone].bShown;
		}
	}
	CHECK(Others == 19);
}

TEST_CASE("Only Fenrir over the first Heimdal is paired, and only with the disc's own models", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	CHECK(FBattleGearDress::FindPairing(Heimdal, FCharacterLooks::SwordHeimdal) == nullptr);
	CHECK(FBattleGearDress::FindPairing(Fenrir, Heimdal) == nullptr);
	const FBattleGearDress::FPairing* Pairing = FBattleGearDress::FindPairing(Heimdal, Fenrir);
	REQUIRE(Pairing != nullptr);
	// The two the other way round are not the skeletons it was drawn up for.
	FGearModelPack Wearer = Gears->Heimdal.Model;
	CHECK_FALSE(FBattleGearDress::Make(*Pairing, Gears->Fenrir.Model, Gears->Fenrir.Motion, std::move(Wearer), Gears->Heimdal.Motion, nullptr));
}
