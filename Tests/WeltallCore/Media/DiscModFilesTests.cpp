#include "Foundation/ByteSource.h"
#include "Foundation/LittleEndian.h"
#include "Media/CdImage.h"
#include "Media/DiscModFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <cstring>
#include <fstream>

using namespace Weltall;

namespace
{
	struct FModFolder
	{
		explicit FModFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::filesystem::remove_all(Path);
			std::filesystem::create_directories(Path);
		}
		~FModFolder() { std::filesystem::remove_all(Path); }

		std::filesystem::path Write(const char* Name, const std::vector<uint8>& Bytes) const
		{
			std::ofstream File(Path / Name, std::ios::binary | std::ios::trunc);
			File.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
			REQUIRE(File.good());
			return Path / Name;
		}

		std::filesystem::path Path;
	};

	FModFile ModFile(int32 Record, const std::filesystem::path& Path, const std::filesystem::path& Section5 = {})
	{
		FModFile File;
		File.Record = Record;
		File.Path = Path;
		File.Section5 = Section5;
		return File;
	}

	/** Records 3 (sectors 3 and 4, 2049 bytes) and 8 (sector 5, 100 bytes) are files; the rest are empty or a group. */
	FGameFileIndex ModIndex()
	{
		Testing::FByteWriter Records;
		const auto Add = [&](uint32 Lba, int32 Size)
		{
			Records.U8(static_cast<uint8>(Lba)).U8(static_cast<uint8>(Lba >> 8)).U8(static_cast<uint8>(Lba >> 16)).U32(static_cast<uint32>(Size));
		};
		Add(1, -1);
		Add(1, 2 * 2336);
		Add(0, 0);
		Add(3, 2049);
		for (int32 Empty = 4; Empty < 8; ++Empty)
		{
			Add(0, 0);
		}
		Add(5, 100);
		Add(0xFFFFFF, 0);
		std::vector<uint8> File(FPsxExecutable::HeaderSize + 0x8004 + FGameFileIndex::DirectoryCount * 2, 0);
		std::memcpy(File.data(), "PS-X EXE", 8);
		const uint32 Fields[] = {0x80010000, 0, 0x80010000, static_cast<uint32>(File.size() - FPsxExecutable::HeaderSize)};
		std::memcpy(File.data() + 0x10, Fields, sizeof(Fields));
		std::memcpy(File.data() + FPsxExecutable::HeaderSize + 4, Records.Bytes.data(), Records.Size());
		File[FPsxExecutable::HeaderSize + 0x8004 + 25 * 2] = 1;
		File[FPsxExecutable::HeaderSize + 0x8004 + 4 * 2] = 4;
		return *FGameFileIndex::Read(*FPsxExecutable::Parse(std::move(File)));
	}

	/** Six cooked sectors, each filled with its number. */
	FCdImage ModImage()
	{
		std::vector<uint8> Bytes(6 * CdSector::Form1DataSize, 0);
		for (uint32 Sector = 0; Sector < 6; ++Sector)
		{
			std::memset(Bytes.data() + Sector * CdSector::Form1DataSize, static_cast<int>(Sector), CdSector::Form1DataSize);
		}
		return FCdImage(std::make_shared<FMemoryByteSource>(std::move(Bytes)), CdSector::Form1DataSize, 0);
	}

	std::vector<uint8> Counting(std::size_t Size)
	{
		std::vector<uint8> Bytes(Size);
		for (std::size_t At = 0; At < Size; ++At)
		{
			Bytes[At] = static_cast<uint8>(At * 7 + 1);
		}
		return Bytes;
	}
}

