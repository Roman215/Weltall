#include "Battle/BattleRuntime.h"
#include "Field/FieldMap.h"
#include "Game/BattleGameFiles.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/StorySweepStrength.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>

using namespace Weltall;
using namespace Weltall::Testing;

TEST_CASE("The Brazilian translation's Brigandier battle finishes despite its five missing event messages", "[Battle][Content]")
{
	REQUIRE_BRAZILIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint16 BattleMap = 62;
	constexpr uint8 FormationNumber = 2;
	constexpr uint16 QuietMap = 15;
	constexpr uint8 EventNumber = 5;
	const auto Game = FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FGameState& State = Session.Game();
	State.SetSavedEventVariable(0, 43);
	State.SetParty({0, 0xFF, 0xFF});
	Session.Unreached().bStops = false;
	Game->EnterField(QuietMap);
	Game->RunUntil([&] { return Game->Loop->Field()->Runtime().Context().bFieldRunning; }, 100);
	State.SetPartySlotInGear(0, 1);
	StrengthenParty(State);

	const auto Map = LoadFieldMap(Disc->Image(), *Game->Index, BattleMap);
	REQUIRE(Map);
	REQUIRE(Map->Encounters.size() >= Session.Kernel().Formations.size());
	std::copy_n(Map->Encounters.begin(), Session.Kernel().Formations.size(), Session.Kernel().Formations.begin());
	const auto Record = Session.Kernel().Formation(FormationNumber);
	REQUIRE(Record);
	REQUIRE(FBattleFormation::FromRecord(*Record).EventScript == EventNumber);

	FEventContext& Field = Game->Loop->Field()->Runtime().Context();
	Field.bBattleRequested = true;
	Field.Requests.bScriptedBattle = true;
	Field.Requests.bLeavingField = true;
	Field.Requests.BattleKind = 0;
	Field.Encounters.Formation = FormationNumber;
	Game->RunUntil([&] { return Game->Loop->Program() == EGameProgram::Battle; }, 8);
	REQUIRE(Game->Loop->Battle() != nullptr);
	const auto Messages = FMessageTable::Parse(Session.BattleFiles().Events.Messages(EventNumber));
	REQUIRE(Messages);
	REQUIRE(Messages->Count() == 7);
	bool bFought = false;
	for (int32 Frame = 0; Frame < 5000 && Game->Loop->Program() == EGameProgram::Battle; ++Frame)
	{
		bFought = bFought || Game->Loop->Battle()->Runtime().IsFought();
		FPadState Held;
		Held.Buttons = Frame % 12 < 2 ? FPadState::Bit(EPadButton::Cross) : 0;
		const auto Error = Game->Frame(Held);
		INFO("frame " << Frame << ": " << (Error ? Error->Message : std::string()));
		REQUIRE_FALSE(Error);
	}
	CHECK(bFought);
	CHECK(Game->Loop->Program() != EGameProgram::Battle);
	const auto Noted = Session.Unreached().TakeNoted();
	CHECK(std::count(Noted.begin(), Noted.end(), "a battle event's missing message, skipped") == 1);
}
