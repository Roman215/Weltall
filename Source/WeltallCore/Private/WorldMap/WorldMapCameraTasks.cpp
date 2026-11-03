#include "WorldMap/WorldMapOps.h"

#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"
#include "WorldMap/WorldMapInput.h"
#include "WorldMap/WorldMapPlayerCamera.h"

#include <cstdlib>

namespace Weltall::WorldMapOps
{
	namespace
	{
		constexpr int16 FollowHeadingMessage = 9;
		constexpr int16 FollowPadMessage = 10;
		constexpr int16 BehindShipMessage = 0xB;
		constexpr int16 FixedYawMessages = 15;
		constexpr int16 FixedYaws[3] = {0x800, 0xA00, 0xC00};
		constexpr int16 FlyingPresetsMessage = 9;
		constexpr int16 GroundPresetsMessage = 10;
		constexpr int16 FreeCameraMessage = 0xE;
		constexpr int16 CloseUpMessage = 0x11;
		constexpr int32 GroundPresets = 0;
		constexpr int32 FlyingPresets = 1;
		/** The start's distance and pitch before the presets take over. */
		constexpr int32 StartDistance = 0x460000;
		constexpr int32 FlyingStartDistance = 0x280000;
		constexpr int16 StartPitch = static_cast<int16>(0xFDA0);
		constexpr int32 TurnStep = 0x40;
		constexpr int32 TurnSize = 0x200;

		namespace TargetStep
		{
			constexpr int16 Follow = 0;
			constexpr int16 Turning = 1;
			constexpr int16 FollowHeading = 2;
			constexpr int16 Locked = 3;
			constexpr int16 FixedYaw = 0x10;
		}

		namespace DistanceStep
		{
			constexpr int16 Settled = 0;
			constexpr int16 Moving = 1;
			constexpr int16 Free = 2;
			constexpr int16 CloseUp = 3;
		}

		FWorldMapTask& TaskAt(FWorldMapContext& Context, int32 Slot)
		{
			return Context.Tasks[static_cast<std::size_t>(Slot)];
		}

		const std::array<int16, 4>& PitchTable(const FWorldMapContext& Context, int32 Table)
		{
			return Table == FlyingPresets ? Context.Tables->Camera.FlyingPitches : Context.Tables->Camera.Pitches;
		}

		const std::array<int16, 4>& ClearanceTable(const FWorldMapContext& Context, int32 Table)
		{
			return Table == FlyingPresets ? Context.Tables->Camera.FlyingClearances : Context.Tables->Camera.Clearances;
		}

		/** The short way round from one yaw to another, folded past Limit. */
		int32 Fold(int32 Delta, int32 Limit)
		{
			if (std::abs(Delta) > Limit)
			{
				Delta += Delta < 0 ? 0x1000 : -0x1000;
			}
			return Delta;
		}

		/** The yaw eased toward the goal by an eighth (or a divisor's share), in 20.12. */
		void EaseYaw(FWorldMapContext& Context, FWorldMapTask& Task, int32 Delta, int32 Shift)
		{
			const uint32 Yaw = static_cast<uint32>(Task.Word(WorldMapTaskWord::SmoothYaw) + ((Delta << 12) >> Shift)) & 0xFFFFFF;
			Task.Word(WorldMapTaskWord::SmoothYaw) = static_cast<int32>(Yaw);
			Context.CameraYaw = static_cast<int16>(static_cast<int32>(Yaw) >> 12);
		}

