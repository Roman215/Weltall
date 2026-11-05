#include "Battle/BattleRuntime.h"
#include "Battle/BattleScene.h"
#include "Battle/Show/BattlePresentation.h"
#include "Foundation/LittleEndian.h"
#include "Game/BattleGameProgram.h"
#include "Game/CharacterLooks.h"
#include "Game/FieldProgram.h"
#include "Game/GameLoop.h"
#include "Game/GameState.h"
#include "Game/KernelState.h"
#include "Support/GameDiscs.h"
#include "Support/PlayedGame.h"
#include "Timing/PsxTiming.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace Weltall;
using namespace Weltall::Testing;

// Through the whole game loop with the real show: each character performs every Deathblow and Ether it can learn, on foot
// and in its gears, one a turn, battle after battle until all have played to their end; and the enemies of the story's
// battles, its bosses among them, act through their scripts while the party guards.
namespace
{
	constexpr std::size_t CharacterRecords = 0x26C;
	constexpr std::size_t CharacterSize = 0xA4;
	constexpr std::size_t HpField = 0x4C;
	constexpr std::size_t EpField = 0x50;
	constexpr std::size_t AgilityField = 0x5A;
	/** The status word, whose bit 15 is KO. */
	constexpr std::size_t StatusField = 0x7C;
	constexpr uint16 KnockedOut = 0x8000;
	constexpr std::size_t GearField = 0xA0;
	constexpr std::size_t GearRecords = 0x978;
	constexpr std::size_t GearSize = 0xA4;
	constexpr std::size_t GearFuelField = 0x38;
	constexpr std::size_t GearHpField = 0x60;
	constexpr std::size_t GearAgilityField = 0x98;
	/** A gear's attack: its weapons' attacks and its output, which its multiplier scales. */
	constexpr std::array<std::size_t, 4> GearAttackFields = {0x12, 0x1A, 0x22, 0x3C};
	constexpr std::size_t LearnedSkills = 0x16C0;
	constexpr std::size_t LearnedSize = 0x20;
	constexpr std::size_t DeathblowsWord = 0x00;
	constexpr std::size_t AbilitiesWord = 0x02;
	constexpr std::size_t GearDeathblowsWord = 0x04;
	constexpr std::size_t GearAbilitiesWord = 0x06;
	constexpr std::size_t ApPerTurn = 0x17;
	/** The game's flags word whose bit 14 opens the Deathblows of seven AP. */
	constexpr std::size_t DeathblowTierWord = 0x22B6;
	constexpr uint16 SevenApTier = 0x4000;
	constexpr uint16 AllCharacters = 0x7FF;
	constexpr uint16 Plenty = 9999;
	/** The most EP anyone has (the items that raise it stop there). */
	constexpr uint16 HighestEp = 99;
	constexpr uint16 PlentyOfFuel = 9900;
	constexpr uint32 PlentyForAGear = 99999;
	/** Agility from 19 up gives the shortest wait, 2 to 9 steps: the striker acts several times between the enemies' turns. */
	constexpr uint8 Quickest = 20;
	/** A party slot left empty. */
	constexpr uint8 NoMember = 0xFF;
	/** The characters, numbered as the game's records. */
	constexpr uint8 Fei = 0;
	constexpr uint8 Elly = 1;
	constexpr uint8 Citan = 2;
	constexpr uint8 Bart = 3;
	constexpr uint8 Billy = 4;
	constexpr uint8 Rico = 5;
	constexpr uint8 Emeralda = 6;
	constexpr uint8 ChuChu = 7;
	constexpr uint8 Maria = 8;
	constexpr uint8 SwordCitan = 9;
	constexpr uint8 GrownEmeralda = 10;
	/** The gear the grown Emeralda pilots. */
	constexpr uint8 Crescens = 9;
	/** The forest's second formation: two enemies on foot; a corridor walked in gears, its fourth: two gears. */
	constexpr uint16 ForestMap = 15;
	constexpr uint8 ForestFormation = 1;
	constexpr uint16 GearsMap = 414;
	constexpr uint8 GearsFormation = 3;
	/**
	 * The party turn's current attack is its command + 1: on foot Deathblow n is command 7 + n and ability n command
	 * 22 + n; in a gear the commands are the gear's records (the Deathblows 3-11, ether n 21 + n).
	 */
	constexpr uint8 FirstDeathblowAttack = 8;
	constexpr uint8 FirstAbilityAttack = 0x17;
	constexpr uint8 FirstGearAbilityAttack = 0x16;
	/** The rings' commands on foot and in a gear, and the attack modes as the menu shows them and fades them out. */
	constexpr uint8 AttackCommand = 1;
	constexpr uint8 GuardCommand = 3;
	constexpr uint8 EtherCommand = 4;
	constexpr uint8 AttackModeState = 5;
	constexpr uint8 FadingAttackModeState = 100;
	constexpr uint8 GearAttackCommand = 0x10;
	constexpr uint8 ChargeCommand = 0x12;
	constexpr uint8 GearEtherCommand = 0x13;
	constexpr uint8 GearRingEnd = 0x18;
	constexpr uint8 GearAttackModeState = 0x19;
	constexpr uint8 FadingGearAttackModeState = 0x65;
	constexpr uint8 EtherList = 1;
	/** Frames one move may take from its member's first command ring to the ring after it played (a gear's Deathblow waits for its level). */
	constexpr int32 MoveFrameLimit = 12000;

	/**
	 * Each Deathblow's presses (W Weak, S Strong, F Fierce), the last finishing its pattern: the whole turn's presses before
	 * a Fierce one name the Deathblow it makes.
	 */
	constexpr std::array<std::string_view, 13> DeathblowPresses = {"WF", "WWF", "SF", "WWWF", "WSF", "SWF", "FF", "WWWWF", "WWSF", "WSWF", "SWWF", "SSF", "FWF"};

