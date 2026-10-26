#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattleGameHud.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Game/GameState.h"
#include "Game/GameStateNames.h"
#include "Game/KernelState.h"
#include "Game/PsxRandom.h"
#include "Input/ButtonGlyphs.h"
#include "Input/KernelPad.h"
#include "Menu/MenuButtonIcons.h"
#include "Menu/MenuDrawing.h"
#include "Support/BattleProgramDisc.h"
#include "Support/BattleSoundRecorder.h"
#include "Support/BoundGlyphs.h"
#include "Support/GameDiscs.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

using namespace Weltall;

TEST_CASE("The battle's presentation plays a battle with the game's HUD, which draws the party's panels once the battle has started", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
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
	const FBattleHudSettings Settings{Kernel.WindowColour, Kernel.BattleWindowBlend};

	FBattleRequest Request;
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, Settings);
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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

	// Cross pressed now and then: the menus' confirm, and the attacks' press.
	bool bDrewPanels = false;
	for (int32 Frame = 0; Frame < 1500 && !(*Runtime)->IsOver(); ++Frame)
	{
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			const bool bPressed = Frame % 24 < 2;
			Pad.Vblank(FPadState{static_cast<uint16>(bPressed ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		INFO("frame " << Frame << ", the show's fault " << static_cast<int32>(Presentation->Fault()) << ", the HUD's " << static_cast<int32>(Hud.Fault()));
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		if (Presentation->HasStarted() && !Presentation->Show.Frame().Hud.Primitives.empty())
		{
			bDrewPanels = true;
		}
	}
	CHECK(bDrewPanels);
}

TEST_CASE("A paused battle keeps showing the frame it was paused over until it goes on", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
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
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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
	constexpr uint16 Start = 1u << 3;
	const auto Frame = [&](uint16 Held) {
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{Held}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
	};
	for (int32 Count = 0; Count < 2000 && !Presentation->HasStarted(); ++Count)
	{
		Frame(0);
	}
	REQUIRE(Presentation->HasStarted());
	for (int32 Count = 0; Count < 10; ++Count)
	{
		Frame(0);
	}
	const FBattleShow& Show = Presentation->Show;
	REQUIRE_FALSE(Show.Frame().Quads.empty());

	// Start pauses the battle inside a frame it has begun: what is shown stays the last frame displayed, whole.
	Frame(Start);
	for (int32 Count = 0; Count < 10 && !Presentation->IsPaused(); ++Count)
	{
		Frame(0);
	}
	REQUIRE(Presentation->IsPaused());
	const uint32 Step = Show.Frame().Step;
	const std::size_t Links = Show.Frame().Order.size();
	const std::size_t Quads = Show.Frame().Quads.size();
	CHECK(Links > Quads);
	for (int32 Count = 0; Count < 30; ++Count)
	{
		Frame(0);
		REQUIRE(Presentation->IsPaused());
		CHECK(Show.Frame().Step == Step);
		CHECK(Show.Frame().Order.size() == Links);
		CHECK(Show.Frame().Quads.size() == Quads);
		for (const FBattleShowLink& Link : Show.Frame().Order)
		{
			if (Link.Kind == EBattleShowLink::Quad)
			{
				REQUIRE(Link.Index < Show.Frame().Quads.size());
			}
		}
	}

	// Start again: the battle goes on from the frame it held.
	Frame(Start);
	for (int32 Count = 0; Count < 10 && Presentation->IsPaused(); ++Count)
	{
		Frame(0);
	}
	REQUIRE_FALSE(Presentation->IsPaused());
	Frame(0);
	CHECK(Show.Frame().Step > Step);
	CHECK_FALSE(Show.Frame().Order.empty());
}

TEST_CASE("A won battle's result screen is drawn by the game's HUD and waits for the player's button, with its count's tick and its sounds stopped at its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
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
	const FBattleHudSettings Settings{Kernel.WindowColour, Kernel.BattleWindowBlend};

	FBattleRequest Request;
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, Settings);
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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
	const FBattleContext& Context = (*Runtime)->Context();

	// Cross pressed now and then through the battle; once the result's panels are up, not for a while: the screen waits.
	constexpr int32 Patience = 120;
	constexpr uint32 Tick = 0x5B;
	std::size_t PanelParts = 0;
	std::size_t SpoilsParts = 0;
	int32 Waited = 0;
	int32 Ticks = 0;
	bool bStopped = false;
	for (int32 Frame = 0; Frame < 4000 && !(*Runtime)->IsOver(); ++Frame)
	{
		const bool bWaiting = Context.ResultRun.bPanelsShown && Waited < Patience;
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			const bool bPressed = !bWaiting && Frame % 24 < 2;
			Pad.Vblank(FPadState{static_cast<uint16>(bPressed ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		INFO("frame " << Frame << ", the show's fault " << static_cast<int32>(Presentation->Fault()) << ", the HUD's " << static_cast<int32>(Hud.Fault()));
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		const FBattleResultRun& Run = Context.ResultRun;
		const std::size_t Parts = Presentation->Show.Frame().Hud.Primitives.size();
		if (bWaiting)
		{
			++Waited;
			CHECK(Run.bPanelsShown);
			CHECK_FALSE(Run.bCounting);
		}
		if (Run.bPanelsShown)
		{
			PanelParts = std::max(PanelParts, Parts);
		}
		if (Run.bSpoilsShown)
		{
			SpoilsParts = std::max(SpoilsParts, Parts);
		}
		for (const Testing::FBattleSoundRecorder::FEvent& Event : Sound.Events)
		{
			Ticks += Run.bBegun && Event.Name == "play" && (Event.Values[0] & 0xFF) == Tick;
			bStopped = bStopped || Event.Name == "stop all";
		}
		Sound.Events.clear();
	}
	REQUIRE((*Runtime)->IsOver());
	REQUIRE((*Runtime)->Outcome());
	CHECK((*Runtime)->Outcome()->Result == FKernelState::BattleWon);
	CHECK(Waited == Patience);
	// The three panels' portraits, labels and figures; the spoils' two windows with the gold's labels and figures.
	CHECK(PanelParts > 90);
	CHECK(SpoilsParts > 40);
	CHECK(Ticks > 0);
	CHECK(bStopped);
	CHECK(Context.Result.TotalExp > 0);
}

TEST_CASE("The battle's show sorts a stage's quads by their average depth, as the original's: a banner hangs behind the roofs before it", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Rico's fight on foot: stage 2, whose fifth bone (the banner and the door signs) is sorted by each polygon's
	// average depth, its quads among them.
	constexpr int32 Map = 54;
	constexpr int32 Formation = 7;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
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
	const FBattleHudSettings Settings{Kernel.WindowColour, Kernel.BattleWindowBlend};

	FBattleRequest Request;
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Request.Kind = 5;
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, Settings, &*Loaded.EventFiles);
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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
	for (int32 Frame = 0; Frame < 300; ++Frame)
	{
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{0}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
	}

	// The event's opening shot, where the original's own drawing of the stage was read.
	const FBattleShowFrame& Shown = Presentation->Show.Frame();
	REQUIRE(Shown.Eye == std::array<int16, 3>{813, -401, -390});
	REQUIRE(Shown.Target == std::array<int16, 3>{813, -103, 590});
	REQUIRE(Shown.Objects != nullptr);
	constexpr uint16 BannerBone = 5;
	// The original's ordering-table entries for the bone's quads in this shot, by the polygon's number in its part.
	constexpr std::array<std::array<int32, 2>, 7> Original{{{48, 82}, {50, 81}, {58, 83}, {70, 84}, {72, 86}, {74, 85}, {75, 85}}};
	int32 Found = 0;
	int32 NearestRoof = 0x1000;
	int32 NearestBanner = 0x1000;
	for (const FBattleObjectLink& Link : Shown.Objects->Links)
	{
		if (Link.Kind != EBattleObjectDraw::Polygon)
		{
			continue;
		}
		const FBattleDrawnPolygon& Polygon = Shown.Objects->Polygons[Link.Index];
		const FBattleDrawnPart& Part = Shown.Objects->Parts[Polygon.Part];
		if (Part.Slot != FBattleObjects::StageSlot)
		{
			continue;
		}
		if (Part.Bone == BannerBone)
		{
			NearestBanner = std::min(NearestBanner, Link.Entry);
			for (const std::array<int32, 2>& Expected : Original)
			{
				if (Polygon.Number == Expected[0])
				{
					INFO("the bone's polygon " << Expected[0]);
					CHECK(Link.Entry == Expected[1]);
					++Found;
				}
			}
		}
		else if (Part.Bone == 2 || Part.Bone == 3)
		{
			NearestRoof = std::min(NearestRoof, Link.Entry);
		}
	}
	CHECK(Found == static_cast<int32>(Original.size()));
	// The buildings' nearest pieces lie before the banner's: drawn after it, over it.
	CHECK(NearestRoof < NearestBanner);
}

TEST_CASE("The battle HUD's button pictures are shown as the player's controller prints the bound buttons", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr int32 Map = 15;
	constexpr int32 Formation = 1;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
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
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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

	// Cross now and then takes Attack and strikes: the frame kept is the first that shows the ring's four face buttons
	// with the symbol of an attack button pressed.
	const auto Count = [](const FMenuFrame& Frame, EMenuIconSlot Slot) {
		return std::count_if(Frame.Primitives.begin(), Frame.Primitives.end(),
			[Slot](const FMenuPrimitive& Primitive) { return Primitive.ButtonRole != 0 && Primitive.ButtonSlot == Slot; });
	};
	FMenuFrame Kept;
	for (int32 Frame = 0; Frame < 1500 && !(*Runtime)->IsOver() && Kept.Primitives.empty(); ++Frame)
	{
		for (int32 Vblank = (*Runtime)->VblanksPerFrame(); Vblank > 0; --Vblank)
		{
			Pad.Vblank(FPadState{static_cast<uint16>(Frame % 24 < 2 ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		FMenuFrame Shown;
		const std::span<const FMenuPrimitive> Primitives = Presentation->Show.Frame().Hud.Primitives;
		Shown.Primitives.assign(Primitives.begin(), Primitives.end());
		if (Count(Shown, EMenuIconSlot::FaceButton) >= 4 && Count(Shown, EMenuIconSlot::Symbol) >= 1)
		{
			Kept = std::move(Shown);
		}
	}
	REQUIRE_FALSE(Kept.Primitives.empty());
	Kept.Number = 1;
	FMenuDrawBuilder Builder;
	FDrawList Original;
	Builder.Build(Kept, nullptr, 1.0f, Original);
	const std::vector<FMenuDrawnIcon> Icons = Builder.Icons;
	const FMenuSpriteSheet& Sheet = Loaded.HudResources->Sheet;
	const auto IconOf = [&Icons](uint16 Role, EMenuIconSlot Slot) {
		const auto Found = std::find_if(Icons.begin(), Icons.end(), [Role, Slot](const FMenuDrawnIcon& Icon) { return Icon.Role == Role && Icon.Slot == Slot; });
		REQUIRE(Found != Icons.end());
		return Found->Primitive;
	};
	// The ring's physical circle (the game's logical Cross) and cross (its logical Circle), and the cross pressed.
	const uint32 RingCircle = IconOf(0x40, EMenuIconSlot::FaceButton);
	const uint32 RingCross = IconOf(0x20, EMenuIconSlot::FaceButton);
	const uint32 Pressed = IconOf(0x20, EMenuIconSlot::Symbol);
	FMenuButtonIcons Pictures;

	// A PlayStation pad bound as the console's: the game's own pictures, as drawn.
	Testing::FBoundGlyphs PlayStation(EButtonIconSet::PlayStation);
	PlayStation.Bind({{EPadButton::Cross, "Cross"}, {EPadButton::Circle, "Circle"}, {EPadButton::Square, "Square"}, {EPadButton::Triangle, "Triangle"}});
	FDrawList List = Original;
	Pictures.Apply(List, Icons, PlayStation, Sheet, BattleHudButtonPictures);
	for (const FMenuDrawnIcon& Icon : Icons)
	{
		INFO("role " << Icon.Role << ", slot " << static_cast<int32>(Icon.Slot));
		CHECK(List.Primitives[Icon.Primitive].Texture == Original.Primitives[Icon.Primitive].Texture);
		CHECK(List.Primitives[Icon.Primitive].Corners[0].U == Original.Primitives[Icon.Primitive].Corners[0].U);
		CHECK(List.Primitives[Icon.Primitive].Corners[0].V == Original.Primitives[Icon.Primitive].Corners[0].V);
	}
	CHECK(Pictures.Pictures().Revision() == 0);

	// Confirm and cancel swapped: the ring's two pictures change places, and the pressed button, a circle the sheet has
	// no symbol of, is the port's picture.
	PlayStation.Bind({{EPadButton::Cross, "Circle"}, {EPadButton::Circle, "Cross"}, {EPadButton::Square, "Square"}, {EPadButton::Triangle, "Triangle"}});
	List = Original;
	Pictures.Apply(List, Icons, PlayStation, Sheet, BattleHudButtonPictures);
	CHECK(List.Primitives[RingCross].Corners[0].U == Original.Primitives[RingCircle].Corners[0].U);
	CHECK(List.Primitives[RingCircle].Corners[0].U == Original.Primitives[RingCross].Corners[0].U);
	CHECK(List.Primitives[RingCross].Texture->Vram == 0);
	CHECK(List.Primitives[Pressed].Texture->Vram == FMenuButtonIcons::PicturesVram);
	CHECK(Pictures.Pictures().Revision() == 1);

	// An Xbox pad: the port's pictures everywhere, each as dimmed and blended as the game's was.
	Testing::FBoundGlyphs Xbox(EButtonIconSet::Xbox);
	Xbox.Bind({{EPadButton::Cross, "A"}, {EPadButton::Circle, "B"}, {EPadButton::Square, "X"}, {EPadButton::Triangle, "Y"}});
	List = Original;
	Pictures.Apply(List, Icons, Xbox, Sheet, BattleHudButtonPictures);
	for (const FMenuDrawnIcon& Icon : Icons)
	{
		const FDrawPrimitive& Drawn = List.Primitives[Icon.Primitive];
		const FDrawPrimitive& Before = Original.Primitives[Icon.Primitive];
		INFO("role " << Icon.Role << ", slot " << static_cast<int32>(Icon.Slot));
		CHECK(Drawn.Texture->Vram == FMenuButtonIcons::PicturesVram);
		CHECK(Drawn.Texture->Page.Blend == Before.Texture->Page.Blend);
		CHECK(Drawn.bModulate == Before.bModulate);
		CHECK(Drawn.Blend == Before.Blend);
		CHECK(Drawn.Corners[1].X - Drawn.Corners[0].X == Catch::Approx(Before.Corners[1].X - Before.Corners[0].X));
	}

	// The original icons in the Japanese layout, as the player's glyphs give them on any pad: the ring's cross and circle
	// change places, and so do its square and triangle (the weak attack on the square, the strong on the triangle).
	struct FLayoutGlyphs final : IButtonGlyphs
	{
		FControlBindings Bindings = FControlBindings::Defaults();
		FButtonGlyph Glyph(EPadButton Button) const override { return ButtonGlyph(Button, Bindings, EButtonIconSet::Xbox); }
		uint32 Revision() const override { return 1; }
	} Japan;
	Japan.Bindings.bOriginalButtonIcons = true;
	Japan.Bindings.ChangeButtonLayout(EButtonLayout::Japan);
	const uint32 RingSquare = IconOf(0x10, EMenuIconSlot::FaceButton);
	const uint32 RingTriangle = IconOf(0x80, EMenuIconSlot::FaceButton);
	List = Original;
	Pictures.Apply(List, Icons, Japan, Sheet, BattleHudButtonPictures);
	for (const auto& [Cell, Shown] : {std::pair{RingCross, RingCircle}, std::pair{RingCircle, RingCross}, std::pair{RingSquare, RingTriangle},
		std::pair{RingTriangle, RingSquare}})
	{
		CHECK(List.Primitives[Cell].Texture->Vram == 0);
		CHECK(List.Primitives[Cell].Corners[0].U == Original.Primitives[Shown].Corners[0].U);
		CHECK(List.Primitives[Cell].Corners[0].V == Original.Primitives[Shown].Corners[0].V);
	}
	CHECK(List.Primitives[Pressed].Texture->Vram == FMenuButtonIcons::PicturesVram);

	// The prompt's words go with the button of the command they name (cancel's with the circle), and the Japanese layout
	// sets each button out with its words at its symbol's place on the pad, as the Japanese release draws them.
	const auto Find = [](const std::vector<FMenuPrimitive>& Primitives, uint16 Role, bool bWords) {
		const auto Found = std::find_if(Primitives.begin(), Primitives.end(), [Role, bWords](const FMenuPrimitive& Primitive) {
			return Primitive.PromptRole == Role && (bWords ? Primitive.ButtonRole == 0 : Primitive.ButtonRole == Role && Primitive.ButtonSlot == EMenuIconSlot::FaceButton);
		});
		REQUIRE(Found != Primitives.end());
		return std::pair{Found->Corners[0].X, Found->Corners[0].Y};
	};
	std::vector<FMenuPrimitive> Placed = Kept.Primitives;
	Pictures.PlacePrompt(Placed, Japan);
	for (const auto& [Role, Place] : {std::pair<uint16, uint16>{0x80, 0x10}, {0x10, 0x80}, {0x20, 0x40}, {0x40, 0x20}})
	{
		INFO("role " << Role);
		const auto [ButtonX, ButtonY] = Find(Kept.Primitives, Role, false);
		const auto [WordsX, WordsY] = Find(Kept.Primitives, Role, true);
		const auto [PlaceX, PlaceY] = Find(Kept.Primitives, Place, false);
		CHECK(Find(Placed, Role, false) == std::pair{PlaceX, PlaceY});
		CHECK(Find(Placed, Role, true) == std::pair{static_cast<int16>(WordsX + PlaceX - ButtonX), static_cast<int16>(WordsY + PlaceY - ButtonY)});
	}
	// A PlayStation pad with confirm and cancel swapped by the player: the fierce attack at the circle's place.
	Placed = Kept.Primitives;
	Pictures.PlacePrompt(Placed, PlayStation);
	CHECK(Find(Placed, 0x20, false) == Find(Kept.Primitives, 0x40, false));
}

TEST_CASE("A scripted battle's message is typed in its window by the game's HUD and goes on at the player's button", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Map 54's formation 7: a battle with an event, whose script opens with a walk, two voices and then messages
	// in a window with their speaker's portrait.
	constexpr int32 Map = 54;
	constexpr int32 Formation = 7;
	constexpr uint8 ScriptedKind = 5;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
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
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Request.Kind = ScriptedKind;
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend}, &*Loaded.EventFiles);
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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

	// The pad's Cross: the game's confirm on this release.
	constexpr uint16 Confirm = 1u << 14;
	const uint32 TextPage = MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, 0x380, 0x100));
	// What a frame's HUD shows of a message: its lines' sprites and how wide they are together, each after the
	// drawing mode that names the text's page.
	struct FShownText
	{
		int32 Lines = 0;
		int32 Width = 0;
	};
	const auto ShownText = [&]() {
		FShownText Shown;
		uint32 Mode = 0;
		for (const FMenuPrimitive& Primitive : Presentation->Show.Frame().Hud.Primitives)
		{
			if (Primitive.IsDrawMode())
			{
				Mode = Primitive.Words[0];
			}
			else if (Primitive.IsRectangle() && Mode == TextPage)
			{
				++Shown.Lines;
				Shown.Width += Primitive.Width;
				CHECK(Primitive.Height == 13);
				CHECK(Primitive.Code == (MenuGpu::Sprite | MenuGpu::RawTexture));
			}
		}
		return Shown;
	};
	const auto Frame = [&](bool bConfirm) {
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{bConfirm ? Confirm : uint16{0}}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		INFO("the show's fault " << static_cast<int32>(Presentation->Fault()) << ", the HUD's " << static_cast<int32>(Hud.Fault()));
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
	};

	// The first message: its text grows a glyph a frame until it stops for the button.
	int32 Widest = 0;
	int32 Still = 0;
	for (int32 Count = 0; Count < 900 && Still < 30; ++Count)
	{
		Frame(false);
		const FShownText Shown = ShownText();
		Still = Shown.Width > 0 && Shown.Width == Widest ? Still + 1 : 0;
		CHECK(Shown.Width >= Widest);
		Widest = std::max(Widest, Shown.Width);
	}
	REQUIRE(Widest > 0);
	REQUIRE(Still == 30);
	const FBattleEventWork& Event = (*Runtime)->Context().Event;
	CHECK(Event.bMessageShown);
	CHECK(Event.bArrowShown);
	CHECK(Event.bPortraitShown);
	// Its speaker's portrait is drawn from the image the HUD uploaded for it: 64 by 64 of an 8-bit page.
	const std::span<const FMenuPrimitive> Primitives = Presentation->Show.Frame().Hud.Primitives;
	const auto Portrait = std::find_if(Primitives.begin(), Primitives.end(), [](const FMenuPrimitive& Primitive) {
		return Primitive.IsPolygon() && Primitive.Page == MenuGpu::TexturePage(1, 0, 0x3C0, 0x100) && Primitive.Clut == MenuGpu::Clut(0, 0x1D0);
	});
	REQUIRE(Portrait != Primitives.end());
	CHECK(std::abs(Portrait->Corners[1].X - Portrait->Corners[0].X) == 0x40);
	CHECK(Portrait->Corners[2].Y - Portrait->Corners[0].Y == 0x40);
	// The text's image is in VRAM where the lines' sprites read it.
	const FVram& Vram = *Presentation->Show.Frame().Vram;
	bool bTyped = false;
	for (int32 X = 0x380; X < 0x3C0 && !bTyped; ++X)
	{
		for (int32 Y = 0x100; Y < 0x10D && !bTyped; ++Y)
		{
			bTyped = Vram.At(X, Y) != 0;
		}
	}
	CHECK(bTyped);

	// The button: the message goes on, to its end or its next page, and what was shown is gone.
	Frame(true);
	bool bChanged = false;
	for (int32 Count = 0; Count < 60 && !bChanged; ++Count)
	{
		Frame(false);
		bChanged = ShownText().Width != Widest;
	}
	CHECK(bChanged);
	CHECK_FALSE(Hud.HasStopped());
}

TEST_CASE("A scripted battle's message tells the buttons it shows", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Lahan's formation 2: Weltall's first battle, whose event has the gear's computer explain the attack buttons.
	constexpr int32 Map = 2;
	constexpr int32 Formation = 2;
	constexpr uint8 ScriptedKind = 5;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 0xFF, 0xFF});
	Game.SetJoinedCharacters(1);
	Game.SetCharacterGear(0, 0);
	Game.SetPartySlotInGear(0, 1);
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
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Request.Kind = ScriptedKind;
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend}, &*Loaded.EventFiles);
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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

	// The pad's Cross, the game's confirm on this release, now and then: the message goes on page by page.
	constexpr uint16 Confirm = 1u << 14;
	const uint32 TextPage = MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, 0x380, 0x100));
	std::array<int32, 3> Seen{};
	for (int32 Count = 0; Count < 4000 && std::any_of(Seen.begin(), Seen.end(), [](int32 Times) { return Times == 0; }); ++Count)
	{
		for (int32 Vblank = (*Runtime)->VblanksPerFrame(); Vblank > 0; --Vblank)
		{
			Pad.Vblank(FPadState{Count % 40 == 0 ? Confirm : uint16{0}}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		INFO("frame " << Count << ", the show's fault " << static_cast<int32>(Presentation->Fault()) << ", the HUD's " << static_cast<int32>(Hud.Fault()));
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
		const FBattleHudFrame& Shown = Presentation->Show.Frame().Hud;
		for (const FTextButtonCell& Cell : Shown.TextButtons)
		{
			// Each is a glyph of a line of the message: a sprite of the text's page, 13 rows high, wide enough to hold it.
			REQUIRE(Cell.Primitive < Shown.Primitives.size());
			const FMenuPrimitive& Line = Shown.Primitives[Cell.Primitive];
			REQUIRE(Line.IsRectangle());
			REQUIRE(Line.Height == 13);
			REQUIRE(Cell.OffsetX + Cell.Width <= Line.Width);
			uint32 Mode = 0;
			for (std::size_t Before = 0; Before < Cell.Primitive; ++Before)
			{
				Mode = Shown.Primitives[Before].IsDrawMode() ? Shown.Primitives[Before].Words[0] : Mode;
			}
			REQUIRE(Mode == TextPage);
			REQUIRE(Cell.LineRest == 0);
			Seen[0] += Cell.Button == EPadButton::Triangle ? 1 : 0;
			Seen[1] += Cell.Button == EPadButton::Square ? 1 : 0;
			Seen[2] += Cell.Button == EPadButton::Cross ? 1 : 0;
		}
	}
	CHECK(Seen[0] > 0);
	CHECK(Seen[1] > 0);
	CHECK(Seen[2] > 0);
}

TEST_CASE("A 3D combatant the target markers mark takes the objects' pulsing ambient", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Map 64's formation 2: a random encounter in gears, whose enemies are 3D objects.
	constexpr int32 Map = 64;
	constexpr int32 Formation = 2;
	constexpr std::array<int32, 1> Maps{Map};
	Testing::FBattleProgramDisc Loaded;
	REQUIRE(Testing::LoadBattleProgramDisc(*Disc, Maps, Loaded));

	FGameState Game = *Loaded.NewGame;
	Game.SetParty({0, 1, 2});
	Game.SetJoinedCharacters(0x1FF);
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		Game.SetPartySlotInGear(Slot, 1);
		REQUIRE(Game.SetCharacterGear(Slot, static_cast<uint8>(Slot)));
	}
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
	const std::vector<uint8>& Encounters = Loaded.Encounters.at(Map);
	std::copy_n(Encounters.begin() + Formation * static_cast<std::ptrdiff_t>(FBattleFormation::RecordSize), FBattleFormation::RecordSize,
		Request.Formation.begin());
	Testing::FBattleSoundRecorder Sound;
	FBattleGameHud Hud(*Loaded.HudResources, Text, FBattleHudSettings{Kernel.WindowColour, Kernel.BattleWindowBlend});
	const auto Presentation = std::make_unique<FBattlePresentation>(Loaded.PresentationFiles(), &Sound, &Hud);
	FBattleRuntimeServices Services;
	Services.Image = &Disc->Image();
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
	const auto Frame = [&](bool bCross) {
		for (int32 Count = (*Runtime)->VblanksPerFrame(); Count > 0; --Count)
		{
			Pad.Vblank(FPadState{static_cast<uint16>(bCross ? 1u << 14 : 0u)}, Game.PlayTime());
		}
		(*Runtime)->Frame();
		REQUIRE((*Runtime)->Fault() == EBattleFault::None);
	};
	for (int32 Count = 0; Count < 2000 && !Presentation->HasStarted(); ++Count)
	{
		Frame(false);
	}
	REQUIRE(Presentation->HasStarted());
	// The party's turns mark their targets as the player picks them (Cross now and then): every drawn combatant the markers
	// mark takes the pulsing ambient.
	const FBattleObjects& Objects = Presentation->Show.Objects;
	int32 PulsedFrames = 0;
	for (int32 Count = 0; Count < 1500 && !(*Runtime)->IsOver(); ++Count)
	{
		Frame(Count % 24 < 2);
		CHECK(Objects.PulseMask == Presentation->Show.Markers.Mask);
		REQUIRE(Presentation->Show.Frame().Objects != nullptr);
		for (const FBattleDrawnPart& Part : Presentation->Show.Frame().Objects->Parts)
		{
			if (Part.Slot >= FBattleObjects::CombatantSlots || ((Objects.PulseMask >> Part.Slot) & 1) == 0)
			{
				continue;
			}
			INFO("frame " << Count << ", slot " << static_cast<int32>(Part.Slot) << ", bone " << Part.Bone);
			CHECK(Part.Ambient == Objects.PulsedAmbient);
			++PulsedFrames;
		}
	}
	CHECK(PulsedFrames > 0);
}
