#include "Field/FieldMap.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldScriptFixes.h"
#include "Game/FieldProgram.h"
#include "Mods/PerfectWorks.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace Weltall;

TEST_CASE("The party rides the pirate base platform down with Perfect Works", "[Field][Content][Mods]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
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
	Options.bScript = true;
	const auto Files = Release->Files(Options, 1);
	const Testing::FModFilesGiven Mod(*Disc, FDiscModFiles::Make(*Index, 1, Files, Release->Changes(Options, Files)));

	const auto Game = Testing::FGameLoopSession::Start(*Disc);
	constexpr int32 Hangar = 84;
	Game->Session->Game().SetSavedEventVariable(0, 59);
	Game->Session->Game().SetParty({0, 2, 3});
	Game->Session->Game().SetJoinedCharacters(0x0D);
	Game->EnterField(Hangar);
	REQUIRE(Game->Loop->Field() != nullptr);
	const FEventContext& Context = Game->Loop->Field()->Runtime().Context();
	Game->RunUntil([&] { return Context.bFieldRunning && Context.bPlayerControlled && Context.States[2].Layer == 1; }, 400);
	CHECK(Context.RunningMap == Hangar);
	CHECK(Context.States[2].Position[0] >> 16 == -566);
	CHECK(Context.States[2].Position[2] >> 16 == 700);
}

TEST_CASE("Perfect Works field scenes finish their dialogue and fade on instruction boundaries", "[Field][Content][Mods]")
{
	const auto Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
	if (!Release || Release->Version() != "0.11.4")
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	FPerfectWorksOptions Options;
	Options.bEnabled = true;
	Options.bScript = true;
	const auto ReadScripts = [&](const FIso9660& Disc, int32 Number, int32 Map)
	{
		const auto Executable = ReadBootExecutable(Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Files = Release->Files(Options, Number);
		const Testing::FModFilesGiven Mod(Disc, FDiscModFiles::Make(*Index, Number, Files, Release->Changes(Options, Files)));
		const auto Field = LoadFieldMap(Disc.Image(), *Index, Map);
		REQUIRE(Field);
		auto Scripts = FEventScripts::Parse(Field->EventScripts);
		REQUIRE(Scripts);
		ApplyFieldScriptFixes(Map, *Scripts, FEventOptions::Original());
		return std::move(*Scripts);
	};
	REQUIRE_DISC(First, EDiscRelease::UsaDisc1);
	const FEventScripts Hideout = ReadScripts(*First, 1, 75);
	CHECK(Hideout.Code()[0x874] == 0x95);
	CHECK(Hideout.Code()[0x895] == 0);
	REQUIRE_DISC(Second, EDiscRelease::UsaDisc2);
	const FEventScripts Narration = ReadScripts(*Second, 2, 618);
	CHECK(Narration.Code()[0x27A] == 0x8E);
	CHECK(Narration.Code()[0x28E] == 0x23);
	CHECK(Narration.Code()[0x28F] == 0);
}

// The field's fixes of the original's quirks on the real maps they were found on, each against the
// original's behavior, which the tick oracles check.

TEST_CASE("Map 592's guide on the ladder keeps his own animation, or in the original takes the ladder's past his table", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	constexpr int32 Map = 592;
	constexpr std::size_t Guide = 9;
	const auto GuideAfterEntry = [&](const FEventOptions& Options)
	{
		Testing::FOracleTickSession Session(*Files, Map);
		FFieldServices Services = Session.Services(*Files, Resident);
		Services.Options = Options;
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(Map));
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks; ++Tick)
		{
			REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())));
		}
		const FEventActorState& State = Runtime.Context().States[Guide];
		return std::pair{static_cast<int16>(State.Animation), static_cast<int16>(State.IdleAnimation)};
	};
	constexpr int16 LadderAnimation = 6;
	CHECK(GuideAfterEntry(FEventOptions::Original()).first == LadderAnimation);
	const auto [Animation, Idle] = GuideAfterEntry(FEventOptions());
	CHECK(Animation == Idle);
	CHECK(Animation != LadderAnimation);
}

TEST_CASE("The Bledavik hotel's head count asks about party slot 2, or in the original about slot 1 twice", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	constexpr int32 Map = 104;
	const auto SecondTest = [&](const FEventOptions& Options)
	{
		auto Loaded = LoadFieldMap(Disc->Image(), Files->Index, Map);
		REQUIRE(Loaded);
		auto Scripts = FEventScripts::Parse(Loaded->EventScripts);
		REQUIRE(Scripts);
		ApplyFieldScriptFixes(Map, *Scripts, Options);
		// Actor 14's script 4 at 039Fh: If var[?] == #255 else L03B0.
		const uint8* Code = Scripts->Code();
		REQUIRE(Code[0x39F] == 0x02);
		return Code[0x3A0] | Code[0x3A1] << 8;
	};
	CHECK(SecondTest(FEventOptions::Original()) == 0x040);
	CHECK(SecondTest(FEventOptions()) == 0x042);
}

