#include "WorldMap/WorldMapOps.h"

#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Gte.h"
#include "WorldMap/WorldMapInput.h"
#include "WorldMap/WorldMapStateLayout.h"

#include <algorithm>

namespace Weltall::WorldMapOps
{
	void RestorePartyPace(FWorldMapContext& Context)
	{
		for (const auto& [Slot, TimeScale] : Context.SlowedSprites)
		{
			if (FSprite* Sprite = SlotSprite(Context, Context.Tasks[static_cast<std::size_t>(Slot)]))
			{
				Sprite->SetTimeScale(static_cast<int32>(TimeScale));
			}
		}
		Context.SlowedSprites.clear();
	}

	namespace
	{
		constexpr int32 MemberSpeed = 8;
		constexpr int32 MemberScale = 0x1800;
		constexpr int32 PartySize = 3;
		constexpr uint32 LargestTimeScale = 0xFFF;
		constexpr int32 FirstDustEffect = 0x2E;
		constexpr int32 LeaderDust = FirstDustEffect + FWorldMapContext::LeaderSlot;
		/** How far behind on the trail members 2 and 3 walk. */
		constexpr std::array<int32, 3> TrailOffsets{0, 15, 30};
		/** The Yggdrasil's collider may be walked into; the party's radius and height on foot. */
		constexpr int32 FootRadius = 0x10;
		constexpr int32 FootHeight = 0x20;
		/** A member steps out of its gear 48 units ahead of it. */
		constexpr int32 StepOutReach = 0x30;

		/** Messages the party's tasks take. */
		constexpr int16 WalkToSlotMessage = 1;
		constexpr int16 LeaveGearMessage = 2;
		constexpr int16 ResumeMessage = 3;
		constexpr int16 AboardMessage = 4;
		constexpr int16 GatherMessage = 5;
		constexpr int16 BackOnFootMessage = 6;
		constexpr int16 GearsGatherMessage = 8;

		namespace LeaderStep
		{
			constexpr int16 Walking = 0;
			constexpr int16 Riding = 1;
			constexpr int16 Aboard = 2;
			constexpr int16 Sailing = 3;
			constexpr int16 BoardSecond = 8;
			constexpr int16 BoardThird = 9;
			constexpr int16 BoardSelf = 10;
			constexpr int16 WalkToShip = 0xD;
			constexpr int16 WalkingToShip = 0xE;
			constexpr int16 Embark = 0xF;
			constexpr int16 GatherSecond = 0x10;
			constexpr int16 GatherThird = 0x11;
			constexpr int16 GatherAtGear = 0x12;
			constexpr int16 StepOut = 0x28;
			constexpr int16 SteppingOut = 0x29;
			constexpr int16 OnFoot = 0x2A;
			constexpr int16 WaitForMembers = 0x2B;
			constexpr int16 CallSecond = 0x2C;
			constexpr int16 CallThird = 0x2D;
			constexpr int16 Done = 0x40;
		}

		namespace MemberStep
		{
			constexpr int16 Following = 0;
			constexpr int16 Riding = 1;
			constexpr int16 Aboard = 2;
			constexpr int16 WalkToSlot = 8;
			constexpr int16 WalkingToSlot = 9;
			constexpr int16 Embark = 10;
			constexpr int16 StepOut = 0x28;
			constexpr int16 SteppingOut = 0x29;
			constexpr int16 OnFoot = 0x2A;
			constexpr int16 WalkToLeader = 0x30;
			constexpr int16 WalkingToLeader = 0x31;
			constexpr int16 Rejoin = 0x32;
			constexpr int16 Done = 0x40;
		}

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		bool SameSpot(const FWorldMapVector& A, const FWorldMapVector& B)
		{
			return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
		}

		void CopySpot(FWorldMapVector& To, const FWorldMapVector& From)
		{
			To.X = From.X;
			To.Y = From.Y;
			To.Z = From.Z;
		}

		/** Heads a task toward a point and sets it walking there (whole units kept as its walk's target). */
		void HeadFor(FWorldMapContext& Context, FWorldMapTask& Task, const FWorldMapVector& Target)
		{
			const FWorldMapHeading Heading = WorldMapDirectionTo(Task.Position, Target, *Context.Arctangents);
			Task.Heading = Heading.Heading;
			Task.Direction.X = Heading.DirectionX;
			Task.Direction.Z = Heading.DirectionZ;
			Task.Word(WorldMapTaskWord::WalkX) = Target.X >> 12;
			Task.Word(WorldMapTaskWord::WalkZ) = Target.Z >> 12;
		}

