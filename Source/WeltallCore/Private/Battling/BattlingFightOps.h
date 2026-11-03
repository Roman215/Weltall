#pragma once

#include "Battling/BattlingFight.h"

namespace Weltall::BattlingFightOps
{
	inline FBattlingFighter& Opponent(FBattlingFight& Fight, const FBattlingFighter& Fighter)
	{
		return Fight.Fighters[static_cast<std::size_t>(1 - Fighter.SideIndex())];
	}

	inline FBattlingAiState& AiOf(FBattlingFight& Fight, const FBattlingFighter& Fighter)
	{
		return Fight.Ai[static_cast<std::size_t>(Fighter.SideIndex())];
	}

	inline bool NoHeat(const FBattlingFight& Fight)
	{
		return Fight.Settings.Mode == EBattlingMode::Practice || Fight.Settings.Mode == EBattlingMode::Demonstration;
	}

	/** The fight plays by Perfect Works Build's arena's rules, Basic's or Expert's. */
	inline bool ArenaRules(const FBattlingFight& Fight)
	{
		return Fight.Settings.Rules != EBattlingRules::Original;
	}

	inline bool ExpertRules(const FBattlingFight& Fight)
	{
		return Fight.Settings.Rules == EBattlingRules::PerfectWorksExpert;
	}

	// The command queue.
	void QueuePush(FBattlingFighter& Fighter, uint8 Command);
	uint8 QueuePop(FBattlingFighter& Fighter);
	void QueueClear(FBattlingFighter& Fighter);

	// Heat.
	bool HeatAdd(FBattlingFight& Fight, FBattlingFighter& Fighter, int32 Heat, int32 Source);
	bool HeatCanAdd(const FBattlingFighter& Fighter, int32 Heat);

