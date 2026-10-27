#include "Field/FieldSpriteFiles.h"

#include "Formats/Lzss.h"
#include "Foundation/LittleEndian.h"
#include "Media/GameFileIndex.h"

#include <algorithm>
#include <string>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		/** A sprite file's first word: its section count. A packed file starts with its unpacked size instead. */
		constexpr uint32 SpriteFileMark = 3;
	}

	TResult<std::vector<FSpriteFile>> ParseFieldSpriteFiles(std::span<const uint8> Section)
	{
		if (Section.size() < 4)
		{
			return MakeError("sprite section is shorter than its count");
		}
		const uint32 Count = ReadU32(Section, 0);
		if (4 + std::size_t{Count} * 4 > Section.size())
		{
			return MakeError("sprite section names " + std::to_string(Count) + " files its " + std::to_string(Section.size()) + " bytes cannot hold");
		}
		std::vector<uint32> Starts(Count);
		for (uint32 File = 0; File < Count; ++File)
		{
			Starts[File] = ReadU32(Section, 4 + 4 * std::size_t{File});
		}
		std::vector<uint32> Ends = Starts;
		Ends.push_back(static_cast<uint32>(Section.size()));
		std::sort(Ends.begin(), Ends.end());
		std::vector<FSpriteFile> Files;
		Files.reserve(Count);
		for (uint32 File = 0; File < Count; ++File)
		{
			const uint32 Start = Starts[File];
			if (Start >= Section.size())
			{
				return MakeError("sprite file " + std::to_string(File) + " starts past its section");
			}
			const uint32 End = *std::upper_bound(Ends.begin(), Ends.end(), Start);
			auto Parsed = FSpriteFile::Parse(Section.subspan(Start, End - Start));
			if (!Parsed)
			{
				return MakeError("sprite file " + std::to_string(File) + ": " + Parsed.Error().Message);
			}
			Files.push_back(std::move(*Parsed));
		}
		return Files;
	}

	namespace
	{
		/** Each file's start and end within its section, in the files' order. */
		TResult<std::vector<std::pair<uint32, uint32>>> FieldSpriteFileRanges(std::span<const uint8> Section)
		{
			if (Section.size() < 4)
			{
				return MakeError("sprite section is shorter than its count");
			}
			const uint32 Count = ReadU32(Section, 0);
			if (4 + std::size_t{Count} * 4 > Section.size())
			{
				return MakeError("sprite section names " + std::to_string(Count) + " files its " + std::to_string(Section.size()) + " bytes cannot hold");
			}
			std::vector<uint32> Starts(Count);
			for (uint32 File = 0; File < Count; ++File)
			{
				Starts[File] = ReadU32(Section, 4 + 4 * std::size_t{File});
			}
			std::vector<uint32> Ends = Starts;
			Ends.push_back(static_cast<uint32>(Section.size()));
			std::sort(Ends.begin(), Ends.end());
			std::vector<std::pair<uint32, uint32>> Ranges;
			for (uint32 File = 0; File < Count; ++File)
			{
				const uint32 Start = Starts[File];
				if (Start >= Section.size())
				{
					return MakeError("sprite file " + std::to_string(File) + " starts past its section");
				}
				Ranges.emplace_back(Start, *std::upper_bound(Ends.begin(), Ends.end(), Start));
			}
			return Ranges;
		}
	}

	TResult<std::span<const uint8>> FieldSpriteFileBytes(std::span<const uint8> Section, uint32 File)
	{
		TResult<std::vector<std::pair<uint32, uint32>>> Ranges = FieldSpriteFileRanges(Section);
		if (!Ranges)
		{
			return Ranges.Error();
		}
		if (File >= Ranges->size())
		{
			return MakeError("sprite section has no file " + std::to_string(File));
		}
		const auto [Start, End] = (*Ranges)[File];
		return Section.subspan(Start, End - Start);
	}

	TResult<std::vector<uint8>> ReplaceFieldSpriteFile(std::span<const uint8> Section, uint32 File, std::span<const uint8> Bytes)
	{
		TResult<std::vector<std::pair<uint32, uint32>>> Ranges = FieldSpriteFileRanges(Section);
		if (!Ranges)
		{
			return Ranges.Error();
		}
		if (File >= Ranges->size())
		{
			return MakeError("sprite section has no file " + std::to_string(File));
		}
		const std::size_t Count = Ranges->size();
		std::vector<uint8> Out(4 + Count * 4);
		WriteU32(Out, 0, static_cast<uint32>(Count));
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			WriteU32(Out, 4 + Index * 4, static_cast<uint32>(Out.size()));
			const std::span<const uint8> Source = Index == File ? Bytes : Section.subspan((*Ranges)[Index].first, (*Ranges)[Index].second - (*Ranges)[Index].first);
			Out.insert(Out.end(), Source.begin(), Source.end());
		}
		return Out;
	}

	TResult<FSpriteFile> ReadSpriteFile(const FCdImage& Image, const FGameFileIndex& Index, int32 Directory, int32 Number)
	{
		const FIndexRecord* Record = Index.Find(Directory, Number);
		if (Record == nullptr)
		{
			return MakeError("sprite file " + std::to_string(Directory) + "/" + std::to_string(Number) + " is not in the file index");
		}
		auto Sectors = ReadFileSectors(Image, *Record);
		if (!Sectors)
		{
			return Sectors.Error();
		}
		if (Sectors->size() >= 4 && ReadU32(*Sectors, 0) == SpriteFileMark)
		{
			return FSpriteFile::Parse(*Sectors);
		}
		auto Unpacked = UnpackLzss(*Sectors);
		if (!Unpacked)
		{
			return Unpacked.Error();
		}
		return FSpriteFile::Parse(*Unpacked);
	}
}
