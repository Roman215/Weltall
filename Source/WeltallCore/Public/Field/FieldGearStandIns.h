#pragma once

#include "Battle/Show/BattleGearDress.h"
#include "Battle/Show/BattleGearOpening.h"
#include "Field/FieldCharacters.h"
#include "Field/FieldGearSprites.h"
#include "Field/FieldRenderState.h"
#include "Foundation/Result.h"
#include "Game/CharacterLooks.h"
#include "Foundation/Types.h"
#include "Gears/GearFiles.h"
#include "Gears/GearModels.h"
#include "Gears/GearPoseTrack.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Weltall
{
	class FBattleObjectDisc;
	class FCdImage;
	class FFieldRuntime;
	class FGameState;
	class FGameFileIndex;
	class FSprite;
	class FVram;
	struct FGteArctangents;

	/**
	 * The gears the field draws as sprites, drawn with their 3D models: the party's gears on a map in gears, ridden or
	 * parked (a sprite of a party slot's buffer while it holds a gear's file), and every gear a map draws from a sprite
	 * file of its own that the port knows (FFieldGearSprite: the scenes' and the enemies' gears, and copies of the
	 * party's where the party does not ride them). A stand-in is the gear's model (its battle model: a party gear's,
	 * another of the battles' gears, or a battle scene's model enemy), drawn in the sprite's place at the sprite's size
	 * and turned as its actor faces, moving as the sprite moves: standing, walking, dashing, leaping, idling in place.
	 *
	 * Only the drawing changes. The sprite stays as the game has it and goes on running the game; a stand-in reads it
	 * each tick it is drawn and is drawn instead. The models live in modules of their own (FGearModels, posed here
	 * rather than by scripts, eight models a module), each at its files' scale in a space of its own, its floor at 0,
	 * which its placement puts where the sprite stands at the sprite's size (FGearModel::Placement); each model's
	 * textures lie once in VRAM's port rows, however many actors show it; its poses come from its own animation
	 * records, played out once (FGearPoseTrack) and sampled by how far the gear has gone or how long it has stood. A
	 * sprite carries its own shading and takes no light from the map: the stand-ins are lit alike on every map
	 * (Lights), under the sprite's tint. Where a stand-in cannot show what the sprite does (an animation it has no
	 * motion for, a sprite drawn semi-transparent, a model that cannot be read), the sprite is drawn.
	 *
	 * A party gear is drawn as the player has its pilot drawn (FCharacterLooks): with Citan's sword put away, the second
	 * Heimdal as the first, and Fenrir in its own model with the first Heimdal's poses, worn as in battle (FBattleGearDress).
	 */
	class FFieldGearStandIns
	{
	public:
		/** The stand-ins' modules follow the map's own gears in a frame's layout, ten slots a module. */
		static constexpr int32 FirstLayoutSlot = FGearModels::Slots;
		/** The textures of this many models fit the port's rows at once: a map showing more draws the rest as sprites. */
		static constexpr int32 TexturePlaces = 24;

		/**
		 * How a gear's model shows its sprite's run, a dash on its thrusters: the lunge its battle approach ends in, held;
		 * its battle approach played as far as the sprite goes, a run on its legs; or played in the sprite's own time, a hop.
		 */
		enum class ERun : uint8
		{
			Lunge,
			Strides,
			Hops,
		};

		/** The disc and its index must outlive the stand-ins, which read the models' files from it as they need them. */
		WELTALLCORE_API FFieldGearStandIns(const FCdImage& Image, const FGameFileIndex& Index);
		WELTALLCORE_API ~FFieldGearStandIns();
		FFieldGearStandIns(const FFieldGearStandIns&) = delete;
		FFieldGearStandIns& operator=(const FFieldGearStandIns&) = delete;

		/** A tick's drawing begins: no gear is followed yet. */
		WELTALLCORE_API void BeginTick();

		/**
		 * Whether a stand-in is drawn for the actor's sprite in this tick: true for a gear's sprite that a stand-in can
		 * show, which then follows it (the sprite's place, size, facing, tint and what it plays); the sprite itself is
		 * then not to be drawn. A stand-in's model is read, and its textures put into Vram's port rows, when its gear is
		 * first met on a map. PartyGear, where set, is the party's gear Sprite shows (the stand-in of a party slot's sprite
		 * drawing another member's gear, FFieldMemberStandIns), in place of the one its slot's buffer holds.
		 */
		WELTALLCORE_API bool Follow(const FFieldRuntime& Runtime, int32 Actor, const FSprite& Sprite, const FFieldSpritePass& Pass, FVram& Vram,
			std::optional<int32> PartyGear = std::nullopt);

		/** What a party gear's sprite shows in a tick where no field map draws it (the world map's): what its stand-in follows. */
		struct FSight
		{
			/** The party's gear the sprite is of. */
			int32 Gear = 0;
			/**
			 * Whether it goes or stands. Such a sprite has one way of going, at a pace its model's walk would have to hurry
			 * its short steps to keep (twice as fast as they play) and its run keeps as it is: the model goes as it shows
			 * a field map's dash, each gear its own way.
			 */
			bool bGoes = false;
			/** Where the sprite stands (16.16) on its floor, in the space the models are drawn in. */
			std::array<int32, 3> Position{};
			int16 FloorY = 0;
			/** Where it heads, as a field actor's yaw: 0 along +x, a quarter turn along -z. */
			uint16 Yaw = 0;
			/** That space's units to a unit of the sprite's parts as they are drawn. */
			float Up = 0.0f;
			/** The ground it covered since the tick before (x, z), in that space's units: the space itself may move with a camera. */
			std::array<float, 2> Covered{};
			/** How the player has the party drawn. */
			FCharacterLooks Looks;
		};

		/**
		 * As Follow, for a party gear's sprite that no field map draws: Key tells it from the others as an actor's
		 * number does, the sprite gives its file's standing frames and its tint, and Sight the rest.
		 */
		WELTALLCORE_API bool FollowGear(int32 Key, const FSprite& Sprite, const FSight& Sight, const FGteArctangents* Arctangents, FVram& Vram);

		/**
		 * The followed stand-ins posed for the frame, and their modules' frames made: bTickRan moves their animations on
		 * by a tick (a frame that ran none shows the gears followed last as they stood).
		 */
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Pose(bool bTickRan);

		/** A new map: the gears stand afresh where they are next met. */
		WELTALLCORE_API void Clear();

		int32 ModuleCount() const { return static_cast<int32>(Modules.size()); }
		const FGearModels& Module(int32 Number) const { return Modules[static_cast<std::size_t>(Number)]->Models; }
		/** A module's slot in a frame's layout. */
		static int32 LayoutSlot(int32 Module, int32 Slot) { return FirstLayoutSlot + Module * FGearModels::Slots + Slot; }
		/** Changes whenever a module is made or a model loaded or freed: the frame's layout with it. */
		WELTALLCORE_API uint32 Revision() const;
		/** The tints of the sprites a module's slots stand in for: 128 neutral, as a sprite's. */
		std::span<const std::array<uint8, 3>> Tints(int32 Module) const { return Modules[static_cast<std::size_t>(Module)]->Tints; }

		/**
		 * The light the stand-ins are drawn under: the one the maps most often give their own gears, a white light at
		 * half strength from above and a little ahead, over an ambient grey.
		 */
		WELTALLCORE_API static const FFieldGearLights& Lights();

		/** Whether an actor's stand-in was followed in the last tick drawn. */
		WELTALLCORE_API bool IsShown(int32 Actor) const;
		/** The model an actor's stand-in is of: its kind, its number (and its scene's entry); none for no stand-in made. */
		WELTALLCORE_API std::optional<std::array<int16, 3>> ModelOf(int32 Actor) const;
		/** The battle gear an actor's stand-in is (a party gear's number), -1 for another model or none. */
		WELTALLCORE_API int32 GearOf(int32 Actor) const;
		/** An actor's stand-in's loaded model, null for none. */
		WELTALLCORE_API const FGearModel* LoadedModelOf(int32 Actor) const;
		/**
		 * Whether an actor's stand-in, followed in this tick, runs or hops on its legs where its sprite dashes: the
		 * thrusters the map's scripts light for the dash are no part of what it shows.
		 */
		WELTALLCORE_API bool RunsOnFoot(int32 Actor) const;
		/**
		 * Whether an actor's stand-in, followed in this tick, is a party gear dashing or leaping on its thrusters: the
		 * map's scripts light them where the sprite's back is, which its model's pose carries elsewhere.
		 */
		WELTALLCORE_API bool Thrusts(int32 Actor) const;
		/** Where an actor's stand-in lies in a frame's layout; none for no stand-in made. */
		WELTALLCORE_API std::optional<int32> LayoutSlotOf(int32 Actor) const;

	private:
		enum class EMotion : uint8
		{
			None,
			Stand,
			Walk,
			Run,
			Leap,
			Idle,
			Gesture,
		};

		/** A model's files, read once a session. */
		struct FSource
		{
			EFieldGearModelKind Kind = EFieldGearModelKind::BattleGear;
			int16 Number = 0;
			int16 Entry = 0;
			bool bUnreadable = false;
			FGearModelPack Model;
			FGearMotionPack Motion;
			FBattleGearOpening Opening;
			/** Its textures' place in the port's rows on this map; none yet. */
			int32 Place = -1;
		};

		/** A model as one kind of sprite shows it: its poses, measured once. */
		struct FShape
		{
			FSource* Source = nullptr;
			/** The gear whose poses it takes, worn over its skeleton (Dress); null for its own. */
			FSource* Performer = nullptr;
			std::optional<FBattleGearDress> Dress;
			/** Which of its bones are shown over the performer's as the performer's opening leaves them. */
			std::vector<uint8> DressShown;
			int8 StanceRecord = 0;
			float StanceWeight = 0.0f;
			std::array<int16, 2> RaisedArms{FFieldGearSprite::NoArm, FFieldGearSprite::NoArm};
			int16 ArmTurn = 0;
			int8 GestureRecord = 0;
			bool bMeasured = false;
			bool bUnusable = false;
			/** Standing, as its sprite's stop holds one frame: a pose between the model's rest pose and one of its stances. */
			FGearPose Stand;
			/** Its way of covering ground in a battle, which a walk takes shorter steps of. */
			FGearPoseTrack Move;
			/** How its sprite's run is shown: Move whole (by the ground covered, or in its sprite's time) or Dash held. */
			ERun Run = ERun::Lunge;
			/** The lunge its battles' approach ends in: the sprite's run is such a dash, held, these limbs as it stands. */
			std::array<int16, 2> LungeLimbs{FFieldGearSprite::NoArm, FFieldGearSprite::NoArm};
			FGearPose Dash;
			FGearPose Leap;
			FGearPoseTrack Idle;
			FGearPoseTrack Gesture;
			/** From the model's soles to its top as it stands, and how far below its own space's floor its soles then reach. */
			int32 ModelHeight = 0;
			/**
			 * Worn over a gear whose sprite it stands in for (Id's red gear over Fei's own): how tall the performer stands,
			 * which the sprite's height sizes, so the worn model keeps its own size against it. 0 otherwise.
			 */
			int32 PerformerHeight = 0;
			int32 Sole = 0;
			/** How far below that floor the lowest of its strides reaches, walking and running Move whole. */
			int32 WalkSole = 0;
			int32 RunSole = 0;
			/** A hanging gear's: how high its raised arms reach above its own space's floor. */
			int32 HandsAbove = 0;
			/** How far a cycle of Move carries the model, in its own space's units. */
			float Stride = 0.0f;
		};

		/** The gear a parked gear's picture stands for: its own, or Regurus in Vierge's place while Elly's gear is Regurus. */
		static int32 ParkedGearOf(const FFieldGearSprite& Parked, const FGameState* Game);

		/** An actor's stand-in. */
		struct FStandIn
		{
			int32 Actor = -1;
			FShape* Shape = nullptr;
			/** The map's sprite file it follows, or null for a party slot's gear. */
			const FFieldGearSprite* Known = nullptr;
			int32 PartyGear = -1;
			/** For a parked gear's picture, the gear it stands for (ParkedGearOf); -1 for any other. */
			int32 ParkedGear = -1;
			/** How the player had the party drawn when it was made. */
			FCharacterLooks Looks;
			int32 Module = -1;
			int32 Slot = -1;
			bool bUnusable = false;
			/** Its sprite's standing frames' height above the origin, in texels: the mean of its facings', and the least. */
			int32 SpriteHeight = 0;
			int32 SpriteLowest = 0;

			// What the sprite it follows showed in the tick.
			bool bFollowed = false;
			bool bHeld = false;
			EMotion Motion = EMotion::None;
			/** 16.16, as the sprite's. */
			std::array<int32, 3> Position{};
			int16 FloorY = 0;
			uint16 Yaw = 0;
			/** How far the scenery drifts under it this tick (x, z): the ground moves that way, not the gear. */
			std::array<int32, 2> Drift{};
			/** The ground it covered this tick (x, z), where what it follows tells it (FSight): else its position's move. */
			bool bCoveredTold = false;
			std::array<float, 2> Covered{};
			/** World units to a unit of the model's space (the model at its files' scale). */
			float Scale = 0.0f;
			/** How far above the sprite's floor the model's soles are, in world units: a hanging gear's feet. */
			float Lift = 0.0f;

			// What it shows.
			bool bPlaced = false;
			EMotion Shown = EMotion::None;
			std::array<int32, 3> PlacedAt{};
			int32 ShownYaw = 0;
			int32 Phase = 0;
			int32 BlendLeft = 0;
			FGearPose Pose;
			FGearPose BlendFrom;
			/** How far its cloths' points are left behind this tick (x, z), in its own space's units. */
			std::array<float, 2> ClothTrail{};
			/** The soles it stands on (FShape's, by what it shows), eased from the last as its pose blends. */
			float Sole = 0.0f;
			float SoleFrom = 0.0f;
		};

		struct FModule
		{
			FGearModels Models;
			std::array<std::array<uint8, 3>, FGearModels::Slots> Tints{};
		};

		/** The known sprite file of a map's sprite, if it is one: by the file it was made from. */
		const FFieldGearSprite* KnownSprite(const FFieldRuntime& Runtime, const FSprite& Sprite);
		static EMotion MotionOf(const FSprite& Sprite, const FFieldGearSprite* Known);
		FSource* SourceOf(EFieldGearModelKind Kind, int16 Number, int16 Entry);
		FShape* ShapeOf(FSource& Source, const FFieldGearSprite* Known, int32 PartyGear, FSource* Performer);
		void Make(FStandIn& StandIn, const FSprite& Sprite, FVram& Vram);
		bool Measure(FShape& Shape, FGearModel& Model);
		/** A shape's poses, its performer's, put on its own model (of Scale) as worn over the performer's skeleton. */
		static bool Dress(FShape& Shape, int16 Scale);
		void Step(FStandIn& StandIn);
		static int32 SoleOf(const FShape& Shape, EMotion Shown);
		void TargetPose(const FStandIn& StandIn, FGearPose& Pose);
		FStandIn* StandInOf(int32 Actor);
		const FStandIn* StandInOf(int32 Actor) const;

		const FCdImage& Image;
		const FGameFileIndex& Index;
		/** The Battle program's tables of the gears' files, read when the first gear's are; none when they cannot be. */
		std::unique_ptr<FBattleObjectDisc> Disc;
		bool bDiscUnreadable = false;
		std::vector<std::unique_ptr<FSource>> Sources;
		std::vector<std::unique_ptr<FShape>> Shapes;
		std::vector<std::unique_ptr<FModule>> Modules;
		std::vector<FStandIn> StandIns;
		/** The map's sprite files as known gears, by their order, and the map load they were read for. */
		std::vector<const FFieldGearSprite*> MapSprites;
		int32 MapSpritesRead = -1;
		int32 PlacesUsed = 0;
		uint32 ModulesMade = 0;
		FGearPose Scratch;
		FGearPose Moving;
	};
}
