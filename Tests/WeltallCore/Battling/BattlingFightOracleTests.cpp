#include "Battling/BattlingDrawing.h"
#include "Battling/BattlingFight.h"
#include "Battling/BattlingMenus.h"
#include "Formats/Lzss.h"
#include "Game/BattlingGameProgram.h"
#include "Foundation/Sha1.h"
#include "Game/PsxRandom.h"
#include "Graphics/MdlLights.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Mods/PerfectWorks.h"
#include "Support/BattlingFightDigest.h"
#include "Support/GameDiscs.h"
#include "Support/MenuFrameDigest.h"
#include "Support/XorShift32.h"
#include "Text/MesResident.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

using namespace Weltall;
using Weltall::Testing::BattlingFightPartBytes;
using Weltall::Testing::BattlingFightParts;
using Weltall::Testing::FBattlingSoundRecorder;
using Weltall::Testing::FModFilesGiven;
using Weltall::Testing::FXorShift32;

// Battling's fight against the original program run whole: two gears loaded from
// their files, a round started, then pass after pass of the fight tick, the camera and the frame's drawing, every part of
// the state digested after each pass. The pads are random buttons and sticks drawn from the same generator on both sides; the
// cases with menus press Start now and then and run the paused passes' menus, one vertical blank a pass. The script machine's
// cases run the tutorial, its lessons chosen and confirmed by the same pads, or the scripted bout's ending, with the kernel's
// message window typing the disc's text. The last two are the menus' demonstration, its gears both the computer's, its
// passes the demonstration's.
namespace
{
	struct FFightCase
	{
		std::array<int32, 2> Gears;
		EBattlingMode Mode;
		uint8 Level;
		bool bRubberBand;
		uint32 Seed;
		uint32 Pads;
		int32 Ticks;
		std::array<bool, 2> Computer;
		uint8 Style;
		/** Where each gear starts (x and z): the round's marks but for the cases at the arena's edge. */
		std::array<std::array<int32, 2>, 2> Start;
		const char* Digest;
		int32 TicksRun;
		/** The wins a set needs (0 none), for a case of whole sets; a case of the fight alone (-1) stops at the replay. */
		int32 WinsPerSet = -1;
		/** The pads press Start, L2 and repeat too, and the pauses run. */
		bool bMenus = false;
		/** The story's bout against Ganador, which ends scripted. */
		bool bScripted = false;
		/** Player 1's HP as the round starts, n/255 of its maximum (-1 full). */
		int32 Hp = -1;
		/** The bout fades in through the versus screen's fire, as a story bout against the computer does. */
		bool bFire = false;
		/** Player 2's HP as the round starts, as Hp. */
		int32 Hp2 = -1;
		/** The winner screen's pads (DrawWinnerPads): its passes before the confirm; -1 the bout's pads throughout. */
		int32 WinnerPasses = -1;
	};

	constexpr std::array<std::array<int32, 2>, 2> Marks{{{16000, 0x3F80}, {0x4080, 0x3F80}}};
	constexpr std::array<std::array<int32, 2>, 2> EastEdge{{{0x3F80 + 14600, 0x3F80 + 300}, {0x3F80 + 15200, 0x3F80 + 300}}};
	constexpr std::array<std::array<int32, 2>, 2> NorthEdge{{{0x3F80 - 300, 0x3F80 - 14800}, {0x3F80 + 300, 0x3F80 - 15300}}};

	constexpr std::array<FFightCase, 33> Cases{{
		{{0, 17}, EBattlingMode::ComputerVersusComputer, 0, false, 0x00001234, 0, 1200, {true, true}, 0, Marks, "43b6e332fcb2c37d0cef4c1c2c53791d38a3b3e0", 1200},
		{{1, 5}, EBattlingMode::ComputerVersusComputer, 1, false, 0x0BADF00D, 0, 1200, {true, true}, 0, Marks, "0650a55d102d4f907272c614ec3f46bcd0c26a80", 1200},
		{{10, 13}, EBattlingMode::ComputerVersusComputer, 2, false, 0x51DE5EED, 0, 1200, {true, true}, 0, Marks, "59fa1ef7a5713476ebd2dbc3ff4b6eeccfc21b79", 833},
		{{3, 36}, EBattlingMode::ComputerVersusComputer, 2, true, 0x0000ABCD, 0, 1200, {true, true}, 0, Marks, "1a771192b3b2f9eca229a365296241cce1feaa2b", 1200},
		{{28, 9}, EBattlingMode::VersusComputer, 1, false, 0x00C0FFEE, 0x13579BDF, 900, {false, true}, 0, Marks, "1d767e23dc334d4a018706e45f843f84107e46fd", 829},
		{{0, 41}, EBattlingMode::TwoPlayers, 0, false, 0x00FACADE, 0x2468ACE0, 900, {false, false}, 0, Marks, "f4c10704039e606233756f2e5a49aaad418c7c4a", 900},
		{{11, 22}, EBattlingMode::Practice, 0, false, 0x0000BEEF, 0x1F2E3D4C, 900, {false, true}, 12, Marks, "83009c2bd0ea09a7895be37bdab1b7b96a8853a1", 900},
		{{27, 37}, EBattlingMode::ComputerVersusComputer, 1, false, 0x00DECADE, 0, 1200, {true, true}, 0, Marks, "ee36387ed7421c9f69d64d2c1047c7e4448c12ad", 1200},
		{{0, 18}, EBattlingMode::ComputerVersusComputer, 2, false, 0x0EDCBA98, 0, 1200, {true, true}, 0, EastEdge, "603c1540e2d159d96ac5ec3f330468546be9124d", 998},
		{{2, 24}, EBattlingMode::VersusComputer, 0, false, 0x00ED6E00, 0x5A5A0F0F, 900, {false, true}, 0, NorthEdge, "110db13a6bf4fd4a153a070ba71400a5e8a88427", 611},
		{{0, 28}, EBattlingMode::ComputerVersusComputer, 2, false, 0x0001F00D, 0, 4000, {true, true}, 0, Marks, "bba4901b530e0962a394842cd2ebc581e53b572e", 4000, 2},
		{{5, 17}, EBattlingMode::VersusComputer, 0, false, 0x00BA5EBA, 0x0BEEF123, 3000, {false, true}, 0, Marks, "4cfa56782c1482a47ed62467e16bd7a2ce4c8b63", 561, 1},
		{{9, 33}, EBattlingMode::TwoPlayers, 0, false, 0x00C0C0A0, 0x7E57AB1E, 3000, {false, false}, 0, Marks, "2088d9b4a9e8ac72f99cfd423fcd2a8554d00134", 3000, 0},
		{{12, 25}, EBattlingMode::ComputerVersusComputer, 1, true, 0x00AB12CD, 0, 4000, {true, true}, 0, Marks, "614d3fa00aadfaea29ff349d840b0c3e7c960b96", 2793, 1},
		{{7, 20}, EBattlingMode::TwoPlayers, 0, false, 0x00F1E2D3, 0x31415926, 3000, {false, false}, 0, Marks, "d398d5bdb728bb17aeb839cacf6ed9eedc8edc10", 992, 1},
		{{0, 21}, EBattlingMode::VersusComputer, 1, false, 0x00A11CE5, 0x600DCAFE, 3000, {false, true}, 0, Marks, "055ca5b3f0eccb369ba51130e92f831b3d10e9be", 820, 2, true},
		{{4, 30}, EBattlingMode::TwoPlayers, 0, false, 0x00B0B0B0, 0x0DDBA133, 3000, {false, false}, 0, Marks, "d8fe24f39550a01f64748a67aaab351a2c6eada5", 580, 0, true},
		{{14, 6}, EBattlingMode::Practice, 0, false, 0x00CAFE01, 0x7A7A12B2, 3000, {false, true}, 3, Marks, "ae2caa0e778d66f380ab26a7a759ae6841f52bde", 1333, 0, true},
		{{19, 31}, EBattlingMode::ComputerVersusComputer, 2, true, 0x00D00D1E, 0x5EED0042, 3000, {true, true}, 0, Marks, "fd8b507762aba53a3a5da1947855a100850ded90", 609, 1, true},
		{{0, 28}, EBattlingMode::Tutorial, 0, false, 0x00707070, 0x7070CAFE, 3000, {false, false}, 0, Marks, "6ac28f35913021a8b21362dd7ec3afc7abf92a46", 3000},
		{{0, 1}, EBattlingMode::Tutorial, 0, false, 0x00ABCDEF, 0x1E550A51, 3000, {false, false}, 0, Marks, "1be60e4d9b8af3c4cb608c7d645df39d900c68c0", 1045},
		{{0, 28}, EBattlingMode::Tutorial, 0, false, 0x00515151, 0x0F1E2D3C, 3000, {false, false}, 0, Marks, "7ae9c73f0ca7f6feb557c70501f402e2ac85d0f0", 321},
		{{0, 17}, EBattlingMode::VersusComputer, 0, false, 0x00DEFACE, 0x6A6A6A6A, 2000, {false, true}, 0, Marks, "c6ba0489a6471cbeda420b8b60cbb2246a3b6e91", 986, -1, false, true, 0x68},
		{{0, 17}, EBattlingMode::VersusComputer, 0, false, 0x00FEED00, 0x13131313, 2000, {false, true}, 0, Marks, "5c3b552e6c5518c5ab408749b3a5bf3c1efc31cf", 1012, -1, false, true, 0x70},
		{{0, 28}, EBattlingMode::Tutorial, 0, false, 0x00707070, 0x00012345, 3000, {false, false}, 0, Marks, "58862fc27fff055205cab0d96180db1626b79260", 3000},
		{{0, 17}, EBattlingMode::VersusComputer, 0, false, 0x00DEFACE, 0x00004444, 2000, {false, true}, 0, Marks, "d57b4eef158ef9e3c7dfda61e123b59091a1b1ec", 1047, -1, false, true},
		{{0, 23}, EBattlingMode::VersusComputer, 0, false, 0x00F12E00, 0x0F12E123, 400, {false, true}, 0, Marks, "f398dbe3206b402d930c4c04b0eb1c9618a7872f", 400, -1, false, false, -1, true},
		{{0, 17}, EBattlingMode::VersusComputer, 0, false, 0x00F12E17, 0x44440000, 2000, {false, true}, 0, Marks, "34838e2347fba478b604198f3be9b24a60f72722", 949, -1, false, true, 0x68, true},
		{{0, 17}, EBattlingMode::VersusComputer, 1, false, 0x00500000, 0x0A000000, 2000, {false, true}, 0, Marks, "94a37ad9b528ec8292fae1920c614e98ad245583", 303, 1, false, false, -1, false, 0x10, 150},
		{{22, 5}, EBattlingMode::TwoPlayers, 1, false, 0x0050BBBC, 0x0A0B0B12, 2000, {false, false}, 0, Marks, "3204a880984694f57c8ad4c4c68e06456486b32a", 292, 1, false, false, -1, false, 0x10, 150},
		{{21, 7}, EBattlingMode::TwoPlayers, 1, false, 0x0050999B, 0x0A090917, 2000, {false, false}, 0, Marks, "652092f0208873b3d0ed1e90eb029f8cefe4a598", 296, 1, false, false, 0x10, false, -1, 150},
		{{0, 1}, EBattlingMode::Demonstration, 2, false, 0x00DE3000, 0, 1500, {true, true}, 0, Marks, "65f5e5edeabee49a6e2018ba2ff60f47aa24511d", 1500},
		{{13, 0}, EBattlingMode::Demonstration, 1, true, 0x00DE3001, 0, 1500, {true, true}, 0, Marks, "929fc92f1cb7353abaee29d063863a5965ac9f3c", 1500},
	}};

