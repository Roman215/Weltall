#include "Foundation/WholeFiles.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#if defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Weltall
{
	namespace
	{
		constexpr const char* NewSuffix = ".new";

		TResult<bool> RenameWithoutReplacing(const std::filesystem::path& Prepared, const std::filesystem::path& Target)
		{
#if defined(_WIN32)
			if (_wrename(Prepared.c_str(), Target.c_str()) == 0) return true;
			const int Failure = errno;
#elif defined(__linux__) && defined(SYS_renameat2)
			constexpr unsigned RenameNoReplace = 1;
			if (syscall(SYS_renameat2, AT_FDCWD, Prepared.c_str(), AT_FDCWD, Target.c_str(), RenameNoReplace) == 0) return true;
			const int Failure = errno;
#else
			return MakeError("this filesystem cannot publish files without replacing existing files");
#endif
#if defined(_WIN32) || (defined(__linux__) && defined(SYS_renameat2))
			std::error_code InspectError;
			const auto Status = std::filesystem::symlink_status(Target, InspectError);
			if (!InspectError && std::filesystem::exists(Status)) return false;
			return MakeError("cannot publish " + Target.string() + ": " + std::error_code(Failure, std::generic_category()).message());
#endif
		}
	}

	TResult<std::optional<std::vector<uint8>>> ReadWholeFile(const std::filesystem::path& Path)
	{
		std::error_code Error;
		if (!std::filesystem::exists(Path, Error))
		{
			return std::optional<std::vector<uint8>>();
		}
		std::ifstream File(Path, std::ios::binary);
		std::vector<uint8> Bytes{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
		if (!File.good() && !File.eof())
		{
			return MakeError("the file " + Path.string() + " cannot be read");
		}
		return std::optional<std::vector<uint8>>(std::move(Bytes));
	}

	std::optional<FError> ReplaceWholeFile(const std::filesystem::path& Path, std::span<const uint8> Bytes)
	{
		std::error_code Error;
		std::filesystem::create_directories(Path.parent_path(), Error);
		if (Error)
		{
			return MakeError("the folder " + Path.parent_path().string() + " cannot be made: " + Error.message());
		}
		std::filesystem::path Written = Path;
		Written += NewSuffix;
		{
			std::ofstream File(Written, std::ios::binary | std::ios::trunc);
			File.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
			File.close();
			if (!File)
			{
				return MakeError("the file " + Written.string() + " cannot be written");
			}
		}
		// Renaming over the old file replaces it in one step on every platform.
		std::filesystem::rename(Written, Path, Error);
		if (Error)
		{
			return MakeError("the file " + Path.string() + " cannot be put in place: " + Error.message());
		}
		return std::nullopt;
	}

	TResult<bool> PublishNewFile(const std::filesystem::path& Prepared, const std::filesystem::path& Target)
	{
		std::error_code Error;
		std::filesystem::create_hard_link(Prepared, Target, Error);
		const bool bCreated = !Error;
		if (Error && Error != std::errc::file_exists)
		{
			// Filesystems without hard links can publish the completed file with a non-replacing rename.
			const auto Renamed = RenameWithoutReplacing(Prepared, Target);
			if (!Renamed || *Renamed) return Renamed;
		}
		std::filesystem::remove(Prepared, Error);
		if (Error) return MakeError("cannot remove " + Prepared.string() + ": " + Error.message());
		return bCreated;
	}
}
