#include "Field/FieldRenderState.h"
#include "Game/PsxRandom.h"
#include "Gears/GearShopModel.h"
#include "Menu/MenuGearView.h"
#include "Support/GameDiscs.h"
#include "Support/GearDigest.h"
#include "Support/GearDisc.h"
#include "Support/OracleDigest.h"

#include <catch_amalgamated.hpp>

#include <optional>

using namespace Weltall;
using Testing::FOracleDigest;

// The gear shop's model as the shop runs the game's own gears' module, under the oracle:
// each gear of the shop's table shown (its
// files loaded into slot 1 with the shop's texture place, its floor and scale from the table, its root
// turned, its animation played), 24 menu frames at the shop's cadence (three ticks every other frame),
// then the next gear switched to and 24 frames more. Each frame's digest covers the module's words,
// the model's state, its bones and cloths and the channel pool.

namespace
{
	constexpr int32 Frames = 24;

	struct FShopCase
	{
		int32 Gear;
		int32 NextGear;
		const char* Digest;
	};

	constexpr FShopCase Cases[] = {
		{0, 1, "3eee51bd8c502c18ea65cfed61fec9d375ea14e3"},
		{1, 2, "6ecb67085bc8d72b500c2e296fb79c60d742fdfd"},
		{2, 3, "d69a39e48051949810ffeaa5beeaf32387bb29b2"},
		{3, 4, "c96defadbe63543f41df51168ed5649ab05c0e82"},
		{4, 5, "4f05150529acb86c117fbf3ddce1e27465c04460"},
		{5, 6, "3826656080c1a5923402342275912043ffd26987"},
		{6, 7, "1a80873b71fe9f30b60dd4d35debb418fea2f152"},
		{7, 8, "a3755033c54c7a3c1dc3c2b60df0f8d399981a91"},
		{8, 9, "e6d24bee09814a664b9536394dad7fa3891e9730"},
		{9, 10, "7c7826d35573a31a10d742e1e5cfe0a7b9094aed"},
		{10, 11, "c3d4e0b1711069e5c6a51b6a3da526746b8f8ef3"},
		{11, 12, "f9f9551cf7fa2c74c41339c3f7a73ed9621c7bc5"},
		{12, 13, "f19ed23918f68e7ea68073afbef5714292842067"},
		{13, 14, "2cf293be44af7f5449a6ae0d91d3d67108d23d99"},
		{14, 15, "b96b914108d1fca3a4f374ac16e3a151ce9b06e4"},
		{15, 16, "9b6ef7505276be63d09226b3ead9a135b26dc7b6"},
		{16, 0, "ac9340b12a07e79b3e01c44fc468137447bf4bdb"},
	};

	struct FShopDisc
	{
		Testing::FGearDisc Gears;
		FGearShopTable Table;
	};

	std::optional<FShopDisc> ReadShopDisc(const FIso9660& Disc)
	{
		std::optional<Testing::FGearDisc> Gears = Testing::ReadGearDisc(Disc);
		if (!Gears)
		{
			return std::nullopt;
		}
		auto Table = ReadGearShopTable(Disc.Image(), Gears->Index);
		if (!Table)
		{
			return std::nullopt;
		}
		return FShopDisc{std::move(*Gears), *Table};
	}
}

TEST_CASE("The gear shop's table gives each gear its model, animation, floor and scale", "[Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FShopDisc> Shop = ReadShopDisc(*Disc);
	REQUIRE(Shop);
	const FGearShopGear& Weltall = Shop->Table.Entries[0];
	CHECK(Weltall.GearModel == 0);
	CHECK(Weltall.Animation == 13);
	CHECK(Weltall.Height == 200);
	CHECK(Weltall.Scale == 240);
	// Gear 10 is Weltall's model again, standing as another; gear 7 has no floor, scale or animation.
	CHECK(Shop->Table.Entries[10].GearModel == 0);
	CHECK(Shop->Table.Entries[10].Scale == 250);
	CHECK(Shop->Table.Entries[7].GearModel == 36);
	CHECK(Shop->Table.Entries[7].Scale == 0);
	CHECK(Shop->Table.Entries[16].GearModel == 17);
}

