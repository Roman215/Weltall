#include "Field/FieldGearStandIns.h"
#include "Field/IdsFieldSprite.h"
#include "Game/CharacterLooks.h"
#include "Game/PartyOrder.h"
#include "Game/WorldMapProgram.h"
#include "Graphics/Texture.h"
#include "Graphics/TextureRegion.h"
#include "Menu/MenuGameMenus.h"
#include "Sound/WorldMapMusic.h"
#include "Support/GameDiscs.h"
#include "Support/GameLoopSession.h"
#include "WorldMap/WorldMapContext.h"
#include "WorldMap/WorldMapFrameBlend.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace Weltall;

// The world map program over the game's session, as the kernel would run it after the field: its fades,
// its ticks, and what it leaves the kernel for a battle and takes back after it.

namespace
{
	constexpr FPadState NoButtons{};
	constexpr FPadState DownHeld{FPadState::Bit(EPadButton::Down)};
	constexpr FPadState SquarePressed{FPadState::Bit(EPadButton::Square)};
	constexpr FPadState SoftResetHeld{static_cast<uint16>(FPadState::Bit(EPadButton::L1) | FPadState::Bit(EPadButton::R1) |
														  FPadState::Bit(EPadButton::Start) | FPadState::Bit(EPadButton::Select))};
	/** Era 1 (the story's value 30), at the scene's first entrance, facing north-west. */
	constexpr uint16 Story = 30;
	constexpr uint16 Place = 0x401;
	constexpr uint16 Entrance = 1;
	constexpr uint16 CameraYaw = 0xC00;
	constexpr uint16 Scene = 1;
	/** The ticks of the scene's own fade-in, which keep the menu and the battles off. */
	constexpr int32 FadeInTicks = 62;

	bool Mentions(const std::optional<FError>& Error, const std::string& Text)
	{
		return Error && Error->Message.find(Text) != std::string::npos;
	}

	void EnterWorldMap(FGameSession& Session)
	{
		FGameState& Game = Session.Game();
		Game.SetSavedEventVariable(0, Story);
		Game.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
	}

	/** One frame of the program with Held down through its vertical blanks (pressed at the first only when bPressedOnce). */
	std::optional<FError> Frame(FWorldMapProgram& Program, FPadState Held, bool bPressedOnce = false)
	{
		std::vector<FPadState> Vblanks(static_cast<std::size_t>(Program.VblanksPerFrame()), bPressedOnce ? NoButtons : Held);
		Vblanks[0] = Held;
		return Program.Frame(Vblanks);
	}

	void RunEntryFade(FWorldMapProgram& Program)
	{
		for (int32 Index = 0; Index < FWorldMapProgram::EntryFadeFrames; ++Index)
		{
			REQUIRE(Program.VblanksPerFrame() == 1);
			REQUIRE(Program.IsFadingOut());
			REQUIRE_FALSE(Frame(Program, NoButtons));
		}
		REQUIRE(Program.VblanksPerFrame() == 2);
	}
}

TEST_CASE("The world map program fades in and ticks, and the pad restarts it", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	const uint32 Before = Session.Game().PlayTime();
	auto Started = FWorldMapProgram::Start(Session);
	INFO((Started ? std::string() : Started.Error().Message));
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < 100; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	CHECK(Program.State() == EGameProgramState::Running);
	CHECK(Session.Game().PlayTime() == Before + FWorldMapProgram::EntryFadeFrames + 200);
	CHECK(Program.Runtime().Context().Era == 1);
	REQUIRE_FALSE(Frame(Program, SoftResetHeld));
	CHECK(Program.State() == EGameProgramState::SoftReset);
}

TEST_CASE("The world map program's menu fades out and stops where the menu is not run", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	// The press is read on the next tick, whose checks open the menu.
	REQUIRE_FALSE(Frame(Program, SquarePressed, true));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	CHECK(Session.Game().Block()[0x1836] == 1);
	std::optional<FError> Stopped;
	for (int32 Index = 0; Index < FWorldMapProgram::MenuFadeOutFrames && !Stopped; ++Index)
	{
		REQUIRE(Program.VblanksPerFrame() == 1);
		Stopped = Frame(Program, NoButtons);
	}
	INFO((Stopped ? Stopped->Message : std::string()));
	CHECK(Mentions(Stopped, "(mode 0, parameter 1, from the world map) is not run yet"));
	CHECK(Session.Kernel().MenuMode == 0);
	CHECK(Session.Kernel().Field.MenuParam == 1);
}

TEST_CASE("The world map program leaves for a battle and comes back from what it kept", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	// The period's encounter falls on the next tick the party moves.
	FWorldMapEncounters& Encounters = Program.Runtime().Context().Encounters;
	Encounters.Timers[0] = 1;
	Encounters.Left = FWorldMapEncounters::FootPeriod;
	for (int32 Tick = 0; Tick < 8 && Program.State() == EGameProgramState::Running; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, DownHeld));
	}
	REQUIRE(Program.State() == EGameProgramState::Ended);
	const FKernelState& Kernel = Session.Kernel();
	CHECK(Kernel.NextProgram == EGameProgram::Battle);
	CHECK(Kernel.BattleKind == 0);
	CHECK_FALSE(Kernel.bBattleDebug);
	REQUIRE(Kernel.WorldMapKeep);
	CHECK((Session.Game().Scene() & 0x8000) != 0);
	const FWorldMapRequest& Request = Program.Runtime().Request();
	const FWorldMapRegion& Region = Program.Runtime().Scene().Regions[static_cast<std::size_t>(Request.Region)];
	CHECK(Kernel.Field.BattleFormation == Request.Formation);
	CHECK(std::equal(Region.Formations[0].begin(), Region.Formations[0].end(), Kernel.Formations.begin()));
	const FWorldMapVector Leader = Program.Runtime().Context().Tasks[FWorldMapContext::LeaderSlot].Position;

	auto Returned = FWorldMapProgram::Start(Session);
	INFO((Returned ? std::string() : Returned.Error().Message));
	REQUIRE(Returned);
	const FWorldMapContext& After = (*Returned)->Runtime().Context();
	CHECK(After.Tasks[FWorldMapContext::LeaderSlot].Position.X == Leader.X);
	CHECK(After.Tasks[FWorldMapContext::LeaderSlot].Position.Z == Leader.Z);
	CHECK(After.ReturnWord == 2);
	RunEntryFade(**Returned);
	REQUIRE_FALSE(Frame(**Returned, NoButtons));
	CHECK((*Returned)->State() == EGameProgramState::Running);
}

