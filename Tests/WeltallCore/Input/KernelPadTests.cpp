#include "Input/KernelPad.h"

#include <catch_amalgamated.hpp>

#include <initializer_list>
#include <vector>

using namespace Weltall;

namespace
{
	constexpr uint16 LogicalCircle = 0x20;
	constexpr uint16 LogicalCross = 0x40;
	constexpr uint16 LogicalTriangle = 0x10;
	constexpr uint16 LogicalSquare = 0x80;
	constexpr uint16 LogicalUp = 0x1000;

	FPadState Holding(std::initializer_list<EPadButton> Buttons)
	{
		FPadState State;
		for (const EPadButton Button : Buttons)
		{
			State.Buttons |= FPadState::Bit(Button);
		}
		return State;
	}
}

TEST_CASE("The kernel's pad swaps Circle with Cross and Triangle with Square and passes the rest", "[Input]")
{
	FKernelPad Pad;
	Pad.Vblank(Holding({EPadButton::Cross, EPadButton::Square, EPadButton::Up, EPadButton::L1}), 1);
	const FKernelPadWords Words = Pad.TakeMerged();
	CHECK(Words.Held == (LogicalCircle | LogicalTriangle | LogicalUp | 0x04));
	CHECK(FKernelPad::LogicalButtons(0x0020) == LogicalCross);
	CHECK(FKernelPad::LogicalButtons(0x0010) == LogicalSquare);
}

TEST_CASE("A held button repeats 32 vblanks after the last press, then on every count divisible by 4", "[Input]")
{
	FKernelPad Pad;
	std::vector<uint32> Repeats;
	// The press lands on count 2, so the delay runs out at count 34 and the repeats fall on 36, 40, 44.
	for (uint32 Count = 2; Count <= 45; ++Count)
	{
		Pad.Vblank(Holding({EPadButton::Up}), Count);
		if (Pad.TakeMerged().Repeated != 0)
		{
			Repeats.push_back(Count);
		}
	}
	CHECK(Repeats == std::vector<uint32>{2, 36, 40, 44});
}

TEST_CASE("A held button repeats in the player's time when the vertical blanks come faster", "[Input]")
{
	// At three times the pace the delay is 96 vertical blanks and the repeats fall on every count divisible by 12.
	FKernelPad Pad;
	Pad.SetRepeatPace(3);
	std::vector<uint32> Repeats;
	for (uint32 Count = 2; Count <= 130; ++Count)
	{
		Pad.Vblank(Holding({EPadButton::Up}), Count);
		if (Pad.TakeMerged().Repeated != 0)
		{
			Repeats.push_back(Count);
		}
	}
	CHECK(Repeats == std::vector<uint32>{2, 108, 120});

	// Twice: 64 and 8. A press is a press at any pace.
	FKernelPad Twice;
	Twice.SetRepeatPace(2);
	Repeats.clear();
	for (uint32 Count = 1; Count <= 82; ++Count)
	{
		Twice.Vblank(Holding({EPadButton::Up}), Count);
		const FKernelPadWords Words = Twice.TakeMerged();
		CHECK(Words.Pressed == (Count == 1 ? LogicalUp : 0));
		if (Words.Repeated != 0)
		{
			Repeats.push_back(Count);
		}
	}
	CHECK(Repeats == std::vector<uint32>{1, 72, 80});

	// Back at the game's own pace a button still held repeats as the console's does.
	Twice.SetRepeatPace(1);
	Repeats.clear();
	for (uint32 Count = 83; Count <= 92; ++Count)
	{
		Twice.Vblank(Holding({EPadButton::Up}), Count);
		if (Twice.TakeMerged().Repeated != 0)
		{
			Repeats.push_back(Count);
		}
	}
	CHECK(Repeats == std::vector<uint32>{84, 88, 92});
}

TEST_CASE("The kernel's pad merges queued vblanks so a tick sees a press released before it, and holds 16", "[Input]")
{
	FKernelPad Pad;
	Pad.Vblank(Holding({EPadButton::Cross}), 1);
	Pad.Vblank(Holding({}), 2);
	const FKernelPadWords Merged = Pad.TakeMerged();
	CHECK(Merged.Pressed == LogicalCircle);
	CHECK(Merged.Held == LogicalCircle);
	CHECK(Pad.TakeMerged() == FKernelPadWords{});

	for (uint32 Count = 0; Count < FKernelPad::QueueCapacity; ++Count)
	{
		Pad.Vblank(Holding({}), 3 + Count);
	}
	Pad.Vblank(Holding({EPadButton::Triangle}), 100);
	CHECK(Pad.TakeMerged().Pressed == 0);
	// The lost press still counts as held from then on: the next vblank sees no new press.
	Pad.Vblank(Holding({EPadButton::Triangle}), 101);
	CHECK(Pad.TakeMerged().Pressed == 0);
}

