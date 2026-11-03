#pragma once

#include "Foundation/Types.h"
#include "Input/PadState.h"

#include <array>
#include <optional>

namespace Weltall
{
	/** A controller's words as the kernel keeps them for a vertical blank, in the game's logical buttons. */
	struct FKernelPadWords
	{
		uint16 Held = 0;
		/** Held now and not at the vertical blank before. */
		uint16 Pressed = 0;
		/** Pressed, or every held button when it repeats. */
		uint16 Repeated = 0;
		/** The directions among Held that only the stick holds (FPadState::StickButtons). */
		uint16 StickHeld = 0;
		/** The analog stick's report, and whether the controller is analog: what the arena reads of a DualShock. */
		uint8 StickX = FPadState::StickCentre;
		uint8 StickY = FPadState::StickCentre;
		bool bAnalog = false;
		/** L1 and R1 among Held that zoom the view (FPadState::ZoomButtons). */
		uint16 ZoomHeld = 0;

		bool operator==(const FKernelPadWords&) const = default;
	};

	/**
	 * The two controller ports as the kernel reads them at every vertical blank for the game's modes, digital
	 * pads: each one's buttons turned into the game's logical ones (the US release swaps Circle with Cross and
	 * Triangle with Square), the newly pressed ones, and the repeat (the held buttons 32 vertical blanks after
	 * the port's last new press, then on every vertical blank whose count is a multiple of 4), both ports' words
	 * queued together for the mode's next tick; a queue already holding 16 loses the new ones and marks the
	 * overflow, which only a reset clears. Every mode reads the first port; the arena reads both, and a gamepad's
	 * stick with them, as a DualShock's in analog mode. A kernel with the Japanese commands swaps the first port's
	 * face buttons before the map, the second's not.
	 */
	class FKernelPad
	{
	public:
		static constexpr int32 Ports = 2;
		static constexpr int32 QueueCapacity = 16;
		static constexpr int32 RepeatDelay = 32;
		static constexpr int32 RepeatEvery = 4;

		/** The first port with a controller, the second without. */
		FKernelPad() { State[0].bConnected = true; }

		/** The controller's report word: bit 15 Left to bit 8 Select, bit 7 Square to bit 0 L2. */
		static constexpr uint16 ReportWord(FPadState Held) { return static_cast<uint16>((Held.Buttons & 0xFF) << 8 | Held.Buttons >> 8); }

		/** The game's logical buttons for a report word: the face buttons through the release's map. */
		WELTALLCORE_API static uint16 LogicalButtons(uint16 Report);

		/**
		 * A report word with its face buttons swapped as the Italian translation's Japanese commands swap the first
		 * port's before the release's map: Circle with Cross, Square with Triangle. Each pair's second test undoes the
		 * first's where both are held, so Circle and Cross together read as Circle alone, Square and Triangle as Square.
		 */
		WELTALLCORE_API static uint16 SwapFaceButtons(uint16 Report);

		/** The game's logical buttons for a report word of the first port, as a kernel with or without the Japanese commands reads it. */
		static uint16 FirstPortButtons(uint16 Report, bool bJapaneseCommands)
		{
			return LogicalButtons(bJapaneseCommands ? SwapFaceButtons(Report) : Report);
		}

		/** Whether the kernel has the Japanese commands (FDiscIdentity::bJapaneseCommands); set at power-on. */
		bool HasJapaneseCommands() const { return bJapaneseCommands; }
		void SetJapaneseCommands(bool bInJapaneseCommands) { bJapaneseCommands = bInJapaneseCommands; }

		/** The first port's logical buttons for what its controller holds, as this kernel reads them. */
		uint16 FirstPortButtons(FPadState Held) const { return FirstPortButtons(ReportWord(Held), bJapaneseCommands); }

		/**
		 * One vertical blank with Held down on the first port's controller and SecondHeld on the second's;
		 * VblankCount is the console's count once this one is counted.
		 */
		WELTALLCORE_API void Vblank(FPadState Held, uint32 VblankCount, FPadState SecondHeld = {});

		/**
		 * Every queued vertical blank's words of the first port ORed, the queue emptied: how the field reads it each tick.
		 * The stick is where the last of them left it.
		 */
		WELTALLCORE_API FKernelPadWords TakeMerged();

		/** The oldest queued vertical blank's words of the first port, taken off the queue: how the menus read it, one at a time. */
		WELTALLCORE_API std::optional<FKernelPadWords> Pop();

		/** The oldest queued vertical blank's words of both ports, taken off the queue: how the arena reads it. */
		WELTALLCORE_API std::optional<std::array<FKernelPadWords, Ports>> PopBoth();

		/** Whether a vertical blank found the queue full since the last reset. */
		bool Overflowed() const { return bOverflowed; }

		/** Empties the queue and forgets an overflow, as the kernel's reset of the input does; the held state and repeat go on. */
		WELTALLCORE_API void Reset();

		/** The first port's logical buttons held at the last vertical blank. */
		uint16 Held() const { return State[0].Previous; }

		/**
		 * Whether a port has a controller, as the modes ask before their frames. The first port's is the player's:
		 * false while they have none (the controller in use was pulled and nothing else has been used since), when
		 * the modes wait for one. The second port's is a second player's, which only the arena asks for.
		 */
		bool IsConnected(int32 Port = 0) const { return State[static_cast<std::size_t>(Port)].bConnected; }
		void SetConnected(bool bInConnected, int32 Port = 0) { State[static_cast<std::size_t>(Port)].bConnected = bInConnected; }

		/** Whether a port's controller has a motor the kernel's vibration can run. */
		bool HasMotor(int32 Port) const { return State[static_cast<std::size_t>(Port)].bMotor; }
		void SetMotor(int32 Port, bool bInMotor) { State[static_cast<std::size_t>(Port)].bMotor = bInMotor; }

		/**
		 * How many vertical blanks pass in the time one does on the console (1 unless a battle is fast-forwarded):
		 * the repeat's delay and step last as many times the vertical blanks, so a held button repeats at the pace
		 * the player's hand knows however fast the game runs. The count given at each vertical blank then goes up
		 * by one each, as ever.
		 */
		WELTALLCORE_API void SetRepeatPace(int32 Pace);

	private:
		struct FPort
		{
			uint16 Previous = 0;
			int32 HoldVblanks = 0;
			bool bConnected = false;
			bool bMotor = false;
		};

		FKernelPadWords Read(FPort& Port, uint16 Logical, FPadState Held, uint32 VblankCount);

		std::array<FPort, Ports> State{};
		bool bJapaneseCommands = false;
		/** The repeat's delay and step at the pace set, in vertical blanks. */
		int32 PacedDelay = RepeatDelay;
		uint32 PacedEvery = RepeatEvery;
		std::array<std::array<FKernelPadWords, Ports>, QueueCapacity> Queue{};
		int32 First = 0;
		int32 Count = 0;
		bool bOverflowed = false;
	};
}