	enum class EMove : uint8
	{
		Deathblow,
		Ether,
		/** A gear's Deathblow by its record, 3-11: three for each attack level. */
		GearDeathblow,
		GearEther,
	};

	struct FMove
	{
		EMove Kind = EMove::Deathblow;
		uint8 Number = 0;

		bool InGear() const { return Kind == EMove::GearDeathblow || Kind == EMove::GearEther; }
		bool IsEther() const { return Kind == EMove::Ether || Kind == EMove::GearEther; }

		/** The party turn's current attack once the move is performed. */
		uint8 Attack() const
		{
			switch (Kind)
			{
			case EMove::Deathblow:
				return static_cast<uint8>(FirstDeathblowAttack + Number);
			case EMove::Ether:
				return static_cast<uint8>(FirstAbilityAttack + Number);
			case EMove::GearDeathblow:
				return static_cast<uint8>(Number + 1);
			case EMove::GearEther:
				return static_cast<uint8>(FirstGearAbilityAttack + Number);
			}
			return 0;
		}
	};

	struct FMoveOutcome
	{
		FMove Move;
		/** The battle took the move: the turn's attack became it. */
		bool bPerformed = false;
		/** Its playback ended: a command ring came up again after its turn, or the battle was won. */
		bool bPlayed = false;
		/** The game loop's error, where it stopped. */
		std::string Stop;
	};

	FMoveOutcome Begun(const FMove& Move)
	{
		FMoveOutcome Outcome;
		Outcome.Move = Move;
		return Outcome;
	}

	/** A member who performs Moves, drawn as Looks has it, riding Gear where the moves are a gear's. */
	struct FStriker
	{
		uint8 Record = 0;
		FCharacterLooks Looks;
		std::vector<FMove> Moves;
		std::optional<uint8> Gear;
	};

	/** The first Deathblows and abilities, as the learned skills' words keep them (bit 15 the first). */
	uint16 FirstBits(int32 Count)
	{
		return static_cast<uint16>(0xFFFF0000u >> Count);
	}

	/** Every gear Deathblow learned: records 3-11, bit 15 record 0. */
	constexpr uint16 GearDeathblowBits = 0x1FF0;

	std::vector<FMove> Moves(EMove Kind, int32 Count, int32 First = 0)
	{
		std::vector<FMove> All;
		for (int32 Number = First; Number < First + Count; ++Number)
		{
			All.push_back({Kind, static_cast<uint8>(Number)});
		}
		return All;
	}

	std::vector<FMove> OnFoot(int32 Deathblows, int32 Ethers)
	{
		std::vector<FMove> All = Moves(EMove::Deathblow, Deathblows);
		const std::vector<FMove> Abilities = Moves(EMove::Ether, Ethers);
		All.insert(All.end(), Abilities.begin(), Abilities.end());
		return All;
	}

	std::string Describe(const FMove& Move)
	{
		constexpr std::array<std::string_view, 4> Names = {"Deathblow ", "Ether ", "gear Deathblow ", "gear Ether "};
		return std::string(Names[static_cast<std::size_t>(Move.Kind)]) + std::to_string(Move.Number);
	}

	/** On foot, the member beside the striker: fallen before each battle, the target the Ethers that revive need. */
	uint8 FallenAlly(uint8 Record)
	{
		return Record == Fei ? Elly : Fei;
	}

	std::size_t CharacterRecord(uint8 Record)
	{
		return CharacterRecords + Record * CharacterSize;
	}

	std::size_t GearRecord(const FGameState& State, uint8 Record)
	{
		return GearRecords + State.Block()[CharacterRecord(Record) + GearField] * GearSize;
	}

	/** The striker acts often (as a quick character or gear), with every move it performs learned, drawn as its looks have it. */
	void SetUp(FGameState& State, const FStriker& Striker, bool bInGears)
	{
		State.SetJoinedCharacters(AllCharacters);
		if (Striker.Record == SwordCitan)
		{
			REQUIRE(State.CopyCharacter(Citan, SwordCitan));
		}
		// The grown Emeralda's record of a new game names Yggdra, whose rules disable her Ether; she pilots Crescens.
		const std::optional<uint8> Gear = Striker.Record == GrownEmeralda ? Striker.Gear.value_or(Crescens) : Striker.Gear;
		if (Gear)
		{
			REQUIRE(State.SetCharacterGear(Striker.Record, *Gear));
		}
		State.SetParty({Striker.Record, bInGears ? NoMember : FallenAlly(Striker.Record), NoMember});
		State.Block()[CharacterRecord(Striker.Record) + AgilityField] = Quickest;
		State.Block()[GearRecord(State, Striker.Record) + GearAgilityField] = Quickest;
		if (bInGears)
		{
			// Hits that do no damage kill nobody: the attack level a gear's Deathblows need is built up in one battle.
			for (const std::size_t Field : GearAttackFields)
			{
				State.Block()[GearRecord(State, Striker.Record) + Field] = 0;
			}
		}
		int32 Deathblows = 0;
		int32 Ethers = 0;
		int32 GearEthers = 0;
		for (const FMove& Move : Striker.Moves)
		{
			int32& Count = Move.Kind == EMove::Deathblow ? Deathblows : Move.Kind == EMove::Ether ? Ethers : GearEthers;
			Count = Move.Kind == EMove::GearDeathblow ? Count : std::max(Count, Move.Number + 1);
		}
		const std::size_t Skills = LearnedSkills + Striker.Record * LearnedSize;
		LittleEndian::WriteU16(State.Block(), Skills + DeathblowsWord, FirstBits(Deathblows));
		LittleEndian::WriteU16(State.Block(), Skills + AbilitiesWord, FirstBits(Ethers));
		LittleEndian::WriteU16(State.Block(), Skills + GearDeathblowsWord, GearDeathblowBits);
		LittleEndian::WriteU16(State.Block(), Skills + GearAbilitiesWord, FirstBits(GearEthers));
		State.Block()[Skills + ApPerTurn] = 7;
		LittleEndian::WriteU16(State.Block(), DeathblowTierWord, static_cast<uint16>(LittleEndian::ReadU16(State.Block(), DeathblowTierWord) | SevenApTier));
		State.SetCharacterLooks(Striker.Looks);
	}

