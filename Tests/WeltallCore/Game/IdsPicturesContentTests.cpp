#include "Formats/Tim.h"
#include "Game/IdsPictures.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <cmath>
#include <set>
#include <span>
#include <vector>

using namespace Weltall;

// Id's portraits for Fei's look as him, made from his dialogue face.
namespace
{
	std::vector<uint8> FaceOf(const FIso9660& Disc)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		TResult<std::vector<uint8>> Face = IdsPictures::ReadFace(Disc.Image(), *Index);
		REQUIRE(Face);
		return std::move(*Face);
	}

	/** How far a picture's colours are, on average, from the face's averaged over the same squares, mirrored or not. */
	double Distance(const FTim& Picture, const FTim& Face, int32 Inside, int32 Border, int32 Left, int32 Top, int32 Side, bool bMirrored)
	{
		double Total = 0.0;
		for (int32 Y = 0; Y < Inside; ++Y)
		{
			for (int32 X = 0; X < Inside; ++X)
			{
				const uint16 Shown = Picture.Clut->Units[Picture.IndexAt(X + Border, Y + Border)];
				double Sum = 0.0;
				int32 Count = 0;
				for (int32 FaceY = Top + Y * Side / Inside; FaceY < Top + (Y + 1) * Side / Inside; ++FaceY)
				{
					for (int32 FaceX = Left + X * Side / Inside; FaceX < Left + (X + 1) * Side / Inside; ++FaceX)
					{
						const uint16 Colour = Face.Clut->Units[Face.IndexAt(bMirrored ? 63 - FaceX : FaceX, FaceY)];
						Sum += (Colour & 31) + (Colour >> 5 & 31) + (Colour >> 10 & 31);
						++Count;
					}
				}
				Total += std::abs(Sum / Count - ((Shown & 31) + (Shown >> 5 & 31) + (Shown >> 10 & 31)));
			}
		}
		return Total / (Inside * Inside);
	}
}

TEST_CASE("Id's dialogue face is the same on both discs, and his portraits are made from it mirrored, in its own colours", "[Game][Content]")
{
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const std::vector<uint8> FaceBytes = FaceOf(*Second);
	CHECK(FaceOf(*First) == FaceBytes);
	const TResult<FTim> Face = ParseTim(FaceBytes);
	REQUIRE(Face);
	REQUIRE(Face->PixelWidth() == 64);
	std::set<uint16> FaceColours;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			FaceColours.insert(static_cast<uint16>(Face->Clut->Units[Face->IndexAt(X, Y)] | 0x8000));
		}
	}

	const TResult<FTim> Portrait = IdsPictures::Portrait(FaceBytes);
	REQUIRE(Portrait);
	CHECK(Portrait->BitsPerPixel == 8);
	CHECK(Portrait->PixelWidth() == 48);
	CHECK(Portrait->Image.Rect.Height == 48);
	REQUIRE(Portrait->Clut);
	CHECK(Portrait->Clut->Units.size() == 256);
	for (int32 Y = 0; Y < 48; ++Y)
	{
		for (int32 X = 0; X < 48; ++X)
		{
			// The party's portraits' colours all carry the semi-transparency bit; none of his is see-through.
			CHECK(FaceColours.contains(Portrait->Clut->Units[Portrait->IndexAt(X, Y)]));
		}
	}
	// The whole face, turned the party's portraits' way.
	const double Mirrored = Distance(*Portrait, *Face, 48, 0, 0, 0, 64, true);
	CHECK(Mirrored < 6.0);
	CHECK(Mirrored * 3 < Distance(*Portrait, *Face, 48, 0, 0, 0, 64, false));

	const TResult<FTim> SmallFace = IdsPictures::SmallFace(FaceBytes);
	REQUIRE(SmallFace);
	CHECK(SmallFace->PixelWidth() == 24);
	CHECK(SmallFace->Image.Rect.Height == 24);
	const uint8 Line = SmallFace->IndexAt(0, 0);
	CHECK(SmallFace->Clut->Units[Line] == 0x8000);
	for (int32 Y = 0; Y < 24; ++Y)
	{
		for (int32 X = 0; X < 24; ++X)
		{
			const bool bEdge = X == 0 || Y == 0 || X == 23 || Y == 23;
			const uint8 Index = SmallFace->IndexAt(X, Y);
			CHECK((Index == Line) == bEdge);
			CHECK((bEdge || FaceColours.contains(SmallFace->Clut->Units[Index])));
		}
	}
	// His head: 40 of the mirrored face's pixels square from (10, 4), inside the line.
	CHECK(Distance(*SmallFace, *Face, 22, 1, 10, 4, 40, true) < 6.0);
}

TEST_CASE("Id's name is typed as two of the font's letters", "[Game]")
{
	const std::span<const uint8> Name = IdsPictures::Name();
	REQUIRE(Name.size() == 3);
	CHECK(Name[0] == MesCodeOfCharacter('I')->Code);
	CHECK(Name[1] == MesCodeOfCharacter('d')->Code);
	CHECK(Name[2] == 0);
}

TEST_CASE("A file that is no 64 x 64 picture of 256 colours makes no portrait of Id", "[Game]")
{
	CHECK_FALSE(IdsPictures::Portrait({}));
	const std::vector<uint8> NotATim(0x1220, 0);
	CHECK_FALSE(IdsPictures::SmallFace(NotATim));
}
