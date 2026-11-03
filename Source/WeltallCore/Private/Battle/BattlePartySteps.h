#pragma once

#include "Battle/BattleContext.h"

namespace Weltall
{
	/**
	 * The party's turn as routines that run across frames. The game's turn code waits for its frames in
	 * the middle of its routines; each routine here keeps the point it waits at and its locals in the
	 * context's party state, and a frame resumes the innermost one.
	 */
	namespace BattleParty
	{
		/** How a routine leaves: it waits for the next frame, returns to its caller, or has called another (which runs at once). */
		enum class EFlow : uint8
		{
			Yield,
			Return,
			Call,
		};

		/** The menu states the turn's code names. */
		namespace EState
		{
			inline constexpr uint8 Attack = 1;
			inline constexpr uint8 Item = 2;
			inline constexpr uint8 Guard = 3;
			inline constexpr uint8 Ether = 4;
			inline constexpr uint8 AttackMode = 5;
			inline constexpr uint8 Combo = 7;
			inline constexpr uint8 OuterItem = 8;
			inline constexpr uint8 Escape = 9;
			inline constexpr uint8 Board = 10;
			inline constexpr uint8 GearAttack = 0x10;
			inline constexpr uint8 GearItem = 0x11;
			inline constexpr uint8 Charge = 0x12;
			inline constexpr uint8 GearEther = 0x13;
			inline constexpr uint8 Booster = 0x15;
			inline constexpr uint8 GearOuterItem = 0x16;
			inline constexpr uint8 GearEscape = 0x17;
			inline constexpr uint8 Special = 0x18;
			inline constexpr uint8 GearAttackMode = 0x19;
			/** The attack modes while the menu fades out after the first hit. */
			inline constexpr uint8 FadingAttackMode = 100;
			inline constexpr uint8 FadingGearAttackMode = 0x65;
		}

		/** The menu's command numbers (their disabled flags). */
		namespace ECommand
		{
			inline constexpr int32 WeakButton = 0;
			inline constexpr int32 StrongButton = 1;
			inline constexpr int32 FierceButton = 2;
			inline constexpr int32 SpecialFourth = 3;
			inline constexpr int32 Special = 4;
			inline constexpr int32 Combo = 5;
			inline constexpr int32 GearEther = 6;
			inline constexpr int32 Board = 7;
			inline constexpr int32 Escape = 8;
			inline constexpr int32 Attack = 9;
			inline constexpr int32 Ether = 10;
			inline constexpr int32 Item = 11;
			inline constexpr int32 GearItem = 12;
			inline constexpr int32 Booster = 13;
			inline constexpr int32 SpecialSecond = 14;
			inline constexpr int32 SpecialThird = 15;
		}

		/** The system's sound effects the turn plays. */
		namespace ESound
		{
			inline constexpr uint8 Move = 0x4C;
			inline constexpr uint8 Accept = 0x4D;
			inline constexpr uint8 Back = 0x4E;
			inline constexpr uint8 Refuse = 0x4F;
			inline constexpr uint8 TurnStart = 0x5A;
		}

		inline constexpr uint8 Billy = 4;
		inline constexpr uint8 ChuChu = 7;
		inline constexpr uint8 InfinityLevel = 4;

		// The routines' machinery (BattleParty.cpp).
		FBattlePartyFrame& Current(FBattleContext& Context);
		EFlow Call(FBattleContext& Context, EBattlePartyRoutine Routine);
		/** Count frames pass without the event being read, then the caller goes on. */
		EFlow CallFrames(FBattleContext& Context, uint8 Count);
		/** A routine's return with a value for its caller. */
		EFlow ReturnValue(FBattleContext& Context, uint8 Value);
		/** The show still plays what the call just made began: the game takes those frames inside the call, the routine waits for them. */
		bool ShowBlocks(const FBattleContext& Context);

		// What the turn hands the show.
		void PlaySound(FBattleContext& Context, uint8 Effect);
		/** A sound of the menu's, which plays only while the menu's sounds are on. */
		void PlayMenuSound(FBattleContext& Context, uint8 Effect);
		void FrameCamera(FBattleContext& Context, uint16 Slots);
		void SetMarkers(FBattleContext& Context, uint16 Slots);
		/** The HUD's drawing of the party's menus for Slot, where the game calls the HUD's routine. */
		void PlayMenuCue(FBattleContext& Context, EBattleMenuCue Cue, uint8 Slot, uint8 Arg = 0);

