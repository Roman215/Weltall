#pragma once

#include "Foundation/Result.h"

#include <filesystem>
#include <optional>

/** Copies a desktop profile into an existing portable folder once, keeping any files already there. */
WELTALL_API std::optional<Weltall::FError> ImportPortableProfile(const std::filesystem::path& Portable,
	const std::filesystem::path& Settings, const std::filesystem::path& Saves);
