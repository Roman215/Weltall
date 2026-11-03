#pragma once

#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Media/DiscIdentification.h"
#include "WorldMap/WorldMapCutsceneTables.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/** An entry of the world map's scene table: the scene group's header file, the world's size in blocks and the exit code a scene starts with. */
	struct FWorldMapSceneEntry
	{
		int16 GroupHeader = 0;
		int16 BlocksX = 0;
		int16 BlocksZ = 0;
		int16 StartExitCode = 0;
	};

	/**
	 * A place's record: the rectangle it covers (x to x + width, z to z + height, whole units, edges
	 * included), the field map and entrance it leads to, its name's message and its kind.
	 */
	struct FWorldMapTrigger
	{
		int16 X = 0;
		int16 Z = 0;
		int16 Width = 0;
		int16 Height = 0;
		int16 Map = 0;
		int16 Scene = 0;
		int16 Name = 0;
		int16 Kind = 0;

		bool operator==(const FWorldMapTrigger&) const = default;
	};

	/** A cell attribute's split line: a normal (4.12) and a point in the cell (1/512 units). */
	struct FWorldMapSplitLine
	{
		int32 NormalX = 0;
		int32 NormalZ = 0;
		int32 PointX = 0;
		int32 PointZ = 0;
	};

	/** Where a world-map sprite (a member's, a gear's) keeps its palette and texture in VRAM. */
	struct FWorldMapSpritePlace
	{
		int16 ClutX = 0;
		int16 ClutY = 0;
		int16 TextureX = 0;
		int16 TextureY = 0;
	};

	/** The camera's distance, pitch and clearance presets, nearest first. */
	struct FWorldMapCameraPresets
	{
		/** 20.12 units. */
		std::array<int32, 4> Distances{};
		std::array<int16, 4> Pitches{};
		std::array<int16, 4> Clearances{};
		/** The same while the Yggdrasil flies: only the nearest differs. */
		std::array<int16, 4> FlyingPitches{};
		std::array<int16, 4> FlyingClearances{};
	};

	/**
	 * The world map program's tables: the story's eras and encounter bands, the scene table, what each
	 * travel mode may cross, the terrain's split lines, the colliders' rules, the camera's presets, the
	 * gears' sprite places and the places the Yggdrasil docks at. Read from the program on the disc.
	 */
	struct FWorldMapTables
	{
		/** The scene table's entries: 0 unused, 1-9 the eras, then scene s at s + 2 for scenes 8-18. */
		static constexpr int32 SceneEntryCount = 21;
		static constexpr int32 EraCount = 9;
		static constexpr int32 TravelModeCount = 8;
		static constexpr int32 TerrainTypeCount = 8;
		static constexpr int32 SplitKindCount = 16;
		static constexpr int32 RegionCount = 16;

		/** Event variable 0 values at which the next era begins. */
		std::vector<uint16> EraThresholds;
		/** Event variable 0 values at which the next encounter band begins. */
		std::vector<uint16> BandThresholds;
		std::array<FWorldMapSceneEntry, SceneEntryCount> Scenes{};
		/** [travel mode][terrain type]: nonzero where the mode may cross the terrain. */
		std::array<std::array<int16, TerrainTypeCount>, TravelModeCount> WalkTable{};
		std::array<FWorldMapSplitLine, SplitKindCount> SplitLines{};
		/** Along each split line (4.12): the way a blocked move slides. */
		std::array<std::array<int32, 2>, SplitKindCount> SlideDirections{};
		/** The normals of a cell's two diagonals as the height's triangle test weighs them: NE-SW (x, z), then NW-SE. */
		std::array<int32, 4> DiagonalWeights{};
		/** The region an encounter on forest ground uses instead of the cell's. */
		std::array<int16, RegionCount> ForestRegions{};
		/** Whether a collider lets a move through, by contact (none, near, touching): an era object's, then the Yggdrasil's. */
		std::array<int16, 6> ColliderPasses{};
		FWorldMapCameraPresets Camera;
		/** The three party slots' gear sprites. */
		std::array<FWorldMapSpritePlace, 3> GearSprites{};
		/** The places the flying Yggdrasil enters by touching Thames and Shevat, and the ship's interior the menu button leads to. */
		FWorldMapTrigger ThamesPlace;
		FWorldMapTrigger ShevatPlace;
		FWorldMapTrigger InteriorPlace;
		/** The clouds' places in a 2048-unit tile (x, z), repeated over four tiles each way. */
		std::array<std::array<int32, 2>, 5> CloudPlaces{};
		/**
		 * What the program reads as the gear of an empty party slot's character (FFh): a byte of its
		 * own code, which is not FFh, so an empty slot counts as having a gear.
		 */
		uint8 EmptySlotGear = 0;
		/** The waypoints (x, z) of the Yggdrasil's scripted voyages: to the village (scene 3) and to the factory (scene 4). */
		std::array<std::array<int16, 2>, 4> VillageVoyage{};
		std::array<std::array<int16, 2>, 7> FactoryVoyage{};
		/**
		 * Each era's moving models (0 where the era has none): the spinner pair's two, the fast pair's two,
		 * Thames's hull (its four parts are the four models before it), Shevat's first model, and the two
		 * landmarks the colliders stand on.
		 */
		std::array<std::array<uint16, 2>, EraCount> SpinnerPairObjects{};
		std::array<std::array<uint16, 2>, EraCount> FastSpinnerPairObjects{};
		std::array<uint16, EraCount> ThamesObjects{};
		std::array<uint16, EraCount> ShevatObjects{};
		std::array<uint16, EraCount> LandmarkAObjects{};
		std::array<uint16, EraCount> LandmarkCObjects{};
		/** Thames's cruise (x, z): it heads for each in turn. */
		std::array<std::array<uint16, 2>, 8> ThamesWaypoints{};
		/** Shevat's models chained to each other: a parent's and a child's place after Shevat's first. */
		std::vector<std::array<int16, 2>> ShevatAttachments;
		/** The models chained to the story's landmark (model 69), shown only at one value of the story. */
		std::vector<int16> StoryObjects;
		/** The cutscene scenes' own tables. */
		FWorldMapCutsceneTables Cutscenes;

		/** Reads the tables from the Worldmap program on a disc of the given release. */
		WELTALLCORE_API static TResult<FWorldMapTables> Read(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);
	};

	/** The scene a world map entry plays: its scene table entry, era (none for the cutscenes) and group. */
	struct FWorldMapSceneChoice
	{
		int32 Entry = 0;
		std::optional<int32> Era;
		FWorldMapSceneEntry Scene;
	};

	/** The world map's files in directory 36: a scene group's, from its header file. */
	namespace WorldMapFiles
	{
		inline constexpr int32 Directory = 36;
		/** The party's sprites: file Character + this; the gears' sprites: file Gear + GearSpriteBase. */
		inline constexpr int32 CharacterSpriteBase = 2;
		inline constexpr int32 GearSpriteBase = 0x13;
		inline constexpr int32 GroupCount = 17;
		/** The first group's header file; each group's files follow its header, the next group's header eleven files on. */
		inline constexpr int32 FirstGroupHeader = 43;
		inline constexpr int32 GroupFiles = 11;
		inline constexpr int32 Group(int32 Header) { return (Header - FirstGroupHeader) / GroupFiles; }
		inline constexpr int32 GroupHeader(int32 Group) { return FirstGroupHeader + Group * GroupFiles; }

		inline constexpr int32 Archive(int32 Header) { return Header + 1; }
		inline constexpr int32 TerrainTextures(int32 Header) { return Header + 2; }
		inline constexpr int32 ObjectTextures(int32 Header) { return Header + 3; }
		inline constexpr int32 Bank(int32 Header) { return Header + 4; }
		inline constexpr int32 MapSong(int32 Header) { return Header + 5; }
		inline constexpr int32 Effects(int32 Header) { return Header + 6; }
		inline constexpr int32 VehicleSong(int32 Header) { return Header + 7; }
		inline constexpr int32 BattleSong(int32 Header) { return Header + 8; }
		/** The 256 terrain blocks, one a sector, row by row; the next file holds them column by column. */
		inline constexpr int32 Terrain(int32 Header) { return Header + 9; }
		inline constexpr int32 TerrainByColumns(int32 Header) { return Header + 10; }
	}

	/** Where the kernel loads the Worldmap program: the first byte of its unpacked image. */
	inline constexpr uint32 WorldMapProgramAddress = 0x8006FAF0;

	/** The Worldmap program's image, unpacked, as the kernel loads it at WorldMapProgramAddress. */
	WELTALLCORE_API TResult<std::vector<uint8>> ReadWorldMapProgram(const FCdImage& Image, const FGameFileIndex& Index, EDiscRelease Release);

	/**
	 * The scene a world map entry plays, as the program picks it: scenes 0-7 by the story's era (event
	 * variable 0 against the thresholds), scenes 8-18 their own. An error where the program would read
	 * past its tables: a scene past 18, or a variable no threshold stops.
	 */
	WELTALLCORE_API TResult<FWorldMapSceneChoice> ChooseWorldMapScene(const FWorldMapTables& Tables, int32 Scene, uint16 StoryVariable);
}
