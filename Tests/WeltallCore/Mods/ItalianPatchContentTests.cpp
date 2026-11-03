#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/PsxExecutable.h"
#include "Mods/GameMods.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <utility>

using namespace Weltall;

// The Italian translation's patch read from Mods: the team's patch and movie patches in a folder of the Mods folder
// WELTALL_ITALIAN_PATCH names, laid over the USA discs of Games, against the images `dev italian images` makes of the
// same variants (WELTALL_ITALIAN_GAMES, WELTALL_ITALIAN_JP_GAMES): laid out otherwise, their files the same.

namespace
{
	/** A movie each disc's patch re-encodes: the first disc's movie 2, the second's movie 8. */
	constexpr std::pair<int32, int32> PatchedMovieRecords{3, 9};
	constexpr uint32 MovieSectorBytes = 2336;

	/** The patch laid over the Games folder's discs while it lives, the discs' own again after. */
	struct FPatchApplied
	{
		FPatchApplied(const FFoundMods& Mods, const FItalianOptions& Options)
		{
			REQUIRE_FALSE(ApplyMods(Testing::GamesLibrary(), Mods, {}, Options));
		}
		~FPatchApplied() { ApplyMods(Testing::GamesLibrary(), {}, {}); }
		FPatchApplied(const FPatchApplied&) = delete;
		FPatchApplied& operator=(const FPatchApplied&) = delete;
	};

	/** The executable's bytes with its file index zeroed: what the variant's and the image's share. */
	std::vector<uint8> WithoutIndex(const FPsxExecutable& Executable)
	{
		std::vector<uint8> Bytes(Executable.Body().begin(), Executable.Body().end());
		const std::size_t At = FGameFileIndex::IndexAddress - Executable.LoadAddress();
		std::fill(Bytes.begin() + static_cast<std::ptrdiff_t>(At), Bytes.begin() + static_cast<std::ptrdiff_t>(At + FGameFileIndex::IndexBytes), uint8{0});
		return Bytes;
	}

	void CheckAgainstImage(const FFoundMods& Mods, const FItalianOptions& Options, EDiscRelease Release, const FIso9660& Patched)
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			WARN("disc not found in Games/");
			return;
		}
		const int32 Number = DiscNumberOfRelease(Release);
		INFO("disc " << Number);
		const FPatchApplied Applied(Mods, Options);

		// The executable, through the disc's file system: the variant's, with the disc's own index.
		const TResult<FPsxExecutable> Executable = ReadBootExecutable(*Disc);
		const TResult<FPsxExecutable> Image = ReadBootExecutable(Patched);
		REQUIRE(Executable);
		REQUIRE(Image);
		CHECK(WithoutIndex(*Executable) == WithoutIndex(*Image));
		const TResult<FDiscIdentity> Identity = IdentifyDisc(*Disc);
		REQUIRE(Identity);
		CHECK(Identity->Release == Release);
		CHECK(Identity->bPatched);
		CHECK(Identity->bUsaProgram);
		CHECK(Identity->bJapaneseCommands == Options.bJapaneseCommands);

		// Every file of the variant, at the disc's own record.
		const TResult<FGameFileIndex> Index = FGameFileIndex::Read(*Executable);
		const TResult<FGameFileIndex> ImageIndex = FGameFileIndex::Read(*Image);
		REQUIRE(Index);
		REQUIRE(ImageIndex);
		const std::vector<FModFile> Files = Mods.Italian->Files(Options, Number);
		CHECK(Files.size() > 700);
		int32 Different = 0;
		for (const FModFile& File : Files)
		{
			const std::size_t Record = static_cast<std::size_t>(Number == 2 ? File.Record - FDiscModFiles::SecondDiscShift : File.Record);
			const TResult<std::vector<uint8>> Read = ReadFileBytes(Disc->Image(), Index->Records()[Record]);
			const TResult<std::vector<uint8>> Made = ReadFileBytes(Patched.Image(), ImageIndex->Records()[Record]);
			Different += !Read || !Made || *Read != *Made ? 1 : 0;
		}
		CHECK(Different == 0);

		// A movie the patch re-encodes, sector by sector.
		const std::size_t Movie = static_cast<std::size_t>(Number == 1 ? PatchedMovieRecords.first : PatchedMovieRecords.second);
		const FIndexRecord& Own = Index->Records()[Movie];
		const FIndexRecord& Made = ImageIndex->Records()[Movie];
		REQUIRE(Own.Size == Made.Size);
		int32 DifferentSectors = 0;
		for (uint32 Sector = 0; Sector < static_cast<uint32>(Own.Size) / MovieSectorBytes; ++Sector)
		{
			const TResult<std::vector<uint8>> Read = Disc->Image().ReadSourceSector(Own.Lba + Sector);
			const TResult<std::vector<uint8>> Patch = Patched.Image().ReadSourceSector(Made.Lba + Sector);
			DifferentSectors += !Read || !Patch || *Read != *Patch ? 1 : 0;
		}
		CHECK(DifferentSectors == 0);
	}
}