		/** The camera's preset: the nearest whose height clears the terrain around where its eye would be. */
		int32 ChoosePreset(FWorldMapContext& Context, int32 Current, int32 Pitches, int32 Clearances)
		{
			const std::array<int16, 4>& PitchValues = PitchTable(Context, Pitches);
			const std::array<int16, 4>& ClearanceValues = ClearanceTable(Context, Clearances);
			const int32 Height = Context.Controlled.Y >> 12;
			int32 Preset = 0;
			int32 Highest = 0;
			for (; Preset < 3; ++Preset)
			{
				Highest = 0;
				const std::array<int16, 3> Angles{PitchValues[static_cast<std::size_t>(Preset)], Context.CameraYaw, Context.CameraRoll};
				SetCameraOrbit(Context, Context.Tables->Camera.Distances[static_cast<std::size_t>(Preset)] - (Context.ProjectionDistance / 2) * 0x1000, Angles);
				const uint32 RowStart = static_cast<uint32>(Context.CameraTarget.X + (Context.Orbit.Eye[0] - 0x180) * 0x1000);
				FWorldMapVector Probe;
				Probe.Z = static_cast<int32>(static_cast<uint32>(Context.CameraTarget.Z + (Context.Orbit.Eye[2] + 0x180) * -0x1000) & 0xFFF80000u);
				for (int32 Row = 0; Row < 6; ++Row)
				{
					Probe.X = static_cast<int32>(RowStart & 0xFFF80000u);
					for (int32 Column = 0; Column < 6; ++Column)
					{
						Context.Ground.Wrap(Probe);
						const int32 Corner = static_cast<int8>(Context.Ground.CornerWord(Probe.X, Probe.Z, 0) & 0xFF);
						if (Corner < Highest)
						{
							Highest = Corner;
						}
						Probe.X += 0x80000;
					}
					Probe.Z += 0x80000;
				}
				if (ClearanceValues[static_cast<std::size_t>(Preset)] + Height + 0x50 < Highest * 8)
				{
					break;
				}
			}
			if (Preset < Current)
			{
				// Stepping back needs a margin, measured against the ground presets' clearance whatever table is in use.
				const int32 Margin = std::abs((Context.Tables->Camera.Clearances[static_cast<std::size_t>(Current)] + Height) - (Highest * 8 - 0x50));
				if (Margin > 0x40)
				{
					return Preset;
				}
				return Current;
			}
			return Preset;
		}
	}

	void SetCameraOrbit(FWorldMapContext& Context, int32 Distance, const std::array<int16, 3>& Angles)
	{
		FWorldMapOrbit& Orbit = Context.Orbit;
		Orbit.Target = {0, static_cast<int16>(Context.CameraTarget.Y >> 12), 0};
		Orbit.Eye = WorldMapOrbitEye(Orbit.Target[1], Distance, Angles[0], Angles[1]);
		const FGteMatrix Roll = RotationYxz({0, Angles[1], Angles[2]});
		Orbit.Up = ApplyMatrix(Roll, {0, static_cast<int16>(0xF000), 0});
	}

	void SetCameraMatrix(FWorldMapContext& Context)
	{
		Context.CameraMatrix = WorldMapCameraMatrix(Context.Orbit.Eye, Context.CameraPitch, Context.CameraYaw, Context.CameraRoll);
		Context.CameraMatrixPad = static_cast<int16>(Context.CameraMatrix.M[2][2] < 0 ? -1 : 0);
	}

	FResult InitCameraTarget(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		Task.Position = Context.Controlled;
		Context.CameraRoll = 0;
		Context.CameraYaw = Context.ControlledHeading;
		Task.Word(WorldMapTaskWord::GoalYaw) = Context.ControlledHeading;
		Task.Word(WorldMapTaskWord::SmoothYaw) = Context.CameraYaw << 12;
		// Mode 6, which nothing sets, counts as flying throughout.
		if (Context.TravelMode > WorldMapTravel::SeaShip && Context.TravelMode <= WorldMapTravel::Flying)
		{
			Task.Step = TargetStep::Locked;
		}
		return WorldMapTaskState::Update;
	}

