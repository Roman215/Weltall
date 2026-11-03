#pragma once

#include "Gears/GearFiles.h"
#include "Gears/GearPackets.h"
#include "Gears/GearSkeleton.h"
#include "Graphics/TexturePictures.h"

#include <memory>
#include <span>
#include <vector>

namespace Weltall
{
	/**
	 * Another model drawn over a loaded model's moving skeleton in its stead: its parts, built for the loaded model's
	 * texture place, and its bones placed over the loaded model's as they stand. The loaded model goes on moving and
	 * playing as itself; only its drawing changes.
	 */
	class IGearDress
	{
	public:
		virtual ~IGearDress() = default;

		/** The worn model, its textures to go up at the loaded model's place. */
		virtual const FGearModelPack& Model() const = 0;
		/** Each of the worn model's parts as its bone draws it, in bone order. */
		virtual const std::vector<FGearBonePackets>& Packets() const = 0;
		/** The worn model's texture regions at that place, with their loaded colours and keys. */
		virtual const std::shared_ptr<const std::vector<FTexturePictureSeed>>& TextureSeeds() const = 0;
		/** The worn model's bones placed over the loaded model's (Performer) as they stand. */
		virtual void Place(std::span<const FGearBone> Performer, std::vector<FGearBone>& Bones) const = 0;
	};
}
