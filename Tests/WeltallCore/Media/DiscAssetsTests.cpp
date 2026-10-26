#include "Media/AssetFiles.h"
#include "Media/DiscAssets.h"
#include "Sound/MusicReplacements.h"
#include "Support/AppleDouble.h"
#include "Support/ByteWriter.h"

#include <catch_amalgamated.hpp>

#include <cstring>
#include <fstream>

using namespace Weltall;

namespace
{
	struct FAssetFolder
	{
		explicit FAssetFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::filesystem::remove_all(Path);
		}
		~FAssetFolder() { std::filesystem::remove_all(Path); }
		std::filesystem::path Path;
	};

	FGameFileIndex AssetIndex(uint32 FileLba = 3)
	{
		Testing::FByteWriter Records;
		const auto Add = [&](uint32 Lba, int32 Size)
		{
			Records.U8(static_cast<uint8>(Lba)).U8(static_cast<uint8>(Lba >> 8)).U8(static_cast<uint8>(Lba >> 16)).U32(static_cast<uint32>(Size));
		};
		Add(1, -1);
		Add(1, 2 * 2336);
		Add(0, 0);
		Add(FileLba, 2049);
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

	FCdImage AssetImage()
	{
		std::vector<uint8> Bytes(6 * CdSector::RawSize, 0);
		for (uint32 Sector = 0; Sector < 6; ++Sector)
		{
			uint8* At = Bytes.data() + Sector * CdSector::RawSize;
			std::memset(At + 1, 0xFF, 10);
			At[15] = 2;
			At[16] = At[20] = 1;
			At[17] = At[21] = 3;
			At[18] = At[22] = Sector == 1 ? CdSector::SubmodeForm2 | CdSector::SubmodeAudio : CdSector::SubmodeData;
			std::memset(At + 24, static_cast<int>(Sector), Sector == 1 ? CdSector::Form2DataSize : CdSector::Form1DataSize);
		}
		return FCdImage(std::make_shared<FMemoryByteSource>(std::move(Bytes)), CdSector::RawSize, 0);
	}

	void Write(const std::filesystem::path& Path, const std::vector<uint8>& Bytes)
	{
		std::filesystem::create_directories(Path.parent_path());
		std::ofstream File(Path, std::ios::binary | std::ios::trunc);
		File.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
		REQUIRE(File.good());
	}

	void Finish(FDiscAssetExport& Job)
	{
		for (int Step = 0; Step < 100 && !Job.Status().bComplete; ++Step) REQUIRE(Job.Step(2048));
		REQUIRE(Job.Status().bComplete);
	}
}

TEST_CASE("Disc assets export every native byte and preserve movie XA sectors", "[Media][Assets]")
{
	FAssetFolder Folder("WeltallAssetsRoundtrip");
	const auto Image = AssetImage();
	const auto Index = AssetIndex();
	const auto Store = FDiscAssetStore::Open(Folder.Path, "usa.disc1", Image, Index);
	REQUIRE(Store);
	CHECK_FALSE(std::filesystem::exists(Folder.Path));
	const auto Job = (*Store)->Export();
	REQUIRE(Job);
	REQUIRE((*Job)->Step(1));
	CHECK((*Job)->Status().CompletedBytes == CdSector::RawSize);
	CHECK_FALSE((*Job)->Status().bComplete);
	Finish(**Job);
	CHECK((*Job)->Status().TotalFiles == 2);
	CHECK((*Job)->Status().CompletedFiles == 2);
	CHECK((*Job)->Status().CompletedBytes == 2 * CdSector::RawSize + 2 * CdSector::Form1DataSize);
	CHECK(std::filesystem::file_size(Folder.Path / "usa.disc1/movies/00.str") == 2 * CdSector::RawSize);
	CHECK((*Store)->AssetPath(*Index.Find(4, 1)) == Folder.Path / "usa.disc1/files/04/0001.bin");
	Image.SetAssetStore(*Store);
	(*Store)->SetEnabled(true);
	const auto Expected = AssetImage();
	CHECK(*ReadFileSectors(Image, *Index.Find(4, 1)) == *ReadFileSectors(Expected, *Index.Find(4, 1)));
	CHECK(*Image.ReadUserData(1) == *Expected.ReadUserData(1));
	CHECK(Image.ReadHeader(1)->IsForm2());
	CHECK(Image.ReadHeader(1)->Channel == 3);
}

