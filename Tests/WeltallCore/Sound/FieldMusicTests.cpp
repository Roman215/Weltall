#include "Media/PsxExecutable.h"
#include "Sound/GameSoundSession.h"
#include "Support/GameDiscs.h"
#include "Support/SoundRendering.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using Testing::Peak;

namespace
{
	constexpr int32 Second = 44100;
	/** A song plays far above this; the reverb's tail after it may hover a few units off 0, below Quiet. */
	constexpr int32 Playing = 1000;
	constexpr int32 Quiet = 64;

	/** The game's sound as it starts, with effect file 0 registered as the field does. */
	struct FFieldSound
	{
		explicit FFieldSound(const FIso9660& Disc)
		{
			auto Started = FGameSoundSession::Start(Disc);
			REQUIRE(Started);
			Session = std::move(*Started);
			REQUIRE(Session->Sound()->Lock()->RegisterEffects(Session->Resident().MainEffects));
		}

		FFieldMusic& Music() { return Session->FieldMusic(); }
		const FSoundBank* Bank(int32 Id) { return Session->Sound()->Lock()->FindBank(Id); }
		std::vector<int16> Render(int32 Frames) { return Testing::RenderSound(*Session->Sound(), Frames); }

		std::unique_ptr<FGameSoundSession> Session;
	};
}

TEST_CASE("The field's song table names each song's bank and its need of the battle themes' bank", "[Sound][Content]")
{
	for (const EDiscRelease Release : {EDiscRelease::UsaDisc1, EDiscRelease::UsaDisc2})
	{
		const FIso9660* Disc = Testing::FindDisc(Release);
		if (Disc == nullptr)
		{
			continue;
		}
		const auto Executable = ReadBootExecutable(*Disc);
		REQUIRE(Executable);
		const auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		const auto Songs = FFieldSongTable::Read(Disc->Image(), *Index, Release);
		REQUIRE(Songs);
		REQUIRE(Songs->Count() == 73);
		CHECK((*Songs)[0].BankSong == 0);
		CHECK((*Songs)[18].BankSong == 17);
		CHECK((*Songs)[64].BankSong == 40);
		CHECK((*Songs)[29].BankSong == FFieldSong::ResidentBank);
		CHECK((*Songs)[10].EvictsBattleThemes());
		CHECK((*Songs)[36].EvictsBattleThemes());
		CHECK((*Songs)[36].BankSong == 35);
		CHECK((*Songs)[35].NeedsBattleThemes());
		int32 Evicting = 0;
		for (int32 Song = 0; Song < Songs->Count(); ++Song)
		{
			Evicting += (*Songs)[Song].EvictsBattleThemes() ? 1 : 0;
			CHECK(((*Songs)[Song].EvictsBattleThemes() || (*Songs)[Song].NeedsBattleThemes()));
		}
		CHECK(Evicting == 24);
	}
}

TEST_CASE("A field song plays at full volume with its bank, or silent until the scripts fade it in", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	auto Loaded = Field.Music().ChangeSong(0, EFieldSongStart::FullVolume);
	REQUIRE(Loaded);
	CHECK(*Loaded);
	CHECK(Field.Music().Song() == 0);
	REQUIRE(Field.Music().Sequence() != nullptr);
	REQUIRE(Field.Bank(0x21) != nullptr);
	CHECK(FSoundDriver::BankAddress(*Field.Bank(0x21)) == 0x38000);
	CHECK(Peak(Field.Render(Second)) > Playing);
	// The same song again changes nothing.
	CHECK_FALSE(*Field.Music().ChangeSong(0, EFieldSongStart::FullVolume));

	REQUIRE(Field.Music().ChangeSong(4, EFieldSongStart::Silent));
	CHECK(Field.Bank(0x21) == nullptr);
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);
	Field.Session->Sound()->Lock()->SetSequenceVolume(*Field.Music().Sequence(), 0x7F, 0);
	CHECK(Peak(Field.Render(Second)) > Playing);

	Field.Music().Stop();
	CHECK(Field.Music().Sequence() == nullptr);
	Field.Render(3 * Second);
	CHECK(Peak(Field.Render(Second)) < Quiet);
}

TEST_CASE("A song played anew loads again from its start, even the song playing", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	REQUIRE(Field.Music().ChangeSong(8, EFieldSongStart::FullVolume));
	Field.Render(2 * Second);
	CHECK_FALSE(*Field.Music().ChangeSong(8, EFieldSongStart::FullVolume));
	const auto Again = Field.Music().PlayAnew(8);
	REQUIRE(Again);
	CHECK(*Again);
	CHECK(Field.Music().Song() == 8);
	CHECK(Field.Music().IsLoaded());
	REQUIRE(Field.Music().Sequence() != nullptr);
	CHECK(Peak(Field.Render(Second)) > Playing);
}

