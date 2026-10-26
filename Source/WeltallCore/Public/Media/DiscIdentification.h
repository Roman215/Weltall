#pragma once

#include "Foundation/Result.h"
#include "Media/Iso9660.h"

#include <span>
#include <string_view>

namespace Weltall
{
	/** The exact disc releases Weltall can read. Offsets inside the game differ between releases. */
	enum class EDiscRelease : uint8
	{
		UsaDisc1,
		UsaDisc2,
		UndubDisc1,
		UndubDisc2,
		JapanDisc1,
		JapanDisc2,
	};

	/** Stable directory name for a disc's locally extracted assets. */
	WELTALLCORE_API std::string_view DiscReleaseId(EDiscRelease Release);
	/** One or two, independently of the disc's language. */
	WELTALLCORE_API uint8 DiscNumberOfRelease(EDiscRelease Release);
	/** Whether the release provides the English game data the port plays. */
	WELTALLCORE_API bool IsPlayableRelease(EDiscRelease Release);
	/** The compatible original layout for a playable release. */
	WELTALLCORE_API EDiscRelease GameplayRelease(EDiscRelease Release);

	struct FDiscIdentity
	{
		EDiscRelease Release;
		/** 1 or 2: which of the game's two discs this is. */
		uint8 DiscNumber;
		const char* Title;
		/**
		 * A translation's copy of the release, which plays in its place: its data patched, and its executable's code the
		 * release's byte for byte, only its file index (where each file lies, its size) and the boot notice's picture its
		 * own; or a translation's own program Weltall knows (the French translation's).
		 */
		bool bPatched = false;
	};

	/**
	 * Recognizes a disc by its boot executable: the file name gives the disc, the file's hash gives
	 * the exact release. Different rips of the same disc therefore match, whatever their other files.
	 * A disc whose executable differs from the USA release's only where a patch of the game's data
	 * rewrites it is that release, patched; so is a French translation's disc.
	 */
	WELTALLCORE_API TResult<FDiscIdentity> IdentifyDisc(const FIso9660& Disc);

	/** IdentifyDisc of a boot executable by its name on the disc and its file's bytes. */
	WELTALLCORE_API TResult<FDiscIdentity> IdentifyExecutable(std::string_view BootPath, std::span<const uint8> Executable);
}
