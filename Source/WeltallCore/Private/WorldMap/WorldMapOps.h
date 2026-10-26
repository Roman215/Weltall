#pragma once

#include "WorldMap/WorldMapContext.h"

#include <optional>

namespace Weltall::WorldMapOps
{
	/** The routines' results: the slot's next state. */
	using FResult = int16;

	/** Runs one routine for a slot; its result is the slot's next state. */
	FResult RunRoutine(FWorldMapContext& Context, int32 Slot, EWorldMapRoutine Routine);
	/** Every slot with an update, in order, once. */
	void RunTasks(FWorldMapContext& Context);
	/** The first slot without an update takes the pair. */
	void AddTask(FWorldMapContext& Context, EWorldMapRoutine Init, EWorldMapRoutine Update);
	/** A slot runs another init first, its update and words kept. */
	void SetTask(FWorldMapContext& Context, EWorldMapRoutine Init, int32 Slot);
	/** Taken only when the slot has none waiting; the slot then runs its update. */
	bool SendMessage(FWorldMapContext& Context, int32 Slot, int16 Message);

	/** The party's slot characters (FFh none) and their riding bytes, from the game state. */
	uint8 PartyCharacter(const FWorldMapContext& Context, int32 Slot);
	uint8 Riding(const FWorldMapContext& Context, int32 Slot);
	void SetRiding(FWorldMapContext& Context, int32 Slot, uint8 Value);
	/** A character's gear (FFh none), as the program reads its record, for any number. */
	uint8 CharacterGear(const FWorldMapContext& Context, int32 Character);

	/** The game state's world-map words. */
	int16 StateWord(const FWorldMapContext& Context, std::size_t Offset);
	void SetStateWord(FWorldMapContext& Context, std::size_t Offset, int16 Value);
	/** The ship's flags (its type, start aboard, aboard) as the visit has them: none while the ship is away. */
	uint16 VehicleFlags(const FWorldMapContext& Context);

	/** The shared state the tasks keep up: the controlled task's place and heading, the trail. */
	void SetControlled(FWorldMapContext& Context, const FWorldMapTask& Task);
	void FillTrail(FWorldMapContext& Context, const FWorldMapVector& Position, int16 Heading);
	void PushTrail(FWorldMapContext& Context, const FWorldMapTask& Task);
	/** The trigger under Position on list List (0 on foot, 1 in gears, 2 in the vehicle). */
	void FindTrigger(FWorldMapContext& Context, const FWorldMapVector& Position, int32 List);
	/** A shadow of a kind at a place. */
	void AddShadow(FWorldMapContext& Context, int16 Kind, const FWorldMapVector& Position);

	/** The sprites' calls, on a slot's sprite. */
	FSprite* SlotSprite(FWorldMapContext& Context, const FWorldMapTask& Task);
	/** The party's sprites given their own time scales back once their walk is no longer slowed (analog movement). */
	void RestorePartyPace(FWorldMapContext& Context);
	int32 SpriteAnimation(FWorldMapContext& Context, const FWorldMapTask& Task);
	void SetSpriteAnimation(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Animation);
	/** A sprite made for a slot, with the program's scale and its draw flag cleared. */
	void CreateSprite(FWorldMapContext& Context, int32 Slot, const FSpriteFile& File, int32 ClutX, int32 ClutY, int32 TextureX, int32 TextureY,
		int32 Animation, int32 Scale);

	/** Every song stopped, the current one freed, Song loaded and played at full volume. */
	void PlaySong(FWorldMapContext& Context, EWorldMapSong Song);
	/** The current song's volume: Volume over Ticks of the sound driver's. */
	void SetSongVolume(FWorldMapContext& Context, int32 Volume, int32 Ticks);
	/** The scene's sound effect Effect played. */
	void PlaySceneEffect(FWorldMapContext& Context, int32 Effect);

	/** The effect under a task on sand, stopped elsewhere. */
	void UpdateDust(FWorldMapContext& Context, int32 Effect, const FWorldMapTask& Task);
	/** The gears brought to the members who mount, the members out where their gears stand. */
	void ApplyMountFlags(FWorldMapContext& Context);
	/** The encounters reset on the game state's vehicle flag. */
	void ResetEncounters(FWorldMapContext& Context);

	/** The party. */
	FResult InitLeader(FWorldMapContext& Context, int32 Slot);
	FResult RestoreLeader(FWorldMapContext& Context, int32 Slot);
	FResult UpdateLeader(FWorldMapContext& Context, int32 Slot);
	FResult InitMember(FWorldMapContext& Context, int32 Slot, int32 PartySlot);
	FResult RestoreMember(FWorldMapContext& Context, int32 Slot, int32 PartySlot);
	FResult UpdateMember(FWorldMapContext& Context, int32 Slot);

	/** The gears. */
	FResult InitGear(FWorldMapContext& Context, int32 Slot, int32 PartySlot);
	FResult RestoreGear(FWorldMapContext& Context, int32 Slot, int32 PartySlot);
	FResult UpdateGear1(FWorldMapContext& Context, int32 Slot);
	FResult UpdateGear(FWorldMapContext& Context, int32 Slot);

