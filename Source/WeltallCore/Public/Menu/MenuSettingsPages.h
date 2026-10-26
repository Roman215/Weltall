#pragma once

#include "Foundation/Types.h"
#include "Game/PlayerSettings.h"
#include "Input/ButtonGlyphs.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/**
	 * The options screen's pages: the four L1 and R1 turn through first, then the fixes' own list, opened from Fixes,
	 * the CRT's parts, opened from Display's CRT filter, Modding, the last L1 and R1 turn to (its rows here are the
	 * balance's; the screen adds the Assets page's and the mods'), and a mod's own page.
	 */
	enum class ESettingsPage : uint8
	{
		Display,
		Sound,
		Camera,
		Fixes,
		Custom,
		Crt,
		Modding,
		/** Perfect Works Build's parts, opened from Modding's row of the mod where the game found it. */
		PerfectWorks,
	};

	/** How many pages L1 and R1 turn through, from Display. */
	inline constexpr int32 TurnedSettingsPages = 4;

	/** The fixes' presets: every fix on (the default), every fix off, or the player's own choice of each. */
	enum class EFixesPreset : uint8
	{
		PortEnhancements,
		Original,
		Custom,
	};

	/** What a row does when confirmed. */
	enum class ESettingsAction : uint8
	{
		/** Nothing: the row is changed with Left and Right. */
		None,
		/** The fixes' own list opens. */
		OpenCustom,
		/** The CRT's parts open. */
		OpenCrt,
		/** The row put its settings back to their defaults. */
		Reset,
	};

	/**
	 * What the game found that the pages' rows follow: the display's window sizes, the voices and subtitles given, and
	 * whether the player's discs come in through the device's file picker (Android), which the help then names in place
	 * of the folders.
	 */
	struct FSettingsContext
	{
		FDisplayModes Display;
		FSoundSources Sound;
		bool bDiscsPicked = false;
	};

	/** A row of a page as the screen shows it: its label and value, and three lines on what it does. */
	struct FSettingsRow
	{
		std::string Label;
		std::string Value;
		std::array<std::string, 3> Help{};
		/** A heading over the rows after it, which the cursor passes over. */
		bool bHeading = false;
		/** A row shown as it is, which the cursor passes over too. */
		bool bReadOnly = false;
		/** Keys shown as their pictures in the value's place, side by side (an empty Icon a free place); none for a value. */
		std::vector<FButtonGlyph> Cells{};
	};

	/**
	 * The options screen's pages over the player's settings: what each row shows, and what Left, Right and
	 * Confirm do to the settings there. The screen keeps nothing else, except whether the player has picked
	 * Custom while the fixes still match a preset (the preset shown is otherwise read from the fixes).
	 */
	class FSettingsPages
	{
	public:
		WELTALLCORE_API static std::string_view Title(ESettingsPage Page);
		/**
		 * The page's rows; the Display page has its window's rows only where the display offers sizes, the Sound page its
		 * subtitles' source only where some are given.
		 */
		WELTALLCORE_API static int32 RowCount(ESettingsPage Page, const FSettingsContext& Context = {});

		/**
		 * Row of Page as Settings has it; bCustomPicked shows the Fixes page's preset as Custom whatever the fixes. A choice
		 * the game was given nothing to choose between for shows what there is, and in its help why.
		 */
		WELTALLCORE_API static FSettingsRow Row(ESettingsPage Page, int32 Row, const FPlayerSettings& Settings, bool bCustomPicked = false,
			const FSettingsContext& Context = {});

		/**
		 * Left (-1) or Right (+1) on a row: its setting moved to the previous or next choice, wrapping, a
		 * number stepped within its range. False where nothing changed. On the preset row, Custom keeps the
		 * fixes as they are and sets bCustomPicked; the other two set every fix and clear it. The window's
		 * size goes to the next smaller or larger of the display's, from the window's own until one is chosen. A
		 * CRT part changes the look shown (ShownCrtLook) into the player's own, and the filter to Custom.
		 */
		WELTALLCORE_API static bool Step(ESettingsPage Page, int32 Row, int32 Direction, FPlayerSettings& Settings, bool& bCustomPicked,
			const FSettingsContext& Context = {});

		/** Confirm on a row. */
		WELTALLCORE_API static ESettingsAction Activate(ESettingsPage Page, int32 Row, FPlayerSettings& Settings, bool bCustomPicked,
			const FSettingsContext& Context = {});

		/** The preset the fixes match, Custom when they match neither. */
		WELTALLCORE_API static EFixesPreset Preset(const FGameFixes& Fixes);
		/** Every fix as the preset has it (Custom changes nothing). */
		WELTALLCORE_API static void ApplyPreset(EFixesPreset Preset, FGameFixes& Fixes);

		/** The fixes the Custom page lists, each once. */
		WELTALLCORE_API static int32 FixCount();
	};
}
