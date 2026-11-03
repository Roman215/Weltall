#pragma once

#include "CoreMinimal.h"
#include "Graphics/Texture.h"

namespace Weltall
{
	class FVram;
	struct FTextureRegion;
}

/** A replacement picture in RGBA byte order, ready for upload on the rendering thread. */
struct FTextureModImage
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Rgba;
	/**
	 * Its alpha is each pixel's kind (Weltall::TexelKindAlpha), as a Beetle PSX HW pack's is. Otherwise a pixel takes the
	 * kind of the original texel under it, its alpha below one half leaving a hole.
	 */
	bool bKindsFromAlpha = false;
};

/** What the library has of a key's picture. */
struct FTextureModLookup
{
	TSharedPtr<const FTextureModImage> Image;
	/** Being read in the background: asked again later, it may be there. */
	bool bPending = false;
	/** Larger than the pixels it was asked within. */
	bool bOverBudget = false;
};

/**
 * Local texture replacements and captures in a textures folder, named by the key of the picture they stand for
 * (Weltall::TextureRegionKey). Replacements are PNGs anywhere below the folder except its originals folder, their
 * names ending in the key; originals, exported or captured, are below "original". The pictures of Beetle PSX HW texture
 * packs below it stand for the uploads they name (FindPacked). Pictures are decoded on worker threads, many at once,
 * and arrive at a later Find. Game thread only.
 */
class WELTALL_API FTextureModLibrary
{
public:
	/** The folder below the textures folder that holds original pictures, which are never read as replacements. */
	static const TCHAR* OriginalsFolder;

	/**
	 * Takes the folder and the switches, and the kinds of upload whose texture packs' pictures show (bit k for
	 * Weltall::EVramUploadKind k); while replacements are on, finds the PNGs the folder holds again.
	 */
	static void Configure(FString Folder, bool bEnabled, bool bCapture, uint8 ShownPackKinds = 0xFF);
	static uint64 Generation();
	/** Replacing or capturing. */
	static bool IsActive();
	static bool IsReplacing();
	static bool IsCapturing();
	/**
	 * The picture standing for Key, a region Width x Height texels: a picture of any size up to 16 times the region's
	 * each way, decoded once and kept while it is drawn. Pending while it is read, the first time it is asked for and
	 * until it arrives; none when there is none, it fails to load, or it would take more than MaxPixels.
	 */
	static FTextureModLookup Find(uint64 Key, int32 Width, int32 Height, int64 MaxPixels);
	/**
	 * The picture Beetle PSX HW texture packs give a region whose own key is RegionKey: the packs' pictures of the uploads
	 * under it, through its view's palette, each laid where its upload's part lies, the rest the region's own texels. As
	 * Find: made on a worker thread, pending until it arrives; none when no pack has a picture for any part of it.
	 */
	static FTextureModLookup FindPacked(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, uint64 RegionKey, int64 MaxPixels);
	/** Adds to Regions a region for each part of Region that a Beetle pack has a picture of (FBeetleTexturePacks::AddPartRegions). */
	static void AddPackedPartRegions(const Weltall::FVram& Vram, const Weltall::FTextureRegion& Region, TArray<Weltall::FTextureRegion>& Regions);
	/** How many readings have been started: a change means a later Find may have something new. */
	static uint64 DecodesStarted();
	/**
	 * Waits for the pictures being read to arrive, at most Seconds when that is not negative; true when every one has.
	 * The arrived ones are there for the next Find.
	 */
	static bool WaitForDecodes(double Seconds = -1.0);
	/** Takes in the pictures that have arrived. */
	static void CollectDecodes();
	/** Whether Key's original is still to be saved: capturing, and no original of it is saved or queued. */
	static bool NeedsCapture(uint64 Key);
	/** Whether the queue of background writes takes another. */
	static bool HasCaptureRoom();
	/** Saves an original picture under original/captured, named by its key, in the background, when NeedsCapture. */
	static void Capture(uint64 Key, const Weltall::FImageRgba8& Original);
	/** Finishes the small queue of PNG writes before exit, and for deterministic verification. */
	static void WaitForCaptures();
};
