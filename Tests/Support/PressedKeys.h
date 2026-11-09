#pragma once

#include "Menu/MenuProgram.h"

#include <deque>
#include <optional>
#include <string>

namespace Weltall::Testing
{
	/** The player's keys as a test presses them: what it queues is taken while the menu listens, dropped when it stops. */
	class FPressedKeys final : public IMenuKeys
	{
	public:
		void Listen(bool bOn, bool bControllerChords) override
		{
			bListening = bOn;
			bChords = bOn && bControllerChords;
			Listens += bOn ? 1 : 0;
			if (!bOn)
			{
				Queue.clear();
			}
		}

		std::optional<FPressedKey> TakePressed() override
		{
			if (!bListening || Queue.empty())
			{
				return std::nullopt;
			}
			FPressedKey Next = Queue.front();
			Queue.pop_front();
			return Next;
		}

		EInputFamily LastFamily() const override { return Family; }
		EButtonIconSet GamepadIcons() const override { return Icons; }

		void Press(std::string Key, int32 Axis = 0) { Queue.push_back({std::move(Key), Axis}); }

		EInputFamily Family = EInputFamily::KeyboardAndMouse;
		EButtonIconSet Icons = EButtonIconSet::Xbox;
		bool bListening = false;
		bool bChords = false;
		/** How many times listening began. */
		int32 Listens = 0;
		std::deque<FPressedKey> Queue;
	};
}