TEST_CASE("A mod's file stands for the disc's with its own size, padded to whole sectors for the game's reads", "[Media][Mods]")
{
	const FModFolder Folder("WeltallModFiles");
	const FCdImage Image = ModImage();
	const FGameFileIndex Index = ModIndex();
	const std::vector<uint8> Longer = Counting(3000);
	const FModFile Files[] = {ModFile(3, Folder.Write("0003", Longer)), ModFile(6, Folder.Write("0006", Counting(10))), ModFile(40, Folder.Write("0040", Counting(10)))};
	const std::shared_ptr<const FDiscModFiles> Mods = FDiscModFiles::Make(Index, 1, Files);
	// Record 6 holds no file and 40 is past the index: only record 3's is kept.
	CHECK(Mods->Count() == 1);
	Image.SetModFiles(Mods);

	const FIndexRecord& Third = Index.Records()[3];
	const auto Sectors = ReadFileSectors(Image, Third);
	REQUIRE(Sectors);
	CHECK(Sectors->size() == 2 * CdSector::Form1DataSize);
	CHECK(std::equal(Longer.begin(), Longer.end(), Sectors->begin()));
	CHECK(std::all_of(Sectors->begin() + 3000, Sectors->end(), [](uint8 Byte) { return Byte == 0; }));
	const auto Bytes = ReadFileBytes(Image, Third);
	REQUIRE(Bytes);
	CHECK(*Bytes == Longer);

	// A file the mod does not have is the disc's, to its record's size.
	const auto Disc = ReadFileBytes(Image, Index.Records()[8]);
	REQUIRE(Disc);
	CHECK(*Disc == std::vector<uint8>(100, 5));

	// The exports' copy reads the disc alone.
	const FCdImage Alone = Image;
	Alone.UseDiscAlone();
	const auto Original = ReadFileBytes(Alone, Third);
	REQUIRE(Original);
	CHECK(Original->size() == 2049);
	CHECK(Original->front() == 3);
	CHECK(ReadFileBytes(Image, Third)->size() == 3000);
}

TEST_CASE("The second disc's records are the first's five lower", "[Media][Mods]")
{
	const FModFolder Folder("WeltallModFilesSecondDisc");
	const FCdImage Image = ModImage();
	const FGameFileIndex Index = ModIndex();
	const FModFile Files[] = {ModFile(8, Folder.Write("0008", Counting(64))), ModFile(3, Folder.Write("0003", Counting(64)))};
	Image.SetModFiles(FDiscModFiles::Make(Index, 2, Files));
	// The first disc's record 8 is the second's record 3; the first's record 3 has none there.
	CHECK(*ReadFileBytes(Image, Index.Records()[3]) == Counting(64));
	CHECK(*ReadFileBytes(Image, Index.Records()[8]) == std::vector<uint8>(100, 5));
}

TEST_CASE("A map's scripts are spliced in at their own place, cut or ended with zeros to fit", "[Media][Mods]")
{
	const FModFolder Folder("WeltallModFilesScripts");
	const FCdImage Image = ModImage();
	const FGameFileIndex Index = ModIndex();
	// A map whose header puts section 5 at 160h and section 6 at 170h.
	std::vector<uint8> Map = Counting(0x200);
	LittleEndian::WriteU32(Map, 0x51 * 4, 0x160);
	LittleEndian::WriteU32(Map, 0x52 * 4, 0x170);
	const std::filesystem::path MapPath = Folder.Write("0003", Map);

	const std::vector<uint8> Short(8, 0xAB);
	Image.SetModFiles(FDiscModFiles::Make(Index, 1, std::vector<FModFile>{ModFile(3, MapPath, Folder.Write("0003_5", Short))}));
	const auto Spliced = ReadFileBytes(Image, Index.Records()[3]);
	REQUIRE(Spliced);
	REQUIRE(Spliced->size() == Map.size());
	CHECK(std::equal(Map.begin(), Map.begin() + 0x160, Spliced->begin()));
	CHECK(std::all_of(Spliced->begin() + 0x160, Spliced->begin() + 0x168, [](uint8 Byte) { return Byte == 0xAB; }));
	CHECK(std::all_of(Spliced->begin() + 0x168, Spliced->begin() + 0x170, [](uint8 Byte) { return Byte == 0; }));
	CHECK(std::equal(Map.begin() + 0x170, Map.end(), Spliced->begin() + 0x170));

	const std::vector<uint8> Long(0x30, 0xCD);
	Image.SetModFiles(FDiscModFiles::Make(Index, 1, std::vector<FModFile>{ModFile(3, MapPath, Folder.Write("0003_5", Long))}));
	const auto Cut = ReadFileBytes(Image, Index.Records()[3]);
	REQUIRE(Cut);
	CHECK(std::all_of(Cut->begin() + 0x160, Cut->begin() + 0x170, [](uint8 Byte) { return Byte == 0xCD; }));
	CHECK(std::equal(Map.begin() + 0x170, Map.end(), Cut->begin() + 0x170));

	// A missing file is an error, not the disc's file in silence.
	Image.SetModFiles(FDiscModFiles::Make(Index, 1, std::vector<FModFile>{ModFile(3, Folder.Path / "missing")}));
	CHECK_FALSE(ReadFileBytes(Image, Index.Records()[3]));
}
