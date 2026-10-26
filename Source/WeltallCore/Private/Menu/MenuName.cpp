#include "Menu/MenuName.h"

#include "Game/GameState.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 CharacterCount = 16;
		constexpr uint16 CharacterMask = 0x7FF;
		/** L1, R1, Start and Select held together, and nothing else: the console starts again. */
		constexpr uint16 SoftResetButtons = 0x090C;
		constexpr int32 ClosingFrames = 2;
		constexpr int16 TextClutX = 0;
		constexpr int16 TextClutY = 0x1D1;
		constexpr int16 WhiteClutY = 0x1C0;
		constexpr uint16 White = 0x7FFF;
		/** People's names (numbers below 11) are 9 glyphs long, the gears' 10. */
		constexpr uint8 FirstGearName = 11;
		constexpr int32 PeopleLength = 9;
		constexpr int32 GearLength = 10;
		/** Crescens' name shows the grown Emeralda once she has joined. */
		constexpr uint8 CrescensName = 20;
		constexpr uint8 GrownEmeralda = 10;
		constexpr uint16 GrownEmeraldaJoined = 0x400;
		constexpr std::size_t NameSlotsAt = 0x20;
		constexpr std::size_t NameBytes = 20;
		/** A name is copied into its slots up to this many bytes. */
		constexpr std::size_t CopiedBytes = 18;
		/** The byte a confirmed name is trimmed of at its end: a blank in the Japanese font, "s" in the American. */
		constexpr uint8 TrimmedByte = 0x4F;
		/** The American font's blank, which the fix trims instead. */
		constexpr uint8 AmericanBlank = 0x10;
		constexpr int32 GridWindow = 2;
		constexpr int32 PromptWindow = 3;
		constexpr int32 PiecesEntry = 4;
		/** The layer has gone this far back when the screen closes. */
		constexpr int32 ClosedDepth = 0x600;
		/** The cursor starts on "End". */
		constexpr int32 EndColumn = 0x16;
		constexpr int32 EndRow = 4;
		constexpr int32 LastRow = 4;
		/** The grid's blocks: six columns of nine rows of six bytes, the rows' last byte a spacer. */
		constexpr int32 RowBytes = 6;
		constexpr int32 BlockColumns = 6;
		constexpr int32 BlockBytes = 48;

		namespace Glyph
		{
			/** Not typed yet (no text), deleted, the name's end. */
			constexpr uint16 Untyped = 0x1C;
			constexpr uint16 Deleted = 0x0F;
			constexpr uint16 End = 0x1F;
		}

		namespace GridCell
		{
			constexpr uint8 Blank = 0x0F;
			constexpr uint8 Wall = 0xCF;
			constexpr uint8 End = 0x1F;
			constexpr uint8 RowEnd = 0xFF;
			/** A blank typed: a space, which the cell holds from then on. */
			constexpr uint8 Space = 0xC3;
		}

		namespace Effect
		{
			constexpr uint8 Cursor = 1;
			constexpr uint8 Typed = 2;
			constexpr uint8 Cancel = 3;
			constexpr uint8 Buzzer = 4;
		}

		/** Enter Name's input: the directions repeat with their sound, Cancel sounds; the shoulder buttons are new presses. */
		constexpr std::array<FMenuInputTest, 13> NameInput = {{
			{MenuButton::Right, true, EMenuCode::Right, Effect::Cursor},
			{MenuButton::Down, true, EMenuCode::Down, Effect::Cursor},
			{MenuButton::Left, true, EMenuCode::Left, Effect::Cursor},
			{MenuButton::Up, true, EMenuCode::Up, Effect::Cursor},
			{MenuButton::Circle, false, EMenuCode::Confirm, 0},
			{MenuButton::Cross, false, EMenuCode::Cancel, Effect::Cancel},
			{MenuButton::Square, false, EMenuCode::Square, 0},
			{MenuButton::Triangle, false, EMenuCode::Triangle, 0},
			{MenuButton::L1, false, EMenuCode::L1, 0},
			{MenuButton::R1, false, EMenuCode::R1, 0},
			{MenuButton::Start, false, EMenuCode::Start, 0},
			{MenuButton::Select, false, EMenuCode::Select, 0},
			{MenuButton::L2, false, EMenuCode::None, 0},
		}};

		bool IsBlank(uint8 Cell)
		{
			return Cell == GridCell::Blank || Cell == GridCell::Wall;
		}
	}

	FMenuName::FMenuName(const FMenuServices& InServices, const FMenuRequest& InRequest) : FMenuModule(InServices, InRequest)
	{
	}

	TMenuTask<> FMenuName::Run()
	{
		Init();
		Buffer = 0;
		Upload({{TextClutX, TextClutY, 32, 1}, {Resources.TextClut().begin(), Resources.TextClut().end()}});
		RenderLabels(Labels, Resources.Name().Labels, 0);
		FVramImage WhiteClut{{0, WhiteClutY, 16, 1}, std::vector<uint16>(16, 0)};
		WhiteClut.Units[1] = White;
		Upload(WhiteClut);
		InitBackdrop();
		bDrawScene = true;
		bSoundOn = true;
		co_await EnterName();
		co_await Shutdown();
	}

	void FMenuName::Init()
	{
		const FGameState& Game = *Services.Game;
		const uint16 Allowed = static_cast<uint16>(Game.JoinedCharacters() & Game.MenuCharacters() & CharacterMask);
		const std::array<uint8, 3> InParty = Game.Party();
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const uint8 Character = InParty[static_cast<std::size_t>(Slot)];
			const bool bKept = Character != FGameState::EmptySlot && Character < CharacterCount && (Allowed >> Character & 1) != 0;
			Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)] = bKept ? Character : FGameState::EmptySlot;
		}
		for (const FVramImage& Image : Resources.ArchiveUploads())
		{
			Upload(Image);
		}
		// The name's owner shows in the first slot's portrait: a person's own, a gear's pilot.
		const uint8 Number = Request.Param;
		const std::span<const uint8> Entries = Resources.NameEntry();
		uint8& Owner = Flags[MenuFlag::Party];
		if (Number < FirstGearName)
		{
			MaxLength = PeopleLength;
			Owner = Number;
		}
		else
		{
			MaxLength = GearLength;
			const bool bGrown = Number == CrescensName && (Game.JoinedCharacters() & GrownEmeraldaJoined) != 0;
			const std::size_t Pilot = static_cast<std::size_t>(Number - FirstGearName);
			Owner = bGrown ? GrownEmeralda : (Pilot < Entries.size() ? Entries[Pilot] : FGameState::EmptySlot);
		}
		for (std::size_t Slot = 0; Slot < NameSlots.size(); ++Slot)
		{
			const std::size_t At = NameSlotsAt + static_cast<std::size_t>(Number) * 3 + Slot;
			NameSlots[Slot] = At < Entries.size() ? Entries[At] : 0;
		}
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			if (Member(Slot) != FGameState::EmptySlot)
			{
				UploadPortrait(Slot, Member(Slot));
			}
		}
		Grid = Resources.Name().Grid;
	}

	void FMenuName::DrawFrame()
	{
		ReadInput(NameInput);
		if (PadHeld == SoftResetButtons)
		{
			bSoftReset = true;
			return;
		}
		Buffer ^= 1;
		UpdateTransition();
		LayerGte = MenuProjection(Layer);
		if (bDrawScene)
		{
			GrowWindows();
			if (bPointerShown && !GridPointer.empty())
			{
				Table.Add(PiecesEntry, GridPointer.front());
			}
			AddLabels();
			AddNameScreen();
			AddWindows();
		}
		AddBackdrop();
		Drawn.Primitives.clear();
		Table.TakeInto(Drawn.Primitives);
		Drawn.Layer = Layer;
		++Drawn.Number;
	}

	uint8& FMenuName::Cell(int32 Row, int32 Column)
	{
		const int32 At = Row * RowBytes + Column / BlockColumns * BlockBytes + Column;
		if (At < 0 || At >= static_cast<int32>(Grid.size()))
		{
			OutsideCell = 0;
			return OutsideCell;
		}
		return Grid[static_cast<std::size_t>(At)];
	}

	void FMenuName::TypeText()
	{
		int32 Count = 0;
		while (Count < static_cast<int32>(Glyphs.size()) && Glyphs[static_cast<std::size_t>(Count)] != Glyph::End)
		{
			++Count;
		}
		GlyphText(std::span<const uint16>(Glyphs.data(), static_cast<std::size_t>(Count)), GlyphScratch);
		Text.fill(0);
		std::copy_n(GlyphScratch.begin(), std::min(GlyphScratch.size(), Text.size()), Text.begin());
	}

	TMenuTask<> FMenuName::EnterName()
	{
		const bool bFrench = Resources.IsFrench();
		const std::span<const uint8> Old = Services.Game->Block().subspan(static_cast<std::size_t>(Request.Param) * NameBytes, NameBytes);
		Text.fill(0);
		if (bFrench)
		{
			// The French translation's program edits the old name: its glyphs are the old name's, each the glyph map's first
			// of its code (a byte a glyph), with no end.
			std::copy_n(Old.begin(), Glyphs.size(), Text.begin());
			for (std::size_t At = 0; At < Glyphs.size(); ++At)
			{
				Glyphs[At] = GlyphOfCode(Text[At]);
			}
		}
		else
		{
			Glyphs.fill(Glyph::Untyped);
			Glyphs[PeopleLength] = Glyph::End;
			std::copy(Old.begin(), Old.end(), Text.begin());
		}
		co_await BuildScreen();
		RenderName();
		OpenLayer();
		while (LayerState != EMenuLayer::Still)
		{
			co_await Frame();
		}
		OpenWindow(GridWindow, 0x38, 0x26, 0xD0, 0x60, true, false, PiecesEntry, false);
		while (!Growths[GridWindow].bGrown)
		{
			co_await Frame();
		}
		bControlsShown = true;
		Flags[MenuFlag::HelpLabels] = 1;
		Flags[MenuFlag::HelpLabels + 3] = 1;
		int32 Column = EndColumn;
		int32 Row = EndRow;
		int32 DrawnColumn = 0xFF;
		int32 DrawnRow = 0xFF;
		bool bRenders = false;
		bool bGoesOn = true;
		while (bGoesOn)
		{
			if (Column != DrawnColumn || Row != DrawnRow)
			{
				PlaceCursor(Column, Row);
				DrawnColumn = Column;
				DrawnRow = Row;
			}
			if (bRenders)
			{
				RenderName();
				bRenders = false;
			}
			co_await Frame();
			int32 Moved = Column;
			switch (Code)
			{
			case EMenuCode::Right:
				// Past a blank once; off the row's end to its start.
				Moved = Column + 1;
				if (Cell(Row, Moved) == GridCell::RowEnd)
				{
					Moved = 0;
				}
				else if (IsBlank(Cell(Row, Moved)))
				{
					Moved = Column + 2;
				}
				break;
			case EMenuCode::Down:
				Row = Row + 1 > LastRow ? 0 : Row + 1;
				if (Cell(Row, Column) == GridCell::Wall)
				{
					Row = 0;
				}
				else if (Cell(Row, Column) == GridCell::Blank)
				{
					++Row;
				}
				break;
			case EMenuCode::Left:
				Moved = Column - 1;
				if (Moved < 0)
				{
					Moved = EndColumn;
				}
				else if (IsBlank(Cell(Row, Moved)))
				{
					Moved = Column - 2;
				}
				break;
			case EMenuCode::Up:
				Row = Row - 1 < 0 ? LastRow : Row - 1;
				if (IsBlank(Cell(Row, Column)))
				{
					--Row;
				}
				break;
			case EMenuCode::Confirm:
			{
				uint8& Typed = Cell(Row, Column);
				if (Typed == GridCell::End)
				{
					bGoesOn = Glyphs[0] == Glyph::Deleted;
				}
				// The French translation's inserts at the cursor while the name's tenth byte is free.
				else if (bFrench ? Text[Glyphs.size() - 1] == 0 : Position < MaxLength)
				{
					if (IsBlank(Typed))
					{
						Typed = GridCell::Space;
					}
					if (bFrench && Position < static_cast<int32>(Glyphs.size()))
					{
						std::copy_backward(Glyphs.begin() + Position, Glyphs.end() - 1, Glyphs.end());
					}
					Glyphs[static_cast<std::size_t>(Position)] = Typed;
					TypeText();
					bRenders = true;
					++Position;
					PlaySound(Effect::Typed);
				}
				else
				{
					PlaySound(Effect::Buzzer);
				}
				break;
			}
			case EMenuCode::Cancel:
				if (bFrench)
				{
					// The French translation's deletes the glyph after the cursor, the rest moving up.
					if (Position < static_cast<int32>(Glyphs.size()))
					{
						std::copy(Glyphs.begin() + Position + 1, Glyphs.end(), Glyphs.begin() + Position);
						Glyphs.back() = Glyph::Deleted;
					}
				}
				else
				{
					// Back one, and the glyph there deleted (the first one too when already there).
					if (Position > 0)
					{
						--Position;
					}
					Glyphs[static_cast<std::size_t>(Position)] = Glyph::Deleted;
				}
				bRenders = true;
				TypeText();
				break;
			case EMenuCode::R1:
				// The French translation's moves on only over a typed byte.
				Position = std::min(Position + (bFrench && Text[static_cast<std::size_t>(Position)] == 0 ? 0 : 1), MaxLength);
				break;
			case EMenuCode::L1:
				Position = std::max(Position - 1, 0);
				break;
			case EMenuCode::Start:
				// A name whose first glyph was deleted is not taken.
				bGoesOn = Glyphs[0] == Glyph::Deleted;
				break;
			default:
				break;
			}
			Column = Moved;
		}
		WriteNames();
		Flags[MenuFlag::HelpLabels] = 0;
		Flags[MenuFlag::HelpLabels + 3] = 0;
		bPointerShown = false;
		co_await Frame();
		bControlsShown = false;
		CloseWindow(GridWindow);
		CloseLayer();
		while (Layer.Translation[2] < ClosedDepth)
		{
			co_await Frame();
		}
		bScreenShown = false;
		co_await HidePrompt();
		CloseWindow(PromptWindow);
	}

	uint16 FMenuName::GlyphOfCode(uint8 Byte) const
	{
		for (int32 Index = 0; Services.Text.Tables != nullptr; ++Index)
		{
			const std::span<const uint8> Entry = Services.Text.Tables->GlyphMapEntry(Index);
			if (Entry.size() != 2)
			{
				break;
			}
			if (Entry[1] == Byte)
			{
				return static_cast<uint16>(Index);
			}
		}
		return Glyph::Untyped;
	}

	void FMenuName::WriteNames()
	{
		// The same name into each slot: copied up to its end, then cut after its last byte that is not a blank.
		const std::span<uint8> Block = Services.Game->Block();
		const uint8 Trimmed = Services.Options.bNamesKeepTrailingS ? AmericanBlank : TrimmedByte;
		std::size_t Kept = 0;
		for (const uint8 Slot : NameSlots)
		{
			const std::span<uint8> Name = Block.subspan(static_cast<std::size_t>(Slot) * NameBytes, NameBytes);
			std::fill(Name.begin(), Name.end(), uint8{0});
			std::size_t Copied = 0;
			while (Copied < CopiedBytes && Text[Copied] != 0)
			{
				Name[Copied] = Text[Copied];
				if (Text[Copied] != Trimmed)
				{
					Kept = Copied;
				}
				++Copied;
			}
			std::fill(Name.begin() + static_cast<std::ptrdiff_t>(Kept) + 1, Name.end(), uint8{0});
		}
	}

	TMenuTask<> FMenuName::Shutdown()
	{
		for (int32 Closing = 0; Closing < ClosingFrames; ++Closing)
		{
			co_await Frame();
		}
		bDrawScene = false;
		co_await Frame();
		do
		{
			co_await Frame();
		} while (Buffer != 0);
	}
}
