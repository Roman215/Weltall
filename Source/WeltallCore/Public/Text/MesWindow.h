#pragma once

#include "Text/MesFont.h"
#include "Text/MessageTable.h"

#include <array>
#include <deque>
#include <span>
#include <vector>

namespace Weltall
{
	/** What a window's inserts read from the game's state, as the kernel reads them from RAM. */
	class IMesGameState
	{
	public:
		virtual ~IMesGameState() = default;

		/** Name Number of the game state's name table: characters, then gears. */
		virtual std::span<const uint8> Name(int32 Number) const = 0;

		/** The byte a {name} code of 80h or more reads from the party map: a name number, FFh for an empty slot. */
		virtual uint8 PartyName(int32 Code) const = 0;
	};

	/**
	 * The kernel's button map, which the {button} insert names buttons through: the US release swaps
	 * Circle with Cross and Triangle with Square.
	 */
	inline constexpr std::array<uint8, 8> MesKernelButtonMap = {1, 0, 3, 2, 4, 5, 6, 7};

	/** How fast the player's setting has messages typed: as the game types them, two or four times the glyphs a step, or a page at once. */
	enum class EMesTextSpeed : uint8
	{
		Original,
		Double,
		Quadruple,
		Instant,
	};

	/**
	 * What the kernel's message windows read besides their own text. The number string is shared by
	 * every window of the context, as the kernel's is.
	 */
	struct FMesContext
	{
		const FMesFont* Font = nullptr;
		const FMesNameTables* Tables = nullptr;
		const IMesGameState* State = nullptr;
		/** Which physical button each logical one is on (the kernel's button map). */
		std::array<uint8, 8> ButtonMap{};
		std::vector<uint8> NumberString;
		/** The player's text speed. */
		EMesTextSpeed Speed = EMesTextSpeed::Original;
		/**
		 * The French translation's kernel flag that types a variable-width font 8 pixels a glyph (7 a two-byte code's), as
		 * its Enter Name sets it for its grid of letters; the glyphs are still drawn their own width.
		 */
		bool bFixedAdvance = false;
	};

	/** Why a window stopped typing. */
	enum class EMesStop : uint8
	{
		Typing = 0,
		End = 1,
		Page = 2,
		Hold = 3,
	};

	/** A glyph a window typed, from its line's left edge in the font's units (FMesFont::UnitPixels: 4 pixels, or 1). */
	struct FMesPlacedGlyph
	{
		FMesCode Code;
		int16 XUnits = 0;
		/** The glyphs its step had left after it (DrawMesGlyph: a variable-width font's drawing reads it). */
		uint8 StepBudget = 0;

		bool operator==(const FMesPlacedGlyph&) const = default;
	};

	/** The glyphs a tick typed on one line: where they begin among the tick's typed glyphs, how many, and the line's strip. */
	struct FMesTypedLine
	{
		int32 First = 0;
		int32 Count = 0;
		uint8 Strip = 0;
	};

	/** A line's end among glyphs read ahead of the typing (FMesWindow::PendingGlyphs): no glyph of any font. */
	inline constexpr FMesCode MesLineEnd{0xFF, 0xFF};

	/**
	 * The kernel's message window, without its drawing: it takes queued strings one at a time and
	 * types them a few glyphs per tick into its lines, scrolling when they are full; it stops at a
	 * page, a hold or the end until the player's button, pauses and closes on timers, and inserts
	 * names and numbers.
	 */
	class FMesWindow
	{
	public:
		/**
		 * A line of the window as shown: its glyphs, which draw over each other in order, its width (in the font's units),
		 * and the strip of the window's text image it is typed into (two strips share a row of 13 texel rows, each a
		 * plane of it).
		 */
		struct FLine
		{
			std::vector<FMesPlacedGlyph> Glyphs;
			int16 WidthUnits = 0;
			uint8 Strip = 0;
		};

		/** The window's state as the kernel keeps it, which checks against the original compare. */
		struct FState
		{
			int16 X = 0;
			int16 Line = 0;
			int16 TopLine = 0;
			int16 LinesTyped = 0;
			uint16 Flags = 0;
			uint8 Reason = 0;
			bool bClosing = false;
			int16 WaitTimer = 0;
			int16 DelayCounter = 0;
			int16 DelayReload = 0;
			uint8 BaseSpeed = 0;
			uint8 Speed = 0;
			uint8 SavedSpeed = 0;
			int32 Queued = 0;
			/** Each line's typed width, by the line's slot rather than its row. */
			std::vector<int16> Widths;
			/** The code the text goes on with. */
			uint8 Next = 0;
		};

		/** WidthUnits is made odd, as the kernel makes it. */
		WELTALLCORE_API FMesWindow(int32 InWidthUnits, int32 InLines);

