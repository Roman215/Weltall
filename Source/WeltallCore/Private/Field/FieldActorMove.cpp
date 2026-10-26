#include "Field/FieldActorMove.h"

#include "EventOps.h"
#include "EventSprites.h"
#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"
#include "WalkmeshGeometry.h"

#include "Field/EventPlacement.h"
#include "Field/FieldActorMotion.h"
#include "Field/FieldWalkTrace.h"
#include "Graphics/GteMath.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using namespace MovementRules;

		constexpr int16 WalkAnimation = 1;
		constexpr int16 RunAnimation = 2;
		constexpr int16 LadderAnimation = 6;
		/** The frozen pose of an actor whose sprite has no animations. */
		constexpr int16 FrozenPose = 2;
		constexpr uint16 NoAnimationRequest = 0xFF;
		constexpr uint16 Standing = 0x8000;
		constexpr uint8 PushCountFloor = 8;
		constexpr uint32 OffTheGround = EventActorFlags0::Airborne | EventActorFlags0::Falling | EventActorFlags0::FixedHeight;
		constexpr uint32 AirborneOrFixed = EventActorFlags0::Airborne | EventActorFlags0::FixedHeight;
		constexpr uint32 PartyClasses = EventActorFlags0::GearPilot | EventActorFlags0::PartyMember;
		constexpr uint32 FrozenOrScripted = EventActorFlags0::Frozen | EventActorFlags0::MovedByScript;
		constexpr uint32 OffScreenBits = 0x600;
		constexpr uint32 BoundsSet = 0x1000;
		/** A blocked actor's slide speed starts again from one unit per tick. */
		constexpr int32 RestingSlideSpeed = 0x10000;
		/** The jump's horizontal speed: the sprite's weight times these, walking and running. */
		constexpr int32 JumpWalkFactor = 0x30;
		constexpr int32 JumpRunFactor = 0x60;

		/**
		 * Analog movement's pace: a walk on the ground from a lighter push than the walk's own (under Always run the push
		 * below a run's, else any short of full) slows the leader's sprite for the tick by as much, no slower than a third.
		 */
		void PaceByStick(FEventContext& Context, int32 Actor, const FEventActorState& State, FSprite& Sprite, int32 Animation)
		{
			if (!Context.bPlayerControlled || !StickWalks(Context, State) || Animation != WalkAnimation || (State.Flags0 & EventActorFlags0::InTheAir) != 0)
			{
				return;
			}
			const int32 Pace = Context.Controls.Stick->Pace(Context.bAlwaysRun ? FAnalogStick::RunTilt : FAnalogStick::FullTilt);
			Context.Controls.LeaderSlow = static_cast<uint16>(FieldUnslowed * FAnalogStick::FullTilt / Pace);
			SlowActorSprite(Context, Actor, Sprite, Context.Controls.LeaderSlow);
		}

		/** Whether the move takes the actor out of the quad its script keeps it in. */
		bool LeavesBounds(const FEventActorState& State, const std::array<int32, 3>& Move)
		{
			using namespace WalkmeshGeometry;
			if ((State.Status & BoundsSet) == 0)
			{
				return false;
			}
			const uint32 Point = PackPoint(Add(State.Position[0], Move[0]) >> 16, Add(State.Position[2], Move[2]) >> 16);
			std::array<uint32, 4> Corners{};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Corners[Corner] = PackPoint(State.MoveBounds[2 * Corner], State.MoveBounds[2 * Corner + 1]);
			}
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				if (PackedNclip(Corners[Corner], Corners[(Corner + 1) % Corners.size()], Point) < 0)
				{
					return true;
				}
			}
			return false;
		}

		/** The collision classes of the party's other members join the leader's while it moves. */
		bool AddPartyClasses(FEventContext& Context, FEventActorState& State)
		{
			for (std::size_t Slot = 1; Slot < Context.PartyActors.size(); ++Slot)
			{
				const uint8 Member = Context.PartyActors[Slot];
				if (Member == FEventContext::NoActor)
				{
					continue;
				}
				const FEventActorState* Other = EventOps::StateOf(Context, Member);
				if (Other == nullptr)
				{
					return false;
				}
				State.Flags0 |= Other->Flags0 & PartyClasses;
			}
			return true;
		}

		/** Traces the move on the ground or through the air; false when it is blocked. */
		bool Walk(FEventContext& Context, int32 Actor, FEventActorState& State, std::array<int32, 3>& Move, int32 Direction)
		{
			const int32 Layer = static_cast<int16>(State.Layer);
			if (Layer < 0 || static_cast<std::size_t>(Layer) >= Context.Walkmesh->Layers.size())
			{
				EventOps::Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State.Layer));
				return false;
			}
			const int32 Triangle = State.FloorTriangles[static_cast<std::size_t>(Layer)];
			if (Triangle == -1)
			{
				return false;
			}
			if (Triangle < -1 || Triangle >= static_cast<int32>(Context.Walkmesh->Layers[static_cast<std::size_t>(Layer)].Triangles.size()))
			{
				EventOps::Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint32>(Triangle));
				return false;
			}
			const uint32 Before = State.Flags0;
			if (Actor == Context.LeaderActor && !AddPartyClasses(Context, State))
			{
				return false;
			}
			// No trace has crossed an edge yet: the original's slide would read whatever its stack held.
			FWalkEdge Edge{};
			const bool bGround = (State.Flags0 & OffTheGround) == 0 && State.LinkedActor == FEventContext::NoActor;
			const bool bMoved = bGround ? MoveOnGround(Context, State, Move, Direction, Edge) : MoveThroughAir(Context, State, Move, Direction, Edge);
			State.Flags0 = (State.Flags0 & ~PartyClasses) | (Before & PartyClasses);
			return bMoved;
		}
	}

	void MoveActor(FEventContext& Context, int32 Actor)
	{
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		FSprite* Sprite = EventOps::ActorSprite(Context, Actor);
		if (Sprite == nullptr)
		{
			EventOps::Fail(Context, EEventFault::NoActorModels, static_cast<uint32>(Actor));
			return;
		}
		if ((State.Flags0 & EventActorFlags0::PartyFollower) != 0)
		{
			return;
		}
		int32 Direction = static_cast<int16>(State.MoveDirection);
		int32 Animation = WalkAnimation;
		if ((State.Flags0 & EventActorFlags0::PadControlled) != 0 && ((Context.PadHeld & FieldRunButton) != 0) != RunsUnheld(Context, State) && Context.bPlayerControlled)
		{
			Animation = RunAnimation;
		}
		if ((State.Flags0 & (EventActorFlags0::Airborne | EventActorFlags0::Falling)) != 0)
		{
			const int16 Current = static_cast<int16>(State.Animation);
			if (Current == WalkAnimation || Current == RunAnimation)
			{
				Animation = Current;
			}
		}
		PaceByStick(Context, Actor, State, *Sprite, Animation);
		if (State.PushCount > PushCountFloor)
		{
			--State.PushCount;
		}

		const bool bDrifting = (State.Velocity[0] | State.Velocity[1] | State.Velocity[2]) != 0 || !IsActorSettled(State, Context.Movement);
		std::array<int32, 3> Move{};
		bool bBlocked = false;
		if ((Direction & Standing) != 0 && !bDrifting && (State.Flags0 & AirborneOrFixed) == 0)
		{
			Animation = State.IdleAnimation;
			State.MoveDirection |= Standing;
			bBlocked = true;
		}
		else
		{
			if ((Direction & Standing) == 0)
			{
				SetActorWalkVelocity(Context, Actor, Direction);
				Move = {Add(Sprite->Velocity[0], State.Velocity[0]), Add(Sprite->Velocity[1], State.Velocity[1]), Add(Sprite->Velocity[2], State.Velocity[2])};
				State.Facing = static_cast<uint16>(Direction);
			}
			else
			{
				Move = State.Velocity;
				Direction = State.Facing & 0xFFF;
			}
			if (LeavesBounds(State, Move))
			{
				bBlocked = true;
			}
			else
			{
				if (Move[0] != 0 || Move[2] != 0)
				{
					Direction = static_cast<int32>(static_cast<uint32>(-static_cast<int16>(GteRatan2(Move[2], Move[0], *Context.Arctangents))) & 0xFFF);
				}
				bBlocked = !Walk(Context, Actor, State, Move, static_cast<int16>(Direction));
			}
		}
		if (bBlocked)
		{
			State.WordF0 = RestingSlideSpeed;
			State.Velocity = {};
			Move = {};
			Sprite->Velocity[0] = 0;
			Sprite->Velocity[2] = 0;
			State.Facing |= Standing;
		}

		State.Flags1 &= ~EventActorFlags1::AnimationPaused;
		if ((State.Flags0 & EventActorFlags0::Airborne) != 0)
		{
			if (Context.Movement.ActionMode == 0)
			{
				if (WholePart(Sprite->Position[1]) == Sprite->FloorY)
				{
					Sprite->Speed = 0;
				}
				else
				{
					Sprite->Speed = Multiply(Sprite->Weight, static_cast<int16>(Animation) == RunAnimation ? JumpRunFactor : JumpWalkFactor);
				}
			}
			Animation = static_cast<uint16>(Context.Movement.JumpAnimation);
		}
		else
		{
			if ((State.MoveDirection & Standing) != 0)
			{
				Animation = State.IdleAnimation;
			}
			const std::optional<uint32> Floor = ActorFloorAttributes(State, *Context.Walkmesh);
			if (!Floor)
			{
				EventOps::Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State.Layer));
				return;
			}
			// With the fix a sprite without a ladder animation keeps its own; the original plays the words past its table.
			const FSpriteResource* Resource = Sprite->Resource;
			const bool bHasLadderAnimation = Resource != nullptr && Resource->File != nullptr && Resource->File->AnimationCount(Resource->Tables) > LadderAnimation;
			if ((*Floor & FieldFloorAttribute::Ladder) != 0 && (bHasLadderAnimation || !Context.Options.bLadderOnlyWithItsAnimation))
			{
				if ((State.MoveDirection & Standing) != 0 && static_cast<int16>(State.Animation) == LadderAnimation)
				{
					State.Flags1 |= EventActorFlags1::AnimationPaused;
				}
				Animation = LadderAnimation;
			}
		}
		if (static_cast<int16>(State.AnimationRequest) != NoAnimationRequest)
		{
			Animation = State.AnimationRequest;
		}
		if (static_cast<int16>(State.Animation) != static_cast<int16>(Animation) && (State.Flags0 & EventActorFlags0::AnimationLocked) == 0)
		{
			State.Animation = static_cast<uint16>(Animation);
			PlayActorAnimation(Context, Actor, static_cast<int16>(Animation));
		}
		if ((State.FloorAttributes & FieldFloorAttribute::SlowGround) != 0)
		{
			Move[0] >>= 1;
			Move[2] >>= 1;
		}
		State.Step = Move;
		State.Velocity = {};
	}

	std::optional<FError> MoveFieldActor(FEventContext& Context, int32 Actor)
	{
		const FFieldActor& Record = Context.Actors[static_cast<std::size_t>(Actor)];
		FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
		if ((Record.Flags & FieldActorFlag::KindBits) != FieldActorFlag::WalkingKind)
		{
			return std::nullopt;
		}
		if ((State.Flags0 & FrozenOrScripted) == 0)
		{
			if ((State.Flags1 & OffScreenBits) != EventActorFlags1::OffScreen)
			{
				const std::optional<uint32> Floor = ActorFloorAttributes(State, *Context.Walkmesh);
				if (!Floor)
				{
					EventOps::Fail(Context, EEventFault::NoFloorTriangle, static_cast<uint16>(State.Layer));
				}
				else
				{
					State.FloorAttributes = *Floor;
					ApplyFloorForces(Context, Actor);
					if (Context.Run.Fault == EEventFault::None)
					{
						MoveActor(Context, Actor);
					}
				}
			}
		}
		else if ((State.Flags1 & EventActorFlags1::NoAnimations) != 0 && (State.Flags0 & EventActorFlags0::MovedByScript) == 0)
		{
			if (static_cast<int16>(State.Animation) != static_cast<int16>(State.AnimationRequest))
			{
				State.AnimationRequest = FrozenPose;
				State.Animation = FrozenPose;
				if (FSprite* Sprite = EventOps::ActorSprite(Context, Actor))
				{
					Context.Models->SpriteSystem()->SetAnimation(*Sprite, FrozenPose);
				}
			}
		}
		else if ((State.Flags1 & EventActorFlags1::ExplicitPosition) != 0)
		{
			if (static_cast<int16>(State.Animation) != static_cast<int16>(State.AnimationRequest))
			{
				State.Animation = State.AnimationRequest;
				PlayActorAnimation(Context, Actor, static_cast<int16>(State.AnimationRequest));
			}
		}
		if (Context.Run.Fault == EEventFault::None)
		{
			return std::nullopt;
		}
		const EEventFault Fault = Context.Run.Fault;
		Context.Run.Fault = EEventFault::None;
		return MakeError("field map " + std::to_string(Context.RunningMap) + ", moving actor " + std::to_string(Actor) + ": " + EventOps::DescribeFault(Context, Fault, Context.Run.FaultValue));
	}
}