TEST_CASE("The Italian translation read from Mods gives the USA discs the files, executable and movies of its patched images", "[Mods][Content]")
{
	const std::optional<std::filesystem::path> Folder = Testing::ItalianPatchMods();
	if (!Folder)
	{
		WARN("the Italian translation's patch not found (WELTALL_ITALIAN_PATCH)");
		return;
	}
	const FFoundMods Mods = FindMods(*Folder);
	REQUIRE(Mods.Italian);
	FItalianOptions Options;
	Options.bEnabled = true;
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_ITALIAN_DISC(Patched, Release);
		CheckAgainstImage(Mods, Options, Release, *Patched);
	}
}

TEST_CASE("The Italian translation read from Mods with the Japanese commands and the Japanese sound's movies gives the USA discs those of its patched images", "[Mods][Content]")
{
	const std::optional<std::filesystem::path> Folder = Testing::ItalianPatchMods();
	if (!Folder)
	{
		WARN("the Italian translation's patch not found (WELTALL_ITALIAN_PATCH)");
		return;
	}
	const FFoundMods Mods = FindMods(*Folder);
	REQUIRE(Mods.Italian);
	FItalianOptions Options;
	Options.bEnabled = true;
	Options.bJapaneseCommands = true;
	Options.Movies = EItalianMovies::Japanese;
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		REQUIRE_ITALIAN_JP_DISC(Patched, Release);
		CheckAgainstImage(Mods, Options, Release, *Patched);
	}
}

TEST_CASE("Off, or over a disc that does not run the USA release's program, the Italian translation changes nothing", "[Mods][Content]")
{
	const std::optional<std::filesystem::path> Folder = Testing::ItalianPatchMods();
	if (!Folder)
	{
		WARN("the Italian translation's patch not found (WELTALL_ITALIAN_PATCH)");
		return;
	}
	const FFoundMods Mods = FindMods(*Folder);
	REQUIRE(Mods.Italian);
	REQUIRE_DISC(Usa, EDiscRelease::UsaDisc1);
	const TResult<FPsxExecutable> Own = ReadBootExecutable(*Usa);
	REQUIRE(Own);
	{
		const FPatchApplied Applied(Mods, FItalianOptions{});
		const TResult<FPsxExecutable> Read = ReadBootExecutable(*Usa);
		REQUIRE(Read);
		CHECK(WithoutIndex(*Read) == WithoutIndex(*Own));
		CHECK_FALSE(IdentifyDisc(*Usa)->bPatched);
	}
	if (const FIso9660* Undub = Testing::FindDisc(EDiscRelease::UndubDisc1))
	{
		FItalianOptions Options;
		Options.bEnabled = true;
		const FPatchApplied Applied(Mods, Options);
		const TResult<FDiscIdentity> Identity = IdentifyDisc(*Undub);
		REQUIRE(Identity);
		CHECK(Identity->Release == EDiscRelease::UndubDisc1);
		CHECK_FALSE(Identity->bPatched);
	}
}