	/** Before each battle a member has its HP and EP full and its gear's HP and fuel in plenty. */
	void Refill(FGameState& State, uint8 Member)
	{
		const std::size_t Record = CharacterRecord(Member);
		for (const std::size_t Field : {HpField, EpField})
		{
			const uint16 Full = Field == HpField ? Plenty : HighestEp;
			LittleEndian::WriteU16(State.Block(), Record + Field, Full);
			LittleEndian::WriteU16(State.Block(), Record + Field + 2, Full);
		}
		const std::size_t Gear = GearRecord(State, Member);
		LittleEndian::WriteU16(State.Block(), Gear + GearFuelField, PlentyOfFuel);
		LittleEndian::WriteU16(State.Block(), Gear + GearFuelField + 2, PlentyOfFuel);
		LittleEndian::WriteU32(State.Block(), Gear + GearHpField, PlentyForAGear);
		LittleEndian::WriteU32(State.Block(), Gear + GearHpField + 4, PlentyForAGear);
	}

	void KnockOut(FGameState& State, uint8 Member)
	{
		const std::size_t Record = CharacterRecord(Member);
		LittleEndian::WriteU16(State.Block(), Record + HpField, 0);
		LittleEndian::WriteU16(State.Block(), Record + StatusField, static_cast<uint16>(LittleEndian::ReadU16(State.Block(), Record + StatusField) | KnockedOut));
	}

	FPadState Press(EPadButton Button)
	{
		FPadState Held;
		Held.Buttons = FPadState::Bit(Button);
		return Held;
	}

	/** The pad's button for an attack press, in the USA layout: Weak (a gear's first button), Strong, Fierce. */
	EPadButton AttackButton(char Press)
	{
		return Press == 'W' ? EPadButton::Triangle : Press == 'S' ? EPadButton::Square : EPadButton::Cross;
	}

	/** The way to press on the rings toward Command: each way reaches its command from anywhere on the two rings. */
	EPadButton TowardCommand(uint8 Command)
	{
		switch (Command)
		{
		case AttackCommand:
		case GearAttackCommand:
			return EPadButton::Right;
		case EtherCommand:
		case GearEtherCommand:
			return EPadButton::Up;
		default:
			return EPadButton::Left;
		}
	}

	/**
	 * In a gear's attack mode, the press toward Deathblow Record: a first press of the level's button once the turn
	 * began at that attack level, then the Deathblow's own; before, the first button alone, which raises the level.
	 */
	EPadButton GearDeathblowPress(uint8 Record, uint8 Level, uint8 Pressed)
	{
		constexpr std::string_view Buttons = "WSF";
		const uint8 Needed = static_cast<uint8>(Record / 3);
		if (Level < Needed)
		{
			return Pressed == 0 ? AttackButton('W') : EPadButton::Circle;
		}
		return Pressed == 0 ? AttackButton(Buttons[Needed - 1]) : Pressed == 1 ? AttackButton(Buttons[Record % 3]) : EPadButton::Circle;
	}

	/**
	 * Plays the striker's moves in order, one a turn: battle after battle from the forest's formation on foot or the gear
	 * corridor's in gears, and a new session after a stop, so that every move is tried.
	 */
	std::vector<FMoveOutcome> PlayEveryMove(const FPlayedGame& Game, const FStriker& Striker)
	{
		const bool bInGears = Striker.Moves.front().InGear();
		std::vector<FMoveOutcome> Outcomes;
		while (Outcomes.size() < Striker.Moves.size())
		{
			// A sound of the session's own: a battle left mid-fight keeps its banks in the sound's memory.
			auto Sound = FGameSoundSession::Start(*Game.Discs[0].Iso);
			REQUIRE(Sound);
			FUnheardGameSound Unheard(*(*Sound)->Sound());
			auto Started = Game.Start(0, nullptr, Sound->get());
			REQUIRE(Started);
			FGameSession& Session = **Started;
			FGameState& State = Session.Game();
			SetUp(State, Striker, bInGears);
			const uint16 Map = bInGears ? GearsMap : ForestMap;
			auto Looped = FGameLoop::EnterField(Session, Map);
			REQUIRE(Looped);
			FGameLoop& Loop = **Looped;
			const auto Frame = [&](FPadState Held) {
				const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop.VblanksPerFrame()), Held);
				Unheard.Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
				return Loop.Frame(PerVblank);
			};