	FResult UpdateCameraTarget(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		switch (Task.Message)
		{
		case FollowHeadingMessage:
			Task.Message = 0;
			Task.Step = TargetStep::FollowHeading;
			Task.Word(WorldMapTaskWord::GoalYaw) = Context.ControlledHeading;
			Task.Word(WorldMapTaskWord::SmoothYaw) = Context.CameraYaw << 12;
			break;
		case FollowPadMessage:
			Task.Message = 0;
			Task.Step = TargetStep::Follow;
			Task.Word(WorldMapTaskWord::GoalYaw) = Context.CameraYaw;
			break;
		case FixedYawMessages:
		case FixedYawMessages + 1:
		case FixedYawMessages + 2:
			Task.Step = TargetStep::FixedYaw;
			Task.Word(WorldMapTaskWord::GoalYaw) = FixedYaws[Task.Message - FixedYawMessages];
			Task.Message = 0;
			break;
		default:
			break;
		}
		const int16 Step = Task.Step;
		switch (Step)
		{
		case TargetStep::Follow:
		case TargetStep::Turning:
		{
			if (Step == TargetStep::Follow)
			{
				const uint16 Turn = static_cast<uint16>(((Context.Pad.Held & ~Context.Pad.ZoomHeld) >> 2) & 3);
				if (Turn == 2)
				{
					Task.Step = TargetStep::Turning;
					Task.Word(WorldMapTaskWord::TurnStep) = TurnStep;
					Task.Word(WorldMapTaskWord::TurnEnd) = (Task.Word(WorldMapTaskWord::GoalYaw) + TurnSize) & 0xFFF;
				}
				else if (Turn == 1 || Turn == 3)
				{
					Task.Step = TargetStep::Turning;
					Task.Word(WorldMapTaskWord::TurnStep) = -TurnStep;
					Task.Word(WorldMapTaskWord::TurnEnd) = (Task.Word(WorldMapTaskWord::GoalYaw) - TurnSize) & 0xFFF;
				}
			}
			else
			{
				Task.Word(WorldMapTaskWord::GoalYaw) = (Task.Word(WorldMapTaskWord::GoalYaw) + Task.Word(WorldMapTaskWord::TurnStep)) & 0xFFF;
				if (Task.Word(WorldMapTaskWord::GoalYaw) == Task.Word(WorldMapTaskWord::TurnEnd))
				{
					Task.Step = TargetStep::Follow;
				}
			}
			const int32 Yaw = Context.CameraYaw;
			if (Yaw == Task.Word(WorldMapTaskWord::GoalYaw))
			{
				Task.Word(WorldMapTaskWord::SmoothYaw) = Yaw << 12;
			}
			else
			{
				EaseYaw(Context, Task, Fold(Task.Word(WorldMapTaskWord::GoalYaw) - Yaw, 0xC00), 3);
			}
			break;
		}
		case TargetStep::FollowHeading:
		case TargetStep::FixedYaw:
		{
			int32 Delta = Fold(Task.Word(WorldMapTaskWord::GoalYaw) - Context.CameraYaw, 0x800);
			if (std::abs(Delta) > 0x180)
			{
				Delta = Delta < 0 ? -0x180 : 0x180;
			}
			EaseYaw(Context, Task, Delta, Step == TargetStep::FixedYaw ? 5 : 3);
			if (Context.CameraYaw == Task.Word(WorldMapTaskWord::GoalYaw) && Task.Word(WorldMapTaskWord::CatchingUp) == 0)
			{
				Task.Step = TargetStep::Locked;
				if (Step == TargetStep::FollowHeading)
				{
					SendMessage(Context, FWorldMapContext::VehicleSlot, BehindShipMessage);
				}
			}
			break;
		}
		default:
			break;
		}

		const int16 Following = Task.Step;
		const bool bMoved = Task.Position.X != Context.Controlled.X || Task.Position.Y != Context.Controlled.Y || Task.Position.Z != Context.Controlled.Z;
		if (Following == TargetStep::Locked)
		{
			if (bMoved)
			{
				Task.Position.Y += (Context.Controlled.Y - Task.Position.Y) >> 4;
				Task.Position.X = Context.Controlled.X;
				Task.Position.Z = Context.Controlled.Z;
			}
		}
		else if ((Following >= TargetStep::Follow && Following < TargetStep::Locked) || Following == TargetStep::FixedYaw)
		{
			if (bMoved)
			{
				FWorldMapVector Delta{Context.Controlled.X - Task.Position.X, 0, Context.Controlled.Z - Task.Position.Z, 0};
				const int32 Rise = Context.Controlled.Y - Task.Position.Y;
				Context.Ground.WrapDelta(Delta);
				const int32 StepX = Delta.X >> 3;
				const int32 StepY = Rise >> 3;
				const int32 StepZ = Delta.Z >> 3;
				if (std::abs(StepX) < 0x40 && std::abs(StepZ) < 0x40)
				{
					Task.Word(WorldMapTaskWord::CatchingUp) = 0;
					Task.Position.X = Context.Controlled.X;
					Task.Position.Z = Context.Controlled.Z;
				}
				else
				{
					Task.Word(WorldMapTaskWord::CatchingUp) = 1;
					Task.Position.X += StepX;
					Task.Position.Z += StepZ;
				}
				Task.Position.Y += StepY;
			}
		}
		Context.Ground.Wrap(Task.Position);
		Context.CameraTarget = Task.Position;
		return WorldMapTaskState::Update;
	}

