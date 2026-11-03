#pragma once

#include "Battle/BattleContext.h"
#include "Foundation/Result.h"
#include "Foundation/Types.h"
#include "Text/MesWindow.h"
#include "Text/MessageOverrides.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Weltall
{
	class FCdImage;
	class FGameFileIndex;

	/**
	 * The battle events' scripts (their module's archive, directory 32 file 2): script k and its message
	 * table, unpacked, by the number a formation names.
	 */
	class FBattleEventArchive
	{
	public:
		WELTALLCORE_API static TResult<FBattleEventArchive> Load(const FCdImage& Image, const FGameFileIndex& Index);

		/** From the archive's file (its sectors in full): entries in pairs, a script then its messages. */
		WELTALLCORE_API static TResult<FBattleEventArchive> Parse(std::span<const uint8> File);

		int32 Count() const { return static_cast<int32>(Scripts.size()); }
		std::span<const uint8> Script(int32 Number) const { return Scripts[static_cast<std::size_t>(Number)]; }
		std::span<const uint8> Messages(int32 Number) const { return MessageTables[static_cast<std::size_t>(Number)]; }
		/** Event Number's table as the disc gives it, before the player's messages (Assets/text) replaced any of it. */
		std::span<const uint8> DiscMessages(int32 Number) const
		{
			const std::size_t At = static_cast<std::size_t>(Number);
			return At < DiscMessageTables.size() && !DiscMessageTables[At].empty() ? std::span<const uint8>(DiscMessageTables[At]) : Messages(Number);
		}
		/** How the scripts measure their message windows: Perfect Works' where the disc's mod gives its own scripts. */
		EBattleMessageLayout Layout() const { return MessageLayout; }
		/** Which of event Number's messages are the player's (Assets/text); none where the archive was parsed alone. */
		const FReplacedMessages* Replaced(int32 Number) const
		{
			return static_cast<std::size_t>(Number) < ReplacedMessages.size() ? &ReplacedMessages[static_cast<std::size_t>(Number)] : nullptr;
		}

	private:
		std::vector<std::vector<uint8>> Scripts;
		std::vector<std::vector<uint8>> MessageTables;
		/** The disc's own table of each event whose messages the player's replaced; empty for the others. */
		std::vector<std::vector<uint8>> DiscMessageTables;
		std::vector<FReplacedMessages> ReplacedMessages;
		EBattleMessageLayout MessageLayout = EBattleMessageLayout::Original;
	};

	namespace BattleEventScript
	{
		/** Where the actors' count and their entry points are in a script, and how many entries an actor has. */
		inline constexpr std::size_t ActorCountOffset = 0x40;
		inline constexpr std::size_t EntriesOffset = 0x44;
		inline constexpr int32 EntryCount = 8;
		inline constexpr int32 OpcodeCount = 0x4C;

		/** An instruction's length by its opcode (0 past the opcodes). */
		WELTALLCORE_API std::size_t InstructionLength(uint8 Opcode);

		/** Where an instruction jumps (Jump's target, If's when false), when it does. */
		WELTALLCORE_API bool JumpTarget(std::span<const uint8> Code, std::size_t Offset, uint16& Target);
	}

	/**
	 * The event's load: a frame, then the work from the script:
	 * each actor's init thread, the order, the message window's defaults, the Wait counters on. The
	 * script and its messages must outlive the battle. Run it with StepBattleEvent.
	 */
	WELTALLCORE_API void BeginBattleEvent(FBattleContext& Context, std::span<const uint8> Script, std::span<const uint8> Messages,
		EBattleMessageLayout Layout = EBattleMessageLayout::Original, const FReplacedMessages* Replaced = nullptr);

	/**
	 * A hook's run of the scripts: the battle's opening, a turn's boundary, after the
	 * battle. Each actor in the order takes a frame and then up to four instructions, sweep after sweep,
	 * until the sweep after one of them returned; a finished event returns at once.
	 */
	WELTALLCORE_API void BeginBattleEventRun(FBattleContext& Context);

	/** The event's end: its music stopped and freed, its sounds released, a frame after each. */
	WELTALLCORE_API void BeginBattleEventShutdown(FBattleContext& Context);

	/**
	 * The event's routine to the next frame: false once it is over (nothing more to run), true when it
	 * waits for the frame. The frame's work (the show, the HUD, the input) comes between two calls.
	 */
	WELTALLCORE_API bool StepBattleEvent(FBattleContext& Context);

	/**
	 * The message input's step (the input mode 2): the Wait counters count down; the frame's event is
	 * cleared unless a message waits for the button, and a confirm press makes it 4.
	 */
	WELTALLCORE_API void TakeBattleMessageInput(FBattleContext& Context, bool bConfirm);

	/** The HUD's frame of the event's message (in the input mode 2): its text types a tick while it shows. */
	WELTALLCORE_API void TickBattleEventMessage(FBattleContext& Context, FMesContext& Text);

	/** The show's report that combatant slot Slot's signalled sequence has ended. */
	WELTALLCORE_API void FinishBattleEventPerform(FBattleContext& Context, uint8 Slot);

	/** The show's report that combatant slot Slot's scripted animation or motion has ended. */
	WELTALLCORE_API void FinishBattleEventMotion(FBattleContext& Context, uint8 Slot);
}