	struct FBattlingDisc
	{
		/** The rules its files are for: the game's, or a mode of Perfect Works' arena given to the disc. */
		EBattlingRules Rules = EBattlingRules::Original;
		FBattlingProgramTables Tables;
		FBattlingTerrain Terrain;
		FBattlingGearTable GearTable;
		FBattlingMessages Messages;
		FMesResident Text;
		const FIso9660* Disc = nullptr;
		std::unique_ptr<FGameFileIndex> Index;
	};

	/**
	 * The pass's vertical blanks of random buttons for both pads (no Start, Select, L2 or R2), and the sticks' latest reading.
	 * With menus, L2 too, Start now and then on either pad, and the kernel's repeat.
	 */
	std::span<const std::array<FBattlingPad, 2>> DrawPads(FXorShift32& Random, int32 Count, bool bMenus, std::array<std::array<FBattlingPad, 2>, 2>& Blanks)
	{
		const std::span<std::array<FBattlingPad, 2>> Drawn = std::span(Blanks).first(std::min(static_cast<std::size_t>(Count), Blanks.size()));
		for (std::array<FBattlingPad, 2>& Blank : Drawn)
		{
			Blank = {};
			if (bMenus)
			{
				Blank[0].Held = static_cast<uint16>(Random.Next() & 0xF0FD);
				Blank[1].Held = static_cast<uint16>(Random.Next() & 0xF0FD);
				const uint32 Start = Random.Next();
				Blank[0].Held = static_cast<uint16>(Blank[0].Held | ((Start & 0x3F) == 0 ? 0x800 : 0));
				Blank[1].Held = static_cast<uint16>(Blank[1].Held | ((Start & 0xFC0) == 0 ? 0x800 : 0));
				Blank[0].Pressed = static_cast<uint16>(Blank[0].Held & Random.Next());
				Blank[1].Pressed = static_cast<uint16>(Blank[1].Held & Random.Next());
				Blank[0].Repeated = static_cast<uint16>(Blank[0].Pressed | (Blank[0].Held & Random.Next()));
				Blank[1].Repeated = static_cast<uint16>(Blank[1].Pressed | (Blank[1].Held & Random.Next()));
				continue;
			}
			Blank[0].Held = static_cast<uint16>(Random.Next() & 0xF0FC);
			Blank[1].Held = static_cast<uint16>(Random.Next() & 0xF0FC);
			Blank[0].Pressed = static_cast<uint16>(Blank[0].Held & Random.Next());
			Blank[1].Pressed = static_cast<uint16>(Blank[1].Held & Random.Next());
		}
		for (int32 Port = 0; Port < 2; ++Port)
		{
			const uint8 X = static_cast<uint8>(Random.Next());
			const uint8 Y = static_cast<uint8>(Random.Next());
			for (std::array<FBattlingPad, 2>& Blank : Blanks)
			{
				Blank[static_cast<std::size_t>(Port)].LeftX = X;
				Blank[static_cast<std::size_t>(Port)].LeftY = Y;
			}
		}
		for (std::array<FBattlingPad, 2>& Blank : Blanks)
		{
			Blank[0].Type = 4;
			Blank[1].Type = 4;
		}
		return Drawn;
	}

	/**
	 * The winner screen's pads, both alike: Left and Right held at random (the spin's speed), L2 pressed one blank in 32 (the
	 * text shown or hidden), and the confirm pressed with bConfirm.
	 */
	std::span<const std::array<FBattlingPad, 2>> DrawWinnerPads(FXorShift32& Random, int32 Count, bool bConfirm, std::array<std::array<FBattlingPad, 2>, 2>& Blanks)
	{
		const std::span<std::array<FBattlingPad, 2>> Drawn = std::span(Blanks).first(std::min(static_cast<std::size_t>(Count), Blanks.size()));
		for (std::array<FBattlingPad, 2>& Blank : Drawn)
		{
			const uint32 Bits = Random.Next();
			uint16 Pressed = ((Bits >> 16) & 0x1F) == 0 ? 0x01 : 0;
			if (bConfirm)
			{
				Pressed |= 0x20;
			}
			for (FBattlingPad& Pad : Blank)
			{
				Pad = {};
				Pad.Type = 4;
				Pad.Held = static_cast<uint16>((Bits & 0xA000) | Pressed);
				Pad.Pressed = Pressed;
				Pad.Repeated = Pressed;
			}
		}
		return Drawn;
	}

	void ReadDisc(FBattlingDisc& Disc, const FIso9660* Iso)
	{
		const auto Executable = ReadBootExecutable(*Iso);
		REQUIRE(Executable);
		auto Index = FGameFileIndex::Read(*Executable);
		REQUIRE(Index);
		Disc.Disc = Iso;
		Disc.Index = std::make_unique<FGameFileIndex>(std::move(*Index));
		auto Tables = FBattlingProgramTables::Read(Iso->Image(), *Disc.Index, *Executable, EDiscRelease::UsaDisc1);
		REQUIRE(Tables);
		Disc.Tables = std::move(*Tables);
		auto Terrain = FBattlingTerrain::Read(Iso->Image(), *Disc.Index);
		REQUIRE(Terrain);
		Disc.Terrain = std::move(*Terrain);
		auto GearTable = FBattlingGearTable::Read(Iso->Image(), *Disc.Index);
		REQUIRE(GearTable);
		Disc.GearTable = *GearTable;
		auto Messages = FBattlingMessages::Read(Iso->Image(), *Disc.Index);
		REQUIRE(Messages);
		Disc.Messages = std::move(*Messages);
		auto Text = LoadMesResident(Iso->Image(), *Disc.Index, *Executable);
		REQUIRE(Text);
		Disc.Text = std::move(*Text);
	}

