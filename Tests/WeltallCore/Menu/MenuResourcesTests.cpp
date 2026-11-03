#include "Menu/MenuResources.h"

#include "Foundation/LittleEndian.h"
#include "Game/GameState.h"
#include "Game/GearPartTables.h"
#include "Game/GearRecompute.h"
#include "Media/DiscModFiles.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Mods/PerfectWorks.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <optional>
#include <vector>

using namespace Weltall;
using Weltall::Testing::FModFilesGiven;

// Perfect Works Build's rebalanced party brings its own part tables, whose defenses are words at +0Ch and +0Eh: the Menu's
// tables, which the Gear screen, the gear shop and a save's load recompute the gears with, read them as its Menu does.
TEST_CASE("With Perfect Works' rebalanced party the Menu's gear parts give their defenses as the mod's Menu reads them", "[Menu][Mods][Content]")
{
	constexpr std::size_t WeltallRecord = 0x978;
	constexpr std::size_t PartSlots = 0x09;
	constexpr std::size_t PartsDefense = 0x40;
	constexpr std::size_t PartsEtherDefense = 0x42;
	constexpr std::size_t PartDefense = 0x0C;
	constexpr std::size_t PartEtherDefense = 0x0E;
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const std::optional<FPerfectWorksRelease> Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
	if (!Release || Release->Version() != "0.11.4")
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	FPerfectWorksOptions Options;
	Options.bEnabled = true;
	Options.bRebalancedParty = true;
	const auto Tables = [&]
	{
		const std::vector<FModFile> Files = Release->Files(Options, 1);
		const FModFilesGiven Given(*Disc, FDiscModFiles::Make(*Index, 1, Files, Release->Changes(Options, Files)));
		auto Resources = FMenuResources::Load(Disc->Image(), *Index, *Executable);
		REQUIRE(Resources);
		return Resources->GearParts();
	};

	const FGearPartTables Rebalanced = Tables();
	REQUIRE(Rebalanced.UsesPerfectWorksRules());
	int32 Defended = 0;
	int32 ReadOtherwise = 0;
	for (int32 Id = 1; Id < 0x100 && !Rebalanced.Part(static_cast<uint8>(Id)).empty(); ++Id)
	{
		const std::span<const uint8> Part = Rebalanced.Part(static_cast<uint8>(Id));
		const uint16 Defense = LittleEndian::ReadU16(Part, PartDefense);
		const uint16 EtherDefense = LittleEndian::ReadU16(Part, PartEtherDefense);
		FGameState State;
		State.Block()[WeltallRecord + PartSlots] = static_cast<uint8>(Id);
		REQUIRE(ApplyGearParts(State, 0, Rebalanced));
		CAPTURE(Id);
		CHECK(LittleEndian::ReadU16(State.Block(), WeltallRecord + PartsDefense) == Defense);
		CHECK(LittleEndian::ReadU16(State.Block(), WeltallRecord + PartsEtherDefense) == EtherDefense);
		Defended += Defense != 0 ? 1 : 0;
		// The original's rules would have read the bytes +0Dh and +0Eh.
		ReadOtherwise += Defense != Part[PartDefense + 1] || EtherDefense != Part[PartEtherDefense] ? 1 : 0;
	}
	CHECK(Defended > 0);
	CHECK(ReadOtherwise > 0);

	Options.bRebalancedParty = false;
	CHECK_FALSE(Tables().UsesPerfectWorksRules());
}
