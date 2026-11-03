#include "Foundation/Crc32.h"
#include "Graphics/Vram.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <vector>

using namespace Weltall;

namespace
{
	FVramImage Image(int16 X, int16 Y, int16 Width, int16 Height, uint16 First)
	{
		FVramImage Made{{X, Y, Width, Height}, {}};
		for (int32 Unit = 0; Unit < Width * Height; ++Unit)
		{
			Made.Units.push_back(static_cast<uint16>(First + Unit));
		}
		return Made;
	}

	const FVramUploadPart* PartAt(const FVram& Vram, const FVramRect& Rect)
	{
		const auto Parts = Vram.UploadParts();
		const auto Found = std::find_if(Parts.begin(), Parts.end(), [&Rect](const FVramUploadPart& Part) { return Part.Rect == Rect; });
		return Found != Parts.end() ? &*Found : nullptr;
	}
}

TEST_CASE("An upload is named by its units' CRC-32 and the indices they name", "[Graphics]")
{
	FVram Vram;
	FVramImage Upload = Image(64, 32, 4, 2, 0);
	Upload.Units = {0x1234, 0x5678, 0x9ABC, 0x0DEF, 0x2222, 0x3333, 0x4444, 0x5555};
	Vram.Upload(Upload);
	REQUIRE(Vram.UploadParts().size() == 1);
	const FVramUploadPart& Part = Vram.UploadParts()[0];
	CHECK(Part.Rect == FVramRect{64, 32, 4, 2});
	CHECK(Part.FromX == 0);
	CHECK(Part.FromY == 0);
	CHECK(Part.Upload->Crc == Crc32OfUnits(Upload.Units));
	CHECK(Part.Upload->Width == 4);
	CHECK(Part.Upload->Height == 2);
	CHECK(Part.Upload->Least4 == 0);
	CHECK(Part.Upload->Greatest4 == 15);
	CHECK(Part.Upload->Least8 == 0x0D);
	CHECK(Part.Upload->Greatest8 == 0xEF);
}

TEST_CASE("An upload over part of another cuts it to what is left, each piece keeping its place in its upload", "[Graphics]")
{
	FVram Vram;
	Vram.Upload(Image(100, 100, 40, 30, 0));
	const uint32 First = Vram.UploadParts()[0].Upload->Crc;
	Vram.Upload(Image(110, 110, 10, 5, 9000));
	REQUIRE(Vram.UploadParts().size() == 5);
	const FVramUploadPart* Above = PartAt(Vram, {100, 100, 40, 10});
	const FVramUploadPart* Below = PartAt(Vram, {100, 115, 40, 15});
	const FVramUploadPart* Left = PartAt(Vram, {100, 110, 10, 5});
	const FVramUploadPart* Right = PartAt(Vram, {120, 110, 20, 5});
	REQUIRE(Above != nullptr);
	REQUIRE(Below != nullptr);
	REQUIRE(Left != nullptr);
	REQUIRE(Right != nullptr);
	for (const FVramUploadPart* Piece : {Above, Below, Left, Right})
	{
		CHECK(Piece->Upload->Crc == First);
	}
	CHECK((Below->FromX == 0 && Below->FromY == 15));
	CHECK((Right->FromX == 20 && Right->FromY == 10));
	CHECK(PartAt(Vram, {110, 110, 10, 5})->Upload->Crc != First);

	// Covered whole, an upload leaves nothing of the earlier one.
	Vram.Upload(Image(90, 90, 100, 100, 1));
	CHECK(Vram.UploadParts().size() == 1);
}

TEST_CASE("A copy carries the uploads under its source to its destination; a fill and a whole-VRAM upload leave none", "[Graphics]")
{
	FVram Vram;
	Vram.Upload(Image(0, 0, 16, 16, 0));
	const uint32 Crc = Vram.UploadParts()[0].Upload->Crc;
	FVramImage Copied = Image(500, 300, 8, 8, 0);
	for (int32 Row = 0; Row < 8; ++Row)
	{
		for (int32 Column = 0; Column < 8; ++Column)
		{
			Copied.Units[static_cast<std::size_t>(Row * 8 + Column)] = Vram.At(4 + Column, 4 + Row);
		}
	}
	Vram.UploadCopy(Copied, {4, 4, 8, 8});
	const FVramUploadPart* Moved = PartAt(Vram, {500, 300, 8, 8});
	REQUIRE(Moved != nullptr);
	CHECK(Moved->Upload->Crc == Crc);
	CHECK((Moved->FromX == 4 && Moved->FromY == 4));
	// The source is left as it was.
	CHECK(PartAt(Vram, {0, 0, 16, 16}) != nullptr);

	FVramImage Cleared = Image(500, 300, 4, 8, 0);
	std::fill(Cleared.Units.begin(), Cleared.Units.end(), 0);
	Vram.UploadFill(Cleared);
	CHECK(PartAt(Vram, {500, 300, 8, 8}) == nullptr);
	CHECK(PartAt(Vram, {504, 300, 4, 8}) != nullptr);

	Vram.Upload(Image(0, 0, 1024, 512, 0));
	CHECK(Vram.UploadParts().empty());
}

