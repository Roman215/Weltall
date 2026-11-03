#include "Game/CharacterLooks.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;

namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Citan = 2;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 GrownEmeralda = 10;
	constexpr uint8 WeltallGear = 0;
	constexpr uint8 Heimdal = 3;
	constexpr uint8 Crescens = 9;
	constexpr uint8 Fenrir = 11;
	constexpr uint8 SwordHeimdal = 16;
}

TEST_CASE("Until the player switches a look, everyone is drawn as the story has them", "[Game]")
{
	const FCharacterLooks Looks;
	CHECK_FALSE(Looks.IsSet());
	for (uint8 Character = 0; Character <= 10; ++Character)
	{
		CHECK(Looks.Drawn(Character) == Character);
	}
	CHECK(Looks.PerformingGear(SwordHeimdal) == SwordHeimdal);
	CHECK(Looks.PerformingGear(Fenrir) == Fenrir);
	CHECK_FALSE(Looks.WearsOwnModel(Fenrir));
}

TEST_CASE("The second Citan without his sword is drawn as the first, his gears doing the first Heimdal's moves", "[Game]")
{
	FCharacterLooks Looks;
	CHECK(Looks.Switch(SwordCitan));
	CHECK(Looks.IsSet());
	CHECK(Looks.Drawn(SwordCitan) == Citan);
	// The second Heimdal is the first altogether; Fenrir keeps its own model over the first Heimdal's moves.
	CHECK(Looks.PerformingGear(SwordHeimdal) == Heimdal);
	CHECK_FALSE(Looks.WearsOwnModel(SwordHeimdal));
	CHECK(Looks.PerformingGear(Fenrir) == Heimdal);
	CHECK(Looks.WearsOwnModel(Fenrir));
	// The others are as they were.
	CHECK(Looks.PerformingGear(WeltallGear) == WeltallGear);
	CHECK_FALSE(Looks.WearsOwnModel(Heimdal));
	CHECK_FALSE(Looks.WearsOwnModel(WeltallGear));
	CHECK(Looks.Drawn(GrownEmeralda) == GrownEmeralda);
	CHECK(Looks.Drawn(Citan) == Citan);
	// Switched again: the sword back.
	CHECK(Looks.Switch(SwordCitan));
	CHECK_FALSE(Looks.IsSet());
	CHECK(Looks.Drawn(SwordCitan) == SwordCitan);
}

TEST_CASE("The grown Emeralda is drawn as the child, her Crescens as it is", "[Game]")
{
	FCharacterLooks Looks;
	CHECK(Looks.Switch(GrownEmeralda));
	CHECK(Looks.Drawn(GrownEmeralda) == Emeralda);
	CHECK(Looks.PerformingGear(Crescens) == Crescens);
	CHECK_FALSE(Looks.WearsOwnModel(Fenrir));
	CHECK(Looks.Drawn(SwordCitan) == SwordCitan);
}

TEST_CASE("Only the second Citan, the grown Emeralda and, in a clear game, Fei and Elly have another look", "[Game]")
{
	FCharacterLooks Looks;
	for (const uint8 Character : {Citan, Emeralda, uint8{3}, uint8{7}, uint8{8}, uint8{11}, uint8{0xFF}})
	{
		CHECK_FALSE(FCharacterLooks::CanSwitch(Character, false));
		CHECK_FALSE(FCharacterLooks::CanSwitch(Character, true));
		CHECK_FALSE(Looks.Switch(Character));
	}
	CHECK_FALSE(Looks.IsSet());
	CHECK(FCharacterLooks::CanSwitch(SwordCitan, false));
	CHECK(FCharacterLooks::CanSwitch(GrownEmeralda, false));
	CHECK_FALSE(FCharacterLooks::CanSwitch(FCharacterLooks::Elly, false));
	CHECK(FCharacterLooks::CanSwitch(FCharacterLooks::Elly, true));
	CHECK_FALSE(FCharacterLooks::CanSwitch(Fei, false));
	CHECK(FCharacterLooks::CanSwitch(Fei, true));
}

TEST_CASE("Elly's purple hair is her own pictures recoloured: she is drawn as herself, the look kept in the save's byte", "[Game]")
{
	FCharacterLooks Looks;
	CHECK_FALSE(Looks.HasPurpleHair(FCharacterLooks::Elly));
	CHECK(Looks.Switch(FCharacterLooks::Elly));
	CHECK(Looks.HasPurpleHair(FCharacterLooks::Elly));
	CHECK_FALSE(Looks.HasPurpleHair(Fei));
	CHECK(Looks.Drawn(FCharacterLooks::Elly) == FCharacterLooks::Elly);
	CHECK(Looks.IsSet());
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x04});
	const std::array<uint8, 1> Saved = Looks.ToSaved();
	CHECK(FCharacterLooks::FromSaved(Saved) == Looks);
	CHECK(Looks.Switch(FCharacterLooks::Elly));
	CHECK_FALSE(Looks.IsSet());
}

TEST_CASE("Fei as Id is drawn and named as Id, his record his own, the look kept in the save's byte", "[Game]")
{
	FCharacterLooks Looks;
	CHECK_FALSE(Looks.IsId(Fei));
	CHECK(Looks.Switch(Fei));
	CHECK(Looks.IsId(Fei));
	CHECK(Looks.IsSet());
	CHECK(Looks.Drawn(Fei) == Fei);
	CHECK_FALSE(Looks.HasPurpleHair(Fei));
	for (uint8 Character = 1; Character <= 10; ++Character)
	{
		CHECK_FALSE(Looks.IsId(Character));
	}
	CHECK(Looks.PerformingGear(WeltallGear) == WeltallGear);
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x08});
	const std::array<uint8, 1> Saved = Looks.ToSaved();
	CHECK(FCharacterLooks::FromSaved(Saved) == Looks);
	CHECK(Looks.Switch(FCharacterLooks::Elly));
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x0C});
	CHECK(Looks.Switch(Fei));
	CHECK_FALSE(Looks.IsId(Fei));
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x04});
}

TEST_CASE("A save keeps both looks in one byte and reads back what it knows", "[Game]")
{
	FCharacterLooks Looks;
	Looks.Switch(SwordCitan);
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x01});
	Looks.Switch(GrownEmeralda);
	CHECK(Looks.ToSaved() == std::array<uint8, 1>{0x03});
	const std::array<uint8, 1> Saved = Looks.ToSaved();
	CHECK(FCharacterLooks::FromSaved(Saved) == Looks);
	const std::array<uint8, 1> Unknown{0xFE};
	const FCharacterLooks Read = FCharacterLooks::FromSaved(Unknown);
	CHECK(Read.Drawn(SwordCitan) == SwordCitan);
	CHECK(Read.Drawn(GrownEmeralda) == Emeralda);
	CHECK(Read.IsId(Fei));
}
