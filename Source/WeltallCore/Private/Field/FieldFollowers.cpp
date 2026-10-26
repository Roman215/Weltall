#include "Field/FieldFollowers.h"

#include "Field/EventContext.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldArchive.h"
#include "Field/FieldLayers.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/Sprite.h"

namespace Weltall
{
	namespace
	{
		constexpr uint32 TrailMask = FFieldControls::TrailLength - 1;
		constexpr uint16 AngleMask = 0xFFF;
		constexpr uint16 Hidden = 0x20;
		/** Second flag word: set while a follower stands in its hanging animation. */
		constexpr uint32 Hanging = 0x1000;
		constexpr int16 HangingAnimation = 6;
		/** How far behind the newest step the second and the third member walk. */
		constexpr uint32 SecondMemberLag = 10;
		constexpr uint32 ThirdMemberLag = 20;
		constexpr uint8 NoCharacter = 0xFF;

		FSprite* SpriteOf(FEventContext& Context, int32 Actor)
		{
			return Context.Models != nullptr ? Context.Models->ActorSprite(Actor) : nullptr;
		}

		int16 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		/** The party slot a character is in, -1 when it is none of the members before the first empty slot. */
		int32 PartySlotOf(const FEventContext& Context, int32 Character)
		{
			if (Character == NoCharacter)
			{
				return -1;
			}
			for (std::size_t Slot = 0; Slot < Context.PartyCharacters.size(); ++Slot)
			{
				if (Context.PartyCharacters[Slot] == NoCharacter)
				{
					return -1;
				}
				if (Context.PartyCharacters[Slot] == Character)
				{
					return static_cast<int32>(Slot);
				}
			}
			return -1;
		}

		/** Plays the animation unless it plays already; a negative one plays as 0. */
		void Play(FEventContext& Context, int32 Actor, FEventActorState& State, int16 Animation)
		{
			if (static_cast<int16>(State.Animation) == Animation)
			{
				return;
			}
			State.Animation = static_cast<uint16>(Animation < 0 ? 0 : Animation);
			PlayActorAnimation(Context, Actor, static_cast<int16>(State.Animation));
		}

		/** Stops at the leader's side: out of the jump, in the idle animation. */
		void StandIdle(FEventContext& Context, int32 Actor, FEventActorState& State)
		{
			State.Flags0 &= ~EventActorFlags0::Airborne;
			const int16 Idle = static_cast<int16>(State.IdleAnimation);
			State.Animation = static_cast<uint16>(Idle < 0 ? 0 : Idle);
			PlayActorAnimation(Context, Actor, static_cast<int16>(State.Animation));
		}

		/** Takes the trail's step: its flags' jump, animation, floor, motion, position and facing. */
		void Replay(FEventContext& Context, int32 Actor, FEventActorState& State, FSprite& Sprite, uint32& Index, bool bJumping)
		{
			if (bJumping)
			{
				State.Flags0 |= EventActorFlags0::Airborne;
			}
			else
			{
				State.Flags0 &= ~EventActorFlags0::Airborne;
			}
			const FFollowStep& Step = Context.Controls.Trail[Index];
			Play(Context, Actor, State, Step.Animation);
			SlowActorSprite(Context, Actor, Sprite, Step.Slow);
			State.FloorTriangles = Step.FloorTriangles;
			State.Layer = Step.Layer;
			State.FloorNormal = Step.FloorNormal;
			Sprite.Velocity = Step.Velocity;
			FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Record.Translation[Axis] = Step.Position[Axis];
				const int32 Position = static_cast<int32>(static_cast<uint32>(Record.Translation[Axis]) << 16);
				Sprite.Position[Axis] = Position;
				State.Position[Axis] = Position;
			}
			Sprite.FloorY = Step.LandingHeight;
			State.Facing = Step.Facing;
			State.MoveDirection = Step.Facing;
			Index = (Index - 1) & TrailMask;
		}

