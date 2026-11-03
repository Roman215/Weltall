#include "WorldMap/WorldMapRuntime.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "WorldMap/WorldMapMovement.h"
#include "WorldMap/WorldMapOps.h"
#include "WorldMap/WorldMapStateLayout.h"

namespace Weltall
{
	namespace
	{
		constexpr uint16 CameraYawMenuFlag = 0x2000;
		constexpr uint16 CameraYawMask = 0x3FFF;
	}

	bool FWorldMapRuntime::CouldOpenMenu() const
	{
		if (Phase != EPhase::Loop || Cutscene != nullptr || PendingRequest.Kind != EWorldMapRequest::None || !MapContext)
		{
			return false;
		}
		const FWorldMapContext& Context = *MapContext;
		const bool bOnFootOrInGears = Context.TravelMode > 0 && Context.TravelMode < WorldMapTravel::SandShip;
		return bOnFootOrInGears && !Context.bCutscene && Context.Fading == 0 && Context.Running != 0 && Context.MenuRequested == 0 && Context.Place == -1
			&& Context.Notice == -1;
	}

	void FWorldMapRuntime::KeepPlaceAsMenuDoes(FGameState& Game) const
	{
		const FWorldMapContext& Context = *MapContext;
		LittleEndian::WriteU16(Game.Block(), WorldMapStateLayout::MenuOpen, 1);
		LittleEndian::WriteU16(Game.Block(), WorldMapStateLayout::CameraYaw, static_cast<uint16>((Context.CameraYaw + CameraYawMenuFlag) & CameraYawMask));
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			LittleEndian::WriteU16(Game.Block(), WorldMapStateLayout::BattleRiding + 2 * static_cast<std::size_t>(Slot), WorldMapOps::Riding(Context, Slot));
		}
	}
}
