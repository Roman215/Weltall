#include "Field/FieldReturn.h"

#include "EventOps.h"
#include "EventSprites.h"
#include "Field/EventMachine.h"
#include "Field/FieldGears.h"
#include "Field/FieldPartySprites.h"
#include "Field/FieldWalkmesh.h"
#include "MovementRules.h"
#include "Game/GameState.h"
#include "Gears/GearModels.h"
#include "Graphics/Sprite.h"

#include <string>

namespace Weltall
{
	namespace
	{
		using MovementRules::Wrap;

		constexpr uint8 NoActor = FEventContext::NoActor;
		constexpr int16 NoExtraAnimations = -1;
		constexpr uint16 NoAnimationRequest = 0xFF;
		/** The first actor's script the field runs aside as it comes back. */
		constexpr int32 ReturnScript = 2;
		/** A script state's record of its sprite: bit 7 of its id a map's sprite file, its kind, colour set and deferred start. */
		constexpr uint8 MapSpriteId = 0x80;
		constexpr uint32 SpriteKindShift = 28;
		constexpr uint32 SpriteKindMask = 0x3;
		constexpr uint32 PaletteSetMask = 0xF;
		constexpr uint32 DeferredBit = 0x10;
		/** The texture slots FE A6 and FE A7 gave a map sprite, as its script state keeps them. */
		constexpr uint32 TextureSlotsShift = 16;
		constexpr uint32 TextureSlotsMask = 0x3;
		constexpr uint32 SlotColumnShift = 18;
		constexpr uint32 SlotColumnMask = 0x3FF;
		constexpr uint32 SlotRowMask = 0x1FF;
		constexpr uint32 SecondSlotColumnShift = 9;
		constexpr uint32 SecondSlotRowShift = 19;
		constexpr uint32 ExtraRotationAxis = 0x3;
		constexpr uint16 HiddenRecord = 0x20;
		/** A leader riding an actor comes back this much higher (16.16). */
		constexpr int32 RiderRise = 8;
		constexpr uint8 Riding = 1;
		constexpr uint32 PilotClasses = EventActorFlags0::GearPilot | EventActorFlags0::PartyMember;

		FError ReturnError(const FEventContext& Context, const std::string& What)
		{
			return MakeError("field map " + std::to_string(Context.RunningMap) + " comes back from a battle: " + What);
		}

		std::optional<FError> TakeFault(FEventContext& Context)
		{
			if (Context.Run.Fault == EEventFault::None)
			{
				return std::nullopt;
			}
			const EEventFault Fault = Context.Run.Fault;
			Context.Run.Fault = EEventFault::None;
			return ReturnError(Context, EventOps::DescribeFault(Context, Fault, Context.Run.FaultValue));
		}

		/** A map sprite's texture slots, as FE A6 and FE A7 left them in its script state. */
		void GiveTextureSlots(FEventContext& Context, int32 Actor)
		{
			const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			const uint32 Slots = State.Status >> TextureSlotsShift & TextureSlotsMask;
			FSprite* Sprite = Context.Models->ActorSprite(Actor);
			if (Slots == 0 || Slots > 2 || Sprite == nullptr)
			{
				return;
			}
			Context.Models->SpriteSystem()->AllocTextureSlots(*Sprite, static_cast<int32>(Slots) + 1);
			Sprite->Extra.TextureSlots[1] = {static_cast<int16>(State.Status >> SlotColumnShift & SlotColumnMask), static_cast<int16>(State.SpriteTexture & SlotRowMask)};
			if (Slots == 2)
			{
				Sprite->Extra.TextureSlots[2] = {static_cast<int16>(State.SpriteTexture >> SecondSlotColumnShift & SlotColumnMask),
					static_cast<int16>(State.SpriteTexture >> SecondSlotRowShift & SlotRowMask)};
			}
		}
	}

