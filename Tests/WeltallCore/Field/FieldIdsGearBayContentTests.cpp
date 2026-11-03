#include "Battle/Show/BattleGearDress.h"
#include "Field/FieldGears.h"
#include "Field/FieldRuntime.h"
#include "Game/CharacterLooks.h"
#include "Game/GameState.h"
#include "Support/FieldPlaySession.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <optional>

using namespace Weltall;

// Fei's bay in the Yggdrasil's hangar close up (map 293) while Fei is drawn as Id: his gear, the bay's own model and
// animations, drawn as Id's red gear of its time.
namespace
{
	constexpr int32 HangarCloseUp = 293;
	/** var[002], the bay the map shows: 0 Fei's, 2 Citan's. */
	constexpr int32 BayVariable = 1;
	constexpr uint16 FeisBay = 0;
	constexpr uint16 CitansBay = 2;
	constexpr int32 Settle = 60;

	struct FBay
	{
		/** The field model of Fei's gear the bay loaded (-1 none), whether it is worn, and the parts its frame draws. */
		int32 FieldModel = -1;
		bool bWorn = false;
		std::size_t DrawnParts = 0;
		std::size_t WornParts = 0;
		bool bDrawsWorn = false;
	};

	FBay Visit(const FIso9660& Disc, uint8 Gear, bool bId, uint16 Bay)
	{
		auto Session = Testing::EnterFieldMap(Disc, HangarCloseUp, [Gear, bId, Bay](FGameState& Game)
		{
			if (Gear != FCharacterLooks::Weltall)
			{
				REQUIRE(Game.CopyGear(FCharacterLooks::Weltall, Gear));
				REQUIRE(Game.SetCharacterGear(FCharacterLooks::Fei, Gear));
			}
			FCharacterLooks Looks;
			if (bId)
			{
				REQUIRE(Looks.Switch(FCharacterLooks::Fei));
			}
			Game.SetCharacterLooks(Looks);
			Game.SetSavedEventVariable(BayVariable, Bay);
		});
		Session->Run(Settle);
		FBay Found;
		const FFieldGears& Gears = Session->Play->Field().Context().Gears;
		if (!Gears.Models)
		{
			return Found;
		}
		for (int32 Slot = 0; Slot < Gears.Count; ++Slot)
		{
			const int32 FieldModel = Gears.FileNumbers[static_cast<std::size_t>(Slot)] / 2;
			const FGearModel* Model = Gears.Models->Model(Slot);
			if ((FieldModel != 0 && FieldModel != 42 && FieldModel != 57) || Model == nullptr)
			{
				continue;
			}
			Found.FieldModel = FieldModel;
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

TEST_CASE("In Fei's bay of the hangar close up, his gear is drawn as Id's red gear while he is drawn as Id", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc2);
	// The bay's model of each of his gears, drawn with the red gear's parts over it: the red Weltall's 35 bones that carry
	// a part, the red gear's 21.
	for (const auto [Gear, FieldModel, Parts] : {std::tuple<uint8, int32, std::size_t>{FCharacterLooks::Weltall, 0, 35}, {FCharacterLooks::Weltall2, 42, 35},
			 {FCharacterLooks::Xenogears, 57, 21}})
	{
		INFO("gear " << int32(Gear));
		const FBay Id = Visit(*Disc, Gear, true, FeisBay);
		CHECK(Id.FieldModel == FieldModel);
		CHECK(Id.bWorn);
		CHECK(Id.WornParts == Parts);
		CHECK(Id.DrawnParts == Parts);
		CHECK(Id.bDrawsWorn);
		const FBay Fei = Visit(*Disc, Gear, false, FeisBay);
		CHECK(Fei.FieldModel == FieldModel);
		CHECK_FALSE(Fei.bWorn);
	}
	// Citan's bay shows none of Fei's gears.
	const FBay Citans = Visit(*Disc, FCharacterLooks::Weltall, true, CitansBay);
	CHECK_FALSE(Citans.bWorn);
}