TEST_CASE("With no controller the world map waits in the pause's frames, and the wait is no play time", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	INFO((Started ? std::string() : Started.Error().Message));
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 10; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}

	// The pass finds no controller: its pause's frames follow, a vertical blank each, the party standing still.
	Session.Pad().SetConnected(false);
	REQUIRE_FALSE(Frame(Program, DownHeld));
	const uint32 PlayTime = Session.Game().PlayTime();
	const FWorldMapVector Where = Program.Runtime().Context().Controlled;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		CHECK(Program.VblanksPerFrame() == 1);
		REQUIRE_FALSE(Frame(Program, DownHeld));
	}
	CHECK(Program.Runtime().Context().Controlled.X == Where.X);
	CHECK(Program.Runtime().Context().Controlled.Z == Where.Z);

	// Back, the wait's last frame sets the play time back, and the ticks go on.
	Session.Pad().SetConnected(true);
	REQUIRE_FALSE(Frame(Program, NoButtons));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	CHECK(Program.VblanksPerFrame() == 2);
	CHECK(Session.Game().PlayTime() == PlayTime);
}

TEST_CASE("The world map program keeps the tick before for the display, and its world blends as the trees stand", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	CHECK(Program.PreviousFrame() == nullptr);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}

	// Walking south, each tick's frame follows the one before; a tree, which stands where it is, moves on the screen
	// between them exactly as the world does.
	std::vector<int32> Matches;
	std::vector<std::pair<uint32, int32>> Scratch;
	FWorldMapFrameBlend Blend;
	int32 TreesChecked = 0;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, DownHeld));
		const FWorldMapFrame* Latest = Program.LatestFrame();
		const FWorldMapFrame* Previous = Program.PreviousFrame();
		REQUIRE(Latest != nullptr);
		REQUIRE(Previous != nullptr);
		CHECK(Latest->Tick == Previous->Tick + 1);
		std::vector<uint32> Keys;
		for (const FWorldMapAnchor& Anchor : Latest->Anchors)
		{
			Keys.push_back(Anchor.Key);
		}
		std::sort(Keys.begin(), Keys.end());
		CHECK(std::adjacent_find(Keys.begin(), Keys.end()) == Keys.end());
		MatchWorldMapAnchors(*Previous, *Latest, Matches, Scratch);
		BlendWorldMapFrames(*Latest, Previous, Matches, 0.0f, Blend);
		const auto World = std::find_if(Latest->Anchors.begin(), Latest->Anchors.end(),
			[](const FWorldMapAnchor& Anchor) { return Anchor.Kind() == EWorldMapAnchorKind::World; });
		REQUIRE(World != Latest->Anchors.end());
		const FWorldMapViewMove& Move = Blend.Moves[static_cast<std::size_t>(World - Latest->Anchors.begin())];
		for (std::size_t Index = 0; Index < Latest->Anchors.size(); ++Index)
		{
			if (Latest->Anchors[Index].Kind() != EWorldMapAnchorKind::Billboard || Matches[Index] < 0)
			{
				continue;
			}
			const std::array<int32, 3>& Now = Latest->Anchors[Index].Matrix.T;
			const std::array<int32, 3>& Then = Previous->Anchors[static_cast<std::size_t>(Matches[Index])].Matrix.T;
			for (std::size_t Row = 0; Row < 3; ++Row)
			{
				const float Moved = Move[Row][0] * static_cast<float>(Now[0]) + Move[Row][1] * static_cast<float>(Now[1]) + Move[Row][2] * static_cast<float>(Now[2]) + Move[Row][3];
				CHECK(std::fabs(Moved - static_cast<float>(Then[Row])) < 2.0f);
			}
			++TreesChecked;
		}
	}
	CHECK(TreesChecked > 0);
	CHECK(Program.Runtime().Context().Controlled.Z != 0);

	// The pause's frames draw nothing: the display holds the latest, and the tick after it follows it again.
	Session.Pad().SetConnected(false);
	REQUIRE_FALSE(Frame(Program, NoButtons));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	CHECK(Program.VblanksPerFrame() == 1);
	CHECK(Program.PreviousFrame() == nullptr);
	Session.Pad().SetConnected(true);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	REQUIRE(Program.PreviousFrame() != nullptr);
	CHECK(Program.LatestFrame()->Tick == Program.PreviousFrame()->Tick + 1);
}

TEST_CASE("The world map's own fades darken their picture a step a frame, the last frames showing the step before the last", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	using EPicture = FWorldMapScreenFade::EPicture;
	const auto Levels = [&Program](int32 Frames, EPicture Picture)
	{
		std::vector<int32> Shown;
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			REQUIRE_FALSE(Frame(Program, NoButtons));
			CHECK(Program.ScreenFade().Picture == Picture);
			Shown.push_back(Program.ScreenFade().Level);
		}
		return Shown;
	};

	// The screen before, 64 steps of 4 subtracted; the map's own fade then goes on from 248.
	const std::vector<int32> Entry = Levels(FWorldMapProgram::EntryFadeFrames, EPicture::ScreenBefore);
	CHECK(Entry[0] == -1);
	CHECK(Entry[1] == 0);
	CHECK(Entry[2] == 4);
	CHECK(Entry[64] == 252);
	CHECK(Entry[65] == 248);
	REQUIRE_FALSE(Frame(Program, NoButtons));
	CHECK(Program.ScreenFade().Picture == EPicture::None);

	// Before the menu: the map's picture, 16 steps of 8.
	for (int32 Tick = 0; Tick < FadeInTicks + 1; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	REQUIRE_FALSE(Frame(Program, SquarePressed, true));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	std::vector<int32> Out;
	for (int32 Index = 0; Index < FWorldMapProgram::MenuFadeOutFrames - 1; ++Index)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
		CHECK(Program.ScreenFade().Picture == EPicture::LatestFrame);
		Out.push_back(Program.ScreenFade().Level);
	}
	CHECK(Out == std::vector<int32>{-1, 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120});
}