	/** Weltall against Siebzehn's files, for practice bouts set up by a test. */
	class FPracticeGears
	{
	public:
		explicit FPracticeGears(const FBattlingDisc& InDisc) : Disc(InDisc)
		{
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				auto File = FBattlingGearFile::Read(Disc.Disc->Image(), *Disc.Index, Gears[Side]);
				REQUIRE(File);
				Files[Side] = std::move(*File);
			}
		}

		/** A practice bout against the computer, its first round started; Configure changes the settings before the gears load. */
		template <typename TConfigure>
		std::unique_ptr<FBattlingFight> Start(FPsxRandom& Random, FBattlingSoundRecorder& Recorder, TConfigure Configure) const
		{
			auto Fight = std::make_unique<FBattlingFight>(Disc.Tables, Disc.Terrain, Random, Recorder);
			Fight->Settings.Mode = EBattlingMode::Practice;
			Fight->Settings.Computer = {false, true};
			Configure(Fight->Settings);
			for (std::size_t Side = 0; Side < 2; ++Side)
			{
				auto Model = ParseMdlModel(std::span<const uint8>(Files[Side].Bytes).subspan(Files[Side].Model));
				REQUIRE(Model);
				Fight->LoadFighter(static_cast<int32>(Side), Gears[Side], Files[Side], Disc.GearTable.Gears[static_cast<std::size_t>(Gears[Side])],
					std::move(*Model));
			}
			Fight->StartRound();
			Fight->State = 1;
			return Fight;
		}

	private:
		static constexpr std::array<int32, 2> Gears{0, 17};
		const FBattlingDisc& Disc;
		std::array<FBattlingGearFile, 2> Files;
	};

	struct FCaseRun
	{
		std::string Digest;
		int32 Ticks = 0;
		/** Whether the program left for the field, and the result it leaves there. */
		bool bLeft = false;
		uint8 Result = 0;
	};

	/** The state a case's fight runs in: the menus' demonstration runs in its own. */
	uint8 FightStateOf(const FFightCase& Case)
	{
		return Case.Mode == EBattlingMode::Demonstration ? 5 : 1;
	}

	/** A case's fight and what it reads, kept beside it: the fight holds their places, so this stays where it was made. */
	struct FCaseFight
	{
		std::array<FBattlingGearFile, 2> Files;
		FPsxRandom Random{0};
		FBattlingSoundRecorder Recorder;
		FMesContext Text;
		std::unique_ptr<FBattlingFight> Fight;
	};

	/**
	 * Starts a case's fight in Run as the harness starts the original's: its gears loaded, its settings, the tutorial's
	 * script for a tutorial, its round started and its gears placed; AutoAdvance is the player's auto-advancing text.
	 */
	void StartCase(FCaseFight& Run, const FBattlingDisc& Disc, const FFightCase& Case, EMesAutoAdvance AutoAdvance = EMesAutoAdvance::Off)
	{
		std::array<FMdlModel, 2> Models;
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			auto File = FBattlingGearFile::Read(Disc.Disc->Image(), *Disc.Index, Case.Gears[Side]);
			REQUIRE(File);
			Run.Files[Side] = std::move(*File);
			auto Model = ParseMdlModel(std::span<const uint8>(Run.Files[Side].Bytes).subspan(Run.Files[Side].Model));
			REQUIRE(Model);
			Models[Side] = std::move(*Model);
		}
		// The harness loads the effects' pictures before it sets the sparks up, both from a cleared seed; the case's seed comes after.
		Run.Fight = std::make_unique<FBattlingFight>(Disc.Tables, Disc.Terrain, Run.Random, Run.Recorder);
		FBattlingFight& Fight = *Run.Fight;
		Fight.InitSplashTiles();
		Fight.InitSparkTiles();
		Run.Random = FPsxRandom(Case.Seed);
		Fight.Settings.Rules = Disc.Rules;
		Fight.Settings.Mode = Case.Mode;
		Fight.Settings.Level = Case.Level;
		Fight.Settings.Computer = Case.Computer;
		Fight.Settings.PracticeStyle = Case.Style;
		Fight.Settings.bRubberBand = Case.bRubberBand;
		Fight.Settings.bRangedMovesByOwnState = false;
		Fight.Settings.bPracticeComputerAtStyleLevel = false;
		Fight.Settings.bPracticeFrameRateAsLabelled = false;
		const bool bRounds = Case.WinsPerSet >= 0;
		Fight.Settings.WinsPerSet = static_cast<uint8>(bRounds ? Case.WinsPerSet : 0);
		const bool bTutorial = Case.Mode == EBattlingMode::Tutorial;
		Run.Text.Font = &Disc.Text.Font;
		Run.Text.Tables = &Disc.Text.NameTables;
		Run.Text.ButtonMap = MesKernelButtonMap;
		Run.Text.AutoAdvance = AutoAdvance;
		Fight.Text = &Run.Text;
		Fight.Messages = &Disc.Messages.Table();
		Fight.Settings.bScripted = Case.bScripted;
		if (bTutorial)
		{
			Fight.StartTutorial();
		}
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			Fight.LoadFighter(static_cast<int32>(Side), Case.Gears[Side], Run.Files[Side], Disc.GearTable.Gears[static_cast<std::size_t>(Case.Gears[Side])],
				std::move(Models[Side]));
		}
		if (Case.bFire)
		{
			Fight.LightFire();
			Fight.StartFadeIn();
		}
		Fight.StartRound();
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			Fight.Fighters[Side].Position = {Case.Start[Side][0], 0, Case.Start[Side][1]};
		}
		for (std::size_t Side = 0; Side < 2; ++Side)
		{
			const int32 Hp = Side == 0 ? Case.Hp : Case.Hp2;
			if (Hp >= 0)
			{
				Fight.Fighters[Side].Hp = static_cast<int16>(Fight.Fighters[Side].MaxHp * Hp / 0xFF);
			}
		}
		if (!bTutorial)
		{
			Fight.State = FightStateOf(Case);
		}
	}

	/** Runs a case: its digest over every tick's parts, the ticks run, and how it ended. EachPass sees the bout after each pass, of at most PassLimit. */
	FCaseRun RunCase(const FBattlingDisc& Disc, const FFightCase& Case, const std::function<void(const FBattlingFight&)>& EachPass = {}, int32 PassLimit = -1)
	{
		FCaseFight Run;
		StartCase(Run, Disc, Case);
		const std::unique_ptr<FBattlingFight>& Fight = Run.Fight;
		FBattlingSoundRecorder& Recorder = Run.Recorder;
		const bool bRounds = Case.WinsPerSet >= 0;
		const bool bTutorial = Case.Mode == EBattlingMode::Tutorial;
		const bool bScript = bTutorial || Case.bScripted;
		const uint8 FightState = FightStateOf(Case);

		FXorShift32 Pads(Case.Pads);
		FSha1 Total;
		int32 Ticks = 0;
		int32 WinnerPasses = 0;
		for (int32 Tick = 0; Tick < (PassLimit < 0 ? Case.Ticks : PassLimit); ++Tick)
		{
			if (bRounds || bTutorial ? Fight->State == 3 : Fight->State != FightState)
			{
				break;
			}
			std::array<std::array<FBattlingPad, 2>, 2> Blanks{};
			FBattlingTickInput Input;
			if (Case.Pads != 0 && Case.WinnerPasses >= 0 && Fight->State == 6)
			{
				++WinnerPasses;
				Input.Blanks = DrawWinnerPads(Pads, Fight->PassBlanks, WinnerPasses > Case.WinnerPasses, Blanks);
			}
			else if (Case.Pads != 0)
			{
				// As many vertical blanks as the pass before waited for.
				Input.Blanks = DrawPads(Pads, Fight->PassBlanks, Case.bMenus || bTutorial, Blanks);
			}
			Recorder.Calls.clear();
			Fight->RunPass(Input);
			if (Fight->Fault != EBattlingFault::None || Fight->Tutorial.bLeft || (!Case.bMenus && Fight->State == 0) || (!bRounds && Fight->State == 4))
			{
				break;
			}
			const auto DigestPart = [&](Weltall::Testing::EBattlingFightPart Part) {
				const std::vector<uint8> Bytes = BattlingFightPartBytes(*Fight, Part, Recorder, bRounds, Case.bMenus);
				FSha1 PartHash;
				PartHash.Update(Bytes);
				const FSha1::FDigest Digest = PartHash.Finish();
				Total.Update(Digest);
			};
			for (const Weltall::Testing::EBattlingFightPart Part : BattlingFightParts)
			{
				DigestPart(Part);
			}
			if (bScript)
			{
				DigestPart(Weltall::Testing::EBattlingFightPart::Script);
			}
			if (Case.bFire)
			{
				DigestPart(Weltall::Testing::EBattlingFightPart::Fire);
			}
			if (EachPass)
			{
				EachPass(*Fight);
			}
			++Ticks;
		}
		return {FSha1::ToHex(Total.Finish()), Ticks, Fight->Tutorial.bLeft, Fight->Result};
	}
}

