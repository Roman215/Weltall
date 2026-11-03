#include "Foundation/WholeFiles.h"

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace Weltall
{
	namespace
	{
		constexpr const char* NewSuffix = ".new";
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
}