TEST_CASE("After its menu the world map draws the party in the order its player keeps, its tasks where they were", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	REQUIRE_FALSE(Frame(Program, SquarePressed, true));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	REQUIRE(Session.Game().Block()[0x1836] == 1);
	const FWorldMapContext& Context = Program.Runtime().Context();
	constexpr std::size_t Leader = FWorldMapContext::LeaderSlot;
	constexpr std::size_t Follower = Leader + 1;
	REQUIRE(Context.MemberFiles[0]);
	REQUIRE(Context.MemberFiles[1]);
	const uint32 FeisFile = Context.MemberFiles[0]->Size();
	const uint32 CitansFile = Context.MemberFiles[1]->Size();
	REQUIRE(FeisFile != CitansFile);
	const FWorldMapVector LeaderAt = Context.Tasks[Leader].Position;
	const FWorldMapVector FollowerAt = Context.Tasks[Follower].Position;

	// Citan placed first while the menu runs: the party's slots show him, then Fei.
	FPartyOrder Order;
	const std::array<uint8, 2> CitanFirst{2, 0};
	Order.Arrange(Session.Game().Party(), CitanFirst);
	Session.Game().SetPartyOrder(Order);
	REQUIRE_FALSE(Program.Runtime().ResumeAfterMenu());
	CHECK(Context.MemberFiles[0]->Size() == CitansFile);
	CHECK(Context.MemberFiles[1]->Size() == FeisFile);
	REQUIRE(Context.SpriteSlots[Leader]);
	REQUIRE(Context.SpriteSlots[Follower]);
	CHECK(Context.SpriteSlots[Leader]->PrimaryFile == &*Context.MemberFiles[0]);
	CHECK(Context.SpriteSlots[Follower]->PrimaryFile == &*Context.MemberFiles[1]);
	CHECK(Context.Tasks[Leader].Position.X == LeaderAt.X);
	CHECK(Context.Tasks[Follower].Position.Z == FollowerAt.Z);
	CHECK(Session.Game().Party()[0] == 0);
}

TEST_CASE("Back from a battle one member boarded in, the world map shows that member in the gear and the others on foot", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	const FWorldMapContext& Before = Program.Runtime().Context();
	REQUIRE(Before.MemberFiles[0]);
	REQUIRE(Before.MemberFiles[1]);
	const uint32 FeisFile = Before.MemberFiles[0]->Size();
	const uint32 CitansFile = Before.MemberFiles[1]->Size();
	REQUIRE(FeisFile != CitansFile);

	// Citan placed before Fei, then a battle on the way.
	FPartyOrder Order;
	const std::array<uint8, 2> CitanFirst{2, 0};
	Order.Arrange(Session.Game().Party(), CitanFirst);
	Session.Game().SetPartyOrder(Order);
	FWorldMapEncounters& Encounters = Program.Runtime().Context().Encounters;
	Encounters.Timers[0] = 1;
	Encounters.Left = FWorldMapEncounters::FootPeriod;
	for (int32 Tick = 0; Tick < 8 && Program.State() == EGameProgramState::Running; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, DownHeld));
	}
	REQUIRE(Program.State() == EGameProgramState::Ended);
	REQUIRE(Session.Kernel().NextProgram == EGameProgram::Battle);

	// In it Fei calls his gear: the battle marks his own slot of the party as riding, the first, whatever the order.
	constexpr std::size_t RidingBytes = 0x22B1;
	Session.Game().Block()[RidingBytes] = 1;
	auto Returned = FWorldMapProgram::Start(Session);
	INFO((Returned ? std::string() : Returned.Error().Message));
	REQUIRE(Returned);
	RunEntryFade(**Returned);
	REQUIRE_FALSE(Frame(**Returned, NoButtons));
	const FWorldMapContext& After = (*Returned)->Runtime().Context();
	// The slot that rides is Fei's and shows Fei, in his own gear; Citan, who leads by the order, walks in the other.
	REQUIRE(After.MemberFiles[0]);
	REQUIRE(After.MemberFiles[1]);
	CHECK(After.MemberFiles[0]->Size() == FeisFile);
	CHECK(After.MemberFiles[1]->Size() == CitansFile);
	CHECK(After.GearFiles[0].has_value());

	// Once nobody rides, the party is shown in its player's order again by the next tick.
	Session.Game().Block()[RidingBytes] = 0;
	REQUIRE_FALSE(Frame(**Returned, NoButtons));
	CHECK(After.MemberFiles[0]->Size() == CitansFile);
	CHECK(After.MemberFiles[1]->Size() == FeisFile);
}

