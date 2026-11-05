#include "Battle/BattleContext.h"
#include "Battle/BattleEvent.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/UnreachedCases.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>
#include <vector>

using namespace Weltall;

TEST_CASE("Missing battle event messages stop strict runs and let recovering runs finish the script", "[Battle]")
{
	// One actor: show a message without a window, set variable 1, return. Its idle script returns too.
	std::vector<uint8> Script(0x54, 0);
	LittleEndian::WriteU32(Script, BattleEventScript::ActorCountOffset, 1);
	LittleEndian::WriteU16(Script, 0x46, 7);
	Script.insert(Script.end(), {0x18, 1, 0, 8, 0x07, 2, 0, 0x22, 0});
	std::array<uint8, 9> Messages{0, 0, 0, 0, 8, 0, 1, 1, 0};
	const bool bPastBytes = GENERATE(false, true);
	INFO("entry starts past the table: " << bPastBytes);
	if (bPastBytes)
	{
		Script[0x55] = 0;
		Messages[4] = static_cast<uint8>(Messages.size());
	}

	const auto Run = [&](FUnreachedCases* Cases, bool bRecovers) {
		auto Context = std::make_unique<FBattleContext>();
		Context->Unreached = Cases;
		BeginBattleEvent(*Context, Script, Messages);
		int32 Frames = 0;
		while (StepBattleEvent(*Context))
		{
			REQUIRE(++Frames < 60);
		}
		REQUIRE(Context->Fault == EBattleFault::None);
		BeginBattleEventRun(*Context);
		while (StepBattleEvent(*Context))
		{
			REQUIRE(++Frames < 60);
		}
		CHECK(Context->Fault == (bRecovers ? EBattleFault::None : EBattleFault::EventMessageOutsideTable));
		CHECK(Context->EventVariables[1] == (bRecovers ? 1 : 0));
		CHECK_FALSE(Context->Event.bMessageOpen);
		CHECK_FALSE(Context->Event.bPortraitShown);
	};
	Run(nullptr, false);
	FUnreachedCases Stopping;
	Stopping.bStops = true;
	Run(&Stopping, false);
	CHECK(Stopping.TakeNoted().empty());
	FUnreachedCases Recovering;
	Run(&Recovering, true);
	Run(&Recovering, true);
	CHECK(Recovering.TakeNoted().size() == 1);
}