TEST_CASE("Battling's fight matches the original pass for pass", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);

	for (std::size_t Number = 0; Number < Cases.size(); ++Number)
	{
		const FFightCase& Case = Cases[Number];
		const FCaseRun Run = RunCase(Disc, Case);
		INFO("case " << Number << ": " << Run.Digest << " after " << Run.Ticks << " ticks");
		CHECK(Run.Ticks == Case.TicksRun);
		CHECK(Run.Digest == Case.Digest);
		// The scripted bout ends in Weltall's breakdown, which the field reads as 7Fh.
		CHECK(Run.bLeft == Case.bScripted);
		if (Case.bScripted)
		{
			CHECK(Run.Result == 0x7F);
		}
	}
}

// Sixteen of the cases with Perfect Works Build's arena (`battling_fight_digests --perfect-works basic` and `expert`): the
// mod's Battling with each mode's gear table and gear files, its slips played as their code means, against the port's
// re-creation of its rules with the files the mod gives the disc.
namespace
{
	struct FArenaCase
	{
		std::size_t Fight;
		EBattlingRules Rules;
		const char* Digest;
		int32 TicksRun;
	};

	constexpr EBattlingRules Basic = EBattlingRules::PerfectWorksBasic;
	constexpr EBattlingRules Expert = EBattlingRules::PerfectWorksExpert;
	constexpr std::array<FArenaCase, 32> ArenaCases{{
		{0, Basic, "a358ec8e406ff9db6eeb32b733a17a5105b79969", 1200}, {2, Basic, "0562dbd5fef8033c3e36b7092eeb040ae4a9978e", 944},
		{3, Basic, "d76f97922f99b794837f2b3260582e0ab7c8fc91", 1200}, {4, Basic, "fb7e2e2bc014f080ba0a6710812ee16a6a91df89", 758},
		{5, Basic, "f5d97bad7f7969bed639c04a5585f8fcae9c252c", 900}, {6, Basic, "5b24e3b2ef1e74b7cb065d18e010dd1828ba31eb", 900},
		{10, Basic, "53f8d059e82c7ce2e40e894cf636ce791d5014ab", 4000}, {11, Basic, "6079f6151d0901e35e7824efb04831164027955a", 942},
		{12, Basic, "3faf720261c5777ef52bc61feae16cbb506ad4e1", 3000}, {15, Basic, "c8b3467a53575dc50477ab1ce45f4d891f2f1b3b", 820},
		{17, Basic, "26679bb7fc97c9931aec4a34d646795ba02251bc", 1333}, {19, Basic, "3e18319197bf79ea284785414e721737be3b3190", 232},
		{22, Basic, "6d3ecddc4c8c8dd03074ed7650813668634a2d8b", 820}, {26, Basic, "ba23d388bd228549abbd53f25b814c931da21896", 400},
		{28, Basic, "457a5097fb0607462c88800c3d29bb207b58ff11", 466}, {31, Basic, "f51f7340b09d46a3ee98ce438ca0bbb1ce260a83", 1500},
		{0, Expert, "2c5b7b72dc7166aee0c93ed287711564cb293f23", 1200}, {2, Expert, "19df994147587f48f15996fc0981e81aa76a1530", 848},
		{3, Expert, "e03c5906b50d55e079daae927f33368e9010d44b", 1200}, {4, Expert, "c27aa542fc3c9c443e8ca89f07d660813358cc1d", 758},
		{5, Expert, "de56274eeef71d2e1238324edf336ab4e12c0dbb", 900}, {6, Expert, "09a8b16835764dea0c7ed338d862b8c0a9d1dba0", 900},
		{10, Expert, "ac531bae751c1285aedab01d12954288fadf2410", 2802}, {11, Expert, "ba84111b2cb3b246b134e2ac900c643f31a1c924", 942},
		{12, Expert, "85c2fac7d0b196e096911efd8bf6fe655c5f60b2", 3000}, {15, Expert, "837fccc1c76e048afee036fcfe21de81384e1717", 820},
		{17, Expert, "91c8c435c173dec74eda0e3d4e5c86ae06a8d4b9", 1333}, {19, Expert, "3e18319197bf79ea284785414e721737be3b3190", 232},
		{22, Expert, "1109be4f17476e23791cffd2f297812a1b82c149", 820}, {26, Expert, "517ebcd0b82145b044b5a5ced0e39b7a913136a1", 400},
		{28, Expert, "b19e29068946fb18b1d112ddb33d8be90f89a789", 466}, {31, Expert, "989d7b81561de64d3716c6f141b0f2fb97a9ceb8", 1500},
	}};
}

TEST_CASE("With Perfect Works' arena Battling's fight runs as the mod's Battling runs it", "[Content][Oracle][Battling][Mods]")
{
	constexpr std::string_view Version = "0.11.4";
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	const std::optional<FPerfectWorksRelease> Release = FPerfectWorksRelease::Find(std::filesystem::path(WELTALL_GAMES_DIR).parent_path() / "Mods" / "PerfectWorks");
	if (!Release || Release->Version() != Version)
	{
		SKIP("Perfect Works Build 0.11.4 not found in Mods/PerfectWorks");
	}
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	for (const EPerfectWorksArena Arena : {EPerfectWorksArena::Basic, EPerfectWorksArena::Expert})
	{
		FPerfectWorksOptions Options;
		Options.bEnabled = true;
		Options.bScript = false;
		Options.Arena = Arena;
		const std::vector<FModFile> Files = Release->Files(Options, 1);
		const FModFilesGiven Given(*Iso, FDiscModFiles::Make(*Index, 1, Files, Release->Changes(Options, Files)));
		REQUIRE(Iso->Image().ModChanges().PerfectWorksArena == Arena);
		FBattlingDisc Disc;
		ReadDisc(Disc, Iso);
		Disc.Rules = Arena == EPerfectWorksArena::Basic ? Basic : Expert;
		for (const FArenaCase& Case : ArenaCases)
		{
			if (Case.Rules != Disc.Rules)
			{
				continue;
			}
			const FCaseRun Run = RunCase(Disc, Cases[Case.Fight]);
			INFO((Arena == EPerfectWorksArena::Basic ? "basic" : "expert") << " case " << Case.Fight << ": " << Run.Digest << " after " << Run.Ticks << " ticks");
			CHECK(Run.Ticks == Case.TicksRun);
			CHECK(Run.Digest == Case.Digest);
		}
	}
}

// The frames of three of the cases against the original's drawing of the bout run for real
// --models): every packet the pass links for the gears, the ground, the wall, the gears' shadows and the sky, in the order
// the GPU takes them; and of the menus' demonstration against its own drawing, its gears alone.
namespace
{
	struct FDrawCase
	{
		std::size_t Fight;
		int32 Passes;
		const char* Digest;
	};

	constexpr std::array<FDrawCase, 4> DrawCases{{
		{0, 300, "bde958038692e9054bf5944332237dc16e52c7f1"},
		{4, 300, "36c207362a5c503d66b0b340ae09a7f4443dcc11"},
		{10, 600, "25f9b0acc42591c225e878e74016635771ec391f"},
		{31, 300, "bc57ce929fab1135261725dbccbbb11f5193c3e1"},
	}};

	constexpr uint16 MainTableEntry = 0xFFFF;
	constexpr uint8 SkyLayer = 3;

	constexpr uint8 ModelLayer = 4;

