#include "Battle/BattleRuntime.h"
#include "Battle/Show/BattlePresentation.h"
#include "Battle/Show/IdsBattleSprite.h"
#include "Formats/Tim.h"
#include "Foundation/LittleEndian.h"
#include "Game/BattleGameProgram.h"
#include "Game/CharacterLooks.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameState.h"
#include "Game/IdsPictures.h"
#include "Media/GameFileIndex.h"
#include "Media/PsxExecutable.h"
#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Timing/PsxTiming.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

// The second Citan drawn without his sword: his battle sprite, attack file and Deathblow names are the first
// Citan's, the battle's numbers his own record's as the story has it.
namespace
{
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSize = 0x20;
	constexpr std::size_t ApPerTurn = 0x17;
	constexpr uint16 AllCharacters = 0x7FF;
	constexpr uint8 Citan = 2;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 Heimdal = 3;
	constexpr uint8 SwordHeimdal = 16;
	constexpr uint8 Fenrir = 11;
	/** Citan's battle slot: the party is Fei, Citan, Elly. */
	constexpr std::size_t CitansSlot = 1;
	constexpr std::size_t EllysSlot = 2;
	/** Deathblows 0 to 10 learned. */
	constexpr uint16 CitansDeathblows = 0xFFE0;
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	constexpr uint8 AttackCommand = 1;
	constexpr uint8 GuardCommand = 3;
	constexpr uint8 AttackModeState = 5;
	constexpr uint8 FadingAttackModeState = 100;
	/** In a gear: the ring's Attack and Charge, the attack mode as the menu shows and fades it. */
	constexpr uint8 GearAttackCommand = 0x10;
	constexpr uint8 GearRingEnd = 0x18;
	constexpr uint8 ChargeCommand = 0x12;
	constexpr uint8 GearAttackModeState = 0x19;
	constexpr uint8 FadingGearAttackModeState = 0x65;
	constexpr int32 FrameLimit = 4000;
	constexpr int32 FirstEnemy = 3;
	constexpr int32 Enemies = 2;

	struct FFought
	{
		/** VRAM as Citan's first command ring finds it. */
		std::vector<uint16> FirstVram;
		/** The enemies' HP once Citan's strike has played and a command ring is up again. */
		std::vector<uint32> EnemyHp;
		bool bStruck = false;
		bool bWentOn = false;
		/** Citan's gear at his first command ring: the model worn over its moves, its own skeleton's bones, the parts drawn from each model. */
		bool bDressed = false;
		std::size_t Bones = 0;
		int32 DressParts = 0;
		int32 OwnParts = 0;
		/** Elly's gear at the first command ring: the model worn over its moves, its own skeleton's bones, the parts drawn from the worn model. */
		bool bEllysDressed = false;
		std::size_t EllysBones = 0;
		int32 EllysDressParts = 0;
		/** The striker's Deathblow counters and the enemies' HP at its first command ring, and once its combo closed. */
		std::array<uint16, 7> CountersBefore{};
		std::array<uint16, 7> CountersAfter{};
		std::vector<uint32> EnemyHpBefore;
		/** Once the striker's combo closed: its AP left and the show's records. */
		uint8 ApLeft = 0xFF;
		std::vector<FBattlePerformance> Records;
	};

	/** Elly's gear as the new game has it. */
	constexpr uint8 EllysOwnGear = 0xFF;

	/** One frame of the game loop with a button held through its vertical blanks; its error, if it stopped. */
	using FLoopFrame = std::function<std::optional<FError>(FPadState)>;
	/** What a fight does at its first command ring before it ends there, the loop going on frame by frame. */
	using FAtFirstRing = std::function<void(FGameLoop&, const FLoopFrame&)>;

