#include "Rendering/TextureModLibrary.h"

#include "Async/Async.h"
#include "Graphics/TextureRegion.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Rendering/TexturePngFiles.h"
#include "TextureModFiles.h"

const TCHAR* FTextureModLibrary::OriginalsFolder = TEXT("original");

namespace
{
	constexpr int64 MaxCacheBytes = 256LL * 1024 * 1024;
	constexpr int32 MaxCaptureJobs = 8;
	constexpr int32 KeptEntries = 16384;

	struct FLoadedPicture
	{
		TSharedPtr<const FTextureModImage> Image;
		/** Kept while a renderer holds it, after the cache let it go. */
		TWeakPtr<const FTextureModImage> Resident;
		int32 Width = 0;
		int32 Height = 0;
		bool bFailed = false;
		/** Being read. */
		bool bPending = false;
		uint64 LastUse = 0;
	};

	/** A reading on a worker thread, and the keys waiting for it: the pictures of one sheet share one. */
	struct FDecodeJob
	{
		TFuture<FTextureModFiles::FDecoded> Result;
		TArray<uint64> Waiting;
		int32 Sheet = INDEX_NONE;
	};

	FString TextureFolder;
	std::filesystem::path CanonicalFolder;
	bool bReplacements = false;
	bool bCaptures = false;
	uint8 PackKinds = 0xFF;
	uint64 ConfigGeneration = 0;
	uint64 Uses = 0;
	int64 CacheBytes = 0;
	FTextureModFiles Files;
	/** Originals on disk or queued, which capture leaves alone. */
	TSet<uint64> Saved;
	TMap<uint64, FLoadedPicture> Loaded;
	TArray<TFuture<void>> CaptureJobs;
	TArray<TSharedRef<FDecodeJob>> DecodeJobs;
	uint64 ReadingsStarted = 0;
	IImageWrapperModule* Wrappers = nullptr;

	void FindOriginals()
	{
		Saved.Empty();
		TArray<FString> Originals;
		IFileManager::Get().FindFilesRecursive(Originals, *(TextureFolder / FTextureModLibrary::OriginalsFolder), TEXT("*.png"), true, false);
		for (const FString& File : Originals)
		{
			if (const auto Key = Weltall::AssetKeyOfName(TCHAR_TO_UTF8(*FPaths::GetBaseFilename(File)))) Saved.Add(*Key);
		}
	}

	void EvictFor(int64 Incoming)
	{
		while (CacheBytes + Incoming > MaxCacheBytes)
		{
			uint64* Oldest = nullptr;
			uint64 OldestUse = MAX_uint64;
			for (auto& Candidate : Loaded)
			{
				if (Candidate.Value.Image && Candidate.Value.LastUse < OldestUse) { Oldest = &Candidate.Key; OldestUse = Candidate.Value.LastUse; }
			}
			if (Oldest == nullptr) return;
			FLoadedPicture& Evicted = Loaded[*Oldest];
			CacheBytes -= Evicted.Image->Rgba.Num();
			Evicted.Image.Reset();
		}
	}

	/** Keeps a decoded picture in the cache under its key, making room for it first. */
	void Keep(uint64 Key, const TSharedRef<FTextureModImage>& Image)
	{
		EvictFor(Image->Rgba.Num());
		FLoadedPicture& Entry = Loaded.FindOrAdd(Key);
		if (Entry.Image) CacheBytes -= Entry.Image->Rgba.Num();
		CacheBytes += Image->Rgba.Num();
		Entry.Image = Image;
		Entry.Resident = Image;
		Entry.Width = Image->Width;
		Entry.Height = Image->Height;
		Entry.bFailed = false;
		Entry.LastUse = ++Uses;
	}

	bool HoldsImage(const FLoadedPicture& Entry)
	{
		return Entry.Image || Entry.Resident.IsValid();
	}

	/** What the cache knows of Key's picture, into Lookup: false when it has to be read. */
	bool LookUp(uint64 Key, int64 MaxPixels, FTextureModLookup& Lookup)
	{
		FLoadedPicture* Entry = Loaded.Find(Key);
		if (Entry == nullptr) return false;
		Entry->LastUse = ++Uses;
		Lookup.bPending = Entry->bPending;
		if (Entry->bPending || Entry->bFailed) return true;
		if (auto Resident = Entry->Image ? Entry->Image : Entry->Resident.Pin())
		{
			Lookup.Image = MoveTemp(Resident);
			return true;
		}
		Lookup.bOverBudget = Entry->Width != 0 && static_cast<int64>(Entry->Width) * Entry->Height > MaxPixels;
		return Lookup.bOverBudget;
	}

