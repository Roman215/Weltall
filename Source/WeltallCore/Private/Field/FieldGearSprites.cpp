#include "Field/FieldGearSprites.h"

#include "Foundation/Sha1.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace Weltall
{
	namespace
	{
		using EM = EFieldGearMotion;
		using EK = EFieldGearModelKind;

		/** A sprite file's own length: its header's word at 10h. */
		constexpr uint32 FileEndWord = 0x10;

		/** The motions by animation from the first on; the rest stand (most files repeat the stop under further numbers). */
		constexpr std::array<EM, FFieldGearSprite::Animations> MotionsOf(std::initializer_list<EM> First = {})
		{
			std::array<EM, FFieldGearSprite::Animations> Motions{};
			Motions.fill(EM::Stand);
			std::size_t Animation = 0;
			for (const EM Motion : First)
			{
				Motions[Animation++] = Motion;
			}
			return Motions;
		}

		/** One pose under every animation: a sprite drawn from a single pose in its facings. */
		constexpr std::array<EM, FFieldGearSprite::Animations> Still = MotionsOf();
		/** The party's gears' numbering: stop, walk, run, jump, the stop again, and the jump again at 9. */
		constexpr std::array<EM, FFieldGearSprite::Animations> GearLike =
			MotionsOf({EM::Stand, EM::Walk, EM::Run, EM::Leap, EM::Stand, EM::Stand, EM::Stand, EM::Stand, EM::Stand, EM::Leap});

		constexpr FFieldGearSprite Enemy(uint64 Digest, int16 Scene, int16 Entry, int8 Stance, float Weight, std::array<EM, FFieldGearSprite::Animations> Motions,
			int8 Gesture = 0)
		{
			return {Digest, EK::SceneEnemy, Scene, Entry, Stance, Weight, Motions, Gesture};
		}

		constexpr FFieldGearSprite Gear(uint64 Digest, int16 Number, int8 Stance, float Weight, std::array<EM, FFieldGearSprite::Animations> Motions)
		{
			return {Digest, EK::BattleGear, Number, 0, Stance, Weight, Motions};
		}

		/** A party gear's copy, standing as the party's gear stands. */
		constexpr FFieldGearSprite Parked(uint64 Digest, int16 Number, std::array<EM, FFieldGearSprite::Animations> Motions = Still)
		{
			FFieldGearSprite Sprite = Gear(Digest, Number, FFieldGearSprite::TheParty, 0.0f, Motions);
			Sprite.bParked = true;
			return Sprite;
		}

		/**
		 * A party gear on a cross: its rest pose with its arms turned out by ArmTurn degrees (below the shoulder pieces, which
		 * stay upright), hanging by its hands from where its sprite's lie; or, without the hands' and feet's places,
		 * standing as tall as its sprite as other gears do.
		 */
		constexpr int16 Level = 90;
		constexpr int16 RaisedALittle = 100;

		constexpr FFieldGearSprite Crucified(uint64 Digest, int16 Number, int16 Arm, int16 OtherArm, int16 ArmTurn, int8 HandsAt = 0, int8 FeetAt = 0)
		{
			FFieldGearSprite Sprite = Gear(Digest, Number, 0, 0.0f, Still);
			Sprite.RaisedArms = {Arm, OtherArm};
			Sprite.ArmTurn = ArmTurn;
			Sprite.HandsAt = HandsAt;
			Sprite.FeetAt = FeetAt;
			return Sprite;
		}

		/**
		 * Each stance and weight chosen against the sprite's standing frames, as the party's (the model's silhouette
		 * blended from its rest pose toward each stance, against the sprite's from the front and from behind).
		 */
		constexpr FFieldGearSprite Known[] = {
			// The soldiers' gears of a hangar, the HarquebusMk10 and the Musha Mk100.
			Enemy(0x288023C04E8F44EF, 16, 0, 6, 0.5f, MotionsOf({EM::Stand, EM::Walk, EM::Leap})),
			Enemy(0xC0C64E652B728355, 9, 1, 6, 0.1f, MotionsOf({EM::Stand, EM::Walk})),
			// A red gear on treads (a cave's, and a row of eight), the battles' gear 17.
			Gear(0x88DC1ECEABCC24B8, 17, 6, 0.8f, GearLike),
			// A white gear ("A Gebler Gear unit!").
			Enemy(0x183DC828AD039976, 34, 0, 6, 0.3f, MotionsOf({EM::Stand, EM::Walk, EM::Run})),
			// The desert's Sand Trippers: a file that skis, and one that also runs and leaps.
			Enemy(0x46AAA5BC04DD69DD, 43, 0, 6, 0.8f, MotionsOf({EM::Stand, EM::Walk})),
			Enemy(0xE8568BF48EDAE6FC, 43, 0, 6, 0.9f, GearLike),
			// The Elements' gears: Bladegash, Skyghene, Marinebasher and Grandgrowl, still; Bladegash dashing and
			// Marinebasher flying in other files.
			Enemy(0x10C5CD3AC709585B, 54, 0, 6, 0.4f, Still),
			Enemy(0xE078B43925369707, 54, 0, 6, 0.4f, MotionsOf({EM::Stand, EM::Run})),
			Enemy(0x65A9409C33458227, 54, 1, 1, 0.8f, Still),
			Enemy(0xD115E55C110454CF, 54, 2, 1, 0.8f, Still),
			Enemy(0x9339488C17D19CF6, 54, 2, 1, 0.8f, MotionsOf({EM::Stand, EM::Walk})),
			Enemy(0xE7519BF7CA827A1C, 54, 3, 1, 0.8f, Still),
			// Amphysvena and Opiomorph.
			Enemy(0xA33F3FE5E00F6066, 20, 0, 1, 1.0f, MotionsOf({EM::Stand, EM::Walk})),
			Enemy(0x7942E03F9EA0032A, 20, 1, 1, 1.0f, MotionsOf({EM::Stand, EM::Walk})),
			// A thin white gear on long legs, which sways where it stands.
			Enemy(0x1771951011EBCDC9, 65, 1, 6, 0.9f, MotionsOf({EM::Idle, EM::Idle, EM::Idle, EM::Idle, EM::Idle, EM::Idle, EM::Idle, EM::Idle, EM::Idle,
				EM::Idle, EM::Idle})),
			// Etone.
			Enemy(0x90867ED08EBA48A3, 61, 0, 6, 0.6f, MotionsOf({EM::Stand, EM::Walk, EM::Leap, EM::Leap})),
			// Alpha Weltall.
			Enemy(0xDC0BCBA0DB0FA555, 9, 0, 4, 0.5f, MotionsOf({EM::Stand, EM::Walk, EM::Run})),
			// The Salvager: its walks, and its arms at work.
			Enemy(0x9F007CEB0F187591, 21, 1, 4, 1.0f, MotionsOf({EM::Stand, EM::Walk, EM::Walk, EM::Walk, EM::Gesture}), 17),
			// The Repair Bot at the save points, drawn with the Nomad Fix Bot it looks like: idling, and at work.
			Enemy(0x0F82A18994F7C355, 21, 0, 1, 1.0f, MotionsOf({EM::Idle, EM::Gesture}), 17),

			// The party's gears where the party does not ride them: parked (the Yggdrasil's hangar, the Generator
			// missions), with the party elsewhere, and crucified at Golgoda.
			Parked(0xCA67AD6C1DDCC0C2, 0),
			Parked(0x55413815B4D01148, 1),
			Parked(0x8A75BBD2A2060222, 2),
			Parked(0xBBDC9EAED20621A4, 3),
			Parked(0x6FF78C328216733A, 4),
			Parked(0xC6B9D8892E86A51B, 5),
			Parked(0xCB81E8C218EB2E39, 6),
			Parked(0x9D24DCDB6582F337, 8),
			Parked(0xC0417BC07434BD4C, 9),
			Parked(0xF1736990277F9F08, 10),
			Parked(0x25E8F68604E05FB5, 11),
			Parked(0x3215FE92D89C6109, 12),
			Parked(0x5F9F23BF43A4EEA8, 13),
			Parked(0x7F90800DA5CD70C3, 14),
			Parked(0x26861FB26AD1A925, 15),
			Parked(0x7974BB3F1F5AE94A, 2, MotionsOf({EM::Stand, EM::Walk})),
			Parked(0xDF3696A37457D612, 14, MotionsOf({EM::Stand, EM::Walk})),
			Crucified(0x1CB7C6AB76E6EDC8, 1, 6, 15, RaisedALittle, 62, -7),
			Crucified(0xE1F62F6DD09D41A3, 8, 15, 24, RaisedALittle, 64, 7),
			Crucified(0x0F92CC8A90694F81, 11, 6, 16, RaisedALittle, 65, -1),
			Crucified(0x71E934FD6062C8FF, 7, 31, 34, Level),
		};
	}

	const FFieldGearSprite* FindFieldGearSprite(uint64 Digest)
	{
		const auto Found = std::find_if(std::begin(Known), std::end(Known), [Digest](const FFieldGearSprite& Sprite) { return Sprite.Digest == Digest; });
		return Found != std::end(Known) ? &*Found : nullptr;
	}

	uint64 FieldGearSpriteDigest(std::span<const uint8> File)
	{
		if (File.size() < FileEndWord + 4)
		{
			return 0;
		}
		const uint32 Own = static_cast<uint32>(File[FileEndWord] | File[FileEndWord + 1] << 8 | File[FileEndWord + 2] << 16 | File[FileEndWord + 3] << 24);
		const FSha1::FDigest Digest = FSha1::Hash(File.first(std::min<std::size_t>(Own, File.size())));
		uint64 Key = 0;
		for (std::size_t Byte = 0; Byte < 8; ++Byte)
		{
			Key = Key << 8 | Digest[Byte];
		}
		return Key;
	}
}
