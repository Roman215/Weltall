#include "Foundation/ByteSource.h"
#include "Media/CdImage.h"
#include "Media/DiscRawPatch.h"
#include "Mods/ItalianPatch.h"

#include <catch_amalgamated.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	struct FFolder
	{
		explicit FFolder(const char* Name) : Path(std::filesystem::temp_directory_path() / Name)
		{
			std::filesystem::remove_all(Path);
			std::filesystem::create_directories(Path);
		}
		~FFolder() { std::filesystem::remove_all(Path); }

		std::filesystem::path Write(const std::filesystem::path& Name, const std::vector<uint8>& Bytes) const
		{
			std::filesystem::create_directories((Path / Name).parent_path());
			std::ofstream File(Path / Name, std::ios::binary | std::ios::trunc);
			File.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
			REQUIRE(File.good());
			return Path / Name;
		}

		std::filesystem::path Path;
	};

	std::vector<uint8> Text(std::string_view Words)
	{
		return std::vector<uint8>(Words.begin(), Words.end());
	}

	void PutU32(std::vector<uint8>& Bytes, uint32 Value)
	{
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Bytes.push_back(static_cast<uint8>(Value >> Shift));
		}
	}

	/** A patch of (record, bits, bytes) entries, laid out as the team's is. */
	std::vector<uint8> Patch(const std::vector<std::tuple<uint16, uint32, std::vector<uint8>>>& Entries)
	{
		std::vector<uint8> Bytes;
		PutU32(Bytes, static_cast<uint32>(Entries.size()));
		PutU32(Bytes, 1);
		PutU32(Bytes, 0);
		PutU32(Bytes, 0);
		uint32 Offset = static_cast<uint32>(16 + 16 * Entries.size());
		std::vector<uint8> Body;
		for (const auto& [Record, Bits, Data] : Entries)
		{
			Bytes.push_back(static_cast<uint8>(Record));
			Bytes.push_back(static_cast<uint8>(Record >> 8));
			PutU32(Bytes, Offset + static_cast<uint32>(Body.size()));
			PutU32(Bytes, static_cast<uint32>(Data.size()));
			PutU32(Bytes, Bits);
			Bytes.push_back(0);
			Bytes.push_back(0);
			Body.insert(Body.end(), Data.begin(), Data.end());
		}
		Bytes.insert(Bytes.end(), Body.begin(), Body.end());
		return Bytes;
	}

	/** A PPF 3.0 patch of (offset, bytes) records with Description, its block check and FILE_ID.DIZ as asked. */
	std::vector<uint8> Ppf(std::string_view Description, const std::vector<std::pair<uint64, std::vector<uint8>>>& Records,
		const std::vector<uint8>* Check = nullptr, std::string_view Diz = {})
	{
		std::vector<uint8> Bytes;
		Bytes.reserve(64 + 1024);
		const std::string_view Mark = "PPF30";
		Bytes.insert(Bytes.end(), Mark.begin(), Mark.end());
		Bytes.push_back(2);
		std::string Padded(Description);
		Padded.resize(50, ' ');
		Bytes.insert(Bytes.end(), Padded.begin(), Padded.end());
		Bytes.insert(Bytes.end(), {0, static_cast<uint8>(Check != nullptr), 0, 0});
		if (Check != nullptr)
		{
			Bytes.insert(Bytes.end(), Check->begin(), Check->end());
		}
		for (const auto& [Offset, Data] : Records)
		{
			PutU32(Bytes, static_cast<uint32>(Offset));
			PutU32(Bytes, static_cast<uint32>(Offset >> 32));
			Bytes.push_back(static_cast<uint8>(Data.size()));
			Bytes.insert(Bytes.end(), Data.begin(), Data.end());
		}
		if (!Diz.empty())
		{
			const std::vector<uint8> Begin = Text("@BEGIN_FILE_ID.DIZ");
			const std::vector<uint8> End = Text("@END_FILE_ID.DIZ");
			Bytes.insert(Bytes.end(), Begin.begin(), Begin.end());
			Bytes.insert(Bytes.end(), Diz.begin(), Diz.end());
			Bytes.insert(Bytes.end(), End.begin(), End.end());
			Bytes.push_back(static_cast<uint8>(Diz.size()));
			Bytes.push_back(static_cast<uint8>(Diz.size() >> 8));
		}
		return Bytes;
	}

	std::map<int32, std::string> Contents(const std::vector<FModFile>& Files)
	{
		std::map<int32, std::string> Read;
		for (const FModFile& File : Files)
		{
			std::ifstream Stream(File.Path, std::ios::binary);
			std::string Bytes(static_cast<std::size_t>(File.Size), '\0');
			Stream.seekg(static_cast<std::streamoff>(File.Offset));
			Stream.read(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
			Read[File.Record] = Bytes;
		}
		return Read;
	}

	constexpr uint32 Every = 0x3F;
}