			FMoveOutcome Outcome = Begun(Striker.Moves[Outcomes.size()]);
			bool bStarted = false;
			bool bTurnEnded = false;
			bool bBattleRequested = false;
			int32 MoveFrames = 0;
			for (int32 Count = 0;; ++Count)
			{
				FPadState Held;
				const bool bPressFrame = (Count & 1) == 0;
				if (Loop.Program() == EGameProgram::Field)
				{
					if (Outcome.bPerformed && bBattleRequested)
					{
						// The battle was won as the move played.
						Outcome.bPlayed = true;
					}
					bBattleRequested = false;
					if (Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning)
					{
						FEventContext& Context = Loop.Field()->Runtime().Context();
						// Each battle won goes back to the map; another (after a defeat, the title's) would ask for another battle.
						REQUIRE(Context.RunningMap == Map);
						// Riding as the map has begun, which sets who rides as it enters.
						for (int32 Slot = 0; Slot < 3; ++Slot)
						{
							State.SetPartySlotInGear(Slot, bInGears ? 1 : 0);
						}
						Refill(State, Striker.Record);
						if (!bInGears)
						{
							KnockOut(State, FallenAlly(Striker.Record));
						}
						Context.bBattleRequested = true;
						Context.Requests.bScriptedBattle = true;
						Context.Requests.bLeavingField = true;
						Context.Requests.BattleKind = 0;
						Context.Encounters.Formation = bInGears ? GearsFormation : ForestFormation;
						bBattleRequested = true;
					}
				}
				else if (Loop.Program() == EGameProgram::Battle)
				{
					const FBattleContext& Battle = Loop.Battle()->Runtime().Context();
					REQUIRE_FALSE(Battle.bEventBattle);
					REQUIRE(Battle.PartyCharacters[0] == Striker.Record);
					const FBattlePartyInput& Party = Battle.Party;
					const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
					const bool bStriker = Battle.PartyCharacters[Party.Actor] == Striker.Record;
					const bool bRingState = bInGears ? Party.MenuState >= GearAttackCommand && Party.MenuState <= GearRingEnd
													 : Party.MenuState >= 1 && Party.MenuState <= 10 && Party.MenuState != AttackModeState;
					// The ring reads the pad once its window has opened on the turn's state; until then the state is the last turn's.
					const bool bRing = Top == EBattlePartyRoutine::Turn && Party.bIconsShown && Party.ShownState == Party.MenuState && bRingState;
					const bool bAttackMode = Top == EBattlePartyRoutine::Turn
						&& (bInGears ? Party.MenuState == GearAttackModeState || Party.MenuState == FadingGearAttackModeState
									 : Party.MenuState == AttackModeState || Party.MenuState == FadingAttackModeState);
					if (bStarted && bStriker && Top != EBattlePartyRoutine::None && Battle.CurrentAttack == Outcome.Move.Attack())
					{
						Outcome.bPerformed = true;
					}
					bTurnEnded = bTurnEnded || (Outcome.bPerformed && Party.Depth == 0);
					if (bTurnEnded && bRing)
					{
						Outcome.bPlayed = true;
					}
					const bool bActs = bStriker && !Outcome.bPerformed;
					if (bPressFrame && !Outcome.bPlayed)
					{
						if (Party.InputMode == 2 || !Loop.Battle()->Runtime().IsFought())
						{
							Held = Press(EPadButton::Cross);
						}
						else if (bRing)
						{
							const uint8 Command = Outcome.Move.IsEther() ? (bInGears ? GearEtherCommand : EtherCommand) : (bInGears ? GearAttackCommand : AttackCommand);
							const uint8 Wanted = bActs ? Command : bInGears ? ChargeCommand : GuardCommand;
							Held = Press(Party.MenuState == Wanted ? EPadButton::Cross : TowardCommand(Wanted));
							bStarted = bStarted || (bActs && Party.MenuState == Wanted);
						}
						else if (bActs && bAttackMode && Outcome.Move.Kind == EMove::Deathblow)
						{
							const std::string_view Presses = DeathblowPresses[Outcome.Move.Number];
							// After its last press the combo has closed; a combo still open did not make the Deathblow and is ended.
							Held = Press(Party.ButtonCount < Presses.size() ? AttackButton(Presses[Party.ButtonCount]) : EPadButton::Circle);
						}
						else if (bActs && bAttackMode && Outcome.Move.Kind == EMove::GearDeathblow)
						{
							Held = Press(GearDeathblowPress(Outcome.Move.Number, Battle.GearTurn.Level, Party.ButtonCount));
						}
						else if (bActs && Top == EBattlePartyRoutine::EtherMenu && Party.ListWindow == EtherList && Party.ListScrollWay == 0)
						{
							const int32 Entry = Party.List.Cell + 2 * Party.List.TopRow;
							Held = Press(Entry < Outcome.Move.Number ? EPadButton::Right : EPadButton::Cross);
						}
						else if (bActs && Top == EBattlePartyRoutine::SelectTarget)
						{
							Held = Press(EPadButton::Cross);
						}
					}
				}
				MoveFrames = bStarted ? MoveFrames + 1 : MoveFrames;
				if (const auto Error = Frame(Held))
				{
					Outcome.Stop = Error->Message;
					Outcomes.push_back(Outcome);
					break;
				}
				if (Outcome.bPlayed || MoveFrames > MoveFrameLimit)
				{
					Outcomes.push_back(Outcome);
					if (Outcomes.size() == Striker.Moves.size() || !Outcome.bPlayed)
					{
						break;
					}
					Outcome = Begun(Striker.Moves[Outcomes.size()]);
					bStarted = false;
					bTurnEnded = false;
					MoveFrames = 0;
				}
				REQUIRE(Count < MoveFrameLimit * static_cast<int32>(Striker.Moves.size()) * 2);
			}
		}
		return Outcomes;
	}

	void CheckEveryMove(const FPlayedGame& Game, const FStriker& Striker)
	{
		for (const FMoveOutcome& Outcome : PlayEveryMove(Game, Striker))
		{
			INFO("record " << int32(Striker.Record) << (Striker.Gear ? " in gear " + std::to_string(*Striker.Gear) : std::string()) << ", " << Describe(Outcome.Move)
						   << (Outcome.Stop.empty() ? "" : ": ") << Outcome.Stop);
			CHECK(Outcome.Stop.empty());
			CHECK(Outcome.bPerformed);
			CHECK(Outcome.bPlayed);
		}
	}

	/** Each its own game: a session's sounds are its own, the discs' files read anew. */
	void CheckEveryMove(const FIso9660& Disc, const FStriker& Striker)
	{
		auto Opened = FPlayedGame::Open(&Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		CheckEveryMove(*Opened, Striker);
	}

	/** A gear's Deathblows of attack levels First to Last: three for each, records 3-5 at level 1 to 9-11 at level 3. */
	std::vector<FMove> GearDeathblows(int32 First, int32 Last)
	{
		return Moves(EMove::GearDeathblow, 3 * (Last - First + 1), 3 * First);
	}

	std::vector<FMove> GearEthers(std::initializer_list<int32> Numbers)
	{
		std::vector<FMove> All;
		for (const int32 Number : Numbers)
		{
			All.push_back({EMove::GearEther, static_cast<uint8>(Number)});
		}
		return All;
	}

	FCharacterLooks Switched(uint8 Record)
	{
		FCharacterLooks Looks;
		REQUIRE(Looks.Switch(Record));
		return Looks;
	}

	/** The party a story battle is fought by. */
	constexpr std::array<uint8, 3> StoryParty = {Fei, Citan, Elly};

	/** A battle the story asks for by its map's script, by the map and the formation in the map's table. */
	struct FStoryBattle
	{
		uint16 Map = 0;
		uint8 Formation = 0;
	};

	/**
	 * Each different battle (by its scene, its enemies and whether they ride gears) that a map's script asks for without
	 * a battle event, from the census of both discs' scripted battles.
	 */
	constexpr std::array<FStoryBattle, 83> StoryBattles = {{
		{22, 8}, {22, 9}, {23, 14}, {47, 14}, {47, 15}, {54, 0}, {55, 1}, {57, 3}, {58, 1}, {58, 2}, {82, 2}, {83, 3}, {83, 4}, {133, 11},
		{133, 12}, {142, 8}, {172, 3}, {180, 0}, {180, 1}, {180, 2}, {181, 3}, {185, 15}, {193, 14}, {224, 14}, {240, 15}, {245, 10}, {245, 11},
		{245, 12}, {245, 13}, {252, 3}, {260, 0}, {260, 1}, {260, 2}, {261, 5}, {268, 14}, {311, 9}, {334, 15}, {335, 11}, {341, 9}, {348, 7},
		{348, 8}, {363, 7}, {379, 14}, {383, 15}, {406, 0}, {411, 13}, {412, 15}, {418, 2}, {418, 3}, {418, 4}, {418, 5}, {420, 14}, {423, 10},
		{423, 11}, {423, 12}, {423, 13}, {447, 6}, {474, 0}, {478, 4}, {483, 15}, {491, 5}, {494, 6}, {495, 1}, {505, 2}, {510, 15}, {568, 7},
		{568, 8}, {568, 9}, {568, 10}, {607, 3}, {622, 9}, {627, 15}, {632, 14}, {650, 15}, {654, 12}, {710, 4}, {717, 9}, {717, 10},
		{717, 11}, {717, 12}, {717, 13}, {717, 14}, {717, 15},
	}};

	/**
	 * Each different battle with a battle event (by its scene, enemies and event) the maps' scripts ask for, but two that
	 * need the party and gears their maps' scripts stage: map 198's formation 8 and map 278's formation 4 (Elly holding
	 * Id), whose events have slot 0's object play sequences of an extension a new game's party never installs.
	 */
	constexpr std::array<FStoryBattle, 44> StoryEventBattles = {{
		{2, 1}, {2, 2}, {2, 3}, {24, 15}, {62, 2}, {70, 15}, {79, 0}, {84, 1}, {130, 1}, {130, 2}, {130, 3}, {130, 4}, {131, 0},
		{152, 13}, {152, 14}, {152, 15}, {161, 10}, {174, 14}, {174, 15}, {190, 13}, {192, 12}, {199, 3}, {199, 4}, {199, 5}, {199, 6},
		{199, 7}, {223, 15}, {245, 8}, {247, 15}, {270, 15}, {273, 6}, {311, 10}, {323, 8}, {388, 12}, {388, 15}, {442, 15}, {456, 14},
		{613, 5}, {619, 14}, {655, 13}, {660, 0}, {666, 3}, {713, 5}, {719, 8},
	}};

	/** The frames the enemies have to act in once the fight begins: many turns each with the party only guarding. */
	constexpr int32 EnemyFrames = 2400;
	constexpr int32 BattleStartLimit = 600;
	constexpr uint8 InGearFlag = 0x80;

	/**
	 * The battle fought from a new game by Fei, Citan and Elly (in their gears where an enemy rides one), who never fall and
	 * only guard or charge, while the enemies act through their scripts and the battle's event runs: the battle must start,
	 * and nothing stop, until it ends or the enemies have had EnemyFrames.
	 */
	void CheckEnemiesAct(const FPlayedGame& Game, const FStoryBattle& Wanted, bool bEvent)
	{
		INFO("map " << Wanted.Map << ", formation " << int32(Wanted.Formation));
		auto Sound = FGameSoundSession::Start(*Game.Discs[0].Iso);
		REQUIRE(Sound);
		FUnheardGameSound Unheard(*(*Sound)->Sound());
		auto Started = Game.Start(0, nullptr, Sound->get());
		REQUIRE(Started);
		FGameSession& Session = **Started;
		FGameState& State = Session.Game();
		State.SetJoinedCharacters(AllCharacters);
		State.SetParty(StoryParty);
		auto Looped = FGameLoop::EnterField(Session, Wanted.Map);
		REQUIRE(Looped);
		FGameLoop& Loop = **Looped;
		bool bInGears = false;
		bool bFought = false;
		int32 Fought = 0;
		for (int32 Count = 0; Fought < EnemyFrames; ++Count)
		{
			REQUIRE(Count < BattleStartLimit + EnemyFrames * 2);
			FPadState Held;
			if (Loop.Program() == EGameProgram::Field)
			{
				if (bFought)
				{
					break;
				}
				if (Loop.Field() != nullptr && Loop.Field()->Runtime().Context().bFieldRunning)
				{
					FEventContext& Context = Loop.Field()->Runtime().Context();
					REQUIRE(Context.RunningMap == Wanted.Map);
					const std::optional<std::array<uint8, FKernelState::FormationBytes>> Record = Session.Kernel().Formation(Wanted.Formation);
					REQUIRE(Record);
					const FBattleFormation Formation = FBattleFormation::FromRecord(*Record);
					REQUIRE(((Formation.Flags & FormationFlags::EventBattle) != 0) == bEvent);
					bInGears = std::any_of(Formation.EnemyTypes.begin(), Formation.EnemyTypes.end(),
						[](uint8 Type) { return (Type & 0x7F) != FBattleFormation::EmptyType && (Type & InGearFlag) != 0; });
					for (int32 Slot = 0; Slot < 3; ++Slot)
					{
						State.SetPartySlotInGear(Slot, bInGears ? 1 : 0);
					}
					for (const uint8 Member : StoryParty)
					{
						Refill(State, Member);
					}
					Context.bBattleRequested = true;
					Context.Requests.bScriptedBattle = true;
					Context.Requests.bLeavingField = true;
					Context.Requests.BattleKind = 0;
					Context.Encounters.Formation = Wanted.Formation;
				}
			}
			else if (Loop.Program() == EGameProgram::Battle)
			{
				const FBattlePartyInput& Party = Loop.Battle()->Runtime().Context().Party;
				const EBattlePartyRoutine Top = Party.Depth > 0 ? Party.Routines[Party.Depth - 1].Routine : EBattlePartyRoutine::None;
				const bool bRingState = bInGears ? Party.MenuState >= GearAttackCommand && Party.MenuState <= GearRingEnd
												 : Party.MenuState >= 1 && Party.MenuState <= 10 && Party.MenuState != AttackModeState;
				const bool bRing = Top == EBattlePartyRoutine::Turn && Party.bIconsShown && Party.ShownState == Party.MenuState && bRingState;
				bFought = bFought || Loop.Battle()->Runtime().IsFought();
				Fought += bFought ? 1 : 0;
				if ((Count & 1) == 0)
				{
					if (Party.InputMode == 2 || !Loop.Battle()->Runtime().IsFought())
					{
						Held = Press(EPadButton::Cross);
					}
					else if (bRing)
					{
						const uint8 Command = bInGears ? ChargeCommand : GuardCommand;
						Held = Press(Party.MenuState == Command ? EPadButton::Cross : TowardCommand(Command));
					}
				}
			}
			else
			{
				// A lost battle goes on to the title's map, and a battle may end in a movie: the enemies have played.
				REQUIRE(bFought);
				break;
			}
			const std::vector<FPadState> PerVblank(static_cast<std::size_t>(Loop.VblanksPerFrame()), Held);
			Unheard.Advance(PsxTiming::FrameSeconds * static_cast<double>(PerVblank.size()));
			if (const auto Error = Loop.Frame(PerVblank))
			{
				FAIL(Error->Message);
			}
		}
		CHECK(bFought);
	}

	/** The battles of a list from First, Count of them, which have battle events where bEvent. */
	void CheckEnemiesAct(const FIso9660& Disc, std::span<const FStoryBattle> Battles, bool bEvent, std::size_t First, std::size_t Count)
	{
		auto Opened = FPlayedGame::Open(&Disc, FindDisc(EDiscRelease::UsaDisc2));
		REQUIRE(Opened);
		for (std::size_t Index = First; Index < std::min(First + Count, Battles.size()); ++Index)
		{
			CheckEnemiesAct(*Opened, Battles[Index], bEvent);
		}
	}
}

