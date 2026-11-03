#pragma once

#include "Foundation/Types.h"

#include <array>
#include <span>

namespace Weltall
{
	/** Where the 3D model a gear's sprite stands in for is read from. */
	enum class EFieldGearModelKind : uint8
	{
		/** A gear of the battles' table (directory 41): the party's, and a few others. */
		BattleGear,
		/** A model enemy of a battle scene's graphics (directory 13): its scene and its entry. */
		SceneEnemy,
	};

	/** What a gear's model shows while its sprite plays an animation. */
	enum class EFieldGearMotion : uint8
	{
		/** Nothing a model shows: the sprite is drawn. */
		None,
		/** It stands, as the sprite's stop holds one frame. */
		Stand,
		/** Its way of closing in on a target in a battle, in shorter steps, as far as the sprite goes. */
		Walk,
		/** The lunge its battles' approach ends in, held: the sprite's run is such a dash. */
		Run,
		/** The top of its leap. */
		Leap,
		/** Its stance played over and over where it stands: a sprite whose stop moves in place. */
		Idle,
		/** One of its records played over and over (FFieldGearSprite::Gesture). */
		Gesture,
	};

	/**
	 * A gear a map draws as a sprite of its own (its section 3), and the model that stands in for it: the gears of the
	 * scenes and the enemies the field shows, and copies of the party's gears where the party does not ride them
	 * (parked in the Yggdrasil's hangar, crucified at Golgoda). Known by the sprite file's bytes, the same on every
	 * map and on both discs. The party's own gear sprites (their buffers' files) are not here: they are known by
	 * the file their slot holds.
	 */
	struct FFieldGearSprite
	{
		static constexpr int32 Animations = 11;
		static constexpr int8 TheParty = -1;
		static constexpr int16 NoArm = -1;

		/** The first eight bytes of the SHA-1 of the file over its own length (its header's word at 10h), big-endian. */
		uint64 Digest = 0;
		EFieldGearModelKind Kind = EFieldGearModelKind::BattleGear;
		/** The gear's number, or the scene's. */
		int16 Number = 0;
		/** The scene's graphics entry. */
		int16 Entry = 0;
		/** How it stands: some way from its rest pose toward a stance's first tick; TheParty: the party's gear's own. */
		int8 StanceRecord = TheParty;
		float StanceWeight = 0.0f;
		/** By the sprite's animation; one past them shows nothing. */
		std::array<EFieldGearMotion, Animations> Motions{};
		int8 Gesture = 0;
		/**
		 * A gear on a cross: its arms turned out from its stand by ArmTurn degrees (90 level), each a bone whose own and lower
		 * parts it raises, and the sprite's hands and feet as its front frame draws them, in texels above its origin: the
		 * model hangs by its hands where the sprite's lie on the bar (with no hands' place, it stands as tall as its sprite).
		 * None for most.
		 */
		std::array<int16, 2> RaisedArms{NoArm, NoArm};
		int16 ArmTurn = 0;
		int8 HandsAt = 0;
		int8 FeetAt = 0;
		/**
		 * A party gear parked where the party does not ride it (the Yggdrasil's hangar, the Generator missions): drawn as
		 * the player has its pilot drawn (FCharacterLooks), as the party's own gears are. The crucified keep the story's.
		 */
		bool bParked = false;

		bool Hangs() const { return RaisedArms[0] != NoArm; }
	};

	/** The gear a map's sprite file is, if it is a known one. */
	WELTALLCORE_API const FFieldGearSprite* FindFieldGearSprite(uint64 Digest);

	/** A sprite file's key in the table: its bytes over its own length. */
	WELTALLCORE_API uint64 FieldGearSpriteDigest(std::span<const uint8> File);
}
