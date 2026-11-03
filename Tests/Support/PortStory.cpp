#include "Support/PortStory.h"

#include "Mods/ExpansionMods.h"

#include <catch_amalgamated.hpp>

#include <filesystem>

namespace Weltall::Testing
{
	namespace
	{
		TResult<FStoryPacks> ReadPortStory()
		{
			TResult<FStoryPacks> Story = FStoryPacks::Read(WELTALL_STORY_DIR);
			if (Story)
			{
				if (std::optional<FError> Error = Story->ReadExpansion(std::filesystem::path(WELTALL_EXAMPLES_DIR) / "disc2", ExamplesFolder))
				{
					return *Error;
				}
			}
			return Story;
		}
	}

	const FStoryPacks& PortStory()
	{
		static const TResult<FStoryPacks> Story = ReadPortStory();
		INFO((Story ? std::string() : Story.Error().Message));
		REQUIRE(Story);
		return *Story;
	}
}