namespace
{
	void CheckBrazilianEnemies(std::span<const FStoryBattle> Battles, bool bEvent, std::size_t First, std::size_t Count)
	{
		REQUIRE_BRAZILIAN_DISC(Disc, EDiscRelease::UsaDisc1);
		REQUIRE_BRAZILIAN_DISC(Other, EDiscRelease::UsaDisc2);
		auto Opened = FPlayedGame::Open(Disc, Other);
		REQUIRE(Opened);
		for (std::size_t Index = First; Index < std::min(First + Count, Battles.size()); ++Index)
		{
			CheckEnemiesAct(*Opened, Battles[Index], bEvent);
		}
	}
}

// On foot: as many Deathblows and Ethers as each learns by the levels of its growth record.

TEST_CASE("Brazilian story battles without events play without a stop, maps 22 to 58", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 0, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 82 to 180", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 10, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 181 to 252", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 20, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 260 to 348", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 30, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 348 to 418", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 40, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 418 to 483", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 50, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 491 to 607", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 60, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 622 to 717", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 70, 10);
}

TEST_CASE("Brazilian story battles without events play without a stop, maps 717", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryBattles, false, 80, 3);
}

TEST_CASE("Brazilian story battles with events play without a stop, maps 2 to 130", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryEventBattles, true, 0, 9);
}