		/** A slot's bit as the game's table gives it (0 for none). */
		uint16 Bit(FBattleContext& Context, uint8 Slot);
		bool IsDisabled(const FBattleContext& Context, uint8 Slot, int32 Command);
		/** The show's current record; past the 32 faults. */
		FBattlePerformance* CurrentRecord(FBattleContext& Context);
		/** A record of a code alone for the actor, with its parameter, and the next becomes current; returns the record (none past the 32). */
		FBattlePerformance* QueueCode(FBattleContext& Context, uint8 Actor, uint8 Code, uint16 Parameter);
		/** The end of the turn's actions, in the current record. */
		void QueueEnd(FBattleContext& Context, uint8 Actor);
		/** Every record emptied, each the actor's aimed at the target: the attack mode's start and its cursor's moves. */
		void ResetQueue(FBattleContext& Context, uint8 Actor, uint8 Target);
		/** A character's learned-skills word; none (and a fault) past the game state. */
		uint16 LearnedWord(FBattleContext& Context, uint8 Character, std::size_t Field);

		// The command menu (BattlePartyMenu.cpp): the current state's handler for the frame's event.
		EFlow RunMenuState(FBattleContext& Context, uint8 Slot);
		/** A member's boarding up to its show (the gear placed, the statuses cleared); FinishBoarding once the show is done. */
		void BeginBoarding(FBattleContext& Context, uint8 Slot);
		void FinishBoarding(FBattleContext& Context, uint8 Slot);
		EFlow RunBoard(FBattleContext& Context, FBattlePartyFrame& Frame);
		/** The boarded gear takes the first party gear place free, its own group's when that one is. */
		void PlaceBoardedGear(FBattleContext& Context, uint8 Slot);

		// The attack modes (BattlePartyAttack.cpp, BattlePartyGearAttack.cpp).
		/** The combo's start up to the playback's (the records emptied, the route planned); FinishAttackStart once the show is done. */
		void BeginAttack(FBattleContext& Context, uint8 Slot);
		void FinishAttackStart(FBattleContext& Context);
		EFlow RunStartAttack(FBattleContext& Context, FBattlePartyFrame& Frame);
		void InitTargetCursor(FBattleContext& Context, uint8 Slot);
		void LockTarget(FBattleContext& Context, uint8 Slot);
		/**
		 * One hit of a combo: the command worked out, applied and queued, the target's AI told; true when it ends the combo.
		 * With bAnyDeathblow a Deathblow is performed whether the member has learned it or not.
		 */
		bool PerformHit(FBattleContext& Context, uint8 Slot, uint8 Cost, bool bAnyDeathblow = false);
		/** The attack cursor's frame: the camera and markers on it while it moves, and Billy's ammunition window. */
		EFlow ShowTarget(FBattleContext& Context, uint8 Slot);
		EFlow RunAttackMode(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunGearAttackMode(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunOpenAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame);
		/** Billy's ammunition window closed (and the ammunition work freed when Close): two frames when it stood. */
		EFlow CloseAmmunition(FBattleContext& Context, uint8 Slot, bool bClose);
		EFlow RunCloseAmmunition(FBattleContext& Context, FBattlePartyFrame& Frame);

		// The lists (BattlePartyLists.cpp, BattlePartyCombo.cpp).
		EFlow RunItemMenu(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunEtherMenu(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunSpecialMenu(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunComboMenu(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunPerformAbility(FBattleContext& Context, FBattlePartyFrame& Frame);
		EFlow RunPerformItem(FBattleContext& Context, FBattlePartyFrame& Frame);

		// Targeting (BattleTargets.cpp).
		bool CanReachTarget(FBattleContext& Context, uint8 Actor, uint8 Target);
		uint8 FindTargetInDirection(FBattleContext& Context, uint8 From, uint8 Direction);
		/** The target selector for a command's targeting word; returns 1 when a target was confirmed. */
		EFlow CallSelectTarget(FBattleContext& Context, uint16 Word, uint8 Actor, bool bReachable);
		EFlow RunSelectTarget(FBattleContext& Context, FBattlePartyFrame& Frame);

		// The automatic turn (BattleAutoTurn.cpp).
		EFlow RunAutoTurn(FBattleContext& Context, FBattlePartyFrame& Frame);
	}
}
