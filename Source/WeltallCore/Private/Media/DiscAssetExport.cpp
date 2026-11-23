#include "Media/DiscAssetsInternal.h"

#include "Foundation/WholeFiles.h"

#include <algorithm>

namespace Weltall
{
	FDiscAssetExport::FDiscAssetExport(std::unique_ptr<FState> InState) : State(std::move(InState)) {}

	FDiscAssetExport::~FDiscAssetExport()
	{
		if (!State->Progress.bComplete) Cancel();
	}

	const FDiscAssetExportStatus& FDiscAssetExport::Status() const { return State->Progress; }

	void FDiscAssetExport::Cancel()
	{
		if (State->Progress.bComplete) return;
		State->Output.close();
		if (!State->Partial.empty())
		{
			std::error_code Error;
			std::filesystem::remove(State->Partial, Error);
			State->Partial.clear();
		}
		State->Progress.bCancelled = true;
	}

	TResult<bool> FDiscAssetExport::Step(uint64 MaxBytes)
	{
		if (State->Progress.bComplete || State->Progress.bCancelled) return true;
		uint64 Copied = 0;
		uint32 Visited = 0;
		while (State->Next < State->Entries.size() && Visited < 64 && (Copied < MaxBytes || Copied == 0))
		{
			const auto& Entry = State->Entries[State->Next];
			State->Progress.CurrentFile = Entry.Path.generic_string();
			if (!State->Output.is_open())
			{
				const auto Ready = DiscAssets::PrepareFolder(State->Folder, Entry.Path);
				if (!Ready) return Ready.Error();
				std::error_code Error;
				const bool bExists = std::filesystem::exists(State->Folder / Entry.Path, Error);
				if (Error) return MakeError("cannot inspect exported asset: " + Error.message());
				if (bExists)
				{
					if (!std::filesystem::is_regular_file(State->Folder / Entry.Path, Error) || Error) return MakeError("asset output is not a regular file");
					State->Progress.CompletedBytes += Entry.Bytes;
					++State->Progress.CompletedFiles;
					++State->Next;
					++Visited;
					continue;
				}
				auto Partial = Entry.Path;
				Partial += ".partial";
				const auto PartialReady = DiscAssets::PrepareFolder(State->Folder, Partial);
				if (!PartialReady) return PartialReady.Error();
				State->Partial = State->Folder / Partial;
				State->Output.open(State->Partial, std::ios::binary | std::ios::trunc);
				if (!State->Output) return MakeError("cannot create asset output: " + Entry.Path.generic_string());
				State->Written = 0;
			}
			const uint32 SectorBytes = Entry.bMovie ? (State->Image.IsRaw() ? CdSector::RawSize : CdSector::CookedSize) : CdSector::Form1DataSize;
			const uint64 Budget = std::max<uint64>(SectorBytes, MaxBytes > Copied ? MaxBytes - Copied : 0);
			const uint64 Take = std::min(Entry.Bytes - State->Written, Budget / SectorBytes * SectorBytes);
			const uint32 Lba = Entry.Record.Lba + static_cast<uint32>(State->Written / SectorBytes);
			if (Entry.bMovie)
			{
				for (uint32 Sector = 0; Sector < Take / SectorBytes; ++Sector)
				{
					const auto Bytes = State->Image.ReadSourceSector(Lba + Sector);
					if (!Bytes) return Bytes.Error();
					State->Output.write(reinterpret_cast<const char*>(Bytes->data()), static_cast<std::streamsize>(Bytes->size()));
				}
			}
			else
			{
				const auto Bytes = State->Image.ReadForm1(Lba, Take);
				if (!Bytes) return Bytes.Error();
				State->Output.write(reinterpret_cast<const char*>(Bytes->data()), static_cast<std::streamsize>(Bytes->size()));
			}
			if (!State->Output) return MakeError("cannot write asset output: " + Entry.Path.generic_string());
			State->Written += Take;
			Copied += Take;
			State->Progress.CompletedBytes += Take;
			if (State->Written == Entry.Bytes)
			{
				State->Output.close();
				if (!State->Output) return MakeError("cannot finish asset output: " + Entry.Path.generic_string());
				const auto Published = PublishNewFile(State->Partial, State->Folder / Entry.Path);
				if (!Published) return Published.Error();
				State->Partial.clear();
				++State->Progress.CompletedFiles;
				++State->Next;
				++Visited;
			}
		}
		State->Progress.bComplete = State->Next == State->Entries.size();
		return State->Progress.bComplete;
	}
}
