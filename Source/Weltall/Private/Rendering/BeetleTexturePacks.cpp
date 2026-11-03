#include "BeetleTexturePacks.h"

#include "Foundation/AssetKey.h"
#include "Foundation/Sha1.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Rendering/TextureModLibrary.h"
#include "Rendering/TexturePngFiles.h"
#include "WeltallLog.h"

#include <cmath>

namespace
{
	constexpr int64 MaxPictureBytes = 128LL * 1024 * 1024;
	/** The pictures kept decoded for the regions that share them: a battle HUD's colouring is 13 MB. */
	constexpr int64 KeptPictureBytes = 192LL * 1024 * 1024;

	uint64 FileKey(const Weltall::FBeetleTextureName& Name)
	{
		return static_cast<uint64>(Name.Upload) << 32 | Name.Palette;
	}

	int32 TexelsInUnit(const Weltall::FTextureRegion& Region)
	{
		const Weltall::ETextureDepth Depth = Weltall::TextureViewPage(Region.View).Depth;
		return Depth == Weltall::ETextureDepth::Indexed4 ? 4 : Depth == Weltall::ETextureDepth::Indexed8 ? 2 : 1;
	}

	/** A pack's picture as RGBA, in whichever format the engine reads. */
	TSharedPtr<const FTextureModImage> ReadPicture(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const FString& File)
	{
		const int64 Bytes = IFileManager::Get().FileSize(*File);
		TArray<uint8> Compressed;
		if (!IsSafeTexturePath(File, Root))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Texture replacement inside a linked folder, which is not read (copy the folder instead): %s"), *File);
			return nullptr;
		}
		if (Bytes <= 0 || Bytes > MaxPictureBytes || !FFileHelper::LoadFileToArray(Compressed, *File))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Cannot read texture replacement: %s"), *File);
			return nullptr;
		}
		const EImageFormat Format = Wrappers.DetectImageFormat(Compressed.GetData(), Compressed.Num());
		// The engine's BMP and TGA readers give only BGRA.
		const bool bBgra = Format == EImageFormat::BMP || Format == EImageFormat::TGA;
		const bool bKnown = bBgra || Format == EImageFormat::PNG || Format == EImageFormat::JPEG;
		const TSharedPtr<IImageWrapper> Wrapper = bKnown ? Wrappers.CreateImageWrapper(Format) : nullptr;
		TSharedRef<FTextureModImage> Picture = MakeShared<FTextureModImage>();
		bool bDecoded = Wrapper.IsValid() && Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()) &&
			Wrapper->GetWidth() * Wrapper->GetHeight() * 4 <= MAX_int32 && Wrapper->GetRaw(bBgra ? ERGBFormat::BGRA : ERGBFormat::RGBA, 8, Picture->Rgba);
		if (bDecoded)
		{
			Picture->Width = static_cast<int32>(Wrapper->GetWidth());
			Picture->Height = static_cast<int32>(Wrapper->GetHeight());
			if (bBgra)
			{
				for (int32 At = 0; At + 3 < Picture->Rgba.Num(); At += 4) Swap(Picture->Rgba[At], Picture->Rgba[At + 2]);
			}
		}
		if (!bDecoded || Picture->Width <= 0 || Picture->Height <= 0 || Picture->Rgba.Num() != static_cast<int64>(Picture->Width) * Picture->Height * 4)
		{
			UE_LOG(LogWeltall, Warning, TEXT("Cannot decode texture replacement: %s"), *File);
			return nullptr;
		}
		return Picture;
	}
}

/**
 * The packs' pictures read lately, shared by the makers on worker threads: one reading of a file at a time, the others
 * asking for it waiting for that one, the least lately used let go past a budget. A picture that failed stays failed.
 */
class FBeetlePictureCache
{
public:
	TSharedPtr<const FTextureModImage> Get(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const FString& File)
	{
		TSharedPtr<FEntry> Entry;
		{
			FScopeLock Lock(&Guard);
			TSharedPtr<FEntry>& Found = Entries.FindOrAdd(File);
			if (!Found) Found = MakeShared<FEntry>();
			Entry = Found;
			Entry->LastUse = ++Uses;
		}
		FScopeLock Reading(&Entry->Reading);
		if (Entry->bRead) return Entry->Picture;
		Entry->Picture = ReadPicture(Wrappers, Root, File);
		Entry->bRead = true;
		if (Entry->Picture)
		{
			FScopeLock Lock(&Guard);
			if (Entries.FindRef(File) == Entry)
			{
				Entry->Bytes = Entry->Picture->Rgba.Num();
				Total += Entry->Bytes;
				Trim(File);
			}
		}
		return Entry->Picture;
	}

private:
	struct FEntry
	{
		FCriticalSection Reading;
		TSharedPtr<const FTextureModImage> Picture;
		bool bRead = false;
		int64 Bytes = 0;
		uint64 LastUse = 0;
	};

