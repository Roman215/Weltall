#include "Foundation/WholeFiles.h"
#include "Game/SaveStateSlots.h"
#include "Game/StateNotice.h"
#include "Menu/OptionsText.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"
#include "Text/MesGlyphs.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <system_error>

using namespace Weltall;

// The save state slots' files and lines, and the line's showing over the game.

namespace
{
	namespace Text = OptionsText::States;

	/** A folder of its own for a test's slots, emptied. */
	std::filesystem::path EmptyFolder(const char* Name)
	{
		const std::filesystem::path Folder = std::filesystem::temp_directory_path() / Name;
		std::error_code Error;
		std::filesystem::remove_all(Folder, Error);
		return Folder;
	}

	FSaveState StateAt(uint32 PlayTime)
	{
		FSaveState State;
		State.Game.SetPlayTime(PlayTime);
		return State;
	}
}

TEST_CASE("A play time is shown in hours and minutes", "[Game]")
{
	CHECK(StatePlayTime(0) == "0:00");
	CHECK(StatePlayTime(59 * 60) == "0:00");
	CHECK(StatePlayTime(60 * 60 * 65) == "1:05");
	CHECK(StatePlayTime(60 * 60 * (32 * 60 + 9)) == "32:09");
}

TEST_CASE("The state slots step round their ten, each telling its play time or that it is empty", "[Game]")
{
	FSaveStateSlots Slots(EmptyFolder("WeltallStateSlotsStep"));
	CHECK(Slots.Chosen() == 1);
	REQUIRE_FALSE(ReplaceWholeFile(Slots.File(2), WriteSaveState(StateAt(60 * 60 * 75))));
	CHECK(Slots.Step(1).Line == "Slot 2: 1:15");
	CHECK(Slots.Step(1).Line == "Slot 3: empty");
	CHECK(Slots.Step(-3).Line == "Slot 10: empty");
	CHECK(Slots.Chosen() == 10);
	CHECK(Slots.Step(1).Line == "Slot 1: empty");
	CHECK(Slots.Chosen() == 1);
}

TEST_CASE("A state slot's read tells an empty slot, another version's state and a damaged one", "[Game]")
{
	FSaveStateSlots Slots(EmptyFolder("WeltallStateSlotsRead"));
	FSaveStateTold Told;
	CHECK_FALSE(Slots.Read(Told).has_value());
	CHECK(Told.Line == "Slot 1 is empty.");
	CHECK(Told.Why.empty());

	std::vector<uint8> Written = WriteSaveState(StateAt(1234));
	REQUIRE_FALSE(ReplaceWholeFile(Slots.File(1), Written));
	const std::optional<FSaveState> Read = Slots.Read(Told);
	REQUIRE(Read.has_value());
	CHECK(Read->Game.PlayTime() == 1234);
	CHECK(Slots.Loaded() == "Loaded slot 1.");
	CHECK(Slots.NeedsDisc(1) == "Slot 1 needs disc 2.");

	Written[8] = static_cast<uint8>(FSaveState::FormatVersion + 1);
	REQUIRE_FALSE(ReplaceWholeFile(Slots.File(1), Written));
	CHECK_FALSE(Slots.Read(Told).has_value());
	CHECK(Told.Line == "Slot 1 is from another version.");
	CHECK_FALSE(Told.Why.empty());

	Written[8] = static_cast<uint8>(FSaveState::FormatVersion);
	Written.resize(Written.size() / 2);
	REQUIRE_FALSE(ReplaceWholeFile(Slots.File(1), Written));
	CHECK_FALSE(Slots.Read(Told).has_value());
	CHECK(Told.Line == "Slot 1 can't be read.");
}

TEST_CASE("A state's line shows at once, holds two seconds and fades over half of one", "[Game]")
{
	FStateNotice Notice;
	CHECK_FALSE(Notice.IsShown());
	CHECK(Notice.Strength() == 0);
	Notice.Show("Saved to slot 1.");
	CHECK(Notice.Strength() == 255);
	Notice.Advance(FStateNotice::HoldSeconds);
	CHECK(Notice.Strength() == 255);
	Notice.Advance(FStateNotice::FadeSeconds / 2);
	CHECK(Notice.Strength() == 127);
	Notice.Advance(FStateNotice::FadeSeconds / 2);
	CHECK_FALSE(Notice.IsShown());
	CHECK(Notice.Strength() == 0);
	Notice.Show("Loaded slot 1.");
	CHECK(Notice.Strength() == 255);
}

TEST_CASE("Every save state line, and every other line the corner tells, is in the game's font and fits across the screen", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FMesResident Resident = Testing::LoadMesResident(*Disc);
	// The line starts 12 pixels in and ends as far from the screen's right edge.
	constexpr int32 Room = 320 - 2 * 12;
	const auto Width = [&Resident](std::string_view Words)
	{
		int32 Units = 0;
		for (const char Character : Words)
		{
			const std::optional<FMesCode> Code = MesCodeOfCharacter(Character);
			CHECK(Code.has_value());
			Units += Code ? Resident.Font.WidthUnits(*Code) : 0;
		}
		return Units * 4;
	};
	const std::string Number = "10";
	const std::string Time = "199:59";
	for (const std::string& Line : {OptionsText::Fill(Text::Saved, {Number}), OptionsText::Fill(Text::Loaded, {Number}),
			 OptionsText::Fill(Text::Chosen, {Number, Time}), OptionsText::Fill(Text::ChosenEmpty, {Number}), std::string(Text::CannotSave),
			 OptionsText::Fill(Text::NotWritten, {Number}), OptionsText::Fill(Text::Empty, {Number}), OptionsText::Fill(Text::OtherVersion, {Number}),
			 OptionsText::Fill(Text::Unreadable, {Number}), OptionsText::Fill(Text::NeedsDisc, {Number, "2"}), OptionsText::Fill(Text::OtherFiles, {Number}),
			 std::string(OptionsText::SecondDisc::ExpansionMissing)})
	{
		INFO(Line);
		CHECK(Width(Line) <= Room);
	}
}
