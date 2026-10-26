#pragma once

#include "Foundation/Types.h"
#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"
#include "Graphics/SpriteSystem.h"
#include "Foundation/Result.h"
#include "Input/AnalogStick.h"
#include "Text/MesWindow.h"
#include "Text/MessageTable.h"
#include "WorldMap/WorldMapEncounters.h"
#include "WorldMap/WorldMapMovement.h"
#include "WorldMap/WorldMapOptions.h"
#include "WorldMap/WorldMapParticles.h"
#include "WorldMap/WorldMapTables.h"
#include "WorldMap/WorldMapScene.h"
#include "WorldMap/WorldMapScreenWaver.h"
#include "WorldMap/WorldMapSound.h"
#include "WorldMap/WorldMapTasks.h"
#include "WorldMap/WorldMapTerrain.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Weltall
{
	class FGameState;
	class FPsxRandom;
	struct FGteArctangents;

	/** The bytes of a place model's record in the original. */
	inline constexpr int32 WorldMapObjectSize = 0x54;

	/**
	 * A model's textured four-point packets rewritten whole by a cutscene: their texture page and CLUT (none: the
	 * model's own), whether they blend, and for each of the two packet buffers (the drawing's buffer alternates every
	 * tick) the colour the texture is modulated by and whether a task made the buffer's packets opaque again. The
	 * texels stay the model's.
	 */
	struct FWorldMapPacketLook
	{
		uint16 Page = 0;
		std::optional<uint16> Clut;
		bool bSemiTransparent = false;
		std::array<bool, 2> bOpaque{};
		std::array<std::array<uint8, 3>, 2> Colours{};
	};

	/** A place model put in the world, as the program moves it (0x54 bytes in the original). */
	struct FWorldMapObject
	{
		int16 Hidden = 0;
		int16 Part = 0;
		int16 Flags = 0;
		/** Whole units, z negated; the Yggdrasil's hull keeps its task's 20.12 place here. */
		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;
		int32 W = 0;
		FGteVector Angles{};
		/** Its rotation, and (for an attached object) the translation the drawing gives it. */
		FGteMatrix Matrix;
		/** The object it is attached to, or -1. */
		int32 Parent = -1;
		/** The blend its polygons draw with: -1 as the model gives them, else semi-transparent in that mode. */
		int16 Blend = -1;
		/** Its packets as a cutscene rewrote them (none: as the model builds them). */
		std::optional<FWorldMapPacketLook> Look;
		/**
		 * Drawn under the original's screen tests whatever the culling (scene 14's rings, whose far rim the other cullings
		 * show as a line along the horizon once the camera is inside them).
		 */
		bool bOriginalScreenTests = false;
	};

	/** An entry of the party's trail: where the leader stood, and its heading. */
	struct FWorldMapTrailEntry
	{
		FWorldMapVector Position;
		int16 Heading = 0;
	};

	/** The camera's orbit as the program keeps it: the eye (relative to the target), the target's height, the up vector. */
	struct FWorldMapOrbit
	{
		std::array<int16, 3> Eye{};
		std::array<int16, 3> Target{};
		FGteLongVector Up{};
	};

	/** A cloud: its place (20.12), its drift a tick along x and z (20.12), and which of its two looks it shows. */
	struct FWorldMapCloud
	{
		FWorldMapVector Position;
		int16 DriftX = 0;
		int16 Look = 0;
		int16 DriftZ = 0;
	};

	/**
	 * The loop's words for the pad: every queued vertical blank's held and newly pressed buttons ORed; the stick's
	 * report where the last of them left it, and the directions only it holds.
	 */
	struct FWorldMapPad
	{
		uint16 Held = 0;
		uint16 Pressed = 0;
		uint16 StickHeld = 0;
		uint8 StickX = 0x80;
		uint8 StickY = 0x80;
	};

	/** A shadow a task asked for this tick: where (whole units) and its shape (0 a member's, 1 a gear's half as big again, 2 the Yggdrasil's, turned with its heading). */
	struct FWorldMapShadow
	{
		int16 X = 0;
		int16 Kind = 0;
		int16 Z = 0;
	};

	/** The kernel sprite flag the world map clears on its sprites: the kernel's own shadow, which the world map draws itself. */
	inline constexpr uint32 WorldMapSpriteDrawFlag = 0x4;

	struct FWorldMapContext;

	/** The task that draws a tick: the map's (sprites, trees, shadows, minimap), a cutscene's (none of them), a cutscene's with the trees. */
	enum class EWorldMapFrameTask : uint8
	{
		Map,
		Cutscene,
		CutsceneTrees,
	};

	/** What takes the world map's drawing: the frame task hands it each thing as the original's frame task draws it. */
	class IWorldMapDrawing
	{
	public:
		virtual ~IWorldMapDrawing() = default;

		/** The particles, as they stand after this tick's emitters and moves, before anything else. */
		virtual void DrawParticles(const FWorldMapContext& Context) = 0;

		/** A task's sprite, placed about the camera's target, drawn into ordering-table entry OtIndex before it turns and animates. */
		virtual void DrawSprite(const FWorldMapContext& Context, int32 Slot, const FSprite& Sprite, int32 OtIndex) = 0;

		/** The rest of the frame after the models (the terrain, the sky), drawn with the wave phase the context holds. */
		virtual void DrawWorld(const FWorldMapContext& Context) = 0;
	};

	/** What the world map's tasks share: the scene, the slots, the program's globals, the models, the sprites, the windows, the particles. */
	struct FWorldMapContext
	{
		static constexpr int32 TaskCount = 64;
		static constexpr int32 TrailLength = 32;
		static constexpr int32 CloudCount = 80;
		static constexpr int32 ShadowCapacity = 16;
		/** A sprite deeper than this (the GTE's screen z) is not drawn, nor does it turn or animate. */
		static constexpr int32 SpriteDepthLimit = 0xB00;
		/** The slots the playable scenes give their tasks. */
		static constexpr int32 FadeSlot = 0;
		static constexpr int32 LeaderSlot = 1;
		static constexpr int32 GearSlot = 4;
		static constexpr int32 VehicleSlot = 7;
		static constexpr int32 VehiclePartsSlot = 8;
		static constexpr int32 CameraTargetSlot = 9;
		static constexpr int32 CameraDistanceSlot = 10;
		static constexpr int32 ScreenCentreSlot = 11;
		static constexpr int32 FirstEraSlot = 15;

		FWorldMapContext(const FWorldMapTables& InTables, const FWorldMapScene& InScene)
			: Tables(&InTables), Scene(&InScene), Ground(InScene.Terrain, InTables), PlaceNameWindow(0x20, 1), NoticeWindow(0x20, 4)
		{
		}

		const FWorldMapTables* Tables;
		const FWorldMapScene* Scene;
		FWorldMapGround Ground;
		FGameState* Game = nullptr;
		FPsxRandom* Random = nullptr;
		const FGteArctangents* Arctangents = nullptr;
		FMesContext* Text = nullptr;
		/** The fixes in use. */
		FWorldMapOptions Options;

		std::array<FWorldMapTask, TaskCount> Tasks{};
		std::vector<FWorldMapObject> Objects;
		/**
		 * Where the start's hide of the fifth model wrote its halfword (1) in a scene of fewer models: the bytes past the
		 * end of the models' block, -1 where it hid the fifth model.
		 */
		int32 StrayHiddenWrite = -1;
		FWorldMapParticles Particles;
		std::array<FWorldMapCloud, CloudCount> Clouds{};
		/** The screen's waver, once a cutscene's task has set it up. */
		std::optional<FWorldMapScreenWaver> Waver;

		/** The kernel's sprites: one per party member and gear, made from the group's sprite files. */
		FSpriteSystem Sprites;
		std::vector<std::unique_ptr<FSprite>> SpriteSlots;
		std::array<std::optional<FSpriteFile>, 3> MemberFiles;
		std::array<std::optional<FSpriteFile>, 3> GearFiles;
		/** The gear each slot's gear file is of; FFh with none. */
		std::array<uint8, 3> GearsShown{0xFF, 0xFF, 0xFF};

		FMesWindow PlaceNameWindow;
		FMesWindow NoticeWindow;

		/** 1 on foot, 2 in gears, 4 the Yggdrasil on sand, 5 on sand and sea, 7 flying. */
		int32 TravelMode = 0;
		/** Where the task the camera follows stands, and its heading. */
		FWorldMapVector Controlled;
		int16 ControlledHeading = 0;
		/** Where the party starts (the entrance, the save or the vehicle) and its heading. */
		FWorldMapVector Start;
		int32 StartYaw = 0;
		/** The hand-over's words: the entrance id, the scene, the location (the place less 400h). */
		int32 Entrance = 0;
		int32 SceneNumber = 0;
		int32 Location = 0;
		/** The battle's return flag from the entry; then the exit code as each scene loop starts. */
		int32 ReturnWord = 0;
		/** A cutscene set the field's map itself: the exit leaves the game state's place alone. */
		int32 SkipPlaceExit = 0;

		FWorldMapVector CameraTarget;
		int16 CameraPitch = 0;
		int16 CameraYaw = 0;
		int16 CameraRoll = 0;
		/**
		 * How far the view the player sees is turned from the camera's yaw (4096 a turn): the d-pad's directions and
		 * the sprites' facing follow it. 0 in the original.
		 */
		int16 ViewTurn = 0;
		FWorldMapOrbit Orbit;
		/** 20.12 units. */
		int32 CameraDistance = 0;
		int32 ScreenCentreY = 0;
		/** Scripted scenes look at the orbit's target instead of turning by the angles. */
		int32 LookAt = 0;
		int32 ProjectionDistance = 0;
		FGteMatrix CameraMatrix;
		/** The halfword after the camera matrix's rotation: the angles' matrix stores its last entry as a word, the look-at leaves it. */
		int16 CameraMatrixPad = 0;

		std::array<FWorldMapTrailEntry, TrailLength> Trail{};
		int16 TrailIndex = 0;
		/** Thames's headings (4.12 x and z) of the last 32 ticks, which it follows 32 ticks late. */
		std::array<std::array<int16, 2>, 32> ThamesWake{};

		FWorldMapColliders Colliders;
		FWorldMapContact Contact;
		/** Where a ground move ends up (the original's scratch vector, whose fourth word the movers copy). */
		FWorldMapVector MoveResult;
		std::array<FWorldMapShadow, ShadowCapacity> Shadows{};

		int16 Place = -1;
		int16 Notice = -1;
		/** The place record stood on, which entering reads; none, a notice or nothing. */
		std::optional<FWorldMapTrigger> PlaceRecord;
		/** Where the record lies: its list (4 for the program's fixed records) and index, -1 none. */
		int32 PlaceList = -1;
		int32 PlaceIndex = -1;

		FWorldMapEncounters Encounters;

		int32 Fading = 0;
		int32 MenuRequested = 0;
		/** L2 and R2 newly held together this tick, their state, and the tick before's. */
		int32 GearToggle = 0;
		int32 GearButtons = 0;
		int32 GearButtonsBefore = 0;
		int32 Running = 0;
		/** 0 to the field, 1 to a battle; 2 while the map runs. */
		int32 ExitCode = 0;
		int32 DrawBuffer = 0;
		int32 WavePhase = 0;
		/** The party's members present. */
		int32 MemberCount = 0;
		int32 FadeStep = 0;
		int32 FadeBlend = 0;
		/** The blend the fade's quad draws with: the fade blend as the fade task last read it (as it starts, and at each message). */
		int32 FadePageBlend = 0;
		/** A cutscene scene (8-18) plays: no party, no windows, no minimap, the scene's own tasks. */
		bool bCutscene = false;
		/** The frame task of the scene, as it last ran. */
		EWorldMapFrameTask FrameTask = EWorldMapFrameTask::Map;
		/** A cutscene set the terrain about a new start point this tick: its frame's trees are those of the blocks about that point. */
		bool bTerrainRestarted = false;
		int32 Era = 0;
		/**
		 * The port's story keeps the party's ship away from this visit (Story/WorldMapPatch.h): none is shown or boarded
		 * and the party travels on foot or in its gears, the ship's words in the game state left as they are.
		 */
		bool bVehicleAway = false;
		/** The kernel's lock on boarding the gears: set in era 0, and in a forest while the menu runs. */
		uint8 GearLock = 0;
		int32 ShadowCount = 0;

		FWorldMapPad Pad;
		/**
		 * Analog movement (the player's setting; off in the original): the stick this tick, none when it does not hold the
		 * direction; the party's sprites' own time scales, by slot, while a light push slows their walk (empty while not
		 * slowed: the frame task animates before the leader moves, so a slowing lasts until a tick the walk is not slowed);
		 * and whether this tick's walk slowed them.
		 */
		bool bAnalogMovement = false;
		std::optional<FAnalogStick> Stick;
		std::vector<std::pair<int32, uint32>> SlowedSprites;
		bool bPartyPaced = false;

		/** The place names and notices. */
		std::optional<FMessageTable> Messages;
		/** What this tick drew besides the world: the fade's level (-1 none), whether the notice's backdrop shows, each slot's sprite depth (-1 not drawn). */
		int32 FadeShown = -1;
		bool bNoticeBackdrop = false;
		/** The shadows the tick drew (Shadows[0, ShadowsShown)); the count the tasks add to starts again at 0. */
		int32 ShadowsShown = 0;
		std::array<int32, TaskCount> SpriteDepths{};

		/** Why the world map stopped: a path the port does not take, or data it cannot run. */
		std::optional<FError> Fault;

		/** The GTE's screen centre y the frame task projects with: the loop sets it from ScreenCentreY as each pass ends. */
		int32 GteCenterY = 0;
		/** What takes this pass's drawing, if anything. */
		IWorldMapDrawing* Drawing = nullptr;
		/** The sound driver the program calls; none: silent. */
		IWorldMapSound* Sound = nullptr;
	};
}