TEST_CASE("The world map goes on after its menu, its song with it, whatever result the program before left", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FMenuGameMenus Menus(*Game->Executable);
	Session.SetMenus(&Menus);
	EnterWorldMap(Session);
	auto Entered = FGameLoop::EnterWorldMap(Session);
	REQUIRE(Entered);
	Game->Loop = std::move(*Entered);
	const FWorldMapMusic& Music = Session.Sound().WorldMapMusic();
	const auto SongAt = [&] {
		const auto At = Music.SongPlace();
		return At ? At->first * 100 + At->second : -1;
	};
	for (int32 Index = 0; Index < 500; ++Index)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	const int32 Before = SongAt();
	REQUIRE(Before > 0);
	// A battle run from leaves the number a game loaded in the menu does.
	Session.Game().ProgramResult = 2;
	const FWorldMapProgram* Running = std::as_const(*Game->Loop).WorldMap();
	REQUIRE_FALSE(Game->Frame(SquarePressed));
	Game->RunUntil([&] { return Menus.Running() != nullptr; }, 120);
	REQUIRE(Menus.Running() != nullptr);
	for (int32 Index = 0; Index < 180; ++Index)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	const int32 InMenu = SongAt();
	CHECK(InMenu > Before);
	int32 Taps = 0;
	while (Menus.Running() != nullptr && Taps < 60)
	{
		REQUIRE_FALSE(Game->Frame(FPadState{FPadState::Bit(EPadButton::Circle)}));
		for (int32 Index = 0; Index < 8 && Menus.Running() != nullptr; ++Index)
		{
			REQUIRE_FALSE(Game->Frame());
		}
		++Taps;
	}
	REQUIRE(Menus.Running() == nullptr);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		REQUIRE_FALSE(Game->Frame());
	}
	CHECK(std::as_const(*Game->Loop).WorldMap() == Running);
	CHECK(SongAt() > InMenu);
	CHECK(Session.Game().ProgramResult == 2);
	Game->Loop.reset();
}

