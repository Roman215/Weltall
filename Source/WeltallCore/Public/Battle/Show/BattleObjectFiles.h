#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Gears/GearFiles.h"
#include "Gears/GearPackets.h"
#include "Graphics/Gte.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FBattleSceneGraphics;
	class FCdImage;
	class FGameFileIndex;

	/**
	 * The Battle program's tables that name its 3D objects' files: by gear number (a combatant's gear)
	 * the file of directory 41 before its model archive and its last variant, the three VRAM places the
	 * gears' textures load to, and the first file of each group of sequence extensions (directory 42).
	 */
	struct FBattleObjectTables
	{
		static constexpr int32 GearCount = 19;
		static constexpr int32 GearPlaceCount = 3;
		static constexpr int32 ExtensionGroups = 17;

		struct FGearEntry
		{
			uint8 First = 0;
			uint8 LastVariant = 0;
		};

		std::array<FGearEntry, GearCount> Gears{};
		std::array<FGearTexturePlace, GearPlaceCount> GearPlaces{};
		std::array<uint8, ExtensionGroups> Extensions{};

		/** From the program as it lies in memory (unpacked), loaded at its base address. */
		WELTALLCORE_API static TResult<FBattleObjectTables> Parse(std::span<const uint8> Program);
		/** Read from the disc as the kernel loads the Battle program. */
		WELTALLCORE_API static TResult<FBattleObjectTables> Load(const FCdImage& Image, const FGameFileIndex& Index);
	};

	/** A gear's sub-object: the bone of the gear it rides and where on it. */
	struct FBattleSubObjectPlace
	{
		uint16 Bone = 0;
		FGteVector Offset{};
	};

	/**
	 * A gear variant's archive: sub-objects (weapons and parts) on the gear's bones that share one model
	 * and one sequence archive, or, with none, an image list uploaded to the gear's texture place.
	 */
	struct FBattleGearVariant
	{
		std::vector<FBattleSubObjectPlace> Parts;
		std::optional<FGearModelPack> Model;
		std::optional<FGearMotionPack> Motion;
		std::vector<FGearTextureBlock> Images;

		WELTALLCORE_API static TResult<FBattleGearVariant> Parse(std::span<const uint8> File);
	};

	/** A gear's own model worn over another gear's moves (FBattleGearDress), and its motion pack, whose record 0 is the pose it was built in. */
	struct FBattleGearDressFiles
	{
		/** The gear whose moves it is worn over, and its own. */
		uint8 PerformerGear = 0;
		uint8 Gear = 0;
		FGearModelPack Model;
		FGearMotionPack Motion;
		/** The model is Id's red gear worn over Fei's own (FBattleGearDress::IdsPairing), not the gear's. */
		bool bIdsGear = false;
	};

	/** A 3D object's two archives (the gears' model pack and motion pack), a gear's variant, and the model a gear wears over them. */
	struct FBattleModelFiles
	{
		FGearModelPack Model;
		FGearMotionPack Motion;
		std::optional<FBattleGearVariant> Variant;
		std::optional<FBattleGearDressFiles> Dress;
	};

	/**
	 * A sequence extension (directory 42): a motion pack whose animations (from 40h) and sequences
	 * (from 50h) an object takes as its second tables, and what its install puts to use, as offsets in
	 * the pack's bytes: a sprite resource, a sound effect file and an image list (each empty where its
	 * entries are equal).
	 */
	struct FBattleSequenceExtension
	{
		struct FSection
		{
			uint32 Offset = 0;
			uint32 End = 0;

			bool IsEmpty() const { return Offset == End; }
		};

		FGearMotionPack Pack;
		FSection Sprite;
		FSection SoundEffects;
		FSection Images;

		WELTALLCORE_API static TResult<FBattleSequenceExtension> Parse(std::vector<uint8> File);
	};

	/** Where the objects' files lie and how they are read (whole sectors, as the game reads them). */
	namespace BattleObjectFiles
	{
		inline constexpr int32 SpecialDirectory = 40;
		inline constexpr int32 GearDirectory = 41;
		inline constexpr int32 ExtensionDirectory = 42;
		inline constexpr int32 SpecialModelCount = 3;

		/**
		 * Gear number Gear's model and sequence archives and variant Variant's archive (none for 0 or a
		 * variant past the gear's last, as the battle's own load chooses); an error past the table.
		 */
		WELTALLCORE_API TResult<FBattleModelFiles> ReadGear(const FCdImage& Image, const FGameFileIndex& Index, const FBattleObjectTables& Tables,
			int32 Gear, int32 Variant);
		/** Special model Model's archives (directory 40). */
		WELTALLCORE_API TResult<FBattleModelFiles> ReadSpecialModel(const FCdImage& Image, const FGameFileIndex& Index, int32 Model);
		/** Directory 42's file File. */
		WELTALLCORE_API TResult<FBattleSequenceExtension> ReadExtension(const FCdImage& Image, const FGameFileIndex& Index, int32 File);
		/** The file sequence op 04 reads: its operand plus the extension group's first file. */
		WELTALLCORE_API std::optional<int32> ExtensionFile(const FBattleObjectTables& Tables, int32 Operand, int32 Group);

		/**
		 * A scene's model enemy of entry Entry (a model entry with its own images): its model archive and,
		 * from the data the battle keeps, its sequence archive; an error for another kind of entry.
		 */
		WELTALLCORE_API TResult<FBattleModelFiles> SceneModel(const FBattleSceneGraphics& Graphics, int32 Entry);
		/** The same, read from scene Scene's graphics on the disc. */
		WELTALLCORE_API TResult<FBattleModelFiles> ReadSceneModel(const FCdImage& Image, const FGameFileIndex& Index, int32 Scene, int32 Entry);
	}

	/**
	 * Battle's 3D objects' files on the disc and the program's tables that name them, read as the objects ask for
	 * them (the gears' loads, the special models, the sequence extensions).
	 */
	class FBattleObjectDisc
	{
	public:
		WELTALLCORE_API static TResult<FBattleObjectDisc> Load(const FCdImage& Image, const FGameFileIndex& Index);

		const FBattleObjectTables& Tables() const { return ObjectTables; }
		WELTALLCORE_API TResult<FBattleModelFiles> ReadGear(int32 Gear, int32 Variant) const;
		WELTALLCORE_API TResult<FBattleModelFiles> ReadSpecialModel(int32 Model) const;
		WELTALLCORE_API TResult<FBattleSequenceExtension> ReadExtension(int32 File) const;
		WELTALLCORE_API TResult<FBattleModelFiles> ReadSceneModel(int32 Scene, int32 Entry) const;

	private:
		const FCdImage* Image = nullptr;
		const FGameFileIndex* Index = nullptr;
		FBattleObjectTables ObjectTables;
	};
}
