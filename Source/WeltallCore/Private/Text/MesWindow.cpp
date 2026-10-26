#include "Text/MesWindow.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/**
		 * A line ended by 01 moves the cursor past any width, so the next step starts a new line; a variable-width font's
		 * program counts the cursor in pixels, and moves it further.
		 */
		constexpr int16 LineEndX = 100;
		constexpr int16 VariableWidthLineEndX = 0x400;

		bool IsVariableWidth(const FMesContext& Context)
		{
			return Context.Font != nullptr && Context.Font->IsVariableWidth();
		}

		/** Kernel name tables the inserts read. */
		constexpr int32 ArmorTable = 17;
		constexpr int32 ItemTable = 22;
		constexpr int32 WeaponTable = 23;
		constexpr int32 PlaceholderTable24 = 24;
		constexpr int32 PlaceholderTable25 = 25;
		constexpr int32 PartyNamesTable = 26;
		constexpr int32 ButtonNamesTable = 49;
		constexpr int32 GearPartTable = 50;
		constexpr int32 GearWeaponTable = 51;
		constexpr uint8 EmptyPartySlot = 0xFF;

		/** Glyph-map indices of numbers: 16 per set (full-width, narrow), the digits then minus and plus. */
		constexpr int32 GlyphSetSize = 16;
		constexpr int32 MinusIndex = 10;
		constexpr int32 PlusIndex = 11;
	}

	FMesWindow::FMesWindow(int32 InWidthUnits, int32 InLines)
		: Width(static_cast<int16>(InWidthUnits | 1)), LineCount(static_cast<int16>(InLines)), Entries(static_cast<std::size_t>(InLines)),
		  Shown(static_cast<std::size_t>(InLines))
	{
		ResetStrips();
	}

	void FMesWindow::ResetStrips()
	{
		for (std::size_t Entry = 0; Entry < Entries.size(); ++Entry)
		{
			Entries[Entry].Strip = static_cast<uint8>(Entry);
		}
	}

	void FMesWindow::Reinitialize(int32 InWidthUnits, int32 InLines)
	{
		Width = static_cast<int16>(InWidthUnits | 1);
		LineCount = static_cast<int16>(InLines);
		Flags = 0;
		WaitTimer = 0;
		Queued.clear();
		BaseSpeed = 1;
		Speed = 1;
		bClosing = false;
		SavedSpeed = 0;
		Mark = 0;
		Reason = 0;
		bAuthoredPace = false;
		HighlightRow = NoHighlight;
		Entries.assign(static_cast<std::size_t>(InLines), FLine{});
		Shown.assign(static_cast<std::size_t>(InLines), FLine{});
		ResetStrips();
		Typed.clear();
		TypedOn.clear();
		bDrew = false;
		bStepped = false;
	}

	int32 FMesWindow::Queue(std::span<const uint8> String)
	{
		Queued.emplace_back(String.begin(), String.end());
		return static_cast<int32>(Queued.size());
	}

	void FMesWindow::Continue()
	{
		const uint16 Before = Flags;
		Flags &= ~FlagStopped;
		if ((Before & FlagStopped) != 0)
		{
			++PaceStretch;
			StretchTicks = 0;
			Scrolled = 0;
		}
		if ((Before & FlagAutoAdvance) != 0)
		{
			WaitTimer = 0;
			bClosing = false;
			Flags &= ~FlagAutoAdvance;
		}
	}

	const FMesWindow::FLine& FMesWindow::TypeLine(FMesContext& Context, std::span<const uint8> String, uint8 Glyphs)
	{
		X = 0;
		Line = 0;
		Flags = 0;
		BaseSpeed = 1;
		Speed = Glyphs;
		SavedSpeed = 0;
		bClosing = false;
		bAuthoredPace = false;
		WaitTimer = 0;
		Text = String;
		Position = 0;
		FLine& First = Entries.front();
		First.WidthUnits = 0;
		First.Glyphs.clear();
		Typed.clear();
		TypedOn.clear();
		Step(Context);
		return First;
	}

	void FMesWindow::Reset()
	{
		if (WaitTimer == 0)
		{
			bClosing = false;
			Flags &= FlagNoBox;
		}
	}

	void FMesWindow::Clear()
	{
		bClosing = false;
		WaitTimer = 0;
		Flags &= FlagNoBox;
		Queued.clear();
	}

	void FMesWindow::FreeQueue()
	{
		if (WaitTimer == 0)
		{
			Queued.clear();
		}
	}

	void FMesWindow::SetLines(int32 InLines)
	{
		LineCount = static_cast<int16>(std::clamp(InLines, 1, static_cast<int32>(Entries.size())));
	}

	EMesStop FMesWindow::StopReason() const
	{
		return (Flags & FlagStopped) != 0 ? static_cast<EMesStop>(Reason) : EMesStop::Typing;
	}

	const FMesWindow::FLine& FMesWindow::Row(int32 Row) const
	{
		return Shown[static_cast<std::size_t>(Row)];
	}

	FMesWindow::FState FMesWindow::State() const
	{
		FState Result;
		Result.X = X;
		Result.Line = Line;
		Result.TopLine = TopLine;
		Result.LinesTyped = LinesTyped;
		Result.Flags = Flags;
		Result.Reason = Reason;
		Result.bClosing = bClosing;
		Result.WaitTimer = WaitTimer;
		Result.DelayCounter = DelayCounter;
		Result.DelayReload = DelayReload;
		Result.BaseSpeed = BaseSpeed;
		Result.Speed = Speed;
		Result.SavedSpeed = SavedSpeed;
		Result.Queued = static_cast<int32>(Queued.size());
		for (const FLine& Entry : Entries)
		{
			Result.Widths.push_back(Entry.WidthUnits);
		}
		Result.Next = Peek(0);
		return Result;
	}

	int32 FMesWindow::CursorRow() const
	{
		const int32 Row = Line - TopLine;
		return Row < 0 ? LineCount - 1 : Row;
	}

	void FMesWindow::Update(FMesContext& Context)
	{
		bDrew = false;
		bStepped = false;
		Typed.clear();
		TypedOn.clear();
		if ((Flags & FlagHasText) == 0)
		{
			if (Queued.empty())
			{
				// Nothing is drawn while the window has no text.
				for (FLine& Row : Shown)
				{
					Row.WidthUnits = 0;
					Row.Glyphs.clear();
				}
				return;
			}
			Current = std::move(Queued.front());
			Queued.pop_front();
			Text = Current;
			Position = 0;
			Flags = static_cast<uint16>((Flags & FlagNoBox) | FlagHasText | FlagClearPending);
			if (SavedSpeed != 0)
			{
				BaseSpeed = SavedSpeed;
				SavedSpeed = 0;
			}
			DelayReload = 0;
			DelayCounter = 0;
			bAuthoredPace = false;
			MeasurePace(Context);
		}
		if ((Flags & FlagStopped) == 0)
		{
			++StretchTicks;
		}
		Speed = (Flags & FlagTripleSpeed) != 0 ? static_cast<uint8>(BaseSpeed * 3) : BaseSpeed;
		if ((Flags & FlagPagePending) != 0 && (Flags & FlagStopped) == 0)
		{
			Flags = static_cast<uint16>((Flags & ~FlagPagePending) | FlagClearPending);
		}
		if ((Flags & FlagClearPending) != 0)
		{
			TopLine = 0;
			LinesTyped = 0;
			Line = 0;
			X = 0;
			// The lines the window shows now: a window set up with more keeps the rest.
			for (int32 Row = 0; Row < LineCount; ++Row)
			{
				Entries[static_cast<std::size_t>(Row)].WidthUnits = 0;
				Entries[static_cast<std::size_t>(Row)].Glyphs.clear();
			}
			// Only the first line goes back to its own strip; the others take theirs as they start.
			if (!Entries.empty())
			{
				Entries.front().Strip = 0;
			}
			Flags &= ~(FlagClearPending | FlagScrolling);
		}
		for (int32 Row = 0; Row < LineCount; ++Row)
		{
			Shown[static_cast<std::size_t>(Row)] = Entries[static_cast<std::size_t>((TopLine + Row) % LineCount)];
		}
		DrawnLineCount = LineCount;
		bDrew = true;
		if (WaitTimer != 0)
		{
			--WaitTimer;
		}
		else if (DelayCounter != 0)
		{
			--DelayCounter;
		}
		else
		{
			DelayCounter = DelayReload;
			if ((Flags & (FlagStopped | FlagHalt | FlagPagePending)) == 0 && !HoldsScroll())
			{
				Step(Context);
				bStepped = true;
			}
			// Instant text types on to the page's stop, a pause, a scroll the game's own speed has not come to, a string's
			// own slow typing, or the end.
			constexpr int32 InstantSteps = 256;
			for (int32 Extra = 0; Context.Speed == EMesTextSpeed::Instant && !bAuthoredPace && Extra < InstantSteps && WaitTimer == 0
				&& (Flags & (FlagStopped | FlagHalt | FlagPagePending)) == 0 && (Flags & FlagHasText) != 0 && !bClosing && !HoldsScroll(); ++Extra)
			{
				Step(Context);
			}
		}
		// The wait timer runs down twice a tick: a pause lasts half the value it was written with.
		if (WaitTimer != 0)
		{
			--WaitTimer;
		}
		Flags &= ~FlagTripleSpeed;
	}

	void FMesWindow::StartNewLine()
	{
		X = 0;
		++Line;
		++LinesTyped;
		if (Line >= LineCount)
		{
			Line = 0;
			Flags |= FlagScrolling;
		}
		if ((Flags & FlagScrolling) != 0)
		{
			++Scrolled;
			Entries[static_cast<std::size_t>(TopLine)].WidthUnits = 0;
			TopLine = static_cast<int16>(TopLine + 1 >= LineCount ? 0 : TopLine + 1);
		}
		// The new line's image starts over where its glyphs are drawn; what it shows is only those. Its strip is
		// the next of one more than the window's lines, so a line scrolled up keeps its own while the new one types.
		FLine& Entry = Entries[static_cast<std::size_t>(Line)];
		Entry.Glyphs.clear();
		Entry.Strip = static_cast<uint8>(LinesTyped % (LineCount + 1));
	}

	void FMesWindow::PendingGlyphs(uint8 LeadByte, std::size_t Limit, std::vector<FMesCode>& Out, bool bNextLine) const
	{
		Out.clear();
		if ((Flags & (FlagHasText | FlagStopped)) != FlagHasText)
		{
			return;
		}
		// The rows show what was typed before this tick's step: its glyphs come first.
		for (const FMesPlacedGlyph& Glyph : Typed)
		{
			Out.push_back(Glyph.Code);
		}
		for (std::size_t At = Position; At < Text.size() && Out.size() < Limit;)
		{
			const uint8 Code = Text[At];
			if (Code == 0x01 && bNextLine)
			{
				Out.push_back(MesLineEnd);
				bNextLine = false;
				++At;
				continue;
			}
			if (Code <= 0x03 || Code == 0x0F)
			{
				return;
			}
			if (Code >= LeadByte && At + 1 < Text.size())
			{
				Out.push_back({Code, Text[At + 1]});
				At += 2;
			}
			else
			{
				Out.push_back({0, Code});
				++At;
			}
		}
	}

	uint8 FMesWindow::Peek(std::size_t Offset) const
	{
		return Position + Offset < Text.size() ? Text[Position + Offset] : 0;
	}

	void FMesWindow::PushString(std::span<const uint8> Nested)
	{
		SavedText = Text;
		SavedPosition = Position;
		Text = Nested;
		Position = 0;
		Flags |= FlagNested;
	}

	void FMesWindow::FormatNumber(FMesContext& Context, int32 Value, bool bNarrow, bool bSigned)
	{
		const int32 Set = bNarrow ? GlyphSetSize : 0;
		int32 Sign = 0;
		uint32 Magnitude = static_cast<uint32>(Value);
		if (bSigned)
		{
			Sign = PlusIndex;
			if (Value < 0)
			{
				Magnitude = 0u - Magnitude;
				Sign = MinusIndex;
			}
		}
		std::array<int32, 10> Digits{};
		uint32 Power = 1000000000u;
		for (int32& Digit : Digits)
		{
			Digit = static_cast<int32>(Magnitude / Power) + Set;
			Magnitude %= Power;
			Power /= 10;
		}
		// Leading zeros go, but the last digit always stays; a sign goes right before the first digit kept.
		std::size_t First = 0;
		while (First + 1 < Digits.size() && Digits[First] == Set)
		{
			++First;
		}
		std::vector<int32> Indices;
		if (Sign != 0)
		{
			Indices.push_back(Sign + Set);
		}
		Indices.insert(Indices.end(), Digits.begin() + static_cast<std::ptrdiff_t>(First), Digits.end());
		Context.NumberString.clear();
		for (const int32 Index : Indices)
		{
			const std::span<const uint8> Entry = Context.Tables != nullptr ? Context.Tables->GlyphMapEntry(Index) : std::span<const uint8>();
			if (Entry.size() == 2)
			{
				if (Entry[0] != 0)
				{
					Context.NumberString.push_back(Entry[0]);
				}
				Context.NumberString.push_back(Entry[1]);
			}
		}
		Context.NumberString.push_back(0);
	}

	void FMesWindow::Step(FMesContext& Context)
	{
		// A variable-width font's cursor counts pixels against the window's width in units of 4.
		const bool bVariableWidth = IsVariableWidth(Context);
		if (X > (bVariableWidth ? Width * 4 : Width))
		{
			StartNewLine();
		}
		if (bClosing)
		{
			bClosing = false;
			Flags &= ~FlagHasText;
			return;
		}
		const auto TableString = [&Context](int32 Table, int32 Index)
		{
			return Context.Tables != nullptr ? Context.Tables->String(Table, Index) : std::span<const uint8>();
		};
		// Glyphs left in this step, less one: most codes cost one, inserts nothing.
		const int32 Scale = bAuthoredPace ? 1 : Context.Speed == EMesTextSpeed::Double ? 2 : Context.Speed == EMesTextSpeed::Quadruple ? 4 : 1;
		int32 Budget = Speed * Scale - 1;
		while (Budget != -1)
		{
			const uint8 Code = Peek(0);
			if (Code == 0x00)
			{
				if ((Flags & FlagNested) == 0)
				{
					Flags |= FlagStopped;
					Reason = static_cast<uint8>(EMesStop::End);
					bClosing = true;
					return;
				}
				Flags &= ~FlagNested;
				Text = SavedText;
				Position = SavedPosition + 1;
				--Budget;
				continue;
			}
			if (Code == 0x03)
			{
				Reason = static_cast<uint8>(EMesStop::Hold);
				Flags |= FlagStopped;
				++Position;
				return;
			}
			if (Code == 0x02)
			{
				Reason = static_cast<uint8>(EMesStop::Page);
				Flags |= FlagStopped | FlagPagePending;
				Position += Peek(1) == 0x01 ? 2 : 1;
				return;
			}
			if (Code == 0x01)
			{
				X = bVariableWidth ? VariableWidthLineEndX : LineEndX;
				++Position;
				return;
			}
			if (Code != 0x0F)
			{
				const bool bTwoBytes = Context.Font != nullptr && Code >= Context.Font->LeadByte();
				const FMesCode Glyph = bTwoBytes ? FMesCode{Code, Peek(1)} : FMesCode{0, Code};
				const int16 GlyphWidth = bVariableWidth && Context.bFixedAdvance ? static_cast<int16>(bTwoBytes ? 7 : 8)
					: static_cast<int16>(Context.Font != nullptr ? Context.Font->WidthUnits(Glyph) : 3);
				if (!bVariableWidth && X + GlyphWidth > Width)
				{
					// The glyph goes to the next line, which the next step starts (a variable-width font's program types it
					// on: only the text's own line ends break its lines).
					X = static_cast<int16>(X + GlyphWidth);
					return;
				}
				FLine& Entry = Entries[static_cast<std::size_t>(Line)];
				const uint8 Left = static_cast<uint8>(std::clamp(Budget, 0, 0xFF));
				Entry.Glyphs.push_back({Glyph, X, Left});
				// Lines typed one after the other never share a strip.
				if (TypedOn.empty() || TypedOn.back().Strip != Entry.Strip)
				{
					TypedOn.push_back({static_cast<int32>(Typed.size()), 0, Entry.Strip});
				}
				++TypedOn.back().Count;
				Typed.push_back({Glyph, X, Left});
				X = static_cast<int16>(X + GlyphWidth);
				Entry.WidthUnits = X;
				Position += bTwoBytes ? 2 : 1;
				--Budget;
				continue;
			}
			const uint8 Operand = Peek(2);
			switch (Peek(1))
			{
			case 0x00:
				WaitTimer = Operand;
				Position += 3;
				return;
			case 0x01:
				if (Operand == 0)
				{
					BaseSpeed = SavedSpeed;
					Speed = SavedSpeed;
					SavedSpeed = 0;
				}
				else
				{
					Budget += Operand;
					SavedSpeed = BaseSpeed;
					BaseSpeed = Operand;
					Speed = Operand;
				}
				Position += 3;
				--Budget;
				continue;
			case 0x02:
				Position += 3;
				bClosing = true;
				// The timer runs down twice a tick: two for each tick the game's own typing would still be at.
				OwedWait = PaceStretch < PaceTicks.size() ? static_cast<int16>(2 * std::max(PaceTicks[PaceStretch] - StretchTicks, 0)) : int16{0};
				WaitTimer = static_cast<int16>(Operand + OwedWait);
				return;
			case 0x03:
			{
				const uint8 Index = Peek(3);
				Position += 3;
				PushString(TableString(Operand, Index));
				continue;
			}
			case 0x04:
			{
				// The selector reads as a signed half: only high bytes 0-4 name a table.
				const int32 Selected = static_cast<int16>(Selector);
				const int32 Kind = Selected & 0xFF00;
				const int32 Index = Selected & 0xFF;
				Position += 1;
				int32 Table = -1;
				switch (Kind)
				{
				case 0x000: Table = ItemTable; break;
				case 0x100: Table = WeaponTable; break;
				case 0x200: Table = ArmorTable; break;
				case 0x300: Table = GearWeaponTable; break;
				case 0x400: Table = GearPartTable; break;
				default: break;
				}
				--Budget;
				if (Table >= 0)
				{
					PushString(TableString(Table, Index));
				}
				continue;
			}
			case 0x05:
			{
				Position += 2;
				int32 Name = Operand;
				std::span<const uint8> Nested;
				if (Name >= 0x80 && Context.State != nullptr)
				{
					Name = Context.State->PartyName(Name);
				}
				if (Name == EmptyPartySlot && Operand >= 0x80)
				{
					Nested = TableString(PartyNamesTable, 0);
				}
				else if (Context.State != nullptr)
				{
					Nested = Context.State->Name(Name);
				}
				PushString(Nested);
				continue;
			}
			case 0x06:
			case 0x07:
			case 0x08:
			{
				const int32 Table = Peek(1) == 0x06 ? WeaponTable : Peek(1) == 0x07 ? PlaceholderTable24 : PlaceholderTable25;
				Position += 2;
				PushString(TableString(Table, Operand));
				continue;
			}
			case 0x09:
			case 0x0A:
			case 0x0C:
			{
				const uint8 Kind = Peek(1);
				Position += 2;
				FormatNumber(Context, Numbers[Operand & 3], Kind != 0x09, Kind == 0x0C);
				PushString(Context.NumberString);
				continue;
			}
			case 0x0B:
				// Only two bytes are skipped, so the operand is read again as the next code.
				Mark = Operand;
				Position += 2;
				--Budget;
				continue;
			case 0x0D:
				Position += 3;
				bClosing = true;
				Flags |= FlagAutoAdvance;
				WaitTimer = Operand;
				return;
			case 0x0E:
				SavedSpeed = BaseSpeed;
				BaseSpeed = 1;
				Speed = 1;
				DelayReload = Operand;
				DelayCounter = Operand;
				bAuthoredPace = true;
				Position += 3;
				return;
			case 0x0F:
				Position += 2;
				PushString(TableString(ButtonNamesTable, Context.ButtonMap[Operand & 7]));
				continue;
			default:
				// An unknown extended code never moves the text on: the step spends its budget on it.
				--Budget;
				continue;
			}
		}
	}
}
