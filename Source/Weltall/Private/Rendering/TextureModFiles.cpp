#include "TextureModFiles.h"

#include "Algo/AnyOf.h"
#include "Algo/NoneOf.h"
#include "Graphics/TextureRegion.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/TextureModLibrary.h"
#include "Rendering/TexturePngFiles.h"
#include "WeltallLog.h"

namespace
{
	constexpr int64 MaxPngBytes = 128LL * 1024 * 1024;

	TOptional<uint64> KeyOfFile(const FString& File)
	{
		const auto Key = Weltall::AssetKeyOfName(TCHAR_TO_UTF8(*FPaths::GetBaseFilename(File)));
		return Key ? TOptional<uint64>(*Key) : TOptional<uint64>();
	}

	/** Every file of the pattern below the folder but the originals and those hidden or in a hidden folder (Weltall::IsHiddenFile), in a fixed order. */
	TArray<FString> FilesBelow(const FString& Folder, const TCHAR* Originals, const TCHAR* Pattern)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Folder / Pattern), true, false);
		for (FString& File : Files) File = Folder / File;
		TArray<FString> Folders;
		IFileManager::Get().FindFiles(Folders, *(Folder / TEXT("*")), false, true);
		for (const FString& Inner : Folders)
		{
			if (Inner.Equals(Originals, ESearchCase::IgnoreCase)) continue;
			TArray<FString> Inside;
			IFileManager::Get().FindFilesRecursive(Inside, *(Folder / Inner), Pattern, true, false);
			Files.Append(Inside);
		}
		Files.RemoveAll([&Folder](const FString& File) { return (TEXT("/") + File.RightChop(Folder.Len())).Contains(TEXT("/.")); });
		Files.Sort();
		return Files;
	}

	/** Whether a PNG Width x Height pixels may stand for a region Across x Down texels: no more than the largest scale each way. */
	bool Fits(int64 Width, int64 Height, int32 Across, int32 Down)
	{
		return Width >= 1 && Height >= 1 && Width <= static_cast<int64>(Across) * FTextureModFiles::MaxScale && Height <= static_cast<int64>(Down) * FTextureModFiles::MaxScale;
	}

	/** Where texel At of a sheet Texels long lies in its PNG Pixels long: the nearest pixel edge. */
	int32 SheetEdge(int32 At, int32 Pixels, int32 Texels)
	{
		return static_cast<int32>((static_cast<int64>(At) * Pixels * 2 + Texels) / (static_cast<int64>(Texels) * 2));
	}

	TSharedPtr<IImageWrapper> OpenPng(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const FString& File)
	{
		const int64 Bytes = IFileManager::Get().FileSize(*File);
		TArray<uint8> Compressed;
		const TSharedPtr<IImageWrapper> Png = Wrappers.CreateImageWrapper(EImageFormat::PNG);
		if (!IsSafeTexturePath(File, Root) || Bytes <= 0 || Bytes > MaxPngBytes || !FFileHelper::LoadFileToArray(Compressed, *File) || !Png ||
			!Png->SetCompressed(Compressed.GetData(), Compressed.Num()))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Cannot read texture replacement: %s"), *File);
			return nullptr;
		}
		return Png;
	}

	FTextureModFiles::FRead ReadPng(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const FString& File, int32 Width, int32 Height,
		int64 MaxPixels)
	{
		FTextureModFiles::FRead Read;
		const TSharedPtr<IImageWrapper> Png = OpenPng(Wrappers, Root, File);
		if (!Png)
		{
			Read.bFailed = true;
			return Read;
		}
		if (!Fits(Png->GetWidth(), Png->GetHeight(), Width, Height))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Texture replacement must be at most %d times %d x %d pixels: %s"), FTextureModFiles::MaxScale, Width, Height, *File);
			Read.bFailed = true;
			return Read;
		}
		Read.Width = static_cast<int32>(Png->GetWidth());
		Read.Height = static_cast<int32>(Png->GetHeight());
		if (static_cast<int64>(Read.Width) * Read.Height > MaxPixels) return Read;
		TSharedRef<FTextureModImage> Image = MakeShared<FTextureModImage>();
		Image->Width = Read.Width;
		Image->Height = Read.Height;
		if (!Png->GetRaw(ERGBFormat::RGBA, 8, Image->Rgba) || Image->Rgba.Num() != static_cast<int64>(Read.Width) * Read.Height * 4)
		{
			UE_LOG(LogWeltall, Warning, TEXT("Cannot decode texture replacement: %s"), *File);
			Read.bFailed = true;
			return Read;
		}
		Read.Image = Image;
		return Read;
	}

	/** Every picture of a sheet, cut from the PNG at its cells' places scaled as the PNG is to the layout. */
	void ReadSheet(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const FString& File, const Weltall::FTextureSheetLayout& Layout,
		FTextureModFiles::FDecoded& Decoded, int32 Width, int32 Height)
	{
		Decoded.Read.bFailed = true;
		const TSharedPtr<IImageWrapper> Png = OpenPng(Wrappers, Root, File);
		if (Png && !Fits(Png->GetWidth(), Png->GetHeight(), Layout.Width, Layout.Height))
		{
			UE_LOG(LogWeltall, Warning, TEXT("Texture sheet must be at most %d times %d x %d pixels: %s"), FTextureModFiles::MaxScale, Layout.Width, Layout.Height, *File);
			return;
		}
		TArray<uint8> Pixels;
		if (!Png || !Png->GetRaw(ERGBFormat::RGBA, 8, Pixels)) return;
		const int32 SheetWidth = static_cast<int32>(Png->GetWidth());
		const int32 SheetHeight = static_cast<int32>(Png->GetHeight());
		for (const Weltall::FTextureSheetCell& Cell : Layout.Cells)
		{
			const int32 Left = SheetEdge(Cell.X, SheetWidth, Layout.Width);
			const int32 Top = SheetEdge(Cell.Y, SheetHeight, Layout.Height);
			if (Left >= SheetWidth || Top >= SheetHeight) continue;
			const int32 Right = FMath::Clamp(SheetEdge(Cell.X + Cell.Width, SheetWidth, Layout.Width), Left + 1, SheetWidth);
			const int32 Bottom = FMath::Clamp(SheetEdge(Cell.Y + Cell.Height, SheetHeight, Layout.Height), Top + 1, SheetHeight);
			TSharedRef<FTextureModImage> Image = MakeShared<FTextureModImage>();
			Image->Width = Right - Left;
			Image->Height = Bottom - Top;
			Image->Rgba.SetNumUninitialized(Image->Width * Image->Height * 4);
			for (int32 Row = 0; Row < Image->Height; ++Row)
			{
				FMemory::Memcpy(&Image->Rgba[Row * Image->Width * 4], &Pixels[(static_cast<int64>(Top + Row) * SheetWidth + Left) * 4], Image->Width * 4);
			}
			if (Cell.Key != Decoded.Key)
			{
				Decoded.Others.Add({Cell.Key, Image});
				continue;
			}
			if (Cell.Width != Width || Cell.Height != Height)
			{
				UE_LOG(LogWeltall, Warning, TEXT("Texture sheet %s holds key %hs at %d x %d, where it is drawn %d x %d"), *File,
					Weltall::AssetKeyName(Cell.Key).c_str(), Cell.Width, Cell.Height, Width, Height);
				continue;
			}
			Decoded.Read = {Image, Image->Width, Image->Height, false};
		}
	}
}

