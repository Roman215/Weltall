#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleObjectFiles.h"
#include "Battle/Show/BattleRegurus.h"
#include "Foundation/LittleEndian.h"
#include "Gears/GearPackets.h"
#include "Gears/GearPoseTrack.h"
#include "Gears/GearSkeleton.h"
#include "Gears/GearTextureRegions.h"
#include "Graphics/Texture.h"
#include "Graphics/Vram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Text/MesResident.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace Weltall;

// Regurus, Elly's Omnigear in the port's clear file, fights as Vierge wearing its own model.
namespace
{
	using BattleRegurus::Regurus;
	using BattleRegurus::Vierge;
	/** Vierge's combos: the extension its sequence 1Ah loads. */
	constexpr int32 ViergeCombos = 4;
	constexpr std::size_t RecordRotations = 0xC;
	constexpr std::size_t RecordTranslations = 0xE;
	/** Each side's soles: Vierge's foot, Regurus' foot. */
	constexpr std::array<std::size_t, 2> ViergeSoles = {12, 15};
	constexpr std::array<std::size_t, 2> RegurusSoles = {34, 40};

	struct FGears
	{
		FBattleModelFiles Vierge;
		FBattleModelFiles Regurus;
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
		auto Performer = BattleObjectFiles::ReadGear(Disc.Image(), *Index, *Tables, Vierge, 0);
		auto Wearer = BattleObjectFiles::ReadGear(Disc.Image(), *Index, *Tables, Regurus, 0);
		auto Combos = BattleObjectFiles::ReadExtension(Disc.Image(), *Index, ViergeCombos);
		REQUIRE(Performer);
		REQUIRE(Wearer);
		REQUIRE(Combos);
		return FGears{std::move(*Performer), std::move(*Wearer), std::move(*Combos)};
	}

	/** Where the battle puts Elly's gear's textures: the first gear place, the CLUTs on the party's third row. */
	constexpr FGearTexturePlace Place{0x340, 0, 0, 0x1C1};

	FBattleGearDress MakeDress(FGears& Gears)
	{
		const FBattleGearDress::FPairing* Pairing = FBattleGearDress::FindPairing(Vierge, Regurus);
		REQUIRE(Pairing != nullptr);
		FGearModelPack Wearer = Gears.Regurus.Model;
		auto Dress = FBattleGearDress::Make(*Pairing, Gears.Vierge.Model, Gears.Vierge.Motion, std::move(Wearer), Gears.Regurus.Motion, &Place);
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

TEST_CASE("Over Vierge standing as she was built, Regurus' pieces stand as Regurus was built", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	const FBattleGearDress Dress = MakeDress(*Gears);
	auto ViergeBuilt = ReadGearPose(Gears->Vierge.Model.Skeleton, Gears->Vierge.Motion, 0);
	auto RegurusBuilt = ReadGearPose(Gears->Regurus.Model.Skeleton, Gears->Regurus.Motion, 0);
	REQUIRE(ViergeBuilt);
	REQUIRE(RegurusBuilt);
	const std::vector<FGearBone> Performer = Standing(Gears->Vierge.Model, *ViergeBuilt);
	const std::vector<FGearBone> Built = Standing(Gears->Regurus.Model, *RegurusBuilt);
	std::vector<FGearBone> Worn;
	Dress.Place(Performer, Worn);
	REQUIRE(Worn.size() == Built.size());
	int32 Turned = 0;
	int32 Moved = 0;
	for (std::size_t Bone = 1; Bone < Worn.size(); ++Bone)
	{
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			Moved = std::max(Moved, std::abs(Worn[Bone].World.T[Row] - Built[Bone].World.T[Row]));
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Turned = std::max(Turned, std::abs(Worn[Bone].World.M[Row][Column] - Built[Bone].World.M[Row][Column]));
			}
		}
	}
	// Within half a degree and 8 units (the hips scaled to Regurus' height from Vierge's place).
	CHECK(Turned <= 28);
	CHECK(Moved <= 8);
}

TEST_CASE("Regurus' soles stand where Vierge's do through every move of her combos", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	const FBattleGearDress Dress = MakeDress(*Gears);
	const FGearModelPack& Model = Gears->Vierge.Model;
	auto Stance = ReadGearPose(Model.Skeleton, Gears->Vierge.Motion, 0);
	REQUIRE(Stance);
	FGearMotionPack& Combos = Gears->Combos.Pack;
	int32 Records = 0;
	int32 Grounded = 0;
	double Furthest = 0.0;
	std::string Far;
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
				const double ViergeLow = Lowest(Model, Performer, ViergeSoles[Side]);
				const double RegurusLow = Lowest(Dress.Model(), Worn, RegurusSoles[Side]);
				if (ViergeLow > -40.0)
				{
					++Grounded;
					Furthest = std::max(Furthest, std::abs(RegurusLow - ViergeLow));
					if (std::abs(RegurusLow - ViergeLow) > 40.0)
					{
						Far += " " + std::to_string(Record + 0x40) + "/" + std::to_string(Side);
					}
				}
			}
		}
		++Records;
	}
	CHECK(Records == 19);
	CHECK(Grounded > 600);
	// Within 40 units but for two ticks of the long lunge (record 4Ah), where Regurus' leg at its full length stays short.
	CHECK(Far == " 74/0 74/0");
	CHECK(Furthest < 160.0);
}