	/**
	 * The forest's first battle from a new game: Fei, Citan as Record, Elly (in EllysGear where given), in their gears
	 * where bInGears, drawn as Looks has them (Citan without his sword too where bWithoutSword). Citan (or Striker, with
	 * StrikerAp AP a turn where given) strikes Buttons on his first turn and the others guard; with none, the fight ends at
	 * his first command ring, after AtFirstRing where given.
	 */
	FFought Fight(const FPlayedGame& Game, uint8 Record, bool bWithoutSword, std::string_view Buttons, bool bInGears = false, uint8 Gear = SwordHeimdal,
		uint8 EllysGear = EllysOwnGear, FCharacterLooks Looks = {}, std::optional<uint8> Striker = std::nullopt, std::optional<uint8> StrikerAp = std::nullopt,
		const FAtFirstRing& AtFirstRing = {})
	{
		FFought Fought;
		auto Started = Game.Start(0, nullptr);
		REQUIRE(Started);
		FGameSession& Session = **Started;
		FGameState& State = Session.Game();
		State.SetJoinedCharacters(AllCharacters);
		const std::size_t Skills = LearnedSkills + Citan * LearnedSize;
		LittleEndian::WriteU16(State.Block(), Skills, CitansDeathblows);
		State.Block()[Skills + ApPerTurn] = 7;
		if (Striker && StrikerAp)
		{
			State.Block()[LearnedSkills + *Striker * LearnedSize + ApPerTurn] = *StrikerAp;
		}
		if (Record == SwordCitan)
		{
			// As the story makes him and his Heimdal (FE D0, FE E1): the first Citan's record and learned skills, the first
			// Heimdal's record, copied.
			REQUIRE(State.CopyCharacter(Citan, SwordCitan));
			REQUIRE(State.CopyGear(Heimdal, Gear));
			REQUIRE(State.SetCharacterGear(SwordCitan, Gear));
		}
		State.SetParty({0, Record, 1});
		if (EllysGear != EllysOwnGear)
		{
			REQUIRE(State.SetCharacterGear(1, EllysGear));
		}
		if (bWithoutSword)
		{
			REQUIRE(Looks.Switch(SwordCitan));
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
		// Riding as the map has begun, which sets who rides as it enters.
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			State.SetPartySlotInGear(Slot, bInGears ? 1 : 0);
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

		for (int32 Count = 0; Count < FrameLimit && !Fought.bWentOn; ++Count)
		{
			REQUIRE(Loop.Program() == EGameProgram::Battle);
			const FBattleContext& Battle = Loop.Battle()->Runtime().Context();
			const FBattlePartyInput& Party = Battle.Party;
			const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
			const bool bCitan = Battle.PartyCharacters[Party.Actor] == Striker.value_or(Record);
			const bool bRing = Top == EBattlePartyRoutine::Turn
				&& ((Party.MenuState >= 1 && Party.MenuState <= 10 && Party.MenuState != AttackModeState) || (Party.MenuState >= GearAttackCommand && Party.MenuState <= GearRingEnd));
			const bool bAttackMode = Party.MenuState == AttackModeState || Party.MenuState == FadingAttackModeState || Party.MenuState == GearAttackModeState
				|| Party.MenuState == FadingGearAttackModeState;
			if (Fought.FirstVram.empty() && bRing && bCitan)
			{
				Fought.CountersBefore = Battle.Combatants[Party.Actor].DeathblowCounters;
				for (int32 Enemy = FirstEnemy; Enemy < FirstEnemy + Enemies; ++Enemy)
				{
					Fought.EnemyHpBefore.push_back(Battle.Combatants[static_cast<std::size_t>(Enemy)].Hp);
				}
				const FBattleShow& Show = Loop.Battle()->Presentation().Show;
				const std::span<const uint16> Units = Show.Vram.ConsoleUnits();
				Fought.FirstVram.assign(Units.begin(), Units.end());
				const FBattleObject& Ridden = Show.Objects.Slots[CitansSlot].Object;
				Fought.bDressed = Ridden.Dress != nullptr;
				const FBattleObject& Ellys = Show.Objects.Slots[EllysSlot].Object;
				Fought.bEllysDressed = Ellys.Dress != nullptr;
				Fought.EllysBones = Ellys.Bones.size();
				Fought.Bones = Ridden.Bones.size();
				const std::span<const FMdlPart> Dressed = Ridden.Dress != nullptr ? std::span<const FMdlPart>(Ridden.Dress->Model().Model.Parts) : std::span<const FMdlPart>{};
				for (const FBattleDrawnPart& Part : Show.Objects.Frame().Parts)
				{
					if (Part.Slot == EllysSlot && Ellys.Dress != nullptr)
					{
						++Fought.EllysDressParts;
					}
					if (Part.Slot == CitansSlot)
					{
						const bool bFromDress = !Dressed.empty() && Part.Part >= Dressed.data() && Part.Part < Dressed.data() + Dressed.size();
						++(bFromDress ? Fought.DressParts : Fought.OwnParts);
					}
				}
				if (Buttons.empty())
				{
					if (AtFirstRing)
					{
						AtFirstRing(Loop, Frame);
					}
					Fought.bWentOn = true;
					break;
				}
			}
			if (Fought.bStruck && Party.bComboClosed && Fought.Records.empty())
			{
				Fought.ApLeft = Party.ApLeft;
				Fought.CountersAfter = Battle.Combatants[Party.Actor].DeathblowCounters;
				Fought.Records.assign(Battle.Performances.begin(), Battle.Performances.begin() + Battle.PerformanceIndex + 1);
			}
			if (bRing && Fought.bStruck && Party.ButtonCount == 0)
			{
				Fought.bWentOn = true;
				for (int32 Enemy = FirstEnemy; Enemy < FirstEnemy + Enemies; ++Enemy)
				{
					Fought.EnemyHp.push_back(Battle.Combatants[static_cast<std::size_t>(Enemy)].Hp);
				}
				break;
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
					const bool bStrikes = bCitan && !Fought.bStruck;
					const uint8 Wanted = bInGears ? (bStrikes ? GearAttackCommand : ChargeCommand) : bStrikes ? AttackCommand : GuardCommand;
					Held.Buttons = FPadState::Bit(Party.MenuState == Wanted ? EPadButton::Cross : bStrikes ? EPadButton::Right : EPadButton::Left);
				}
				else if (Top == EBattlePartyRoutine::Turn && bAttackMode && bCitan
					&& Party.ButtonCount < Buttons.size())
				{
					const char Button = Buttons[Party.ButtonCount];
					Held.Buttons = FPadState::Bit(Button == 'S' ? EPadButton::Square : Button == 'T' ? EPadButton::Triangle : EPadButton::Cross);
					Fought.bStruck = true;
				}
			}
			if (const auto Error = Frame(Held))
			{
				FAIL("frame " << Count << ": " << Error->Message);
			}
		}
		return Fought;
	}
}

