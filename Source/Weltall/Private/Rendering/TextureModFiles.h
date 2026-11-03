#pragma once

#include "BeetleTexturePacks.h"
#include "CoreMinimal.h"
#include "Graphics/TextureSheet.h"

#include <filesystem>

struct FTextureModImage;
class IImageWrapperModule;

/**
 * Where a textures folder's replacements are, by key: PNGs whose names end in their key, and the pictures of sheets (a
 * PNG with its layout beside it, Weltall::FTextureSheetLayout); and the pictures of Beetle PSX HW texture packs, by
 * their names (FBeetleTexturePacks). Game thread, but for the readers it hands out.
 */
class FTextureModFiles
{
public:
	/**
	 * Finds every replacement below Folder but Originals. A key named twice takes the first file by path, a PNG of its own
	 * before a sheet's picture.
	 */
	void Find(IImageWrapperModule& Wrappers, const FString& Folder, const std::filesystem::path& Root, const TCHAR* Originals);
	void Empty();
	bool Contains(uint64 Key) const { return Pngs.Contains(Key) || Cells.Contains(Key); }
	int32 Count() const { return Pngs.Num() + Cells.Num(); }
	const FBeetleTexturePacks& Packs() const { return Beetle; }
	void ShowPackKinds(uint8 Kinds) { Beetle.Show(Kinds); }
	IImageWrapperModule* ImageWrappers() const { return Wrappers; }
	const std::filesystem::path& CanonicalRoot() const { return Root; }

	/** A picture read: its pixels, or its size alone when they would exceed the budget, or a failure. */
	struct FRead
	{
		TSharedPtr<FTextureModImage> Image;
		int32 Width = 0;
		int32 Height = 0;
		bool bFailed = false;
	};

	/** Key's picture read, and every other picture of its sheet when it lies on one. */
	struct FDecoded
	{
		uint64 Key = 0;
		FRead Read;
		TArray<TPair<uint64, TSharedRef<FTextureModImage>>> Others;
	};

	/**
	 * Reads Key's picture for a region Width x Height texels when called, on any thread: a picture of any size up to
	 * MaxScale times the region's each way, its pixels decoded when there are no more than MaxPixels. Unset when Key
	 * has no file.
	 */
	TFunction<FDecoded()> Reader(uint64 Key, int32 Width, int32 Height, int64 MaxPixels) const;
	/** The sheet Key lies on, or INDEX_NONE: its pictures all come from one reading. */
	int32 SheetOf(uint64 Key) const;

	static constexpr int32 MaxScale = 16;

private:
	/** Finds the Beetle PSX HW packs' pictures below Folder but Originals. */
	void FindPacks(const FString& Folder, const TCHAR* Originals);

	struct FSheet
	{
		FString Png;
		Weltall::FTextureSheetLayout Layout;
	};

	IImageWrapperModule* Wrappers = nullptr;
	std::filesystem::path Root;
	TMap<uint64, FString> Pngs;
	TArray<TSharedRef<const FSheet>> Sheets;
	/** Each key a sheet holds: the sheet and its picture. */
	TMap<uint64, TPair<int32, int32>> Cells;
	FBeetleTexturePacks Beetle;
};