	void Trim(const FString& Keep)
	{
		while (Total > KeptPictureBytes)
		{
			const FString* Oldest = nullptr;
			uint64 OldestUse = MAX_uint64;
			for (const auto& [File, Entry] : Entries)
			{
				if (Entry->Bytes > 0 && Entry->LastUse < OldestUse && File != Keep)
				{
					Oldest = &File;
					OldestUse = Entry->LastUse;
				}
			}
			if (Oldest == nullptr) return;
			Total -= Entries[*Oldest]->Bytes;
			Entries.Remove(FString(*Oldest));
		}
	}

	FCriticalSection Guard;
	TMap<FString, TSharedPtr<FEntry>> Entries;
	int64 Total = 0;
	uint64 Uses = 0;
};

FBeetleTexturePacks::FBeetleTexturePacks() : Pictures(MakeShared<FBeetlePictureCache>()) {}

FBeetleTexturePacks::~FBeetleTexturePacks() = default;

void FBeetleTexturePacks::Empty()
{
	Files.Empty();
	Used.Empty();
	Pictures = MakeShared<FBeetlePictureCache>();
}

bool FBeetleTexturePacks::Add(const Weltall::FBeetleTextureName& Name, const FString& File, int32 Depth)
{
	FFile* Taken = Files.Find(FileKey(Name));
	if (Taken == nullptr)
	{
		Files.Add(FileKey(Name), {File, Depth});
		return true;
	}
	if (Depth < Taken->Depth) *Taken = {File, Depth};
	return false;
}

const FBeetleTexturePacks::FFile* FBeetleTexturePacks::FileOf(const Weltall::FBeetleTexturePart& Part, Weltall::FBeetleTextureName& Name) const
{
	if ((ShownKinds & (1u << static_cast<uint32>(Part.Kind))) == 0) return nullptr;
	// A pack made with Beetle's Reduce Palette Range option names a part by the colours its upload uses, where it has such a file.
	Name = {Part.Upload, Part.Palette};
	const FFile* File = Files.Find(FileKey(Name));
	if (File == nullptr && Part.ReducedPalette != Part.Palette)
	{
		Name.Palette = Part.ReducedPalette;
		File = Files.Find(FileKey(Name));
	}
	return File;
}

void FBeetleTexturePacks::AddPartRegions(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, TArray<Weltall::FTextureRegion>& Regions) const
{
	if (Files.IsEmpty()) return;
	Weltall::FBeetleTextureName Name;
	for (const Weltall::FBeetleTexturePart& Part : Weltall::BeetleTextureParts(Vram, Region))
	{
		if (FileOf(Part, Name) == nullptr) continue;
		Regions.Add({Region.View, static_cast<uint8>(Region.PictureFirstU() + Part.U), static_cast<uint8>(Region.PictureFirstV() + Part.V),
			static_cast<uint16>(Part.Width), static_cast<uint16>(Part.Height)});
	}
}

TOptional<FBeetleTexturePacks::FCover> FBeetleTexturePacks::Cover(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, uint64 RegionKey) const
{
	if (Files.IsEmpty()) return {};
	FCover Found;
	Weltall::FSha1 Hash;
	Hash.Update(std::span(reinterpret_cast<const uint8*>(&RegionKey), sizeof(RegionKey)));
	for (const Weltall::FBeetleTexturePart& Part : Weltall::BeetleTextureParts(Vram, Region))
	{
		Weltall::FBeetleTextureName Name;
		const FFile* File = FileOf(Part, Name);
		if (File == nullptr) continue;
		const int32 Placed[] = {Part.UploadUnitsWide, Part.UploadUnitsHigh, Part.FromU, Part.FromV, Part.U, Part.V, Part.Width, Part.Height};
		Hash.Update(std::span(reinterpret_cast<const uint8*>(&Name), sizeof(Name)));
		Hash.Update(std::span(reinterpret_cast<const uint8*>(Placed), sizeof(Placed)));
		Found.Parts.Add({File->Path, Part});
		bool bLogged = false;
		Used.Add(FileKey(Name), &bLogged);
		if (!bLogged) UE_LOG(LogWeltall, Verbose, TEXT("Texture pack picture in use: %s"), *File->Path);
	}
	if (Found.Parts.IsEmpty()) return {};
	Found.Key = Weltall::AssetKeyOfDigest(Hash.Finish());
	return Found;
}