	/** The oracle's layer of a scene polygon: 0 the ground, 1 the wall, 2 a shadow, 4 a gear's part. */
	uint8 DrawLayer(const FWorldMapFrame& Scene, const FWorldMapPolygon& Polygon)
	{
		const FWorldMapAnchor& Anchor = Scene.Anchors[Polygon.Anchor];
		if (Anchor.Kind() == EWorldMapAnchorKind::Shadow)
		{
			return 2;
		}
		if (Anchor.Kind() == EWorldMapAnchorKind::Model)
		{
			return ModelLayer;
		}
		return Anchor.Key == FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::World, BattlingAnchors::Wall) ? 1 : 0;
	}

	void AddLittleEndian(std::vector<uint8>& Bytes, uint32 Value, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Bytes.push_back(static_cast<uint8>(Value >> (8 * Index)));
		}
	}

	uint32 PackColour(const FDrawColor& Colour)
	{
		return static_cast<uint32>(Colour.R | Colour.G << 8 | Colour.B << 16);
	}

	bool IsEffect(const FWorldMapFrame& Scene, const FWorldMapPolygon& Polygon)
	{
		return Polygon.Anchor != FWorldMapPolygon::NoAnchor
			&& Scene.Anchors[Polygon.Anchor].Key == FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::World, BattlingAnchors::Effects);
	}

	/** A frame's packets as the oracle digests them: the entry, the layer and the packet's fields. The effects have a case of their own. */
	std::vector<uint8> FramePackets(const FBattlingFrame& Frame)
	{
		std::vector<uint8> Bytes;
		for (const FWorldMapPolygon& Polygon : Frame.Scene.Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Sky || IsEffect(Frame.Scene, Polygon))
			{
				continue;
			}
			AddLittleEndian(Bytes, Polygon.Bucket, 2);
			const uint8 Layer = DrawLayer(Frame.Scene, Polygon);
			Bytes.push_back(Layer);
			const uint32 Code = 0x20 | (Polygon.bGouraud ? 0x10u : 0u) | (Polygon.CornerCount == 4 ? 0x08u : 0u) | (Polygon.bTextured ? 0x04u : 0u)
				| (Polygon.bSemiTransparent ? 2u : 0u) | (Polygon.bRaw ? 1u : 0u);
			AddLittleEndian(Bytes, PackColour(Polygon.Colors[0]) | Code << 24, 4);
			for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
			{
				// A gear's polygon of any kind: a Gouraud one's other corners' colours go with their corners.
				if (Layer == ModelLayer && Polygon.bGouraud && Corner != 0)
				{
					AddLittleEndian(Bytes, PackColour(Polygon.Colors[Corner]), 3);
				}
				AddLittleEndian(Bytes, static_cast<uint16>(Polygon.ScreenX[Corner]), 2);
				AddLittleEndian(Bytes, static_cast<uint16>(Polygon.ScreenY[Corner]), 2);
				if (Polygon.bTextured)
				{
					Bytes.push_back(Polygon.U[Corner]);
					Bytes.push_back(Polygon.V[Corner]);
				}
			}
			if (Polygon.bTextured)
			{
				AddLittleEndian(Bytes, Polygon.Clut, 2);
				AddLittleEndian(Bytes, Polygon.TexturePage, 2);
			}
		}
		// The sky is linked into the main table, after the scene's: the screen's corners its polygon keeps.
		for (const FWorldMapPolygon& Polygon : Frame.Scene.Polygons)
		{
			if (Polygon.Layer != EWorldMapLayer::Sky)
			{
				continue;
			}
			AddLittleEndian(Bytes, MainTableEntry, 2);
			Bytes.push_back(SkyLayer);
			AddLittleEndian(Bytes, PackColour(Polygon.Colors[0]) | 0x38u << 24, 4);
			for (std::size_t Corner = 0; Corner < 4; ++Corner)
			{
				AddLittleEndian(Bytes, static_cast<uint16>(Polygon.ScreenX[Corner]), 2);
				AddLittleEndian(Bytes, static_cast<uint16>(Polygon.ScreenY[Corner]), 2);
			}
			for (std::size_t Corner = 1; Corner < 4; ++Corner)
			{
				AddLittleEndian(Bytes, PackColour(Polygon.Colors[Corner]), 4);
			}
		}
		return Bytes;
	}
}

TEST_CASE("Battling's frames match the original's packets", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);

	for (std::size_t Number = 0; Number < DrawCases.size(); ++Number)
	{
		const FDrawCase& Case = DrawCases[Number];
		FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
		FBattlingFrame Frame;
		FSha1 Total;
		int32 Frames = 0;
		bool bGearsLoaded = false;
		RunCase(Disc, Cases[Case.Fight], [&](const FBattlingFight& Fight) {
			if (!bGearsLoaded)
			{
				// The gears are built under the lights the scene's start leaves, before any frame turns them: the oracle's load.
				FMdlLights Lights;
				Lights.SetDirectional(0, {0x10, 0x10, 0x10}, {0x800, 0x800, 0x800});
				FGte Lighting;
				Lighting.ColorMatrix = Lights.Colors;
				const std::optional<FError> Failed = Drawing.LoadGears(Fight, Lighting);
				INFO((Failed ? Failed->Message : std::string()));
				REQUIRE_FALSE(Failed);
				bGearsLoaded = true;
			}
			std::vector<uint8> Packets;
			if (Fight.bFrameDrawn)
			{
				// The menus' demonstration draws its gears alone.
				if (Fight.LatchedState == 5)
				{
					Drawing.DrawDemonstration(Fight, Frame);
				}
				else
				{
					Drawing.Draw(Fight, Frame);
				}
				Packets = FramePackets(Frame);
			}
			FSha1 FrameHash;
			FrameHash.Update(Packets);
			const FSha1::FDigest Digest = FrameHash.Finish();
			Total.Update(Digest);
			++Frames;
		}, Case.Passes);
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("draw case " << Number << ": " << Digest << " after " << Frames << " frames");
		CHECK(Frames == Case.Passes);
		CHECK(Digest == Case.Digest);
	}
}

// The effects of four of the cases against the original's drawing of the effects and the sparks
// --effects): the sprites, the trails and bands, the dust, the splashes, the lines and the hit sparks, every packet in the
// order the GPU takes them. Computer against computer, the rubber band, two players' sets, the tutorial's lake.
namespace
{
	constexpr std::array<FDrawCase, 4> EffectCases{{
		{1, 1200, "d44e6783906cf1cfa267bd69fb3b88b6768de163"},
		{3, 900, "ec5263d56234f6bffbd1ee325f466cbfffb7e68c"},
		{14, 992, "a73abf750b085dca4e451bc947a8f10007a103a7"},
		{20, 800, "a714cd0c0fa0f4036d1693e9c0d5532d5e58f116"},
	}};

	/** A frame's effects as the oracle digests them: the entry, then a polygon's fields (its colour left out when its texels are drawn raw), a rectangle's words as a TILE's, a line's words. */
	std::vector<uint8> EffectPackets(const FBattlingFrame& Frame)
	{
		std::vector<uint8> Bytes;
		for (const FWorldMapPolygon& Polygon : Frame.Scene.Polygons)
		{
			if (!IsEffect(Frame.Scene, Polygon))
			{
				continue;
			}
			AddLittleEndian(Bytes, Polygon.Bucket, 2);
			const uint32 Colour = PackColour(Polygon.Colors[0]);
			const uint32 Blend = Polygon.bSemiTransparent ? 2u : 0u;
			const auto Place = [&](std::size_t Corner) {
				return static_cast<uint32>(static_cast<uint16>(Polygon.ScreenX[Corner])) | static_cast<uint32>(static_cast<uint16>(Polygon.ScreenY[Corner])) << 16;
			};
			switch (Polygon.Primitive)
			{
			case EWorldMapPrimitive::Rectangle:
				AddLittleEndian(Bytes, Colour | (0x60u | Blend) << 24, 4);
				AddLittleEndian(Bytes, Place(0), 4);
				AddLittleEndian(Bytes, static_cast<uint32>(Polygon.ScreenX[3] - Polygon.ScreenX[0]) | static_cast<uint32>(Polygon.ScreenY[3] - Polygon.ScreenY[0]) << 16, 4);
				break;
			case EWorldMapPrimitive::Line:
				AddLittleEndian(Bytes, Colour | (0x40u | Blend) << 24, 4);
				AddLittleEndian(Bytes, Place(0), 4);
				AddLittleEndian(Bytes, Place(1), 4);
				break;
			case EWorldMapPrimitive::Polygon:
				AddLittleEndian(Bytes, (Polygon.bRaw ? 0u : Colour) | (0x2Cu | Blend | (Polygon.bRaw ? 1u : 0u)) << 24, 4);
				for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
				{
					AddLittleEndian(Bytes, Place(Corner), 4);
					Bytes.push_back(Polygon.U[Corner]);
					Bytes.push_back(Polygon.V[Corner]);
				}
				AddLittleEndian(Bytes, Polygon.Clut, 2);
				AddLittleEndian(Bytes, Polygon.TexturePage, 2);
				break;
			}
		}
		return Bytes;
	}
}

