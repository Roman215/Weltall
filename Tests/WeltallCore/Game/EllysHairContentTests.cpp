#include "Formats/Lzss.h"
#include "Formats/SectionContainer.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Game/EllysHair.h"
#include "Graphics/SpriteFile.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

using namespace Weltall;

// Elly's purple hair: her own pictures recoloured from the disc's.
namespace
{
	double Hue(uint16 Colour)
	{
		const double R = (Colour & 31) / 31.0;
		const double G = (Colour >> 5 & 31) / 31.0;
		const double B = (Colour >> 10 & 31) / 31.0;
		const double Max = std::max({R, G, B});
		const double Min = std::min({R, G, B});
		if (Max == Min)
		{
			return 0.0;
		}
		double H = Max == R ? (G - B) / (Max - Min) : Max == G ? 2.0 + (B - R) / (Max - Min) : 4.0 + (R - G) / (Max - Min);
		H *= 60.0;
		return H < 0.0 ? H + 360.0 : H;
	}

	double Saturation(uint16 Colour)
	{
		const double Max = std::max({Colour & 31, Colour >> 5 & 31, Colour >> 10 & 31}) / 31.0;
		const double Min = std::min({Colour & 31, Colour >> 5 & 31, Colour >> 10 & 31}) / 31.0;
		return Max > 0.0 ? (Max - Min) / Max : 0.0;
	}

	/** Two colours within Units of each other in every channel. */
	bool Near(uint16 A, uint16 B, int32 Units)
	{
		for (const int32 Shift : {0, 5, 10})
		{
			if (std::abs((A >> Shift & 31) - (B >> Shift & 31)) > Units)
			{
				return false;
			}
		}
		return true;
	}

	/** Her orange as the rule takes it: strongly coloured, between red and yellow. */
	bool Orange(uint16 Colour)
	{
		const double H = Hue(Colour);
		return (Colour & 0x7FFF) != 0 && H >= 7.2 && H <= 43.2 && Saturation(Colour) >= 0.55 && std::max({Colour & 31, Colour >> 5 & 31, Colour >> 10 & 31}) > 1;
	}

	/** The dull dark red-brown that shades her trousers, just short of the orange's hues. */
	bool Shadow(uint16 Colour)
	{
		const int32 Max = std::max({Colour & 31, Colour >> 5 & 31, Colour >> 10 & 31});
		return Hue(Colour) < 7.2 && Saturation(Colour) >= 0.45 && Saturation(Colour) < 0.75 && Max > 1 && Max * 2 < 31;
	}

	struct FDisc
	{
		const FIso9660* Iso = nullptr;
		FGameFileIndex Index;
	};

	std::vector<uint8> ReadFile(const FDisc& Disc, int32 Directory, int32 Number, bool bPacked)
	{
		const FIndexRecord* Record = Disc.Index.Find(Directory, Number);
		REQUIRE(Record != nullptr);
		auto Sectors = ReadFileSectors(Disc.Iso->Image(), *Record);
		REQUIRE(Sectors);
		if (!bPacked)
		{
			return *Sectors;
		}
		auto Unpacked = UnpackLzss(*Sectors);
		REQUIRE(Unpacked);
		return *Unpacked;
	}

	/** Every colour of a sprite file's palette block. */
	std::vector<uint16> Palettes(std::span<const uint8> File)
	{
		const uint32 At = LittleEndian::ReadU32(File, 12);
		const uint32 End = std::min<uint32>(LittleEndian::ReadU32(File, 16), static_cast<uint32>(File.size()));
		std::vector<uint16> Colours;
		for (uint32 Offset = At + 4; Offset + 2 <= End; Offset += 2)
		{
			Colours.push_back(LittleEndian::ReadU16(File, Offset));
		}
		return Colours;
	}
}

TEST_CASE("Elly's orange turns to the purple of Merkava's Elly; her skin, the red of her ribbon and black stay", "[Game]")
{
	// Her hair's shades in her field sprite ((131, 49, 24), (164, 65, 16), (222, 123, 32)), then her skin, her ribbon's red, black.
	for (const uint16 Orange : {uint16{0x0CD0}, uint16{0x0914}, uint16{0x11FB}})
	{
		const uint16 Purple = EllysHair::Purple(Orange);
		CAPTURE(Orange, Purple);
		CHECK(std::abs(Hue(Purple) - 255.0) < 6.0);
		CHECK((Purple & 31) < (Purple >> 10 & 31));
	}
	// Her trousers' darkest shade in battle, (104, 48, 48), with them; the dark red of her ribbon, (120, 8, 16), not.
	CHECK(std::abs(Hue(EllysHair::Purple(0x18CD)) - 255.0) < 6.0);
	CHECK(EllysHair::Purple(0x082F) == 0x082F);
	CHECK(EllysHair::Purple(0x5BBE) == 0x5BBE);
	CHECK(EllysHair::Purple(0x0C5E) == 0x0C5E);
	CHECK(EllysHair::Purple(0x0000) == 0x0000);
	CHECK(EllysHair::Purple(0x8000) == 0x8000);
	CHECK((EllysHair::Purple(0x8914) & 0x8000) != 0);
}