TEST_CASE("The world map draws the party's gears with their models where the player asks, and plays on the same", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FirstGearSlot = FWorldMapContext::LeaderSlot + 3;
	constexpr std::array<uint8, 3> Members{0, 2, 3};
	/** What a walk in gears leaves of the game, and what its frames drew of the party's gears, standing and going. */
	struct FWalk
	{
		std::array<std::array<int32, 3>, 6> Places{};
		uint32 Random = 0;
		int32 GearPolygons = 0;
		int32 GearSprites = 0;
		bool bFineInPortRows = true;
		/** The textured polygons of the gears' models whose texels lie inside a region of the frame's seeds. */
		int32 TexturedGearPolygons = 0;
		int32 SeededGearPolygons = 0;
		std::array<int32, 3> ModelsOf{-1, -1, -1};
		/**
		 * Each gear as it stands, along the world's y (which grows downward) from where the camera is: its top and its
		 * bottom, and with sprites where its sprite's origin lies. A sprite's parts face the screen: its height is theirs.
		 */
		std::array<std::array<float, 2>, 3> Standing{};
		std::array<float, 3> Origins{};
		std::array<float, 3> SpriteHeights{};
	};
	const auto Walk = [&](EFieldGearModels Models) {
		FWalk Result;
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FGameSession& Session = *Game->Session;
		FPlayerSettings Settings = Session.Settings();
		Settings.Field.GearModels = Models;
		Session.SetSettings(Settings);
		FGameState& State = Session.Game();
		State.SetSavedEventVariable(0, 120);
		State.SetParty(Members);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			REQUIRE(State.SetCharacterGear(Members[static_cast<std::size_t>(Slot)], Members[static_cast<std::size_t>(Slot)]));
			State.SetPartySlotInGear(Slot, 1);
		}
		State.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
		auto Started = FWorldMapProgram::Start(Session);
		REQUIRE(Started);
		FWorldMapProgram& Program = **Started;
		RunEntryFade(Program);
		for (int32 Tick = 0; Tick < FadeInTicks + 8; ++Tick)
		{
			REQUIRE_FALSE(Frame(Program, NoButtons));
		}
		const FFieldGearStandIns* StandIns = Program.Drawing().GearStandIns();
		REQUIRE((StandIns != nullptr) == (Models != EFieldGearModels::Off));
		// Of a gear's task: its sprite's parts, or its model's.
		const auto IsOf = [&](const FWorldMapAnchor& Anchor, int32 Slot) {
			if (Anchor.Kind() == EWorldMapAnchorKind::Sprite)
			{
				return Anchor.Key == FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, static_cast<uint32>(Slot));
			}
			return Anchor.Kind() == EWorldMapAnchorKind::Gear && StandIns != nullptr && StandIns->LayoutSlotOf(Slot)
				&& (Anchor.Key & 0xFFFFFF) >> 8 == static_cast<uint32>(*StandIns->LayoutSlotOf(Slot));
		};
		const FWorldMapFrame* Stood = Program.LatestFrame();
		REQUIRE(Stood != nullptr);
		// A point of view space along the world's y: the camera's matrix turns the world, and its second column turns back.
		const auto WorldY = [Stood](const std::array<float, 3>& View) {
			const std::array<std::array<int16, 3>, 3>& M = Stood->Camera.M;
			return (M[0][1] * View[0] + M[1][1] * View[1] + M[2][1] * View[2]) / static_cast<float>(FGteMatrix::One);
		};
		for (int32 Gear = 0; Gear < 3; ++Gear)
		{
			float Top = 1.0e9f;
			float Bottom = -1.0e9f;
			float ScreenTop = 1.0e9f;
			float ScreenBottom = -1.0e9f;
			for (const FWorldMapPolygon& Polygon : Stood->Polygons)
			{
				if (Polygon.Anchor == FWorldMapPolygon::NoAnchor || !IsOf(Stood->Anchors[Polygon.Anchor], FirstGearSlot + Gear))
				{
					continue;
				}
				const FWorldMapAnchor& Anchor = Stood->Anchors[Polygon.Anchor];
				if (Anchor.Kind() == EWorldMapAnchorKind::Sprite)
				{
					Result.Origins[static_cast<std::size_t>(Gear)] = WorldY({static_cast<float>(Anchor.Matrix.T[0]), static_cast<float>(Anchor.Matrix.T[1]),
						static_cast<float>(Anchor.Matrix.T[2])});
				}
				for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
				{
					Top = std::min(Top, WorldY(Polygon.View[Corner]));
					Bottom = std::max(Bottom, WorldY(Polygon.View[Corner]));
					ScreenTop = std::min(ScreenTop, Polygon.View[Corner][1]);
					ScreenBottom = std::max(ScreenBottom, Polygon.View[Corner][1]);
				}
			}
			Result.Standing[static_cast<std::size_t>(Gear)] = {Top, Bottom};
			Result.SpriteHeights[static_cast<std::size_t>(Gear)] = ScreenBottom - ScreenTop;
		}
		for (int32 Tick = 0; Tick < 40; ++Tick)
		{
			REQUIRE_FALSE(Frame(Program, FPadState{FPadState::Bit(EPadButton::Up)}));
		}
		const FWorldMapContext& Context = Program.Runtime().Context();
		for (std::size_t Task = 0; Task < Result.Places.size(); ++Task)
		{
			const FWorldMapVector& At = Context.Tasks[static_cast<std::size_t>(FWorldMapContext::LeaderSlot) + Task].Position;
			Result.Places[Task] = {At.X, At.Y, At.Z};
		}
		Result.Random = Session.Random().Seed();
		const FWorldMapFrame* Going = Program.LatestFrame();
		for (const FWorldMapPolygon& Polygon : Going->Polygons)
		{
			const FWorldMapAnchor* Anchor = Polygon.Anchor != FWorldMapPolygon::NoAnchor ? &Going->Anchors[Polygon.Anchor] : nullptr;
			if (Polygon.Layer == EWorldMapLayer::Gear)
			{
				++Result.GearPolygons;
				Result.bFineInPortRows = Result.bFineInPortRows && Anchor != nullptr && Anchor->Kind() == EWorldMapAnchorKind::Gear && Anchor->bFine
					&& (!Polygon.bTextured || Polygon.bPortRows);
				if (Polygon.bTextured)
				{
					const uint32 View = PackTextureView(FTexturePage::FromPortAttribute(Polygon.TexturePage), FClutPosition::FromPortAttribute(Polygon.Clut));
					++Result.TexturedGearPolygons;
					Result.SeededGearPolygons += std::ranges::any_of(Going->TextureSeeds, [&Polygon, View](const FTexturePictureSeed& Seed)
					{
						for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
						{
							if (Seed.Region.View != View || !Seed.Region.Contains(Polygon.U[Corner], Polygon.V[Corner]))
							{
								return false;
							}
						}
						return true;
					}) ? 1 : 0;
				}
			}
			for (int32 Gear = 0; Gear < 3 && Anchor != nullptr; ++Gear)
			{
				Result.GearSprites += Anchor->Key == FWorldMapAnchor::MakeKey(EWorldMapAnchorKind::Sprite, static_cast<uint32>(FirstGearSlot + Gear)) ? 1 : 0;
			}
		}
		for (int32 Gear = 0; Gear < 3 && StandIns != nullptr; ++Gear)
		{
			Result.ModelsOf[static_cast<std::size_t>(Gear)] = StandIns->IsShown(FirstGearSlot + Gear) ? StandIns->GearOf(FirstGearSlot + Gear) : -1;
		}
		return Result;
	};
	const FWalk Sprites = Walk(EFieldGearModels::Off);
	const FWalk Models = Walk(EFieldGearModels::Everywhere);
	const FWalk InPlay = Walk(EFieldGearModels::OutsideCutscenes);

	// The sprites: three gears' parts, and no model.
	CHECK(Sprites.GearPolygons == 0);
	CHECK(Sprites.GearSprites >= 3);
	// The models: each slot's own gear, placed finer than the GTE's numbers, its textures in the port's rows; no gear's sprite.
	CHECK(Models.GearSprites == 0);
	CHECK(Models.GearPolygons > 600);
	CHECK(Models.bFineInPortRows);
	// Their textures are named by their models' regions, as a field map's stand-ins' are.
	CHECK(Models.TexturedGearPolygons > 300);
	CHECK(Models.SeededGearPolygons == Models.TexturedGearPolygons);
	CHECK(Models.ModelsOf == std::array<int32, 3>{0, 2, 3});
	CHECK(InPlay.GearPolygons == Models.GearPolygons);
	// The game is the same under each: where the party and its gears went, and the random numbers drawn.
	CHECK(Models.Places == Sprites.Places);
	CHECK(Models.Random == Sprites.Random);
	CHECK(InPlay.Places == Sprites.Places);
	// A model stands about as tall as its sprite's standing frames (the frame seen is one of several), its soles on the sprite's floor.
	for (std::size_t Gear = 0; Gear < 3; ++Gear)
	{
		const float SpriteHeight = Sprites.SpriteHeights[Gear];
		const float ModelHeight = Models.Standing[Gear][1] - Models.Standing[Gear][0];
		INFO("gear " << Gear << ": the sprite " << SpriteHeight << " high about " << Sprites.Origins[Gear] << ", the model from " << Models.Standing[Gear][0]
			<< " to " << Models.Standing[Gear][1]);
		REQUIRE(SpriteHeight > 0.0f);
		CHECK(ModelHeight > SpriteHeight * 0.75f);
		CHECK(ModelHeight < SpriteHeight * 1.1f);
		CHECK(std::fabs(Models.Standing[Gear][1] - Sprites.Origins[Gear]) < SpriteHeight * 0.06f);
	}
}

