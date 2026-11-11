#include "Media/DiscAssetsInternal.h"
#include "Media/AssetFiles.h"

#include <sstream>
#include <cerrno>
#include <cstdio>

#if defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Weltall::DiscAssets
{
	namespace
	{
		TResult<bool> RenameWithoutReplacing(const std::filesystem::path& Partial, const std::filesystem::path& Target)
		{
#if defined(_WIN32)
			if (_wrename(Partial.c_str(), Target.c_str()) == 0) return true;
			const int Failure = errno;
#elif defined(__linux__) && defined(SYS_renameat2)
			constexpr unsigned RenameNoReplace = 1;
			if (syscall(SYS_renameat2, AT_FDCWD, Partial.c_str(), AT_FDCWD, Target.c_str(), RenameNoReplace) == 0) return true;
			const int Failure = errno;
#else
			return MakeError("this filesystem cannot publish assets without replacing existing files");
#endif
#if defined(_WIN32) || (defined(__linux__) && defined(SYS_renameat2))
			std::error_code InspectError;
			const auto Status = std::filesystem::symlink_status(Target, InspectError);
			if (!InspectError && std::filesystem::exists(Status)) return false;
			return MakeError("cannot publish asset output: " + std::error_code(Failure, std::generic_category()).message());
#endif
		}
	}
	TResult<bool> SafePath(const std::filesystem::path& Root, const std::filesystem::path& Relative, bool bOptionalOverride)
	{
		if (Relative.empty() || Relative.is_absolute())
		{
			return MakeError("invalid asset path");
		}
		std::filesystem::path Current = Root;
		std::error_code RootError;
		const auto ResolvedRoot = std::filesystem::weakly_canonical(Root, RootError);
		if (RootError || ResolvedRoot != Root.lexically_normal())
		{
			if (bOptionalOverride) return false;
			return MakeError("asset folder changed its resolved location");
		}
		const auto RootStatus = std::filesystem::symlink_status(Root, RootError);
		if (bOptionalOverride && (RootError || !std::filesystem::is_directory(RootStatus))) return false;
		if (std::filesystem::is_symlink(RootStatus))
		{
			return MakeError("asset folders cannot be symbolic links");
		}
		for (const auto& Part : Relative)
		{
			if (Part == ".." || Part == ".")
			{
				return MakeError("invalid asset path");
			}
			Current /= Part;
			std::error_code Error;
			const auto Status = std::filesystem::symlink_status(Current, Error);
			if (Error && Error != std::errc::no_such_file_or_directory)
			{
				return MakeError("cannot inspect asset path: " + Error.message());
			}
			if (std::filesystem::is_symlink(Status))
			{
				return MakeError("asset paths cannot contain symbolic links: " + Current.string());
			}
		}
		return true;
	}

	TResult<bool> PrepareFolder(const std::filesystem::path& Root, const std::filesystem::path& Relative)
	{
		const auto Safe = SafePath(Root, Relative);
		if (!Safe) return Safe.Error();
		std::error_code Error;
		std::filesystem::create_directories((Root / Relative).parent_path(), Error);
		if (Error) return MakeError("cannot create asset folder: " + Error.message());
		return true;
	}

	TResult<bool> WriteManifest(const std::filesystem::path& Root, const std::string& Manifest)
	{
		const auto Ready = PrepareFolder(Root, "manifest.tsv");
		if (!Ready) return Ready.Error();
		const auto Existing = ValidateManifest(Root, Manifest);
		if (!Existing) return Existing.Error();
		if (*Existing) return true;
		const auto PartialReady = PrepareFolder(Root, "manifest.tsv.partial");
		if (!PartialReady) return PartialReady.Error();
		std::ofstream Output(Root / "manifest.tsv.partial", std::ios::binary | std::ios::trunc);
		Output << Manifest;
		Output.close();
		if (!Output) return MakeError("cannot write asset manifest");
		const auto Published = Publish(Root / "manifest.tsv.partial", Root / "manifest.tsv");
		if (!Published) return Published.Error();
		if (!*Published) return ValidateManifest(Root, Manifest);
		return true;
	}

	TResult<bool> ValidateManifest(const std::filesystem::path& Root, const std::string& Manifest)
	{
		const auto Safe = SafePath(Root, "manifest.tsv");
		if (!Safe) return Safe.Error();
		std::error_code Error;
		const bool bExists = std::filesystem::exists(Root / "manifest.tsv", Error);
		if (Error) return MakeError("cannot inspect asset manifest: " + Error.message());
		if (!bExists) return false;
		const auto Size = std::filesystem::file_size(Root / "manifest.tsv", Error);
		if (Error || Size != Manifest.size()) return MakeError("asset manifest belongs to a different disc layout; choose a fresh asset folder");
		std::ifstream Input(Root / "manifest.tsv", std::ios::binary);
		std::ostringstream Text;
		Text << Input.rdbuf();
		if (!Input || Text.str() != Manifest) return MakeError("asset manifest belongs to a different disc layout; choose a fresh asset folder");
		return true;
	}

	TResult<bool> Publish(const std::filesystem::path& Partial, const std::filesystem::path& Target)
	{
		std::error_code Error;
		// Linking publishes a completed file without replacing a mod installed during export.
		std::filesystem::create_hard_link(Partial, Target, Error);
		bool bCreated = !Error;
		if (Error && Error != std::errc::file_exists)
		{
			// FAT and some network filesystems have no hard links. A non-replacing rename keeps the final file atomic.
			const auto Renamed = RenameWithoutReplacing(Partial, Target);
			if (!Renamed) return Renamed.Error();
			if (*Renamed) return true;
			bCreated = false;
		}
		std::filesystem::remove(Partial, Error);
		if (Error) return MakeError("cannot remove partial asset output: " + Error.message());
		return bCreated;
	}
}

namespace Weltall
{
	TResult<bool> PublishAssetFile(const std::filesystem::path& Root, const std::filesystem::path& Relative, std::span<const uint8> Bytes)
	{
		const auto Ready = DiscAssets::PrepareFolder(Root, Relative);
		if (!Ready) return Ready.Error();
		std::error_code Error;
		const bool bExists = std::filesystem::exists(Root / Relative, Error);
		if (Error) return MakeError("cannot inspect an asset file: " + Error.message());
		if (bExists) return false;
		auto Partial = Relative;
		Partial += ".partial";
		const auto PartialReady = DiscAssets::PrepareFolder(Root, Partial);
		if (!PartialReady) return PartialReady.Error();
		std::ofstream Output(Root / Partial, std::ios::binary | std::ios::trunc);
		Output.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
		Output.close();
		if (!Output) return MakeError("cannot write an asset file: " + Relative.generic_string());
		return DiscAssets::Publish(Root / Partial, Root / Relative);
	}
}
