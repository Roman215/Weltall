#pragma once

#include "Battle/Show/BattleShowObjects.h"
#include "Field/FieldScene.h"
#include "Gears/GearEngineSteps.h"

#include <optional>

/** Battle's objects' inner steps, shared by the object system's files; each records a fault where the original breaks. */
namespace Weltall::BattleObjectMachine
{
	using GearMachine::FGearRecord;

	/** The slot a sequence names (FFh the first target, FEh the performer, FDh and F9h itself, FCh its owner, FAh the stage, F8h and F7h its sub-objects), and its bit. */
	uint32 ResolveWho(const FBattleObjects& Objects, const FBattleObject& Object, uint32 Who, uint16& Mask);
	/** The object in a slot a sequence names, a fault past the 32 slots. */
	FBattleObject* NamedObject(FBattleObjects& Objects, uint32 Slot);
	/** The first slot of the play's targets (13 when none). */
	uint32 FirstTarget(const FBattleObjects& Objects);
	/** Where a combatant slot's record places it (x, the height 0, z); none past the eleven records (faults). */
	std::optional<std::array<int16, 3>> SlotPlace(FBattleObjects& Objects, int32 Slot);
	/** Animation Number of the object's tables (FFh and FEh its stance; bit 7 of the stance into bMirrored); none faults. */
	std::optional<FGearRecord> Animation(FBattleObjects& Objects, FBattleObject& Object, uint32 Number, bool& bMirrored);
	/** Whether a target's result class in the record shown passes a filter (1 hit, 8 heal, 4 miss, 2 half; 0 always). */
	bool ResultMatches(FBattleObjects& Objects, int32 Slot, uint8 Filter);
	/** The result class of slot Slot in the record the playback shows, as the record's byte holds it. */
	std::optional<uint8> ResultClass(FBattleObjects& Objects, int32 Slot);
	/** The id in the high half of the sound effects a key event or op 3C names by Base (0 the battle's, 1 the object's, 2 its extension's, 3 the effect's, more a 3). */
	std::optional<uint32> SoundBase(FBattleObjects& Objects, const FBattleObject& Object, uint32 Base);
	/** The id of a pack's sound effect file, read where its sound container's entry points (none past the pack). */
	std::optional<uint16> SoundFileId(const FGearMotionPack& Pack);
	/** A pack's sound effect file, from where its sound container's entry points to the next entry (within the pack). */
	std::span<const uint8> SoundFile(const FGearMotionPack& Pack);

	// What the objects read of the rules' state (BattleObjectStep.cpp); none faults, where the original reads past it.
	/** The combatants the action knocked out. */
	uint16 KnockedOutSlots(const FBattleObjects& Objects);
	/** The code of record Record or the first after it that is not F7h (FFh gives -2). */
	std::optional<int32> PeekCode(FBattleObjects& Objects, int32 Record);
	/** The stage group of slot Slot's record, and the gears of its group's enemies' side (the occupancy's count). */
	std::optional<uint8> SlotGroup(FBattleObjects& Objects, int32 Slot);
	std::optional<uint8> GearGroupCount(FBattleObjects& Objects, int32 Slot);
	/** A gear's stance flags: 1 mirrored, 2 weak (HP below an eighth), 4 at attack level 4. */
	std::optional<uint8> StanceFlags(FBattleObjects& Objects, int32 Slot);
	/** The byte of slot Slot's combatant record key event 1 adds to its animation: +101h + Number. */
	std::optional<uint8> GearEffectBase(FBattleObjects& Objects, int32 Slot, int32 Number);

	/** An object's motion made ready: the sequence tables, nothing running, no motion. */
	void InitMotion(FBattleObject& Object, FGearMotionPack* Motion);
	/** Op 04's read of an extension, and op 05's install of it: its images, its sound effects, its animations and its sprite resource. */
	void LoadExtension(FBattleObjects& Objects, FBattleObject& Object, int32 File);
	void InstallExtension(FBattleObjects& Objects, FBattleObject& Object);

	// The sequences (BattleObjectSequence.cpp) and the key events (BattleObjectKeyEvents.cpp).
	/** The object's sequence for a step: its motion (1 + CatchUp times), its handlers, then its instructions until one waits. */
	void RunSequence(FBattleObjects& Objects, int32 Slot, int32 Events, int32 Ticks, int32 CatchUp);
	void StartKeyEvents(FBattleObject& Object, const FGearRecord& Record, int32 Loop);
	void RunKeyEvents(FBattleObjects& Objects, int32 Slot);