TEST_CASE("The gear shop shows each gear and switches to the next as the shop runs the original's module", "[Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FShopDisc> Shop = ReadShopDisc(*Disc);
	REQUIRE(Shop);
	for (const FShopCase& Case : Cases)
	{
		INFO("gear " << Case.Gear << " then " << Case.NextGear);
		FPsxRandom Random;
		FGearShopModel Model;
		REQUIRE_FALSE(Model.Open());
		Model.Models().Random = &Random;
		Model.Models().Arctangents = &Shop->Gears.Arctangents;
		// The oracle leaves the packets unbuilt: any lighting builds them.
		Model.SetBuildLighting(FGte{});
		FOracleDigest Digest;
		for (const int32 Gear : {Case.Gear, Case.NextGear})
		{
			const FGearShopGear& Entry = Shop->Table.Entries[static_cast<std::size_t>(Gear)];
			REQUIRE_FALSE(Model.Show(Entry, Shop->Gears.Files[static_cast<std::size_t>(Entry.GearModel)]));
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				REQUIRE_FALSE(Model.Frame());
				Testing::DigestGearModels(Digest, Model.Models());
			}
		}
		CHECK(Digest.Hex() == Case.Digest);
	}
}

TEST_CASE("Every gear the shop shows loads under the shop's own light, and one needs a lighting known", "[Gears][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FShopDisc> Shop = ReadShopDisc(*Disc);
	REQUIRE(Shop);
	FPsxRandom Random;
	FGearShopModel Model;
	Model.Models().Random = &Random;
	Model.Models().Arctangents = &Shop->Gears.Arctangents;
	REQUIRE_FALSE(Model.Open());
	for (int32 Gear = 0; Gear < FGearShopTable::Gears; ++Gear)
	{
		INFO("gear " << Gear);
		const FGearShopGear& Entry = Shop->Table.Entries[static_cast<std::size_t>(Gear)];
		CHECK_FALSE(Model.Show(Entry, Shop->Gears.Files[static_cast<std::size_t>(Entry.GearModel)]));
		REQUIRE_FALSE(Model.Frame());
	}
	// Gear 12's model has polygons lit at the build and never again: an unknown lighting refuses it.
	REQUIRE_FALSE(Model.Open());
	Model.SetBuildLighting(std::nullopt);
	const FGearShopGear& Twelve = Shop->Table.Entries[12];
	CHECK(Model.Show(Twelve, Shop->Gears.Files[static_cast<std::size_t>(Twelve.GearModel)]).has_value());
	// Open makes the module afresh.
	REQUIRE_FALSE(Model.Open());
	CHECK_FALSE(Model.Show(Twelve, Shop->Gears.Files[static_cast<std::size_t>(Twelve.GearModel)]));
}

TEST_CASE("The gear shop lights its gear with one grey light, at the loads and at the drawing", "[Gears]")
{
	const FGte Build = FGearShopModel::ShopBuildLighting();
	const FFieldGearLights& Drawn = GearShopLights();
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		INFO("channel " << Channel);
		const std::size_t Row = static_cast<std::size_t>(Channel);
		CHECK(Build.ColorMatrix.M[Row][0] == FGearShopModel::ColorScale);
		CHECK(Build.ColorMatrix.M[Row][1] == 0);
		CHECK(Build.ColorMatrix.M[Row][2] == 0);
		CHECK(Drawn.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(0, Channel))] == FGearShopModel::ColorScale);
		CHECK(Drawn.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(1, Channel))] == 0);
		CHECK(Drawn.Words[static_cast<std::size_t>(FFieldGearLights::ColorWord(2, Channel))] == 0);
	}
}