TEST_CASE("The second Citan without his sword fights drawn as the first, the battle going as with his sword", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	// Each in a game of its own: a battle's sound banks stay in the game's sound memory after it.
	const auto Fought = [&Disc](uint8 Record, bool bWithoutSword)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		// □ △ △ ×: the first Citan's Tsunami Ice, the second's Deathblow of the same buttons.
		return Fight(*Opened, Record, bWithoutSword, "STTX");
	};
	const FFought WithSword = Fought(SwordCitan, false);
	const FFought WithoutSword = Fought(SwordCitan, true);
	const FFought First = Fought(Citan, false);
	REQUIRE(WithSword.bWentOn);
	REQUIRE(WithoutSword.bWentOn);
	REQUIRE(First.bWentOn);
	// Drawn as the first Citan, to the texel, where with the sword he is not.
	const bool bDrawnAsFirst = WithoutSword.FirstVram == First.FirstVram;
	CHECK(bDrawnAsFirst);
	const bool bSwordDrawn = WithSword.FirstVram != First.FirstVram;
	CHECK(bSwordDrawn);
	// His strike's numbers are his own record's either way. (The show's animations draw from the game's random numbers, as
	// the original's do, so a battle's later draws can fall otherwise after another animation.)
	CHECK(WithoutSword.EnemyHp == WithSword.EnemyHp);
}