	void ForgetUnused()
	{
		if (Loaded.Num() < KeptEntries) return;
		for (auto It = Loaded.CreateIterator(); It; ++It) if (!HoldsImage(It.Value()) && !It.Value().bPending) It.RemoveCurrent();
	}

	/** Reads Key's picture on a worker thread with Reader, or waits for the reading of its sheet already under way. */
	void StartDecode(uint64 Key, int32 Sheet, TFunction<FTextureModFiles::FDecoded()> Reader)
	{
		FLoadedPicture& Entry = Loaded.FindOrAdd(Key);
		Entry.bPending = true;
		Entry.LastUse = ++Uses;
		if (Sheet != INDEX_NONE)
		{
			for (const TSharedRef<FDecodeJob>& Job : DecodeJobs)
			{
				if (Job->Sheet == Sheet)
				{
					Job->Waiting.Add(Key);
					return;
				}
			}
		}
		const TSharedRef<FDecodeJob> Job = MakeShared<FDecodeJob>();
		Job->Sheet = Sheet;
		Job->Waiting.Add(Key);
		Job->Result = Async(EAsyncExecution::ThreadPool, MoveTemp(Reader));
		DecodeJobs.Add(Job);
		++ReadingsStarted;
	}

	/** Keeps what a reading gave: its pictures, and for a key waiting that it did not give, why. */
	void TakeDecoded(FDecodeJob& Job)
	{
		FTextureModFiles::FDecoded Decoded = Job.Result.Consume();
		for (const auto& [Key, Image] : Decoded.Others)
		{
			const FLoadedPicture* Entry = Loaded.Find(Key);
			if (Entry == nullptr || !HoldsImage(*Entry)) Keep(Key, Image);
		}
		if (Decoded.Read.Image) Keep(Decoded.Key, Decoded.Read.Image.ToSharedRef());
		for (const uint64 Key : Job.Waiting)
		{
			FLoadedPicture& Entry = Loaded.FindOrAdd(Key);
			Entry.bPending = false;
			if (HoldsImage(Entry)) continue;
			// Its file failed or was too large; a sheet's other key, the sheet failed.
			const bool bAsked = Key == Decoded.Key;
			Entry.bFailed = !bAsked || Decoded.Read.bFailed;
			Entry.Width = bAsked ? Decoded.Read.Width : 0;
			Entry.Height = bAsked ? Decoded.Read.Height : 0;
		}
	}
}

void FTextureModLibrary::Configure(FString Folder, bool bEnabled, bool bCapture, uint8 ShownPackKinds)
{
	if (TextureFolder == Folder && bReplacements == bEnabled && bCaptures == bCapture && PackKinds == ShownPackKinds) return;
	WaitForCaptures();
	WaitForDecodes();
	if (Wrappers == nullptr)
	{
		Wrappers = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		FCoreDelegates::OnPreExit.AddStatic(&FTextureModLibrary::WaitForCaptures);
		FCoreDelegates::OnPreExit.AddLambda([]() { WaitForDecodes(); });
	}
	TextureFolder = MoveTemp(Folder);
	CanonicalFolder = CanonicalTextureRoot(TextureFolder);
	bReplacements = bEnabled;
	bCaptures = bCapture;
	PackKinds = ShownPackKinds;
	Files.ShowPackKinds(PackKinds);
	++ConfigGeneration;
	Loaded.Empty();
	CacheBytes = 0;
	if (bReplacements) Files.Find(*Wrappers, TextureFolder, CanonicalFolder, OriginalsFolder); else Files.Empty();
	if (bCaptures) FindOriginals(); else Saved.Empty();
}

uint64 FTextureModLibrary::Generation() { return ConfigGeneration; }
bool FTextureModLibrary::IsActive() { return bReplacements || bCaptures; }
bool FTextureModLibrary::IsReplacing() { return bReplacements; }
bool FTextureModLibrary::IsCapturing() { return bCaptures; }