TEST_CASE("Brazilian story battles with events play without a stop, maps 130 to 174", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryEventBattles, true, 9, 9);
}

TEST_CASE("Brazilian story battles with events play without a stop, maps 174 to 223", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryEventBattles, true, 18, 9);
}

TEST_CASE("Brazilian story battles with events play without a stop, maps 245 to 442", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryEventBattles, true, 27, 9);
}

TEST_CASE("Brazilian story battles with events play without a stop, maps 456 to 719", "[Battle][Content]")
{
	CheckBrazilianEnemies(StoryEventBattles, true, 36, 8);
}

TEST_CASE("Every Deathblow Fei learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, Moves(EMove::Deathblow, 13), std::nullopt});
}

TEST_CASE("Every Ether Fei learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, Moves(EMove::Ether, 8), std::nullopt});
}

TEST_CASE("Every Deathblow Elly learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, Moves(EMove::Deathblow, 11), std::nullopt});
}

TEST_CASE("Every Ether Elly learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, Moves(EMove::Ether, 12), std::nullopt});
}

TEST_CASE("Every Deathblow the first Citan learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Citan, {}, Moves(EMove::Deathblow, 11), std::nullopt});
}

TEST_CASE("Every Ether the first Citan learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Citan, {}, Moves(EMove::Ether, 11), std::nullopt});
}

