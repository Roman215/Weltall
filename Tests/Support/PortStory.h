#pragma once

#include "Story/StoryPacks.h"

namespace Weltall::Testing
{
	/**
	 * The port's own story as the checkout's Content/Story holds it, with its example scenes (Content/Examples) as the
	 * expansion, as a player who extracted them plays it; read once per test process, the test failing where it cannot be.
	 */
	const FStoryPacks& PortStory();
}
