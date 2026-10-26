#include "Sound/MusicReplacements.h"

#include "Foundation/AssetKey.h"
#include "Foundation/FileByteSource.h"
#include "Foundation/HiddenFiles.h"

#include <algorithm>
#include <cctype>

namespace Weltall
{
	namespace
	{
		constexpr uint64 MostBytes = 512ULL * 1024 * 1024;

		bool IsWave(const std::filesystem::path& Path)
		{
			std::string Extension = Path.extension().string();
			std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
			return Extension == ".wav";
		}
	}

	uint64 MusicSequenceKey(std::span<const uint8> Sequence)
	{
		return AssetKeyOf(Sequence);
	}

	TResult<std::shared_ptr<const FMusicReplacements>> FMusicReplacements::Find(const std::filesystem::path& Folder)
	{
		auto Found = std::make_shared<FMusicReplacements>();
		std::error_code Error;
		if (!std::filesystem::is_directory(Folder, Error))
		{
			return std::shared_ptr<const FMusicReplacements>(std::move(Found));
		}
		std::vector<std::filesystem::path> Waves;
		for (auto Entry = std::filesystem::recursive_directory_iterator(Folder, Error); !Error && Entry != std::filesystem::recursive_directory_iterator();
			Entry.increment(Error))
		{
			if (IsHiddenFile(Entry->path()) || (Entry.depth() == 0 && Entry->is_directory(Error) && Entry->path().filename() == OriginalsFolder))
			{
				Entry.disable_recursion_pending();
				continue;
			}
			if (Entry->is_symlink(Error) || !Entry->is_regular_file(Error) || !IsWave(Entry->path()))
			{
				continue;
			}
			Waves.push_back(Entry->path());
		}
		if (Error)
		{
			return MakeError("cannot list the music replacements: " + Error.message());
		}
		std::sort(Waves.begin(), Waves.end());
		for (const std::filesystem::path& Wave : Waves)
		{
			if (const std::optional<uint64> Key = AssetKeyOfName(Wave.stem().string()))
			{
				Found->Files.emplace(*Key, Wave);
			}
		}
		return std::shared_ptr<const FMusicReplacements>(std::move(Found));
	}

	TResult<std::optional<std::vector<uint8>>> FMusicReplacements::Read(uint64 Key) const
	{
		const auto Found = Files.find(Key);
		if (Found == Files.end())
		{
			return std::optional<std::vector<uint8>>{};
		}
		const auto File = FFileByteSource::Open(Found->second);
		if (!File)
		{
			return File.Error();
		}
		if ((*File)->Size() == 0 || (*File)->Size() > MostBytes)
		{
			return MakeError("music replacement has an invalid size: " + Found->second.string());
		}
		std::vector<uint8> Bytes(static_cast<std::size_t>((*File)->Size()));
		if (!(*File)->Read(0, Bytes))
		{
			return MakeError("cannot read music replacement: " + Found->second.string());
		}
		return std::optional{std::move(Bytes)};
	}
}