	FResult InitCameraDistance(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		const int32 Mode = Context.TravelMode;
		if (Mode > 0 && Mode < 6)
		{
			Task.Word(WorldMapTaskWord::Preset) = 3;
			Context.CameraDistance = StartDistance;
			Context.CameraPitch = StartPitch;
			Task.Word(WorldMapTaskWord::PitchTable) = GroundPresets;
			Task.Word(WorldMapTaskWord::ClearanceTable) = GroundPresets;
		}
		else if (Mode == WorldMapTravel::Flying)
		{
			Task.Word(WorldMapTaskWord::Preset) = 3;
			Context.CameraDistance = FlyingStartDistance;
			Context.CameraPitch = StartPitch;
			Task.Word(WorldMapTaskWord::PitchTable) = FlyingPresets;
			Task.Word(WorldMapTaskWord::ClearanceTable) = FlyingPresets;
		}
		return WorldMapTaskState::Update;
	}

	FResult UpdateCameraDistance(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		switch (Task.Message)
		{
		case GroundPresetsMessage:
		case FlyingPresetsMessage:
		{
			const int32 Table = Task.Message == FlyingPresetsMessage ? FlyingPresets : GroundPresets;
			const int32 Preset = Task.Word(WorldMapTaskWord::Preset);
			Task.Word(WorldMapTaskWord::PitchTable) = Table;
			Task.Word(WorldMapTaskWord::ClearanceTable) = Table;
			Task.Message = 0;
			if (Preset == 0)
			{
				Task.Word(WorldMapTaskWord::Preset) = 1;
			}
			break;
		}
		case FreeCameraMessage:
			Task.Message = 0;
			Task.Step = DistanceStep::Settled;
			Context.LookAt = 0;
			break;
		case CloseUpMessage:
			Context.CameraDistance = 0x400000;
			Task.Step = DistanceStep::CloseUp;
			Task.Message = 0;
			Task.Word(WorldMapTaskWord::GoalPitch) = -0x100;
			break;
		default:
			break;
		}
		const std::array<int16, 4>& Pitches = PitchTable(Context, Task.Word(WorldMapTaskWord::PitchTable));
		bool bOrbit = true;
		switch (Task.Step)
		{
		case DistanceStep::Moving:
		{
			if (Task.Timer > 4)
			{
				const int32 Preset = ChoosePreset(Context, Task.Word(WorldMapTaskWord::Preset), Task.Word(WorldMapTaskWord::PitchTable),
					Task.Word(WorldMapTaskWord::ClearanceTable));
				if (Task.Word(WorldMapTaskWord::Preset) != Preset)
				{
					Task.Word(WorldMapTaskWord::Preset) = Preset;
					Task.Word(WorldMapTaskWord::GoalDistance) = Context.Tables->Camera.Distances[static_cast<std::size_t>(Preset)];
					Task.Word(WorldMapTaskWord::GoalPitch) = Pitches[static_cast<std::size_t>(Preset)];
				}
				Task.Timer = 0;
			}
			const int32 Goal = Task.Word(WorldMapTaskWord::GoalDistance);
			if (Goal != Context.CameraDistance)
			{
				const int32 Difference = Goal - Context.CameraDistance;
				if (Difference < 1)
				{
					Context.CameraDistance -= 0x2000;
				}
				else
				{
					const int32 Growth = Difference > 0x40000 ? 0x8000 : Difference >> 3;
					Context.CameraDistance = Growth < 0x40 ? Goal : Context.CameraDistance + Growth;
				}
			}
			if (Task.Word(WorldMapTaskWord::GoalPitch) != Context.CameraPitch)
			{
				const int32 Difference = (Task.Word(WorldMapTaskWord::GoalPitch) - Context.CameraPitch) * 0x1000;
				int32& Smooth = Task.Word(WorldMapTaskWord::SmoothPitch);
				if (Difference < 0)
				{
					Smooth += Difference >> 5;
				}
				else
				{
					Smooth += Task.Word(WorldMapTaskWord::Preset) == 0 ? 0xF32 : 0x799;
				}
				Context.CameraPitch = static_cast<int16>(Smooth >> 12);
			}
			if (Task.Word(WorldMapTaskWord::GoalDistance) == Context.CameraDistance && Task.Word(WorldMapTaskWord::GoalPitch) == Context.CameraPitch)
			{
				Task.Step = DistanceStep::Settled;
			}
			break;
		}
		case DistanceStep::Settled:
		{
			const int32 Preset = ChoosePreset(Context, Task.Word(WorldMapTaskWord::Preset), Task.Word(WorldMapTaskWord::PitchTable),
				Task.Word(WorldMapTaskWord::ClearanceTable));
			if (Task.Word(WorldMapTaskWord::Preset) != Preset)
			{
				Task.Step = DistanceStep::Moving;
				Task.Timer = 0;
				Task.Word(WorldMapTaskWord::Preset) = Preset;
				Task.Word(WorldMapTaskWord::GoalDistance) = Context.Tables->Camera.Distances[static_cast<std::size_t>(Preset)];
				Task.Word(WorldMapTaskWord::GoalPitch) = Pitches[static_cast<std::size_t>(Preset)];
				Task.Word(WorldMapTaskWord::SmoothPitch) = Context.CameraPitch << 12;
			}
			break;
		}
		case DistanceStep::Free:
			Context.Orbit.Target = {0, static_cast<int16>(Context.CameraTarget.Y >> 12), 0};
			bOrbit = false;
			break;
		case DistanceStep::CloseUp:
			if (Task.Word(WorldMapTaskWord::GoalPitch) != Context.CameraPitch)
			{
				Context.CameraPitch = static_cast<int16>(Context.CameraPitch + 2);
			}
			break;
		default:
			bOrbit = false;
			break;
		}
		if (bOrbit)
		{
			SetCameraOrbit(Context, Context.CameraDistance, {Context.CameraPitch, Context.CameraYaw, Context.CameraRoll});
		}
		Task.Timer = static_cast<int16>(Task.Timer + 1);
		return WorldMapTaskState::Update;
	}

