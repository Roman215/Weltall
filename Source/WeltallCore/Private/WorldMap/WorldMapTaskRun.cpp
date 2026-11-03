#include "WorldMap/WorldMapOps.h"

#include "WorldMap/WorldMapCutscenes.h"

#include "Field/IdsFieldSprite.h"
#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "WorldMap/WorldMapStateLayout.h"

namespace Weltall::WorldMapOps
{
	namespace
	{
		/** The frame task's init. */
		FResult InitFrame(FWorldMapContext&, int32)
		{
			return WorldMapTaskState::Update;
		}
	}

	FResult RunRoutine(FWorldMapContext& Context, int32 Slot, EWorldMapRoutine Routine)
	{
		switch (Routine)
		{
		case EWorldMapRoutine::InitFade: return InitFade(Context, Slot);
		case EWorldMapRoutine::UpdateFade: return UpdateFade(Context, Slot);
		case EWorldMapRoutine::InitLeader: return InitLeader(Context, Slot);
		case EWorldMapRoutine::RestoreLeader: return RestoreLeader(Context, Slot);
		case EWorldMapRoutine::UpdateLeader: return UpdateLeader(Context, Slot);
		case EWorldMapRoutine::InitMember2: return InitMember(Context, Slot, 1);
		case EWorldMapRoutine::InitMember3: return InitMember(Context, Slot, 2);
		case EWorldMapRoutine::RestoreMember2: return RestoreMember(Context, Slot, 1);
		case EWorldMapRoutine::RestoreMember3: return RestoreMember(Context, Slot, 2);
		case EWorldMapRoutine::UpdateMember: return UpdateMember(Context, Slot);
		case EWorldMapRoutine::InitGear1: return InitGear(Context, Slot, 0);
		case EWorldMapRoutine::InitGear2: return InitGear(Context, Slot, 1);
		case EWorldMapRoutine::InitGear3: return InitGear(Context, Slot, 2);
		case EWorldMapRoutine::RestoreGear1: return RestoreGear(Context, Slot, 0);
		case EWorldMapRoutine::RestoreGear2: return RestoreGear(Context, Slot, 1);
		case EWorldMapRoutine::RestoreGear3: return RestoreGear(Context, Slot, 2);
		case EWorldMapRoutine::UpdateGear1: return UpdateGear1(Context, Slot);
		case EWorldMapRoutine::UpdateGear: return UpdateGear(Context, Slot);
		case EWorldMapRoutine::InitVehicle: return InitVehicle(Context, Slot);
		case EWorldMapRoutine::RestoreVehicle: return RestoreVehicle(Context, Slot);
		case EWorldMapRoutine::UpdateVehicle: return UpdateVehicle(Context, Slot);
		case EWorldMapRoutine::InitVehicleParts: return InitVehicleParts(Context, Slot);
		case EWorldMapRoutine::RestoreVehicleParts: return RestoreVehicleParts(Context, Slot);
		case EWorldMapRoutine::UpdateVehicleParts: return UpdateVehicleParts(Context, Slot);
		case EWorldMapRoutine::InitCameraTarget: return InitCameraTarget(Context, Slot);
		case EWorldMapRoutine::UpdateCameraTarget: return UpdateCameraTarget(Context, Slot);
		case EWorldMapRoutine::InitCameraDistance: return InitCameraDistance(Context, Slot);
		case EWorldMapRoutine::UpdateCameraDistance: return UpdateCameraDistance(Context, Slot);
		case EWorldMapRoutine::InitScreenCentre: return InitScreenCentre(Context, Slot);
		case EWorldMapRoutine::UpdateScreenCentre: return UpdateScreenCentre(Context, Slot);
		case EWorldMapRoutine::InitPlaceName: return InitPlaceName(Context, Slot);
		case EWorldMapRoutine::UpdatePlaceName: return UpdatePlaceName(Context, Slot);
		case EWorldMapRoutine::InitNotice: return InitNotice(Context, Slot);
		case EWorldMapRoutine::UpdateNotice: return UpdateNotice(Context, Slot);
		case EWorldMapRoutine::InitFrame: return InitFrame(Context, Slot);
		case EWorldMapRoutine::UpdateFrame: return UpdateFrame(Context, Slot);
		case EWorldMapRoutine::InitSpinner: return InitSpinner(Context, Slot);
		case EWorldMapRoutine::UpdateSpinner: return UpdateSpinner(Context, Slot);
		case EWorldMapRoutine::InitThames: return InitThames(Context, Slot);
		case EWorldMapRoutine::UpdateThames: return UpdateThames(Context, Slot);
		case EWorldMapRoutine::RestoreThames: return RestoreThames(Context, Slot);
		case EWorldMapRoutine::InitShevat: return InitShevat(Context, Slot);
		case EWorldMapRoutine::UpdateShevat: return UpdateShevat(Context, Slot);
		case EWorldMapRoutine::RestoreShevat: return RestoreShevat(Context, Slot);
		case EWorldMapRoutine::InitSpinnerPair: return InitSpinnerPair(Context, Slot);
		case EWorldMapRoutine::UpdateSpinnerPair: return UpdateSpinnerPair(Context, Slot);
		case EWorldMapRoutine::InitFastSpinnerPair: return InitFastSpinnerPair(Context, Slot);
		case EWorldMapRoutine::UpdateFastSpinnerPair: return UpdateFastSpinnerPair(Context, Slot);
		case EWorldMapRoutine::RestoreFastSpinnerPair: return RestoreFastSpinnerPair(Context, Slot);
		case EWorldMapRoutine::InitStoryObject: return InitStoryObject(Context, Slot);
		case EWorldMapRoutine::UpdateStoryObject: return UpdateStoryObject(Context, Slot);
		case EWorldMapRoutine::RestoreStoryObject: return RestoreStoryObject(Context, Slot);
		case EWorldMapRoutine::InitLandmarkColliderA: return InitLandmarkCollider(Context, Slot, ELandmark::A);
		case EWorldMapRoutine::UpdateLandmarkColliderA: return UpdateLandmarkCollider(Context, Slot, ELandmark::A);
		case EWorldMapRoutine::InitLandmarkColliderB: return InitLandmarkCollider(Context, Slot, ELandmark::B);
		case EWorldMapRoutine::UpdateLandmarkColliderB: return UpdateLandmarkCollider(Context, Slot, ELandmark::B);
		case EWorldMapRoutine::InitLandmarkColliderC: return InitLandmarkCollider(Context, Slot, ELandmark::C);
		case EWorldMapRoutine::UpdateLandmarkColliderC: return UpdateLandmarkCollider(Context, Slot, ELandmark::C);
		case EWorldMapRoutine::InitEmptyEraSlot:
		case EWorldMapRoutine::UpdateEmptyEraSlot:
			return WorldMapTaskState::Idle;
		default:
			if (const std::optional<FResult> Result = RunCutsceneRoutine(Context, Slot, Routine))
			{
				return *Result;
			}
			break;
		}
		Context.Fault = MakeError("the world map's task routine " + std::to_string(static_cast<int32>(Routine)) + " (slot " + std::to_string(Slot)
			+ ") is not built");
		return Context.Tasks[static_cast<std::size_t>(Slot)].State;
	}