TEST_CASE("Battling's effects match the original's packets", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);

	for (std::size_t Number = 0; Number < EffectCases.size(); ++Number)
	{
		const FDrawCase& Case = EffectCases[Number];
		FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
		Drawing.bGears = false;
		FBattlingFrame Frame;
		FSha1 Total;
		int32 Frames = 0;
		RunCase(Disc, Cases[Case.Fight], [&](const FBattlingFight& Fight) {
			std::vector<uint8> Packets;
			if (Fight.bFrameDrawn)
			{
				Drawing.Draw(Fight, Frame);
				Packets = EffectPackets(Frame);
			}
			FSha1 FrameHash;
			FrameHash.Update(Packets);
			Total.Update(FrameHash.Finish());
			++Frames;
		}, Case.Passes);
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("effects case " << Number << ": " << Digest << " after " << Frames << " frames");
		CHECK(Frames == Case.Passes);
		CHECK(Digest == Case.Digest);
	}
}

// The 2D layer of whole sets against the original's run for real: the
// labels the logic and practice's info print and the HUD, every command linked, in the GPU's order and the menus' oracles'
// canonical form; and in the tutorial, the scripted bout and a bout's fade in through the fire what lies over them: the
// message window, the lesson menu, the pointer, the ending's tint, the fire. The four cases with the pause's menus draw their
// paused passes too: the menu's box, the help lines and the letters over the picture, and the help lines' image.
namespace
{
	/**
	 * The commands in the oracle's form: CanonicalMenuPackets' bytes, but a lone page command (a drawing mode whose window
	 * word is the GPU's NOP) as the one word SetDrawTPage makes.
	 */
	std::vector<uint8> ScreenPackets(const std::vector<FMenuPrimitive>& Primitives)
	{
		std::vector<uint8> Bytes;
		FMenuFrame One;
		for (const FMenuPrimitive& Primitive : Primitives)
		{
			One.Primitives.assign(1, Primitive);
			std::vector<uint8> Packet = Testing::CanonicalMenuPackets(One);
			if (Primitive.IsDrawMode() && Primitive.Words[1] == 0)
			{
				Packet.resize(Packet.size() - 4);
				Packet[0] = static_cast<uint8>(Packet[0] - 4);
			}
			Bytes.insert(Bytes.end(), Packet.begin(), Packet.end());
		}
		return Bytes;
	}

	constexpr std::array<FDrawCase, 11> ScreenCases{{
		{10, 1200, "91acb6290c57ccdf11df874e69dea906b9f87849"},
		{11, 561, "36d60c540cec741961378068e9e85b0a4d1ec684"},
		{6, 900, "11ca2d65b3a0be86d3aa53e7802f30e69c543bd0"},
		{3, 1200, "66de39219c5a96050e7469cea6d02b96322754e2"},
		{20, 1045, "b2944769a74ac0607900664d9d1b695243eac504"},
		{22, 986, "cafdef5db93f65d13eacbb383f39275240417253"},
		{26, 120, "04f9cef9718d20a694929ba58c85574123f558c3"},
		{15, 820, "b1a2f1a6c9bc6ef749ea2b74e48cec789fab1233"},
		{16, 580, "e5635b14c6330250b14bd6052d77df56f3e9635a"},
		{17, 1333, "ca3ae74b01c6946ece2ebf147d10f51bd76ddc97"},
		{18, 609, "3bd0fcfc509847c7416e07c656c6c5cf91914189"},
	}};

	/** The pause's help lines' image, as the drawing uploads it. */
	constexpr FVramRect HelpImagePlace{0x140, 0x30, 0x42, 0x0D};
}

TEST_CASE("Battling's 2D layer matches the original's packets", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);

	for (std::size_t Number = 0; Number < ScreenCases.size(); ++Number)
	{
		const FDrawCase& Case = ScreenCases[Number];
		FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
		FMenuFrame Frame;
		std::vector<FVramImage> Uploads;
		std::vector<uint8> HelpImage;
		FSha1 Total;
		int32 Frames = 0;
		RunCase(Disc, Cases[Case.Fight], [&](const FBattlingFight& Fight) {
			Drawing.TypeWindows(Fight, Uploads);
			for (const FVramImage& Upload : Uploads)
			{
				if (Upload.Rect == HelpImagePlace)
				{
					HelpImage.clear();
					for (const uint16 Unit : Upload.Units)
					{
						AddLittleEndian(HelpImage, Unit, 2);
					}
				}
			}
			Frame.Primitives.clear();
			FSha1 FrameHash;
			if (Fight.bFrameDrawn)
			{
				Drawing.DrawScreen(Fight, Frame.Primitives);
			}
			else if (Fight.bPauseDrawn)
			{
				// A paused pass: the picture's move as a mark, the commands over it, the help lines' image while a line shows.
				if (Drawing.DrawPause(Fight, Frame.Primitives))
				{
					constexpr std::string_view Picture = "picture";
					FrameHash.Update(std::span(reinterpret_cast<const uint8*>(Picture.data()), Picture.size()));
				}
			}
			FrameHash.Update(ScreenPackets(Frame.Primitives));
			if (Fight.bPauseDrawn && (Fight.Menus.HelpShown[0] != 0 || Fight.Menus.HelpShown[1] != 0))
			{
				FrameHash.Update(HelpImage);
			}
			Total.Update(FrameHash.Finish());
			++Frames;
		}, Case.Passes);
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("screen case " << Number << ": " << Digest << " after " << Frames << " frames");
		CHECK(Frames == Case.Passes);
		CHECK(Digest == Case.Digest);
	}
}

// The winner screens of three sets against the original's winner screen drawn for real
// --winner): the gear's shadow and its parts in the order the GPU takes them, then the text's letters. Weltall against the
// computer (the level's line), Argento (environment-mapped whole) and Siebzehn (which does not spin), the text shown and hidden.
namespace
{
	constexpr std::array<FDrawCase, 3> WinnerCases{{
		{28, 303, "b25ccd7a8aab7f5367027d2b05c54e12a0e7d696"},
		{29, 292, "d8c69add380f040379cc36fb02bff2b50d3436f1"},
		{30, 296, "da51e837a36cb04c3f3bb2b4f860ce993687c456"},
	}};

	/** The shadow's scene has an ordering table of its own, which the frame keeps past the gear's 1000h entries. */
	constexpr uint16 ShadowTableStart = 0x1000;

	std::vector<uint8> WinnerPackets(const FBattlingFrame& Frame)
	{
		FBattlingFrame InTables = Frame;
		for (FWorldMapPolygon& Polygon : InTables.Scene.Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Shadow)
			{
				Polygon.Bucket = static_cast<uint16>(Polygon.Bucket - ShadowTableStart);
			}
		}
		std::vector<uint8> Bytes = FramePackets(InTables);
		const std::vector<uint8> Letters = ScreenPackets(Frame.Screen);
		Bytes.insert(Bytes.end(), Letters.begin(), Letters.end());
		return Bytes;
	}
}

TEST_CASE("Battling's winner screen matches the original's packets", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);

	for (std::size_t Number = 0; Number < WinnerCases.size(); ++Number)
	{
		const FDrawCase& Case = WinnerCases[Number];
		FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
		FBattlingFrame Frame;
		FSha1 Total;
		int32 Frames = 0;
		bool bGearsLoaded = false;
		RunCase(Disc, Cases[Case.Fight], [&](const FBattlingFight& Fight) {
			if (!bGearsLoaded)
			{
				FMdlLights Lights;
				Lights.SetDirectional(0, {0x10, 0x10, 0x10}, {0x800, 0x800, 0x800});
				FGte Lighting;
				Lighting.ColorMatrix = Lights.Colors;
				REQUIRE_FALSE(Drawing.LoadGears(Fight, Lighting));
				bGearsLoaded = true;
			}
			// The bout's frames are drawn for the lights they leave; only the winner screen's are compared.
			std::vector<uint8> Packets;
			if (Fight.bFrameDrawn)
			{
				Drawing.Draw(Fight, Frame);
			}
			else if (Fight.bWinnerDrawn)
			{
				Drawing.DrawWinner(Fight, Frame);
				Packets = WinnerPackets(Frame);
			}
			FSha1 FrameHash;
			FrameHash.Update(Packets);
			Total.Update(FrameHash.Finish());
			++Frames;
		}, Case.Passes);
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("winner case " << Number << ": " << Digest << " after " << Frames << " frames");
		CHECK(Frames == Case.Passes);
		CHECK(Digest == Case.Digest);
	}
}