	// The objects' motion and their place (BattleObjectStep.cpp).
	void SnapToFloor(FBattleObjects& Objects, FBattleObject& Object);
	void UpdateTargetPoint(FBattleObjects& Objects, FBattleObject& Object);
	void FollowOwner(FBattleObjects& Objects, FBattleObject& Object);
	void PushOut(FBattleObjects& Objects, int32 Slot, int32 X, int32 Z, int32 Radius);
	int32 TargetDistance(FBattleObjects& Objects, const FBattleObject& Object);

	// The model camera's tracks (BattleModelCamera.cpp).
	void StartCameraTrack(FBattleObjects& Objects, int32 Track, uint8 Kind, uint8 Tag, const std::array<int16, 6>& Words, int16 Ticks);
	/** Frame is the draw buffer, by which the floor under the eye is looked for once a frame. */
	void StepCameraTracks(FBattleObjects& Objects, int32 Ticks, int32 Frame);
	void FreeCameraTracks(FBattleObjects& Objects);

	// Clones and freeing (BattleObjectCreate.cpp).
	void Clone(FBattleObjects& Objects, int32 Slot, int32 HiddenBone, uint32 Sequence);
	void FreeExtension(FBattleObjects& Objects, FBattleObject& Object);

	// Trails (BattleObjectDraw.cpp): a quad of the pool for an emitter (the spare quad when the pool is full).
	int32 AllocateTrailSegment(FBattleObjects& Objects, uint8 Blend);

	// The part drawing (BattleObjectPolygons.cpp).
	/** How the kernel's part drawing draws a part: its draw mode, the table's shift, the entry the table it links into starts at, the box culling. */
	struct FPartDrawing
	{
		EFieldDrawMode Mode = EFieldDrawMode::Relit;
		int32 Shift = 2;
		int32 FirstEntry = 0;
		bool bBoxCulled = false;
	};
	/**
	 * A bone's part under Gte (its transform the part's matrix, its light matrix the bone's) into the
	 * frame, unless its box is off the screen: each polygon tested as the kernel's handlers test it, its colours lit
	 * again in the relighting mode, its texels made when it is environment-mapped, and linked. Projected is scratch.
	 */
	void DrawPart(FBattleObjects& Objects, FBattleObjectsFrame& Frame, std::vector<FGteProjection>& Projected, const FGte& Gte, const FBattleDrawnPart& Part,
		const FPartDrawing& How);
	/**
	 * The packets a making built for bone Bone's part (Packets: an object's own or its dress's), looked for from Cursor on (the
	 * bones are drawn in order); none faults, naming the object's slot.
	 */
	const FGearBonePackets* BonePackets(FBattleObjects& Objects, std::span<const FGearBonePackets> Packets, int32 Slot, std::size_t Bone, std::size_t& Cursor);
	/** The stage object's shown bones' parts under View into the frame, each by its draw kind. */
	void DrawStageModel(FBattleObjects& Objects, FBattleObjectsFrame& Frame, std::vector<FGteProjection>& Projected, const FBattleObject& Stage,
		const FGteMatrix& View);

	/** What a CLUT animation starts from (sequence op 1B, key event 9). */
	struct FBattleClutStart
	{
		uint8 Type = 0;
		/** The sources' kinds (low nibble the first, the next the second: 1 read, 2 a colour) and the buffers wanted (100h the result, 200h the second, 400h the first). */
		uint32 Flags = 0;
		/** The animation of the object the result feeds, -1 none. */
		int16 Feeds = -1;
		bool bColourMatrix = false;
		/** The first source's place or colour, then the second's. */
		std::array<uint16, 6> Values{};
		int16 X = 0;
		int16 Y = 0;
		int16 Width = 0;
		int16 Height = 0;
		int16 Speed = 0;
		int16 Divisor = 0;
		int16 Base = 0;
		EBattleClutCurve Curve = EBattleClutCurve::Cosine;
	};

	// CLUT animations (BattleClutAnimation.cpp).
	void StartClutAnimation(FBattleObjects& Objects, FBattleClutAnimation& Animation, const FBattleClutStart& Start);
	void StopClutAnimation(FBattleObjects& Objects, FBattleClutAnimation& Animation);
	void StepClutAnimation(FBattleObjects& Objects, FBattleObject& Object, FBattleClutAnimation& Animation, int32 CatchUp);
	/** A CLUT animation's curve: 1 linear, 2 down, 3 up, anything else the cosine. */
	EBattleClutCurve ClutCurve(uint32 Number);
}
