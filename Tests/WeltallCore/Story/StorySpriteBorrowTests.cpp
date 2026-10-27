#include "Field/FieldMap.h"
#include "Field/FieldMapPatch.h"
#include "Field/FieldRuntime.h"
#include "Field/FieldSpriteFiles.h"
#include "Story/StoryPacks.h"
#include "Support/FieldTickSession.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <string>
#include <string_view>

using namespace Weltall;

namespace
{
	/** Nisan's town (map 93) with the Shevat hangar's (map 164) Solarian uniform, slot 2, in its slot 9; an actor wears it. */
	constexpr int32 OwnMap = 1003;
	constexpr int32 Base = 93;
	constexpr int32 Other = 164;
	constexpr int32 Slot = 9;
	constexpr int32 SourceSlot = 2;
	constexpr uint32 InitRan = 0x7F0;
	constexpr std::string_view BorrowText =
		"map 1003 from 93\n"
		"sprite 9 from map 164 slot 2\n"
		"actors 2\n"
		"actor 0 script 0:\n"
		"    SetHiddenModel\n"
		"    SetVar var[7F0] #1\n"
		"    End\n"
		"actor 1 script 0:\n"
		"    SetModel #9\n"
		"    SetPosition #0 #1500\n"
		"    End\n"
		"actor 0 script 1:\n"
		"actor 0 script 2:\n"
		"actor 0 script 3:\n"
		"actor 1 script 1:\n"
		"actor 1 script 2:\n"
		"actor 1 script 3:\n"
		"    End\n";

	std::optional<FError> EnterAndTick(const Testing::FOracleFieldFiles& Files, const FMesResident& Resident, const FStoryPacks& Story, int32 Number, int32 Ticks,
		const std::function<void(const FFieldRuntime&)>& Check)
	{
		Testing::FOracleTickSession Session(Files);
		FFieldServices Services = Session.Services(Files, Resident);
		Services.Story = &Story;
		Services.Options = FEventOptions::Original();
		Services.Game->SetStoryMode(EStoryMode::Unchosen);
		FFieldRuntime Runtime(Services);
		if (std::optional<FError> Error = Runtime.EnterMap(Number))
		{
			return Error;
		}
		for (int32 Tick = 0; Tick < FFieldRuntime::EntryTicks + Ticks; ++Tick)
		{
			const std::vector<FPadState> Vblanks(static_cast<std::size_t>(Runtime.VblanksPerTick()));
			REQUIRE_FALSE(Runtime.Tick(Vblanks));
		}
		Check(Runtime);
		return std::nullopt;
	}
}

TEST_CASE("A map of its own takes a sprite slot from another disc map: that map's sprite file and images, at the slot's own place", "[Story][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	std::optional<Testing::FOracleFieldFiles> Files;
	REQUIRE(Testing::LoadOracleFieldFiles(*Disc, Files));
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	const auto BaseMap = LoadFieldMap(Disc->Image(), Files->Index, Base);
	const auto OtherMap = LoadFieldMap(Disc->Image(), Files->Index, Other);
	REQUIRE(BaseMap);
	REQUIRE(OtherMap);
	const auto Borrowed = FieldSpriteFileBytes(OtherMap->SpriteSets, SourceSlot);
	REQUIRE(Borrowed);

	FStoryPacks Story;
	REQUIRE_FALSE(Story.Add("test", "borrow.txt", BorrowText));
	const auto Entered = EnterAndTick(*Files, Resident, Story, OwnMap, 30, [&](const FFieldRuntime& Runtime)
	{
		const FFieldMap& Map = Runtime.Map();
		// The slot's file is the other map's; every other file is the base map's.
		const auto Files = ParseFieldSpriteFiles(Map.SpriteSets);
		const auto BaseFiles = ParseFieldSpriteFiles(BaseMap->SpriteSets);
		REQUIRE(Files);
		REQUIRE(BaseFiles);
		CHECK(Files->size() == BaseFiles->size());
		const auto Ours = FieldSpriteFileBytes(Map.SpriteSets, Slot);
		REQUIRE(Ours);
		const bool bTheirs = std::equal(Ours->begin(), Ours->end(), Borrowed->begin(), Borrowed->end());
		CHECK(bTheirs);
		for (uint32 File = 0; File < Files->size(); ++File)
		{
			if (static_cast<int32>(File) == Slot)
			{
				continue;
			}
			const auto Kept = FieldSpriteFileBytes(Map.SpriteSets, File);
			const auto Was = FieldSpriteFileBytes(BaseMap->SpriteSets, File);
			REQUIRE(Kept);
			REQUIRE(Was);
			const bool bSame = std::equal(Kept->begin(), Kept->end(), Was->begin(), Was->end());
			CHECK(bSame);
		}
		// Its images come after the base map's uploads, at the slot's place and palette row.
		CHECK(Map.TextureUploads.size() > BaseMap->TextureUploads.size());
		const FFieldSpriteSlot& At = Map.SpriteSlots[static_cast<std::size_t>(Slot)];
		bool bImagesAtSlot = false;
		bool bPaletteAtRow = false;
		for (std::size_t Upload = BaseMap->TextureUploads.size(); Upload < Map.TextureUploads.size(); ++Upload)
		{
			const FVramImage& Image = Map.TextureUploads[Upload];
			bImagesAtSlot = bImagesAtSlot || (Image.Rect.X == At.X && Image.Rect.Y == At.Y);
			bPaletteAtRow = bPaletteAtRow || (Image.Rect.Y == 480 + Slot && Image.Rect.Height == 1);
		}
		CHECK(bImagesAtSlot);
		CHECK(bPaletteAtRow);
		// The scripts ran through, the actor that wears it included (a slot without a file would have stopped them).
		const FEventContext& Context = Runtime.Context();
		CHECK(Context.Variables.Get(InitRan, *Context.Scripts) == 1);
	});
	INFO((Entered ? Entered->Message : std::string()));
	CHECK_FALSE(Entered);

	// A slot the base map has no file for, and a map that is not one of its own, are refused.
	FStoryPacks NoFile;
	REQUIRE_FALSE(NoFile.Add("test", "nofile.txt", "map 1004 from 93\nsprite 20 from map 164 slot 2\nactors 1\nactor 0 script 0:\nactor 0 script 1:\nactor 0 script 2:\nactor 0 script 3:\n    End\n"));
	const auto Refused = EnterAndTick(*Files, Resident, NoFile, 1004, 0, [](const FFieldRuntime&) {});
	REQUIRE(Refused);
	CHECK(Refused->Message.find("sprite 20 from map 164 slot 2") != std::string::npos);
	const auto Patch = FFieldMapPatch::Parse("map 93\nsprite 0 from map 164 slot 2\n");
	REQUIRE_FALSE(Patch);
	CHECK(Patch.Error().Message.find("only a map of its own") != std::string::npos);
}
