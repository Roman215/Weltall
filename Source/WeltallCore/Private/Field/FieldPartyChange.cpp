#include "Field/FieldPartyChange.h"

#include "EventSprites.h"
#include "Field/EventContext.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldPartySprites.h"
#include "Game/GameState.h"

#include <array>

namespace Weltall
{
	namespace
	{
		constexpr int32 GearModeBits = 0xC000;
		constexpr uint32 SpriteKindShift = 28;
		constexpr uint32 SpriteKindMask = 3;

		bool ReplaceSprite(FEventContext& Context, uint8 Actor, EFieldSpriteKind Kind, int32 File, uint8 Id)
		{
			FEventActorState* State = Context.StateOf(Actor);
			if (State == nullptr)
			{
				return true;
			}
			const uint16 Height = State->Height;
			if (!EventOps::LoadActorSprite(Context, Actor, Kind, File, 0, Id, true))
			{
				return false;
			}
			State->Height = Height;
			State->Animation = State->IdleAnimation;
			State->AnimationRequest = State->IdleAnimation;
			PlayActorAnimation(Context, Actor, static_cast<int16>(State->IdleAnimation));
			return true;
		}
	}

	std::optional<FError> RefreshFieldParty(FEventContext& Context, FPartySpriteFiles& Files, const FGameState& Game)
	{
		const std::array<uint8, 3> Before = Context.PartyCharacters;
		Files.ForgetLoadedKind();
		if (auto Error = Files.LoadForMap((Context.MapNumber & GearModeBits) != 0, Game, Context.PartyCharacters))
		{
			return Error;
		}
		for (std::size_t Slot = 0; Slot < Before.size(); ++Slot)
		{
			const uint8 Character = Context.PartyCharacters[Slot];
			if (Before[Slot] == Character || Character == FGameState::EmptySlot)
			{
				continue;
			}
			const uint8 Pilot = Context.PartyActors[Slot];
			const uint8 Gear = Context.GearActors[Slot];
			if (Context.StateOf(Pilot) == nullptr)
			{
				continue;
			}
			const bool bPilotMap = Context.PilotSpriteBase != 0;
			const bool bRiding = bPilotMap && Game.PartySlotInGear(static_cast<int32>(Slot)) != 0;
			if (bRiding && Context.StateOf(Gear) == nullptr)
			{
				return MakeError("a riding party member has no field gear actor");
			}
			// Boarding exchanges the pilot's and gear's sprite ownership. Rebuild both in their own places first.
			if (bRiding)
			{
				Context.Models->SwapSprites(Pilot, Gear);
			}
			const auto PilotFile = bPilotMap ? PilotSpriteFile(Character, Context.PilotSpriteBase) : std::optional<int32>(static_cast<int32>(Slot));
			if (!PilotFile || !ReplaceSprite(Context, Pilot, bPilotMap ? EFieldSpriteKind::Map : EFieldSpriteKind::Party,
				*PilotFile, static_cast<uint8>(*PilotFile | (bPilotMap ? 0x80 : 0))))
			{
				return MakeError("the changed party's pilot sprite could not be loaded");
			}
			if (FEventActorState* State = Context.StateOf(Pilot))
			{
				State->Character = Character;
			}
			if (bPilotMap && !ReplaceSprite(Context, Gear, EFieldSpriteKind::Party, static_cast<int32>(Slot), static_cast<uint8>(Slot)))
			{
				return MakeError("the changed party's gear sprite could not be loaded");
			}
			if (bRiding)
			{
				Context.Models->SwapSprites(Pilot, Gear);
				for (const uint8 Actor : {Pilot, Gear})
				{
					PlayActorAnimation(Context, Actor, static_cast<int16>(Context.StateOf(Actor)->IdleAnimation));
				}
			}
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				const FEventActorState& State = Context.States[Actor];
				const auto Kind = static_cast<EFieldSpriteKind>((State.SpriteTexture >> SpriteKindShift) & SpriteKindMask);
				if (Kind == EFieldSpriteKind::PartyCopy && State.ModelIndex == Slot
					&& !ReplaceSprite(Context, static_cast<uint8>(Actor), Kind, static_cast<int32>(Slot), static_cast<uint8>(Slot)))
				{
					return MakeError("a changed party member's sprite copy could not be loaded");
				}
			}
		}
		return std::nullopt;
	}
}
