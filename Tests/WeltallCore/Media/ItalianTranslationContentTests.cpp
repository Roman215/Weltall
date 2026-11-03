#include "Battle/BattleProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Media/DiscIdentification.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuSpriteSheet.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <utility>

using namespace Weltall;

// The Italian translation (SadNES cITy's 1.2.1, with the English voices and the US commands): its discs, made from the
// USA discs by its patch, are read from the folder WELTALL_ITALIAN_GAMES names.

namespace
{
	/** The Gear screen's "Opzioni Gear": "Opzioni" in its first three parts, "Gear" in the fourth. */
	constexpr int32 GearOptionLabel = 0x13C;
	/** The port's "Options" on the title and on the Normal Menu: where their O stands. */
	constexpr std::array<std::pair<int32, std::pair<int32, int32>>, 2> OptionsPlaces{{{0x200, {-85, 49}}, {0x202, {-145, -71}}}};
}

TEST_CASE("The Italian translation's discs play as their USA releases, patched", "[Media][Content]")
{
	for (const auto& [Release, Number] : {std::pair{EDiscRelease::UsaDisc1, 1}, std::pair{EDiscRelease::UsaDisc2, 2}})
	{
		REQUIRE_ITALIAN_DISC(Disc, Release);
		const TResult<FDiscIdentity> Identity = IdentifyDisc(*Disc);
		REQUIRE(Identity);
		CHECK(Identity->Release == Release);
		CHECK(Identity->DiscNumber == Number);
		CHECK(Identity->bPatched);
		CHECK_FALSE(Identity->bJapaneseCommands);
	}
}

TEST_CASE("The Italian translation's discs with the Japanese commands play as their USA releases, their face buttons swapped", "[Media][Content]")
{
	for (const auto& [Release, Number] : {std::pair{EDiscRelease::UsaDisc1, 1}, std::pair{EDiscRelease::UsaDisc2, 2}})
	{
		REQUIRE_ITALIAN_JP_DISC(Disc, Release);
		const TResult<FDiscIdentity> Identity = IdentifyDisc(*Disc);
		REQUIRE(Identity);
		CHECK(Identity->Release == Release);
		CHECK(Identity->DiscNumber == Number);
		CHECK(Identity->bPatched);
		CHECK(Identity->bJapaneseCommands);
	}
}

TEST_CASE("With the Japanese commands Circle skips the opening and opens Load Game on the title, and Cross does neither", "[Game][Content]")
{
	REQUIRE_ITALIAN_JP_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr FPadState CircleHeld{FPadState::Bit(EPadButton::Circle)};
	constexpr FPadState CrossHeld{FPadState::Bit(EPadButton::Cross)};
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	REQUIRE(Game->Session->Pad().HasJapaneseCommands());
	Game->PowerOn();
	FGameLoop& Loop = *Game->Loop;
	Game->RunUntil([&] { return Loop.Program() == EGameProgram::Movie; }, 1000);
	// Circle held from the movie's start: its second reading skips, and the fade's 5 vertical blanks end it.
	const int32 MovieStart = Game->Vblanks;
	Game->RunUntil([&] { return Loop.Program() != EGameProgram::Movie; }, 200, CircleHeld);
	CHECK(Game->Vblanks - MovieStart == 2 + 4);
	Game->RunUntil([&] { return Loop.Stage() == EGameLoopStage::Stopped; }, 1500, CircleHeld, true);
	REQUIRE(Loop.Stage() == EGameLoopStage::Stopped);
	CHECK(Loop.Program() == EGameProgram::Field);
	CHECK(Game->Session->Kernel().MenuMode == 2);

	// Cross held: the logos' movie plays to its end frame, and the title waits on.
	auto Crossed = Testing::FGameLoopSession::Start(*Disc);
	Crossed->PowerOn();
	FGameLoop& CrossedLoop = *Crossed->Loop;
	Crossed->RunUntil([&] { return CrossedLoop.Program() == EGameProgram::Movie; }, 1000);
	const int32 CrossedStart = Crossed->Vblanks;
	Crossed->RunUntil([&] { return CrossedLoop.Program() != EGameProgram::Movie; }, 2000, CrossHeld);
	CHECK(Crossed->Vblanks - CrossedStart == 926);
	REQUIRE(CrossedLoop.Program() == EGameProgram::Field);
	const int32 FieldStart = Crossed->Frames;
	Crossed->RunUntil([&] { return Crossed->Frames > FieldStart + 150; }, 400, CrossHeld);
	CHECK(CrossedLoop.Stage() == EGameLoopStage::Program);
	CHECK(CrossedLoop.Field()->Runtime().Request().Kind == EFieldRequest::None);
}

TEST_CASE("The Italian Menu weighs in kilograms, and its Opzioni names the port's Options", "[Menu][Content]")
{
	REQUIRE_ITALIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	CHECK(Resources->WeighsInKilograms());
	CHECK_FALSE(Resources->IsFrench());

	const FMenuSpriteSheet& Sheet = Resources->Sheet();
	const std::span<const FMenuSpritePart> Label = Sheet.Frame(GearOptionLabel);
	REQUIRE(Label.size() == 4);
	for (const auto& [Frame, Place] : OptionsPlaces)
	{
		INFO("frame " << Frame);
		const std::span<const FMenuSpritePart> Options = Sheet.Frame(Frame);
		REQUIRE(Options.size() == 3);
		// The label's own parts without its "Gear", moved together so that their top left is where the O stands.
		for (std::size_t Part = 0; Part < Options.size(); ++Part)
		{
			CHECK(Options[Part].U == Label[Part].U);
			CHECK(Options[Part].V == Label[Part].V);
			CHECK(Options[Part].Width == Label[Part].Width);
			CHECK(Options[Part].X - Label[Part].X == Options[0].X - Label[0].X);
			CHECK(Options[Part].Y - Label[Part].Y == Options[0].Y - Label[0].Y);
			CHECK(Options[Part].Picture < 0);
		}
		const auto Left = std::min_element(Options.begin(), Options.end(), [](const FMenuSpritePart& A, const FMenuSpritePart& B) { return A.X < B.X; });
		const auto Top = std::min_element(Options.begin(), Options.end(), [](const FMenuSpritePart& A, const FMenuSpritePart& B) { return A.Y < B.Y; });
		CHECK(Left->X == Place.first);
		CHECK(Top->Y == Place.second);
	}
}

TEST_CASE("The game's own Menu weighs in pounds, and its Battle program takes the camera after a queue write", "[Menu][Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
	REQUIRE(Resources);
	CHECK_FALSE(Resources->WeighsInKilograms());
	const auto Tables = FBattleProgramTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	CHECK_FALSE(Tables->bQueueKeepsCamera);
}

TEST_CASE("The Italian Battle program keeps the camera after a queue write, as the French one does", "[Battle][Content]")
{
	REQUIRE_ITALIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const auto Tables = FBattleProgramTables::Load(Disc->Image(), *Index);
	REQUIRE(Tables);
	CHECK(Tables->bQueueKeepsCamera);
}