TEST_CASE("Regurus over Vierge is paired only with the disc's own models", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	CHECK(FBattleGearDress::FindPairing(Regurus, Vierge) == nullptr);
	const FBattleGearDress::FPairing* Pairing = FBattleGearDress::FindPairing(Vierge, Regurus);
	REQUIRE(Pairing != nullptr);
	FGearModelPack Wearer = Gears->Vierge.Model;
	CHECK_FALSE(FBattleGearDress::Make(*Pairing, Gears->Regurus.Model, Gears->Regurus.Motion, std::move(Wearer), Gears->Vierge.Motion, nullptr));
}

TEST_CASE("Regurus' attacks are named as Vierge's, whose twelve names the English release left Regurus untranslated", "[Battle][Content]")
{
	CHECK(BattleRegurus::NamingGear(Regurus) == Vierge);
	for (uint8 Gear = 0; Gear < 19; ++Gear)
	{
		if (Gear != Regurus)
		{
			CHECK(BattleRegurus::NamingGear(Gear) == Gear);
		}
	}
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resident = LoadMesResident(Disc->Image(), *Index, *Executable);
	REQUIRE(Resident);
	// The kernel's gear attack names, a table a gear from 28: Vierge's in Latin letters (the font's codes below 80h), Regurus'
	// as many, none of them so.
	constexpr int32 GearAttackNameTables = 28;
	const auto Latin = [](std::span<const uint8> Name) { return !Name.empty() && Name[0] < 0x80; };
	int32 Names = 0;
	for (int32 Entry = 3; Entry < 15; ++Entry)
	{
		CAPTURE(Entry);
		const std::span<const uint8> ViergeName = Resident->NameTables.String(GearAttackNameTables + Vierge, Entry);
		const std::span<const uint8> RegurusName = Resident->NameTables.String(GearAttackNameTables + Regurus, Entry);
		CHECK(Latin(ViergeName));
		CHECK_FALSE(Latin(RegurusName));
		Names += !RegurusName.empty();
	}
	CHECK(Names == 12);
}

TEST_CASE("Regurus keeps Vierge's sixteen drones, textured from her own pictures in the port's rows as her place textures them", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	std::optional<FGears> Gears = ReadGears(*Disc);
	REQUIRE(Gears);
	const FBattleGearDress Dress = MakeDress(*Gears);
	const FGearTexturePlace& Kept = Dress.PerformerPlace();
	CHECK((Kept.X == Place.X && Kept.Y == Place.Y + FVram::Height && Kept.ClutX == Place.ClutX && Kept.ClutY == Place.ClutY + FVram::Height));
	std::vector<uint16> Bones;
	for (const FGearBonePackets& Packets : Dress.PerformerPackets())
	{
		Bones.push_back(Packets.Bone);
	}
	CHECK(Bones == std::vector<uint16>{48, 49, 50, 51, 52, 53, 54, 55, 57, 59, 63, 64, 65, 66, 67, 68});

	// VRAM as a battle in Regurus leaves it (its own textures at the place, Vierge's in the port's rows), and as one in Vierge.
	FVram Worn;
	UploadGearTextures(Worn, Gears->Regurus.Model.Textures, &Place);
	UploadGearTextures(Worn, Dress.PerformerTextures(), &Kept);
	FVram Own;
	UploadGearTextures(Own, Gears->Vierge.Model.Textures, &Place);
	std::vector<FGearBone> ViergesBones;
	MakeGearSkeleton(Gears->Vierge.Model.Skeleton, ViergesBones);
	constexpr int32 PartLightMode = 2;
	const auto Hers = BuildGearPackets(Gears->Vierge.Model.Model, ViergesBones, PartLightMode, &Place, FGte{});
	REQUIRE(Hers);
	int32 Texels = 0;
	int32 Overwritten = 0;
	for (const FGearBonePackets& Packets : Dress.PerformerPackets())
	{
		const auto Same = std::find_if(Hers->begin(), Hers->end(), [&](const FGearBonePackets& Each) { return Each.Bone == Packets.Bone; });
		REQUIRE(Same != Hers->end());
		const FMdlPart& Piece = Gears->Vierge.Model.Model.Parts[Packets.Part];
		for (std::size_t Number = 0; Number < Piece.Primitives.size(); ++Number)
		{
			const FMdlPrimitive& Primitive = Piece.Primitives[Number];
			if (!Primitive.Traits().bTextured)
			{
				continue;
			}
			const FTexturePage PortPage = FTexturePage::FromPortAttribute(Packets.Textures[Number].TexturePage);
			const FClutPosition PortClut = FClutPosition::FromPortAttribute(Packets.Textures[Number].Clut);
			const FTexturePage HerPage = FTexturePage::FromAttribute(Same->Textures[Number].TexturePage);
			const FClutPosition HerClut = FClutPosition::FromAttribute(Same->Textures[Number].Clut);
			CHECK(PortPage.Y >= FVram::Height);
			CHECK(PortClut.Y >= FVram::Height);
			for (std::size_t Corner = 0; Corner < Primitive.Traits().Corners; ++Corner)
			{
				const FMdlUv& Uv = Primitive.Uvs[Corner];
				const uint16 Texel = FetchTexel(Own, HerPage, HerClut, Uv.U, Uv.V);
				CHECK(FetchTexel(Worn, PortPage, PortClut, Uv.U, Uv.V) == Texel);
				Overwritten += FetchTexel(Worn, HerPage, HerClut, Uv.U, Uv.V) != Texel ? 1 : 0;
				++Texels;
			}
		}
	}
	CHECK(Texels > 0);
	// At Vierge's own place Regurus' textures would have them drawn otherwise.
	CHECK(Overwritten > 0);
}