TEST_CASE("Out of her gear's size on the world map, Chu-Chu's model shrinks away with her sprite, where another gear's stands parked", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 FirstGearSlot = FWorldMapContext::LeaderSlot + 3;
	constexpr uint8 ChuChusGear = 7;
	constexpr FPadState BothShoulders{static_cast<uint16>(FPadState::Bit(EPadButton::L2) | FPadState::Bit(EPadButton::R2))};
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FPlayerSettings Settings = Session.Settings();
	Settings.Field.GearModels = EFieldGearModels::Everywhere;
	Session.SetSettings(Settings);
	FGameState& State = Session.Game();
	State.SetSavedEventVariable(0, 120);
	State.SetParty({0, 2, 3});
	// The leader in Weltall, the second member in Chu-Chu's own size.
	REQUIRE(State.SetCharacterGear(0, 0));
	REQUIRE(State.SetCharacterGear(2, ChuChusGear));
	REQUIRE(State.SetCharacterGear(3, 3));
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		State.SetPartySlotInGear(Slot, 1);
	}
	State.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 8; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	const FFieldGearStandIns* StandIns = Program.Drawing().GearStandIns();
	REQUIRE(StandIns != nullptr);
	// How high a gear's model stands in the latest frame, along the world's y; 0 with none drawn.
	const auto HeightOf = [&](int32 Gear) {
		const FWorldMapFrame& Drawn = *Program.LatestFrame();
		const std::optional<int32> Layout = StandIns->LayoutSlotOf(FirstGearSlot + Gear);
		float Top = 1.0e9f;
		float Bottom = -1.0e9f;
		for (const FWorldMapPolygon& Polygon : Drawn.Polygons)
		{
			if (Polygon.Layer != EWorldMapLayer::Gear || Polygon.Anchor == FWorldMapPolygon::NoAnchor || !Layout
				|| (Drawn.Anchors[Polygon.Anchor].Key & 0xFFFFFF) >> 8 != static_cast<uint32>(*Layout))
			{
				continue;
			}
			for (std::size_t Corner = 0; Corner < Polygon.CornerCount; ++Corner)
			{
				const std::array<std::array<int16, 3>, 3>& M = Drawn.Camera.M;
				const std::array<float, 3>& View = Polygon.View[Corner];
				const float Y = (M[0][1] * View[0] + M[1][1] * View[1] + M[2][1] * View[2]) / static_cast<float>(FGteMatrix::One);
				Top = std::min(Top, Y);
				Bottom = std::max(Bottom, Y);
			}
		}
		return Bottom > Top ? Bottom - Top : 0.0f;
	};
	const float Weltall = HeightOf(0);
	const float ChuChu = HeightOf(1);
	REQUIRE(Weltall > 0.0f);
	REQUIRE(ChuChu > 0.0f);
	REQUIRE(StandIns->GearOf(FirstGearSlot + 1) == ChuChusGear);

	// Out of the gears: her model is lower each tick and gone within a few; Weltall's stands as it stood.
	REQUIRE_FALSE(Frame(Program, BothShoulders, true));
	std::vector<float> Heights;
	for (int32 Tick = 0; Tick < 12; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
		Heights.push_back(HeightOf(1));
	}
	CHECK(std::all_of(Heights.begin(), Heights.end(), [ChuChu](float Height) { return Height < ChuChu * 1.05f; }));
	CHECK(std::any_of(Heights.begin(), Heights.end(), [ChuChu](float Height) { return Height > ChuChu * 0.2f && Height < ChuChu * 0.9f; }));
	CHECK(Heights.back() == 0.0f);
	CHECK_FALSE(StandIns->IsShown(FirstGearSlot + 1));
	CHECK(StandIns->IsShown(FirstGearSlot));
	CHECK(HeightOf(0) == Catch::Approx(Weltall).epsilon(0.05));

	// Into them again: she is her gear's size at once, as her sprite is.
	REQUIRE_FALSE(Frame(Program, BothShoulders, true));
	for (int32 Tick = 0; Tick < 12; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	CHECK(StandIns->IsShown(FirstGearSlot + 1));
	CHECK(HeightOf(1) == Catch::Approx(ChuChu).epsilon(0.05));
}

TEST_CASE("After its menu the world map draws the grown Emeralda as the child once her player has switched her", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint8 Emeralda = 6;
	constexpr uint8 GrownEmeralda = 10;
	// The size of the second member's sprite file on a world map entered with Member, made from the child's record;
	// then, Switch given, the size after the menu switches her look.
	const auto SecondFile = [&Disc](uint8 Member, bool bSwitch)
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FGameSession& Session = *Game->Session;
		FGameState& State = Session.Game();
		if (Member != Emeralda)
		{
			REQUIRE(State.CopyCharacter(Emeralda, Member));
		}
		State.SetParty({0, Member, 0xFF});
		State.SetJoinedCharacters(static_cast<uint16>(State.JoinedCharacters() | 1u << Member));
		EnterWorldMap(Session);
		auto Started = FWorldMapProgram::Start(Session);
		REQUIRE(Started);
		FWorldMapProgram& Program = **Started;
		RunEntryFade(Program);
		for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
		{
			REQUIRE_FALSE(Frame(Program, NoButtons));
		}
		const FWorldMapContext& Context = Program.Runtime().Context();
		REQUIRE(Context.MemberFiles[1]);
		const uint32 Before = Context.MemberFiles[1]->Size();
		if (!bSwitch)
		{
			return std::pair{Before, Before};
		}
		REQUIRE_FALSE(Frame(Program, SquarePressed, true));
		REQUIRE_FALSE(Frame(Program, NoButtons));
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(Member));
		Session.Game().SetCharacterLooks(Looks);
		REQUIRE_FALSE(Program.Runtime().ResumeAfterMenu());
		REQUIRE(Context.MemberFiles[1]);
		REQUIRE(Context.SpriteSlots[FWorldMapContext::LeaderSlot + 1]);
		CHECK(Context.SpriteSlots[FWorldMapContext::LeaderSlot + 1]->PrimaryFile == &*Context.MemberFiles[1]);
		return std::pair{Before, Context.MemberFiles[1]->Size()};
	};
	const auto [Child, ChildAgain] = SecondFile(Emeralda, false);
	const auto [Grown, Switched] = SecondFile(GrownEmeralda, true);
	CHECK(Grown != Child);
	CHECK(Switched == Child);
	CHECK(ChildAgain == Child);
}

