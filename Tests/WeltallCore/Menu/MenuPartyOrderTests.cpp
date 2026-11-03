#include "Game/PartyOrder.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuPlay.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <vector>

using namespace Weltall;

// The party arranged on the Normal Menu's panels (the port's): Right from the arc, a member picked and another
// confirmed trade places, and the game keeps the order shown while its own party keeps the story's.
namespace
{
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 None = 0xFF;

	bool Drawn(const TResult<bool>& Frame)
	{
		if (!Frame)
		{
			FAIL(Frame.Error().Message);
		}
		return *Frame;
	}

	void Wait(FMenuPlay& Menu, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	}

	void Press(FMenuPlay& Menu, EPadButton Button, int32 Frames = 8)
	{
		REQUIRE(Drawn(Menu.Frame({FPadState::Bit(Button)})));
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			REQUIRE(Drawn(Menu.Frame({})));
		}
	}

	/** A new game's Normal Menu, Elly joined as the party's third, the party shown in its player's order or not. */
	std::unique_ptr<FMenuPlay> OpenMenu(const FIso9660& Disc, bool bArranges)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		auto Play = FMenuPlay::NewGame(Disc.Image(), *Executable, 1, {EMenuMode::Normal, 0, false}, nullptr, nullptr, nullptr, nullptr, bArranges);
		REQUIRE(Play);
		FGameState& Game = (*Play)->Game();
		Game.SetParty({Fei, Citan, Elly});
		Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << Elly));
		return std::move(*Play);
	}
}

TEST_CASE("Right from the Normal Menu's arc arranges the party, the game keeping the order shown", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	Wait(Menu, 60);
	// Right onto the panels, Fei picked: the cursor goes on to Citan; Down to Elly's panel, confirmed: they trade places
	// as their panels slide.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Down);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
	Press(Menu, EPadButton::Cross, 30);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Elly, Citan, Fei});
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Fei, Citan, Elly});
	// A pick dropped with Circle, then Circle again back to the arc and once more to close the menu: the order stays.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Circle);
	Press(Menu, EPadButton::Circle);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Elly, Citan, Fei});
	REQUIRE(Drawn(Menu.Frame({FPadState::Bit(EPadButton::Circle)})));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 100 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	CHECK(bEnded);
}

TEST_CASE("The original's Normal Menu does nothing on Right, whatever order the game keeps", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, false);
	FMenuPlay& Menu = *Play;
	Wait(Menu, 60);
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
}

TEST_CASE("The Gear screen boards the member it shows, whatever slot of the game's party holds it", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	FPartyOrder Order;
	const std::array<uint8, 2> CitanFirst{Citan, Fei};
	Order.Arrange(Menu.Game().Party(), CitanFirst);
	Menu.Game().SetPartyOrder(Order);
	Menu.Game().SetPartySlotInGear(0, 0);
	Menu.Game().SetPartySlotInGear(1, 0);
	Wait(Menu, 60);
	// From Status down four to Gear: Citan's gear shown first. Up three to Get ON-OFF.
	for (int32 Step = 0; Step < 4; ++Step)
	{
		Press(Menu, EPadButton::Down);
	}
	Press(Menu, EPadButton::Cross, 60);
	for (int32 Step = 0; Step < 3; ++Step)
	{
		Press(Menu, EPadButton::Up);
	}
	Press(Menu, EPadButton::Cross, 20);
	CHECK(Menu.Game().PartySlotInGear(1) == 1);
	CHECK(Menu.Game().PartySlotInGear(0) == 0);
}

TEST_CASE("Member Change shows the party in its player's order, a newcomer taking the place of the one it replaces", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::MemberChange, 0, false}, nullptr, nullptr, nullptr, nullptr, true);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	constexpr uint8 Bart = 3;
	FGameState& Game = Menu.Game();
	Game.SetParty({Fei, Citan, Elly});
	Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << Elly | 1u << Bart));
	FPartyOrder Order;
	const std::array<uint8, 3> EllyFirst{Elly, Fei, Citan};
	Order.Arrange(Game.Party(), EllyFirst);
	Game.SetPartyOrder(Order);
	Wait(Menu, 30);
	// Cross picks the first row, Elly; the cursor goes to the reserve's first row, Bart, and Cross swaps them.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Cross, 20);
	// Circle leaves: Bart in Elly's slot of the game's party, and first in the order shown.
	REQUIRE(Drawn(Menu.Frame({FPadState::Bit(EPadButton::Circle)})));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 60 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	CHECK(bEnded);
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Fei, Citan, Bart});
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Bart, Fei, Citan});
}

TEST_CASE("A member picked to swap, the cursor goes on to the next, so a second Confirm swaps the two", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	Menu.Game().SetParty({Fei, Citan, None});
	Wait(Menu, 60);
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Cross, 30);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Citan, Fei, None});
}

TEST_CASE("With one member the panels cannot be arranged: Right leaves the arc as it is", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	Menu.Game().SetParty({Fei, None, None});
	Wait(Menu, 60);
	Press(Menu, EPadButton::Right);
	// Cross confirms the arc's command (Status, where a new game's menu opens): no member is picked.
	Press(Menu, EPadButton::Cross, 40);
	Press(Menu, EPadButton::Circle, 40);
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross, 40);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
}

