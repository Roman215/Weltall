#pragma once

#include "CoreMinimal.h"
#include "Graphics/BeetleTextureNames.h"

#include <filesystem>

struct FTextureModImage;
class IImageWrapperModule;
class FBeetlePictureCache;

/**
 * The pictures of Beetle PSX HW texture packs in a textures folder, and the making of a region's picture from them
 * (Weltall::BeetleTextureParts): each part of an upload under the region that a pack has a picture of, through the
 * view's palette, is cut from that picture and laid where the part lies; whatever no picture covers is the region's own
 * texels. A picture is its whole upload at a whole multiple of its size, the multiple its width over the upload's in
 * units, and its alpha is each pixel's kind, as Beetle reads them: 0 leaves a hole, 255 is drawn opaque and anything
 * between has the console's bit 15, blended where its polygon is semi-transparent. The picture made carries those kinds
 * (FTextureModImage::bKindsFromAlpha), the region's own texels theirs. Game thread, but for the makers it hands out.
 */
class FBeetleTexturePacks
{
public:
	FBeetleTexturePacks();
	~FBeetleTexturePacks();

	/** A part of a region that a pack has a picture of. */
	struct FPart
	{
		FString File;
		Weltall::FBeetleTexturePart Part;
	};

	/** The parts of a region that packs have pictures of, and the key of the picture made from them. */
	struct FCover
	{
		uint64 Key = 0;
		TArray<FPart> Parts;
	};

	void Empty();
	/**
	 * Takes File for its name unless a file nearer the folder's top, or as near and earlier by path, has it, since Beetle
	 * reads only its pack's own folder and packs keep alternatives in folders within it. False when the name was taken.
	 */
	bool Add(const Weltall::FBeetleTextureName& Name, const FString& File, int32 Depth);
	int32 Num() const { return Files.Num(); }

	/** The kinds of upload whose pictures show (bit k for Weltall::EVramUploadKind k); a part of another kind keeps its own texels. */
	void Show(uint8 Kinds) { ShownKinds = Kinds; }

	/** The region's parts that have pictures, unset when none has; RegionKey is the region's own (Weltall::TextureRegionKey). */
	TOptional<FCover> Cover(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, uint64 RegionKey) const;

	/** Adds to Regions a region of the view for each part of Region that has a picture, as the part lies in it. */
	void AddPartRegions(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, TArray<Weltall::FTextureRegion>& Regions) const;

	/**
	 * What makes the region's picture from its Cover on any thread: the region's own texels read now, the packs' pictures
	 * when called (each kept a while for the regions that share it), the whole at the largest of their scales up to 16
	 * times the region's, less where it would take more than MaxPixels. Null when no picture could be read.
	 */
	TFunction<TSharedPtr<FTextureModImage>()> Maker(IImageWrapperModule& Wrappers, const std::filesystem::path& Root, const Weltall::FVram& Vram,
		const Weltall::FTextureRegion& Region, FCover Cover, int64 MaxPixels) const;

	static constexpr int32 MaxScale = 16;

private:
	struct FFile
	{
		FString Path;
		int32 Depth = 0;
	};

	/** The file of a part's picture and the name it has, none where no pack has one. */
	const FFile* FileOf(const Weltall::FBeetleTexturePart& Part, Weltall::FBeetleTextureName& Name) const;

	TMap<uint64, FFile> Files;
	uint8 ShownKinds = 0xFF;
	TSharedRef<FBeetlePictureCache> Pictures;
	/** The files a cover has used, each logged the first time. */
	mutable TSet<uint64> Used;
};