TEST_CASE("In his gear, the second Citan without his sword rides the first Heimdal, which fights with its fists", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Fought = [&Disc](uint8 Record, bool bWithoutSword)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		return Fight(*Opened, Record, bWithoutSword, "", true);
	};
	const FFought WithSword = Fought(SwordCitan, false);
	const FFought WithoutSword = Fought(SwordCitan, true);
	const FFought First = Fought(Citan, false);
	REQUIRE(WithSword.bWentOn);
	REQUIRE(WithoutSword.bWentOn);
	REQUIRE(First.bWentOn);
	const bool bDrawnAsFirst = WithoutSword.FirstVram == First.FirstVram;
	CHECK(bDrawnAsFirst);
	const bool bSwordDrawn = WithSword.FirstVram != First.FirstVram;
	CHECK(bSwordDrawn);
}

TEST_CASE("In Fenrir, the second Citan without his sword fights with the first Heimdal's moves, Fenrir's pieces on them", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Fought = [&Disc](bool bWithoutSword, std::string_view Buttons)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		return Fight(*Opened, SwordCitan, bWithoutSword, Buttons, true, Fenrir);
	};
	// A weak blow: Heimdal's moves played under Fenrir's pieces.
	const FFought WithSword = Fought(false, "S");
	const FFought WithoutSword = Fought(true, "S");
	REQUIRE(WithSword.bWentOn);
	REQUIRE(WithoutSword.bWentOn);
	CHECK(WithSword.bStruck);
	// The blow's numbers are the record's either way.
	CHECK(WithoutSword.EnemyHp == WithSword.EnemyHp);
	// With the sword, Fenrir as the story has it: its own skeleton and pieces.
	CHECK_FALSE(WithSword.bDressed);
	CHECK(WithSword.Bones == 56);
	CHECK(WithSword.OwnParts > 0);
	// Without it, the first Heimdal's skeleton moving and only Fenrir's pieces drawn on it, the sword's strip and one hand of
	// each pair left out.
	CHECK(WithoutSword.bDressed);
	CHECK(WithoutSword.Bones == 45);
	CHECK(WithoutSword.OwnParts == 0);
	CHECK(WithoutSword.DressParts == 21);
}

TEST_CASE("A gear battle with Elly in Regurus begins and comes to its command rings, Regurus worn over Vierge's moves", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	const auto Fought = [&Disc](uint8 Gear)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		return Fight(*Opened, Citan, false, "", true, SwordHeimdal, Gear);
	};
	const FFought InVierge = Fought(Vierge);
	const FFought InRegurus = Fought(Regurus);
	CHECK(InVierge.bWentOn);
	CHECK(InRegurus.bWentOn);
	// Regurus fights as Vierge: Vierge's 70 bones moving, only Regurus' own pieces drawn on them. Vierge as herself.
	CHECK(InRegurus.bEllysDressed);
	CHECK(InRegurus.EllysBones == 70);
	CHECK(InRegurus.EllysDressParts > 0);
	CHECK_FALSE(InVierge.bEllysDressed);
	CHECK(InVierge.EllysBones == 70);
}

namespace
{
	/** Vierge's drones: her hidden pieces, which only Aerods shows and flies. */
	constexpr std::array<uint16, 16> ViergesDrones = {48, 49, 50, 51, 52, 53, 54, 55, 57, 59, 63, 64, 65, 66, 67, 68};

	/** A drone of Vierge's on an object: Elly's gear's or a clone it made of itself, and the drone's bone. */
	using FDrone = std::pair<int32, uint16>;

	/** Aerods as Elly's gear flies it, frame by frame: the drones shown and, worn over by another model, the ones drawn. */
	struct FAerods
	{
		std::vector<std::vector<FDrone>> Shown;
		std::vector<std::vector<FDrone>> Drawn;
		/** A piece textured from the port's rows that is no drone. */
		bool bOtherFromPortRows = false;
	};

	bool IsDrone(uint16 Bone)
	{
		return std::find(ViergesDrones.begin(), ViergesDrones.end(), Bone) != ViergesDrones.end();
	}
}

