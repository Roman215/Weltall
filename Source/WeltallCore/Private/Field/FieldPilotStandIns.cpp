#include "Field/FieldPilotStandIns.h"

#include "Field/EventContext.h"
#include "Field/FieldSpriteFiles.h"
#include "Field/FieldSprites.h"
#include "Graphics/Vram.h"

namespace Weltall
{
	namespace
	{
		constexpr int32 PartySpriteDirectory = 4;
		/** The animations a pilot's file and a member's own number alike: stop, walk, run and jump. */
		constexpr int32 SharedAnimations = 4;
		/** The stand-ins' frames from the port rows' first column, their CLUTs from its first row. */
		constexpr int16 AreaX = 0;
		constexpr int16 AreaY = FVram::Height;
		constexpr int16 ClutX = 0x100;
		constexpr int16 ClutRow = FVram::Height;

		/** Where an animation's script starts: its header's offset, from the offset's own place. */
		uint32 ScriptOf(const FSpriteFile& File, const FSpriteTables& Tables, int32 Animation)
		{
			const uint32 Header = File.AnimationHeader(Tables, Animation);
			return Header + 2 + File.Word(Header + 2);
		}

		/**
		 * The member's animation that shows what the pilot plays: the same number among the four the two files share. A
		 * pilot's file past them holds copies of its stop (Elly's, Citan's and Bart's: their scripts are the stop's).
		 */
		std::optional<int32> SharedAnimation(const FSprite& Pilot)
		{
			const int32 Animation = Pilot.Animation();
			if (Animation < 0 || Pilot.Resource == nullptr || Pilot.Resource->File == nullptr)
			{
				return std::nullopt;
			}
			if (Animation < SharedAnimations)
			{
				return Animation;
			}
			const FSpriteFile& File = *Pilot.Resource->File;
			const FSpriteTables& Tables = Pilot.Resource->Tables;
			if (Animation >= File.AnimationCount(Tables) || ScriptOf(File, Tables, Animation) != ScriptOf(File, Tables, 0))
			{
				return std::nullopt;
			}
			return 0;
		}

		/** The party slot whose member on foot the actor is, drawn with its pilot's sprite: none for any other sprite. */
		std::optional<int32> PilotSlot(const FEventContext& Context, const FFieldSprites& Sprites, int32 Actor, const FSprite& Sprite)
		{
			if (Context.PilotSpriteBase == 0 || Actor < 0 || static_cast<std::size_t>(Actor) >= Context.States.size())
			{
				return std::nullopt;
			}
			const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			if ((State.Flags0 & EventActorFlags0::PartyMember) == 0)
			{
				return std::nullopt;
			}
			const std::optional<int32> File = PilotSpriteFile(State.Character, Context.PilotSpriteBase);
			if (!File || Sprite.PrimaryFile == nullptr || Sprite.PrimaryFile != Sprites.MapFile(*File))
			{
				return std::nullopt;
			}
			for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
			{
				if (Context.PartyActors[static_cast<std::size_t>(Slot)] == Actor)
				{
					return Slot;
				}
			}
			return std::nullopt;
		}
	}

	FFieldPilotStandIns::FFieldPilotStandIns(const FCdImage& InImage, const FGameFileIndex& InIndex)
		: Image(InImage), Index(InIndex), StandIns(FPartySpriteFiles::Slots, AreaX, AreaY, ClutX, ClutRow)
	{
	}

	void FFieldPilotStandIns::Clear()
	{
		StandIns.Clear();
	}

	const FSpriteFile* FFieldPilotStandIns::MemberFile(int32 Character)
	{
		const std::size_t At = static_cast<std::size_t>(Character);
		if (!Files[At] && !bUnreadable[At])
		{
			TResult<FSpriteFile> File = ReadSpriteFile(Image, Index, PartySpriteDirectory, Character + FPartySpriteFiles::DiscFileOffset);
			if (File && !File->IsResident(File->Tables()))
			{
				Files[At] = std::make_unique<FSpriteFile>(std::move(*File));
			}
			else
			{
				bUnreadable[At] = true;
			}
		}
		return Files[At].get();
	}

	const FSprite* FFieldPilotStandIns::Follow(const FEventContext& Context, const FFieldSprites& Sprites, int32 Actor, const FSprite& Pilot, uint32 Tick,
		FVram& Vram, std::optional<int32> Shown, const FSpriteFile* Looked, const FVramImage* Picture)
	{
		// A pilot that shows no frame (none built yet, or a script's frame 0) is drawn as it is: nothing.
		const std::optional<int32> Slot = Pilot.PartCount() != 0 ? PilotSlot(Context, Sprites, Actor, Pilot) : std::nullopt;
		const std::optional<int32> Animation = Slot ? SharedAnimation(Pilot) : std::nullopt;
		if (!Animation)
		{
			return nullptr;
		}
		const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		const int32 Character = Shown.value_or(State.Character);
		const FSpriteFile* File = Looked != nullptr ? Looked : Character >= 0 && Character < FGameState::CharacterCount ? MemberFile(Character) : nullptr;
		if (File == nullptr)
		{
			return nullptr;
		}
		Characters[static_cast<std::size_t>(*Slot)] = Character;
		// A drawn actor's animation runs after the drawing unless it is paused or frozen.
		const bool bAnimates = (State.Flags1 & EventActorFlags1::AnimationPaused) == 0 && (State.Flags0 & EventActorFlags0::Frozen) == 0;
		return StandIns.Follow(*Slot, *File, *Animation, Pilot, bAnimates, Scale, Tick, Context.Arctangents, Vram, Picture);
	}

	FFieldSpritePass FFieldPilotStandIns::PassFor(const FSprite& Pilot, const FSprite& StandIn, const FFieldSpritePass& Pass)
	{
		return FFieldSpriteStandIns::PassFor(Pilot, StandIn, Pass, Scale);
	}
}