TEST_CASE("Characters a wide view shows walk and animate, or in the original those off the 4:3 screen stand frozen", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	constexpr int32 Map = 1;
	const auto OffScreenAfterEntry = [&](const FEventOptions& Options)
	{
		Testing::FOracleTickSession Session(*Files, Map);
		FFieldServices Services = Session.Services(*Files, Resident);
		Services.Options = Options;
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(Map));
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks; ++Tick)
		{
			REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())));
		}
		std::vector<bool> OffScreen;
		for (const FEventActorState& State : Runtime.Context().States)
		{
			OffScreen.push_back((State.Flags1 & EventActorFlags1::OffScreen) != 0);
		}
		return OffScreen;
	};
	FEventOptions Wide = FEventOptions::Original();
	Wide.bActorsRunInWideView = true;
	const std::vector<bool> Original = OffScreenAfterEntry(FEventOptions::Original());
	const std::vector<bool> Widened = OffScreenAfterEntry(Wide);
	REQUIRE(Original.size() == Widened.size());
	// The wide view holds the 4:3 screen: none is off it that the original shows, and some the original froze are in it.
	int32 Seen = 0;
	for (std::size_t Actor = 0; Actor < Original.size(); ++Actor)
	{
		CHECK((!Widened[Actor] || Original[Actor]));
		Seen += Original[Actor] && !Widened[Actor] ? 1 : 0;
	}
	CHECK(Seen > 0);
}

TEST_CASE("A party sprite takes the file loaded over its slot whole, or in the original reads it through the old tables", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	constexpr int32 Map = 1;
	// After the entry, slot 0's buffer takes Citan's file under the leader's sprite, as FE 19 and FE 1A can.
	const auto LeaderTablesAreTheBuffers = [&](const FEventOptions& Options)
	{
		Testing::FOracleTickSession Session(*Files, Map);
		FFieldServices Services = Session.Services(*Files, Resident);
		Services.Options = Options;
		FFieldRuntime Runtime(Services);
		REQUIRE_FALSE(Runtime.EnterMap(Map));
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks; ++Tick)
		{
			REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())));
		}
		const FEventContext& Context = Runtime.Context();
		const FSprite* Leader = Context.Models->ActorSprite(Context.LeaderActor);
		REQUIRE(Leader != nullptr);
		REQUIRE(Leader->Resource->File == &Session.Party.Buffer(0));
		REQUIRE_FALSE(Session.Party.Unpack(0, 2));
		REQUIRE_FALSE(Runtime.Tick(Testing::OracleVblanks({}, Runtime.VblanksPerTick())));
		const FSpriteTables& Taken = Leader->Resource->Tables;
		const FSpriteTables& Own = Session.Party.Buffer(0).Tables();
		return Taken.Animations == Own.Animations && Taken.Frames == Own.Frames && Taken.Palettes == Own.Palettes;
	};
	FEventOptions Following = FEventOptions::Original();
	Following.bPartySpritesFollowTheirFile = true;
	CHECK(LeaderTablesAreTheBuffers(Following));
	CHECK_FALSE(LeaderTablesAreTheBuffers(FEventOptions::Original()));
}

TEST_CASE("Map 272's sounds take the volumes their scripts mean and map 670's room sound fades, or in the original neither", "[Field][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	// Each instruction's channel and volume operands' low bytes, as the map's scripts hold them after the fixes.
	const auto Operands = [&](int32 Map, const FEventOptions& Options, uint16 At, uint8 Opcode, int32 VolumeAt, int32 ChannelAt)
	{
		auto Loaded = LoadFieldMap(Disc->Image(), Files->Index, Map);
		REQUIRE(Loaded);
		auto Scripts = FEventScripts::Parse(Loaded->EventScripts);
		REQUIRE(Scripts);
		ApplyFieldScriptFixes(Map, *Scripts, Options);
		const uint8* Code = Scripts->Code();
		REQUIRE(Code[At] == 0xFE);
		REQUIRE(Code[At + 1] == Opcode);
		return std::pair{static_cast<int32>(Code[At + VolumeAt]), static_cast<int32>(Code[At + ChannelAt])};
	};
	for (const uint16 At : {uint16{0x66}, uint16{0x80}, uint16{0xC6}})
	{
		CHECK(Operands(272, FEventOptions(), At, 0x62, 2, 4) == std::pair{30, 0});
		CHECK(Operands(272, FEventOptions::Original(), At, 0x62, 2, 4) == std::pair{0, 30});
	}
	CHECK(Operands(272, FEventOptions(), 0xBA, 0x62, 2, 4) == std::pair{30, 1});
	CHECK(Operands(272, FEventOptions::Original(), 0xBA, 0x62, 2, 4) == std::pair{1, 30});
	CHECK(Operands(670, FEventOptions(), 0x944, 0x8C, 2, 4) == std::pair{0, 3});
	CHECK(Operands(670, FEventOptions::Original(), 0x944, 0x8C, 2, 4) == std::pair{0, 12});
}