TEST_CASE("The kernel's pad hands its queue out one vblank at a time and marks an overflow until a reset", "[Input]")
{
	FKernelPad Pad;
	CHECK_FALSE(Pad.Pop().has_value());
	Pad.Vblank(Holding({EPadButton::Cross}), 1);
	Pad.Vblank(Holding({}), 2);
	CHECK(Pad.Pop()->Pressed == LogicalCircle);
	Pad.Vblank(Holding({EPadButton::Up}), 3);
	CHECK(Pad.Pop()->Held == 0);
	CHECK(Pad.Pop()->Pressed == LogicalUp);
	CHECK_FALSE(Pad.Pop().has_value());

	for (uint32 Count = 0; Count <= FKernelPad::QueueCapacity; ++Count)
	{
		Pad.Vblank(Holding({}), 4 + Count);
	}
	CHECK(Pad.Overflowed());
	CHECK(Pad.Pop().has_value());
	CHECK(Pad.Overflowed());
	Pad.Reset();
	CHECK_FALSE(Pad.Overflowed());
	CHECK_FALSE(Pad.Pop().has_value());
}

TEST_CASE("The kernel queues both ports' words together, each with its own presses and repeat", "[Input]")
{
	FKernelPad Pad;
	CHECK(Pad.IsConnected());
	CHECK_FALSE(Pad.IsConnected(1));
	Pad.Vblank(Holding({EPadButton::Cross}), 1, Holding({EPadButton::Up}));
	Pad.Vblank(Holding({EPadButton::Cross}), 2, Holding({EPadButton::Up, EPadButton::Square}));
	const auto First = Pad.PopBoth();
	REQUIRE(First.has_value());
	CHECK((*First)[0].Pressed == LogicalCircle);
	CHECK((*First)[1].Pressed == LogicalUp);
	const auto Second = Pad.PopBoth();
	REQUIRE(Second.has_value());
	CHECK((*Second)[0] == FKernelPadWords{LogicalCircle, 0, 0});
	CHECK((*Second)[1] == FKernelPadWords{static_cast<uint16>(LogicalUp | LogicalTriangle), LogicalTriangle, LogicalTriangle});
	CHECK_FALSE(Pad.PopBoth().has_value());

	// A press on one port restarts that port's repeat alone: the second's held button repeats on, the first's waits again.
	FKernelPad Repeating;
	for (uint32 Count = 1; Count <= 40; ++Count)
	{
		Repeating.Vblank(Holding({EPadButton::Up}), Count, Holding({EPadButton::Down}));
	}
	Repeating.Reset();
	Repeating.Vblank(Holding({EPadButton::Up, EPadButton::Cross}), 44, Holding({EPadButton::Down}));
	const auto Words = Repeating.PopBoth();
	REQUIRE(Words.has_value());
	CHECK((*Words)[0].Repeated == LogicalCircle);
	CHECK((*Words)[1].Repeated == 0x4000);

	// The modes that read one controller take the first port's words off the same queue.
	Pad.Vblank(Holding({EPadButton::Triangle}), 3, Holding({EPadButton::Start}));
	CHECK(Pad.TakeMerged().Pressed == LogicalSquare);
	CHECK_FALSE(Pad.PopBoth().has_value());
}

TEST_CASE("The kernel's words carry each port's stick and the directions only it holds", "[Input]")
{
	FKernelPad Pad;
	FPadState First = Holding({EPadButton::Up, EPadButton::Square});
	First.StickButtons = FPadState::Bit(EPadButton::Up);
	First.StickX = 0x40;
	First.StickY = 0x00;
	First.bAnalog = true;
	Pad.Vblank(First, 1);
	const auto Words = Pad.PopBoth();
	REQUIRE(Words.has_value());
	CHECK((*Words)[0].Held == (LogicalUp | LogicalTriangle));
	CHECK((*Words)[0].StickHeld == LogicalUp);
	CHECK((*Words)[0].StickX == 0x40);
	CHECK((*Words)[0].StickY == 0x00);
	CHECK((*Words)[0].bAnalog);
	CHECK((*Words)[1].StickX == FPadState::StickCentre);
	CHECK_FALSE((*Words)[1].bAnalog);
}

TEST_CASE("The field's merged words keep the stick where the tick's last vblank left it", "[Input]")
{
	constexpr uint16 LogicalRight = 0x2000;
	FKernelPad Pad;
	FPadState Up = Holding({EPadButton::Up});
	Up.StickButtons = FPadState::Bit(EPadButton::Up);
	Up.StickY = 0x00;
	Up.bAnalog = true;
	FPadState Right = Holding({EPadButton::Right});
	Right.StickButtons = FPadState::Bit(EPadButton::Right);
	Right.StickX = 0xFF;
	Right.bAnalog = true;
	Pad.Vblank(Up, 1);
	Pad.Vblank(Right, 2);
	const FKernelPadWords Merged = Pad.TakeMerged();
	CHECK(Merged.Held == (LogicalUp | LogicalRight));
	CHECK(Merged.StickHeld == (LogicalUp | LogicalRight));
	CHECK(Merged.StickX == 0xFF);
	CHECK(Merged.StickY == FPadState::StickCentre);
	CHECK(Merged.bAnalog);
}
