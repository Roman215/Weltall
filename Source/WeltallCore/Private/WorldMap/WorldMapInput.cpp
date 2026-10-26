#include "WorldMap/WorldMapInput.h"

#include "Graphics/Gte.h"
#include "Graphics/GteVectors.h"

#include <algorithm>
#include <cstdlib>

namespace Weltall::WorldMapOps
{
	namespace
	{
		constexpr int16 EnterByTouch = 1;

		/** The stick's heading (right 0, down 400h) turned to the world map's way, where up is the camera's yaw. */
		constexpr int32 StickToWorldMap = 0x400;

		/**
		 * The heading the d-pad gives relative to the yaw of the view shown (a d-pad word, bits 12-15); other combinations
		 * keep the heading. Under analog movement the stick, when it holds the direction, gives any heading.
		 */
		void ReadHeading(FWorldMapContext& Context, FWorldMapTask& Task)
		{
			const int32 Yaw = Context.ViewTurn != 0 ? (Context.CameraYaw + Context.ViewTurn) & 0xFFF : Context.CameraYaw;
			if (Context.Stick)
			{
				Task.Heading = static_cast<int16>((Yaw + Context.Stick->Heading + StickToWorldMap) & 0xFFF);
				Task.Direction.X = GteSin(Task.Heading);
				Task.Direction.Z = -GteCos(Task.Heading);
				return;
			}
			switch (Context.Pad.Held >> 12)
			{
			case 1:
				Task.Heading = static_cast<int16>(Yaw);
				break;
			case 2:
				Task.Heading = static_cast<int16>((Yaw + 0x400) & 0xFFF);
				break;
			case 3:
				Task.Heading = static_cast<int16>((Yaw + 0x200) & 0xFFF);
				break;
			case 4:
				Task.Heading = static_cast<int16>((Yaw + 0x800) & 0xFFF);
				break;
			case 6:
				Task.Heading = static_cast<int16>((Yaw + 0x600) & 0xFFF);
				break;
			case 8:
				Task.Heading = static_cast<int16>((Yaw - 0x400) & 0xFFF);
				break;
			case 9:
				Task.Heading = static_cast<int16>((Yaw - 0x200) & 0xFFF);
				break;
			case 0xC:
				Task.Heading = static_cast<int16>((Yaw - 0x600) & 0xFFF);
				break;
			default:
				break;
			}
			if ((Context.Pad.Held & WorldMapButton::Directions) != 0)
			{
				Task.Direction.X = GteSin(Task.Heading);
				Task.Direction.Z = -GteCos(Task.Heading);
			}
		}

		void RequestMenu(FWorldMapContext& Context)
		{
			if ((Context.Pad.Pressed & WorldMapButton::Menu) != 0 && Context.Notice == -1 && Context.Place == -1)
			{
				Context.MenuRequested = 1;
			}
		}

		/** The Yggdrasil's buttons: confirm on a place enters it, else Circle (when Circle may) leaves the ship, else the menu. */
		EWorldMapInput ReadShipButtons(FWorldMapContext& Context, bool bMayDisembark)
		{
			if ((Context.Pad.Pressed & WorldMapButton::Confirm) != 0 && Context.Place != -1)
			{
				return EWorldMapInput::Enter;
			}
			if ((Context.Pad.Pressed & WorldMapButton::Cancel) != 0 && bMayDisembark)
			{
				return EWorldMapInput::Disembark;
			}
			RequestMenu(Context);
			return EWorldMapInput::None;
		}

		/** A value moved toward a goal by at most Step. */
		int32 Approach(int32 Value, int32 Goal, int32 Step)
		{
			if (Value == Goal)
			{
				return Value;
			}
			const int32 Distance = std::abs(Value - Goal);
			const int32 Move = Distance < Step ? Distance : Step;
			return Goal <= Value ? Value - Move : Value + Move;
		}

		namespace PitchState
		{
			constexpr int32 Start = 0;
			constexpr int32 Level = 1;
			constexpr int32 Climbing = 2;
			constexpr int32 Diving = 3;
		}

		constexpr int32 BankGoal = 0x60;
		constexpr int32 YawRateGoal = 0x10000;
		constexpr int32 PitchStep = 0x4000;
		constexpr int32 PitchLimit = 0x80000;
		constexpr int32 PitchLevelling = 0x2000;
		constexpr int32 ThrottleStep = 0x1000;
	}

	EWorldMapInput ReadFootInput(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		ReadHeading(Context, Task);
		if ((Context.Pad.Pressed & WorldMapButton::Confirm) == 0)
		{
			if (Context.Place != -1 && Context.PlaceRecord && Context.PlaceRecord->Kind == EnterByTouch)
			{
				return EWorldMapInput::Enter;
			}
		}
		else
		{
			if (Context.Contact.Contact != 0)
			{
				return EWorldMapInput::Board;
			}
			if (Context.Place != -1)
			{
				return EWorldMapInput::Enter;
			}
		}
		RequestMenu(Context);
		ReadGearToggle(Context);
		return EWorldMapInput::None;
	}

