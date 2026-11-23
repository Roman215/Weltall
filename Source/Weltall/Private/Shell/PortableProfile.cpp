#include "Shell/PortableProfile.h"

#include "Foundation/WholeFiles.h"
#include "Misc/Guid.h"

#include <fstream>
#include <system_error>

namespace
{
	using Weltall::FError;
	namespace fs = std::filesystem;

	FError FileError(const char* Action, const fs::path& Path, const std::error_code& Error)
	{
		const auto Name = Path.generic_u8string();
		return Weltall::MakeError(std::string(Action) + " " + std::string(Name.begin(), Name.end()) + ": " + Error.message());
	}

	std::optional<FError> CopyFile(const fs::path& Source, const fs::path& Target, const fs::path& Prepared)
	{
		std::error_code Error;
		const auto Existing = fs::symlink_status(Target, Error);
		if (Error && Error != std::errc::no_such_file_or_directory) return FileError("cannot inspect", Target, Error);
		if (fs::is_directory(Existing)) return FileError("cannot copy to", Target, std::make_error_code(std::errc::is_a_directory));
		if (fs::exists(Existing)) return std::nullopt;

		fs::create_directories(Target.parent_path(), Error);
		if (Error) return FileError("cannot create", Target.parent_path(), Error);
		if (!fs::copy_file(Source, Prepared, Error)) return FileError("cannot copy", Source, Error);
		const auto Published = Weltall::PublishNewFile(Prepared, Target);
		return Published ? std::nullopt : std::optional<FError>(Published.Error());
	}

	std::optional<FError> CopyFolder(const fs::path& Source, const fs::path& Target, const fs::path& Prepared)
	{
		std::error_code Error;
		fs::recursive_directory_iterator Entry(Source, Error);
		if (Error == std::errc::no_such_file_or_directory) return std::nullopt;
		for (; !Error && Entry != fs::recursive_directory_iterator(); Entry.increment(Error))
		{
			// Saves are written to .new first, then renamed when complete.
			const auto& Path = Entry->path();
			if (Entry->is_regular_file(Error) && Path.extension() != ".new")
			{
				if (auto Failed = CopyFile(Path, Target / Path.lexically_relative(Source), Prepared)) return Failed;
			}
		}
		return Error ? std::optional<FError>(FileError("cannot read", Source, Error)) : std::nullopt;
	}

	std::optional<FError> ImportFiles(const fs::path& Portable, const fs::path& Settings, const fs::path& Saves,
		const fs::path& Marker, const fs::path& Prepared)
	{
		std::error_code Error;
		const bool bHasSettings = fs::exists(Settings, Error);
		if (Error) return FileError("cannot inspect", Settings, Error);
		if (bHasSettings)
		{
			if (auto Failed = CopyFile(Settings, Portable / Settings.filename(), Prepared)) return Failed;
		}
		if (auto Failed = CopyFolder(Saves, Portable / "Saves", Prepared)) return Failed;
		if (auto Failed = CopyFolder(Saves.parent_path() / "States", Portable / "States", Prepared)) return Failed;

		std::ofstream Complete(Prepared, std::ios::binary);
		Complete.close();
		if (!Complete) return Weltall::MakeError("cannot write the portable profile import marker");
		const auto Published = Weltall::PublishNewFile(Prepared, Marker);
		return Published ? std::nullopt : std::optional<FError>(Published.Error());
	}
}

std::optional<Weltall::FError> ImportPortableProfile(const std::filesystem::path& Portable,
	const std::filesystem::path& Settings, const std::filesystem::path& Saves)
{
	const fs::path Marker = Portable / ".profile-imported";
	std::error_code Error;
	const auto Status = fs::symlink_status(Marker, Error);
	if (Error && Error != std::errc::no_such_file_or_directory) return FileError("cannot inspect", Marker, Error);
	if (fs::is_regular_file(Status)) return std::nullopt;
	if (fs::exists(Status)) return Weltall::MakeError("the portable profile import marker is unavailable");

	const fs::path Prepared = Portable / *FString::Printf(TEXT(".profile-import-%s.tmp"), *FGuid::NewGuid().ToString());
	auto Failed = ImportFiles(Portable, Settings, Saves, Marker, Prepared);
	if (Failed) fs::remove(Prepared, Error);
	return Failed;
}
