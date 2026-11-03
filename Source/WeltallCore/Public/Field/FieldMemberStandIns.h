#pragma once

#include "Field/FieldCharacters.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldSpriteStandIns.h"
#include "Field/IdsFieldSprite.h"
#include "Foundation/Types.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"

#include <array>
#include <map>
#include <memory>
#include <optional>

namespace Weltall
{
	class FCdImage;
	class FCharacterLooks;
	class FFieldSprites;
	class FGameFileIndex;
	class FPartyOrder;
	class FVram;
	struct FEventContext;

	/**
	 * The party drawn in the order its player keeps it (FPartyOrder). The game's party keeps the story's order, which its
	 * scenes are written for, and goes on running the game; while the party walks as one behind its leader
	 * (FFieldPartyWatch) and no cutscene plays (FFieldCutscenes), each party slot's sprites are drawn as the member the
	 * player's order shows in that place: the leader's as the first, its followers' as the others. Wherever a member
	 * is set apart from the others, or a cutscene plays (a door's walk, a lift), everyone is in the story's place. A member is drawn with its own sprite where the slot's shows its member, its gear's where it shows its
	 * gear (ridden or parked), and its pilot's where it shows the map's small pilot. A member's own sprite and pilot are
	 * drawn as its player has it drawn (FCharacterLooks), the same while: the grown Emeralda as the child, though in her
	 * own place, and Fei as Id with Id's own sprite (FIdsFieldSprite), his small pilot as Fei's. The pilots' stand-ins,
	 * where they are drawn, take the member's pictures from here (FFieldPilotStandIns).
	 *
	 * The party's sprites share their animations' numbers (stop, walk, run, jump, the gestures, the ladder: fifteen),
	 * and the gears' and pilots' stop, walk, run and jump: a stand-in plays the number its sprite plays, and a sprite
	 * playing anything else (a scene's own set, a gear's scene pose) is drawn as it is, the slot's own member for that
	 * while. Only the drawing changes (FFieldSpriteStandIns).
	 */
	class FFieldMemberStandIns final
	{
	public:
		/** The disc and its index must outlive the stand-ins, which read the members' and the gears' files from it. */
		WELTALLCORE_API FFieldMemberStandIns(const FCdImage& Image, const FGameFileIndex& Index);
		FFieldMemberStandIns(const FFieldMemberStandIns&) = delete;
		FFieldMemberStandIns& operator=(const FFieldMemberStandIns&) = delete;

		/**
		 * The tick's members to show, the field's party (its characters by slot) drawn as Looks has them, shown in Order
		 * while bShown is on, each in its own slot while it is off (a cutscene, a party not walking as one).
		 */
		WELTALLCORE_API void Arrange(const FEventContext& Context, const FPartyOrder& Order, const FCharacterLooks& Looks, bool bShown);

		/** The member a party slot's sprites show this tick, where it is another than the slot's own. */
		std::optional<int32> ShownIn(int32 Slot) const { return Present(ShownMembers[static_cast<std::size_t>(Slot)]); }

		/** The record whose pictures draw a party slot's member this tick (its own sprite, its pilot), where it is another than the slot's own. */
		std::optional<int32> PicturedIn(int32 Slot) const { return Present(ShownPictures[static_cast<std::size_t>(Slot)]); }

		/** The record whose pictures draw the actor's member this tick: the party slot's whose actor or gear the actor is; none for the others. */
		WELTALLCORE_API std::optional<int32> PicturedFor(const FEventContext& Context, int32 Actor) const;

		/** A sprite file a look draws its member with in place of the disc's own: Id's with its picture, Elly's recoloured. */
		struct FLookedFile
		{
			const FSpriteFile* File = nullptr;
			/** A resident file's picture, which goes up with it (Id's). */
			const FVramImage* Picture = nullptr;
		};
		/** The file the actor's member is drawn with this tick where its look changes its pictures, as PicturedFor finds it; none for the others. */
		WELTALLCORE_API FLookedFile LookedFileFor(const FEventContext& Context, int32 Actor);

		/** A stand-in drawing a sprite: none where the sprite is drawn as it is; and the gear it shows, where it shows a gear. */
		struct FLook
		{
			const FSprite* StandIn = nullptr;
			std::optional<int32> Gear;
		};

		/**
		 * The stand-in that draws the actor's sprite, brought up to it: none where the sprite shows its own member, is no
		 * party slot's, or plays what the stand-in cannot. The pilots' sprites are taken here only while their own
		 * stand-ins are not drawn (bPilotsDrawn off). Tick and Vram as FFieldSpriteStandIns::Follow takes them.
		 */
		WELTALLCORE_API FLook Follow(const FEventContext& Context, const FFieldSprites& Sprites, int32 Actor, const FSprite& Sprite, bool bPilotsDrawn,
			uint32 Tick, FVram& Vram);

		/** The pass a stand-in is drawn in for its sprite's: the same place and size, with its own pivot. */
		WELTALLCORE_API static FFieldSpritePass PassFor(const FSprite& Sprite, const FSprite& StandIn, const FFieldSpritePass& Pass);

		/** A new map: the stand-ins are made again as they are needed. */
		WELTALLCORE_API void Clear();

		/** A party slot's stand-in for its own sprite as it was last drawn; none until one was made on the map. */
		const FSprite* Of(int32 Slot) const { return StandIns.Of(Slot); }

	private:
		/** A file of directory 4 by its number there, read once (recoloured with Elly's purple hair if asked); none when it cannot be read. */
		const FSpriteFile* DiscFile(int32 Number, bool bPurpleHair = false);
		/** Id's field sprite, read once; none when it cannot be read. */
		const FIdsFieldSprite* IdsSprite();

		const FCdImage& Image;
		const FGameFileIndex& Index;
		std::map<int32, std::unique_ptr<FSpriteFile>> Files;
		std::optional<FIdsFieldSprite> Id;
		bool bIdUnreadable = false;
		static std::optional<int32> Present(int32 Value) { return Value >= 0 ? std::optional<int32>(Value) : std::nullopt; }

		/** By party slot, the member shown where it is another than the slot's own; -1 elsewhere. */
		std::array<int32, FPartySpriteFiles::Slots> ShownMembers{-1, -1, -1};
		/** By party slot, the record whose pictures draw the member shown where it is another than the slot's own; -1 elsewhere. */
		std::array<int32, FPartySpriteFiles::Slots> ShownPictures{-1, -1, -1};
		/** By party slot, whether the member shown is drawn with Elly's purple hair (its own pictures, recoloured). */
		std::array<bool, FPartySpriteFiles::Slots> PurpleHair{};
		/** By party slot, whether the member shown is drawn as Id. */
		std::array<bool, FPartySpriteFiles::Slots> AsId{};
		/** A stand-in for each slot's sprite made from its buffer, then for each slot's pilot. */
		FFieldSpriteStandIns StandIns;
	};
}