TEST_CASE("With a member picked, up and down go only among the others", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	Wait(Menu, 60);
	// Fei picked, the cursor on Citan: Down to Elly, Down again past Fei back to Citan, and the two swap.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Down);
	Press(Menu, EPadButton::Cross, 30);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Citan, Fei, Elly});
}

TEST_CASE("Triangle while arranging puts the party back in the story's order, the game keeping none", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::unique_ptr<FMenuPlay> Play = OpenMenu(*Disc, true);
	FMenuPlay& Menu = *Play;
	// An order kept for Fei and Citan alone: Elly with them, the menu shows the story's order.
	FPartyOrder Earlier;
	const std::array<uint8, 2> CitanFirst{Citan, Fei};
	Earlier.Arrange({Fei, Citan, None}, CitanFirst);
	Menu.Game().SetPartyOrder(Earlier);
	Wait(Menu, 60);
	// Fei and Citan swapped on the panels, which were in the story's order: Citan first.
	Press(Menu, EPadButton::Right);
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Cross, 30);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Citan, Fei, Elly});
	// Triangle: no order kept, and the panels back in the story's order, so the cursor's second place is Citan's again
	// and swapping him with Elly shows Fei, Elly, Citan.
	Press(Menu, EPadButton::Triangle, 30);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Cross, 30);
	CHECK(Menu.Game().PartyOrder().Shown(Menu.Game().Party()) == std::array<uint8, 3>{Fei, Elly, Citan});
	// Citan picked where he now stands, last, and swapped back with Elly (Up from Fei): the story's order, which the
	// game keeps no order for either; Triangle then changes nothing.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Up);
	Press(Menu, EPadButton::Cross, 30);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
	Press(Menu, EPadButton::Triangle, 30);
	CHECK_FALSE(Menu.Game().PartyOrder().IsSet());
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Fei, Citan, Elly});
}

TEST_CASE("Member Change leaves an order kept for other people as it is", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	auto Play = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, {EMenuMode::MemberChange, 0, false}, nullptr, nullptr, nullptr, nullptr, true);
	REQUIRE(Play);
	FMenuPlay& Menu = **Play;
	constexpr uint8 Bart = 3;
	FGameState& Game = Menu.Game();
	Game.SetParty({Fei, Citan, Elly});
	Game.SetJoinedCharacters(static_cast<uint16>(Game.JoinedCharacters() | 1u << Elly | 1u << Bart));
	FPartyOrder Earlier;
	const std::array<uint8, 2> CitanFirst{Citan, Fei};
	Earlier.Arrange({Fei, Citan, None}, CitanFirst);
	Game.SetPartyOrder(Earlier);
	Wait(Menu, 30);
	// The rows in the story's order: Cross picks Fei and swaps him with the reserve's Bart.
	Press(Menu, EPadButton::Cross);
	Press(Menu, EPadButton::Cross, 20);
	REQUIRE(Drawn(Menu.Frame({FPadState::Bit(EPadButton::Circle)})));
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 60 && !bEnded; ++Frame)
	{
		bEnded = !Drawn(Menu.Frame({}));
	}
	CHECK(bEnded);
	CHECK(Menu.Game().Party() == std::array<uint8, 3>{Bart, Citan, Elly});
	CHECK(Menu.Game().PartyOrder() == Earlier);
}

TEST_CASE("Arranged panels keep every member's name, as the menu types them opening with that order", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// The names are typed into strips two to a row, each 40 units wide and the second 24 units in, so a slot's strip covers
	// the start of the next one's.
	const auto Strips = [](const FMenuPlay& Menu)
	{
		const std::span<const uint16> All = Menu.Menu().Vram().ConsoleUnits();
		std::vector<uint16> Units;
		for (int32 Row = 59; Row < 111; ++Row)
		{
			const auto Start = All.begin() + Row * 1024 + 384;
			Units.insert(Units.end(), Start, Start + 64);
		}
		return Units;
	};
	std::unique_ptr<FMenuPlay> Swapped = OpenMenu(*Disc, true);
	Wait(*Swapped, 60);
	// Fei picked, then Citan, beside him: Elly's panel stays where it is.
	Press(*Swapped, EPadButton::Right);
	Press(*Swapped, EPadButton::Cross);
	Press(*Swapped, EPadButton::Cross, 30);

	std::unique_ptr<FMenuPlay> Opened = OpenMenu(*Disc, true);
	constexpr std::array<uint8, 3> Arranged{Citan, Fei, Elly};
	FPartyOrder Order;
	Order.Arrange(Opened->Game().Party(), Arranged);
	Opened->Game().SetPartyOrder(Order);
	Wait(*Opened, 60);
	REQUIRE(Swapped->Game().PartyOrder().Shown(Swapped->Game().Party()) == Arranged);
	CHECK(Strips(*Swapped) == Strips(*Opened));
}