	std::optional<FError> SaveFieldForReturn(const FEventContext& Context, IFieldActorModels& Models, FFieldSnapshot& Saved)
	{
		Saved.ScriptsKey = Context.ScriptsKey;
		Saved.Actors.resize(Context.States.size());
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			FFieldSavedActor& Out = Saved.Actors[Actor];
			const FSprite* Sprite = Models.ActorSprite(static_cast<int32>(Actor));
			if (Sprite == nullptr)
			{
				return MakeError("field map " + std::to_string(Context.RunningMap) + " saves itself for a battle, and actor " + std::to_string(Actor) + " has no sprite");
			}
			Out.Rotation = Context.Actors[Actor].Rotation;
			Out.Flags = Context.Actors[Actor].Flags;
			Out.Sprite = FSpriteSystem::SaveState(*Sprite);
			Out.State = Context.States[Actor];
		}
		Saved.WalkAttributes = Context.Walkmesh != nullptr ? Context.Walkmesh->Attributes : std::vector<uint32>{};
		Saved.Variables = Context.Variables;
		Saved.Camera = Context.Camera;

		const FFieldRenderState& Render = Context.Render;
		Saved.BackgroundColor = Render.BackgroundColor;
		Saved.FarBucket = Render.FarBucket;
		Saved.Fog = Render.Fog;
		Saved.Panorama = Render.Panorama;
		Saved.GearLights = Render.GearLights;
		Saved.SceneryDrift = Render.SceneryDrift;
		Saved.DriftStep = Render.DriftStep;
		Saved.DriftTotal = Render.DriftTotal;
		Saved.Fades = Render.Fades;
		Saved.ScreenWave = Render.ScreenWave;

		Saved.EncounterPeriod = Context.Encounters.Period;
		Saved.EncountersPerPeriod = Context.Encounters.PerPeriod;
		Saved.EncounterCountdown = Context.Encounters.Countdown;
		Saved.EncountersPending = Context.Encounters.Pending;

		Saved.ScriptMask = Context.Controls.ScriptMask;
		Saved.bReversed = Context.Controls.bReversed;
		Saved.bPartyPlaced = Context.Controls.bPartyPlaced;
		Saved.bFollowClose = Context.Controls.bFollowClose;
		Saved.bLeaderApart = Context.Controls.bLeaderApart;
		Saved.Movement = Context.Movement;
		Saved.SoundListener = Context.ActorSounds.Listener;
		Saved.SoundRange = Context.ActorSounds.Range;
		Saved.SoundKeepMask = Context.ActorSounds.KeepMask;

		const FFieldGears& Gears = Context.Gears;
		Saved.GearCount = Gears.Count;
		Saved.GearFileNumbers = Gears.FileNumbers;
		Saved.GearAnimations = Gears.Animations;
		Saved.GearLoadScales = Gears.LoadScales;
		Saved.GearTextureShifts = Gears.TextureShifts;
		Saved.GearPoolSize = Gears.PoolSize;

		Saved.PlayerLock = Context.PlayerLock;
		Saved.bMenuLocked = Context.bMenuLocked;
		Saved.bCompassHidden = Context.bCompassHidden;
		Saved.EncounterBattleKind = Context.EncounterBattleKind;
		Saved.ScriptedBattleKind = Context.ScriptedBattleKind;
		Saved.GearBoardingOverride = Context.GearBoardingOverride;
		Saved.GearTurnSpeed = Context.GearTurnSpeed;
		Saved.bPartyWalking = Context.bPartyWalking;
		Saved.OpenWindows = Context.Messages.OpenMask;
		Saved.WindowOpenTicks = Context.Messages.OpenTicks;
		Saved.FrameDelay = Context.FrameDelay;
		Saved.LeaderActor = Context.LeaderActor;
		Saved.PilotSpriteBase = Context.PilotSpriteBase;
		Saved.PartyWalkHurries = Context.PartyWalkHurries;
		Saved.LeaderFlagsBeforeWalk = Context.LeaderFlagsBeforeWalk;
		Saved.PauseDisabled = Context.PauseDisabled;

