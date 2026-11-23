#pragma once

#include "Field/EventContext.h"
#include "Field/EventScripts.h"
#include "Foundation/Types.h"

namespace Weltall
{
	/**
	 * Fixes of the maps' own scripts, where their bytes are wrong: each changes a few bytecode bytes of
	 * one map as the map loads. The original's fixes follow the options, and compatibility fixes for
	 * Perfect Works apply when the affected instructions match. The hotel on map 104 counts party slot 2 by its own variable
	 * (`bHotelCountsWholeParty`); the ship on map 272 sets its sounds' volumes (`bShipSoundsAtTheirVolume`); the room's
	 * sound in map 670's memories fades (`bMemoryAmbienceFades`).
	 */
	WELTALLCORE_API void ApplyFieldScriptFixes(int32 Map, FEventScripts& Scripts, const FEventOptions& Options);
}
