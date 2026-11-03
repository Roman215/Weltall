#pragma once

#include "Battling/BattlingSkeleton.h"
#include "Text/MesWindow.h"

#include <array>

namespace Weltall
{
	/** The camera's modes under the script machine. */
	enum class EBattlingScriptCamera : uint8
	{
		/** The fight's camera, placed at once rather than eased. */
		Fight,
		OnPlayerOne,
		OnPlayerTwo,
		/** Close behind player 1's shoulder, high, then lower. */
		BehindPlayerOneHigh,
		BehindPlayerOneLow,
		/** In front of player 2. */
		FacingPlayerTwo,
	};

	/**
	 * The tutorial's script machine and the scripted bout's ending, which runs on the same machine: the script, its message
	 * window, the lesson menu, the pointer, the camera's mode, and the ending's sparks and fades.
	 */
	struct FBattlingTutorial
	{
		static constexpr int32 TextWidth = 64;
		static constexpr int32 TextLines = 4;
		static constexpr int32 MenuWidth = 28;
		static constexpr int32 MenuLines = 8;
		/** Where the windows' text lies on the screen: the message window's left (its top is TextY) and the lesson menu's corner. */
		static constexpr int16 TextX = 28;
		static constexpr int16 MenuX = 162;
		static constexpr int16 MenuY = 42;
		/** Where the windows' text images lie in VRAM, and the kernel's text palettes the program loads for them. */
		static constexpr int16 ImagesX = 0x140;
		static constexpr int16 TextImageY = 0x30;
		static constexpr int16 MenuImageY = 0x70;
		static constexpr int16 PalettesX = 0x140;
		static constexpr int16 PalettesY = 0xFF;

		static constexpr uint16 NoScript = 0xFFFF;
		/** Where the script runs: an offset into the program's scripts, NoScript before one starts. */
		uint16 Script = NoScript;
		/** The fighter the instructions drive (0 or 1), -1 until a script chooses one. */
		int8 Fighter = -1;
		/** A wait's or a timed push's ticks left; 0 before it starts. */
		int32 Counter = 0;
		/** The heat the driven fighter is held at, at least (0 none). */
		int16 HeldHeat = 0;
		/** The message the script asks for and the one the window was given (-1 none yet). */
		int32 Message = 0;
		int32 MessageShown = -1;
		EBattlingScriptCamera Camera = EBattlingScriptCamera::Fight;

		/** The lesson menu is open, and its cursor (0-7: the lessons 1-8). */
		bool bMenuOpen = false;
		int8 MenuCursor = 0;

		/** The pointer at a part of the screen: shown, where it is going and where it is (x, y). */
		bool bPointerShown = false;
		std::array<int32, 2> PointerGoal{};
		std::array<int32, 2> Pointer{};
		/**
		 * What the pass drew of the pointer (first, on the passes whose count has bit 2 set, where the pass before left it) and
		 * of the lesson menu (only when it was open as the pass came to update it).
		 */
		bool bPointerDrawn = false;
		std::array<int32, 2> PointerDrawn{};
		bool bMenuDrawn = false;

		/** The ending's step: 0 the script alone, 11 the breakdown's sounds, 1 the sparks, 2 HP to 1, 3 the white flash, 4 the fade to black. */
		int32 EndingStep = 0;
		struct FSpark
		{
			FBattlingPoint Point{};
			int32 Timer = 0;
		};
		std::array<FSpark, 3> Sparks{};
		/** The flash's and the fade's strength (FFh full): drawn additive in step 3, subtractive in step 4. */
		int16 Fade = 0;
		/** What the frame draws of them over the arena: whether it does, the strength (outside 0-FFh nothing shows), and how. */
		bool bTintDrawn = false;
		int16 Tint = 0;
		bool bTintSubtracts = false;
		/** The ending reached full black and left Battling. */
		bool bLeft = false;

		/** The message window under the arena, and its top on the screen (two lines lower for the lesson menu's prompt). */
		FMesWindow Text{TextWidth, TextLines};
		/** Auto-advancing text: how long a lesson's page has waited for the button, and whether this tick turns it. */
		FMesReading Reading;
		bool bPageRead = false;
		int16 TextY = 154;
		/** Where the pass drew the message window: its place as the pass began, which a lesson can change later in the pass. */
		int16 TextDrawnY = 154;
		/** The lesson menu's list. */
		FMesWindow Menu{MenuWidth, MenuLines};
	};
}