		for (int32 Slot = 0; Slot < FPartySpriteFiles::Slots; ++Slot)
		{
			Saved.PartySpriteFiles[static_cast<std::size_t>(Slot)] = Context.PartySprites != nullptr ? Context.PartySprites->File(Slot) : FPartySpriteFiles::NoFile;
		}
		Saved.PartyCharacters = Context.PartyCharacters;
		Saved.PartyActors = Context.PartyActors;
		Saved.GearActors = Context.GearActors;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			Saved.Riding[static_cast<std::size_t>(Slot)] = Context.GameState != nullptr ? Context.GameState->PartySlotInGear(Slot) : 0;
		}
		return std::nullopt;
	}

	std::optional<FError> RestoreFieldForReturn(const FFieldSnapshot& Saved, const FGameState& Game, FEventContext& Context)
	{
		if (Saved.ScriptsKey != Context.ScriptsKey)
		{
			return ReturnError(Context, "its scripts are not the ones it was saved with");
		}
		if (Saved.Actors.size() != Context.States.size())
		{
			return ReturnError(Context, "it saved " + std::to_string(Saved.Actors.size()) + " script actors and has " + std::to_string(Context.States.size()));
		}
		if (Context.Walkmesh == nullptr || Saved.WalkAttributes.size() != Context.Walkmesh->Attributes.size())
		{
			return ReturnError(Context, "its walkmesh's attributes are not the ones it saved");
		}
		if (Context.Models == nullptr)
		{
			return ReturnError(Context, "its actors have no sprites");
		}
		Context.PartyCharacters = Saved.PartyCharacters;
		Context.PartyActors = Saved.PartyActors;
		Context.GearActors = Saved.GearActors;

		// The camera but what its save leaves to the load: the follow's counts, the kept height and look-at, the view's pitch.
		FFieldCamera& Camera = Context.Camera;
		const FFieldCamera Loaded = Camera;
		Camera = Saved.Camera;
		Camera.FollowTicks = Loaded.FollowTicks;
		Camera.ReturnTicks = Loaded.ReturnTicks;
		Camera.bHeightKept = Loaded.bHeightKept;
		Camera.ViewPitch = Loaded.ViewPitch;
		Camera.LookAt = Loaded.LookAt;
		Camera.WorldRotation = Loaded.WorldRotation;
		Context.ScriptView.View = Saved.Camera.View.View;

		FFieldRenderState& Render = Context.Render;
		Render.BackgroundColor = Saved.BackgroundColor;
		Render.FarBucket = Saved.FarBucket;
		Render.Fog = Saved.Fog;
		Render.Panorama = Saved.Panorama;
		Render.GearLights = Saved.GearLights;
		Render.SceneryDrift = Saved.SceneryDrift;
		Render.DriftStep = Saved.DriftStep;
		Render.DriftTotal = Saved.DriftTotal;
		Render.Fades = Saved.Fades;
		Render.ScreenWave = Saved.ScreenWave;
		Render.ScreenWave.bDrawn = false;

		Context.Encounters.Period = Saved.EncounterPeriod;
		Context.Encounters.PerPeriod = Saved.EncountersPerPeriod;
		Context.Encounters.Countdown = Saved.EncounterCountdown;
		Context.Encounters.Pending = Saved.EncountersPending;

		Context.Controls.ScriptMask = Saved.ScriptMask;
		Context.Controls.bReversed = Saved.bReversed;
		Context.Controls.bPartyPlaced = Saved.bPartyPlaced;
		Context.Controls.bFollowClose = Saved.bFollowClose;
		Context.Controls.bLeaderApart = Saved.bLeaderApart;
		const bool bFirstTickDone = Context.Movement.bFirstTickDone;
		Context.Movement = Saved.Movement;
		Context.Movement.bFirstTickDone = bFirstTickDone;
		Context.ActorSounds.Listener = Saved.SoundListener;
		Context.ActorSounds.Range = Saved.SoundRange;
		Context.ActorSounds.KeepMask = Saved.SoundKeepMask;

		FFieldGears& Gears = Context.Gears;
		Gears.Count = Saved.GearCount;
		Gears.FileNumbers = Saved.GearFileNumbers;
		Gears.Animations = Saved.GearAnimations;
		Gears.LoadScales = Saved.GearLoadScales;
		Gears.TextureShifts = Saved.GearTextureShifts;
		Gears.PoolSize = Saved.GearPoolSize;

		Context.PlayerLock = Saved.PlayerLock;
		Context.bMenuLocked = Saved.bMenuLocked;
		Context.bCompassHidden = Saved.bCompassHidden;
		Context.EncounterBattleKind = Saved.EncounterBattleKind;
		Context.ScriptedBattleKind = Saved.ScriptedBattleKind;
		Context.GearBoardingOverride = Saved.GearBoardingOverride;
		Context.GearTurnSpeed = Saved.GearTurnSpeed;
		Context.bPartyWalking = Saved.bPartyWalking;
		Context.Messages.OpenMask = Saved.OpenWindows;
		Context.Messages.OpenTicks = Saved.WindowOpenTicks;
		Context.FrameDelay = Saved.FrameDelay;
		Context.LeaderActor = Saved.LeaderActor;
		Context.PilotSpriteBase = Saved.PilotSpriteBase;
		Context.PartyWalkHurries = Saved.PartyWalkHurries;
		Context.LeaderFlagsBeforeWalk = Saved.LeaderFlagsBeforeWalk;
		Context.PauseDisabled = Saved.PauseDisabled;

		Context.Walkmesh->Attributes = Saved.WalkAttributes;
		Context.Variables = Saved.Variables;
		for (std::size_t Actor = 0; Actor < Context.States.size(); ++Actor)
		{
			const FFieldSavedActor& In = Saved.Actors[Actor];
			FFieldActor& Record = Context.Actors[Actor];
			Record.Rotation = In.Rotation;
			Record.Flags = In.Flags;
			// The MIMe weights' block is the one the load made, not the one saved.
			FEventActorState& State = Context.States[Actor];
			const std::array<int32, FEventActorState::PartValueCount> PartValues = State.PartValues;
			State = In.State;
			State.PartValues = PartValues;
		}

		// Every script actor's sprite, made again as its script state says.
		for (std::size_t Index = 0; Index < Context.States.size(); ++Index)
		{
			const int32 Actor = static_cast<int32>(Index);
			const FEventActorState& State = Context.States[Index];
			const auto Kind = static_cast<EFieldSpriteKind>(State.SpriteTexture >> SpriteKindShift & SpriteKindMask);
			if (!EventOps::LoadActorSprite(Context, Actor, Kind, State.ModelIndex, static_cast<int32>(State.SpriteDraw & PaletteSetMask), State.ModelId,
					(State.SpriteDraw & DeferredBit) != 0))
			{
				return TakeFault(Context);
			}
			if ((State.ModelId & MapSpriteId) != 0)
			{
				GiveTextureSlots(Context, Actor);
			}
		}

		// In gear mode, a riding member and its gear hand each other their sprites.
		if (Context.PilotSpriteBase != 0)
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const uint8 Member = Context.PartyActors[static_cast<std::size_t>(Slot)];
				if (Member == NoActor)
				{
					continue;
				}
				const uint8 Gear = Context.GearActors[static_cast<std::size_t>(Slot)];
				FEventActorState* GearState = Context.StateOf(Gear);
				if (GearState == nullptr)
				{
					return ReturnError(Context, "party slot " + std::to_string(Slot) + "'s gear, actor " + std::to_string(Gear) + ", has no script state");
				}
				if (Game.PartySlotInGear(Slot) == 0)
				{
					GearState->Flags0 = (GearState->Flags0 | EventActorFlags0::PartyMember) & ~(EventActorFlags0::MapSprite | EventActorFlags0::GearPilot);
					continue;
				}
				Context.Models->SwapSprites(Member, Gear);
				GearState->Flags0 = (GearState->Flags0 | EventActorFlags0::GearPilot) & ~(EventActorFlags0::MapSprite | EventActorFlags0::PartyMember);
				Context.Actors[Gear].Flags |= HiddenRecord;
			}
		}
		return TakeFault(Context);
	}

	std::optional<FError> RestoreFieldSprites(const FFieldSnapshot& Saved, const FGameState& Game, FEventContext& Context)
	{
		IFieldActorModels& Models = *Context.Models;
		const int32 ActorCount = static_cast<int32>(Context.States.size());
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			const int16 File = Context.States[static_cast<std::size_t>(Actor)].ExternalAnimationFile;
			if (File != NoExtraAnimations && !Models.LoadExtraAnimations(Actor, File))
			{
				return ReturnError(Context, "actor " + std::to_string(Actor) + "'s extra animation set " + std::to_string(File) + " cannot be read");
			}
		}
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			if (Context.States[static_cast<std::size_t>(Actor)].ExternalAnimationFile != NoExtraAnimations)
			{
				Models.UseExtraAnimations(Actor);
			}
		}
		Context.ScriptView.View = Saved.Camera.View.View;

		// A pilot's or a gear's sprite is left as it is made when a slot's riding changed since the save.
		int32 RidingChanged = 0;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			RidingChanged += Saved.Riding[static_cast<std::size_t>(Slot)] != Game.PartySlotInGear(Slot) ? 1 : 0;
		}
		FSpriteSystem& Kernel = *Models.SpriteSystem();
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			const FEventActorState& State = Context.States[static_cast<std::size_t>(Actor)];
			FSpriteSavedState Sprite = Saved.Actors[static_cast<std::size_t>(Actor)].Sprite;
			if (State.ExternalAnimationFile != NoExtraAnimations && State.AnimationRequest != NoAnimationRequest)
			{
				Sprite.Animation = static_cast<int16>(State.AnimationRequest);
			}
			const bool bKeptAsMade = Context.PilotSpriteBase != 0 && (State.Flags0 & PilotClasses) != 0 && RidingChanged != 0;
			FSprite* Actual = Models.ActorSprite(Actor);
			if ((State.Flags1 & EventActorFlags1::NoAnimations) != 0 || bKeptAsMade || Actual == nullptr)
			{
				continue;
			}
			Kernel.RestoreState(*Actual, Sprite);
		}
		if (auto Error = Kernel.FaultError())
		{
			return ReturnError(Context, Error->Message);
		}
		if (Context.Render.ScreenWave.bOn)
		{
			Context.Render.ScreenWave.bOffWhenDone = false;
		}
		FEventMachine::SetMapStartVariables(Context);
		for (int32 Actor = 0; Actor < ActorCount; ++Actor)
		{
			EventOps::BuildModelMatrix(Context, static_cast<uint32>(Actor));
		}
		return std::nullopt;
	}

	std::optional<FError> RestoreFieldScene(const FGameState& Game, FEventContext& Context)
	{
		if (auto Error = FEventMachine::RunScriptAside(Context, ReturnScript))
		{
			return Error;
		}
		if (Context.PilotSpriteBase != 0)
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				FEventActorState* Member = Context.StateOf(Context.PartyActors[static_cast<std::size_t>(Slot)]);
				if (Member != nullptr && Game.PartySlotInGear(Slot) == Riding)
				{
					Member->Flags0 = (Member->Flags0 | EventActorFlags0::GearPilot) & ~(EventActorFlags0::MapSprite | EventActorFlags0::PartyMember);
				}
			}
		}
		if (auto Error = ReplayFieldGearAnimations(Context))
		{
			return ReturnError(Context, Error->Message);
		}
		if (FEventActorState* Leader = Context.StateOf(Context.LeaderActor); Leader != nullptr && Leader->LinkedActor != NoActor)
		{
			Leader->Position[1] = Wrap(int64{Leader->Position[1]} - RiderRise);
		}
		// The scenery's drift so far, on the records it moves.
		const uint8 Mode = Context.Render.SceneryDrift & FieldSceneryDrift::ModeMask;
		for (std::size_t Actor = 0; Actor < Context.Actors.size(); ++Actor)
		{
			const bool bScripted = Actor < Context.States.size();
			const bool bDrifts = bScripted ? (Context.States[Actor].Status & ExtraRotationAxis) == 0 && Mode == 1 : Mode <= 1;
			if (!bDrifts)
			{
				continue;
			}
			std::array<int32, 3>& Translation = Context.Actors[Actor].Translation;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Translation[Axis] = Wrap(int64{Translation[Axis]} + Context.Render.DriftTotal[Axis]);
			}
		}
		return std::nullopt;
	}
}
