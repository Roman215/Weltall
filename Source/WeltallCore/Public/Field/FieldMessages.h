#pragma once

#include "Foundation/Types.h"
#include "Graphics/GteMatrix.h"
#include "Text/MesWindow.h"
#include "Text/MessageTable.h"

#include <array>
#include <optional>
#include <span>

namespace Weltall
{
	struct FEventContext;
	struct FReplacedMessages;

	/** Where a message instruction puts its window. */
	enum class EFieldMessageMode : uint8
	{
		/** Sized to the message, above or below its speaker's head (D2, D4, FC). */
		AtSpeaker = 0,
		/** The fixed 72 x 4 box at the top or the bottom, growing from its speaker (D3). */
		FixedFromSpeaker = 1,
		/** The fixed box, growing from the screen's middle (03). */
		Fixed = 2,
		/** Sized to the message, centred in the top or bottom band, growing from its own middle (F5). */
		Centered = 3,
	};

	/** Bits of a message's style: an actor's default (D0) or a message instruction's own byte. */
	namespace FieldMessageStyle
	{
		/** The window of a speaker off screen stays; otherwise it closes as soon as it opens. */
		constexpr uint32 StaysOffScreen = 0x1;
		constexpr uint32 NoPortrait = 0x2;
		/** The portrait's side: 0x4 the right, 0x8 the left, neither the side the speaker faces. */
		constexpr uint32 PortraitRight = 0x4;
		constexpr uint32 PortraitSides = 0xC;
		/** Bits 4-5, the band: 0 the one the speaker's facing gives, 1 the top, 2 the bottom, 3 none (CF, D0 place it). */
		constexpr uint32 PlaceShift = 4;
		constexpr uint32 NoBackground = 0x40;
		/** The window is at its full size at once. */
		constexpr uint32 NoOpening = 0x100;
		/** Added to the style the window keeps when its portrait shows on the right. */
		constexpr uint32 PortraitOnRight = 0x400;
		/** The window takes the player's button whether it is the frontmost or not. */
		constexpr uint32 TakesFocus = 0x800;
	}

	/** Bits of a window's flags. */
	namespace FieldMessageWindowFlags
	{
		/** Put in the top or the bottom band (a window a script places keeps its previous flags). */
		constexpr uint16 Banded = 0x1;
		constexpr uint16 PortraitOnRight = 0x20;
		constexpr uint16 NoBackground = 0x40;
		constexpr uint16 Lower = 0x80;
	}

	/** A rectangle on the field's 320 x 224 screen. */
	struct FFieldScreenRect
	{
		int16 X = 0;
		int16 Y = 0;
		int16 Width = 0;
		int16 Height = 0;

		bool operator==(const FFieldScreenRect&) const = default;
	};

	/**
	 * What a window's drawing shows in a tick, as the field places it: the frame (continuous while it
	 * opens), the cursors and the portrait. The text is the kernel window's rows, the background a
	 * subtractive box inside the frame; the cursors' frame is FFieldMessages::CursorFrame.
	 */
	struct FFieldMessageFrame
	{
		/** The frame as the opening leaves it this tick: growing from where it opens to the window's rect. */
		FFieldScreenRect Rect;
		/** The red arrow after the text while it waits for the button (the frontmost window only). */
		bool bMoreCursor = false;
		int16 MoreCursorX = 0;
		int16 MoreCursorY = 0;
		/** The red arrow before the chosen line of a choice. */
		bool bChoiceCursor = false;
		int16 ChoiceCursorX = 0;
		int16 ChoiceCursorY = 0;
		/** The portrait's quad: the x of its first and second corners (the second left of the first when it faces the other way), its top and bottom. */
		int16 PortraitX0 = 0;
		int16 PortraitX1 = 0;
		int16 PortraitY0 = 0;
		int16 PortraitY1 = 0;
	};

	/** One of the field's four message windows: the kernel's text window and what the field keeps around it. */
	struct FFieldMessageWindow
	{
		static constexpr int16 NoActor = 0xFF;
		/** The age of a free window. */
		static constexpr int16 Free = -1;

		/** The kernel's window while this one is open. */
		std::optional<FMesWindow> Text;
		/** The message it shows: its bytes from its start to the end of the map's table. */
		std::span<const uint8> Message;
		bool bOpen = false;
		/** 0 for the frontmost of the open windows, counting back to the oldest; Free while closed. */
		int16 Age = Free;
		/** It takes the player's button whatever its age. */
		bool bFocus = false;
		/** A script asked for it to close: it closes at the windows' next tick. */
		bool bCloseRequested = false;
		/** The actor whose script opened it, and the one it opened at. */
		int16 Owner = NoActor;
		int16 Speaker = NoActor;
		/** Ticks of its opening left; its text starts when none are. */
		int16 OpenTicks = 0;
		/** FieldMessageWindowFlags. */
		uint16 Flags = 0;
		/** The frame's place once open. */
		FFieldScreenRect Rect;
		/** Where its text starts on the screen. */
		int16 TextX = 0;
		int16 TextY = 0;
		/** The opening: the frame's offset from its place toward where it grows from, and the step per tick (16.16, x then y). */
		std::array<int32, 2> OpenOffset{};
		std::array<int32, 2> OpenStep{};
		/** A choice is offered between its lines. */
		bool bChoice = false;
		/** The choice's first line, how many it has and the cursor: kept from the window's last choice, 0 when the field starts. */
		int16 ChoiceFirst = 0;
		int16 ChoiceCount = 0;
		int16 ChoiceCursor = 0;
		/** Its text waits for the button and offers no choice. */
		bool bTextWaits = false;
		/** Ticks before the "more" cursor shows. */
		int16 MoreCursorDelay = 2;
		/** The portrait beside its text: whose face (0x80 when none shows) and its cell, the cache slot times 2, plus 1 unless it is on the right. */
		bool bPortrait = false;
		uint8 Portrait = 0;
		uint8 PortraitCell = 0;
		/** What its drawing showed at the last tick. */
		FFieldMessageFrame Frame;