TEST_CASE("External assets can be disabled and missing files fall back to disc", "[Media][Assets]")
{
	FAssetFolder Folder("WeltallAssetsFallback");
	const auto Image = AssetImage();
	const auto Index = AssetIndex();
	const auto Store = FDiscAssetStore::Open(Folder.Path, "usa.disc1", Image, Index);
	REQUIRE(Store);
	Image.SetAssetStore(*Store);
	const FIndexRecord& Record = *Index.Find(4, 1);
	const auto Path = (*Store)->AssetPath(Record);
	Write(Path, std::vector<uint8>(4096, 0xAA));
	CHECK((*ReadFileSectors(Image, Record))[0] == 3);
	(*Store)->SetEnabled(true);
	CHECK((*ReadFileSectors(Image, Record))[0] == 0xAA);
	CHECK((*ReadFileSectors(Image, Record))[4095] == 0xAA);
	Write(Path, std::vector<uint8>(6144, 0xBB));
	const auto Grown = ReadFileSectors(Image, Record);
	REQUIRE(Grown);
	CHECK(Grown->size() == 6144);
	CHECK((*Grown)[6143] == 0xBB);
	const auto MusicFolder = Folder.Path / "music";
	Write(MusicFolder / "theme-0123456789abcdef.wav", {1, 2, 3});
	Write(MusicFolder / "original" / "song-00-0000000000000042.wav", {4});
	// macOS's companion of the recording names its key too, and sorts before it.
	Testing::WriteAppleDoubleCompanion(MusicFolder / "theme-0123456789abcdef.wav");
	Write(MusicFolder / ".backup" / "song-01-0000000000000043.wav", {5});
	const auto Music = FMusicReplacements::Find(MusicFolder);
	REQUIRE(Music);
	CHECK((*Music)->Count() == 1);
	(*Store)->SetMusic(*Music);
	const auto Wave = Image.ReadMusicReplacement(0x0123456789ABCDEFull);
	REQUIRE(Wave);
	REQUIRE(*Wave);
	CHECK(**Wave == std::vector<uint8>{1, 2, 3});
	const auto Original = Image.ReadMusicReplacement(0x42);
	REQUIRE(Original);
	CHECK_FALSE(*Original);
	std::filesystem::remove(Path);
	CHECK((*ReadFileSectors(Image, Record))[0] == 3);
	Write(Path, {4});
	CHECK_FALSE(ReadFileSectors(Image, Record));
	(*Store)->SetEnabled(false);
	CHECK((*ReadFileSectors(Image, Record))[0] == 3);
}

TEST_CASE("Disc export resumes without overwriting mods or publishing partial files", "[Media][Assets]")
{
	FAssetFolder Folder("WeltallAssetsResume");
	const auto Image = AssetImage();
	const auto Index = AssetIndex();
	const auto Store = FDiscAssetStore::Open(Folder.Path, "usa.disc1", Image, Index);
	REQUIRE(Store);
	auto Job = (*Store)->Export();
	REQUIRE(Job);
	REQUIRE((*Job)->Step(1));
	CHECK_FALSE(std::filesystem::exists(Folder.Path / "usa.disc1/movies/00.str"));
	(*Job)->Cancel();
	CHECK((*Job)->Status().bCancelled);
	CHECK_FALSE(std::filesystem::exists(Folder.Path / "usa.disc1/movies/00.str.partial"));
	Write((*Store)->AssetPath(*Index.Find(4, 1)), std::vector<uint8>(4096, 0xAB));
	Job = (*Store)->Export();
	REQUIRE(Job);
	Finish(**Job);
	(*Store)->SetEnabled(true);
	Image.SetAssetStore(*Store);
	CHECK((*ReadFileSectors(Image, *Index.Find(4, 1)))[0] == 0xAB);
	CHECK_FALSE(FDiscAssetStore::Open(Folder.Path, "usa.disc1", Image, AssetIndex(4)));
	CHECK_FALSE(FDiscAssetStore::Open(Folder.Path, "../outside", Image, Index));
	const auto Published = PublishAssetFile(Folder.Path, "music/original/song-00.wav", std::vector<uint8>{1, 2, 3});
	REQUIRE(Published);
	CHECK(*Published);
	const auto Kept = PublishAssetFile(Folder.Path, "music/original/song-00.wav", std::vector<uint8>{4, 5});
	REQUIRE(Kept);
	CHECK_FALSE(*Kept);
	CHECK(std::filesystem::file_size(Folder.Path / "music/original/song-00.wav") == 3);
	CHECK_FALSE(PublishAssetFile(Folder.Path, "../outside.wav", std::vector<uint8>{1}));
}

TEST_CASE("Movie replacement headers and audio are read from the external stream", "[Media][Assets]")
{
	FAssetFolder Folder("WeltallAssetsMovies");
	const auto Image = AssetImage();
	const auto Index = AssetIndex();
	const auto Store = FDiscAssetStore::Open(Folder.Path, "usa.disc1", Image, Index);
	REQUIRE(Store);
	const auto Job = (*Store)->Export();
	REQUIRE(Job);
	Finish(**Job);
	std::vector<uint8> Movie;
	for (uint32 Lba = 1; Lba <= 2; ++Lba)
	{
		auto Sector = *Image.ReadSourceSector(Lba);
		Sector[17] = Sector[21] = 7;
		Sector[24] = 0xFE;
		Movie.insert(Movie.end(), Sector.begin(), Sector.end());
	}
	Write(Folder.Path / "usa.disc1/movies/00.str", Movie);
	Image.SetAssetStore(*Store);
	(*Store)->SetEnabled(true);
	CHECK(Image.ReadHeader(1)->Channel == 7);
	CHECK((*Image.ReadUserData(1))[0] == 0xFE);
	(*Store)->SetEnabled(false);
	CHECK(Image.ReadHeader(1)->Channel == 3);
	CHECK((*Image.ReadUserData(1))[0] == 1);
}