TEST_CASE("Units read back and uploaded again to the same place bring back the uploads they were read from", "[Graphics]")
{
	FVram Vram;
	Vram.Upload(Image(0, 0, 8, 4, 0));
	Vram.Upload(Image(8, 0, 8, 4, 100));
	const FVramRect Read{4, 0, 8, 4};
	FVramImage Kept{Read, {}};
	for (int32 Row = 0; Row < Read.Height; ++Row)
	{
		for (int32 Column = 0; Column < Read.Width; ++Column)
		{
			Kept.Units.push_back(Vram.At(Read.X + Column, Read.Y + Row));
		}
	}
	Vram.NoteReadback(Read);
	Vram.Upload(Image(0, 0, 16, 4, 5000));
	REQUIRE(Vram.UploadParts().size() == 1);
	Vram.Upload(Kept);
	// The two halves come back as the pieces they were, around them what the later upload left.
	const FVramUploadPart* Left = PartAt(Vram, {4, 0, 4, 4});
	const FVramUploadPart* Right = PartAt(Vram, {8, 0, 4, 4});
	REQUIRE(Left != nullptr);
	REQUIRE(Right != nullptr);
	CHECK((Left->FromX == 4 && Left->Upload->Width == 8));
	CHECK((Right->FromX == 0 && Right->Upload->Width == 8));
	CHECK(PartAt(Vram, {0, 0, 4, 4}) != nullptr);
	CHECK(PartAt(Vram, {12, 0, 4, 4}) != nullptr);

	// Other units uploaded there are an upload of their own.
	Kept.Units[0] ^= 1;
	Vram.Upload(Kept);
	CHECK(PartAt(Vram, Read) != nullptr);
}

TEST_CASE("Upload parts lie where the GPU wraps their rectangles' corners", "[Graphics]")
{
	FVram Vram;
	Vram.Upload(Image(1024 + 1000, 512 + 10, 4, 4, 0));
	REQUIRE(Vram.UploadParts().size() == 1);
	CHECK(Vram.UploadParts()[0].Rect == FVramRect{1000, 10, 4, 4});
}

TEST_CASE("An upload keeps the kind its image names, else the kind the VRAM uploads under", "[Graphics]")
{
	FVram Vram;
	Vram.SetUploadKind(EVramUploadKind::Scenery);
	Vram.Upload(Image(0, 0, 4, 1, 0));
	{
		const FVram::FKindScope TypedKind(Vram, EVramUploadKind::Text);
		Vram.Upload(Image(16, 0, 4, 1, 100));
		FVramImage Face = Image(32, 0, 4, 1, 200);
		Face.Kind = EVramUploadKind::Portraits;
		Vram.Upload(Face);
	}
	Vram.Upload(Image(48, 0, 4, 1, 300));
	CHECK(PartAt(Vram, {0, 0, 4, 1})->Upload->Kind == EVramUploadKind::Scenery);
	CHECK(PartAt(Vram, {16, 0, 4, 1})->Upload->Kind == EVramUploadKind::Text);
	CHECK(PartAt(Vram, {32, 0, 4, 1})->Upload->Kind == EVramUploadKind::Portraits);
	CHECK(PartAt(Vram, {48, 0, 4, 1})->Upload->Kind == EVramUploadKind::Scenery);

	// Units read back and put back bring their uploads' own kinds, whatever the VRAM uploads under then.
	const FVramRect Kept{0, 0, 64, 1};
	FVramImage Read{Kept, std::vector<uint16>(64)};
	for (int32 X = 0; X < 64; ++X)
	{
		Read.Units[static_cast<std::size_t>(X)] = Vram.At(X, 0);
	}
	Vram.NoteReadback(Kept);
	Vram.Upload(Image(0, 0, 64, 1, 1000));
	const FVram::FKindScope OtherKind(Vram, EVramUploadKind::ModelsAndEffects);
	Vram.Upload(Read);
	CHECK(PartAt(Vram, {16, 0, 4, 1})->Upload->Kind == EVramUploadKind::Text);
	CHECK(PartAt(Vram, {32, 0, 4, 1})->Upload->Kind == EVramUploadKind::Portraits);
}