TEST_CASE("Every Deathblow the second Citan learns plays to its end, his sword drawn", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, Moves(EMove::Deathblow, 11), std::nullopt});
}

TEST_CASE("Every Deathblow the second Citan learns plays to its end, his sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), Moves(EMove::Deathblow, 11), std::nullopt});
}

TEST_CASE("Every Ether the second Citan learns plays to its end, his sword drawn or put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, Moves(EMove::Ether, 11), std::nullopt});
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), Moves(EMove::Ether, 11), std::nullopt});
}

TEST_CASE("Every Deathblow Bart learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, Moves(EMove::Deathblow, 11), std::nullopt});
}

TEST_CASE("Every Ether Bart learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, Moves(EMove::Ether, 10), std::nullopt});
}

TEST_CASE("Every Deathblow Billy learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, Moves(EMove::Deathblow, 10), std::nullopt});
}

TEST_CASE("Every Ether Billy learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, Moves(EMove::Ether, 10), std::nullopt});
}

TEST_CASE("Every Deathblow and Ether Rico learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, OnFoot(10, 4), std::nullopt});
}

TEST_CASE("Every Deathblow Emeralda learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Emeralda, {}, Moves(EMove::Deathblow, 10), std::nullopt});
}

TEST_CASE("Every Ether Emeralda learns plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Emeralda, {}, Moves(EMove::Ether, 8), std::nullopt});
}

TEST_CASE("Every Deathblow the grown Emeralda learns plays to its end, grown or drawn as the child", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {GrownEmeralda, {}, Moves(EMove::Deathblow, 10), std::nullopt});
	CheckEveryMove(*Disc, {GrownEmeralda, Switched(GrownEmeralda), Moves(EMove::Deathblow, 10), std::nullopt});
}

TEST_CASE("Every Ether the grown Emeralda learns plays to its end, grown or drawn as the child", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {GrownEmeralda, {}, Moves(EMove::Ether, 8), std::nullopt});
	CheckEveryMove(*Disc, {GrownEmeralda, Switched(GrownEmeralda), Moves(EMove::Ether, 8), std::nullopt});
}

TEST_CASE("Every Ether Chu-Chu and Maria learn plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {ChuChu, {}, Moves(EMove::Ether, 8), std::nullopt});
	CheckEveryMove(*Disc, {Maria, {}, Moves(EMove::Ether, 5), std::nullopt});
}

// In gears: every gear a member pilots, with the pilot the game gives it, its Deathblows by level and the ethers its
// pilot learns; also as the player may have it drawn.

TEST_CASE("Every level 1 and 2 Deathblow of Weltall plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(1, 2), 0});
}

TEST_CASE("Every level 1 and 2 Deathblow of Weltall plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(1, 2), 0});
}

TEST_CASE("Every level 3 Deathblow of Weltall plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(3, 3), 0});
}

TEST_CASE("Every level 3 Deathblow of Weltall plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(3, 3), 0});
}

TEST_CASE("Every ether of Weltall plays to its end, also drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearEthers({0, 1, 2, 3, 4, 5}), 0});
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearEthers({0, 1, 2, 3, 4, 5}), 0});
}

TEST_CASE("Every level 1 and 2 Deathblow of Weltall-2 plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(1, 2), 1});
}

TEST_CASE("Every level 1 and 2 Deathblow of Weltall-2 plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(1, 2), 1});
}

TEST_CASE("Every level 3 Deathblow of Weltall-2 plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(3, 3), 1});
}

TEST_CASE("Every level 3 Deathblow of Weltall-2 plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(3, 3), 1});
}

TEST_CASE("Every ether of Weltall-2 plays to its end, also drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearEthers({0, 1, 2, 3, 4, 5}), 1});
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearEthers({0, 1, 2, 3, 4, 5}), 1});
}

TEST_CASE("Every level 1 and 2 Deathblow of Vierge plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearDeathblows(1, 2), 2});
}

TEST_CASE("Every level 3 Deathblow of Vierge plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearDeathblows(3, 3), 2});
}

TEST_CASE("Every ether of Vierge plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}), 2});
}

TEST_CASE("Every level 1 and 2 Deathblow of Heimdal plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Citan, {}, GearDeathblows(1, 2), 3});
}

TEST_CASE("Every level 3 Deathblow of Heimdal plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Citan, {}, GearDeathblows(3, 3), 3});
}

TEST_CASE("Every ether of Heimdal plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Citan, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 3});
}

TEST_CASE("Every level 1 and 2 Deathblow of Brigandier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearDeathblows(1, 2), 4});
}

TEST_CASE("Every level 3 Deathblow of Brigandier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearDeathblows(3, 3), 4});
}

TEST_CASE("Every ether of Brigandier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearEthers({0, 1, 2, 3, 4, 5}), 4});
}

TEST_CASE("Every level 1 and 2 Deathblow of Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearDeathblows(1, 2), 5});
}

TEST_CASE("Every level 3 Deathblow of Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearDeathblows(3, 3), 5});
}

TEST_CASE("Every ether of Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearEthers({0, 1, 2, 3, 4, 5}), 5});
}

TEST_CASE("Every level 1 and 2 Deathblow of Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearDeathblows(1, 2), 6});
}

TEST_CASE("Every level 3 Deathblow of Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearDeathblows(3, 3), 6});
}

TEST_CASE("Every ether of Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearEthers({0}), 6});
}


