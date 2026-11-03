#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Weltall
{
	class FGameFileIndex;
	struct FIndexRecord;

	/** One of a mod's files: the index record it stands for, numbered as the first disc's index numbers it. */
	struct FModFile
	{
		int32 Record = 0;
		std::filesystem::path Path;
		/** A packed section 5 (a field map's scripts) written over the file's own place for it, cut or padded to fit. */
		std::filesystem::path Section5;
		/** Where the file lies in Path, for a mod that packs its files into one; a Size of 0 is the whole of Path. */
		uint64 Offset = 0;
		uint64 Size = 0;
		/** The file made in memory, read in place of Path. */
		std::shared_ptr<const std::vector<uint8>> Bytes{};
	};

	/** Perfect Works Build's rebalance of the Battling arena: none, its Basic mode, or its Expert mode (Basic's with tweaks for two players). */
	enum class EPerfectWorksArena : uint8
	{
		Original,
		Basic,
		Expert,
	};

	/** Changes a mod made to the game's code that the port makes in its own where the mod's files in use need them. */
	struct FModProgramChanges
	{
		/** Perfect Works Build's battle-event messages, which its battle events' scripts measure for (EBattleMessageLayout). */
		bool bPerfectWorksBattleMessages = false;
		/** Perfect Works Build's rebalance: its rules of battle and of the menus' items and parts, which its party and items need. */
		bool bPerfectWorksRebalance = false;
		/** Perfect Works Build's arena: its rules of the fight and of the computer, which its gears and gear table need. */
		EPerfectWorksArena PerfectWorksArena = EPerfectWorksArena::Original;

		bool operator==(const FModProgramChanges&) const = default;
	};

	/** A file read in place of the disc's: its bytes to the end of their last sector, and the file's own size within them. */
	struct FFileReplacement
	{
		std::vector<uint8> Bytes;
		uint64 Size = 0;
	};

	/** A mod's files standing for files of one disc, found by the sectors the disc's index gives those files. */
	class FDiscModFiles
	{
	public:
		/** The second disc's index has five movies fewer than the first's: every record after them is five lower. */
		static constexpr int32 SecondDiscShift = 5;

		/**
		 * Files for the disc whose index is Index, played as disc Disc (1 or 2). A file's record is the first disc's
		 * number: the second disc's index, which has five movies fewer, has it five records lower. Files whose record
		 * the disc lacks, or holds no file at, are left out. Changes are those of the mod's code the files need.
		 */
		WELTALLCORE_API static std::shared_ptr<const FDiscModFiles> Make(const FGameFileIndex& Index, int32 Disc, std::span<const FModFile> Files,
			const FModProgramChanges& Changes = {});

		/** Record's file from the mod, read now; none where the mod has none. */
		WELTALLCORE_API TResult<std::optional<FFileReplacement>> Read(const FIndexRecord& Record) const;

		/** The mod's file whose record starts at sector Lba, read now, for a reader that finds files otherwise (the disc's file system). */
		WELTALLCORE_API TResult<std::optional<FFileReplacement>> ReadAt(uint32 Lba) const;

		std::size_t Count() const { return BySector.size(); }
		const FModProgramChanges& Changes() const { return ProgramChanges; }

	private:
		std::unordered_map<uint32, FModFile> BySector;
		FModProgramChanges ProgramChanges;
	};
}