TEST_CASE("The Italian translation's patch is found in a folder of Mods and gives each variant its files", "[Mods]")
{
	const FFolder Mods("WeltallItalianPatch");
	Mods.Write("A first/notes.xpt", Text("not the team's patch at all"));
	Mods.Write("Italian/itpch_xenogears_v121.xpt", Patch({
		{0, 0x1B, Text("Il numero originale di questo exe e 0017")},
		{22, 0x17, Text("disc 1's executable, US commands")},
		{22, 0x27, Text("disc 1's executable, Japanese commands")},
		{38, 0x1F, Text("battle, US commands")},
		{38, 0x2F, Text("battle, Japanese commands")},
		{3005, 0x3D, Text("a Japanese voice")},
		{616, Every, Text("a map")},
	}));
	Mods.Write("Italian/xenocd1_emovies_ita.ppf", Ppf("Xenogears: Traduzione filmati CD1", {}));
	Mods.Write("Italian/xenocd2_jmovies_ita.ppf", Ppf("Xenogears: Traduzione filmati + audio jap CD2", {}));
	Mods.Write("Italian/other.ppf", Ppf("Another game's patch", {}));

	const std::optional<FItalianPatch> Found = FItalianPatch::Find(Mods.Path);
	REQUIRE(Found);
	CHECK(Found->Path().filename() == "itpch_xenogears_v121.xpt");
	FItalianOptions Options;
	Options.bEnabled = true;
	CHECK(Contents(Found->Files(Options, 1)) == std::map<int32, std::string>{{38, "battle, US commands"}, {616, "a map"}});
	Options.bJapaneseVoices = true;
	Options.bJapaneseCommands = true;
	CHECK(Contents(Found->Files(Options, 1)) == std::map<int32, std::string>{{38, "battle, Japanese commands"}, {616, "a map"}, {3005, "a Japanese voice"}});
	const TResult<std::vector<uint8>> Executable = Found->Executable(Options, 1);
	REQUIRE(Executable);
	CHECK(*Executable == Text("disc 1's executable, Japanese commands"));
	// The patch's first disc's records, which the second disc's files are given by too; this one has no executable for it.
	CHECK(Contents(Found->Files(Options, 2)) == std::map<int32, std::string>{{38, "battle, Japanese commands"}, {616, "a map"}, {3005, "a Japanese voice"}});
	CHECK_FALSE(Found->Executable(Options, 2));
	// The movie patches by their descriptions, whatever their names.
	CHECK(Found->MoviePatch(1, false) == Mods.Path / "Italian/xenocd1_emovies_ita.ppf");
	CHECK(Found->MoviePatch(2, true) == Mods.Path / "Italian/xenocd2_jmovies_ita.ppf");
	CHECK_FALSE(Found->MoviePatch(1, true));
	CHECK_FALSE(Found->MoviePatch(2, false));
	CHECK_FALSE(FItalianPatch::Find(Mods.Path / "A first"));
}