	// The fighter's tick, in the fight tick's order.
	void ResetFighter(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void BeginTick(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void CheckHits(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void ReadPad(FBattlingFight& Fight, FBattlingFighter& Fighter, const std::array<FBattlingPad, 2>& Pads);
	void TickStatus(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void ProcessCommands(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void UpdateVelocity(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void Move(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void Separate(FBattlingFight& Fight);
	void Animate(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void RecordFlags(FBattlingFighter& Fighter);
	/** Every part shown but those the gear's file hides (and El-Fenrir's thirteenth joint), as each animation starts. */
	void ResetParts(FBattlingFighter& Fighter);
	void UpdateBody(FBattlingFight& Fight, FBattlingFighter& Fighter);
	/** EffectsRun: how many effect events the caller's register says have run (0 in every bout). */
	void RunAnimationEvents(FBattlingFight& Fight, FBattlingFighter& Fighter, int32 Frame, int32 Count, int32 EffectsRun);
	void BackDashTry(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void Vibrate(FBattlingFight& Fight, const FBattlingFighter& Fighter, int32 Strength);

	// Projectiles and hit volumes.
	void UpdateProjectiles(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void SpawnProjectile(FBattlingFight& Fight, FBattlingFighter& Fighter, const FBattlingPoint& From, const FBattlingPoint* Toward, int32 Kind,
		int16 Power, uint32 Id);
	void AgeVolumes(FBattlingFighter& Fighter);

	// The arena.
	/** The ground's height under a point; a point whose cells lie past the heightfield's rows faults the fight (0). */
	int32 TerrainHeight(FBattlingFight& Fight, const FBattlingPoint& Point, bool bWithBed);
	uint32 TerrainCell(FBattlingFight& Fight, const FBattlingPoint& Point);
	/** The clamp normalizes a vector whose height it never sets: StaleY is what the stack holds there. */
	void ClampToArena(const FBattlingPoint& Position, FBattlingPoint& Velocity, int32 Radius, int32 StaleY);
	bool InLakeZone(FBattlingFight& Fight, const FBattlingPoint& Point);

	// The computer.
	void InitAi(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void TickAi(FBattlingFight& Fight, FBattlingFighter& Fighter);
	/** The computer's tick at a given level, where the menus' level would otherwise be taken. */
	void TickAiAtLevel(FBattlingFight& Fight, FBattlingFighter& Fighter, uint8 Level);
	void TickPracticeAi(FBattlingFight& Fight, FBattlingFighter& Fighter);
	void SteerAi(FBattlingFight& Fight, FBattlingFighter& Fighter, int32 Direction, int32 Magnitude);

	// The camera.
	void StartCamera(FBattlingFight& Fight);
	void FollowFight(FBattlingFight& Fight);

	// Effects.
	void SpawnEffect(FBattlingFight& Fight, const FBattlingPoint& Point, int32 Effect);
	void AddTrail(FBattlingFight& Fight, int32 Side, const FBattlingPoint& Point, uint32 Id, int32 Kind);
	void AddSwept(FBattlingFight& Fight, int32 Side, const FBattlingPoint& A, const FBattlingPoint& B, uint32 Id);
	void SpawnLineEffect(FBattlingFight& Fight, const FBattlingPoint& From, const FBattlingPoint& To, int32 Effect);
	void AddLine(FBattlingFight& Fight, const FBattlingPoint& From, const FBattlingPoint& To, const std::array<uint8, 3>& Colour);
	void SpawnDust(FBattlingFight& Fight, const FBattlingPoint& Point, int32 Count);
	void SpawnSplashes(FBattlingFight& Fight, const FBattlingPoint& Base, const FBattlingPoint& PreviousBase);
	void TickEffects(FBattlingFight& Fight);
	/** The random sizes the program draws as it starts: every spark's tile, every splash's. */
	void InitSparkTiles(FBattlingFight& Fight);
	void InitSplashTiles(FBattlingFight& Fight);
	void ClearEffects(FBattlingFight& Fight);
	void ClearSplashesAndDust(FBattlingFight& Fight);
	void ClearSparks(FBattlingFight& Fight);
	/** What drawing the frame changes of the effects: the dust (half each pass), the splashes and the sparks. */
	/** What drawing the frame's effects changes: the dust's and the splashes' steps (but in the demonstration's), the hit sparks'. */
	void StepDrawnEffects(FBattlingFight& Fight, bool bDustAndSplashes);

	// Sounds through the fighter's set.
	void PlayFighterSound(FBattlingFight& Fight, const FBattlingFighter& Fighter, int32 Index, const FBattlingPoint* Point, EBattlingSoundPlace Place);
	void StopFighterSound(FBattlingFight& Fight, const FBattlingFighter& Fighter, int32 Index);

	// The round and the screens after it.
	/** The program's state set: the state, and the start of the replay, the result screen or the winner screen. */
	void SetState(FBattlingFight& Fight, uint8 State);
	void TickReplay(FBattlingFight& Fight, const FBattlingTickInput& Input);
	void ReplayCamera(FBattlingFight& Fight, bool bEased);
	void TickResult(FBattlingFight& Fight, const FBattlingTickInput& Input);
	void TickWinner(FBattlingFight& Fight, const FBattlingTickInput& Input);
	void EndWinner(FBattlingFight& Fight);
	/** The pads' queued blanks ORed together, as the kernel's merge leaves them (nothing after an overflow), the sticks and kinds the latest. */
	std::array<FBattlingPad, 2> MergePads(const FBattlingTickInput& Input);
	/** Starts an animation at once and runs its first events, as the winner screen does each pass. */
	void PlayAnimationNow(FBattlingFight& Fight, FBattlingFighter& Fighter, uint8 Animation);
	void CheckRound(FBattlingFight& Fight);
	uint8 GradeResult(const FBattlingFight& Fight, bool bPlayerOneWon);

	// The fight tick, as the bout's, the tutorial's and the demonstration's passes run it.
	void FightTick(FBattlingFight& Fight, const FBattlingTickInput& Input);

	// The script machine: the tutorial and the scripted bout's ending.
	/** One pass of the tutorial: the lesson menu, the script, the pointer, the windows, then the fight tick and the camera. */
	void TickTutorial(FBattlingFight& Fight, const FBattlingTickInput& Input);
	/** Starts the scripted bout's ending, once. */
	void StartEnding(FBattlingFight& Fight);
	/** The ending's step within the fight tick of Input: the breakdown's sparks, flash and fade, the script, the window and the camera. */
	void TickEnding(FBattlingFight& Fight, const FBattlingTickInput& Input);

	/** The pass's wait: the vertical blanks counted, the odd ones stepping the fire while a bout fades in through it. */
	void WaitBlanks(FBattlingFight& Fight);
	/**
	 * The menus' loop as a pass begins: the idle countdown, and the demonstration started at its end (true), which takes the
	 * rest of the pass.
	 */
	bool CountMenusDown(FBattlingFight& Fight);
	/** The rest of the menus' pass: the menus, the fire, the frame's letters, the wait. */
	void RunMenusLoop(FBattlingFight& Fight, const FBattlingTickInput& Input);

	// The menus.
	/** A menu value's step flags: the value wraps, steps on the repeat beat (else on a press), steps the other way. */
	namespace MenuStep
	{
		constexpr uint32 Wraps = 1;
		constexpr uint32 OnRepeat = 2;
		constexpr uint32 Reverses = 4;
	}
	inline constexpr uint16 MenuButtonRight = 0x2000;
	inline constexpr uint16 MenuButtonLeft = 0x8000;
	inline constexpr int32 MenuSoundValue = 0x20;
	/** A menu opened: the menu open before kept as the previous one. */
	void OpenMenu(FBattlingMenus& Menus, EBattlingMenu Menu);
	/** A help line takes a message, typed as it changes. */
	void ShowHelp(FBattlingMenus& Menus, uint8 Message, int32 Line);
	/** A menu value stepped on the last input's pad: Right and Left step a value, clamped (a clamped step silent) or wrapping. */
	int32 StepMenuValue(FBattlingFight& Fight, int32 Value, int32 Last, uint32 Flags);
	/** A menu's input on a pad: the help line, the item's action, cancel, the cursor. */
	void ReadMenuInput(FBattlingFight& Fight, EBattlingMenu Menu, int32 PadIndex, const std::array<FBattlingPad, 2>& Pads);
	/** The pause's menu or practice's opened over the frame; None closes the menus back to the fight. */
	void OpenPause(FBattlingFight& Fight, EBattlingMenu Menu);
	/** The open menu reads the pads merged and runs what is chosen, the gear choice its own; its items printed. */
	void TickMenus(FBattlingFight& Fight, const FBattlingTickInput& Input);

	// The arena's menus.
	/** The gear choice for a kind of bout. */
	void StartGearChoice(FBattlingFight& Fight, EBattlingMode Mode);
	/** The tutorial chosen, Weltall against Vierge. */
	void StartTutorialFromMenus(FBattlingFight& Fight);
	/** A side's cursor over the roster, confirm and cancel. */
	void ChooseGear(FBattlingFight& Fight, int32 Side, const std::array<FBattlingPad, 2>& Pads);
	/** The gear choice's tick, with its labels and the carousels' step. */
	void TickGearChoice(FBattlingFight& Fight, const std::array<FBattlingPad, 2>& Pads);
	/** The matches, the level and the rubber band beside their items. */
	void PrintBonusValues(FBattlingFight& Fight, const FBattlingMenuPlace& Place, int16 Cursor);
	/** The portraits' logic: each carousel's slides stepped, the portraits it shows listed, VS and the names printed. */
	void StepPortraits(FBattlingFight& Fight);
}
