#include "Field/FieldPartySprites.h"
#include "Field/FieldSprites.h"
#include "Game/FieldProgram.h"
#include "Game/GameMenus.h"
#include "Game/GameProgram.h"
#include "Game/GameState.h"
#include "Game/WorldMapProgram.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <array>

using namespace Weltall;
using namespace Weltall::Testing;

namespace
{
	class FPartyMenu final : public IGameProgram
	{
	public:
		explicit FPartyMenu(FGameSession& InSession) : Session(InSession) {}
		int32 VblanksPerFrame() const override { return 1; }
		std::optional<FError> Frame(std::span<const FPadState> Held) override
		{
			for (const FPadState Pad : Held)
			{
				Session.Vblank(Pad);
			}
			Session.Game().SetParty({0, 3, 1});
			bEnded = true;
			return std::nullopt;
		}
		EGameProgramState State() const override { return bEnded ? EGameProgramState::Ended : EGameProgramState::Running; }
		FGameSession& Session;
		bool bEnded = false;
	};

	class FMenus final : public IGameMenus
	{
	public:
		TResult<std::unique_ptr<IGameProgram>> Open(FGameSession& Session, const FFieldRequest& Request) override
		{
			++Opened;
			Mode = Request.MenuMode;
			bByButton = Request.bMenuByButton;
			return std::unique_ptr<IGameProgram>(std::make_unique<FPartyMenu>(Session));
		}
		int32 Opened = 0;
		uint8 Mode = 0;
		bool bByButton = false;
	};

	void ChangePartyOnMap(uint16 Map, bool bEnabled = true, bool bModifier = true)
	{
		REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
		const auto Game = FGameLoopSession::Start(*Disc);
		FGameState& State = Game->Session->Game();
		State.SetParty({0, 2, 1});
		State.SetJoinedCharacters(0xF);
		State.SetMenuCharacters(0xF);
		FPlayerSettings Settings = Game->Session->Settings();
		Settings.bPartyChangeAnywhere = bEnabled;
		Game->Session->SetSettings(Settings);
		FMenus Menus;
		Game->Session->SetMenus(&Menus);
		Game->EnterField(Map);
		FFieldRuntime& Runtime = Game->Loop->Field()->Runtime();
		Game->RunUntil([&] { return Runtime.Context().bFieldRunning; }, 150);
		Game->RunUntil([&] { return Runtime.Context().bPlayerControlled; }, 150);
		const auto Leader = Runtime.Context().LeaderActor;
		const auto Position = Runtime.Context().StateOf(Leader)->Position;
		const int32 Loads = Runtime.MapLoads();
		const auto PartyActors = Runtime.Context().PartyActors;
		const uint16 Modifier = bModifier ? FPadState::Bit(EPadButton::L2) : 0;
		REQUIRE_FALSE(Game->Frame({static_cast<uint16>(FPadState::Bit(EPadButton::Square) | Modifier)}));
		Game->RunUntil([&] { return Menus.Opened == 1; }, 100);
		CHECK(Menus.Mode == (bEnabled && bModifier ? 1 : 0));
		CHECK(Menus.bByButton);
		Game->RunUntil([&] { return Runtime.Context().MenuRequest == FEventContext::NoMenu; }, 150);
		CHECK(State.Party() == std::array<uint8, 3>{0, 3, 1});
		CHECK(Runtime.Context().PartyCharacters == State.Party());
		CHECK(Runtime.MapLoads() == Loads);
		CHECK(Runtime.Context().StateOf(Leader)->Position == Position);
		CHECK(Runtime.Context().PartyActors == PartyActors);
		const int32 MemberActor = Runtime.Context().PartyActors[1];
		REQUIRE(Runtime.Context().StateOf(MemberActor) != nullptr);
		CHECK(Runtime.Context().StateOf(MemberActor)->Character == 3);
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			REQUIRE_FALSE(Game->Frame());
		}
	}
}

TEST_CASE("A changed party returns to the same field position without restarting its map", "[Field][Menu][Content]")
{
	ChangePartyOnMap(1);
}

TEST_CASE("A changed party refreshes its field pilots and gears without restarting their map", "[Field][Menu][Content]")
{
	ChangePartyOnMap(414);
}

TEST_CASE("The world map's party-change shortcut follows the opt-in setting", "[WorldMap][Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const bool bEnabled = GENERATE(false, true);
	const auto Game = FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FPlayerSettings Settings = Session.Settings();
	Settings.bPartyChangeAnywhere = bEnabled;
	Session.SetSettings(Settings);
	Session.Game().SetParty({0, 2, 1});
	Session.Game().SetSavedEventVariable(0, 30);
	Session.Game().SetWorldMapPlace(0x401, 1, 0xC00, 1);
	FMenus Menus;
	Session.SetMenus(&Menus);
	const auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	for (int32 Frame = 0; Frame < 200 && Menus.Opened == 0; ++Frame)
	{
		FPadState Held;
		Held.Buttons = FPadState::Bit(EPadButton::L2);
		if (Frame % 4 == 0)
		{
			Held.Buttons |= FPadState::Bit(EPadButton::Square);
		}
		const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program.VblanksPerFrame()), Held);
		REQUIRE_FALSE(Program.Frame(Vblanks));
	}
	REQUIRE(Menus.Opened == 1);
	CHECK(Menus.Mode == (bEnabled ? 1 : 0));
	for (int32 Frame = 0; Frame < 100; ++Frame)
	{
		const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program.VblanksPerFrame()));
		REQUIRE_FALSE(Program.Frame(Vblanks));
	}
	CHECK(Session.Game().Party() == std::array<uint8, 3>{0, 3, 1});
}

TEST_CASE("The field's normal menu stays available without the party-change option or modifier", "[Field][Menu][Content]")
{
	ChangePartyOnMap(1, false, true);
	ChangePartyOnMap(1, true, false);
}
