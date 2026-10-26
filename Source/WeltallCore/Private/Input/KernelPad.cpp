#include "Input/KernelPad.h"

namespace Weltall
{
	namespace
	{
		/** The face and shoulder buttons' bits in a report word: Circle, Cross, Triangle, Square, L1, L2, R1, R2. */
		constexpr std::array<uint16, 8> ButtonMasks = {0x20, 0x40, 0x10, 0x80, 0x04, 0x01, 0x08, 0x02};
		/** The release's map: button i reads as button ButtonMap[i]. */
		constexpr std::array<std::size_t, 8> ButtonMap = {1, 0, 3, 2, 4, 5, 6, 7};
	}

	uint16 FKernelPad::LogicalButtons(uint16 Report)
	{
		uint16 Logical = Report & 0xFF00;
		for (std::size_t Button = 0; Button < ButtonMasks.size(); ++Button)
		{
			if ((Report & ButtonMasks[Button]) != 0)
			{
				Logical |= ButtonMasks[ButtonMap[Button]];
			}
		}
		return Logical;
	}

	FKernelPadWords FKernelPad::Read(FPort& Port, FPadState Held, uint32 VblankCount)
	{
		FKernelPadWords Words;
		Words.Held = LogicalButtons(ReportWord(Held));
		Words.Pressed = static_cast<uint16>((Words.Held ^ Port.Previous) & Words.Held);
		if (Words.Pressed != 0)
		{
			Port.HoldVblanks = 0;
		}
		Words.Repeated = Words.Held;
		if (Port.HoldVblanks < PacedDelay || (VblankCount % PacedEvery) != 0)
		{
			if (Port.HoldVblanks < PacedDelay)
			{
				++Port.HoldVblanks;
			}
			Words.Repeated = Words.Pressed;
		}
		Port.Previous = Words.Held;
		Words.StickHeld = LogicalButtons(ReportWord(FPadState{Held.StickButtons}));
		Words.StickX = Held.StickX;
		Words.StickY = Held.StickY;
		Words.bAnalog = Held.bAnalog;
		return Words;
	}

	void FKernelPad::SetRepeatPace(int32 Pace)
	{
		PacedDelay = RepeatDelay * Pace;
		PacedEvery = static_cast<uint32>(RepeatEvery * Pace);
	}

	void FKernelPad::Vblank(FPadState Held, uint32 VblankCount, FPadState SecondHeld)
	{
		const std::array<FKernelPadWords, Ports> Words{Read(State[0], Held, VblankCount), Read(State[1], SecondHeld, VblankCount)};
		if (Count < QueueCapacity)
		{
			Queue[static_cast<std::size_t>((First + Count++) % QueueCapacity)] = Words;
		}
		else
		{
			bOverflowed = true;
		}
	}

	FKernelPadWords FKernelPad::TakeMerged()
	{
		FKernelPadWords Merged;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FKernelPadWords& Words = Queue[static_cast<std::size_t>((First + Index) % QueueCapacity)][0];
			Merged.Held |= Words.Held;
			Merged.Pressed |= Words.Pressed;
			Merged.Repeated |= Words.Repeated;
			Merged.StickHeld |= Words.StickHeld;
			Merged.StickX = Words.StickX;
			Merged.StickY = Words.StickY;
			Merged.bAnalog = Words.bAnalog;
		}
		First = 0;
		Count = 0;
		return Merged;
	}

	std::optional<FKernelPadWords> FKernelPad::Pop()
	{
		const std::optional<std::array<FKernelPadWords, Ports>> Both = PopBoth();
		return Both ? std::optional<FKernelPadWords>((*Both)[0]) : std::nullopt;
	}

	std::optional<std::array<FKernelPadWords, FKernelPad::Ports>> FKernelPad::PopBoth()
	{
		if (Count == 0)
		{
			return std::nullopt;
		}
		const std::array<FKernelPadWords, Ports> Words = Queue[static_cast<std::size_t>(First)];
		First = (First + 1) % QueueCapacity;
		--Count;
		return Words;
	}

	void FKernelPad::Reset()
	{
		First = 0;
		Count = 0;
		bOverflowed = false;
	}
}
