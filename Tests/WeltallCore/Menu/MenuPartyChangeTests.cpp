#include "Game/PlayerSettings.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuPlay.h"
#include "Menu/MenuSettingsPages.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

namespace
{
	void Wait(FMenuPlay& Play, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const auto Drawn = Play.Frame({});
			INFO((Drawn ? "" : Drawn.Error().Message));
			REQUIRE(Drawn);
			REQUIRE(*Drawn);
		}
	}

	void Press(FMenuPlay& Play, EPadButton Button, int32 Frames = 8)
	{
		const auto Drawn = Play.Frame({FPadState::Bit(Button)});
		REQUIRE(Drawn);
		REQUIRE(*Drawn);
		Wait(Play, Frames);
	}
}

TEST_CASE("Changing party anywhere is opt-in and persists with the player's settings", "[Menu][Settings]")
{
	FPlayerSettings Settings;
	CHECK_FALSE(Settings.bPartyChangeAnywhere);
	bool bCustom = false;
	int32 Row = 0;
	for (; Row < FSettingsPages::RowCount(ESettingsPage::Modding); ++Row)
	{
		if (FSettingsPages::Row(ESettingsPage::Modding, Row, Settings, false).Label == "Party changes")
		{
			break;
		}
	}
	REQUIRE(Row < FSettingsPages::RowCount(ESettingsPage::Modding));
	REQUIRE(FSettingsPages::Step(ESettingsPage::Modding, Row, 1, Settings, bCustom));
	CHECK(Settings.bPartyChangeAnywhere);
	CHECK(ReadPlayerSettings(WritePlayerSettings(Settings)).bPartyChangeAnywhere);
}

TEST_CASE("Free-play Member Change retains the current party and story locks", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FMenuRequest Request;
	Request.Mode = EMenuMode::MemberChange;
	Request.bKeepParty = true;
	const auto Started = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, Request, nullptr);
	REQUIRE(Started);
	FMenuPlay& Play = **Started;
	Play.Game().SetParty({0, 2, 1});
	Play.Game().SetJoinedCharacters(0xF);
	Play.Game().SetMenuCharacters(0xD); // Elly is in the current party but absent from the reserve mask.
	Play.Game().SetLockedCharacters(1);
	Wait(Play, 40);
	Press(Play, EPadButton::Cross); // Fei picked, then Bart: the lock refuses the swap.
	Press(Play, EPadButton::Cross);
	Press(Play, EPadButton::Circle);
	Press(Play, EPadButton::Left);
	Press(Play, EPadButton::Down);
	Press(Play, EPadButton::Cross);
	Press(Play, EPadButton::Cross, 24); // Citan traded for Bart.
	// Elly is kept in the party, but her story exclusion must also prevent swapping her out.
	Press(Play, EPadButton::Cross);
	Press(Play, EPadButton::Down);
	Press(Play, EPadButton::Down);
	Press(Play, EPadButton::Cross);
	Press(Play, EPadButton::Circle);
	const auto Closing = Play.Frame({FPadState::Bit(EPadButton::Circle)});
	REQUIRE(Closing);
	bool bEnded = false;
	for (int32 Frame = 0; Frame < 100 && !bEnded; ++Frame)
	{
		const auto Drawn = Play.Frame({});
		REQUIRE(Drawn);
		bEnded = !*Drawn;
	}
	CHECK(bEnded);
	CHECK(Play.Game().Party() == std::array<uint8, 3>{0, 3, 1});
	CHECK(Play.Game().LockedCharacters() == 1);
	CHECK(Play.Game().MenuCharacters() == 0xD);
}

TEST_CASE("Free-play Member Change preserves empty slots and excludes members without a required gear", "[Menu][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	FMenuRequest Request;
	Request.Mode = EMenuMode::MemberChange;
	Request.bKeepParty = true;
	Request.bRequiresGears = true;
	const auto Started = FMenuPlay::NewGame(Disc->Image(), *Executable, 1, Request, nullptr);
	REQUIRE(Started);
	FMenuPlay& Play = **Started;
	Play.Game().SetParty({0, 2, 0xFF});
	Play.Game().SetJoinedCharacters(0xF);
	Play.Game().SetMenuCharacters(0xF);
	// Elly is the first reserve member; without a gear she must be skipped in favour of Bart.
	Play.Game().Block()[0x26C + 0xA4 + 0xA0] = 0xFF;
	Wait(Play, 40);
	Press(Play, EPadButton::Down);
	Press(Play, EPadButton::Cross);
	Press(Play, EPadButton::Cross, 24);
	REQUIRE(Play.Frame({FPadState::Bit(EPadButton::Circle)}));
	for (int32 Frame = 0; Frame < 100 && !Play.Menu().HasEnded(); ++Frame)
	{
		REQUIRE(Play.Frame({}));
	}
	CHECK(Play.Menu().HasEnded());
	CHECK(Play.Game().Party() == std::array<uint8, 3>{0, 3, 0xFF});
}
