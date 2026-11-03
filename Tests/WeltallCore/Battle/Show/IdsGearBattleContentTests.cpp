#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattlePresentation.h"
#include "Game/BattleGameProgram.h"
#include "Game/CharacterLooks.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameState.h"
#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Timing/PsxTiming.h"

#include <catch_amalgamated.hpp>

#include <optional>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

// Fei as Id in his gear: drawn as Id's red gear over his gear's own moves, and in Xenogears given True Id.
namespace
{
	constexpr uint16 AllCharacters = 0x7FF;
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	constexpr int32 FrameLimit = 4000;
	constexpr int32 FeisSlot = 0;
	/** The gear ring's states: Attack, Charge, Special; the Special list's second entry, Xenogears' Infinity. */
	constexpr uint8 GearAttackState = 0x10;
	constexpr uint8 GearRingEnd = 0x18;
	constexpr uint8 ChargeState = 0x12;
	constexpr uint8 SpecialState = 0x18;
	constexpr uint8 InfinitySpecial = 1;
	constexpr uint8 InfinityLevel = 4;
	constexpr uint8 SpecialsCommand = 37;

	struct FRidden
	{
		/** At Fei's first command ring: his object's dress, its own skeleton's bones, the parts drawn from each model. */
		bool bDressed = false;
		std::size_t Bones = 0;
		int32 DressParts = 0;
		int32 OwnParts = 0;
		/** Whether the second Special was offered as True Id, its cost, and Fei's attack level and fuel before and after it. */
		bool bTrueIdOffered = false;
		uint16 TrueIdCost = 0xFFFF;
		uint16 FuelBefore = 0;
		uint16 FuelAfter = 0;
		uint8 LevelAfter = 0;
		bool bRefused = false;
	};

	/**
	 * The forest's first battle from a new game, Fei, Elly and Citan in their gears, Fei's gear Gear (Weltall's record
	 * copied to it), Fei drawn as Id where bId. With bTrueId Fei chooses his gear's second Special on his first turn and
	 * the battle runs until his attack level is Infinity or the Special was refused; the others charge.
	 */
	FRidden Ride(const FIso9660& Disc, uint8 Gear, bool bId, bool bTrueId)
	{
		// Each in a game of its own: a battle's sound banks stay in the game's sound memory after it.
		auto Opened = FPlayedGame::Open(&Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		const FPlayedGame& Game = *Opened;
		FRidden Ridden;
		auto Started = Game.Start(0, nullptr);
		REQUIRE(Started);
		FGameSession& Session = **Started;
		FGameState& State = Session.Game();
		State.SetJoinedCharacters(AllCharacters);
		if (Gear != FCharacterLooks::Weltall)
		{
			REQUIRE(State.CopyGear(FCharacterLooks::Weltall, Gear));
			REQUIRE(State.SetCharacterGear(FCharacterLooks::Fei, Gear));
		}
		State.SetParty({0, 2, 1});
		FCharacterLooks Looks;
		if (bId)
		{
			REQUIRE(Looks.Switch(FCharacterLooks::Fei));
		}
		State.SetCharacterLooks(Looks);

		auto Looped = FGameLoop::EnterField(Session, ForestMap);
		REQUIRE(Looped);
		FGameLoop& Loop = **Looped;
		const auto Frame = [&](FPadState Held) {
			const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop.VblanksPerFrame()), Held);
			Game.Unheard->Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
			return Loop.Frame(PerVblank);
		};
		for (int32 Waited = 0; Waited < 100 && !(Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning); ++Waited)
		{
			REQUIRE_FALSE(Frame({}));
		}
		REQUIRE(Loop.Field() != nullptr);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			State.SetPartySlotInGear(Slot, 1);
		}
		FEventContext& Context = Loop.Field()->Runtime().Context();
		Context.bBattleRequested = true;
		Context.Requests.bScriptedBattle = true;
		Context.Requests.bLeavingField = true;
		Context.Requests.BattleKind = 0;
		Context.Encounters.Formation = ForestFormation;
		for (int32 Waited = 0; Waited < 8 && Loop.Program() != EGameProgram::Battle; ++Waited)
		{
			REQUIRE_FALSE(Frame({}));
		}
		REQUIRE(Loop.Program() == EGameProgram::Battle);

		bool bSeen = false;
		bool bChosen = false;
		for (int32 Count = 0; Count < FrameLimit; ++Count)
		{
			REQUIRE(Loop.Program() == EGameProgram::Battle);
			const FBattleContext& Battle = Loop.Battle()->Runtime().Context();
			const FBattlePartyInput& Party = Battle.Party;
			const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
			const bool bFei = Party.Actor == FeisSlot;
			const bool bRing = Top == EBattlePartyRoutine::Turn && Party.MenuState >= GearAttackState && Party.MenuState <= GearRingEnd;
			const FBattleCombatant& Fei = Battle.Combatants[FeisSlot];
			if (!bSeen && bRing && bFei)
			{
				bSeen = true;
				const FBattleObject& Object = Loop.Battle()->Presentation().Show.Objects.Slots[FeisSlot].Object;
				Ridden.bDressed = Object.Dress != nullptr;
				Ridden.Bones = Object.Bones.size();
				const std::span<const FMdlPart> Worn = Object.Dress != nullptr ? std::span<const FMdlPart>(Object.Dress->Model().Model.Parts) : std::span<const FMdlPart>{};
				for (const FBattleDrawnPart& Part : Loop.Battle()->Presentation().Show.Objects.Frame().Parts)
				{
					if (Part.Slot == FeisSlot)
					{
						const bool bFromDress = !Worn.empty() && Part.Part >= Worn.data() && Part.Part < Worn.data() + Worn.size();
						++(bFromDress ? Ridden.DressParts : Ridden.OwnParts);
					}
				}
				Ridden.bTrueIdOffered = Battle.IsTrueId(FeisSlot, InfinitySpecial);
				Ridden.TrueIdCost = Battle.Commands[FBattleContext::GearCommandIndex(FeisSlot, SpecialsCommand + InfinitySpecial)].FuelCost;
				Ridden.FuelBefore = Fei.GearPart.Fuel;
				if (!bTrueId)
				{
					return Ridden;
				}
			}
			if (bChosen && Fei.AttackLevel == InfinityLevel)
			{
				Ridden.LevelAfter = Fei.AttackLevel;
				Ridden.FuelAfter = Fei.GearPart.Fuel;
				return Ridden;
			}
			FPadState Held;
			if ((Count & 1) == 0)
			{
				if (Party.InputMode == 2 || !Loop.Battle()->Runtime().IsFought())
				{
					Held.Buttons = FPadState::Bit(EPadButton::Cross);
				}
				else if (bRing)
				{
					const uint8 Wanted = bFei && !bChosen ? SpecialState : ChargeState;
					Held.Buttons = FPadState::Bit(Party.MenuState == Wanted ? EPadButton::Cross : bFei && !bChosen ? EPadButton::Up : EPadButton::Left);
				}
				else if (Top == EBattlePartyRoutine::SpecialMenu && bFei)
				{
					if (Party.List.Cell == InfinitySpecial && bChosen)
					{
						// Confirmed already and still in the list: the Special was refused.
						Ridden.bRefused = true;
						Ridden.FuelAfter = Fei.GearPart.Fuel;
						return Ridden;
					}
					Held.Buttons = FPadState::Bit(Party.List.Cell == InfinitySpecial ? EPadButton::Cross : EPadButton::Down);
					bChosen = bChosen || Party.List.Cell == InfinitySpecial;
				}
				else if (Top == EBattlePartyRoutine::SelectTarget)
				{
					Held.Buttons = FPadState::Bit(EPadButton::Cross);
				}
			}
			if (const auto Error = Frame(Held))
			{
				FAIL("frame " << Count << ": " << Error->Message);
			}
		}
		FAIL("the battle ran out of frames");
		return Ridden;
	}
}

