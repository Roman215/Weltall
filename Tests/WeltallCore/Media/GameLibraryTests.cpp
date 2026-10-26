#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Support/AppleDouble.h"
#include "Support/ChdWriter.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

using namespace Weltall;

namespace
{
	using FChange = std::function<void(uint32, std::vector<uint8>&)>;

	/** One of a disc's sectors as a raw one, its user data and subheader as read, Change applied to the data. */
	std::vector<uint8> RawSector(const FIso9660& Disc, uint32 Lba, const FChange& Change = {})
	{
		const auto Header = Disc.Image().ReadHeader(Lba);
		auto Data = Disc.Image().ReadUserData(Lba);
		REQUIRE(Header);
		REQUIRE(Data);
		if (Change)
		{
			Change(Lba, *Data);
		}
		return Testing::RawSector(*Header, *Data);
	}

	/** A disc's first sectors as raw ones (RawSector). */
	std::vector<uint8> RawSectors(const FIso9660& Disc, uint32 Count, const FChange& Change = {})
	{
		std::vector<uint8> Bin;
		for (uint32 Lba = 0; Lba < Count; ++Lba)
		{
			const std::vector<uint8> Sector = RawSector(Disc, Lba, Change);
			Bin.insert(Bin.end(), Sector.begin(), Sector.end());
		}
		return Bin;
	}

	/** The sectors up to the end of the disc's directories and boot files, which tell which disc it is. */
	uint32 BootSectors(const FIso9660& Disc, std::string_view Executable)
	{
		uint32 Copied = 0;
		for (const FIsoEntry& Entry : Disc.Entries())
		{
			if (Entry.bDirectory || Entry.Path == "SYSTEM.CNF" || Entry.Path == Executable)
			{
				Copied = std::max(Copied, Entry.Lba + Entry.SectorCount());
			}
		}
		return Copied;
	}

	void WriteFile(const std::filesystem::path& Path, const std::vector<uint8>& Bytes)
	{
		std::ofstream(Path, std::ios::binary).write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
	}
}

