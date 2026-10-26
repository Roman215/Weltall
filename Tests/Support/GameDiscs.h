#pragma once

#include "Media/DiscIdentification.h"
#include "Media/DiscModFiles.h"
#include "Media/Iso9660.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace Weltall::Testing
{
	/**
	 * The player's disc of a given release from the Games folder, or null when it is not there.
	 * The folder is scanned once per test process.
	 */
	const FIso9660* FindDisc(EDiscRelease Release);

	/**
	 * A translation's disc of a release from the folder the environment variable WELTALL_PATCHED_GAMES names (copies of
	 * the player's discs that a translation's patch made, kept apart from Games, where they would play in place of the
	 * originals), or null when the variable or the disc is not there. The folder is scanned once per test process.
	 */
	const FIso9660* FindPatchedDisc(EDiscRelease Release);

	/** The folder WELTALL_PATCHED_GAMES names, if it names one. */
	std::optional<std::filesystem::path> PatchedGamesFolder();

	/** A mod's files given to a disc's image while it lives, the image's own again after (other tests share the disc). */
	struct FModFilesGiven
	{
		FModFilesGiven(const FIso9660& InDisc, std::shared_ptr<const FDiscModFiles> Files) : Disc(InDisc) { Disc.Image().SetModFiles(std::move(Files)); }
		~FModFilesGiven() { Disc.Image().SetModFiles(nullptr); }
		FModFilesGiven(const FModFilesGiven&) = delete;
		FModFilesGiven& operator=(const FModFilesGiven&) = delete;

		const FIso9660& Disc;
	};
}

/** Skips the current test, naming the missing disc, unless the release is present. */
#define REQUIRE_DISC(Variable, Release)                                                       \
	const ::Weltall::FIso9660* Variable = ::Weltall::Testing::FindDisc(Release);         \
	if (Variable == nullptr)                                                                  \
	{                                                                                         \
		SKIP("disc not found in Games/: " #Release);                                          \
	}

/**
 * Leaves the current test with a warning naming the missing disc unless WELTALL_PATCHED_GAMES's folder holds a
 * translation's disc of the release: those discs are optional, and SKIP needs the exceptions the build turns off.
 */
#define REQUIRE_PATCHED_DISC(Variable, Release)                                               \
	const ::Weltall::FIso9660* Variable = ::Weltall::Testing::FindPatchedDisc(Release);  \
	if (Variable == nullptr)                                                                  \
	{                                                                                         \
		WARN("patched disc not found (WELTALL_PATCHED_GAMES): " #Release);                    \
		return;                                                                               \
	}