TEST_CASE("With Citan's sword put away, the world map draws the second Heimdal as the first and Fenrir in the first's stance", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 CitansGearSlot = FWorldMapContext::LeaderSlot + 4;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 Heimdal = 3;
	constexpr uint8 Fenrir = 11;
	constexpr uint8 SwordHeimdal = 16;
	/** Fenrir's fist and open hand on one side. */
	constexpr std::size_t Fist = 50;
	constexpr std::size_t OpenHand = 51;
	struct FRidden
	{
		int32 Gear = -1;
		std::size_t Bones = 0;
		std::array<uint8, 2> Hands{};
	};
	// The world map with Fei in Weltall and the second Citan riding Gear, his sword put away.
	const auto Ride = [&Disc](uint8 Gear)
	{
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FGameSession& Session = *Game->Session;
		FPlayerSettings Settings = Session.Settings();
		Settings.Field.GearModels = EFieldGearModels::Everywhere;
		Session.SetSettings(Settings);
		FGameState& State = Session.Game();
		State.SetSavedEventVariable(0, 120);
		State.SetParty({0, SwordCitan, 0xFF});
		REQUIRE(State.SetCharacterGear(0, 0));
		REQUIRE(State.SetCharacterGear(SwordCitan, Gear));
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			State.SetPartySlotInGear(Slot, 1);
		}
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(SwordCitan));
		State.SetCharacterLooks(Looks);
		State.SetWorldMapPlace(Place, Entrance, CameraYaw, Scene);
		auto Started = FWorldMapProgram::Start(Session);
		REQUIRE(Started);
		FWorldMapProgram& Program = **Started;
		RunEntryFade(Program);
		for (int32 Tick = 0; Tick < FadeInTicks + 8; ++Tick)
		{
			REQUIRE_FALSE(Frame(Program, NoButtons));
		}
		const FFieldGearStandIns* StandIns = Program.Drawing().GearStandIns();
		REQUIRE(StandIns != nullptr);
		FRidden Ridden;
		Ridden.Gear = StandIns->GearOf(CitansGearSlot);
		if (const FGearModel* Model = StandIns->LoadedModelOf(CitansGearSlot))
		{
			Ridden.Bones = Model->Bones.size();
			if (Ridden.Bones > OpenHand)
			{
				Ridden.Hands = {Model->Bones[Fist].bShown, Model->Bones[OpenHand].bShown};
			}
		}
		return Ridden;
	};
	const FRidden Second = Ride(SwordHeimdal);
	CHECK(Second.Gear == Heimdal);
	CHECK(Second.Bones == 45);
	const FRidden Worn = Ride(Fenrir);
	CHECK(Worn.Gear == Fenrir);
	CHECK(Worn.Bones == 56);
	CHECK(Worn.Hands == std::array<uint8, 2>{1, 0});
}

namespace
{
	/** The orange colours (hue 7-43 degrees, strongly coloured) a sprite file's palette block holds. */
	int32 OrangeColours(const FSpriteFile& File)
	{
		const uint32 Palettes = File.Tables().Palettes;
		const uint32 End = std::min<uint32>(static_cast<uint32>(File.Word(16) | File.Word(18) << 16), File.Size());
		int32 Count = 0;
		for (uint32 At = Palettes + 4; At + 2 <= End; At += 2)
		{
			const uint16 Colour = File.Word(At);
			const int32 R = Colour & 31;
			const int32 G = Colour >> 5 & 31;
			const int32 B = Colour >> 10 & 31;
			const int32 Max = std::max({R, G, B});
			const int32 Min = std::min({R, G, B});
			if (Max > 1 && Max == R && Max != Min)
			{
				const double Hue = 60.0 * (G - B) / (Max - Min);
				Count += Hue >= 7.2 && Hue <= 43.2 && (Max - Min) * 100 >= Max * 55 ? 1 : 0;
			}
		}
		return Count;
	}
}

TEST_CASE("After its menu the world map draws Elly with purple hair once her player has switched her", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	auto Game = Testing::FGameLoopSession::Start(*Disc);
	FGameSession& Session = *Game->Session;
	FGameState& State = Session.Game();
	State.SetParty({0, FCharacterLooks::Elly, 0xFF});
	EnterWorldMap(Session);
	auto Started = FWorldMapProgram::Start(Session);
	REQUIRE(Started);
	FWorldMapProgram& Program = **Started;
	RunEntryFade(Program);
	for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
	{
		REQUIRE_FALSE(Frame(Program, NoButtons));
	}
	const FWorldMapContext& Context = Program.Runtime().Context();
	REQUIRE(Context.MemberFiles[1]);
	CHECK(OrangeColours(*Context.MemberFiles[1]) > 4);
	REQUIRE_FALSE(Frame(Program, SquarePressed, true));
	REQUIRE_FALSE(Frame(Program, NoButtons));
	FCharacterLooks Looks;
	REQUIRE(Looks.Switch(FCharacterLooks::Elly));
	Session.Game().SetCharacterLooks(Looks);
	REQUIRE_FALSE(Program.Runtime().ResumeAfterMenu());
	REQUIRE(Context.MemberFiles[1]);
	CHECK(OrangeColours(*Context.MemberFiles[1]) == 0);
	CHECK(Context.SpriteSlots[FWorldMapContext::LeaderSlot + 1]->PrimaryFile == &*Context.MemberFiles[1]);
}