TEST_CASE("Elly's field, battle and attack sprites keep no orange in any palette, nor her trousers' red-brown shading, every other colour as it was", "[Game][Content]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FDisc Disc{Iso, std::move(*Index)};
	struct FFile
	{
		int32 Directory = 0;
		int32 Number = 0;
		bool bPacked = false;
		int32 Orange = 0;
	};
	// Her field sprite (4/6), her battle sprite (45/2) and her attack file (45/19): how many orange colours each holds.
	for (FFile File : {FFile{4, 6, true}, FFile{45, 2, false}, FFile{45, 19, false}})
	{
		CAPTURE(File.Directory, File.Number);
		std::vector<uint8> Bytes = ReadFile(Disc, File.Directory, File.Number, File.bPacked);
		const std::vector<uint16> Before = Palettes(Bytes);
		EllysHair::RecolourSpriteFile(Bytes);
		const std::vector<uint16> After = Palettes(Bytes);
		REQUIRE(After.size() == Before.size());
		int32 Changed = 0;
		for (std::size_t Colour = 0; Colour < Before.size(); ++Colour)
		{
			CHECK_FALSE(Orange(After[Colour]));
			CHECK_FALSE(Shadow(After[Colour]));
			if (!Orange(Before[Colour]) && !Shadow(Before[Colour]))
			{
				CHECK(After[Colour] == Before[Colour]);
			}
			Changed += After[Colour] != Before[Colour] ? 1 : 0;
		}
		CHECK(Changed > 8);
		CHECK(FSpriteFile::ParseWithSections(Bytes));
	}
}

TEST_CASE("Elly's large portrait has purple hair, the strands beside her cheek too, the shading of her skin under her jaw kept", "[Game][Content]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc2);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const FDisc Disc{Iso, std::move(*Index)};
	// The Menu's file 16/1: section 5 holds the eleven large portraits, 0xB20 bytes each, Elly's the second.
	const std::vector<uint8> Menu = ReadFile(Disc, 16, 1, false);
	auto Portraits = UnpackContainerSection(Menu, 5);
	REQUIRE(Portraits);
	constexpr std::size_t PortraitBytes = 0xB20;
	auto Read = ParseTim(std::span<const uint8>(*Portraits).subspan(PortraitBytes, PortraitBytes));
	REQUIRE(Read);
	const FTim Before = *Read;
	FTim After = *Read;
	EllysHair::RecolourTim(After);
	REQUIRE(After.PixelWidth() == 48);
	const auto ColourOf = [](const FTim& Tim, int32 X, int32 Y) { return Tim.Clut->Units[Tim.IndexAt(X, Y)]; };
	int32 Hair = 0;
	int32 Kept = 0;
	int32 Others = 0;
	int32 OthersKept = 0;
	for (int32 Y = 0; Y < 48; ++Y)
	{
		for (int32 X = 0; X < 48; ++X)
		{
			const uint16 Was = ColourOf(Before, X, Y);
			const uint16 Now = ColourOf(After, X, Y);
			if (Orange(Was))
			{
				// Below the cheeks a light pixel with skin just above it is the skin's shading: its colour stays. A dark one is a
				// strand of hair beside her cheek, purple.
				const bool bLight = std::max({Was & 31, Was >> 5 & 31, Was >> 10 & 31}) >= 21;
				const auto Skin = [&](int32 AboveY) { return AboveY >= 0 && !Orange(ColourOf(Before, X, AboveY)) && Saturation(ColourOf(Before, X, AboveY)) < 0.55
					&& std::max({ColourOf(Before, X, AboveY) & 31, ColourOf(Before, X, AboveY) >> 5 & 31, ColourOf(Before, X, AboveY) >> 10 & 31}) > 17; };
				if (Y >= 26 && bLight && (Skin(Y - 1) || Skin(Y - 2)))
				{
					// Its colour, or where its entry went purple with the hair, the nearest left (a few units off at most).
					++Kept;
					CHECK(Near(Now, Was, 3));
				}
				else
				{
					// Purple, the darkest (near black, whose hue is anyone's) given the nearest dark entry.
					++Hair;
					CHECK_FALSE(Orange(Now));
					if (std::max({Now & 31, Now >> 5 & 31, Now >> 10 & 31}) >= 6)
					{
						CHECK(std::abs(Hue(Now) - 255.0) < 25.0);
					}
				}
			}
			else
			{
				++Others;
				OthersKept += Now == Was ? 1 : 0;
			}
		}
	}
	CHECK(Hair > 1200);
	CHECK(Kept > 5);
	// The rest keeps its colours, but for the few pixels that shared an entry with the hair, given the nearest one left.
	CHECK(OthersKept * 100 >= Others * 95);
}