TEST_CASE("Battling's practice computer styles play at the level they name, or at the menus' level as the original", "[Content][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	FPracticeGears Gears(Disc);
	// EASY COMPUTER (style 11) with the menus at HARD: the computer's level as its tick leaves it.
	const auto LevelAfterPasses = [&](bool bFixed) {
		FPsxRandom Random(0x00C0FFEE);
		FBattlingSoundRecorder Recorder;
		auto Fight = Gears.Start(Random, Recorder, [&](FBattlingSettings& Settings) {
			Settings.Level = 2;
			Settings.PracticeStyle = 11;
			Settings.bPracticeComputerAtStyleLevel = bFixed;
		});
		for (int32 Pass = 0; Pass < 300 && Fight->State == 1; ++Pass)
		{
			Fight->RunPass(FBattlingTickInput{});
		}
		return Fight->Ai[1].Level;
	};
	CHECK(LevelAfterPasses(true) == 0);
	CHECK(LevelAfterPasses(false) == 2);
}

// The versus screen of a story bout against the computer against the original's run for real
// as well: the passes while the gears are read, then the load's four, each
// frame's commands as the screen's are digested.
namespace
{
	struct FVersusCase
	{
		std::size_t Fight;
		int32 ReadPasses;
		const char* Digest;
	};

	constexpr int32 VersusLoadPasses = 4;

	constexpr std::array<FVersusCase, 2> VersusCases{{
		{26, 12, "d3377112b52d04a7c571b1606669cc862af47853"},
		{27, 3, "3a1105a7d3ec68cf40abbd07d84d028210107e1d"},
	}};
}

TEST_CASE("Battling's versus screen matches the original's packets", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);
	const FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
	FPsxRandom Random(0);
	FBattlingSoundRecorder Recorder;
	const FBattlingFight Fight(Disc.Tables, Disc.Terrain, Random, Recorder);

	for (std::size_t Number = 0; Number < VersusCases.size(); ++Number)
	{
		const FVersusCase& Case = VersusCases[Number];
		FSha1 Total;
		std::vector<FMenuPrimitive> Primitives;
		for (int32 Pass = 0; Pass < Case.ReadPasses + VersusLoadPasses; ++Pass)
		{
			Primitives.clear();
			Drawing.DrawVersus(Fight, Cases[Case.Fight].Gears, Pass >= Case.ReadPasses, Primitives);
			FSha1 FrameHash;
			FrameHash.Update(ScreenPackets(Primitives));
			Total.Update(FrameHash.Finish());
		}
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("versus case " << Number << ": " << Digest);
		CHECK(Digest == Case.Digest);
	}
}

TEST_CASE("Battling's practice frame rates run as their labels say, or skip whole ticks as the original", "[Content][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	FPracticeGears Gears(Disc);
	// FRAME RATE at its third step: "15 FPS".
	const auto Run = [&](bool bFixed) {
		FPsxRandom Random(0x00C0FFEE);
		FBattlingSoundRecorder Recorder;
		auto Fight = Gears.Start(Random, Recorder, [&](FBattlingSettings& Settings) { Settings.bPracticeFrameRateAsLabelled = bFixed; });
		Fight->Settings.FrameSkip = 2;
		int32 Blanks = 0;
		for (int32 Pass = 0; Pass < 30; ++Pass)
		{
			Fight->RunPass(FBattlingTickInput{});
			Blanks += Fight->PassBlanks;
		}
		return std::pair{static_cast<int32>(Fight->RecordIndex), Blanks};
	};
	// Fixed: a tick each pass of four vertical blanks (15 a second); the original: a tick every third pass of two (10).
	CHECK(Run(true) == std::pair{30, 120});
	CHECK(Run(false) == std::pair{10, 60});
}

// The arena's menus against the original's run for real: the program's start of
// its menus from the story's progress, the settings' word and the gears beaten, then pass after pass of the menus' loop with
// a press on a pad now and then, until a bout is chosen, the demonstration starts or EXIT leaves; after each pass the menus,
// the gear choice, the settings, the fire and the sound calls, then the frame's commands and the help lines' image.
namespace
{
	enum class EMenusEnding : uint8
	{
		Bout,
		Demonstration,
		Exit,
	};

	struct FMenusCase
	{
		uint32 Seed;
		uint32 Pads;
		uint16 Progress;
		uint32 Settings;
		uint8 Defeated;
		std::array<int8, 2> PadTypes;
		/** A pad presses one pass in PressOdds; pad 1 starts holding L2 one pass in HoldOdds. */
		uint32 PressOdds;
		uint32 HoldOdds;
		/** Pad 1 may confirm EXIT; else its confirm there is dropped. */
		bool bExits;
		const char* Digest;
		int32 PassesRun;
		EMenusEnding Ending;
	};

	constexpr std::array<FMenusCase, 10> MenusCases{{
		{0x00001077, 0x6A001357, 400, 0x02051, 0x5A, {4, 1}, 12, 128, false, "7dc2713a89f3911968c845cc8648b512d5c0a84a", 1026, EMenusEnding::Bout},
		{0x00004077, 0x6A004D5C, 400, 0x02001, 0x5A, {4, 4}, 12, 512, false, "c715907f5716be73d5a747231a1b76d950c2ea2c", 284, EMenusEnding::Bout},
		{0x00008077, 0x6A009AB8, 999, 0x04001, 0x5A, {4, 4}, 16, 512, false, "f71df79f1ffca1838133571055d14f6b9198bc95", 647, EMenusEnding::Bout},
		{0x0000B077, 0x6A00D4BD, 999, 0x040D1, 0x5A, {1, 0}, 16, 128, false, "bdf6f40bb5b880fc2d4c4ff92344798127b5af67", 174, EMenusEnding::Demonstration},
		{0x0000D077, 0x6A00FB6B, 400, 0x02051, 0x5A, {4, 1}, 12, 128, false, "36ce32c56f26e56da7ac862431dc16d855f809b0", 852, EMenusEnding::Bout},
		{0x0000E077, 0x6A010EC2, 999, 0x04081, 0x5A, {3, 4}, 16, 512, false, "8969b57f31f05fd670119eae539efa7c264cebdf", 148, EMenusEnding::Bout},
		{0x00014077, 0x6A0182CC, 999, 0x04001, 0x5A, {4, 4}, 16, 512, false, "5dba4d77f6a8c25e56ee3067645b405b1e1f588b", 539, EMenusEnding::Demonstration},
		{0x00015077, 0x6A019623, 200, 0x00051, 0x5A, {4, 1}, 8, 128, false, "1812dcd98e21b10ba8f5b394d8c4a8176d937a10", 102, EMenusEnding::Bout},
		{0x00002222, 0x51DE0002, 200, 0x02051, 0x00, {4, 4}, 8, 512, true, "c1087f4588811f0e21d0b35572f957f47bb946e8", 141, EMenusEnding::Exit},
		{0x00008077, 0x6A009AB8, 999, 0x14001, 0xFF, {4, 4}, 16, 512, false, "c3ac1d13787dc422a215bb4d659a73fcc7832d71", 647, EMenusEnding::Bout},
	}};

	constexpr int32 MenusCasePasses = 3000;
	constexpr std::array<uint16, 7> MenuPresses{0x1000, 0x4000, 0x8000, 0x2000, 0x0020, 0x0040, 0x0001};
	constexpr int16 ExitItem = 4;
	constexpr uint32 SettingsArgento = 0x10000;

	/**
	 * The pass's vertical blank: on each pad a press now and then (but pad 1's confirm on EXIT unless the case exits), pad 1's
	 * L2 held for 96 to 159 passes now and then (pressed on the first), each stick at rest or now and then pushed anywhere.
	 */
	std::array<FBattlingPad, 2> DrawMenuPads(FXorShift32& Random, int32& L2Hold, const FMenusCase& Case, const FBattlingFight& Fight)
	{
		std::array<FBattlingPad, 2> Blank{};
		for (FBattlingPad& Pad : Blank)
		{
			const uint32 Bits = Random.Next();
			const uint16 Button = Bits % Case.PressOdds == 0 ? MenuPresses[(Bits >> 8) % MenuPresses.size()] : uint16{0};
			Pad.Held = Button;
			Pad.Pressed = Button;
		}
		const uint32 Bits = Random.Next();
		if (L2Hold > 0)
		{
			--L2Hold;
			Blank[0].Held |= 1;
		}
		else if (Bits % Case.HoldOdds == 0)
		{
			L2Hold = static_cast<int32>(0x5F + (Bits >> 16) % 0x40);
			Blank[0].Held |= 1;
			Blank[0].Pressed |= 1;
		}
		if (!Case.bExits && Fight.Menus.Current == EBattlingMenu::Top && Fight.Menus.Cursor(EBattlingMenu::Top) == ExitItem)
		{
			Blank[0].Held = static_cast<uint16>(Blank[0].Held & ~0x20);
			Blank[0].Pressed = static_cast<uint16>(Blank[0].Pressed & ~0x20);
		}
		for (std::size_t Port = 0; Port < Blank.size(); ++Port)
		{
			FBattlingPad& Pad = Blank[Port];
			Pad.Repeated = Pad.Pressed;
			Pad.Type = Case.PadTypes[Port];
			const uint32 Stick = Random.Next();
			if ((Stick & 0xF) == 0)
			{
				Pad.LeftX = static_cast<uint8>(Stick >> 8);
				Pad.LeftY = static_cast<uint8>(Stick >> 16);
			}
		}
		return Blank;
	}
}

