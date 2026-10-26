#pragma once

#include <filesystem>
#include <string>

namespace Weltall::Testing
{
	/**
	 * The file macOS writes beside each file it copies to a drive of another file system, named "._" and the file's
	 * name: an AppleDouble header, the Finder's info with the file's extended attributes (a download's quarantine) and
	 * a blank resource fork, 4096 bytes.
	 */
	std::string AppleDoubleCompanion();

	/** Writes the companion macOS would leave beside File. */
	void WriteAppleDoubleCompanion(const std::filesystem::path& File);
}
