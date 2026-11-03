#include "Game/CharacterLooks.h"

namespace Weltall
{
	namespace
	{
		constexpr uint8 CitanBit = 0x01;
		constexpr uint8 EmeraldaBit = 0x02;
		constexpr uint8 EllyBit = 0x04;
		constexpr uint8 IdBit = 0x08;
	}

	bool FCharacterLooks::Switch(uint8 Character)
	{
		if (Character == SwordCitan)
		{
			bCitanWithoutSword = !bCitanWithoutSword;
			return true;
		}
		if (Character == GrownEmeralda)
		{
			bChildEmeralda = !bChildEmeralda;
			return true;
		}
		if (Character == Elly)
		{
			bEllyPurpleHair = !bEllyPurpleHair;
			return true;
		}
		if (Character == Fei)
		{
			bFeiAsId = !bFeiAsId;
			return true;
		}
		return false;
	}

	uint8 FCharacterLooks::Drawn(uint8 Character) const
	{
		if (Character == SwordCitan && bCitanWithoutSword)
		{
			return Citan;
		}
		if (Character == GrownEmeralda && bChildEmeralda)
		{
			return Emeralda;
		}
		return Character;
	}

	uint8 FCharacterLooks::PerformingGear(uint8 Gear) const
	{
		return (Gear == SwordHeimdal || Gear == Fenrir) && bCitanWithoutSword ? Heimdal : Gear;
	}

	bool FCharacterLooks::WearsOwnModel(uint8 Gear) const
	{
		return Gear == Fenrir && bCitanWithoutSword;
	}

	std::array<uint8, FCharacterLooks::SavedBytes> FCharacterLooks::ToSaved() const
	{
		return {static_cast<uint8>((bCitanWithoutSword ? CitanBit : 0) | (bChildEmeralda ? EmeraldaBit : 0) | (bEllyPurpleHair ? EllyBit : 0) | (bFeiAsId ? IdBit : 0))};
	}

	FCharacterLooks FCharacterLooks::FromSaved(std::span<const uint8, SavedBytes> Bytes)
	{
		FCharacterLooks Looks;
		Looks.bCitanWithoutSword = (Bytes[0] & CitanBit) != 0;
		Looks.bChildEmeralda = (Bytes[0] & EmeraldaBit) != 0;
		Looks.bEllyPurpleHair = (Bytes[0] & EllyBit) != 0;
		Looks.bFeiAsId = (Bytes[0] & IdBit) != 0;
		return Looks;
	}
}