TEST_CASE("Battling's arena menus match the original pass for pass", "[Content][Oracle][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const auto Executable = ReadBootExecutable(*Iso);
	REQUIRE(Executable);
	const auto Arctangents = FGteArctangents::Read(*Executable);
	REQUIRE(Arctangents);
	const auto Tables = FBattlingArenaTables::Read(Iso->Image(), *Disc.Index, EDiscRelease::UsaDisc1);
	REQUIRE(Tables);
	const auto Textures = FBattlingArenaTextures::Read(Iso->Image(), *Disc.Index);
	REQUIRE(Textures);

	for (std::size_t Number = 0; Number < MenusCases.size(); ++Number)
	{
		const FMenusCase& Case = MenusCases[Number];
		FBattlingDrawing Drawing(*Tables, *Textures, Disc.Terrain, *Arctangents);
		FPsxRandom Random(0);
		FBattlingSoundRecorder Recorder;
		auto Fight = std::make_unique<FBattlingFight>(Disc.Tables, Disc.Terrain, Random, Recorder);
		FMesContext Text;
		Text.Font = &Disc.Text.Font;
		Text.Tables = &Disc.Text.NameTables;
		Text.ButtonMap = MesKernelButtonMap;
		Fight->Text = &Text;
		Fight->Messages = &Disc.Messages.Table();
		// As the program starts its menus: the saved settings, both sides' kinds as the settings start them, the gears beaten
		// (forgotten once Argento is unlocked), the roster.
		const FBattlingSavedSettings Saved = FBattlingSavedSettings::FromWord(Case.Settings);
		Fight->Settings.Vibration = Saved.Vibration;
		Fight->Settings.WinsPerSet = Saved.WinsPerSet;
		Fight->Settings.Level = Saved.Level;
		Fight->Settings.Computer = {true, false};
		Fight->bArgentoUnlocked = (Case.Settings & SettingsArgento) != 0;
		Fight->DefeatedGears.fill(Fight->bArgentoUnlocked ? uint8{0} : Case.Defeated);
		Fight->Menus.Roster = BattlingRoster(Disc.GearTable, Case.Progress);
		if (Fight->bArgentoUnlocked)
		{
			Fight->Menus.ListArgento();
		}
		Fight->StartMenus();
		Random = FPsxRandom(Case.Seed);

		FXorShift32 Pads(Case.Pads);
		int32 L2Hold = 0;
		std::vector<FVramImage> Uploads;
		std::vector<uint8> HelpImage;
		std::vector<FMenuPrimitive> Primitives;
		FSha1 Total;
		int32 Passes = 0;
		EMenusEnding Ending = EMenusEnding::Bout;
		bool bEnded = false;
		for (int32 Pass = 0; Pass < MenusCasePasses && !bEnded; ++Pass)
		{
			const std::array<std::array<FBattlingPad, 2>, 1> Blanks{DrawMenuPads(Pads, L2Hold, Case, *Fight)};
			FBattlingTickInput Input;
			Input.Blanks = Blanks;
			Recorder.Calls.clear();
			Fight->RunMenusPass(Input);
			if (Fight->Menus.bDemonstrationChosen || Fight->Menus.bExited)
			{
				Ending = Fight->Menus.bExited ? EMenusEnding::Exit : EMenusEnding::Demonstration;
				break;
			}
			Drawing.TypeWindows(*Fight, Uploads);
			for (const FVramImage& Upload : Uploads)
			{
				if (Upload.Rect == HelpImagePlace)
				{
					HelpImage.clear();
					for (const uint16 Unit : Upload.Units)
					{
						AddLittleEndian(HelpImage, Unit, 2);
					}
				}
			}
			Primitives.clear();
			Drawing.DrawMenus(*Fight, false, Primitives);
			FSha1 PassHash;
			PassHash.Update(Weltall::Testing::BattlingArenaMenusBytes(*Fight, Recorder));
			PassHash.Update(ScreenPackets(Primitives));
			if (Fight->Menus.HelpShown[0] != 0 || Fight->Menus.HelpShown[1] != 0)
			{
				constexpr std::string_view Help = "help";
				std::vector<uint8> Bytes;
				AddLittleEndian(Bytes, static_cast<uint32>(Help.size() + HelpImage.size()), 2);
				Bytes.insert(Bytes.end(), Help.begin(), Help.end());
				Bytes.insert(Bytes.end(), HelpImage.begin(), HelpImage.end());
				PassHash.Update(Bytes);
			}
			Total.Update(PassHash.Finish());
			++Passes;
			bEnded = Fight->Menus.bBoutChosen;
		}
		const std::string Digest = FSha1::ToHex(Total.Finish());
		INFO("menus case " << Number << ": " << Digest << " after " << Passes << " passes");
		CHECK(Passes == Case.PassesRun);
		CHECK(Ending == Case.Ending);
		CHECK(Digest == Case.Digest);
	}
}

TEST_CASE("Auto-advancing text turns the tutorial's lesson pages and leaves the lesson menu's prompt to the menu", "[Content][Battling]")
{
	REQUIRE_DISC(Iso, EDiscRelease::UsaDisc1);
	FBattlingDisc Disc;
	ReadDisc(Disc, Iso);
	const FFightCase Tutorial{{0, 28}, EBattlingMode::Tutorial, 0, false, 0x00707070, 0, 0, {false, false}, 0, Marks, "", 0};

	// Passes with no button down, each as many vertical blanks as the pass before waited for.
	const auto RunQuietPass = [](FBattlingFight& Fight) {
		std::array<std::array<FBattlingPad, 2>, 2> Blanks{};
		for (std::array<FBattlingPad, 2>& Blank : Blanks)
		{
			for (FBattlingPad& Pad : Blank)
			{
				Pad.Type = 4;
				Pad.LeftX = 0x80;
				Pad.LeftY = 0x80;
			}
		}
		FBattlingTickInput Input;
		Input.Blanks = std::span(Blanks).first(std::min(static_cast<std::size_t>(Fight.PassBlanks), Blanks.size()));
		Fight.RunPass(Input);
		REQUIRE(Fight.Fault == EBattlingFault::None);
	};
	// The tutorial opens on its welcome, a message and a wait for the button, then the lesson menu: the passes until the
	// menu opens (-1 if not within Limit), and the run, there.
	const auto PassesToMenu = [&](FCaseFight& Run, EMesAutoAdvance Pace, int32 Limit) {
		StartCase(Run, Disc, Tutorial, Pace);
		for (int32 Pass = 1; Pass <= Limit; ++Pass)
		{
			RunQuietPass(*Run.Fight);
			if (Run.Fight->Tutorial.bMenuOpen)
			{
				return Pass;
			}
		}
		return -1;
	};

	FCaseFight Waiting;
	CHECK(PassesToMenu(Waiting, EMesAutoAdvance::Off, 3000) == -1);
	FCaseFight Slow;
	const int32 SlowPasses = PassesToMenu(Slow, EMesAutoAdvance::Slow, 3000);
	FCaseFight Fast;
	const int32 FastPasses = PassesToMenu(Fast, EMesAutoAdvance::Fast, 3000);
	INFO("slow " << SlowPasses << ", fast " << FastPasses);
	REQUIRE(FastPasses > 0);
	CHECK(SlowPasses > FastPasses);

	// The prompt waits for the menu's own confirm: the script stays on its wait, with no lesson started.
	FBattlingFight& Menu = *Fast.Fight;
	for (int32 Pass = 0; Pass < 10; ++Pass)
	{
		RunQuietPass(Menu);
	}
	const uint16 PromptWait = Menu.Tutorial.Script;
	for (int32 Pass = 0; Pass < 1200; ++Pass)
	{
		RunQuietPass(Menu);
		REQUIRE(Menu.Tutorial.bMenuOpen);
		REQUIRE(Menu.Tutorial.Script == PromptWait);
	}
}