	/** The Yggdrasil. */
	FResult InitVehicle(FWorldMapContext& Context, int32 Slot);
	FResult RestoreVehicle(FWorldMapContext& Context, int32 Slot);
	FResult UpdateVehicle(FWorldMapContext& Context, int32 Slot);
	FResult InitVehicleParts(FWorldMapContext& Context, int32 Slot);
	FResult RestoreVehicleParts(FWorldMapContext& Context, int32 Slot);
	FResult UpdateVehicleParts(FWorldMapContext& Context, int32 Slot);

	/** The camera. */
	FResult InitCameraTarget(FWorldMapContext& Context, int32 Slot);
	FResult UpdateCameraTarget(FWorldMapContext& Context, int32 Slot);
	FResult InitCameraDistance(FWorldMapContext& Context, int32 Slot);
	FResult UpdateCameraDistance(FWorldMapContext& Context, int32 Slot);
	FResult InitScreenCentre(FWorldMapContext& Context, int32 Slot);
	FResult UpdateScreenCentre(FWorldMapContext& Context, int32 Slot);
	/** The context's camera orbit, at a distance and angles (pitch, yaw, roll). */
	void SetCameraOrbit(FWorldMapContext& Context, int32 Distance, const std::array<int16, 3>& Angles);
	/** The camera matrix from the angles and the orbit's eye. */
	void SetCameraMatrix(FWorldMapContext& Context);
	/** The camera matrix looking from the orbit's eye at its target, its up vector up (the matrix's pad kept). */
	void LookAtCamera(FWorldMapContext& Context);
	/** The angles (x, y, z) a rotation turns by, left as they were where its last row has neither x nor z. */
	void AnglesFromMatrix(const FGteMatrix& Matrix, const FGteArctangents& Arctangents, FGteVector& Angles);
	/** A uniform quadratic B-spline through three points at Fraction (0-FFFh), in 16.16 units. */
	std::array<int32, 3> QuadraticBlend(int32 Fraction, const std::array<int16, 4>& First, const std::array<int16, 4>& Second, const std::array<int16, 4>& Third);

	/** The fade and the windows. */
	FResult InitFade(FWorldMapContext& Context, int32 Slot);
	FResult UpdateFade(FWorldMapContext& Context, int32 Slot);
	FResult InitPlaceName(FWorldMapContext& Context, int32 Slot);
	FResult UpdatePlaceName(FWorldMapContext& Context, int32 Slot);
	FResult InitNotice(FWorldMapContext& Context, int32 Slot);
	FResult UpdateNotice(FWorldMapContext& Context, int32 Slot);

	/** The frame's logic (the frame's task without its drawing). */
	FResult UpdateFrame(FWorldMapContext& Context, int32 Slot);
	/** The cutscenes' frame tasks' logic: the map's without the sprites and shadows, with or without the trees. */
	FResult UpdateCutsceneFrame(FWorldMapContext& Context, int32 Slot, EWorldMapFrameTask Kind);
	/** The slot waits for a message. */
	FResult InitIdleTask(FWorldMapContext& Context, int32 Slot);

	/** The era's objects. */
	FResult InitSpinner(FWorldMapContext& Context, int32 Slot);
	FResult UpdateSpinner(FWorldMapContext& Context, int32 Slot);
	FResult InitThames(FWorldMapContext& Context, int32 Slot);
	FResult UpdateThames(FWorldMapContext& Context, int32 Slot);
	FResult RestoreThames(FWorldMapContext& Context, int32 Slot);
	FResult InitShevat(FWorldMapContext& Context, int32 Slot);
	FResult UpdateShevat(FWorldMapContext& Context, int32 Slot);
	FResult RestoreShevat(FWorldMapContext& Context, int32 Slot);
	FResult InitSpinnerPair(FWorldMapContext& Context, int32 Slot);
	FResult UpdateSpinnerPair(FWorldMapContext& Context, int32 Slot);
	FResult InitFastSpinnerPair(FWorldMapContext& Context, int32 Slot);
	FResult UpdateFastSpinnerPair(FWorldMapContext& Context, int32 Slot);
	FResult RestoreFastSpinnerPair(FWorldMapContext& Context, int32 Slot);
	FResult InitStoryObject(FWorldMapContext& Context, int32 Slot);
	FResult UpdateStoryObject(FWorldMapContext& Context, int32 Slot);
	FResult RestoreStoryObject(FWorldMapContext& Context, int32 Slot);
	/** The colliders standing on landmarks: A on the era's first landmark (64 units lower), B on model 75, C on the era's second. */
	enum class ELandmark : uint8
	{
		A,
		B,
		C,
	};
	FResult InitLandmarkCollider(FWorldMapContext& Context, int32 Slot, ELandmark Landmark);
	FResult UpdateLandmarkCollider(FWorldMapContext& Context, int32 Slot, ELandmark Landmark);

	/** The clouds' places and looks, with rand(). */
	void InitClouds(FWorldMapContext& Context);

	/** The clouds' logic: each cloud moved by its drift, wrapped within the clouds' four tiles. */
	void DriftClouds(FWorldMapContext& Context);

	/** The place models. */
	void InitObjects(FWorldMapContext& Context);
	void AttachObject(FWorldMapContext& Context, int32 Parent, int32 Child);
	/** The objects' logic: each attached object's translation from its place. */
	void PlaceAttachedObjects(FWorldMapContext& Context);
}
