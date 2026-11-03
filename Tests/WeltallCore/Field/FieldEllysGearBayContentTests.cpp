#include "Battle/Show/BattleRegurus.h"
#include "Field/FieldGears.h"
#include "Field/FieldRuntime.h"
#include "Game/GameState.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace Weltall;

// Elly's bay in the Yggdrasil's hangar close up (map 296, entered from her place in the hangar by entrance 4), whose
// script always shows Vierge's model: while she holds Regurus (the clear file's game), it is drawn as Regurus.
namespace
{
	constexpr int32 OtherBaysCloseUp = 296;
	/** var[002], the bay the map shows: 4 Elly's, 6 Emeralda's. */
	constexpr int32 BayVariable = 1;
	constexpr uint16 EllysBay = 4;
	constexpr uint16 EmeraldasBay = 6;
	constexpr int32 Elly = 1;
	constexpr int32 ViergesFieldModel = 1;
	constexpr int32 Settle = 60;

	struct FBay
	{
		/** Whether the bay loaded Vierge's model, whether it is worn, and the parts its frame draws. */
		bool bVierge = false;
		bool bWorn = false;
		std::size_t DrawnParts = 0;
		std::size_t WornParts = 0;
		bool bDrawsWorn = false;
	};

	FBay Visit(const FIso9660& Disc, uint8 EllysGear, uint16 Bay)
	{
		auto Session = Testing::EnterFieldMap(Disc, OtherBaysCloseUp, [EllysGear, Bay](FGameState& Game)
		{
			REQUIRE(Game.SetCharacterGear(Elly, EllysGear));
			Game.SetSavedEventVariable(BayVariable, Bay);
		});
		Session->Run(Settle);
		FBay Found;
		const FFieldGears& Gears = Session->Play->Field().Context().Gears;
		if (!Gears.Models)
		{
			return Found;
		}
		for (int32 Slot = 0; Slot < std::min(Gears.Count, FFieldGears::Capacity); ++Slot)
		{
			const FGearModel* Model = Gears.Models->Model(Slot);
			if (Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2 != ViergesFieldModel || Model == nullptr)
			{
				continue;
			}
			Found.bVierge = true;
			Found.bWorn = Model->Dress != nullptr;
			const FGearFrameModel& Drawn = Gears.Models->Frame().Models[static_cast<std::size_t>(Slot)];
			Found.DrawnParts = Drawn.Parts.size();
			if (Model->Dress != nullptr)
			{
				Found.WornParts = Model->Dress->Packets().size();
				Found.bDrawsWorn = Drawn.Model == &Model->Dress->Model().Model && Drawn.Packets == &Model->Dress->Packets();
			}
		}
		return Found;
	}
}

TEST_CASE("In Elly's bay of the hangar close up, Vierge's model is drawn as Regurus while she holds it", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	// Regurus' 26 bones that carry a part (its 24 parts, two of them on both sides), over the bay's Vierge and its animation.
	const FBay Regurus = Visit(*Disc, BattleRegurus::Regurus, EllysBay);
	CHECK(Regurus.bVierge);
	CHECK(Regurus.bWorn);
	CHECK(Regurus.WornParts == 26);
	CHECK(Regurus.DrawnParts == 26);
	CHECK(Regurus.bDrawsWorn);
	const FBay Vierge = Visit(*Disc, BattleRegurus::Vierge, EllysBay);
	CHECK(Vierge.bVierge);
	CHECK_FALSE(Vierge.bWorn);
	// Emeralda's bay shows no Vierge.
	CHECK_FALSE(Visit(*Disc, BattleRegurus::Regurus, EmeraldasBay).bVierge);
}
