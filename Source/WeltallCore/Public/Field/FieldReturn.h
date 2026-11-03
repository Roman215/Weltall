#pragma once

#include "Field/EventActorState.h"
#include "Field/EventContext.h"
#include "Field/EventVariables.h"
#include "Graphics/SpriteSystem.h"

#include <array>
#include <optional>
#include <vector>

namespace Weltall
{
	class FGameState;

	/** A script actor as the field saves it: its record's rotation and flags, its sprite and its script state. */
	struct FFieldSavedActor
	{
		FGteVector Rotation{};
		uint16 Flags = 0;
		FSpriteSavedState Sprite;
		FEventActorState State;
	};

	/**
	 * The field as it saves itself on leaving for a battle or Battling, which its next start comes back
	 * to instead of running the map's init: every script actor, the walkmesh's attribute words, the
	 * variables, the camera and the view the scripts project through, and the settings the scripts
	 * made; the rest the Field program's start gives afresh. With them, the words of the console's
	 * memory the field finds as it left them: the party's characters and actors, the gears' actors, and
	 * the slots' riding as it left.
	 */
	struct FFieldSnapshot
	{
		/** The map's scripts the actors' threads ran in (FEventContext::ScriptsKey). */
		uint32 ScriptsKey = 0;
		std::vector<FFieldSavedActor> Actors;
		std::vector<uint32> WalkAttributes;
		FEventVariables Variables;
		/** All of it but the follow's and return's ticks, the kept height, the view's pitch, the kept look-at and the world's rotation. */
		FFieldCamera Camera;

		// The render state's: all but the drawing's clip, the culling margins, the pictures of the screen,
		// the VRAM changes and scrollers, the CLUT fades, the markers and the cockpit.
		FMdlColor BackgroundColor{};
		uint16 FarBucket = 0;
		FFieldFog Fog;
		FFieldPanorama Panorama;
		FFieldGearLights GearLights;
		uint8 SceneryDrift = 0;
		std::array<int16, 3> DriftStep{};
		std::array<int32, 3> DriftTotal{};
		std::array<FFieldFade, 2> Fades{};
		FFieldScreenWave ScreenWave;

		/** The encounters' period, count and schedule (their weights and formation are the console's). */
		int32 EncounterPeriod = 0;
		int32 EncountersPerPeriod = 0;
		int32 EncounterCountdown = 0;
		std::array<uint16, FFieldEncounters::MaxPerPeriod> EncountersPending{};

		// The controls', the movement's and the positional sounds' settings.
		uint16 ScriptMask = 0;
		bool bReversed = false;
		bool bPartyPlaced = false;
		bool bFollowClose = false;
		bool bLeaderApart = false;
		FFieldMovement Movement;
		uint16 SoundListener = 0;
		int16 SoundRange = 0;
		uint16 SoundKeepMask = 0;

		// The gears' arrays (their models are loaded again from them).
		int32 GearCount = 0;
		std::array<uint16, FFieldGears::Capacity> GearFileNumbers{};
		std::array<int16, FFieldGears::Capacity> GearAnimations{};
		std::array<int32, FFieldGears::Capacity> GearLoadScales{};
		std::array<uint8, FFieldGears::Capacity> GearTextureShifts{};
		int16 GearPoolSize = 0;

		uint16 PlayerLock = 0;
		bool bMenuLocked = false;
		bool bCompassHidden = false;
		uint8 EncounterBattleKind = 0;
		uint8 ScriptedBattleKind = 0;
		uint16 GearBoardingOverride = 0;
		uint16 GearTurnSpeed = 0;
		bool bPartyWalking = false;
		uint16 OpenWindows = 0;
		int16 WindowOpenTicks = 0;
		int32 FrameDelay = 0;
		uint8 LeaderActor = 0;
		uint8 PilotSpriteBase = 0;
		int16 PartyWalkHurries = 0;
		uint32 LeaderFlagsBeforeWalk = 0;
		uint8 PauseDisabled = 0;

		// The console's memory.
		/** The files the party's sprite slots held (FPartySpriteFiles::File). */
		std::array<int32, 3> PartySpriteFiles{};
		std::array<uint8, 3> PartyCharacters{};
		std::array<uint8, 3> PartyActors{};
		std::array<uint8, 3> GearActors{};
		std::array<uint8, 3> Riding{};
	};

	/** The field's state as the field keeps it for its return, its actors' sprites saved through Models. */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> SaveFieldForReturn(const FEventContext& Context, IFieldActorModels& Models, FFieldSnapshot& Saved);

	/**
	 * The map's init when the field comes back: the saved state copied over the load's (each script
	 * state but the MIMe weights' block the load made), then every script actor's sprite made again
	 * from its script state as the load makes it (with its texture slots), and in gear mode the pilots'
	 * and their gears' sprites exchanged as the party rides.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> RestoreFieldForReturn(const FFieldSnapshot& Saved, const FGameState& Game, FEventContext& Context);

	/**
	 * The load's tail's own steps when the field comes back, after the gears' load: the extra animation
	 * sets read again and played, the sprites' saved states put back (not an actor's whose animations are
	 * off, nor in gear mode a pilot's or gear's while a slot's riding changed since), a screen wave on
	 * made again, the map's start variables, every script actor's model matrix.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> RestoreFieldSprites(const FFieldSnapshot& Saved, const FGameState& Game, FEventContext& Context);

	/**
	 * The entry's step when the field comes back, before its ticks: the first actor's script 2 run
	 * aside, the riding members marked as pilots, every gear playing its last animation again, a
	 * leader riding an actor raised a little, and the scenery's drift so far applied to the records.
	 */
	[[nodiscard]] WELTALLCORE_API std::optional<FError> RestoreFieldScene(const FGameState& Game, FEventContext& Context);
}
