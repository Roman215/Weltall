#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** A file's bytes: none when there is no file, an error when it cannot be read. */
	WELTALLCORE_API TResult<std::optional<std::vector<uint8>>> ReadWholeFile(const std::filesystem::path& Path);

	/**
	 * Bytes written to a file beside Path, then put in its place (its folder made first): a failed write never loses the
	 * file it replaces. Why not, when it could not be.
	 */
	WELTALLCORE_API std::optional<FError> ReplaceWholeFile(const std::filesystem::path& Path, std::span<const uint8> Bytes);
}
