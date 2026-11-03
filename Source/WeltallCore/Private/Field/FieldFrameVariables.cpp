#include "Field/FieldFrameVariables.h"
#include "FieldDirections.h"

#include "Game/GameState.h"

#include <string>

namespace Weltall
{
	namespace
	{
		constexpr uint32 LeaderDirection = 0x006;
		constexpr uint32 CameraDirection = 0x008;
		constexpr uint32 PlayMinutesAndSeconds = 0x00C;
		constexpr uint32 PlayHours = 0x00E;
		constexpr uint32 LeaderX = 0x01E;
		constexpr uint32 LeaderZ = 0x020;
		constexpr uint32 LeaderY = 0x022;
		constexpr uint32 CameraPitch = 0x024;
		constexpr uint32 MapVariable = 0x03C;
		constexpr uint32 PartyVariables = 0x03E;
		constexpr uint32 BattleActingTurns = 0x044;
		constexpr uint32 ProgramResult = 0x046;
		/** The saved variables the place takes: 0x002 the next map's entrance, 0x008 the camera's direction. */
		constexpr int32 SavedEntrance = 1;
		constexpr int32 SavedCameraDirection = 4;
		/** A direction in eighths of a turn, as the camera's yaw keeps it. */
		constexpr int32 DirectionShift = 9;
		/** Each slot's three variables where its pilot rides: map and layer, x, z. */
		constexpr std::array<uint32, 3> RidingPlaces = {0x02A, 0x030, 0x036};
		constexpr int32 MapBits = 0xFFF;
		constexpr int32 LayerShift = 14;

		int16 Whole(int32 Value)
		{
			return static_cast<int16>(Value >> 16);
		}

		/** False when the slot's actor has no script state. */
		bool RecordRidingSlot(FEventContext& Context, int32 Slot)
		{
			const uint8 Actor = Context.PartyActors[static_cast<std::size_t>(Slot)];
			if (Actor == FEventContext::NoActor)
			{
				return true;
			}
			const FEventActorState* State = Context.StateOf(Actor);
			if (State == nullptr)
			{
				return false;
			}
			const uint32 Place = RidingPlaces[static_cast<std::size_t>(Slot)];
			Context.Variables.Set(Place, (Context.MapNumber & MapBits) | static_cast<int16>(State->Layer) << LayerShift);
			Context.Variables.Set(Place + 2, Whole(State->Position[0]));
			Context.Variables.Set(Place + 4, Whole(State->Position[2]));
			return true;
		}

		FError NoState(const char* Who, uint8 Actor)
		{
			return MakeError(std::string("the field's ") + Who + " " + std::to_string(Actor) + " has no script state");
		}
	}

	void SaveFieldVariables(FEventContext& Context, const FEventActorState& Leader)
	{
		SaveFieldVariables(Context, Leader, Context.Variables, *Context.GameState);
	}

	void SaveFieldVariables(const FEventContext& Context, const FEventActorState& Leader, FEventVariables& Variables, FGameState& Game)
	{
		Game.SetFieldPlace(static_cast<uint16>(Context.MapNumber), static_cast<uint16>(Game.SavedEventVariable(SavedCameraDirection) << DirectionShift),
			Game.SavedEventVariable(SavedEntrance), static_cast<uint16>(Context.Songs->ChosenSong()));
		Variables.Set(BattleActingTurns, static_cast<int16>(Game.BattleActingTurns));
		Variables.Set(ProgramResult, Game.ProgramResult);
		Variables.Set(LeaderDirection, FieldDirections::FacingEighth(Leader.Facing));
		Variables.Set(CameraDirection, FieldDirections::CameraEighth(Context.Camera));
		Variables.Set(CameraPitch, Context.Camera.Pitch);
		Variables.Set(MapVariable, static_cast<int16>(Context.MapNumber));
		for (std::size_t Slot = 0; Slot < Context.PartyCharacters.size(); ++Slot)
		{
			Variables.Set(PartyVariables + 2 * static_cast<uint32>(Slot), Context.PartyCharacters[Slot]);
		}
		Variables.Save(Game);
	}

	std::optional<FError> UpdateFieldFrameVariables(FEventContext& Context, const FPlayClock& Clock)
	{
		if (Context.bFrameVariablesFrozen)
		{
			return std::nullopt;
		}
		const FEventActorState* Leader = Context.StateOf(Context.LeaderActor);
		if (Leader == nullptr)
		{
			return NoState("leader", Context.LeaderActor);
		}
		Context.PadLatched |= Context.PadHeld;
		FGameState& Game = *Context.GameState;
		Game.SetParty(Context.PartyCharacters);
		SaveFieldVariables(Context, *Leader);
		Context.Clock.Tick(Context.Variables, *Context.Scripts);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Game.PartySlotInGear(Slot) && !RecordRidingSlot(Context, Slot))
			{
				return NoState("party member", Context.PartyActors[static_cast<std::size_t>(Slot)]);
			}
		}
		Context.Variables.Set(PlayMinutesAndSeconds, Clock.Minutes << 8 | Clock.Seconds);
		Context.Variables.Set(PlayHours, Clock.Hours);
		Context.Variables.Set(LeaderX, Whole(Leader->Position[0]));
		Context.Variables.Set(LeaderZ, Whole(Leader->Position[2]));
		Context.Variables.Set(LeaderY, Whole(Leader->Position[1]));
		return std::nullopt;
	}
}
