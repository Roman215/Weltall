#include "Field/FieldTick.h"

#include "Field/EventMachine.h"
#include "Field/FieldActorFacing.h"
#include "Field/FieldActorMove.h"
#include "Field/FieldActorSounds.h"
#include "Field/FieldCameraUpdate.h"
#include "Field/FieldCharacters.h"
#include "Field/FieldCollisions.h"
#include "Field/FieldCompass.h"
#include "Field/FieldFollowers.h"
#include "Field/FieldFrameVariables.h"
#include "Field/FieldLayers.h"
#include "Field/FieldMessages.h"
#include "Field/FieldModels.h"
#include "Field/FieldParticles.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldTriggers.h"
#include "Game/PsxRandom.h"
#include "Graphics/Sprite.h"
#include "Graphics/SpriteSystem.h"

#include <string>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** A sprite's signal that its jump has landed. */
		constexpr uint16 LandedSignal = 1;

		FError Stopped(const std::string& What)
		{
			return MakeError("the field's tick stopped: " + What);
		}

		void KeepPreviousPositions(FEventContext& Context)
		{
			for (FEventActorState& State : Context.States)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					State.PreviousPosition[Axis] = static_cast<int16>(State.Position[Axis] >> 16);
				}
			}
		}

		/** The layer step of every walking actor but the leader, after the leader's collisions. */
		std::optional<FError> StepOtherActors(FEventContext& Context)
		{
			for (std::size_t Index = 0; Index < Context.States.size(); ++Index)
			{
				const int32 Actor = static_cast<int32>(Index);
				const FFieldActor& Record = Context.Actors[Index];
				FEventActorState& State = Context.States[Index];
				const bool bRunsOnScreen = (State.Flags1 & (EventActorFlags1::KeepsRunningOffScreen | EventActorFlags1::OffScreen)) != EventActorFlags1::OffScreen;
				if (!Record.TakesPartInEvents() || !bRunsOnScreen || (Record.Flags & FieldActorFlag::KindBits) != FieldActorFlag::WalkingKind
					|| (State.Flags0 & (EventActorFlags0::Frozen | EventActorFlags0::MovedByScript)) != 0 || Actor == Context.LeaderActor)
				{
					continue;
				}
				if (!StepWalkLayer(Context, Actor))
				{
					return Stopped("actor " + std::to_string(Actor) + " has no sprite or walkmesh floor for its layer step");
				}
				const FSprite* Sprite = Context.Models->ActorSprite(Actor);
				if (Sprite->Extra.Signal == LandedSignal)
				{
					State.Flags0 &= ~EventActorFlags0::Airborne;
				}
			}
			return std::nullopt;
		}

		/**
		 * With the fix, a sprite made from a party slot whose buffer took another file since takes that file
		 * whole, as a sprite made from the slot now would: its tables, its palette and its animation from the
		 * start. The original reads the new file through the old one's tables.
		 */
		void FollowPartySpriteFiles(FEventContext& Context)
		{
			if (!Context.Options.bPartySpritesFollowTheirFile || Context.PartySprites == nullptr || Context.Models == nullptr)
			{
				return;
			}
			FSpriteSystem* System = Context.Models->SpriteSystem();
			for (std::size_t Actor = 0; Actor < Context.States.size() && System != nullptr; ++Actor)
			{
				FSprite* Sprite = Context.Models->ActorSprite(static_cast<int32>(Actor));
				FSpriteResource* Resource = Sprite != nullptr ? Sprite->Resource : nullptr;
				for (int32 Slot = 0; Resource != nullptr && Slot < FPartySpriteFiles::Slots; ++Slot)
				{
					const FSpriteFile& Buffer = Context.PartySprites->Buffer(Slot);
					const FSpriteTables& Taken = Resource->Tables;
					const FSpriteTables& Own = Buffer.Tables();
					if (Resource->File != &Buffer || Buffer.Size() == 0
						|| (Taken.Animations == Own.Animations && Taken.Frames == Own.Frames && Taken.Palettes == Own.Palettes))
					{
						continue;
					}
					System->ParseResource(*Resource, Buffer, Resource->TextureX, Resource->TextureY, Resource->ClutX, Resource->ClutY);
					Sprite->CurrentFile = &Buffer;
					Sprite->DrawFlags |= SpriteDrawFlags::PaletteUploadPending;
					System->SetAnimation(*Sprite, Sprite->Animation());
				}
			}
		}

		/** The actors' update: the scripts and every actor's movement, the triggers and the followers. */
		std::optional<FError> UpdateActors(FEventContext& Context)
		{
			Context.Controls.bTrailStepped = false;
			if (auto Error = FEventMachine::Update(Context))
			{
				return Error;
			}
			FollowPartySpriteFiles(Context);
			KeepPreviousPositions(Context);
			if (const FEventActorState* Leader = Context.StateOf(Context.LeaderActor))
			{
				Context.Controls.LeaderFrom = {Leader->Position[0], Leader->Position[2]};
			}
			Context.Gears.MetThisTick = 0;
			for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
			{
				if (auto Error = MoveFieldActor(Context, static_cast<int32>(Actor)))
				{
					return Error;
				}
			}
			if (auto Error = CollideControlledActor(Context))
			{
				return Error;
			}
			if (auto Error = StepOtherActors(Context))
			{
				return Error;
			}
			if (Context.PlayerLock == 0 && !Context.bPartyWalking && !CheckFieldTriggers(Context))
			{
				return Stopped("the talk and touch checks have no leader");
			}
			Context.Movement.bFirstTickDone = true;
			if (!UpdatePartyFollowers(Context))
			{
				return Stopped("a follower has no sprite");
			}
			return std::nullopt;
		}
	}

	std::optional<FError> RunFieldFrame(FEventContext& Context, const FFieldTickInput& Input)
	{
		Context.DrawBuffer ^= 1;
		Context.Particles.ClearDrawn();
		Context.PlayClock = Input.Clock;
		Context.ViewTurn = Input.ViewTurn;
		ReadFieldInput(Context, Input.Pad);
		if (auto Error = UpdateFieldFrameVariables(Context, Input.Clock))
		{
			return Error;
		}
		if (auto Error = UpdateActors(Context))
		{
			return Error;
		}
		if (!UpdateFieldCamera(Context))
		{
			return Stopped("the camera follows an actor without a script state, or the map has no camera layer");
		}
		TurnFieldActors(Context);
		if (auto Error = UpdateFieldActorSounds(Context))
		{
			return Error;
		}
		Context.Render.TickFades(Context.Options.bFadesRiseFromBlack);
		TurnFieldCompass(Context);
		if (auto Error = UpdateFieldModels(Context))
		{
			return Error;
		}
		if (auto Error = UpdateFieldCharacters(Context, Context.Camera.View, Input.Drawing))
		{
			return Error;
		}
		RestoreSlowedSprites(Context);
		if (auto Error = UpdateFieldParticles(Context))
		{
			return Error;
		}
		Context.Render.CockpitHud.Tick(Context.Camera, *Context.Arctangents);
		Context.Render.ScreenWave.Tick();
		if (auto Error = TickFieldGears(Context))
		{
			return Error;
		}
		Context.Messages.Tick();
		UpdateFieldMessages(Context, Context.Controls.Pressed, Context.Controls.Repeated);
		Context.Render.TickLineScrolls(Context.Options.bScrollsWrapWithinWidth);
		// The scripts' faded CLUT rows go back into VRAM after the sprites' uploads and the scrollers' copies.
		Context.Render.ClutFade.bLoaded = std::exchange(Context.Render.ClutFade.bLoadPending, false);
		// The next tick's scripts place their speakers through the view and the projection distance
		// the GTE keeps from this tick's drawing.
		Context.ScriptView.View = Context.Camera.View.View;
		Context.ScriptView.ProjectionDistance = static_cast<uint16>(Context.Camera.ProjectionDistance);
		return std::nullopt;
	}

	std::optional<FError> PollFieldMusic(FEventContext& Context)
	{
		Context.Random->Next();
		if (auto Error = Context.Songs->Poll())
		{
			return Error;
		}
		if (Context.SettleTicks != 0)
		{
			--Context.SettleTicks;
		}
		return std::nullopt;
	}

	std::optional<FError> TickField(FEventContext& Context, const FFieldTickInput& Input)
	{
		if (auto Error = RunFieldFrame(Context, Input))
		{
			return Error;
		}
		return PollFieldMusic(Context);
	}
}