TEST_CASE("Asking silently for the song already playing leaves the next song silent only in the original", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	for (const bool bFixed : {true, false})
	{
		FFieldSound Field(*Disc);
		Field.Music().SetOptions({bFixed});
		REQUIRE(Field.Music().ChangeSong(0, EFieldSongStart::FullVolume));
		CHECK_FALSE(*Field.Music().ChangeSong(0, EFieldSongStart::Silent));
		// A battle's song, which no script asked to start silent.
		REQUIRE(Field.Music().StartBattleSong(29));
		Field.Render(3 * Second);
		CHECK((Peak(Field.Render(Second)) > Playing) == bFixed);
	}
}

TEST_CASE("Songs that need the battle themes' room evict their bank, which the next other song loads back", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	REQUIRE(Field.Music().ChangeSong(10, EFieldSongStart::FullVolume));
	CHECK(Field.Music().AreBattleThemesEvicted());
	CHECK(Field.Session->Resident().BattleThemeBank == nullptr);
	CHECK(Field.Bank(0x26) == nullptr);
	REQUIRE(Field.Bank(0x25) != nullptr);
	CHECK(FSoundDriver::BankAddress(*Field.Bank(0x25)) == 0x12000);

	// A battle theme plays with the resident bank, loaded back first.
	REQUIRE(Field.Music().ChangeSong(29, EFieldSongStart::FullVolume));
	CHECK_FALSE(Field.Music().AreBattleThemesEvicted());
	CHECK(Field.Bank(0x25) == nullptr);
	REQUIRE(Field.Bank(0x26) != nullptr);
	CHECK(Field.Session->Resident().BattleThemeBank == Field.Bank(0x26));
	CHECK(FSoundDriver::BankAddress(*Field.Bank(0x26)) == 0x12000);
	CHECK(Peak(Field.Render(Second)) > Playing);

	// Before the world map the field puts it back itself, stopping the music.
	REQUIRE(Field.Music().ChangeSong(30, EFieldSongStart::FullVolume));
	CHECK(Field.Bank(0x26) == nullptr);
	auto Restored = Field.Music().RestoreBattleThemes();
	REQUIRE(Restored);
	CHECK(*Restored);
	CHECK(Field.Bank(0x26) != nullptr);
	CHECK(Field.Music().Sequence() == nullptr);
}

TEST_CASE("A battle keeps the field's song, which goes on when the field returns, unless the battle is lost", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	REQUIRE(Field.Music().ChangeSong(0, EFieldSongStart::FullVolume));
	FSoundSequence* const FieldSong = Field.Music().Sequence();
	Field.Render(Second);

	REQUIRE(Field.Music().StartBattleSong(29));
	CHECK(Field.Music().Song() == 29);
	CHECK(Field.Music().Sequence() != FieldSong);
	CHECK(Field.Bank(0x21) == nullptr);
	CHECK(Peak(Field.Render(Second)) > Playing);

	// The field comes back with the song it left with: the kept one goes on, its bank loaded again.
	Field.Music().SetSong(0);
	REQUIRE(Field.Music().EnterField(false));
	CHECK(Field.Music().Sequence() == FieldSong);
	CHECK(Field.Bank(0x21) != nullptr);
	CHECK(Peak(Field.Render(2 * Second)) > Playing);

	// A lost battle returns to the defeat song instead.
	REQUIRE(Field.Music().StartBattleSong(29));
	Field.Music().SetSong(0);
	REQUIRE(Field.Music().EnterField(true));
	CHECK(Field.Music().Song() == FFieldMusic::DefeatSong);
	CHECK(Field.Music().Sequence() != nullptr);
	CHECK(Field.Bank(0x21) == nullptr);
}

TEST_CASE("A map's first scripts choose its song, which the map loads as it starts", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	REQUIRE(Field.Music().ChangeSong(0, EFieldSongStart::FullVolume));
	Field.Music().ChooseSong(0);
	CHECK_FALSE(Field.Music().IsLoading());
	CHECK_FALSE(*Field.Music().StartChosenSong());
	CHECK(Field.Music().Sequence() != nullptr);

	Field.Music().ChooseSong(6);
	CHECK(Field.Music().IsLoading());
	CHECK(Field.Music().Sequence() == nullptr);
	REQUIRE(Field.Music().StartChosenSong());
	CHECK_FALSE(Field.Music().IsLoading());
	CHECK(Field.Music().Song() == 6);
	CHECK(Field.Music().Sequence() != nullptr);
	CHECK(Field.Music().ChangeSong(200, EFieldSongStart::FullVolume).HasValue() == false);
}

// A regression pin, not a proof of fidelity: the driver's writes are pinned against the original by
// the oracle tests, but the samples depend on the port's SPU, which no capture of the console
// checks. It pins the whole chain as it is (start-up, the song's load, the interrupts' schedule
// among the samples, the SPU's voices, reverb and mix), so any change to what the player hears shows.
TEST_CASE("The first seconds of a field song come out as they did", "[Sound][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FFieldSound Field(*Disc);
	REQUIRE(Field.Music().ChangeSong(0, EFieldSongStart::FullVolume));
	const std::vector<int16> Samples = Field.Render(5 * Second);
	CHECK(Peak(Samples) > Playing);
	CHECK(Testing::DigestSamples(Samples) == "420f8a9bfe8f8a41d64c8aa3aaf1c9734bb465b7");
}
