#include "Battle/BattleRuntime.h"
#include "Battle/BattleScene.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattlePresentation.h"
#include "Foundation/LittleEndian.h"
#include "Foundation/UnreachedCases.h"
#include "Game/BattleGameProgram.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Input/KernelPad.h"
#include "Support/BattleProgramDisc.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Text/MessageTable.h"
#include "Timing/PsxTiming.h"
#include "WorldMap/WorldMapScene.h"

#include <catch_amalgamated.hpp>

#include <array>
#include <memory>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

// TransXeno's Brazilian Portuguese translation, as its players have it: patched images of the USA discs. Its tool moved the
// battle scenes' name tables' last, unused messages past their files' ends.

namespace
{
	constexpr int32 SceneCount = 76;
	constexpr std::size_t NameTableOffset = 0x30;
	constexpr int32 EnemyNames = 8;
	/** The desert's scene, whose messages from 13 on start past its file. */
	constexpr int32 DesertScene = 5;
	constexpr int32 DesertFirstPastBytes = 13;
	/** Era 1's world map (story 24 to 53), the desert's region, and one of its formations of that scene. */
	constexpr int16 EraOneHeader = 54;
	constexpr std::size_t DesertRegion = 1;
	constexpr uint8 DesertFormation = 1;
	constexpr uint16 QuietMap = 15;
	constexpr int32 FightFrames = 1500;
	/** Alkanshel's scene, and its attack's name, which starts past the scene's file: the disc does not hold it. */
	constexpr uint8 AlkanshelScene = 15;
	constexpr uint8 LostAttackName = 19;
	constexpr int32 FirstFormation = 1;

	/** Alkanshel's scene begun with the game's HUD (in the place of the quiet map's first formation), and its lost attack's name typed. */
	bool TypesLostAttackName(const FIso9660& Disc, FUnreachedCases* Unreached)
	{
		constexpr std::array<int32, 1> Maps{QuietMap};
		FBattleProgramDisc Loaded;
		REQUIRE(LoadBattleProgramDisc(Disc, Maps, Loaded));
		FGameState Game = *Loaded.NewGame;
		Game.SetParty({0, 2, 0xFF});
		FPsxRandom Random(0x1234);
		FKernelPad Pad;
		FGameStateNames Names(Game);
		FMesContext Text;
		Text.Font = &Loaded.Resident.Font;
		Text.Tables = &Loaded.Resident.NameTables;
		Text.State = &Names;
		Text.ButtonMap = MesKernelButtonMap;
		FKernelState Kernel;
		Kernel.InitNewGame();

		FBattleRequest Request;
		const std::vector<uint8>& Encounters = Loaded.Encounters.at(QuietMap);
		std::copy_n(Encounters.begin() + FirstFormation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
			Request.Formation.begin());
		Request.Formation[0] = AlkanshelScene;
		FBattleSoundRecorder Sound;
		FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend}, nullptr, Unreached);
		const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
		FBattleRuntimeServices Services;
		Services.Image = &Disc.Image();
		Services.Index = &*Loaded.Index;
		Services.Game = &Game;
		Services.Random = &Random;
		Services.Pad = &Pad;
		Services.Show = Presentation.get();
		Services.Tables = &*Loaded.Tables;
		Services.Program = &*Loaded.Program;
		Services.Growth = &*Loaded.Growth;
		Services.Events = &*Loaded.Events;
		Services.Arctangents = &*Loaded.Arctangents;
		auto Runtime = FBattleRuntime::Begin(Request, Services);
		REQUIRE(Runtime);
		for (int32 Frame = 0; Frame < 300 && !Presentation->HasStarted(); ++Frame)
		{
			(*Runtime)->Frame();
			REQUIRE_FALSE(Hud.HasStopped());
		}
		REQUIRE(Presentation->HasStarted());
		Hud.TypeMessage(0, LostAttackName);
		return Hud.HasStopped();
	}

	/** A scene's name table: its enemies' names, then what their scripts say. */
	FMessageTable SceneNames(const FBattleScene& Scene)
	{
		const std::span<const uint8> File = Scene.Bytes();
		REQUIRE(File.size() >= NameTableOffset + 2);
		const TResult<FMessageTable> Table = FMessageTable::Parse(File.subspan(LittleEndian::ReadU16(File, NameTableOffset)));
		REQUIRE(Table);
		return *Table;
	}

	void CheckSceneNames(const FPlayedDisc& Disc)
	{
		REQUIRE(Disc.Iso != nullptr);
		int32 PastBytes = 0;
		for (int32 Number = 0; Number < SceneCount; ++Number)
		{
			INFO("scene " << Number);
			const auto Scene = FBattleScene::Load(Disc.Iso->Image(), *Disc.Index, Number);
			REQUIRE(Scene);
			const FMessageTable Names = SceneNames(*Scene);
			for (int32 Name = 1; Name <= EnemyNames; ++Name)
			{
				CHECK_FALSE(Names.String(Name).empty());
			}
			PastBytes += Names.StartsPastBytes(Names.Count() - 1) ? 1 : 0;
			if (Number == DesertScene)
			{
				CHECK_FALSE(Names.StartsPastBytes(DesertFirstPastBytes - 1));
				CHECK(Names.StartsPastBytes(DesertFirstPastBytes));
			}
		}
		CHECK(PastBytes == 17);
	}
}