	void RunTasks(FWorldMapContext& Context)
	{
		for (int32 Slot = 0; Slot < FWorldMapContext::TaskCount && !Context.Fault; ++Slot)
		{
			FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
			if (Task.Update == EWorldMapRoutine::None)
			{
				continue;
			}
			switch (Task.State)
			{
			case WorldMapTaskState::Init:
				Task.State = RunRoutine(Context, Slot, Task.Init);
				break;
			case WorldMapTaskState::Update:
				Task.State = RunRoutine(Context, Slot, Task.Update);
				break;
			case WorldMapTaskState::Waiting:
				Task.Countdown = static_cast<int16>(Task.Countdown - 1);
				if (Task.Countdown < 1)
				{
					Task.State = WorldMapTaskState::Update;
				}
				break;
			case WorldMapTaskState::FreeSprite:
				Context.Fault = MakeError("a world map task asked to free its sprite, which the program would free again every tick");
				break;
			default:
				break;
			}
		}
	}

	void AddTask(FWorldMapContext& Context, EWorldMapRoutine Init, EWorldMapRoutine Update)
	{
		for (FWorldMapTask& Task : Context.Tasks)
		{
			if (Task.Update == EWorldMapRoutine::None)
			{
				Task.State = WorldMapTaskState::Init;
				Task.Countdown = 0;
				Task.Message = 0;
				Task.Init = Init;
				Task.Update = Update;
				Task.Step = 0;
				Task.Timer = 0;
				return;
			}
		}
	}