TEST_CASE("In Regurus, Aerods shows and flies Vierge's own drones as she does, drawn from her pictures in the port's rows", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint8 Vierge = 2;
	constexpr uint8 Regurus = 10;
	/** Vierge's sequence of her special attack, which loads Aerods' extension and plays it. */
	constexpr int32 AerodsSequence = 0x48;
	constexpr int32 AerodsFrames = 400;
	const auto Flown = [&Disc](uint8 Gear)
	{
		FAerods Aerods;
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		const FFought Fought = Fight(*Opened, Citan, false, "", true, SwordHeimdal, Gear, {}, std::nullopt, std::nullopt,
			[&](FGameLoop& Loop, const FLoopFrame& Frame)
			{
				FBattleShow& Show = const_cast<FBattleShow&>(Loop.Battle()->Presentation().Show);
				Show.Objects.Perform(static_cast<int32>(EllysSlot), static_cast<uint16>(1u << FirstEnemy), AerodsSequence);
				for (int32 Count = 0; Count < AerodsFrames; ++Count)
				{
					REQUIRE_FALSE(Frame({}));
					std::vector<FDrone>& Shown = Aerods.Shown.emplace_back();
					for (int32 Slot = 0; Slot < FBattleObjects::StageSlot; ++Slot)
					{
						const FBattleObject* Object = Show.Objects.Object(Slot);
						const bool bEllys = Slot == static_cast<int32>(EllysSlot) || (Slot >= FBattleObjects::FirstClone && Object != nullptr && Object->Owner == EllysSlot);
						for (const uint16 Bone : ViergesDrones)
						{
							if (bEllys && Object != nullptr && Bone < Object->Bones.size() && Object->Bones[Bone].bShown != 0 && Object->Bones[Bone].Part != FGearBone::NoPart)
							{
								Shown.push_back({Slot, Bone});
							}
						}
					}
					// A kept piece numbers after the worn model's bones; Elly's clones wear what she wears (one drawn before the
					// step that let it go may be gone).
					const FBattleObject& Ellys = Show.Objects.Slots[EllysSlot].Object;
					std::vector<FDrone>& Drawn = Aerods.Drawn.emplace_back();
					for (const FBattleDrawnPart& Part : Show.Objects.Frame().Parts)
					{
						if (!Part.bPortRows)
						{
							continue;
						}
						const bool bEllys = Part.Slot == EllysSlot || Part.Slot >= FBattleObjects::FirstClone;
						const uint16 Bone = Ellys.Dress != nullptr ? static_cast<uint16>(Part.Bone - (Ellys.Dress->Model().Skeleton.size() + 1)) : 0;
						Aerods.bOtherFromPortRows = Aerods.bOtherFromPortRows || !bEllys || !IsDrone(Bone);
						Drawn.push_back({Part.Slot, Bone});
					}
				}
			});
		REQUIRE(Fought.bWentOn);
		return Aerods;
	};
	const FAerods AsVierge = Flown(Vierge);
	const FAerods InRegurus = Flown(Regurus);
	// Her sequences fly them alike: the same drones out on her and on her clones, frame by frame from the first one out
	// (the attack is started at the first command ring, which the two battles reach at another step of their frames).
	const auto Out = [](const FAerods& Aerods) {
		return static_cast<std::size_t>(std::find_if(Aerods.Shown.begin(), Aerods.Shown.end(), [](const std::vector<FDrone>& Frame) { return !Frame.empty(); })
			- Aerods.Shown.begin());
	};
	const std::size_t FirstOut = Out(AsVierge);
	const std::size_t FirstOutInRegurus = Out(InRegurus);
	REQUIRE(FirstOut < AsVierge.Shown.size());
	REQUIRE(FirstOutInRegurus < InRegurus.Shown.size());
	const std::size_t Flight = std::min(AsVierge.Shown.size() - FirstOut, InRegurus.Shown.size() - FirstOutInRegurus);
	for (std::size_t Frame = 0; Frame < Flight; ++Frame)
	{
		INFO("frame " << Frame << " of the flight");
		CHECK(InRegurus.Shown[FirstOutInRegurus + Frame] == AsVierge.Shown[FirstOut + Frame]);
	}
	std::vector<uint16> EverShown;
	for (const std::vector<FDrone>& Frame : AsVierge.Shown)
	{
		for (const FDrone& Drone : Frame)
		{
			EverShown.push_back(Drone.second);
		}
	}
	std::sort(EverShown.begin(), EverShown.end());
	EverShown.erase(std::unique(EverShown.begin(), EverShown.end()), EverShown.end());
	CHECK(EverShown == std::vector<uint16>(ViergesDrones.begin(), ViergesDrones.end()));
	// Vierge draws her own pieces; over Regurus each drone drawn is one shown as the frame began (its objects step after
	// they are drawn), and every one of them is drawn in its turn.
	CHECK(AsVierge.Drawn == std::vector<std::vector<FDrone>>(AsVierge.Drawn.size()));
	CHECK_FALSE(InRegurus.bOtherFromPortRows);
	std::vector<uint16> EverDrawn;
	for (std::size_t Frame = 1; Frame < InRegurus.Drawn.size(); ++Frame)
	{
		const std::vector<FDrone>& Before = InRegurus.Shown[Frame - 1];
		for (const FDrone& Drone : InRegurus.Drawn[Frame])
		{
			CHECK(std::find(Before.begin(), Before.end(), Drone) != Before.end());
			EverDrawn.push_back(Drone.second);
		}
	}
	std::sort(EverDrawn.begin(), EverDrawn.end());
	EverDrawn.erase(std::unique(EverDrawn.begin(), EverDrawn.end()), EverDrawn.end());
	CHECK(EverDrawn == EverShown);
}