	EWorldMapInput ReadGearInput(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		ReadHeading(Context, Task);
		if ((Context.Pad.Pressed & WorldMapButton::Confirm) != 0)
		{
			if (Context.Contact.Contact != 0)
			{
				return EWorldMapInput::Board;
			}
			if (Context.Place != -1)
			{
				return EWorldMapInput::Enter;
			}
		}
		RequestMenu(Context);
		ReadGearToggle(Context);
		return EWorldMapInput::None;
	}

	void ReadGearToggle(FWorldMapContext& Context)
	{
		const uint16 Both = WorldMapButton::L2 | WorldMapButton::R2;
		Context.GearButtons = (Context.Pad.Held & Both) == Both ? 1 : 0;
		Context.GearToggle = (Context.GearButtons ^ Context.GearButtonsBefore) & Context.GearButtons;
		Context.GearButtonsBefore = Context.GearButtons;
	}
}

namespace Weltall::WorldMapOps
{
	EWorldMapInput ReadShipInput(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		ReadHeading(Context, Task);
		return ReadShipButtons(Context, true);
	}

	EWorldMapInput ReadFlightInput(FWorldMapContext& Context, FWorldMapTask& Task)
	{
		const uint16 Held = Context.Pad.Held;
		int32& Started = Task.Word(WorldMapTaskWord::FlightStarted);
		int32& YawRate = Task.Word(WorldMapTaskWord::YawRate);
		if (Started == 0)
		{
			Started = 1;
			YawRate = 0;
			Task.Word(WorldMapTaskWord::FlightYaw) = Task.Heading << 12;
		}
		else if (Started == 1)
		{
			int32 Bank = 0;
			int32 Rate = 0;
			int32 BankStep = 8;
			int32 RateStep = 0x1200;
			if ((Held & WorldMapButton::Left) != 0)
			{
				Bank = -BankGoal;
				Rate = -YawRateGoal;
			}
			if ((Held & WorldMapButton::Right) != 0)
			{
				Bank += BankGoal;
				Rate += YawRateGoal;
			}
			if ((Held & WorldMapButton::L1) != 0)
			{
				Bank -= BankGoal;
				Rate -= YawRateGoal;
			}
			if ((Held & WorldMapButton::R1) != 0)
			{
				Bank += BankGoal;
				Rate += YawRateGoal;
			}
			if ((Held & (WorldMapButton::Left | WorldMapButton::Right | WorldMapButton::L1 | WorldMapButton::R1)) != 0)
			{
				RateStep = 0x1000;
				BankStep = std::abs(Context.CameraRoll - Bank) >> 4;
			}
			Context.CameraRoll = static_cast<int16>(Approach(Context.CameraRoll, Bank, BankStep));
			YawRate = Approach(YawRate, Rate, RateStep);
		}
		int32& Yaw = Task.Word(WorldMapTaskWord::FlightYaw);
		Yaw += YawRate;
		Context.CameraYaw = static_cast<int16>((Yaw >> 12) & 0xFFF);
		Task.Heading = Context.CameraYaw;

		int32& State = Task.Word(WorldMapTaskWord::PitchState);
		int32& Pitch = Task.Word(WorldMapTaskWord::Pitch);
		switch (State)
		{
		case PitchState::Start:
			State = PitchState::Level;
			Pitch = 0;
			break;
		case PitchState::Level:
			if (Pitch != 0)
			{
				Pitch += Pitch < 0 ? PitchLevelling : -PitchLevelling;
			}
			if ((Held & WorldMapButton::Down) != 0)
			{
				State = PitchState::Climbing;
			}
			if ((Held & WorldMapButton::Up) != 0)
			{
				State = PitchState::Diving;
			}
			break;
		case PitchState::Climbing:
			if ((Held & WorldMapButton::Down) == 0)
			{
				State = PitchState::Level;
			}
			else if ((Pitch -= PitchStep) < -PitchLimit)
			{
				Pitch = -PitchLimit;
			}
			break;
		case PitchState::Diving:
			if ((Held & WorldMapButton::Up) == 0)
			{
				State = PitchState::Level;
			}
			else if ((Pitch += PitchStep) > PitchLimit)
			{
				Pitch = PitchLimit;
			}
			break;
		default:
			break;
		}

		int32& Throttle = Task.Word(WorldMapTaskWord::Throttle);
		if ((Held & WorldMapButton::Throttle) != 0)
		{
			Throttle += (Held & WorldMapButton::R2) != 0 ? -ThrottleStep : ThrottleStep;
			Throttle = std::clamp(Throttle, Task.Speed * -0x1000, Task.Speed << 12);
		}
		else if (Throttle != 0)
		{
			Throttle += Throttle < 0 ? ThrottleStep : -ThrottleStep;
		}

		const FGteLongVector Way = VectorNormal({GteSin(Context.CameraYaw), GteSin(Pitch >> 12), -GteCos(Context.CameraYaw)});
		Task.Direction.X = Way[0];
		Task.Direction.Y = Way[1];
		Task.Direction.Z = Way[2];
		return ReadShipButtons(Context, Throttle == 0 && Context.CameraRoll == 0);
	}
}