		/**
		 * The window set up again on its own memory for new text, as the kernel does it: the queue, the flags, the
		 * timers, the speeds and the lines are new; the cursor, the lines typed and the text position stay as the
		 * last text left them until the next string starts.
		 */
		WELTALLCORE_API void Reinitialize(int32 InWidthUnits, int32 InLines);

		/** Appends a string (copied) to the window's queue; returns how many are queued. */
		WELTALLCORE_API int32 Queue(std::span<const uint8> Text);

		/** One tick of the window: takes the next string when idle, clears a finished page, runs the timers and types. */
		WELTALLCORE_API void Update(FMesContext& Context);

		/** The player's button: go on after a stop; during an auto-advance wait, cancel the ending too. */
		WELTALLCORE_API void Continue();

		/**
		 * A string that ends by itself after a timer is up as long at a faster text speed as at the game's own:
		 * what the faster typing saves is added to its timer, so only how soon its text is whole changes. A wait
		 * the button answers (an auto-advance's, a timed choice's) is never lengthened; a window that becomes a
		 * choice while its timer runs gives the added time back with this.
		 */
		WELTALLCORE_API void DropOwedWait();

		/**
		 * Types String on a fresh first line at once, as the kernel's string renderer does for the menus'
		 * and battles' labels: one step of Glyphs glyphs from the line's start, which stops at a line
		 * end, a page, a hold, the text's end or the first glyph past the window's width. The window's
		 * number variables and selector are the ones the inserts read. Returns the line typed.
		 */
		WELTALLCORE_API const FLine& TypeLine(FMesContext& Context, std::span<const uint8> String, uint8 Glyphs);

		/** Forgets the current text unless a timer runs. */
		WELTALLCORE_API void Reset();

		/** Forgets the current text and the queue, unless a timer runs for the queue. */
		WELTALLCORE_API void Clear();

		/** Forgets the strings still queued, unless a timer runs. */
		WELTALLCORE_API void FreeQueue();

		/**
		 * The lines shown, changed in place as a program writes the window's count: up to the lines it was set up with, the
		 * text and the queue kept.
		 */
		WELTALLCORE_API void SetLines(int32 InLines);

		/** Why the window stopped, or Typing while it types. */
		WELTALLCORE_API EMesStop StopReason() const;

		/** Whether a string is being shown (typed, stopped or waiting to close). */
		bool HasText() const { return (Flags & FlagHasText) != 0; }

		/** Ticks, at two a tick, left of a pause or of the wait before the text ends by itself. */
		int16 WaitTicks() const { return WaitTimer; }

		/** Whether the window has stopped for the player's button (at a page, a hold or the end). */
		bool IsStopped() const { return (Flags & FlagStopped) != 0; }

		/** The line the text types on, counted from the first the window typed. */
		int32 CurrentLine() const { return Line; }

		/** Whether the text ends at the next step: its end was typed, or a timed end waits. */
		bool IsClosing() const { return bClosing; }

		int32 QueuedCount() const { return static_cast<int32>(Queued.size()); }
		int32 WidthUnits() const { return Width; }
		int32 Lines() const { return LineCount; }

		/**
		 * Row Row of the window, from the top, as the latest tick drew it: the kernel draws the lines
		 * before it types, so a glyph shows from the tick after the one that typed it.
		 */
		WELTALLCORE_API const FLine& Row(int32 Row) const;

		/** Whether the latest tick drew the window (it had text), and whether it then typed a step, after which the kernel uploads the typed line's image. */
		bool Drew() const { return bDrew; }
		/** How many lines the latest tick drew: the window's count then (a program can change it after). */
		int32 DrawnLines() const { return DrawnLineCount; }
		bool Stepped() const { return bStepped; }
		/** The glyphs the latest tick typed, in the order typed. */
		std::span<const FMesPlacedGlyph> TypedGlyphs() const { return Typed; }
		/**
		 * The same glyphs line by line, each line with its strip. A tick of the game's own typing stays on the line the
		 * text types on; the player's instant text types a page's lines in one tick, the last of them the current line.
		 */
		std::span<const FMesTypedLine> TypedLines() const { return TypedOn; }
		/**
		 * The glyphs the current line shows next, up to Limit: those this tick's step typed (the rows show them from the
		 * next), then the string's plain ones before its next control code (an end, a line's end, a page, a hold, an
		 * insert); with bNextLine the next line's too, after a MesLineEnd. None while the window is stopped. Out keeps
		 * its memory.
		 */
		WELTALLCORE_API void PendingGlyphs(uint8 LeadByte, std::size_t Limit, std::vector<FMesCode>& Out, bool bNextLine = false) const;
		int32 CurrentStrip() const { return Entries[static_cast<std::size_t>(Line)].Strip; }

		/** Where the text ends, for the "more" cursor: in the font's units from the left (FMesFont::CursorPixels), and the row. */
		int32 CursorXUnits() const { return X; }
		WELTALLCORE_API int32 CursorRow() const;