namespace
{
	/** A colour of Elly's orange: hue 7-43 degrees, strongly coloured, not near black. */
	bool IsOrange(uint16 Colour)
	{
		const int32 R = Colour & 31;
		const int32 G = Colour >> 5 & 31;
		const int32 B = Colour >> 10 & 31;
		const int32 Max = std::max({R, G, B});
		const int32 Min = std::min({R, G, B});
		if (Max <= 1 || Max == Min || Max != R)
		{
			return false;
		}
		const double Hue = 60.0 * (G - B) / (Max - Min);
		return Hue >= 7.2 && Hue <= 43.2 && (Max - Min) * 100 >= Max * 55;
	}
}

TEST_CASE("With purple hair Elly's battle sprite and portrait are drawn without her orange, nothing else changed", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Fought = [&Disc](bool bPurple)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		FCharacterLooks Looks;
		if (bPurple)
		{
			Looks.Switch(FCharacterLooks::Elly);
		}
		return Fight(*Opened, Citan, false, "", false, SwordHeimdal, EllysOwnGear, Looks);
	};
	const FFought Plain = Fought(false);
	const FFought Purple = Fought(true);
	REQUIRE(Plain.bWentOn);
	REQUIRE(Purple.bWentOn);
	REQUIRE(Plain.FirstVram.size() == Purple.FirstVram.size());
	// Where VRAM differs it holds her colours: orange in the plain battle, never in hers (her sprite's and her portrait's CLUTs).
	int32 Changed = 0;
	int32 StillOrange = 0;
	int32 WasNotOrange = 0;
	for (std::size_t At = 0; At < Plain.FirstVram.size(); ++At)
	{
		if (Plain.FirstVram[At] != Purple.FirstVram[At])
		{
			++Changed;
			StillOrange += IsOrange(Purple.FirstVram[At]) ? 1 : 0;
			WasNotOrange += IsOrange(Plain.FirstVram[At]) ? 0 : 1;
		}
	}
	CHECK(Changed > 20);
	CHECK(StillOrange == 0);
	// A portrait's pixels moved to another entry beside its recoloured ones are indices, not colours.
	CHECK(WasNotOrange * 4 < Changed);
}

