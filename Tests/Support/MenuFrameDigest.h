#pragma once

#include "Graphics/Vram.h"
#include "Menu/MenuPrimitive.h"

#include <span>
#include <string>
#include <vector>

namespace Weltall
{
	class FGearShopModel;
}

namespace Weltall::Testing
{
	/**
	 * A menu frame's primitives as the GPU takes them, in the bytes the menu's oracle
	 * makes of each packet: its command, then per corner its colour (a flat
	 * primitive's first only, none for a raw texture), position and texel, then a textured polygon's
	 * CLUT and page; a drawing-mode command's words. Each packet is preceded by its length.
	 */
	std::vector<uint8> CanonicalMenuPackets(const FMenuFrame& Frame);

	/** The uploads a menu makes, as the oracle records LoadImage's: rectangle, then units. */
	class FRecordedUploads final : public IVramUploads
	{
	public:
		void Upload(const FVramImage& Image) override { Images.push_back(Image); }

		std::vector<FVramImage> Images;
	};

	/**
	 * The gear shop's part of a frame, as the oracle adds it: "gear", the shop's camera as the PlayStation's
	 * MATRIX (its rotation, a zero pad, its translation) and the digest of its model's state.
	 */
	std::vector<uint8> GearShopFrameBytes(const FMenuFrame& Frame, const FGearShopModel& Model);

	/** The bytes a frame's digest is made of: its packets, its sounds, its uploads, the gear shop's part. */
	std::vector<uint8> MenuFrameBytes(const FMenuFrame& Frame, std::span<const uint8> Sounds, std::span<const FVramImage> Uploads,
		std::span<const uint8> Gear = {});

	/** A frame's digest as the oracle prints it (8 hex digits): the SHA-1 of MenuFrameBytes. */
	std::string MenuFrameDigest(const FMenuFrame& Frame, std::span<const uint8> Sounds, std::span<const FVramImage> Uploads,
		std::span<const uint8> Gear = {});
}