void FTextureModFiles::Empty()
{
	Pngs.Empty();
	Sheets.Empty();
	Cells.Empty();
	Beetle.Empty();
}

void FTextureModFiles::FindPacks(const FString& Folder, const TCHAR* Originals)
{
	static const TCHAR* const Read[] = {TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp"), TEXT("tga")};
	static const TCHAR* const Unreadable[] = {TEXT("webp"), TEXT("dds")};
	int32 Repeated = 0;
	int32 Unread = 0;
	int32 Pages = 0;
	for (const FString& File : FilesBelow(Folder, Originals, TEXT("*.*")))
	{
		const auto Name = Weltall::BeetleTextureNameOf(TCHAR_TO_UTF8(*FPaths::GetBaseFilename(File)));
		if (!Name) continue;
		const FString Extension = FPaths::GetExtension(File);
		const auto IsExtension = [&Extension](const TCHAR* Format) { return Extension.Equals(Format, ESearchCase::IgnoreCase); };
		if (Algo::NoneOf(Read, IsExtension))
		{
			Unread += Algo::AnyOf(Unreadable, IsExtension) ? 1 : 0;
			continue;
		}
		const FString Within = File.RightChop(Folder.Len());
		// Pages packs name whole VRAM pages rather than uploads.
		if (Within.Contains(TEXT("-texture-replacements-pages/"), ESearchCase::IgnoreCase))
		{
			++Pages;
			continue;
		}
		int32 Depth = 0;
		for (const TCHAR Character : Within) Depth += Character == TEXT('/') ? 1 : 0;
		if (!Beetle.Add(*Name, File, Depth)) ++Repeated;
	}
	if (Beetle.Num() > 0 || Repeated > 0)
	{
		UE_LOG(LogWeltall, Log, TEXT("Texture pack pictures found: %d, and %d more of the same names in folders farther in, not used"), Beetle.Num(), Repeated);
	}
	if (Unread > 0) UE_LOG(LogWeltall, Warning, TEXT("%d texture pack pictures are WEBP or DDS, which are not read: convert them to PNG"), Unread);
	if (Pages > 0) UE_LOG(LogWeltall, Warning, TEXT("%d texture pack pictures name whole VRAM pages (a -texture-replacements-pages folder), which are not read"), Pages);
}

void FTextureModFiles::Find(IImageWrapperModule& InWrappers, const FString& Folder, const std::filesystem::path& InRoot, const TCHAR* Originals)
{
	Empty();
	Wrappers = &InWrappers;
	Root = InRoot;
	int32 Repeated = 0;
	for (const FString& File : FilesBelow(Folder, Originals, TEXT("*.png")))
	{
		const TOptional<uint64> Key = KeyOfFile(File);
		if (!Key) continue;
		if (Pngs.Contains(*Key)) { ++Repeated; continue; }
		Pngs.Add(*Key, File);
	}
	for (const FString& File : FilesBelow(Folder, Originals, TEXT("*.tsv")))
	{
		const FString Png = FPaths::ChangeExtension(File, TEXT("png"));
		FString Text;
		if (!IFileManager::Get().FileExists(*Png) || !IsSafeTexturePath(File, Root) || !FFileHelper::LoadFileToString(Text, *File)) continue;
		auto Layout = Weltall::ReadTextureSheetLayout(TCHAR_TO_UTF8(*Text));
		if (!Layout)
		{
			UE_LOG(LogWeltall, Warning, TEXT("Texture sheet layout %s: %hs"), *File, Layout.Error().Message.c_str());
			continue;
		}
		const int32 Sheet = Sheets.Add(MakeShared<const FSheet>(FSheet{Png, MoveTemp(*Layout)}));
		const std::vector<Weltall::FTextureSheetCell>& Pictures = Sheets[Sheet]->Layout.Cells;
		for (int32 Picture = 0; Picture < static_cast<int32>(Pictures.size()); ++Picture)
		{
			const uint64 Key = Pictures[static_cast<std::size_t>(Picture)].Key;
			if (Contains(Key)) { ++Repeated; continue; }
			Cells.Add(Key, {Sheet, Picture});
		}
	}
	UE_LOG(LogWeltall, Log, TEXT("Texture replacements found: %d, and %d on %d sheets"), Pngs.Num(), Cells.Num(), Sheets.Num());
	if (Repeated > 0) UE_LOG(LogWeltall, Warning, TEXT("%d texture replacements repeat another's key; the first by path is used"), Repeated);
	FindPacks(Folder, Originals);
}

TFunction<FTextureModFiles::FDecoded()> FTextureModFiles::Reader(uint64 Key, int32 Width, int32 Height, int64 MaxPixels) const
{
	if (const TPair<int32, int32>* OnSheet = Cells.Find(Key))
	{
		return [Module = Wrappers, Root = Root, Sheet = Sheets[OnSheet->Key], Key, Width, Height]()
		{
			FDecoded Decoded;
			Decoded.Key = Key;
			ReadSheet(*Module, Root, Sheet->Png, Sheet->Layout, Decoded, Width, Height);
			return Decoded;
		};
	}
	const FString* File = Pngs.Find(Key);
	if (File == nullptr) return {};
	return [Module = Wrappers, Root = Root, File = *File, Key, Width, Height, MaxPixels]()
	{
		return FDecoded{Key, ReadPng(*Module, Root, File, Width, Height, MaxPixels), {}};
	};
}

int32 FTextureModFiles::SheetOf(uint64 Key) const
{
	const TPair<int32, int32>* OnSheet = Cells.Find(Key);
	return OnSheet != nullptr ? OnSheet->Key : INDEX_NONE;
}
