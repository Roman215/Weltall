#include "Battle/BattleContext.h"
#include "Battle/BattleProgram.h"
#include "Battle/Show/BattleShowHud.h"
#include "Game/GameState.h"
#include "Game/PsxRandom.h"
#include "Graphics/Vram.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/GameText.h"

#include <catch_amalgamated.hpp>

#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

// The HUD's moving parts carry blend keys from frame to frame, which the display draws between two of the show's frames.
namespace
{
	constexpr uint8 NoMember = 0x7F;
	constexpr uint8 InputBattle = 1;
	constexpr uint8 IconStarting = 3;
	constexpr uint8 IconArrived = 1;
	/** The battle view's reach: no part of a flight or a growth moves further in one frame. */
	constexpr int32 DisplayReach = 64;

	/** A battle's HUD read from the disc, Fei alone in the middle slot, drawn frame by frame as the battle's input mode does. */
	class FHudRun
	{
	public:
		explicit FHudRun(const FIso9660& Disc)
		{
			const auto Executable = ReadBootExecutable(Disc);
			REQUIRE(Executable);
			const auto Index = FGameFileIndex::Read(*Executable);
			REQUIRE(Index);
			auto LoadedResources = FBattleHudResources::Load(Disc.Image(), *Index, *Executable);
			REQUIRE(LoadedResources);
			Resources = std::make_unique<FBattleHudResources>(std::move(*LoadedResources));
			auto LoadedProgram = FBattleProgramTables::Load(Disc.Image(), *Index);
			REQUIRE(LoadedProgram);
			Program = std::make_unique<FBattleProgramTables>(std::move(*LoadedProgram));
			Resident = std::make_unique<FMesResident>(LoadMesResident(Disc));
			Text.Font = &Resident->Font;
			Text.Tables = &Resident->NameTables;
			Text.ButtonMap = MesKernelButtonMap;

			Context.Program = Program.get();
			Context.GameState = &State;
			Context.Random = &Random;
			for (std::size_t Slot = 0; Slot < 3; ++Slot)
			{
				const uint8 Character = Slot == 1 ? uint8{0} : NoMember;
				Context.PartyCharacters[Slot] = Character;
				Context.Slots[Slot].Occupant = Character;
				Context.Combatants[Slot].Character = Character == NoMember ? uint8{0} : Character;
				Context.Combatants[Slot].Hp = 400;
				Context.Combatants[Slot].MaxHp = 500;
				Context.Waits[Slot] = 30;
				Context.FullWaits[Slot] = 60;
			}
			Hud = std::make_unique<FBattleHud>(*Resources, Text, Vram);
			Hud->Begin(Context, {});
			Context.Party.InputMode = 2;
			Frame();
			Hud->BeginMessageBoxes();
			Context.Party.InputMode = InputBattle;
		}

		/** One frame: the buffer flips, the HUD draws, the input mode steps its counters. */
		const FBattleHudFrame& Frame()
		{
			Hud->Flip();
			Hud->Draw(Drawn);
			REQUIRE(Hud->Fault() == EBattleHudFault::None);
			if (Context.Party.InputMode == InputBattle)
			{
				Hud->StepMenuInput();
			}
			return Drawn;
		}

		FBattleContext Context;
		std::unique_ptr<FBattleHud> Hud;

	private:
		std::unique_ptr<FBattleHudResources> Resources;
		std::unique_ptr<FBattleProgramTables> Program;
		std::unique_ptr<FMesResident> Resident;
		FMesContext Text;
		FGameState State;
		FPsxRandom Random{1};
		FVram Vram;
		FBattleHudFrame Drawn;
	};

	/** The frame's keyed parts by their keys, each key once. */
	std::map<uint32, FMenuPrimitive> Keyed(const FBattleHudFrame& Frame)
	{
		std::map<uint32, FMenuPrimitive> Parts;
		for (const FMenuPrimitive& Primitive : Frame.Primitives)
		{
			if (Primitive.BlendKey != 0)
			{
				CHECK(Parts.emplace(Primitive.BlendKey, Primitive).second);
			}
		}
		return Parts;
	}