TEST_CASE("Every ether of Chu-Chu's giant form plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {ChuChu, {}, GearEthers({0}), 7});
}

TEST_CASE("Every level 1 and 2 Deathblow of Seibzehn plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Maria, {}, GearDeathblows(1, 2), 8});
}

TEST_CASE("Every level 3 Deathblow of Seibzehn plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Maria, {}, GearDeathblows(3, 3), 8});
}

TEST_CASE("Every level 1 and 2 Deathblow of Crescens plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Emeralda, {}, GearDeathblows(1, 2), 9});
}

TEST_CASE("Every level 3 Deathblow of Crescens plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Emeralda, {}, GearDeathblows(3, 3), 9});
}

TEST_CASE("Every ether of Crescens plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Emeralda, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 9});
}

TEST_CASE("Every level 1 and 2 Deathblow of Regurus plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearDeathblows(1, 2), 10});
}

TEST_CASE("Every level 3 Deathblow of Regurus plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearDeathblows(3, 3), 10});
}

TEST_CASE("Every ether of Regurus plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Elly, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}), 10});
}

TEST_CASE("Every level 1 and 2 Deathblow of Fenrir plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearDeathblows(1, 2), 11});
}

TEST_CASE("Every level 1 and 2 Deathblow of Fenrir plays to its end, its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearDeathblows(1, 2), 11});
}

TEST_CASE("Every level 3 Deathblow of Fenrir plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearDeathblows(3, 3), 11});
}

TEST_CASE("Every level 3 Deathblow of Fenrir plays to its end, its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearDeathblows(3, 3), 11});
}

TEST_CASE("Every ether of Fenrir plays to its end, also its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 11});
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 11});
}

TEST_CASE("Every level 1 and 2 Deathblow of Andvari plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearDeathblows(1, 2), 12});
}

TEST_CASE("Every level 3 Deathblow of Andvari plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearDeathblows(3, 3), 12});
}

TEST_CASE("Every ether of Andvari plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Bart, {}, GearEthers({0, 1, 2, 3, 4, 5}), 12});
}

TEST_CASE("Every level 1 and 2 Deathblow of the Omnigear Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearDeathblows(1, 2), 13});
}

TEST_CASE("Every level 3 Deathblow of the Omnigear Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearDeathblows(3, 3), 13});
}

TEST_CASE("Every ether of the Omnigear Renmazuo plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Billy, {}, GearEthers({0, 1, 2, 3, 4, 5}), 13});
}

TEST_CASE("Every level 1 and 2 Deathblow of the Omnigear Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearDeathblows(1, 2), 14});
}

TEST_CASE("Every level 3 Deathblow of the Omnigear Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearDeathblows(3, 3), 14});
}

TEST_CASE("Every ether of the Omnigear Stier plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Rico, {}, GearEthers({0}), 14});
}

TEST_CASE("Every level 1 and 2 Deathblow of Xenogears plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(1, 2), 15});
}

TEST_CASE("Every level 1 and 2 Deathblow of Xenogears plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(1, 2), 15});
}

TEST_CASE("Every level 3 Deathblow of Xenogears plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearDeathblows(3, 3), 15});
}

TEST_CASE("Every level 3 Deathblow of Xenogears plays to its end, drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearDeathblows(3, 3), 15});
}

TEST_CASE("Every ether of Xenogears plays to its end, also drawn as Id's red gear", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {Fei, {}, GearEthers({0, 1, 2, 3, 4, 5}), 15});
	CheckEveryMove(*Disc, {Fei, Switched(Fei), GearEthers({0, 1, 2, 3, 4, 5}), 15});
}

TEST_CASE("Every level 1 and 2 Deathblow of the second Heimdal plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearDeathblows(1, 2), 16});
}

TEST_CASE("Every level 1 and 2 Deathblow of the second Heimdal plays to its end, its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearDeathblows(1, 2), 16});
}

TEST_CASE("Every level 3 Deathblow of the second Heimdal plays to its end", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearDeathblows(3, 3), 16});
}

TEST_CASE("Every level 3 Deathblow of the second Heimdal plays to its end, its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearDeathblows(3, 3), 16});
}

TEST_CASE("Every ether of the second Heimdal plays to its end, also its sword put away", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEveryMove(*Disc, {SwordCitan, {}, GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 16});
	CheckEveryMove(*Disc, {SwordCitan, Switched(SwordCitan), GearEthers({0, 1, 2, 3, 4, 5, 6, 7}), 16});
}

// The enemies of the story's battles, its bosses among them: each runs its scripts and its moves while the party guards.

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 22 to 58", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 0, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 82 to 180", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 10, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 181 to 252", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 20, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 260 to 348", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 30, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 348 to 418", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 40, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 418 to 483", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 50, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 491 to 607", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 60, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, maps 622 to 717", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 70, 10);
}

TEST_CASE("The enemies of the story's battles without an event act without a stop, map 717", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryBattles, false, 80, 3);
}

TEST_CASE("The enemies of the story's battle events act without a stop, maps 2 to 130", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryEventBattles, true, 0, 9);
}

TEST_CASE("The enemies of the story's battle events act without a stop, maps 130 to 174", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryEventBattles, true, 9, 9);
}

TEST_CASE("The enemies of the story's battle events act without a stop, maps 174 to 223", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryEventBattles, true, 18, 9);
}

TEST_CASE("The enemies of the story's battle events act without a stop, maps 245 to 442", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryEventBattles, true, 27, 9);
}

TEST_CASE("The enemies of the story's battle events act without a stop, maps 456 to 719", "[Battle][Content]")
{
	REQUIRE_DISC(Disc, EDiscRelease::UsaDisc1);
	CheckEnemiesAct(*Disc, StoryEventBattles, true, 36, 8);
}