FTextureModLookup FTextureModLibrary::Find(uint64 Key, int32 Width, int32 Height, int64 MaxPixels)
{
	FTextureModLookup Lookup;
	if (!bReplacements || !Files.Contains(Key) || LookUp(Key, MaxPixels, Lookup)) return Lookup;
	ForgetUnused();
	StartDecode(Key, Files.SheetOf(Key), Files.Reader(Key, Width, Height, MaxPixels));
	Lookup.bPending = true;
	return Lookup;
}

FTextureModLookup FTextureModLibrary::FindPacked(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, uint64 RegionKey, int64 MaxPixels)
{
	FTextureModLookup Lookup;
	if (!bReplacements || Files.Packs().Num() == 0) return Lookup;
	TOptional<FBeetleTexturePacks::FCover> Cover = Files.Packs().Cover(Vram, Region, RegionKey);
	if (!Cover) return Lookup;
	const uint64 Key = Cover->Key;
	if (LookUp(Key, MaxPixels, Lookup)) return Lookup;
	ForgetUnused();
	StartDecode(Key, INDEX_NONE, [Key, Make = Files.Packs().Maker(*Wrappers, CanonicalFolder, Vram, Region, MoveTemp(*Cover), MaxPixels)]()
	{
		const TSharedPtr<FTextureModImage> Made = Make();
		return FTextureModFiles::FDecoded{Key, {Made, Made ? Made->Width : 0, Made ? Made->Height : 0, !Made}, {}};
	});
	Lookup.bPending = true;
	return Lookup;
}

void FTextureModLibrary::AddPackedPartRegions(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, TArray<Weltall::FTextureRegion>& Regions)
{
	if (bReplacements && Files.Packs().Num() != 0) Files.Packs().AddPartRegions(Vram, Region, Regions);
}

uint64 FTextureModLibrary::DecodesStarted() { return ReadingsStarted; }

void FTextureModLibrary::CollectDecodes()
{
	for (int32 Index = 0; Index < DecodeJobs.Num();)
	{
		if (!DecodeJobs[Index]->Result.IsReady())
		{
			++Index;
			continue;
		}
		const TSharedRef<FDecodeJob> Job = DecodeJobs[Index];
		DecodeJobs.RemoveAt(Index, EAllowShrinking::No);
		TakeDecoded(*Job);
	}
}

bool FTextureModLibrary::WaitForDecodes(double Seconds)
{
	const double Until = FPlatformTime::Seconds() + Seconds;
	for (const TSharedRef<FDecodeJob>& Job : DecodeJobs)
	{
		if (Seconds < 0.0)
		{
			Job->Result.Wait();
			continue;
		}
		const double Left = Until - FPlatformTime::Seconds();
		if (Left <= 0.0 || !Job->Result.WaitFor(FTimespan::FromSeconds(Left))) break;
	}
	CollectDecodes();
	return DecodeJobs.IsEmpty();
}

bool FTextureModLibrary::NeedsCapture(uint64 Key)
{
	return bCaptures && !Saved.Contains(Key);
}

bool FTextureModLibrary::HasCaptureRoom()
{
	CaptureJobs.RemoveAll([](const TFuture<void>& Job) { return Job.IsReady(); });
	return CaptureJobs.Num() < MaxCaptureJobs;
}

void FTextureModLibrary::Capture(uint64 Key, const Weltall::FImageRgba8& Original)
{
	if (!NeedsCapture(Key)) return;
	Saved.Add(Key);
	const FString File = TextureFolder / OriginalsFolder / TEXT("captured") / (FString(UTF8_TO_TCHAR(Weltall::AssetKeyName(Key).c_str())) + TEXT(".png"));
	TArray<uint8> Pixels(Original.Pixels.data(), static_cast<int32>(Original.Pixels.size()));
	CaptureJobs.Add(Async(EAsyncExecution::ThreadPool, [File, Root = CanonicalFolder, Pixels = MoveTemp(Pixels), Width = Original.Width,
		Height = Original.Height, Module = Wrappers]()
	{
		WriteTexturePng(*Module, File, Root, Pixels.GetData(), Width, Height);
	}));
}

void FTextureModLibrary::WaitForCaptures()
{
	for (auto& Job : CaptureJobs) Job.Wait();
	CaptureJobs.Empty();
}