TEST_CASE("As Id, Fei's gear fights as itself with Id's red gear worn over it, only the red gear's pieces drawn", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Weltall's 52 bones, Weltall-2's 83, Xenogears' 71: the gear's own skeleton moves under the red gear's pieces.
	for (const auto [Gear, Bones] : {std::pair<uint8, std::size_t>{FCharacterLooks::Weltall, 52}, {FCharacterLooks::Weltall2, 83}, {FCharacterLooks::Xenogears, 71}})
	{
		INFO("gear " << int32(Gear));
		const FRidden Id = Ride(*Disc, Gear, true, false);
		CHECK(Id.bDressed);
		CHECK(Id.Bones == Bones);
		CHECK(Id.DressParts > 10);
		CHECK(Id.OwnParts == 0);
		const FRidden Fei = Ride(*Disc, Gear, false, false);
		CHECK_FALSE(Fei.bDressed);
		CHECK(Fei.DressParts == 0);
		CHECK(Fei.OwnParts > 10);
	}
}

TEST_CASE("As Id in Xenogears, Fei has True Id: the Infinity Xenogears loses, at no fuel", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const FRidden Id = Ride(*Disc, FCharacterLooks::Xenogears, true, true);
	CHECK(Id.bTrueIdOffered);
	CHECK(Id.TrueIdCost == 0);
	CHECK_FALSE(Id.bRefused);
	CHECK(Id.LevelAfter == InfinityLevel);
	CHECK(Id.FuelAfter == Id.FuelBefore);
	// As Fei, Xenogears' second Special is taken at the battle's start, as in the original; Weltall-2 keeps System Id
	// as the story gives it, with no True Id.
	const FRidden Fei = Ride(*Disc, FCharacterLooks::Xenogears, false, true);
	CHECK_FALSE(Fei.bTrueIdOffered);
	CHECK(Fei.bRefused);
	const FRidden Weltall2 = Ride(*Disc, FCharacterLooks::Weltall2, true, false);
	CHECK_FALSE(Weltall2.bTrueIdOffered);
	CHECK(Weltall2.TrueIdCost == 1000);
}