		int32 WalkOn(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			return WalkTowardWorldMapPoint(Context.Ground, Task.Position, Task.Direction, Task.Speed, Task.Word(WorldMapTaskWord::WalkX),
				Task.Word(WorldMapTaskWord::WalkZ));
		}

		/** Where a member steps out of its parked gear: the gear's place and heading from the game state, and a point ahead. */
		void StepOutOfGear(FWorldMapContext& Context, FWorldMapTask& Task, int32 PartySlot)
		{
			const std::size_t Gear = static_cast<std::size_t>(PartySlot);
			Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearX(Gear)))) << 12);
			Task.Position.Z = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearZ(Gear)))) << 12);
			Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
			const uint16 Heading = static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearHeading(Gear)));
			Task.Word(WorldMapTaskWord::DrawnFacing) = Heading;
			Task.Heading = static_cast<int16>(Heading);
			FWorldMapVector Ahead = Task.Position;
			Ahead.X = Task.Position.X + GteSin(static_cast<int16>(Heading)) * StepOutReach;
			Ahead.Z = Task.Position.Z + GteCos(Task.Heading) * -StepOutReach;
			HeadFor(Context, Task, Ahead);
		}

		void WriteLeaderPlace(FWorldMapContext& Context, const FWorldMapTask& Task)
		{
			SetStateWord(Context, WorldMapStateLayout::PartyX, static_cast<int16>(Task.Position.X >> 12));
			SetStateWord(Context, WorldMapStateLayout::PartyZ, static_cast<int16>(Task.Position.Z >> 12));
			SetStateWord(Context, WorldMapStateLayout::LeaderHeading, Task.Heading);
		}

		/** Analog movement's walk on foot: the share of the walk's pace a light push gives (FullTilt: not slowed). */
		int32 WalkPace(const FWorldMapContext& Context)
		{
			return Context.Stick ? Context.Stick->Pace(FAnalogStick::RunTilt) : FAnalogStick::FullTilt;
		}

		/** The party's sprites slowed as much as their walk, each one's own time scale kept for RestorePartyPace. */
		void PaceParty(FWorldMapContext& Context, int32 Pace)
		{
			for (int32 Slot = FWorldMapContext::LeaderSlot; Slot < FWorldMapContext::LeaderSlot + PartySize; ++Slot)
			{
				FSprite* Sprite = SlotSprite(Context, TaskAt(Context, Slot));
				if (Sprite == nullptr)
				{
					continue;
				}
				auto Kept = std::find_if(Context.SlowedSprites.begin(), Context.SlowedSprites.end(), [Slot](const auto& Each) { return Each.first == Slot; });
				if (Kept == Context.SlowedSprites.end())
				{
					Kept = Context.SlowedSprites.insert(Context.SlowedSprites.end(), {Slot, Sprite->TimeScale()});
				}
				Sprite->SetTimeScale(static_cast<int32>(std::min<uint32>(Kept->second * FAnalogStick::FullTilt / static_cast<uint32>(Pace), LargestTimeScale)));
			}
			Context.bPartyPaced = true;
		}

		/** The leader's walk: the move, the colliders, the trail and the encounters, the triggers of its list. */
		void WalkControlled(FWorldMapContext& Context, FWorldMapTask& Task, int32 Dust, int32 Radius, int32 Height, int32 List)
		{
			if (Task.Direction.X == 0 && Task.Direction.Y == 0 && Task.Direction.Z == 0)
			{
				if (SpriteAnimation(Context, Task) != 0)
				{
					SetSpriteAnimation(Context, Task, 0);
					Context.Particles.Stop(Dust);
				}
			}
			else
			{
				if (SpriteAnimation(Context, Task) != 1)
				{
					SetSpriteAnimation(Context, Task, 1);
				}
				UpdateDust(Context, Dust, Task);
			}
			const int32 Pace = WalkPace(Context);
			if (Pace != FAnalogStick::FullTilt)
			{
				PaceParty(Context, Pace);
			}
			const int32 Speed = static_cast<int32>(int64{Task.Speed << 12} * Pace / FAnalogStick::FullTilt);
			FWorldMapVector& Out = Context.MoveResult;
			bool bFree = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
			if (!bFree)
			{
				Task.Direction = Out;
				bFree = MoveOnWorldMapGround(Context.Ground, Task.Position, Task.Direction, Out, Speed, Context.TravelMode);
				if (!bFree)
				{
					Task.Direction.Z = 0;
					Task.Direction.X = 0;
				}
			}
			if (bFree)
			{
				Context.Contact = Context.Colliders.Find(Context.Ground, Out, Radius, Height);
				const std::size_t Pass = Context.Contact.Slot == FWorldMapContext::VehicleSlot ? Context.Contact.Contact + 3u : Context.Contact.Contact;
				if (Context.Tables->ColliderPasses[Pass] != 0)
				{
					Task.Position = Out;
					if (Task.Direction.X != 0 || Task.Direction.Z != 0)
					{
						PushTrail(Context, Task);
						Context.Encounters.Tick(*Context.Random);
					}
				}
			}
			else
			{
				Context.Contact = Context.Colliders.Find(Context.Ground, Task.Position, Radius, Height);
			}
			FindTrigger(Context, Task.Position, List);
			Task.Direction.Z = 0;
			Task.Direction.Y = 0;
			Task.Direction.X = 0;
			SetControlled(Context, Task);
		}
	}

	FResult InitLeader(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		CreateSprite(Context, Slot, *Context.MemberFiles[0], 0x100, 0x1E0, 0x140, 0x100, 0, MemberScale);
		Task.Hidden = 0;
		Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::StoryVariable))) << 12);
		Task.Position.Z = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::SecondVariable))) << 12);
		Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
		Task.Direction.Z = 0;
		Task.Direction.Y = 0;
		Task.Direction.X = 0;
		Task.Speed = MemberSpeed;
		Task.Heading = static_cast<int16>(Context.StartYaw);
		Task.Word(WorldMapTaskWord::DrawnFacing) = Task.Heading;
		switch (Context.TravelMode)
		{
		case 1:
		case 2:
		case 3:
			if (Riding(Context, 0) == 0)
			{
				Task.Position.X = Context.Start.X;
				Task.Position.Z = Context.Start.Z;
				Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				Task.Heading = static_cast<int16>(Context.StartYaw);
				SetControlled(Context, Task);
			}
			else
			{
				Task.Step = LeaderStep::Riding;
				Task.Hidden = 1;
			}
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			Task.Step = Context.TravelMode == 7 ? LeaderStep::Aboard : LeaderStep::Sailing;
			Task.Hidden = 1;
			if (PartyCharacter(Context, 0) != 0xFF)
			{
				SetRiding(Context, 0, 1);
			}
			break;
		default:
			break;
		}
		FillTrail(Context, Task.Position, Task.Heading);
		WriteLeaderPlace(Context, Task);
		return WorldMapTaskState::Update;
	}

	FResult RestoreLeader(FWorldMapContext& Context, int32 Slot)
	{
		CreateSprite(Context, Slot, *Context.MemberFiles[0], 0x100, 0x1E0, 0x140, 0x100, 0, MemberScale);
		return WorldMapTaskState::Update;
	}

	FResult UpdateLeader(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 Members = Context.MemberCount;
		switch (Task.Message)
		{
		case ResumeMessage:
			Task.Message = 0;
			Task.Step = LeaderStep::Riding;
			break;
		case LeaveGearMessage:
			Task.Message = 0;
			Task.Step = LeaderStep::StepOut;
			break;
		case BackOnFootMessage:
			Task.Message = 0;
			++Task.Word(WorldMapTaskWord::Follow);
			if (Members == Task.Word(WorldMapTaskWord::Follow))
			{
				Task.Step = LeaderStep::Walking;
				Context.TravelMode = WorldMapTravel::OnFoot;
				Context.Colliders.Count = 0;
			}
			break;
		default:
			break;
		}

		FWorldMapTask& Vehicle = TaskAt(Context, FWorldMapContext::VehicleSlot);
		std::optional<int16> NextStep;
		switch (Task.Step)
		{
		case LeaderStep::Walking:
		case LeaderStep::Riding:
			if (Riding(Context, 0) == 0)
			{
				switch (ReadFootInput(Context, Task))
				{
				case EWorldMapInput::Enter:
					Task.Step = LeaderStep::Done;
					Context.Running = 0;
					Context.ExitCode = 0;
					break;
				case EWorldMapInput::Board:
					if (Context.Contact.Slot == FWorldMapContext::VehicleSlot)
					{
						Task.Step = LeaderStep::BoardSecond;
					}
					else
					{
						Context.Contact.Slot = FWorldMapContext::GearSlot;
						Task.Step = LeaderStep::GatherSecond;
					}
					break;
				default:
					WalkControlled(Context, Task, LeaderDust, FootRadius, FootHeight, 0);
					break;
				}
				Context.Colliders.Count = 0;
				Task.Hidden = 0;
			}
			else
			{
				const FWorldMapTask& Gear = TaskAt(Context, FWorldMapContext::GearSlot);
				Task.Hidden = 1;
				CopySpot(Task.Position, Gear.Position);
				Task.Heading = Gear.Heading;
				Context.Particles.Stop(LeaderDust);
			}
			break;
		case LeaderStep::Aboard:
		case LeaderStep::Sailing:
			CopySpot(Task.Position, Vehicle.Position);
			Task.Heading = Vehicle.Heading;
			break;
		case LeaderStep::BoardSecond:
		case LeaderStep::BoardThird:
		{
			const int32 PartySlot = Task.Step == LeaderStep::BoardSecond ? 1 : 2;
			if (PartyCharacter(Context, PartySlot) != 0xFF)
			{
				if (Riding(Context, PartySlot) == 0)
				{
					SendMessage(Context, FWorldMapContext::LeaderSlot + PartySlot, WalkToSlotMessage);
					TaskAt(Context, FWorldMapContext::LeaderSlot + PartySlot).MessageArgument = Context.Contact.Slot;
					SendMessage(Context, FWorldMapContext::GearSlot + PartySlot, GearsGatherMessage);
				}
				else
				{
					SendMessage(Context, FWorldMapContext::GearSlot + PartySlot, WalkToSlotMessage);
					TaskAt(Context, FWorldMapContext::GearSlot + PartySlot).MessageArgument = Context.Contact.Slot;
				}
			}
			NextStep = static_cast<int16>(Task.Step + 1);
			break;
		}
		case LeaderStep::BoardSelf:
			if (PartyCharacter(Context, 0) != 0xFF)
			{
				if (SendMessage(Context, FWorldMapContext::GearSlot, GearsGatherMessage))
				{
					Task.Step = LeaderStep::WalkToShip;
				}
				break;
			}
			NextStep = LeaderStep::WalkToShip;
			break;
		case LeaderStep::WalkToShip:
		case LeaderStep::WalkingToShip:
			if (Task.Step == LeaderStep::WalkToShip)
			{
				HeadFor(Context, Task, TaskAt(Context, Context.Contact.Slot).Position);
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			SetControlled(Context, Task);
			UpdateDust(Context, Slot + FirstDustEffect, Task);
			break;
		case LeaderStep::Embark:
			if (SendMessage(Context, Context.Contact.Slot, AboardMessage))
			{
				Task.Hidden = 1;
				Task.Step = LeaderStep::Aboard;
				Context.Particles.Stop(LeaderDust);
			}
			break;
		case LeaderStep::GatherSecond:
		case LeaderStep::GatherThird:
		{
			const int32 PartySlot = Task.Step == LeaderStep::GatherSecond ? 1 : 2;
			if (PartyCharacter(Context, PartySlot) == 0xFF)
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			else if (SendMessage(Context, FWorldMapContext::LeaderSlot + PartySlot, WalkToSlotMessage))
			{
				TaskAt(Context, FWorldMapContext::LeaderSlot + PartySlot).MessageArgument = static_cast<int16>(FWorldMapContext::GearSlot + PartySlot);
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		}
		case LeaderStep::GatherAtGear:
		{
			FWorldMapVector Spot;
			Spot.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearX(0)))) << 12);
			Spot.Z = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::GearZ(0)))) << 12);
			Spot.Y = Context.Ground.Height(Spot.X, Spot.Z);
			FillTrail(Context, Spot, StateWord(Context, WorldMapStateLayout::GearHeading(0)));
			NextStep = LeaderStep::WalkToShip;
			break;
		}
		case LeaderStep::StepOut:
		case LeaderStep::SteppingOut:
			if (Task.Step == LeaderStep::StepOut)
			{
				StepOutOfGear(Context, Task, 0);
				Task.Hidden = 0;
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
				break;
			}
			if (WalkOn(Context, Task) == 3)
			{
				SetSpriteAnimation(Context, Task, 0);
				Task.Direction.Z = 0;
				Task.Direction.Y = 0;
				Task.Direction.X = 0;
				++Task.Step;
			}
			SetControlled(Context, Task);
			break;
		case LeaderStep::OnFoot:
		case LeaderStep::WaitForMembers:
			if (Task.Step == LeaderStep::OnFoot)
			{
				SetRiding(Context, 0, 0);
				Task.Word(WorldMapTaskWord::Follow) = 1;
				FillTrail(Context, Task.Position, Task.Heading);
				++Task.Step;
			}
			if (Context.MemberCount == 2)
			{
				if (Riding(Context, 1) != 0)
				{
					break;
				}
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			else if (Context.MemberCount < 3)
			{
				if (Context.MemberCount == 1)
				{
					NextStep = LeaderStep::Riding;
				}
			}
			else if (Context.MemberCount == 3 && Riding(Context, 1) == 0 && Riding(Context, 2) == 0)
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case LeaderStep::CallSecond:
			if (PartyCharacter(Context, 1) == 0xFF || SendMessage(Context, FWorldMapContext::LeaderSlot + 1, GatherMessage))
			{
				NextStep = static_cast<int16>(Task.Step + 1);
			}
			break;
		case LeaderStep::CallThird:
			if (PartyCharacter(Context, 2) == 0xFF || SendMessage(Context, FWorldMapContext::LeaderSlot + 2, GatherMessage))
			{
				NextStep = LeaderStep::Done;
			}
			break;
		default:
			break;
		}
		if (NextStep)
		{
			Task.Step = *NextStep;
		}
		WriteLeaderPlace(Context, Task);
		if (Task.Hidden == 0)
		{
			AddShadow(Context, 0, Task.Position);
		}
		return WorldMapTaskState::Update;
	}

	FResult InitMember(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		FResult Result = WorldMapTaskState::Update;
		if (PartyCharacter(Context, PartySlot) == 0xFF)
		{
			Task.Hidden = 1;
			Result = WorldMapTaskState::Idle;
		}
		else
		{
			CreateSprite(Context, Slot, *Context.MemberFiles[static_cast<std::size_t>(PartySlot)], 0x100 + 0x10 * PartySlot, 0x1E0,
				0x140 + 0x10 * PartySlot, 0x100, 0, MemberScale);
			Task.Hidden = 0;
		}
		Task.Position.X = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::PartyX))) << 12);
		Task.Position.Z = static_cast<int32>(static_cast<uint32>(static_cast<uint16>(StateWord(Context, WorldMapStateLayout::PartyZ))) << 12);
		Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
		Task.Direction.Z = 0;
		Task.Direction.Y = 0;
		Task.Direction.X = 0;
		Task.Speed = MemberSpeed;
		Task.Word(WorldMapTaskWord::Follow) = TrailOffsets[static_cast<std::size_t>(PartySlot)];
		Task.Heading = StateWord(Context, WorldMapStateLayout::LeaderHeading);
		Task.Word(WorldMapTaskWord::DrawnFacing) = Task.Heading;
		switch (Context.TravelMode)
		{
		case 1:
		case 2:
		case 3:
			if (Riding(Context, PartySlot) == 0)
			{
				Task.Position.X = Context.Start.X;
				Task.Position.Z = Context.Start.Z;
				Task.Position.Y = Context.Ground.Height(Task.Position.X, Task.Position.Z);
				Task.Heading = static_cast<int16>(Context.StartYaw);
				return Result;
			}
			Task.Step = MemberStep::Riding;
			Task.Hidden = 1;
			return Result;
		case 4:
		case 5:
		case 6:
		case 7:
			Task.Step = Context.TravelMode == 7 ? MemberStep::Aboard : static_cast<int16>(3);
			Task.Hidden = 1;
			if (PartyCharacter(Context, PartySlot) != 0xFF)
			{
				SetRiding(Context, PartySlot, 1);
			}
			return Result;
		default:
			return Result;
		}
	}

	FResult RestoreMember(FWorldMapContext& Context, int32 Slot, int32 PartySlot)
	{
		if (PartyCharacter(Context, PartySlot) == 0xFF)
		{
			return WorldMapTaskState::Idle;
		}
		CreateSprite(Context, Slot, *Context.MemberFiles[static_cast<std::size_t>(PartySlot)], 0x100 + 0x10 * PartySlot, 0x1E0,
			0x140 + 0x10 * PartySlot, 0x100, 0, MemberScale);
		return WorldMapTaskState::Update;
	}

	FResult UpdateMember(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 PartySlot = Slot - FWorldMapContext::LeaderSlot;
		const int32 Dust = Slot + FirstDustEffect;
		int32 Target = -1;
		switch (Task.Message)
		{
		case LeaveGearMessage:
			Task.Message = 0;
			Task.Step = MemberStep::StepOut;
			break;
		case ResumeMessage:
			Task.Message = 0;
			Task.Step = MemberStep::Riding;
			break;
		case GatherMessage:
			Task.Message = 0;
			Task.Step = MemberStep::WalkToLeader;
			break;
		case WalkToSlotMessage:
			Task.Message = 0;
			Task.Step = MemberStep::WalkToSlot;
			Target = Task.MessageArgument;
			break;
		default:
			break;
		}

		switch (Task.Step)
		{
		case MemberStep::Following:
		case MemberStep::Riding:
			if (Riding(Context, PartySlot) == 0)
			{
				const FWorldMapTrailEntry& Entry = Context.Trail[static_cast<std::size_t>((Context.TrailIndex - Task.Word(WorldMapTaskWord::Follow)) & 0x1F)];
				if (SameSpot(Task.Position, Entry.Position))
				{
					if (SpriteAnimation(Context, Task) != 0)
					{
						SetSpriteAnimation(Context, Task, 0);
						Context.Particles.Stop(Dust);
					}
				}
				else
				{
					if (SpriteAnimation(Context, Task) != 1)
					{
						SetSpriteAnimation(Context, Task, 1);
					}
					UpdateDust(Context, Dust, Task);
				}
				CopySpot(Task.Position, Entry.Position);
				Task.Hidden = 0;
				Task.Heading = Entry.Heading;
			}
			else
			{
				const FWorldMapTask& Gear = TaskAt(Context, Slot + 3);
				CopySpot(Task.Position, Gear.Position);
				Task.Hidden = 1;
				Task.Heading = Gear.Heading;
				Context.Particles.Stop(Dust);
			}
			break;
		case MemberStep::Aboard:
		{
			const FWorldMapTask& Vehicle = TaskAt(Context, FWorldMapContext::VehicleSlot);
			CopySpot(Task.Position, Vehicle.Position);
			Task.Heading = Vehicle.Heading;
			break;
		}
		case MemberStep::WalkToSlot:
		case MemberStep::WalkingToSlot:
			if (Task.Step == MemberStep::WalkToSlot)
			{
				if (Target < 0)
				{
					Context.Fault = MakeError("a world map member walks to a slot no message named");
					break;
				}
				HeadFor(Context, Task, TaskAt(Context, Target).Position);
				SetSpriteAnimation(Context, Task, 1);
				++Task.Step;
			}
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			UpdateDust(Context, Dust, Task);
			break;
		case MemberStep::Embark:
			if (SendMessage(Context, Task.MessageArgument, AboardMessage))
			{
				Task.Hidden = 1;
				Task.Step = MemberStep::Aboard;
				Context.Particles.Stop(Dust);
			}
			break;
		case MemberStep::StepOut:
			StepOutOfGear(Context, Task, PartySlot);
			Task.Hidden = 0;
			SetSpriteAnimation(Context, Task, 1);
			++Task.Step;
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			break;
		case MemberStep::SteppingOut:
		case MemberStep::WalkingToLeader:
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			break;
		case MemberStep::OnFoot:
			SetRiding(Context, PartySlot, 0);
			Task.Step = MemberStep::Done;
			break;
		case MemberStep::WalkToLeader:
			HeadFor(Context, Task, TaskAt(Context, FWorldMapContext::LeaderSlot).Position);
			SetSpriteAnimation(Context, Task, 1);
			++Task.Step;
			if (WalkOn(Context, Task) == 3)
			{
				++Task.Step;
			}
			break;
		case MemberStep::Rejoin:
			if (SendMessage(Context, FWorldMapContext::LeaderSlot, BackOnFootMessage))
			{
				Task.Step = MemberStep::Following;
			}
			break;
		default:
			break;
		}
		if (Task.Hidden == 0)
		{
			AddShadow(Context, 0, Task.Position);
		}
		return WorldMapTaskState::Update;
	}
}