TEST_CASE("The game library skips files that are not readable discs", "[Media]")
{
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallGameLibraryTest";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder / "Nested", Error);
	REQUIRE_FALSE(Error);
	std::ofstream(Folder / "Nested" / "junk.bin", std::ios::binary) << std::string(4096, 'x');
	std::ofstream(Folder / "missing.cue") << "FILE \"nowhere.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
	std::ofstream(Folder / "notes.txt") << "not a disc";
	std::ofstream(Folder / "eboot.PBP", std::ios::binary) << std::string(64, 'x');
	std::ofstream(Folder / "game.chd", std::ios::binary) << std::string(64, 'x');
	// Hidden files and folders are passed over unnoted: macOS's companions of copied files, a desktop's trash.
	Testing::WriteAppleDoubleCompanion(Folder / "missing.cue");
	Testing::WriteAppleDoubleCompanion(Folder / "Nested" / "junk.bin");
	std::filesystem::create_directories(Folder / ".Trash-1000" / "files", Error);
	std::ofstream(Folder / ".Trash-1000" / "files" / "old.iso", std::ios::binary) << std::string(4096, 'x');

	const FGameLibrary Library = FGameLibrary::Scan(Folder);
	CHECK(Library.Releases().empty());
	CHECK(Library.Find(EDiscRelease::UsaDisc1) == nullptr);
	// Each file it cannot use is noted, with why; a file that is no disc image of any kind is passed over.
	const auto NoteOf = [&Library](const std::filesystem::path& File)
	{
		const auto Found = std::find_if(Library.Notes().begin(), Library.Notes().end(), [&File](const FLibraryNote& Note) { return Note.File == File; });
		return Found == Library.Notes().end() ? std::string() : Found->Text;
	};
	CHECK(Library.Notes().size() == 4);
	CHECK(NoteOf(Folder / "Nested" / "junk.bin") == "Not a CD's image the game can read.");
	CHECK(NoteOf(Folder / "missing.cue") == "The file it names, nowhere.bin, is not beside it.");
	CHECK(NoteOf(Folder / "eboot.PBP").starts_with("A PSP eboot"));
	CHECK(NoteOf(Folder / "game.chd") == "Not a CD's image the game can read.");
	CHECK(NoteOf(Folder / "notes.txt").empty());
	CHECK(FGameLibrary::Scan(Folder / "absent").Releases().empty());
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("The game library names the discs it does not play and keeps the ones the game reads as it looks again", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	FGameLibrary Library = FGameLibrary::Scan(WELTALL_GAMES_DIR);
	const FIso9660* Found = Library.Find(EDiscRelease::UsaDisc1);
	REQUIRE(Found != nullptr);
	CHECK_FALSE(Library.FileOf(EDiscRelease::UsaDisc1).empty());
	// Whatever else lies beside the USA discs (other releases, packed copies) is noted with why; the discs played are not.
	for (const FLibraryNote& Note : Library.Notes())
	{
		INFO(Note.File.string() << ": " << Note.Text);
		CHECK_FALSE(Note.Text.empty());
		CHECK(Note.File != Library.FileOf(EDiscRelease::UsaDisc1));
		CHECK(Note.File != Library.FileOf(EDiscRelease::UsaDisc2));
	}
	Library.Adopt(FGameLibrary::Scan(WELTALL_GAMES_DIR));
	CHECK(Library.Find(EDiscRelease::UsaDisc1) == Found);
}

TEST_CASE("The game library passes over a disc whose image ends before its last file, as one still being copied does", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	// Disc 1 copied as far as its directories and program, which tell which disc it is, but not the files after them.
	const uint32 Copied = BootSectors(*Disc1, "SLUS_006.64");
	REQUIRE(Copied + 1000 < Disc1->Image().SectorCount());
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallGameLibraryCopying";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder, Error);
	const auto CheckPassedOver = [&Folder](const std::filesystem::path& File)
	{
		const FGameLibrary Library = FGameLibrary::Scan(Folder);
		CHECK(Library.Find(EDiscRelease::UsaDisc1) == nullptr);
		REQUIRE(Library.Notes().size() == 1);
		CHECK(Library.Notes()[0].File == File);
		CHECK(Library.Notes()[0].Text == "Ends too soon: still being copied, or a rip cut short.");
		CHECK_FALSE(Library.Notes()[0].bPlays);
		std::error_code Removed;
		std::filesystem::remove(File, Removed);
	};
	SECTION("Its data alone, ending where the copy stopped")
	{
		const std::filesystem::path Iso = Folder / "Xenogears (Disc 1).iso";
		{
			std::ofstream File(Iso, std::ios::binary);
			for (uint32 Lba = 0; Lba < Copied; ++Lba)
			{
				auto Data = Disc1->Image().ReadUserData(Lba);
				REQUIRE(Data);
				Data->resize(CdSector::Form1DataSize);
				File.write(reinterpret_cast<const char*>(Data->data()), static_cast<std::streamsize>(Data->size()));
			}
		}
		CheckPassedOver(Iso);
	}
	const auto RawCopied = [&Disc1, Copied]() { return RawSectors(*Disc1, Copied); };
	SECTION("Raw sectors in a file already the whole disc's size, zeros where the copy has not reached")
	{
		const std::filesystem::path Bin = Folder / "Xenogears (Disc 1).bin";
		WriteFile(Bin, RawCopied());
		std::filesystem::resize_file(Bin, static_cast<uint64>(Disc1->Image().SectorCount()) * CdSector::RawSize);
		CheckPassedOver(Bin);
	}
	SECTION("A CHD of the sectors copied so far: the disc is known through it, and passed over")
	{
		const std::filesystem::path Chd = Folder / "Xenogears (Disc 1).chd";
		WriteFile(Chd, Testing::MakePlainCdChd(RawCopied(), "MODE2_RAW"));
		CheckPassedOver(Chd);
	}
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("A patched copy of a disc, a fan translation's, plays in place of the original beside it", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	const FIsoEntry* Program = Disc1->Find("SLUS_006.64");
	REQUIRE(Program != nullptr);
	// The program's last byte changed, which ends the boot notice's packed picture a translation redraws; the rest of the
	// disc as long as the original, zeros where the copy did not write.
	const uint32 LastByte = Program->Size - 1;
	const std::vector<uint8> Patched = RawSectors(*Disc1, BootSectors(*Disc1, "SLUS_006.64"), [&Program, LastByte](uint32 Lba, std::vector<uint8>& Data)
	{
		if (Lba == Program->Lba + LastByte / CdSector::Form1DataSize)
		{
			Data[LastByte % CdSector::Form1DataSize] ^= 0xFF;
		}
	});
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / "WeltallGameLibraryPatched";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder, Error);
	const std::filesystem::path Original = std::filesystem::absolute(std::filesystem::path(WELTALL_GAMES_DIR) / "Xenogears (USA) (Disc 1).bin");
	REQUIRE(std::filesystem::exists(Original));
	const auto Sheet = [](const std::filesystem::path& Path, const std::filesystem::path& Bin)
	{
		std::ofstream(Path) << "FILE \"" << Bin.string() << "\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
	};
	const auto Check = [&Folder](const std::filesystem::path& Played, const std::filesystem::path& Replaced)
	{
		const FGameLibrary Library = FGameLibrary::Scan(Folder);
		REQUIRE(Library.Find(EDiscRelease::UsaDisc1) != nullptr);
		CHECK(Library.FileOf(EDiscRelease::UsaDisc1) == Played);
		const auto NoteOf = [&Library](const std::filesystem::path& File)
		{
			const auto Found = std::find_if(Library.Notes().begin(), Library.Notes().end(), [&File](const FLibraryNote& Note) { return Note.File == File; });
			return Found == Library.Notes().end() ? FLibraryNote{} : *Found;
		};
		CHECK(NoteOf(Played).Text == "A patched copy of disc 1 (a fan translation, for example). It plays in place of the original.");
		CHECK(NoteOf(Played).bPlays);
		CHECK(NoteOf(Replaced).Text == "Not played: a patched copy of this disc is in the folder and plays instead.");
	};
	const std::filesystem::path PatchedBin = Folder / "patched.bin";
	WriteFile(PatchedBin, Patched);
	std::filesystem::resize_file(PatchedBin, Disc1->Image().SectorCount() * uint64{CdSector::RawSize});
	{
		// The sector the game's last file ends in, copied, so the image reads as whole rather than still being copied.
		const auto Index = FGameFileIndex::Read(*ReadBootExecutable(*Disc1));
		REQUIRE(Index);
		uint32 End = 0;
		for (const FIndexRecord& Record : Index->Records())
		{
			if (Record.IsFile())
			{
				End = std::max(End, Record.Lba + (static_cast<uint32>(Record.Size) + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize);
			}
		}
		const std::vector<uint8> Last = RawSector(*Disc1, End - 1);
		std::fstream File(PatchedBin, std::ios::binary | std::ios::in | std::ios::out);
		File.seekp(static_cast<std::streamoff>(End - 1) * CdSector::RawSize);
		File.write(reinterpret_cast<const char*>(Last.data()), static_cast<std::streamsize>(Last.size()));
	}
	SECTION("The original found first")
	{
		Sheet(Folder / "a-original.cue", Original);
		Sheet(Folder / "b-patched.cue", PatchedBin);
		Check(Folder / "b-patched.cue", Folder / "a-original.cue");
	}
	SECTION("The patched copy found first")
	{
		Sheet(Folder / "a-patched.cue", PatchedBin);
		Sheet(Folder / "b-original.cue", Original);
		Check(Folder / "a-patched.cue", Folder / "b-original.cue");
	}
	std::filesystem::remove_all(Folder, Error);
}

TEST_CASE("A disc's BIN found without its CUE is the disc, its movies' sound kept", "[Media][Content]")
{
	REQUIRE_DISC(Disc1, EDiscRelease::UsaDisc1);
	const FGameLibrary Games = FGameLibrary::Scan(WELTALL_GAMES_DIR);
	const std::filesystem::path Bin = std::filesystem::path(Games.FileOf(EDiscRelease::UsaDisc1)).replace_extension(".bin");
	REQUIRE(std::filesystem::is_regular_file(Bin));
	// A folder beside Games holding the BIN alone (linked, on the same drive), as a player who picked only it would have.
	const std::filesystem::path Folder = std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "out" / "tests" / "bin-without-cue";
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
	std::filesystem::create_directories(Folder);
	std::filesystem::create_hard_link(Bin, Folder / Bin.filename(), Error);
	REQUIRE_FALSE(Error);
	const FGameLibrary Alone = FGameLibrary::Scan(Folder);
	const FIso9660* Found = Alone.Find(EDiscRelease::UsaDisc1);
	REQUIRE(Found != nullptr);
	CHECK(Found->Image().IsRaw());
	CHECK(Alone.PlayableDisc(1) == Found);
	CHECK(Alone.Notes().empty());
	std::filesystem::remove_all(Folder, Error);
}