TEST_CASE("The world map draws Fei as Id with Id's own sprite from the port's rows, and as Fei again after", "[Game][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto AsId = [](FGameState& State, bool bId)
	{
		FCharacterLooks Looks = State.CharacterLooks();
		if (Looks.IsId(FCharacterLooks::Fei) != bId)
		{
			REQUIRE(Looks.Switch(FCharacterLooks::Fei));
		}
		State.SetCharacterLooks(Looks);
	};
	// The leader drawn as Id: his file at his picture's page, the picture in the drawing's port rows, his parts drawn from it.
	const auto CheckId = [](const FWorldMapProgram& Program)
	{
		const FWorldMapContext& Context = Program.Runtime().Context();
		REQUIRE(Program.Runtime().IdShown() != nullptr);
		const FIdsFieldSprite& Id = *Program.Runtime().IdShown();
		REQUIRE(Context.MemberFiles[0]);
		CHECK(Context.MemberFiles[0]->IsResident(Context.MemberFiles[0]->Tables()));
		CHECK(Context.MemberFiles[0]->AnimationCount(Context.MemberFiles[0]->Tables()) == 15);
		const FSprite& Leader = *Context.SpriteSlots[FWorldMapContext::LeaderSlot];
		CHECK(Leader.PrimaryFile == &*Context.MemberFiles[0]);
		CHECK(Leader.Resource->TextureX == FIdsFieldSprite::TextureX);
		CHECK(Leader.Resource->TextureY == FIdsFieldSprite::TextureY);
		// Both his palettes at a place of their own: his frames toward the camera draw with the second, which at the
		// leader's place would lie where the next member's palette goes.
		CHECK(Leader.Resource->ClutX == FIdsFieldSprite::ClutX);
		CHECK(Leader.Resource->ClutY == FIdsFieldSprite::ClutY);
		const FSpriteFile& File = *Context.MemberFiles[0];
		REQUIRE(File.PalettesPerSet(File.Tables()) == 2);
		int32 Colours = 0;
		for (int32 Colour = 0; Colour < 32; ++Colour)
		{
			Colours += Program.Drawing().Vram().TexelAt(FIdsFieldSprite::ClutX + Colour, FIdsFieldSprite::ClutY)
						== File.Word(File.PaletteSet(File.Tables(), 0) + 2 * static_cast<uint32>(Colour)) ? 1 : 0;
		}
		CHECK(Colours == 32);
		const FSprite& Follower = *Context.SpriteSlots[FWorldMapContext::LeaderSlot + 1];
		const FSpriteFile& Hers = *Context.MemberFiles[1];
		for (int32 Colour = 0; Colour < 16; ++Colour)
		{
			CHECK(Program.Drawing().Vram().TexelAt(Follower.Resource->ClutX + Colour, Follower.Resource->ClutY)
				  == Hers.Word(Hers.PaletteSet(Hers.Tables(), 0) + 2 * static_cast<uint32>(Colour)));
		}
		const FVramRect& Rect = Id.Picture().Rect;
		int32 Placed = 0;
		for (int32 Row = 0; Row < Rect.Height; ++Row)
		{
			for (int32 Column = 0; Column < Rect.Width; ++Column)
			{
				Placed += Program.Drawing().Vram().TexelAt(Rect.X + Column, Rect.Y + Row)
							== Id.Picture().Units[static_cast<std::size_t>(Row * Rect.Width + Column)] ? 1 : 0;
			}
		}
		CHECK(Placed == Rect.Width * Rect.Height);
		const FWorldMapFrame* Drawn = Program.LatestFrame();
		REQUIRE(Drawn != nullptr);
		int32 HisParts = 0;
		for (const FWorldMapPolygon& Polygon : Drawn->Polygons)
		{
			if (Polygon.Layer == EWorldMapLayer::Sprite && Polygon.bPortRows)
			{
				const FTexturePage Page = FTexturePage::FromPortAttribute(Polygon.TexturePage);
				HisParts += Page.X == FIdsFieldSprite::TextureX && Page.Y == FIdsFieldSprite::TextureY ? 1 : 0;
			}
		}
		CHECK(HisParts > 0);
	};
	for (const bool bEnteredAsId : {false, true})
	{
		INFO("entered as Id " << bEnteredAsId);
		auto Game = Testing::FGameLoopSession::Start(*Disc);
		FGameSession& Session = *Game->Session;
		Session.Game().SetParty({FCharacterLooks::Fei, FCharacterLooks::Elly, 0xFF});
		AsId(Session.Game(), bEnteredAsId);
		EnterWorldMap(Session);
		auto Started = FWorldMapProgram::Start(Session);
		REQUIRE(Started);
		FWorldMapProgram& Program = **Started;
		RunEntryFade(Program);
		for (int32 Tick = 0; Tick < FadeInTicks + 2; ++Tick)
		{
			REQUIRE_FALSE(Frame(Program, NoButtons));
		}
		const FWorldMapContext& Context = Program.Runtime().Context();
		const auto Leader = [&Context]() -> const FSprite& { return *Context.SpriteSlots[FWorldMapContext::LeaderSlot]; };
		// The look switched as the menu would, the party's sprites made again on the map's next tick.
		const auto SwitchTo = [&](bool bId)
		{
			AsId(Session.Game(), bId);
			for (int32 Tick = 0; Tick < 4; ++Tick)
			{
				REQUIRE_FALSE(Frame(Program, NoButtons));
			}
		};
		if (!bEnteredAsId)
		{
			SwitchTo(true);
		}
		CheckId(Program);
		// As Fei again, his own file streamed where the leader's task put it.
		SwitchTo(false);
		CHECK(Program.Runtime().IdShown() == nullptr);
		CHECK_FALSE(Context.MemberFiles[0]->IsResident(Context.MemberFiles[0]->Tables()));
		const FWorldMapSpritePlace& Asked = Context.SpritePlaces[FWorldMapContext::LeaderSlot];
		CHECK(Asked.TextureY < FVram::Height);
		CHECK(Leader().Resource->TextureX == Asked.TextureX);
		CHECK(Leader().Resource->TextureY == Asked.TextureY);
		CHECK(Leader().Resource->ClutX == Asked.ClutX);
		CHECK(Leader().Resource->ClutY == Asked.ClutY);
	}
}
