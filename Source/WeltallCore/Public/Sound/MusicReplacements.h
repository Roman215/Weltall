#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	/** The key of a music sequence (an smds file): its bytes' asset key, the same for every copy the discs hold. */
	WELTALLCORE_API uint64 MusicSequenceKey(std::span<const uint8> Sequence);

	/**
	 * Recordings standing for the game's music sequences: WAV files anywhere below a folder but its originals, each named
	 * so that its name ends with its sequence's key (Weltall::AssetKeyName). One recording serves every copy of its
	 * sequence, on either disc and in every mode. Links are not followed.
	 */
	class FMusicReplacements
	{
	public:
		/** The folder below a music folder that holds exported originals, which are never read as replacements. */
		static constexpr std::string_view OriginalsFolder = "original";

		/** Finds every recording below Folder but hidden ones (IsHiddenFile); a key named twice takes the first file by path. None for a missing folder. */
		WELTALLCORE_API static TResult<std::shared_ptr<const FMusicReplacements>> Find(const std::filesystem::path& Folder);

		/** The recording's bytes, or none when no recording stands for the key. */
		WELTALLCORE_API TResult<std::optional<std::vector<uint8>>> Read(uint64 Key) const;
		std::size_t Count() const { return Files.size(); }

	private:
		std::map<uint64, std::filesystem::path> Files;
	};
}
