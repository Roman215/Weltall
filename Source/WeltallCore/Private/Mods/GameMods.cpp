#include "Mods/GameMods.h"

#include "Foundation/FolderNames.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/GameLibrary.h"
#include "Media/PsxExecutable.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** The movies' sectors, which a movie patch writes into: the files of the index's first group (record 0 heads it). */
		std::pair<uint32, uint32> MovieSectors(const FGameFileIndex& Index)
		{
			constexpr uint32 MovieSectorBytes = 2336;
			const std::vector<FIndexRecord>& Records = Index.Records();
			std::pair<uint32, uint32> Sectors{0, 0};
			if (Records.empty() || Records.front().Size >= 0)
			{
				return Sectors;
			}
			const std::size_t Count = std::min<std::size_t>(static_cast<std::size_t>(-static_cast<int64>(Records.front().Size)), Records.size() - 1);
			for (std::size_t Movie = 1; Movie <= Count; ++Movie)
			{
				const FIndexRecord& Record = Records[Movie];
				if (Record.IsFile())
				{
					Sectors.first = Sectors.second == 0 ? Record.Lba : std::min(Sectors.first, Record.Lba);
					Sectors.second = std::max(Sectors.second, Record.Lba + static_cast<uint32>(Record.Size) / MovieSectorBytes);
				}
			}
			return Sectors;
		}

		/**
		 * The Italian translation's variant over a disc of the USA release's program: its files by record, and its
		 * executable with the disc's own index, since each file is read at the disc's own record.
		 */
		std::optional<FError> ApplyItalian(const FIso9660& Disc, int32 Number, const FItalianPatch& Patch, const FItalianOptions& Options)
		{
			const TResult<FDiscIdentity> Identity = IdentifyDisc(Disc);
			if (!Identity || !Identity->bUsaProgram)
			{
				return std::nullopt;
			}
			const TResult<FPsxExecutable> Own = ReadBootExecutable(Disc);
			if (!Own)
			{
				return Own.Error();
			}
			const TResult<FGameFileIndex> Index = FGameFileIndex::Read(*Own);
			if (!Index)
			{
				return Index.Error();
			}
			TResult<std::vector<uint8>> Variant = Patch.Executable(Options, Number);
			if (!Variant)
			{
				return Variant.Error();
			}
			const TResult<FPsxExecutable> Parsed = FPsxExecutable::Parse(*Variant);
			const std::span<const uint8> OwnIndex = Own->Read(FGameFileIndex::IndexAddress, FGameFileIndex::IndexBytes);
			if (!Parsed || !Parsed->Contains(FGameFileIndex::IndexAddress, FGameFileIndex::IndexBytes) || OwnIndex.empty())
			{
				return MakeError("the Italian translation's executable for disc " + std::to_string(Number) + " holds no file index");
			}
			std::copy(OwnIndex.begin(), OwnIndex.end(), Variant->begin() + FPsxExecutable::HeaderSize + (FGameFileIndex::IndexAddress - Parsed->LoadAddress()));
			std::vector<FModFile> Files = Patch.Files(Options, Number);
			FModFile Executable;
			Executable.Record = FItalianPatch::ExecutableRecord;
			Executable.Bytes = std::make_shared<const std::vector<uint8>>(std::move(*Variant));
			Files.push_back(std::move(Executable));
			const TResult<std::shared_ptr<const FDiscRawPatch>> Movies = Patch.OpenMovies(Number, Options.Movies);
			if (!Movies)
			{
				return Movies.Error();
			}
			Disc.Image().SetModFiles(FDiscModFiles::Make(*Index, Number, Files));
			// The movie patches are of whole raw sectors: an image of 2048-byte sectors keeps its own (silent) movies.
			if (*Movies && (*Movies)->Fits(Disc.Image()))
			{
				const std::pair<uint32, uint32> Sectors = MovieSectors(*Index);
				Disc.Image().SetRawPatch(*Movies, Sectors.first, Sectors.second);
			}
			return std::nullopt;
		}
	}

	FFoundMods FindMods(const std::filesystem::path& Folder)
	{
		return {FPerfectWorksRelease::Find(FolderIgnoringCase(Folder, "PerfectWorks")), FItalianPatch::Find(Folder)};
	}

	std::optional<FError> ApplyMods(const FGameLibrary& Library, const FFoundMods& Mods, const FPerfectWorksOptions& Options, const FItalianOptions& Italian)
	{
		for (const EDiscRelease Release : Library.Releases())
		{
			if (Release == EDiscRelease::JapanDisc1 || Release == EDiscRelease::JapanDisc2)
			{
				continue;
			}
			const FIso9660* Disc = Library.Find(Release);
			const int32 Number = DiscNumberOfRelease(Release);
			Disc->Image().SetModFiles(nullptr);
			Disc->Image().SetRawPatch(nullptr);
			if (Mods.Italian && Italian.bEnabled)
			{
				if (std::optional<FError> Error = ApplyItalian(*Disc, Number, *Mods.Italian, Italian))
				{
					return Error;
				}
				continue;
			}
			if (!Mods.PerfectWorks || !Options.bEnabled)
			{
				continue;
			}
			const auto Executable = ReadBootExecutable(*Disc);
			if (!Executable)
			{
				return Executable.Error();
			}
			const auto Index = FGameFileIndex::Read(*Executable);
			if (!Index)
			{
				return Index.Error();
			}
			const std::vector<FModFile> Files = Mods.PerfectWorks->Files(Options, Number);
			Disc->Image().SetModFiles(FDiscModFiles::Make(*Index, Number, Files, Mods.PerfectWorks->Changes(Options, Files)));
		}
		return std::nullopt;
	}
}