TEST_CASE("As Id, Fei's battle portrait is Id's small face and his sprite Id's, nothing else changed", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	const auto Fought = [&Disc](bool bId)
	{
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		FCharacterLooks Looks;
		if (bId)
		{
			Looks.Switch(FCharacterLooks::Fei);
		}
		return Fight(*Opened, Citan, false, "", false, SwordHeimdal, EllysOwnGear, Looks);
	};
	const FFought Plain = Fought(false);
	const FFought AsId = Fought(true);
	REQUIRE(Plain.bWentOn);
	REQUIRE(AsId.bWentOn);
	REQUIRE(Plain.FirstVram.size() == AsId.FirstVram.size());

	const auto Executable = ReadBootExecutable(*Disc);
	REQUIRE(Executable);
	const auto Index = FGameFileIndex::Read(*Executable);
	REQUIRE(Index);
	const TResult<std::vector<uint8>> Face = IdsPictures::ReadFace(Disc->Image(), *Index);
	REQUIRE(Face);
	const TResult<FTim> SmallFace = IdsPictures::SmallFace(*Face);
	REQUIRE(SmallFace);
	// What changed is Id's small face, its pixels and its CLUT, where the battle puts Fei's portrait.
	REQUIRE(Plain.FirstVram != AsId.FirstVram);
	const auto Block = [&AsId](int32 X, int32 Y, int32 Width, int32 Height)
	{
		std::vector<uint16> Units;
		for (int32 Row = Y; Row < Y + Height; ++Row)
		{
			const auto Start = AsId.FirstVram.begin() + Row * 1024 + X;
			Units.insert(Units.end(), Start, Start + Width);
		}
		return Units;
	};
	int32 FaceX = -1;
	int32 FaceY = -1;
	for (int32 Y = 0; Y + 24 <= 512 && FaceX < 0; ++Y)
	{
		for (int32 X = 0; X + 12 <= 1024; ++X)
		{
			if (Block(X, Y, 12, 24) == SmallFace->Image.Units)
			{
				FaceX = X;
				FaceY = Y;
				break;
			}
		}
	}
	REQUIRE(FaceX >= 0);
	int32 ClutRow = -1;
	for (int32 Y = 0; Y < 512 && ClutRow < 0; ++Y)
	{
		for (int32 X = 0; X + 256 <= 1024; X += 16)
		{
			if (Block(X, Y, 256, 1) == SmallFace->Clut->Units)
			{
				ClutRow = Y;
				break;
			}
		}
	}
	REQUIRE(ClutRow >= 0);
	// His sprite is Id's, resident in the port's rows: its colours on Fei's slot's CLUT row, nothing streamed into Fei's
	// place among the party's textures.
	const TResult<FIdsBattleSprite> IdsSprite = FIdsBattleSprite::Load(Disc->Image(), *Index);
	REQUIRE(IdsSprite);
	const FSpriteFile& Sprite = IdsSprite->File().Sprite();
	constexpr int32 SlotClutRow = 0x1C0;
	constexpr int32 PartyTexturesX = 0x100;
	constexpr int32 PartyTexturesWidth = 0x40;
	for (int32 Colour = 0; Colour < 16 * Sprite.PalettesPerSet(Sprite.Tables()); ++Colour)
	{
		CHECK(AsId.FirstVram[static_cast<std::size_t>(SlotClutRow * 1024 + Colour)] == Sprite.Word(Sprite.PaletteSet(Sprite.Tables(), 0) + 2 * static_cast<uint32>(Colour)));
	}
	int32 Elsewhere = 0;
	for (std::size_t At = 0; At < Plain.FirstVram.size(); ++At)
	{
		const int32 X = static_cast<int32>(At % 1024);
		const int32 Y = static_cast<int32>(At / 1024);
		const bool bFace = X >= FaceX && X < FaceX + 12 && Y >= FaceY && Y < FaceY + 24;
		const bool bSprite = Y == SlotClutRow || (X >= PartyTexturesX && X < PartyTexturesX + PartyTexturesWidth && Y > SlotClutRow);
		Elsewhere += Plain.FirstVram[At] != AsId.FirstVram[At] && !bFace && Y != ClutRow && !bSprite ? 1 : 0;
	}
	CHECK(Elsewhere == 0);
}