		/** Glyphs typed a tick (a string's own speed codes change it for a while). */
		void SetSpeed(uint8 GlyphsPerTick) { BaseSpeed = GlyphsPerTick; }

		/** The window draws no box of its own behind the text: its owner draws a frame. */
		void HideBox() { Flags |= FlagNoBox; }
		bool HasBox() const { return (Flags & FlagNoBox) == 0; }

		void SetNumber(int32 Index, int32 Value) { Numbers[static_cast<std::size_t>(Index & 3)] = Value; }
		void SetSelector(uint16 Value) { Selector = Value; }
		void SetHighlightRow(int32 Row) { HighlightRow = static_cast<uint8>(Row); }
		void ClearHighlight() { HighlightRow = NoHighlight; }
		/** The row drawn in the window's colour (a choice's cursor), or -1. */
		int32 HighlightedRow() const { return HighlightRow == NoHighlight ? -1 : HighlightRow; }

		/** The colour the highlighted row's glyphs are modulated by (128 leaves them as they are). */
		void SetColor(uint8 R, uint8 G, uint8 B) { Color = {R, G, B}; }
		const std::array<uint8, 3>& HighlightColor() const { return Color; }

		WELTALLCORE_API FState State() const;

	private:
		static constexpr uint16 FlagScrolling = 0x1;
		static constexpr uint16 FlagNoBox = 0x2;
		static constexpr uint16 FlagHasText = 0x4;
		static constexpr uint16 FlagStopped = 0x8;
		/** Holds the typing like a stop; nothing in the modes studied sets it. */
		static constexpr uint16 FlagHalt = 0x10;
		static constexpr uint16 FlagClearPending = 0x20;
		static constexpr uint16 FlagPagePending = 0x40;
		static constexpr uint16 FlagNested = 0x80;
		static constexpr uint16 FlagTripleSpeed = 0x100;
		static constexpr uint16 FlagAutoAdvance = 0x200;
		static constexpr uint8 NoHighlight = 0xFF;

		void Step(FMesContext& Context);
		/** Times the string just taken at the game's own speed, where the player's is faster: its scrolls and its timed end. */
		void MeasurePace(const FMesContext& Context);
		/** Whether the next step would scroll the window before the game's own speed scrolls it there. */
		bool HoldsScroll() const;
		void StartNewLine();
		/** The byte Offset past the text position; zero past the end of the string. */
		uint8 Peek(std::size_t Offset) const;
		void PushString(std::span<const uint8> Nested);
		void FormatNumber(FMesContext& Context, int32 Value, bool bNarrow, bool bSigned);

		int16 X = 0;
		int16 Line = 0;
		int16 Width;
		int16 LineCount;
		int16 TopLine = 0;
		int16 LinesTyped = 0;
		uint16 Flags = 0;
		uint8 BaseSpeed = 1;
		uint8 Speed = 1;
		uint8 SavedSpeed = 0;
		uint8 Reason = 0;
		bool bClosing = false;
		uint8 Mark = 0;
		uint8 HighlightRow = NoHighlight;
		std::array<uint8, 3> Color{};
		std::array<int32, 4> Numbers{};
		uint16 Selector = 0;
		int16 WaitTimer = 0;
		int16 DelayCounter = 0;
		int16 DelayReload = 0;
		std::deque<std::vector<uint8>> Queued;
		std::vector<uint8> Current;
		std::span<const uint8> Text;
		std::size_t Position = 0;
		std::span<const uint8> SavedText;
		std::size_t SavedPosition = 0;
		std::vector<FLine> Entries;
		std::vector<FLine> Shown;
		int16 DrawnLineCount = 0;
		std::vector<FMesPlacedGlyph> Typed;
		std::vector<FMesTypedLine> TypedOn;
		bool bDrew = false;
		bool bStepped = false;
		/**
		 * The ticks the game's own speed takes over each stretch of the current string between the player's
		 * stops, the last one up to its timed end; empty at the game's speed and for a string without one.
		 */
		std::vector<int32> PaceTicks;
		std::size_t PaceStretch = 0;
		/**
		 * For each stretch of the current string between the player's stops, the ticks into it at which the game's own
		 * speed scrolls the window a line; empty at the game's speed. A line is not scrolled away sooner at a faster one.
		 */
		std::vector<std::vector<int32>> ScrollTicks;
		/** The window's scrolls in the current stretch. */
		std::size_t Scrolled = 0;
		/** The ticks this window has typed for since the string began or the player last went on. */
		int32 StretchTicks = 0;
		/** The part of the wait timer that makes up for the faster typing. */
		int16 OwedWait = 0;
		/**
		 * A string's own slow typing ({delay}) runs, which lasts until the window takes its next string: it types at the
		 * game's own pace whatever the player's text speed, as the authors voiced those lines.
		 */
		bool bAuthoredPace = false;

		/** Each line in its own strip, as a window is set up. */
		void ResetStrips();
	};
}
