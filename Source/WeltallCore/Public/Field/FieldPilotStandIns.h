#pragma once

#include "Field/FieldCharacters.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldSpriteStandIns.h"
#include "Foundation/Types.h"
#include "Game/GameState.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteFile.h"

#include <array>
#include <memory>
#include <optional>

namespace Weltall
{
	class FCdImage;
	class FFieldSprites;
	struct FVramImage;
	class FGameFileIndex;
	class FVram;
	struct FEventContext;

	/**
	 * The party's own field sprites drawn for their pilots while the party is in gears. On such a map the game draws a
	 * member on foot with a pilot sprite of the map's own, a small figure of few texels; a stand-in is the member's own
	 * sprite, the one it walks with on foot, drawn in the pilot's place at a quarter of the size, as the map's people are
	 * drawn (their scripts' SetScale #1024), playing what the pilot plays.
	 *
	 * Only the drawing changes (FFieldSpriteStandIns): the pilot's sprite stays as the game has it and runs the game. A
	 * stand-in follows it, one per party slot. The stand-ins outlive no map: a new map makes them again as its pilots appear.
	 */
	class FFieldPilotStandIns final
	{
	public:
		/** The stand-in's size against its pilot's, as a 4.12 scale. */
		static constexpr int32 Scale = 0x400;

		/** The disc and its index must outlive the stand-ins, which read the members' files from it as they need them. */
		WELTALLCORE_API FFieldPilotStandIns(const FCdImage& Image, const FGameFileIndex& Index);
		FFieldPilotStandIns(const FFieldPilotStandIns&) = delete;
		FFieldPilotStandIns& operator=(const FFieldPilotStandIns&) = delete;

		/**
		 * The stand-in that draws the actor's sprite, brought up to its pilot: none when the sprite is not the pilot of a
		 * party member on foot, or when the pilot plays what a stand-in cannot (an animation of an extra set, or past its
		 * own file's). Tick numbers the field's ticks: a stand-in steps once a tick, however many passes draw it, and its
		 * new frame goes into Vram's port rows. Shown, where set, is the member whose own sprite it is (the party drawn in
		 * its player's order, FFieldMemberStandIns): by default the pilot's own. Looked, where given, is the file its look
		 * draws the member with instead (FFieldMemberStandIns::LookedFileFor: Id's, with its Picture; Elly's recoloured).
		 */
		WELTALLCORE_API const FSprite* Follow(const FEventContext& Context, const FFieldSprites& Sprites, int32 Actor, const FSprite& Pilot, uint32 Tick,
			FVram& Vram, std::optional<int32> Shown = std::nullopt, const FSpriteFile* Looked = nullptr, const FVramImage* Picture = nullptr);

		/** The pass the stand-in is drawn in for its pilot's: the same place and sorting, at the stand-in's scale, with its own pivot. */
		WELTALLCORE_API static FFieldSpritePass PassFor(const FSprite& Pilot, const FSprite& StandIn, const FFieldSpritePass& Pass);

		/** A new map: the stand-ins are made again as its pilots appear. */
		WELTALLCORE_API void Clear();

		/** A party slot's stand-in as it was last drawn, and whose it is; none until one was made on the map. */
		const FSprite* Of(int32 Slot) const { return StandIns.Of(Slot); }
		int32 CharacterOf(int32 Slot) const { return StandIns.Of(Slot) != nullptr ? Characters[static_cast<std::size_t>(Slot)] : -1; }

	private:
		/** The member's own sprite file, read once; none when it cannot be read. */
		const FSpriteFile* MemberFile(int32 Character);

		const FCdImage& Image;
		const FGameFileIndex& Index;
		std::array<std::unique_ptr<FSpriteFile>, FGameState::CharacterCount> Files;
		std::array<bool, FGameState::CharacterCount> bUnreadable{};
		FFieldSpriteStandIns StandIns;
		/** The member each slot's stand-in shows. */
		std::array<int32, FPartySpriteFiles::Slots> Characters{};
	};
}