TEST_CASE("As Id, one press of Fei's is a whole combo of his AP per turn, his own presses' hits played as one of Id's", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	constexpr uint8 Fei = FCharacterLooks::Fei;
	constexpr uint8 Approach = 0xFD;
	constexpr uint8 Reaction = 0xF3;
	constexpr uint8 TurnEnd = 0xFE;
	constexpr uint8 IdsQuickCombo = 0x1D;
	constexpr uint8 IdsBigCombo = 0x1F;
	FCharacterLooks Looks;
	Looks.Switch(Fei);
	// Triangle, the Weak button: at 7 AP four Weak hits (basic attacks 0 to 3) and the Fierce press of Deathblow 7 (not
	// learned yet), at a new game's 3 AP three Weak hits, both played as Id's quick combo. Cross, the Fierce button, at 6
	// AP: a Fierce hit (basic attack 6) and Deathblow 6, his big combo.
	struct FCase
	{
		uint8 Ap;
		std::string_view Button;
		uint8 CountersUp;
		uint8 Animation;
	};
	for (const FCase& Case : {FCase{7, "T", 0x0F, IdsQuickCombo}, FCase{3, "T", 0x07, IdsQuickCombo}, FCase{6, "X", 0x40, IdsBigCombo}})
	{
		INFO("AP per turn " << static_cast<int32>(Case.Ap));
		auto Opened = FPlayedGame::Open(Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		const FFought AsId = Fight(*Opened, Citan, false, Case.Button, false, SwordHeimdal, EllysOwnGear, Looks, Fei, Case.Ap);
		REQUIRE(AsId.bWentOn);
		REQUIRE_FALSE(AsId.Records.empty());
		// The press took all the turn's AP.
		CHECK(AsId.ApLeft == 0);
		// Fei's basic hits, each counted toward his Deathblows as his own, and no other.
		for (std::size_t Counter = 0; Counter < AsId.CountersBefore.size(); ++Counter)
		{
			INFO("counter " << Counter);
			CHECK((AsId.CountersAfter[Counter] > AsId.CountersBefore[Counter]) == ((Case.CountersUp >> Counter & 1) != 0));
		}
		// One record played for them all, by Id's quick combo: after the approach (and a reaction), before the turn's end.
		std::vector<uint8> Codes;
		int32 Joined = -1;
		for (std::size_t At = 0; At < AsId.Records.size(); ++At)
		{
			const FBattlePerformance& Record = AsId.Records[At];
			Codes.push_back(Record.Code);
			if (Record.IdsAnimation != 0xFF)
			{
				CHECK(Joined < 0);
				Joined = static_cast<int32>(At);
			}
		}
		REQUIRE(Joined > 0);
		CHECK(AsId.Records[static_cast<std::size_t>(Joined)].IdsAnimation == Case.Animation);
		CHECK(Codes.front() == Approach);
		CHECK(Codes.back() == TurnEnd);
		CHECK(static_cast<std::size_t>(Joined) + 2 == Codes.size());
		for (std::size_t At = 1; At < static_cast<std::size_t>(Joined); ++At)
		{
			CHECK(Codes[At] == Reaction);
		}
		// It shows what the five hits did: the HP its target lost.
		const FBattlePerformance& Record = AsId.Records[static_cast<std::size_t>(Joined)];
		uint32 Lost = 0;
		uint32 Shown = 0;
		for (int32 Enemy = 0; Enemy < Enemies; ++Enemy)
		{
			Lost += AsId.EnemyHpBefore[static_cast<std::size_t>(Enemy)] - AsId.EnemyHp[static_cast<std::size_t>(Enemy)];
			Shown += Record.Results[static_cast<std::size_t>(FirstEnemy + Enemy)] == 0 ? Record.WholeAmounts[static_cast<std::size_t>(FirstEnemy + Enemy)] : 0;
		}
		CHECK(Lost > 0);
		CHECK(Shown >= Lost);
	}
}