TFunction<TSharedPtr<FTextureModImage>()> FBeetleTexturePacks::Maker(IImageWrapperModule& Wrappers, const std::filesystem::path& Root,
	const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, FCover Cover, int64 MaxPixels) const
{
	return [Module = &Wrappers, Root, Pictures = Pictures, Original = Weltall::DecodeTextureRegionKinds(Vram, Region), Cover = MoveTemp(Cover),
		Per = TexelsInUnit(Region), MaxPixels]() -> TSharedPtr<FTextureModImage>
	{
		struct FRead
		{
			const FPart* Part = nullptr;
			TSharedPtr<const FTextureModImage> Picture;
			double ScaleU = 1.0;
			int32 ScaleV = 1;
		};
		TArray<FRead> Reads;
		double Largest = 1.0;
		for (const FPart& Part : Cover.Parts)
		{
			TSharedPtr<const FTextureModImage> Picture = Pictures->Get(*Module, Root, Part.File);
			if (!Picture) continue;
			// Beetle's scale: the picture's width over the upload's in units, and its height over the upload's rows, in whole numbers.
			const int32 PerUnit = Picture->Width / FMath::Max(1, Part.Part.UploadUnitsWide);
			const int32 PerRow = Picture->Height / FMath::Max(1, Part.Part.UploadUnitsHigh);
			if (PerUnit == 0 || PerRow == 0)
			{
				UE_LOG(LogWeltall, Warning, TEXT("Texture pack picture is smaller than its upload's %d x %d units: %s"), Part.Part.UploadUnitsWide,
					Part.Part.UploadUnitsHigh, *Part.File);
				continue;
			}
			const FRead Read{&Part, Picture, static_cast<double>(PerUnit) / Per, PerRow};
			Largest = FMath::Max(Largest, FMath::Max(Read.ScaleU, static_cast<double>(Read.ScaleV)));
			Reads.Add(Read);
		}
		if (Reads.IsEmpty()) return nullptr;
		int32 Scale = FMath::Min(static_cast<int32>(std::ceil(Largest)), MaxScale);
		while (Scale > 1 && static_cast<int64>(Original.Width) * Scale * Original.Height * Scale > MaxPixels) --Scale;
		TSharedRef<FTextureModImage> Made = MakeShared<FTextureModImage>();
		Made->Width = Original.Width * Scale;
		Made->Height = Original.Height * Scale;
		Made->bKindsFromAlpha = true;
		Made->Rgba.SetNumUninitialized(static_cast<int64>(Made->Width) * Made->Height * 4);
		uint8* Pixel = Made->Rgba.GetData();
		for (int32 Y = 0; Y < Made->Height; ++Y)
		{
			for (int32 X = 0; X < Made->Width; ++X, Pixel += 4)
			{
				FMemory::Memcpy(Pixel, &Original.Pixels[(static_cast<std::size_t>(Y / Scale) * Original.Width + X / Scale) * 4], 4);
			}
		}
		for (const FRead& Read : Reads)
		{
			const Weltall::FBeetleTexturePart& Part = Read.Part->Part;
			const FTextureModImage& Picture = *Read.Picture;
			for (int32 Y = Part.V * Scale; Y < (Part.V + Part.Height) * Scale; ++Y)
			{
				const double V = Part.FromV + (Y - Part.V * Scale + 0.5) / Scale;
				const int32 Row = FMath::Min(static_cast<int32>(V * Read.ScaleV), Picture.Height - 1);
				uint8* Out = &Made->Rgba[(static_cast<int64>(Y) * Made->Width + Part.U * Scale) * 4];
				for (int32 X = Part.U * Scale; X < (Part.U + Part.Width) * Scale; ++X, Out += 4)
				{
					const double U = Part.FromU + (X - Part.U * Scale + 0.5) / Scale;
					const int32 Column = FMath::Min(static_cast<int32>(U * Read.ScaleU), Picture.Width - 1);
					const uint8* In = &Picture.Rgba[(static_cast<int64>(Row) * Picture.Width + Column) * 4];
					const bool bDrawn = In[3] != 0;
					Out[0] = bDrawn ? In[0] : 0;
					Out[1] = bDrawn ? In[1] : 0;
					Out[2] = bDrawn ? In[2] : 0;
					Out[3] = !bDrawn ? Weltall::TexelKindAlpha::Transparent : In[3] == 255 ? Weltall::TexelKindAlpha::Plain : Weltall::TexelKindAlpha::SemiTransparent;
				}
			}
		}
		return Made;
	};
}