TEST_CASE("The Brazilian translation's battle scenes keep their enemies' names, though their last messages start past their files", "[Battle][Content]")
{
	REQUIRE_BRAZILIAN_DISC(First, EDiscRelease::UsaDisc1);
	REQUIRE_BRAZILIAN_DISC(Second, EDiscRelease::UsaDisc2);
	const auto Opened = FPlayedGame::Open(First, Second);
	REQUIRE(Opened);
	CheckSceneNames(Opened->Discs[0]);
	CheckSceneNames(Opened->Discs[1]);
}

TEST_CASE("The Brazilian translation's desert battle plays on through its enemies' turns", "[Battle][Content]")
{
	REQUIRE_BRAZILIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Opened = FPlayedGame::Open(Disc, FindBrazilianDisc(EDiscRelease::UsaDisc2));
	REQUIRE(Opened);
	auto Started = Opened->Start(0, nullptr);
	REQUIRE(Started);
	FGameSession& Session = **Started;
	FGameState& State = Session.Game();
	// Fei and Citan as a new game makes them: the enemies take their turns before the battle ends.
	State.SetParty({0, 2, 0xFF});
	auto Looped = FGameLoop::EnterField(Session, QuietMap);
	REQUIRE(Looped);
	FGameLoop& Loop = **Looped;
	const auto Frame = [&](FPadState Held) {
		const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop.VblanksPerFrame()), Held);
		Opened->Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
		return Loop.Frame(PerVblank);
	};
	for (int32 Waited = 0; Waited < 100 && !(Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning); ++Waited)
	{
		REQUIRE_FALSE(Frame({}));
	}
	REQUIRE(Loop.Field() != nullptr);

	// The world map's formations of the desert, and its battle asked for as a script asks for one.
	const auto Scene = ReadWorldMapScene(Disc->Image(), *Opened->Discs[0].Index, FWorldMapSceneEntry{EraOneHeader, 16, 16, 2});
	REQUIRE(Scene);
	REQUIRE(Scene->Regions.size() > DesertRegion);
	auto Out = Session.Kernel().Formations.begin();
	for (const auto& Record : Scene->Regions[DesertRegion].Formations)
	{
		Out = std::copy(Record.begin(), Record.end(), Out);
	}
	FEventContext& Context = Loop.Field()->Runtime().Context();
	Context.bBattleRequested = true;
	Context.Requests.bScriptedBattle = true;
	Context.Requests.bLeavingField = true;
	Context.Requests.BattleKind = 0;
	Context.Encounters.Formation = DesertFormation;
	for (int32 Waited = 0; Waited < 8 && Loop.Program() != EGameProgram::Battle; ++Waited)
	{
		REQUIRE_FALSE(Frame({}));
	}
	REQUIRE(Loop.Program() == EGameProgram::Battle);
	REQUIRE(Loop.Battle()->Runtime().Context().Formation.Scene == DesertScene);

	// Each enemy's turn shows its name from the scene's table; the battle went on to its end or past the frames.
	for (int32 Fought = 0; Fought < FightFrames && Loop.Program() == EGameProgram::Battle; ++Fought)
	{
		FPadState Held;
		Held.Buttons = Fought % 24 < 2 ? FPadState::Bit(EPadButton::Cross) : 0;
		const auto Error = Frame(Held);
		INFO("frame " << Fought << ": " << (Error ? Error->Message : std::string()));
		REQUIRE_FALSE(Error);
	}
}

TEST_CASE("The Brazilian translation's lost attack name is typed as none where play carries past it, and stops the battle elsewhere", "[Battle][Content]")
{
	REQUIRE_BRAZILIAN_DISC(Disc, EDiscRelease::UsaDisc1);
	CHECK(TypesLostAttackName(*Disc, nullptr));
	FUnreachedCases Stopping;
	Stopping.bStops = true;
	CHECK(TypesLostAttackName(*Disc, &Stopping));
	CHECK(Stopping.TakeNoted().empty());
	FUnreachedCases CarryingOn;
	CHECK_FALSE(TypesLostAttackName(*Disc, &CarryingOn));
	CHECK(CarryingOn.TakeNoted().size() == 1);
}