	FResult InitScreenCentre(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		if (Context.TravelMode > 0)
		{
			if (Context.TravelMode < 6)
			{
				Context.ScreenCentreY = 0x8C;
			}
			else if (Context.TravelMode < 8)
			{
				Context.ScreenCentreY = 0x78;
				Task.Step = 1;
			}
		}
		Task.Word(WorldMapTaskWord::CentreY) = Context.ScreenCentreY << 12;
		return WorldMapTaskState::Update;
	}

	FResult UpdateScreenCentre(FWorldMapContext& Context, int32 Slot)
	{
		FWorldMapTask& Task = TaskAt(Context, Slot);
		if (Task.Message == 9)
		{
			Task.Message = 0;
			Task.Step = 1;
		}
		else if (Task.Message == 10)
		{
			Task.Message = 0;
			Task.Step = 2;
		}
		int32& Centre = Task.Word(WorldMapTaskWord::CentreY);
		switch (Task.Step)
		{
		case 0:
			return WorldMapTaskState::Idle;
		case 1:
			Centre -= 0x1000;
			Context.ScreenCentreY = Centre >> 12;
			if (Context.ScreenCentreY > 0x77)
			{
				return WorldMapTaskState::Update;
			}
			Context.ScreenCentreY = 0x78;
			Centre = 0x78000;
			break;
		case 2:
			Centre += 0x1000;
			Context.ScreenCentreY = Centre >> 12;
			if (Context.ScreenCentreY < 0x8C)
			{
				return WorldMapTaskState::Update;
			}
			Context.ScreenCentreY = 0x8C;
			Centre = 0x8C000;
			break;
		default:
			return WorldMapTaskState::Update;
		}
		Task.Step = 0;
		return WorldMapTaskState::Update;
	}
}