		/** A follower's tick: false when it has no sprite. */
		bool Follow(FEventContext& Context, int32 Actor, FEventActorState& State)
		{
			FFieldControls& Controls = Context.Controls;
			const int32 Slot = PartySlotOf(Context, static_cast<int16>(State.Character));
			if (Slot < 0)
			{
				return true;
			}
			FSprite* Sprite = SpriteOf(Context, Actor);
			if (Sprite == nullptr)
			{
				return false;
			}
			uint32& Index = Controls.TrailIndices[static_cast<std::size_t>(Slot)];
			const uint32 Head = Controls.TrailIndices[0];
			SetActorWalkVelocity(Context, Actor, static_cast<int16>(Controls.Trail[Index].Facing));
			const bool bStepJumps = (Controls.Trail[Index].Flags0 & EventActorFlags0::Airborne) != 0;
			const uint32 FloorAttributes = State.FloorAttributes;
			if (Controls.bFollowClose)
			{
				Index = (Head + 1) & TrailMask;
				Replay(Context, Actor, State, *Sprite, Index, bStepJumps);
				return true;
			}
			if (!bStepJumps)
			{
				State.Flags1 &= ~Hanging;
				if ((FloorAttributes & FieldFloorAttribute::Slopes) == 0)
				{
					if (!Controls.bTrailStepped)
					{
						if (Sprite->FloorY == Whole(State.Position[1]))
						{
							if (static_cast<int16>(State.Animation) == HangingAnimation)
							{
								State.Flags1 |= Hanging;
							}
							else
							{
								Play(Context, Actor, State, static_cast<int16>(State.IdleAnimation));
							}
							return true;
						}
					}
					else if (((Head + (Slot == 1 ? SecondMemberLag : ThirdMemberLag)) & TrailMask) != Index)
					{
						return true;
					}
				}
			}
			if (Index != Head)
			{
				Replay(Context, Actor, State, *Sprite, Index, bStepJumps);
			}
			else
			{
				StandIdle(Context, Actor, State);
			}
			return true;
		}
	}

	bool RecordLeaderStep(FEventContext& Context, int32 Actor)
	{
		if (Actor != Context.LeaderActor || Context.bPartyWalking)
		{
			return true;
		}
		FEventActorState* State = Context.StateOf(Actor);
		FSprite* Sprite = SpriteOf(Context, Actor);
		if (State == nullptr || Sprite == nullptr)
		{
			return false;
		}
		FFieldControls& Controls = Context.Controls;
		uint32& Head = Controls.TrailIndices[0];
		FFollowStep& Step = Controls.Trail[Head];
		Step.Velocity = Sprite->Velocity;
		Step.FloorNormal = State->FloorNormal;
		Step.Facing = State->Facing & AngleMask;
		Step.LandingHeight = Sprite->FloorY;
		Step.Position = {Whole(State->Position[0]), Whole(State->Position[1]), Whole(State->Position[2])};
		Step.Animation = static_cast<int16>(State->Animation);
		Step.FloorAttributes = State->FloorAttributes;
		Step.Flags0 = State->Flags0;
		Step.Flags1 = State->Flags1;
		Step.FloorTriangles = State->FloorTriangles;
		Step.Layer = static_cast<uint8>(State->Layer);
		Step.Slow = Controls.LeaderSlow;
		Controls.bTrailStepped = true;
		Head = (Head - 1) & TrailMask;
		return true;
	}

	bool RestartFollowTrail(FEventContext& Context)
	{
		Context.Controls.TrailIndices = {};
		for (int32 Step = 0; Step < FFieldControls::TrailLength; ++Step)
		{
			if (!RecordLeaderStep(Context, Context.LeaderActor))
			{
				return false;
			}
		}
		return true;
	}

	std::optional<int32> PlacePartyOnLeader(FEventContext& Context, bool bOnPlatform)
	{
		const int32 Leader = Context.LeaderActor;
		const FEventActorState* LeaderState = Context.StateOf(Leader);
		if (LeaderState == nullptr || !StepWalkLayer(Context, Leader, bOnPlatform ? std::optional<int32>(Whole(LeaderState->Position[1])) : std::nullopt))
		{
			return Leader;
		}
		const FSprite* LeaderSprite = SpriteOf(Context, Leader);
		for (std::size_t Index = 0; Index < Context.States.size(); ++Index)
		{
			const int32 Actor = static_cast<int32>(Index);
			FEventActorState& State = Context.States[Index];
			FFieldActor& Record = Context.Actors[Index];
			if ((Record.Flags & FieldActorFlag::KindBits) != FieldActorFlag::WalkingKind || PartySlotOf(Context, static_cast<int16>(State.Character)) <= 0)
			{
				continue;
			}
			FSprite* Sprite = SpriteOf(Context, Actor);
			if (!StepWalkLayer(Context, Actor, bOnPlatform ? std::optional<int32>(Whole(State.Position[1])) : std::nullopt) || Sprite == nullptr || LeaderSprite == nullptr)
			{
				return Actor;
			}
			Sprite->Position = LeaderSprite->Position;
			Record.Translation = Context.Actors[static_cast<std::size_t>(Leader)].Translation;
		}
		if (!RestartFollowTrail(Context))
		{
			return Leader;
		}
		return std::nullopt;
	}

	bool UpdatePartyFollowers(FEventContext& Context)
	{
		for (std::size_t Index = 0; Index < Context.States.size(); ++Index)
		{
			FEventActorState& State = Context.States[Index];
			const int32 Actor = static_cast<int32>(Index);
			if ((State.Flags0 & EventActorFlags0::PartyFollower) == 0 || Actor == Context.LeaderActor || (Context.Actors[Index].Flags & Hidden) != 0)
			{
				continue;
			}
			if (Context.Controls.bLeaderApart)
			{
				Play(Context, Actor, State, static_cast<int16>(State.IdleAnimation));
			}
			else if (!Follow(Context, Actor, State))
			{
				return false;
			}
		}
		return true;
	}
}