		/**
		 * How the opener placed it from its speaker's head, for showing it under another view than the
		 * one it was placed under: whether its x was centred on the head and its y above or below it
		 * (not a script's own place, nor a band's), and the width (in 4-pixel units) and lines the
		 * placing took.
		 */
		struct FHeadPlace
		{
			bool bX = false;
			bool bY = false;
			bool bLower = false;
			int16 Width = 0;
			int16 Lines = 0;
		};
		FHeadPlace HeadPlace;
		/** Counts its openings: a new count is another message's window. */
		uint16 Openings = 0;
	};

	/** One of the three places the field keeps characters' faces in, as messages load them. */
	struct FFieldPortraitSlot
	{
		static constexpr int16 None = 0xFF;

		/** The character whose face it holds. */
		int16 Character = None;
		/** Reading, then Read (its file's memory goes the tick after), then Ready; None before its first face. */
		int16 Load = None;
		/** Which of the field's face reads put its faces here (0 before any): each read places them in VRAM again. */
		uint32 ReadNumber = 0;

		static constexpr int16 Ready = 0;
		static constexpr int16 Reading = 1;
		static constexpr int16 Read = 2;
	};

	/**
	 * The field's message windows: the four windows the scripts open, place, turn into choices and
	 * close, the opening's length (D6), the faces' cache, and what the windows' text reads (the map's
	 * messages, the font and name tables). The field keeps it from map to map; a map's load frees
	 * its windows (EnterMap).
	 */
	struct FFieldMessages
	{
		static constexpr int32 WindowCount = 4;
		static constexpr int32 PortraitSlotCount = 3;
		static constexpr int16 FullRateOpenTicks = 8;

		std::array<FFieldMessageWindow, WindowCount> Windows;
		/** Ticks a window takes to open: 8, or 6 and 4 while the field runs slowed (D6). At 8 the text types a glyph a tick, else two. */
		int16 OpenTicks = FullRateOpenTicks;
		/** Bit w set while window w is open: the player cannot talk to anyone. */
		uint16 OpenMask = 0;
		std::array<FFieldPortraitSlot, PortraitSlotCount> Portraits;
		/** The slot the last face went to: the next goes to the first after it that no open window shows. */
		int32 LastPortraitSlot = 0;
		/** Faces read since the field began, emptied caches or not: the number the next read's slot takes. */
		uint32 PortraitReads = 0;
		/** The cursors' animation, shared by every window: ticks, and the frame (0-4) that advances every fourth. */
		int32 CursorTicks = 0;
		int32 CursorFrame = 0;
		/** Set while the field runs a map's first ticks: no window opens. */
		bool bMapEntering = false;
		/** The map's messages, and which of them are the player's (none without any). */
		FMessageTable Table;
		const FReplacedMessages* Replaced = nullptr;
		/** The font and name tables the windows' text reads, the button map, and the number string they share. */
		FMesContext Text;
		/** Auto-advancing text: how long the windows the button reaches have waited for it. */
		FMesReading Reading;

		/** A map's load: every window closed and free, the faces' cache empty, the opening at its full length. */
		WELTALLCORE_API void EnterMap();

		/** The faces' cache emptied, as a map's load and a dissolve of the screen empty it. */
		WELTALLCORE_API void ResetPortraits();

		/** The open window a script of Actor opened, or -1. */
		WELTALLCORE_API int32 FindOwnWindow(int32 Actor) const;

		/** Asks the window a script of Actor opened, if one is open, to close. */
		WELTALLCORE_API void RequestClose(int32 Actor);

		/** Whether a window offers a choice (the player's controls are off meanwhile). */
		WELTALLCORE_API bool OffersChoice() const;

		WELTALLCORE_API void Close(int32 Window);
		WELTALLCORE_API void CloseAll();

		/** The windows' first step of a tick, after the scripts: finished and dismissed windows close, openings count down. */
		WELTALLCORE_API void Tick();
	};

	/**
	 * Where the opener places a window it places at its speaker's head with the head at (HeadX, HeadY)
	 * on the 320 x 224 screen: its place's x and y (the frame starts 8 above y), held on the screen as
	 * the opener holds them.
	 */
	WELTALLCORE_API std::array<int32, 2> PlaceFieldMessageAtHead(const FFieldMessageWindow::FHeadPlace& Place, int32 HeadX, int32 HeadY);

	/** An actor's head on the screen (its model's point 64 units up), through View with projection distance H. */
	WELTALLCORE_API std::array<int32, 2> FieldMessageSpeakerHead(const FEventContext& Context, uint32 Actor, const FGteMatrix& View, uint16 H);

	/**
	 * The windows' second step, after Tick: the player's button (Pressed & 0x20) on the frontmost or
	 * focused window, the text typed, the opening and the cursors placed, a choice's cursor moved by
	 * Up and Down on the repeat word; the windows then renumbered by age. With auto-advancing text the
	 * button is pressed for the player once every window it reaches has waited for it, without a choice,
	 * as long as the pace gives its glyphs; the tick lasts Vblanks.
	 */
	WELTALLCORE_API void UpdateFieldMessages(FEventContext& Context, uint16 Pressed, uint16 Repeat, int32 Vblanks);
}
