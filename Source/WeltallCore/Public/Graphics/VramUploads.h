#pragma once

#include "Foundation/Types.h"
#include "Graphics/VramRect.h"

#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	/** What an upload holds, as a player tells pictures apart: a texture pack's pictures of a kind may be left out. */
	enum class EVramUploadKind : uint8
	{
		/** Told apart from none: a pack's picture of it always shows. */
		Other,
		Text,
		Portraits,
		MenusAndHud,
		Characters,
		Scenery,
		ModelsAndEffects,
	};

	inline constexpr int32 VramUploadKindCount = 7;

	/** One image the console uploaded into VRAM (LoadImage), as Beetle PSX HW's texture tracker knows it. */
	struct FVramUpload
	{
		/** The CRC-32 of its units, row by row (Crc32OfUnits). */
		uint32 Crc = 0;
		int16 Width = 0;
		int16 Height = 0;
		/** The least and greatest CLUT index its units name, read at 4 bits a texel and at 8. */
		uint8 Least4 = 0;
		uint8 Greatest4 = 0;
		uint8 Least8 = 0;
		uint8 Greatest8 = 0;
		EVramUploadKind Kind = EVramUploadKind::Other;
	};

	/** Part of an upload VRAM still holds: where it lies, and where it begins in its upload. */
	struct FVramUploadPart
	{
		std::shared_ptr<const FVramUpload> Upload;
		/** In the console's rows from X 0-1023 and Y 0-511; like its upload, it may run on past the right or bottom edge. */
		FVramRect Rect;
		int16 FromX = 0;
		int16 FromY = 0;
	};

	/**
	 * Which upload each unit of the console's rows holds, kept as Beetle PSX HW's texture tracker keeps it, since that is
	 * what a texture pack made with it names its pictures by. An upload lays a part over the rectangle it writes, cutting
	 * each part it covers down to the up to four rectangles left of it; a copy carries the parts under its source to its
	 * destination; a fill leaves none. A readback keeps the parts under its rectangle, and an upload of the same units back
	 * to the same rectangle puts them back rather than becoming an upload of its own. An upload of the whole VRAM (a save
	 * state, to the tracker) leaves no part. Rectangles wrap as the GPU's transfers do: from where they start, X within
	 * 0-1023 and Y within 0-511.
	 */
	class FVramUploadTracker
	{
	public:
		WELTALLCORE_API void Upload(const FVramRect& Rect, std::span<const uint16> Units, EVramUploadKind Kind = EVramUploadKind::Other);
		WELTALLCORE_API void Copy(const FVramRect& From, int32 X, int32 Y);
		WELTALLCORE_API void Fill(const FVramRect& Rect);
		/** A readback of Rect, whose units' CRC-32 is Crc. */
		WELTALLCORE_API void Readback(const FVramRect& Rect, uint32 Crc);

		std::span<const FVramUploadPart> Parts() const { return PartList; }

		/** The readbacks kept for their units to come back, past which the oldest is let go. */
		static constexpr std::size_t KeptReadbacks = 32;

	private:
		struct FReadback
		{
			FVramRect Rect;
			uint32 Crc = 0;
			std::vector<FVramUploadPart> Parts;
		};

		void Clear(const FVramRect& Rect);
		void Place(FVramUploadPart Part);

		std::vector<FVramUploadPart> PartList;
		std::vector<FReadback> Readbacks;
	};
}
