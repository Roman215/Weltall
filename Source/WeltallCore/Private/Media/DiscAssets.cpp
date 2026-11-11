#include "Media/DiscAssetsInternal.h"

#include "Foundation/FileByteSource.h"
#include "Foundation/Sha1.h"
#include "Media/GameMovies.h"
#include "Sound/MusicReplacements.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Weltall
{
	namespace
	{
		std::string Number(uint32 Value, int Width)
		{
			std::ostringstream Text;
			Text << std::setfill('0') << std::setw(Width) << Value;
			return Text.str();
		}

		std::filesystem::path FilePath(const FGameFileIndex& Index, std::size_t Record)
		{
			const std::optional<FGameFileIndex::FFileName> Name = Index.NameOf(Record);
			if (!Name) return std::filesystem::path("records") / (Number(static_cast<uint32>(Record), 6) + ".bin");
			return std::filesystem::path("files") / Number(static_cast<uint32>(Name->Directory), 2) / (Number(static_cast<uint32>(Name->Number), 4) + ".bin");
		}

		bool ValidName(std::string_view Name)
		{
			return !Name.empty() && Name.size() <= 80 && Name != "." && Name != ".." &&
				std::all_of(Name.begin(), Name.end(), [](char C) { return (C >= 'a' && C <= 'z') ||
					(C >= '0' && C <= '9') || C == '.' || C == '_' || C == '-'; });
		}
	}

	FDiscAssetStore::FDiscAssetStore(std::unique_ptr<FState> InState) : State(std::move(InState)) {}
	FDiscAssetStore::~FDiscAssetStore() = default;

	TResult<std::shared_ptr<FDiscAssetStore>> FDiscAssetStore::Open(const std::filesystem::path& AssetsRoot,
		std::string SourceId, const FCdImage& Image, const FGameFileIndex& Index)
	{
		if (!ValidName(SourceId)) return MakeError("invalid disc asset source name");
		std::error_code Error;
		const auto Root = std::filesystem::weakly_canonical(AssetsRoot, Error);
		if (Error || Root.empty()) return MakeError("cannot resolve asset folder");
		const auto Safe = DiscAssets::SafePath(Root, SourceId);
		if (!Safe) return Safe.Error();
		auto State = std::make_unique<FState>(Root / SourceId, Image);
		std::map<uint32, std::pair<int32, FMovieLocation>> Movies;
		for (int32 Movie = 0; const auto Location = FindMovie(Index, Movie); ++Movie)
		{
			Movies.emplace(Location->FirstLba, std::pair{Movie, *Location});
		}
		std::ostringstream Manifest;
		Manifest << "weltall-assets\t1\nsource\t" << SourceId << "\nsector-bytes\t"
			<< (Image.IsRaw() ? CdSector::RawSize : CdSector::CookedSize) << "\nimage-sectors\t" << Image.SectorCount()
			<< "\nrecord\tlba\tdeclared-bytes\tstored-bytes\tpath\n";
		for (std::size_t Record = 0; Record < Index.Records().size(); ++Record)
		{
			const FIndexRecord& Indexed = Index.Records()[Record];
			if (!Indexed.IsFile())
			{
				Manifest << Record << '\t' << Indexed.Lba << '\t' << Indexed.Size << "\t0\t-\n";
				continue;
			}
			const auto Movie = Movies.find(Indexed.Lba);
			const bool bMovie = Movie != Movies.end();
			const uint64 Bytes = bMovie ? static_cast<uint64>(Movie->second.second.SectorCount) *
				(Image.IsRaw() ? CdSector::RawSize : CdSector::CookedSize) :
				(static_cast<uint64>(Indexed.Size) + CdSector::Form1DataSize - 1) / CdSector::Form1DataSize * CdSector::Form1DataSize;
			const auto Path = bMovie ? std::filesystem::path("movies") / (Number(static_cast<uint32>(Movie->second.first), 2) + ".str") : FilePath(Index, Record);
			State->Entries.push_back({Indexed, Path, Bytes, bMovie});
			if (bMovie) State->Movies.emplace(Indexed.Lba, State->Entries.back());
			Manifest << Record << '\t' << Indexed.Lba << '\t' << Indexed.Size << '\t' << Bytes << '\t' << Path.generic_string() << '\n';
		}
		const auto Layout = Manifest.str();
		State->Manifest = "layout-sha1\t" + FSha1::ToHex(FSha1::Hash(std::span<const uint8>(reinterpret_cast<const uint8*>(Layout.data()), Layout.size()))) + "\n" + Layout;
		const auto Existing = DiscAssets::ValidateManifest(State->Folder, State->Manifest);
		if (!Existing) return Existing.Error();
		return std::shared_ptr<FDiscAssetStore>(new FDiscAssetStore(std::move(State)));
	}

	void FDiscAssetStore::SetEnabled(bool bEnabled)
	{
		if (State->bEnabled != bEnabled) State->MovieFiles.clear();
		State->bEnabled = bEnabled;
	}

	bool FDiscAssetStore::IsEnabled() const { return State->bEnabled; }
	std::filesystem::path FDiscAssetStore::Folder() const { return State->Folder; }

	std::filesystem::path FDiscAssetStore::AssetPath(const FIndexRecord& Record) const
	{
		const auto Entry = std::find_if(State->Entries.begin(), State->Entries.end(), [&](const FDiscAssetEntry& Candidate) { return Candidate.Record == Record; });
		if (Entry == State->Entries.end()) return {};
		return State->Folder / Entry->Path;
	}

	void FDiscAssetStore::SetMusic(std::shared_ptr<const FMusicReplacements> Music)
	{
		State->Music = std::move(Music);
	}

	TResult<std::optional<std::vector<uint8>>> FDiscAssetStore::ReadMusic(uint64 Key) const
	{
		if (!State->bEnabled || !State->Music) return std::optional<std::vector<uint8>>{};
		return State->Music->Read(Key);
	}

	void FDiscAssetStore::SetMessages(std::shared_ptr<const FMessageOverrides> Messages)
	{
		State->Messages = std::move(Messages);
	}

	std::shared_ptr<const FMessageOverrides> FDiscAssetStore::Messages() const
	{
		return State->bEnabled ? State->Messages : nullptr;
	}

	void FDiscAssetStore::SetFontSheet(std::shared_ptr<const FMesFontSheet> Sheet)
	{
		State->FontSheet = std::move(Sheet);
	}

	std::shared_ptr<const FMesFontSheet> FDiscAssetStore::FontSheet() const
	{
		return State->bEnabled ? State->FontSheet : nullptr;
	}

	TResult<std::optional<std::vector<uint8>>> FDiscAssetStore::ReadOverride(const FIndexRecord& Record) const
	{
		if (!State->bEnabled) return std::optional<std::vector<uint8>>{};
		const auto Path = AssetPath(Record);
		if (Path.empty()) return std::optional<std::vector<uint8>>{};
		const auto Safe = DiscAssets::SafePath(State->Folder, Path.lexically_relative(State->Folder), true);
		if (!Safe) return Safe.Error();
		if (!*Safe) return std::optional<std::vector<uint8>>{};
		std::error_code Error;
		const bool bExists = std::filesystem::exists(Path, Error);
		if (Error) return MakeError("cannot inspect external asset: " + Error.message());
		if (!bExists) return std::optional<std::vector<uint8>>{};
		const auto File = FFileByteSource::Open(Path);
		if (!File) return File.Error();
		if ((*File)->Size() == 0 || (*File)->Size() % CdSector::Form1DataSize != 0 || (*File)->Size() > 512ULL * 1024 * 1024)
		{
			return MakeError("external asset has an invalid size: " + Path.string());
		}
		std::vector<uint8> Bytes(static_cast<std::size_t>((*File)->Size()));
		if (!(*File)->Read(0, Bytes)) return MakeError("cannot read external asset: " + Path.string());
		return std::optional{std::move(Bytes)};
	}

	TResult<bool> FDiscAssetStore::ReadMovieSector(uint32 Lba, std::span<uint8> Destination) const
	{
		if (!State->bEnabled) return false;
		auto Movie = State->Movies.upper_bound(Lba);
		if (Movie == State->Movies.begin()) return false;
		const auto& Entry = std::prev(Movie)->second;
		if (Destination.empty() || static_cast<uint64>(Lba - Entry.Record.Lba) * Destination.size() >= Entry.Bytes) return false;
		auto Found = State->MovieFiles.find(Entry.Record.Lba);
		if (Found == State->MovieFiles.end())
		{
			const auto Safe = DiscAssets::SafePath(State->Folder, Entry.Path, true);
			if (!Safe) return Safe.Error();
			std::error_code Error;
			const bool bExists = *Safe && std::filesystem::exists(State->Folder / Entry.Path, Error);
			if (Error) return MakeError("cannot inspect movie replacement: " + Error.message());
			std::shared_ptr<const IByteSource> Source;
			if (bExists)
			{
				auto File = FFileByteSource::Open(State->Folder / Entry.Path);
				if (!File) return File.Error();
				if ((*File)->Size() != Entry.Bytes) return MakeError("movie replacement has an invalid sector count: " + Entry.Path.generic_string());
				Source = std::move(*File);
			}
			Found = State->MovieFiles.emplace(Entry.Record.Lba, std::move(Source)).first;
		}
		if (!Found->second) return false;
		if (!Found->second->Read(static_cast<uint64>(Lba - Entry.Record.Lba) * Destination.size(), Destination)) return MakeError("cannot read movie replacement");
		return true;
	}

	TResult<std::unique_ptr<FDiscAssetExport>> FDiscAssetStore::Export() const
	{
		const auto Ready = DiscAssets::WriteManifest(State->Folder, State->Manifest);
		if (!Ready) return Ready.Error();
		auto Job = std::make_unique<FDiscAssetExport::FState>(State->Folder, State->Image, State->Entries);
		Job->Progress.TotalFiles = static_cast<uint32>(State->Entries.size());
		for (const auto& Entry : State->Entries) Job->Progress.TotalBytes += Entry.Bytes;
		return std::unique_ptr<FDiscAssetExport>(new FDiscAssetExport(std::move(Job)));
	}
}