TEST_CASE("A movie patch writes its records into the raw sectors it is given as an image reads them, across sectors, not into a 2048-byte image", "[Media][Mods]")
{
	const FFolder Folder("WeltallRawPatch");
	constexpr uint32 Raw = CdSector::RawSize;
	std::vector<uint8> Sectors(Raw * 4);
	for (std::size_t At = 0; At < Sectors.size(); ++At)
	{
		Sectors[At] = static_cast<uint8>(At * 7);
	}
	const std::vector<uint8> Across(6, 0xAA);
	const std::vector<uint8> Within{1, 2, 3};
	const std::filesystem::path Path = Folder.Write("movies.ppf",
		Ppf("Xenogears: Traduzione filmati CD1", {{Raw * 2 - 4, Across}, {Raw * 3 + 100, Within}}, nullptr, "SadNES cITY"));
	const TResult<std::shared_ptr<const FDiscRawPatch>> Patch = FDiscRawPatch::Open(Path);
	REQUIRE(Patch);
	CHECK((*Patch)->SectorCount() == 3);

	const FCdImage Image(std::make_shared<FMemoryByteSource>(Sectors), Raw, 0);
	REQUIRE((*Patch)->Fits(Image));
	Image.SetRawPatch(*Patch, 0, 4);
	std::vector<uint8> Expected = Sectors;
	std::copy(Across.begin(), Across.end(), Expected.begin() + Raw * 2 - 4);
	std::copy(Within.begin(), Within.end(), Expected.begin() + Raw * 3 + 100);
	for (uint32 Lba = 0; Lba < 4; ++Lba)
	{
		INFO("sector " << Lba);
		const TResult<std::vector<uint8>> Read = Image.ReadSourceSector(Lba);
		REQUIRE(Read);
		CHECK(*Read == std::vector<uint8>(Expected.begin() + Lba * Raw, Expected.begin() + (Lba + 1) * Raw));
	}
	// Outside the sectors it is given (a disc's movies), a sector is the disc's own.
	Image.SetRawPatch(*Patch, 0, 3);
	CHECK(*Image.ReadSourceSector(3) == std::vector<uint8>(Sectors.begin() + 3 * Raw, Sectors.end()));
	Image.UseDiscAlone();
	CHECK(*Image.ReadSourceSector(1) == std::vector<uint8>(Sectors.begin() + Raw, Sectors.begin() + 2 * Raw));

	// A 2048-byte image keeps its sectors as they are.
	const FCdImage Cooked(std::make_shared<FMemoryByteSource>(std::vector<uint8>(2048 * 4, 9)), 2048, 0);
	Cooked.SetRawPatch(*Patch, 0, 4);
	CHECK(*Cooked.ReadSourceSector(1) == std::vector<uint8>(2048, 9));
}

TEST_CASE("A movie patch with a block check fits only the image holding its bytes; other files are refused", "[Media][Mods]")
{
	const FFolder Folder("WeltallRawPatchCheck");
	constexpr uint32 Raw = CdSector::RawSize;
	std::vector<uint8> Sectors(Raw * 17, 0);
	const std::vector<uint8> Check(1024, 0x5A);
	const std::filesystem::path Path = Folder.Write("checked.ppf", Ppf("Xenogears: Traduzione filmati CD2", {{0, {1}}}, &Check));
	const TResult<std::shared_ptr<const FDiscRawPatch>> Patch = FDiscRawPatch::Open(Path);
	REQUIRE(Patch);
	CHECK_FALSE((*Patch)->Fits(FCdImage(std::make_shared<FMemoryByteSource>(Sectors), Raw, 0)));
	std::copy(Check.begin(), Check.end(), Sectors.begin() + Raw * 16 + 0x20);
	CHECK((*Patch)->Fits(FCdImage(std::make_shared<FMemoryByteSource>(Sectors), Raw, 0)));

	CHECK_FALSE(FDiscRawPatch::Open(Folder.Write("old.ppf", Text("PPF20 an older patch's header, long enough to be read whole here....."))));
	CHECK_FALSE(FDiscRawPatch::Open(Folder.Path / "missing.ppf"));
}
