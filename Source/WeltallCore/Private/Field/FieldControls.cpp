#include "Field/FieldControls.h"

#include "Field/EventContext.h"
#include "Field/FieldWalkmesh.h"
#include "Graphics/Sprite.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr uint16 NoDirection = 0x8000;
		constexpr uint16 AngleMask = 0xFFF;
		constexpr uint16 HalfTurn = 0x800;
		constexpr uint16 JumpButton = 0x80;
		constexpr uint32 DpadShift = 12;
		constexpr uint16 DpadBits = 0xF000;
		constexpr uint32 LargestTimeScale = 0xFFF;
		constexpr uint32 Jumping = 0x800;
		constexpr int16 StuckJumpTicks = 0x20;
		constexpr uint16 AnimationChosenByMove = 0xFF;

		/** The walking direction of each d-pad combination, indexed by the directions not held: up, right, down, left. */
		constexpr std::array<uint16, 16> Directions = {
			0x8000, 0x400, 0x800, 0x600, 0xC00, 0x8000, 0xA00, 0x800,
			0x000, 0x200, 0x8000, 0x400, 0xE00, 0x000, 0xC00, 0x8000};
		/** The same with every direction reversed. */
		constexpr std::array<uint16, 16> ReversedDirections = {
			0x8000, 0xC00, 0x000, 0xE00, 0x400, 0x8000, 0x200, 0x000,
			0x800, 0xA00, 0x8000, 0xC00, 0x600, 0x800, 0x400, 0x8000};

		/** Whether the floor lets the actor jump: its jump classes share none with the actor's. */
		bool CanJump(const FEventActorState& Actor)
		{
			return ((Actor.Flags0 >> 9) & 3 & (Actor.FloorAttributes >> 3)) == 0;
		}

		void Jump(FEventActorState& Actor)
		{
			if (CanJump(Actor))
			{
				Actor.Flags0 |= Jumping;
			}
		}

		/** The action mode's jump: a press while the cooldown is out jumps, even in the air, and reloads it. */
		void JumpOnCooldown(FEventContext& Context, FEventActorState& Actor)
		{
			FFieldMovement& Movement = Context.Movement;
			if ((Context.Controls.Pressed & JumpButton) != 0 && Movement.JumpCooldown == 0 && Context.MenuRequest == FEventContext::NoMenu && CanJump(Actor))
			{
				Actor.Flags0 |= Jumping;
				Actor.Animation = AnimationChosenByMove;
				Movement.JumpCooldown = Movement.JumpInterval;
			}
			if (Movement.JumpCooldown != 0)
			{
				--Movement.JumpCooldown;
			}
		}

		/** The leader's jumps: out of a spot it is stuck in on steep ground, or on the jump button. */
		void Jumps(FEventContext& Context, FEventActorState& Actor)
		{
			FFieldControls& Controls = Context.Controls;
			const bool bMenuQuiet = Context.MenuRequest == FEventContext::NoMenu;
			if (Controls.StuckTicks > StuckJumpTicks)
			{
				Controls.StuckTicks = StuckJumpTicks;
				if ((Context.PadHeld & JumpButton) != 0 && (Actor.Flags0 & EventActorFlags0::InTheAir) == 0 && bMenuQuiet)
				{
					Jump(Actor);
					return;
				}
			}
			if (Context.Movement.ActionMode != 0)
			{
				JumpOnCooldown(Context, Actor);
			}
			else if ((Controls.Pressed & JumpButton) != 0 && (Actor.Flags0 & EventActorFlags0::InTheAir) == 0 && (Actor.FloorAttributes & FieldFloorAttribute::Steep) == 0 && bMenuQuiet)
			{
				Jump(Actor);
			}
		}

		bool StoodStill(const FEventActorState& Actor)
		{
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				if (Actor.PreviousPosition[Axis] != static_cast<int16>(Actor.Position[Axis] >> 16))
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * Analog movement's reading of the stick: its heading and push when it alone holds a direction the tick's masks
		 * let through (a direction the scripts or the walkmesh keep out keeps the stick out).
		 */
		std::optional<FAnalogStick> ReadStick(const FFieldPadWords& Words, uint16 Mask)
		{
			if ((Mask & DpadBits) != DpadBits)
			{
				return std::nullopt;
			}
			return ReadAnalogStick(Words.Held, Words.StickHeld, Words.StickX, Words.StickY);
		}
	}

	bool StickWalks(const FEventContext& Context, const FEventActorState& Actor)
	{
		return Context.Controls.Stick.has_value() && (Actor.Flags0 & EventActorFlags0::PadControlled) != 0;
	}

	bool RunsUnheld(const FEventContext& Context, const FEventActorState& Actor)
	{
		return Context.bAlwaysRun && (!StickWalks(Context, Actor) || Context.Controls.Stick->Tilt >= FAnalogStick::RunTilt);
	}

	void SlowActorSprite(FEventContext& Context, int32 Actor, FSprite& Sprite, uint16 Slow)
	{
		if (Slow == FieldUnslowed)
		{
			return;
		}
		for (FFieldControls::FSlowedSprite& Slowed : Context.Controls.Slowed)
		{
			if (Slowed.Actor < 0 || Slowed.Actor == Actor)
			{
				if (Slowed.Actor < 0)
				{
					Slowed = {Actor, Sprite.TimeScale()};
				}
				Sprite.SetTimeScale(static_cast<int32>(std::min<uint32>(Slowed.TimeScale * Slow >> 8, LargestTimeScale)));
				return;
			}
		}
	}

	void RestoreSlowedSprites(FEventContext& Context)
	{
		for (FFieldControls::FSlowedSprite& Slowed : Context.Controls.Slowed)
		{
			if (Slowed.Actor >= 0 && Context.Models != nullptr)
			{
				if (FSprite* Sprite = Context.Models->ActorSprite(Slowed.Actor))
				{
					Sprite->SetTimeScale(static_cast<int32>(Slowed.TimeScale));
				}
			}
			Slowed = {};
		}
		Context.Controls.LeaderSlow = FieldUnslowed;
	}

	void FFieldControls::ResetForMap()
	{
		Pressed = 0;
		Repeated = 0;
		ScriptMask = 0xFFFF;
		WalkMask = 0xFFFF;
		bRunLetThrough = false;
		StuckTicks = 0;
		LeaderFrom = {};
		PushedDoor = -1;
		PushedTicks = 0;
		AnsweredDoor = -1;
		AnsweredDirection = 0;
		bReversed = false;
		Stick.reset();
		LeaderSlow = FieldUnslowed;
		Slowed = {};
		bPartyPlaced = false;
		bFollowClose = false;
		bLeaderApart = false;
	}

	void ReadFieldInput(FEventContext& Context, const FFieldPadWords& Words)
	{
		FFieldControls& Controls = Context.Controls;
		const uint16 Mask = Controls.ScriptMask & Controls.WalkMask;
		Context.PadHeld = Words.Held & Mask;
		Controls.Pressed = Words.Pressed & Mask;
		Controls.Repeated = Words.Repeated & Mask;
		if (Context.SettleTicks != 0)
		{
			Context.PadHeld = 0;
			Controls.Pressed = 0;
			Controls.Repeated = 0;
		}
		Controls.bRunLetThrough = Context.SettleTicks == 0 && (Mask & FieldRunButton) != 0;
		Controls.Stick = Context.bAnalogMovement && Context.SettleTicks == 0 ? ReadStick(Words, Mask) : std::nullopt;
		if (Context.bBattleRequested)
		{
			Controls.Pressed &= static_cast<uint16>(~JumpButton);
		}
	}

	uint16 ScriptHeldButtons(const FEventContext& Context)
	{
		const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
		const bool bRunsUnheld = (Leader != nullptr ? RunsUnheld(Context, *Leader) : Context.bAlwaysRun) && Context.Controls.bRunLetThrough
			&& (Context.bPlayerControlled || Context.bPlayerHadControl);
		return bRunsUnheld ? static_cast<uint16>(Context.PadHeld ^ FieldRunButton) : Context.PadHeld;
	}

	bool ControlByPad(FEventContext& Context, FEventActorState& Actor)
	{
		FFieldControls& Controls = Context.Controls;
		if ((Actor.Flags0 & EventActorFlags0::PadControlled) == 0)
		{
			if (!Controls.bPartyPlaced)
			{
				Actor.Flags0 |= EventActorFlags0::PartyFollower;
			}
			return true;
		}
		if (Context.Messages.OffersChoice() || Context.PlayerLock != 0)
		{
			Actor.MoveDirection = NoDirection;
			return true;
		}
		if ((Context.PadHeld >> DpadShift) != 0 && !RollFieldEncounter(Context))
		{
			return false;
		}
		Context.bPlayerControlled = true;
		if ((Actor.FloorAttributes & FieldFloorAttribute::Steep) == 0)
		{
			Controls.StuckTicks = 0;
		}
		else if (StoodStill(Actor))
		{
			++Controls.StuckTicks;
		}
		Jumps(Context, Actor);
		const uint32 Released = (Context.PadHeld >> DpadShift) ^ 0xF;
		uint16 Direction = (Controls.bReversed ? ReversedDirections : Directions)[Released];
		if ((Direction & NoDirection) == 0 && StickWalks(Context, Actor))
		{
			Direction = static_cast<uint16>((Controls.Stick->Heading + (Controls.bReversed ? HalfTurn : 0)) & AngleMask);
		}
		if ((Direction & NoDirection) == 0)
		{
			Direction = static_cast<uint16>((Direction - (Context.Camera.GoalViewYaw + Context.ViewTurn)) & AngleMask);
		}
		Actor.MoveDirection = Direction;
		return true;
	}
}
