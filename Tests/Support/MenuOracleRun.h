#pragma once

#include "Game/GameState.h"
#include "Media/Iso9660.h"
#include "Menu/MenuProgram.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Weltall::Testing
{
	/** Physical buttons in the controller's report word, as the menu oracle's scripts give them. */
	namespace MenuPad
	{
		constexpr uint16 Up = 0x1000;
		constexpr uint16 Down = 0x4000;
		constexpr uint16 Right = 0x2000;
		constexpr uint16 Left = 0x8000;
		constexpr uint16 Cross = 0x0040;
		constexpr uint16 Circle = 0x0020;
		constexpr uint16 Square = 0x0080;
		constexpr uint16 Triangle = 0x0010;
		constexpr uint16 L1 = 0x0004;
		constexpr uint16 R1 = 0x0008;
		constexpr uint16 Select = 0x0100;
		constexpr uint16 Start = 0x0800;
	}

	/** A save on one of the File screen's cards as the oracle's scenarios make it: the new game changed by pokes and its play time, saved to Slot. */
	struct FMenuCardSave
	{
		int32 Slot = 0;
		std::vector<std::pair<std::size_t, std::vector<uint8>>> Pokes = {};
		uint32 PlayTime = 0;
	};

	/**
	 * A run of the menu as the oracle's scenarios
	 * describe it: the menu's mode and parameter, the buttons held a number of vertical blanks at a
	 * time, the frames compared, the console's state and the changes to a new game's state block.
	 */
	struct FMenuScenario
	{
		const char* Name;
		EMenuMode Mode;
		uint8 Param;
		std::vector<std::pair<uint16, int32>> Pad;
		int32 Frames;
		uint32 PlayTime = 0;
		uint32 Seed = 0;
		uint8 LastCommand = 6;
		int32 Disc = 1;
		int32 OutputMode = 1;
		bool bGearLocked = true;
		/** Changes to the new game's state block: offset, bytes. */
		std::vector<std::pair<std::size_t, std::vector<uint8>>> Pokes = {};
		/** The oracle's scenarios run as the original; others the port's fixes. */
		FMenuOptions Options = FMenuOptions::Original();
		/** The run stops at an error the port reports (a part not built yet) instead of failing the test. */
		bool bStopsAtError = false;
		/**
		 * The File screen's two cards, as the first two of the player's stores (a temporary folder); what the
		 * menu leaves on them is part of what it leaves. None: the menu has no stores.
		 */
		std::optional<std::array<std::vector<FMenuCardSave>, 2>> Cards = std::nullopt;
		/** A frame whose digest's bytes the run keeps (FMenuRun::KeptBytes), for comparing it with the oracle's part by part. */
		int32 KeptFrame = -1;
	};

	/** What a scenario's run gave: each frame's digest, what the menu left, and checks on the frames. */
	struct FMenuRun
	{
		std::vector<std::string> Frames;
		std::string Leaves;
		/** The game's state as the menu left it. */
		FGameState Game;
		/** Blend keys given to more than one primitive of a frame, over all frames: the display pairs by them. */
		int32 RepeatedKeys = 0;
		/** The longest Deathblow learning bar drawn (green Gouraud quads), in pixels. */
		int32 LongestLearningBar = 0;
		/** The error the run stopped at, with bStopsAtError. */
		std::string Error;
		/** The bytes the scenario's KeptFrame's digest is made of (MenuFrameBytes). */
		std::vector<uint8> KeptBytes;
	};

	FMenuRun RunMenuScenario(const FIso9660& Disc, const FMenuScenario& Scenario);

	/** Digests joined by spaces, a run of equal ones written once with its length ("a*3"), as the oracle prints them. */
	std::string JoinedDigests(const std::vector<std::string>& Digests);

	/** The scenario on the first disc (skipped without it), its frames and what it leaves as expected. */
	void CheckMenuScenario(const FMenuScenario& Scenario, const char* ExpectedLeaves, const char* ExpectedFrames);

	/** The File screen's saves its scenarios put on the cards. */
	namespace MenuCardSaves
	{
		/** No.1: the new game an hour and a bit in. */
		inline FMenuCardSave FirstSave()
		{
			return {0, {}, 0x3F2A1};
		}

		/** No.3: a party of three who have all joined, with levels, HP and EP of their own, at a later place. */
		inline FMenuCardSave ThirdSave()
		{
			// The party (Fei, Billy, Citan) and both joined masks; the levels; Fei's HP, Billy's EP, Citan's HP; the place.
			return {2,
				{{0x1D34, {0x00, 0x04, 0x02}}, {0x1D30, {0xFF, 0x07, 0xFF, 0x07}}, {0x2CE, {0x0C}}, {0x55E, {0x25}}, {0x416, {0x63}},
					{0x2B8, {0x41, 0x01}}, {0x2BA, {0xE0, 0x01}}, {0x54C, {0x07, 0x00}}, {0x400, {0xE7, 0x03}}, {0x402, {0xE7, 0x03}},
					{0x1930, {0x78, 0x00}}},
				0x123456};
		}

		/** No.2: a game of nearly 1,000 hours at a later place still. */
		inline FMenuCardSave LongSave()
		{
			return {1, {{0x1930, {0x2C, 0x01}}}, 0xCDFE5FF};
		}

		/** A card of fifteen saves, No.1 to No.15, each a day further in. */
		inline std::vector<FMenuCardSave> FullCard()
		{
			std::vector<FMenuCardSave> Card;
			for (int32 Slot = 0; Slot < 15; ++Slot)
			{
				Card.push_back({Slot, {}, 0x100000u + static_cast<uint32>(Slot) * 5184000u});
			}
			return Card;
		}
	}

	/** As CheckMenuScenario, on the French translation's first disc (with the English voices; skipped without it). */
	void CheckFrenchMenuScenario(const FMenuScenario& Scenario, const char* ExpectedLeaves, const char* ExpectedFrames);
}