	void SetTask(FWorldMapContext& Context, EWorldMapRoutine Init, int32 Slot)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		Task.State = WorldMapTaskState::Init;
		Task.Init = Init;
	}

	bool SendMessage(FWorldMapContext& Context, int32 Slot, int16 Message)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		if (Task.Message != 0)
		{
			return false;
		}
		Task.State = WorldMapTaskState::Update;
		Task.Message = Message;
		return true;
	}

	uint8 PartyCharacter(const FWorldMapContext& Context, int32 Slot)
	{
		return Context.Game->Block()[WorldMapStateLayout::Party + static_cast<std::size_t>(Slot)];
	}

	uint8 Riding(const FWorldMapContext& Context, int32 Slot)
	{
		return Context.Game->Block()[WorldMapStateLayout::Riding + static_cast<std::size_t>(Slot)];
	}

	void SetRiding(FWorldMapContext& Context, int32 Slot, uint8 Value)
	{
		Context.Game->Block()[WorldMapStateLayout::Riding + static_cast<std::size_t>(Slot)] = Value;
	}

	uint8 CharacterGear(const FWorldMapContext& Context, int32 Character)
	{
		// An empty slot's FFh reads past the characters, a byte of the program.
		if (Character == 0xFF)
		{
			return Context.Tables->EmptySlotGear;
		}
		return Context.Game->Block()[WorldMapStateLayout::CharacterGear(static_cast<std::size_t>(Character))];
	}

	int16 StateWord(const FWorldMapContext& Context, std::size_t Offset)
	{
		return LittleEndian::ReadS16(Context.Game->Block(), Offset);
	}

	uint16 VehicleFlags(const FWorldMapContext& Context)
	{
		return Context.bVehicleAway ? 0 : static_cast<uint16>(StateWord(Context, WorldMapStateLayout::VehicleFlags));
	}

	void SetStateWord(FWorldMapContext& Context, std::size_t Offset, int16 Value)
	{
		LittleEndian::WriteU16(Context.Game->Block(), Offset, static_cast<uint16>(Value));
	}

	void SetControlled(FWorldMapContext& Context, const FWorldMapTask& Task)
	{
		Context.Controlled = Task.Position;
		Context.ControlledHeading = Task.Heading;
	}

	void FillTrail(FWorldMapContext& Context, const FWorldMapVector& Position, int16 Heading)
	{
		Context.TrailIndex = 0;
		for (FWorldMapTrailEntry& Entry : Context.Trail)
		{
			Entry.Position = Position;
			Entry.Heading = Heading;
		}
	}

	void PushTrail(FWorldMapContext& Context, const FWorldMapTask& Task)
	{
		Context.TrailIndex = static_cast<int16>((Context.TrailIndex + 1) & (FWorldMapContext::TrailLength - 1));
		FWorldMapTrailEntry& Entry = Context.Trail[static_cast<std::size_t>(Context.TrailIndex)];
		Entry.Position = Task.Position;
		Entry.Heading = Task.Heading;
	}

	void FindTrigger(FWorldMapContext& Context, const FWorldMapVector& Position, int32 List)
	{
		const std::optional<std::vector<FWorldMapTrigger>>& Triggers = Context.Scene->TriggerLists[static_cast<std::size_t>(List)];
		const FWorldMapTriggerHit Hit = FindWorldMapTrigger(*Triggers, Position);
		Context.Place = Hit.Place;
		Context.Notice = Hit.Notice;
		Context.PlaceRecord = Hit.Record;
		Context.PlaceList = Hit.Record ? List : -1;
		Context.PlaceIndex = Hit.RecordIndex;
	}

	void AddShadow(FWorldMapContext& Context, int16 Kind, const FWorldMapVector& Position)
	{
		FWorldMapShadow& Shadow = Context.Shadows[static_cast<std::size_t>(Context.ShadowCount)];
		Shadow.X = static_cast<int16>(Position.X >> 12);
		Shadow.Kind = Kind;
		Shadow.Z = static_cast<int16>(Position.Z >> 12);
		Context.ShadowCount = (Context.ShadowCount + 1) & (FWorldMapContext::ShadowCapacity - 1);
	}

	FSprite* SlotSprite(FWorldMapContext& Context, const FWorldMapTask& Task)
	{
		return Task.Sprite == FWorldMapTask::NoSprite ? nullptr : Context.SpriteSlots[static_cast<std::size_t>(Task.Sprite)].get();
	}

	int32 SpriteAnimation(FWorldMapContext& Context, const FWorldMapTask& Task)
	{
		return static_cast<int8>(SlotSprite(Context, Task)->TimeFlags >> SpriteTimeFlags::AnimationShift);
	}

	void SetSpriteAnimation(FWorldMapContext& Context, const FWorldMapTask& Task, int32 Animation)
	{
		Context.Sprites.SetAnimation(*SlotSprite(Context, Task), Animation);
	}

	void CreateSprite(FWorldMapContext& Context, int32 Slot, const FSpriteFile& File, int32 ClutX, int32 ClutY, int32 TextureX, int32 TextureY,
		int32 Animation, int32 Scale)
	{
		FWorldMapTask& Task = Context.Tasks[static_cast<std::size_t>(Slot)];
		std::unique_ptr<FSprite>& Sprite = Context.SpriteSlots[static_cast<std::size_t>(Slot)];
		Sprite = std::make_unique<FSprite>();
		Context.SpritePlaces[static_cast<std::size_t>(Slot)] = {static_cast<int16>(ClutX), static_cast<int16>(ClutY), static_cast<int16>(TextureX), static_cast<int16>(TextureY)};
		// The one resident file a member has is Id's, which draws from his picture in the port's rows, his two palettes
		// beside it (the slots' palettes lie only 16 colours apart).
		if (File.IsResident(File.Tables()))
		{
			Context.Sprites.Create(*Sprite, File, FIdsFieldSprite::ClutX, FIdsFieldSprite::ClutY, FIdsFieldSprite::TextureX, FIdsFieldSprite::TextureY);
		}
		else
		{
			Context.Sprites.Create(*Sprite, File, ClutX, ClutY, TextureX, TextureY);
		}
		Context.Sprites.SetAnimation(*Sprite, Animation);
		Context.Sprites.SetScale(*Sprite, Scale);
		Sprite->DrawFlags &= ~WorldMapSpriteDrawFlag;
		Task.Sprite = Slot;
	}

	void PlaySong(FWorldMapContext& Context, EWorldMapSong Song)
	{
		if (IWorldMapSound* Sound = Context.Sound)
		{
			Sound->StopAllSongs();
			Sound->FreeSong();
			Sound->LoadSong(Song);
			Sound->PlaySong(WorldMapFullVolume, 0);
		}
	}

	void SetSongVolume(FWorldMapContext& Context, int32 Volume, int32 Ticks)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->SetSongVolume(Volume, Ticks);
		}
	}

	void PlaySceneEffect(FWorldMapContext& Context, int32 Effect)
	{
		if (Context.Sound != nullptr)
		{
			Context.Sound->PlayEffect(Effect);
		}
	}

	void UpdateDust(FWorldMapContext& Context, int32 Effect, const FWorldMapTask& Task)
	{
		if (Context.Ground.TerrainType(Task.Position.X, Task.Position.Z) == WorldMapTerrainType::Sand)
		{
			const std::array<int16, 4> Place{static_cast<int16>(Task.Position.X >> 12), static_cast<int16>(Task.Position.Y >> 12),
				static_cast<int16>(Task.Position.Z >> 12), 0};
			const FGteVector Rotation{0, static_cast<int16>(Task.Word(WorldMapTaskWord::DrawnFacing)), 0};
			Context.Particles.Start(Effect, Place, Rotation);
		}
		else
		{
			Context.Particles.Stop(Effect);
		}
	}

	void ApplyMountFlags(FWorldMapContext& Context)
	{
		for (int32 PartySlot = 0; PartySlot < 3; ++PartySlot)
		{
			FWorldMapTask& Member = Context.Tasks[static_cast<std::size_t>(FWorldMapContext::LeaderSlot + PartySlot)];
			FWorldMapTask& Gear = Context.Tasks[static_cast<std::size_t>(FWorldMapContext::GearSlot + PartySlot)];
			const uint8 Before = static_cast<uint8>(StateWord(Context, WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(PartySlot)));
			if (Riding(Context, PartySlot) == Before)
			{
				continue;
			}
			if (Riding(Context, PartySlot) == 0)
			{
				Member.Position.X = Gear.Position.X;
				Member.Position.Y = Gear.Position.Y;
				Member.Position.Z = Gear.Position.Z;
				Member.Word(WorldMapTaskWord::Follow) = Gear.Word(WorldMapTaskWord::Follow);
			}
			else
			{
				SetStateWord(Context, WorldMapStateLayout::GearFlags(static_cast<std::size_t>(PartySlot)), WorldMapStateLayout::GearOnMap);
				Gear.Hidden = 0;
				Gear.Position.X = Member.Position.X;
				Gear.Position.Y = Member.Position.Y;
				Gear.Position.Z = Member.Position.Z;
				Gear.Word(WorldMapTaskWord::Follow) = Member.Word(WorldMapTaskWord::Follow);
			}
		}
		int32 Riders = 0;
		for (int32 PartySlot = 0; PartySlot < 3; ++PartySlot)
		{
			if (PartyCharacter(Context, PartySlot) != 0xFF && Riding(Context, PartySlot) == 1)
			{
				++Riders;
			}
		}
		if ((VehicleFlags(Context) & WorldMapStateLayout::Aboard) == 0)
		{
			Context.TravelMode = Riders == 0 ? WorldMapTravel::OnFoot : WorldMapTravel::Gears;
		}
	}

	void ResetEncounters(FWorldMapContext& Context)
	{
		Context.Encounters.Reset((VehicleFlags(Context) & WorldMapStateLayout::Aboard) != 0);
	}
}