	/** The parts of Kind (BattleHudBlendKeys::Kind) in Parts. */
	std::map<uint32, FMenuPrimitive> OfKind(const std::map<uint32, FMenuPrimitive>& Parts, uint32 Kind)
	{
		std::map<uint32, FMenuPrimitive> Of;
		for (const auto& [Key, Primitive] : Parts)
		{
			if ((Key & 0xF0000000u) == BattleHudBlendKeys::Kind(Kind))
			{
				Of.emplace(Key, Primitive);
			}
		}
		return Of;
	}

	/** The furthest any corner of Now's parts moved from Before's of the same keys, and how many of them Before has. */
	std::pair<int32, std::size_t> Moved(const std::map<uint32, FMenuPrimitive>& Before, const std::map<uint32, FMenuPrimitive>& Now)
	{
		int32 Furthest = 0;
		std::size_t Partners = 0;
		for (const auto& [Key, Part] : Now)
		{
			const auto Found = Before.find(Key);
			if (Found == Before.end())
			{
				continue;
			}
			++Partners;
			for (int32 Corner = 0; Corner < Part.PolygonCorners(); ++Corner)
			{
				const FMenuCorner& From = Found->second.Corners[static_cast<std::size_t>(Corner)];
				const FMenuCorner& To = Part.Corners[static_cast<std::size_t>(Corner)];
				Furthest = std::max({Furthest, std::abs(To.X - From.X), std::abs(To.Y - From.Y)});
			}
		}
		return {Furthest, Partners};
	}
}

TEST_CASE("The turn-start icon keeps its blend keys through its flight and onto the command window", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FHudRun Run(*Disc);
	Run.Context.Party.TurnIcons[1] = IconStarting;
	std::map<uint32, FMenuPrimitive> Before;
	int32 Flying = 0;
	for (int32 Frame = 0; Frame < 40 && Run.Context.Party.TurnIcons[1] != IconArrived; ++Frame)
	{
		const std::map<uint32, FMenuPrimitive> Now = OfKind(Keyed(Run.Frame()), 1);
		REQUIRE_FALSE(Now.empty());
		if (!Before.empty())
		{
			const auto [Furthest, Partners] = Moved(Before, Now);
			CHECK(Partners == Now.size());
			CHECK(Furthest > 0);
			CHECK(Furthest <= DisplayReach);
		}
		Before = Now;
		++Flying;
	}
	CHECK(Flying > 10);
	REQUIRE(Run.Context.Party.TurnIcons[1] == IconArrived);
	// At rest over the command window: the same parts, from the flight's last place.
	const std::map<uint32, FMenuPrimitive> Resting = OfKind(Keyed(Run.Frame()), 1);
	const auto [Furthest, Partners] = Moved(Before, Resting);
	CHECK(Partners == Resting.size());
	CHECK(Furthest <= DisplayReach);
}

TEST_CASE("A growing window keeps its parts' blend keys about its centre, and one opened elsewhere takes others", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	FHudRun Run(*Disc);
	constexpr int32 Window = 1;
	Run.Hud->OpenWindow(Window, 0x40, 0x60, 0x80, 0x40, true);
	std::map<uint32, FMenuPrimitive> Before;
	int32 Grown = 0;
	for (int32 Frame = 0; Frame < 30 && !Run.Hud->WindowGrown(Window); ++Frame)
	{
		const std::map<uint32, FMenuPrimitive> Now = OfKind(Keyed(Run.Frame()), 6);
		if (!Before.empty() && !Now.empty())
		{
			const auto [Furthest, Partners] = Moved(Before, Now);
			CHECK(Partners == Now.size());
			CHECK(Furthest <= DisplayReach);
			++Grown;
		}
		if (!Now.empty())
		{
			Before = Now;
		}
	}
	CHECK(Grown > 0);
	REQUIRE(Run.Hud->WindowGrown(Window));
	const std::map<uint32, FMenuPrimitive> Full = OfKind(Keyed(Run.Frame()), 6);
	REQUIRE(Full.size() >= 13);
	CHECK(Moved(Before, Full).second == Full.size());

	Run.Hud->CloseWindow(Window);
	Run.Hud->OpenWindow(Window, 0x90, 0x20, 0x80, 0x40, false);
	const std::map<uint32, FMenuPrimitive> Elsewhere = OfKind(Keyed(Run.Frame()), 6);
	REQUIRE_FALSE(Elsewhere.empty());
	CHECK(Moved(Full, Elsewhere).second == 0);
}
