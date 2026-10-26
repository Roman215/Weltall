#pragma once

#include "Field/FieldArchive.h"
#include "Field/FieldWalkmesh.h"
#include "Field/FieldZone.h"
#include "Foundation/Result.h"
#include "Graphics/MdlModel.h"
#include "Graphics/Vram.h"
#include "Text/MessageOverrides.h"

#include <array>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	inline constexpr int32 FieldMapCount = 730;

	/** The bounding sphere the field culls a model by: the centre of its box, twice its longest side. */
	struct FFieldModelBounds
	{
		FGteVector Centre{};
		int16 Radius = 0;
	};

	/** What the field builds for each actor with a model when the map loads. */
	struct FFieldModelObject
	{
		/** The actor's number among the map's actor definitions. */
		uint16 Actor = 0;
		/** The model builder's texture page and CLUT when it built this actor's packets. */
		FMdlTextureState TextureAtBuild;
		FFieldModelBounds Bounds;
	};

	/** A field map as the field program loads it: its archive's header and sections, and its textures. */
	struct FFieldMap
	{
		std::array<FFieldSpriteSlot, FieldSpriteSlotCount> SpriteSlots{};
		FFieldLights Lights;
		std::vector<FFieldActorDefinition> Actors;
		std::vector<FMdlModel> Models;
		/** One per actor with a model, in the order of the actors, which is the order they are built in. */
		std::vector<FFieldModelObject> ModelObjects;
		FFieldWalkmesh Walkmesh;
		std::vector<FFieldZone> Zones;
		/**
		 * The map's images in the order the field uploads them: the archive's TIMs, the sprite images
		 * of the slots that take them from the archive, then every sector of the texture stream.
		 */
		std::vector<FVramImage> TextureUploads;
		/** How many of TextureUploads come from the archive, which the load uploads as it reads the archive. */
		std::size_t ArchiveUploads = 0;
		/** The sections later systems read: sprite sets, event scripts, encounters and messages. */
		std::vector<uint8> SpriteSets;
		std::vector<uint8> EventScripts;
		std::vector<uint8> Encounters;
		std::vector<uint8> Messages;
		/** Which of Messages are the player's (Assets/text), which the windows a script sizes are checked against. */
		FReplacedMessages ReplacedMessages;
	};

	/**
	 * Builds a map from its archive and its texture stream, each its file's sectors in full.
	 *
	 * The model builder keeps its texture page and CLUT from one model to the next, so an actor's
	 * model can inherit them from the model built before it. The first model of a map would inherit
	 * the state the previous map or mode left; this starts from the kernel's initial state, and no
	 * map's first model shows the difference.
	 */
	WELTALLCORE_API TResult<FFieldMap> ParseFieldMap(std::span<const uint8> ArchiveSectors, std::span<const uint8> TextureStreamSectors);

	/** Reads map MapNumber's archive alone from the disc: its header and sections, nothing built from them. */
	WELTALLCORE_API TResult<FFieldArchive> LoadFieldArchive(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber);

	/** Reads map MapNumber (0 to 729) from the disc: files 0xB8 + 2 * MapNumber and the next one of directory 4. */
	WELTALLCORE_API TResult<FFieldMap> LoadFieldMap(const FCdImage& Image, const FGameFileIndex& Index, int32 MapNumber);

	/**
	 * Makes the map's uploads into VRAM, then, with bBrightenPalettes, raises every colour of the
	 * palette rows (0, 480)-(255, 511) but transparent black to at least 3 in each component. The
	 * field brightens them when the map's scripts have switched the player's action mode on by the
	 * time the texture stream ends; it then also stops dithering.
	 */
	WELTALLCORE_API void UploadFieldTextures(const FFieldMap& Map, FVram& Vram, bool bBrightenPalettes);

	/**
	 * UploadFieldTextures in the two steps of the field's load: the archive's images as it reads the archive, before the
	 * scripts' init and the gears, then the texture stream, as it streams in after them.
	 */
	WELTALLCORE_API void UploadFieldArchiveImages(const FFieldMap& Map, FVram& Vram);
	WELTALLCORE_API void UploadFieldTextureStream(const FFieldMap& Map, FVram& Vram, bool bBrightenPalettes);
}
