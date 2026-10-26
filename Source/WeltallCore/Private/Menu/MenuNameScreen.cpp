#include "Menu/MenuName.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 FullScale = 0x1000;
		constexpr int32 HalfScale = 0x800;
		constexpr int32 PiecesEntry = 4;
		constexpr int32 PromptWindow = 3;
		constexpr uint8 Unshaded = 0x80;
		constexpr int32 RowHeight = 13;
		constexpr int32 TextClutY = 0x1D1;
		/** The prompt's three lines, on the layer from (72, 160). */
		constexpr uint8 PromptText = 0x1D;
		constexpr int32 PromptX = 0x48;
		constexpr int32 PromptY = 0xA0;
		/** The owner's portrait, and the name typed so far from its strip at (384, 234). */
		constexpr int32 PortraitFrame = 0x14B;
		constexpr int32 PortraitX = 0x18;
		constexpr int32 PortraitY = 0x9E;
		constexpr int16 NameStripX = 0x180;
		constexpr int16 NameStripY = 0xEA;
		constexpr int16 NameStripWidth = 0x28;
		constexpr int32 NameX = 0x50;
		constexpr int32 NameY = 0xB6;
		constexpr int32 GlyphWidth = 8;
		/** The buttons' pictures, their labels and "End". */
		constexpr int32 AcceptFrame = 0xF9;
		constexpr int32 BackFrame = 0xFC;
		constexpr int32 EndFrame = 0xF0;
		/** The grid's strips: 36 labels of five glyphs, 28 units wide, two to a row of strips from (384, 0). */
		constexpr int32 StripCount = 36;
		constexpr int32 StripGlyphs = 5;
		constexpr int32 GridRowBytes = 6;
		constexpr int32 StripRows = 9;
		constexpr int16 StripX = 0x180;
		constexpr int16 StripColumn = 0x20;
		constexpr int16 StripWidth = 0x1C;
		constexpr int32 StripTexels = 0x3C;
		constexpr int32 BlockX = 0x30;
		constexpr int32 GridX = 0x44;
		constexpr int32 GridY = 0x2E;
		constexpr int32 CellHeight = 0x10;
		constexpr int32 NamePageX = 0x180;
		/** The cursor's pointer above its cell, the box around it, the underline under the insertion place. */
		constexpr int32 PointerFrame = 0x108;
		constexpr int32 PointerX = 0x48;
		constexpr int32 PointerY = 0x36;
		constexpr int32 BoxLeft = 0x44;
		constexpr int32 BoxRight = 0x4C;
		constexpr int32 BoxTopY = 0x2E;
		constexpr int32 BoxBottomY = 0x3A;
		constexpr int16 UnderlineY = 0xC6;
		/** The French translation's: a bar at the insertion place, from the name's line's top, a pixel left of its next glyph. */
		constexpr int16 BarTop = 0xB6;
		constexpr int16 BarBottom = 0xC1;
		constexpr FDrawColor BoxGreen{0, 0xFF, 0};
		constexpr FDrawColor UnderlineGreen{0, 0x80, 0};
		/** The underline blinks: shown 30 of every 61 frames. */
		constexpr uint8 BlinkCycle = 0x3C;
		constexpr uint8 BlinkShown = 0x1E;

		void SetCorners(FMenuPrimitive& Quad, int32 Left, int32 Top, int32 Width, int32 Height)
		{
			Quad.Corners[0].X = static_cast<int16>(Left);
			Quad.Corners[0].Y = static_cast<int16>(Top);
			Quad.Corners[1].X = static_cast<int16>(Left + Width);
			Quad.Corners[1].Y = static_cast<int16>(Top);
			Quad.Corners[2].X = static_cast<int16>(Left);
			Quad.Corners[2].Y = static_cast<int16>(Top + Height);
			Quad.Corners[3].X = static_cast<int16>(Left + Width);
			Quad.Corners[3].Y = static_cast<int16>(Top + Height);
		}

		/** An unshaded strip of the page at x 384, its texels drawn as they are. */
		void MakeStrip(FMenuPrimitive& Quad)
		{
			Quad.Code = MenuGpu::TexturedQuad | MenuGpu::RawTexture;
			Quad.Corners[0].Color = {Unshaded, Unshaded, Unshaded};
			Quad.Clut = MenuGpu::Clut(0, TextClutY);
			Quad.Page = MenuGpu::TexturePage(0, 0, NamePageX, 0);
		}

		void MakeLine(FMenuPrimitive& Line, uint8 Code, uint8 Points, FDrawColor Color)
		{
			Line.Code = Code;
			Line.LinePoints = Points;
			Line.Corners[0].Color = Color;
		}
	}

	TMenuTask<> FMenuName::BuildScreen()
	{
		OpenWindow(PromptWindow, 0x10, 0x9A, 0xC0, 0x3C, true, true, PiecesEntry, false);
		co_await ShowPrompt(PromptText);
		const FMenuSpriteSheet& Sheet = Resources.Sheet();
		Scratch.clear();
		Sheet.Draw(PortraitFrame, 0, 0, FullScale, Scratch);
		if (!Scratch.empty())
		{
			Portrait = Scratch.front();
			const std::array<FMenuCorner, 4>& At = Portrait.Corners;
			SetLayerQuad(Portrait.Points, At[0].X + PortraitX, At[0].Y + PortraitY, At[1].X - At[0].X, At[2].Y - At[0].Y);
		}
		AcceptIcon.clear();
		Sheet.Draw(AcceptFrame, 0xE8, 0xB6, FullScale, AcceptIcon);
		BackIcon.clear();
		Sheet.Draw(BackFrame, 0xE0, 0xC6, FullScale, BackIcon);
		EndMark.clear();
		Sheet.Draw(EndFrame, 0xF4, 0x6E, FullScale, EndMark);
		// The buttons' labels beside their pictures: "Accept" placed in the copy this buffer draws, "Cancel" made drawn from it.
		FMenuLabel& Accept = Labels[0];
		SetCorners(Accept.Buffer == Buffer ? Accept.Quad : Accept.Other, 0xF8, 0xB6, Accept.Width, RowHeight);
		FMenuLabel& Back = Labels[3];
		RecordLabel(Back);
		SetCorners(Back.Quad, 0xF0, 0xC6, Back.Width, RowHeight);
		BuildGrid();
		bPortraitShown = true;
		bScreenShown = true;
	}

	TMenuTask<> FMenuName::ShowPrompt(uint8 FirstText)
	{
		RenderPromptLines(Prompt, FirstText, PromptX, PromptY);
		bPromptShown = true;
		co_await Frame();
		co_await Frame();
	}

	TMenuTask<> FMenuName::HidePrompt()
	{
		bPromptShown = false;
		co_await Frame();
	}

	void FMenuName::BuildGrid()
	{
		for (int32 Strip = 0; Strip < StripCount; ++Strip)
		{
			std::array<uint16, StripGlyphs> Cells{};
			for (int32 Cell = 0; Cell < StripGlyphs; ++Cell)
			{
				Cells[static_cast<std::size_t>(Cell)] = Grid[static_cast<std::size_t>(Strip * GridRowBytes + Cell)];
			}
			GlyphText(Cells, GlyphScratch);
			std::fill(LabelImage.Units.begin(), LabelImage.Units.end(), uint16{0});
			// The French translation's program types the grid 8 pixels a letter, its cells' own width.
			Services.Text.bFixedAdvance = Resources.IsFrench();
			RenderString(LabelTyping, LabelImage, GlyphScratch, 0);
			Services.Text.bFixedAdvance = false;
			LabelImage.Rect = {static_cast<int16>(StripX + Strip % 2 * StripColumn), static_cast<int16>(Strip / 2 * RowHeight), StripWidth, RowHeight};
			Upload(LabelImage);
			FMenuPrimitive& Quad = Strips[static_cast<std::size_t>(Strip)];
			MakeStrip(Quad);
			SetTexelRect(Quad, Strip % 2 * 0x80, Strip / 2 * RowHeight, StripTexels, RowHeight);
			// A pixel further right in the French translation's program.
			SetCorners(Quad, Strip / StripRows * BlockX + GridX + (Resources.IsFrench() ? 1 : 0), Strip % StripRows * CellHeight + GridY, StripTexels, RowHeight);
		}
		MakeStrip(NameQuad);
		SetTexelRect(NameQuad, 0, NameStripY, MaxLength * GlyphWidth, RowHeight);
		SetLayerQuad(NameQuad.Points, NameX, NameY, MaxLength * GlyphWidth, RowHeight);
		MakeLine(BoxTop, MenuGpu::FlatPolyline, 3, BoxGreen);
		MakeLine(BoxBottom, MenuGpu::FlatPolyline, 3, BoxGreen);
		MakeLine(Underline, MenuGpu::FlatLine, 2, UnderlineGreen);
	}

	void FMenuName::RenderName()
	{
		std::fill(NameImage.Units.begin(), NameImage.Units.end(), uint16{0});
		RenderString(NameTyping, NameImage, Text, 0);
		NameImage.Rect = {NameStripX, NameStripY, NameStripWidth, RowHeight};
		Upload(NameImage);
	}

	void FMenuName::PlaceCursor(int32 Column, int32 Row)
	{
		GridPointer.clear();
		Resources.Sheet().Draw(PointerFrame, Column * 8 + PointerX, Row * CellHeight + PointerY, HalfScale, GridPointer);
		bPointerShown = true;
		const int16 Left = static_cast<int16>(Column * 8 + BoxLeft);
		const int16 Right = static_cast<int16>(Column * 8 + BoxRight);
		const int16 Top = static_cast<int16>(Row * CellHeight + BoxTopY);
		const int16 Bottom = static_cast<int16>(Row * CellHeight + BoxBottomY);
		BoxTop.Corners[0].X = Left;
		BoxTop.Corners[0].Y = Top;
		BoxTop.Corners[1].X = Right;
		BoxTop.Corners[1].Y = Top;
		BoxTop.Corners[2].X = Right;
		BoxTop.Corners[2].Y = Bottom;
		BoxBottom.Corners[0].X = Left;
		BoxBottom.Corners[0].Y = Top;
		BoxBottom.Corners[1].X = Left;
		BoxBottom.Corners[1].Y = Bottom;
		BoxBottom.Corners[2].X = Right;
		BoxBottom.Corners[2].Y = Bottom;
	}

	void FMenuName::AddLabels()
	{
		for (std::size_t Label = 0; Label < Labels.size(); ++Label)
		{
			if (Flags[static_cast<std::size_t>(MenuFlag::HelpLabels) + Label] != 0)
			{
				Table.Add(PiecesEntry, Labels[Label].Quad);
			}
		}
		if (bPromptShown)
		{
			for (FMenuLabel& Line : Prompt)
			{
				AddProjected(Line.Quad, true, PiecesEntry);
			}
		}
	}

	void FMenuName::AddNameScreen()
	{
		if (!bScreenShown)
		{
			return;
		}
		if (bPortraitShown)
		{
			AddProjected(Portrait, true, PiecesEntry);
			AddProjected(NameQuad, true, PiecesEntry);
		}
		if (!bControlsShown)
		{
			return;
		}
		for (const std::vector<FMenuPrimitive>* Icon : {&AcceptIcon, &BackIcon})
		{
			if (!Icon->empty())
			{
				Table.Add(PiecesEntry, Icon->front());
			}
		}
		AddScreen(EndMark);
		Table.Add(PiecesEntry, BoxTop);
		Table.Add(PiecesEntry, BoxBottom);
		Blink = Blink + 1 > BlinkCycle ? uint8{0} : static_cast<uint8>(Blink + 1);
		if (Blink < BlinkShown)
		{
			if (Resources.IsFrench())
			{
				// The name's bytes before the cursor, each its font's width (the program reads its table from code 10h).
				int32 Before = 0;
				for (int32 At = 0; At < Position; ++At)
				{
					Before += Services.Text.Font != nullptr ? Services.Text.Font->WidthUnits({0, Text[static_cast<std::size_t>(At)]}) : 0;
				}
				const int16 Left = static_cast<int16>(NameX - 1 + Before);
				Underline.Corners[0].X = Left;
				Underline.Corners[0].Y = BarTop;
				Underline.Corners[1].X = Left;
				Underline.Corners[1].Y = BarBottom;
			}
			else
			{
				const int16 Left = static_cast<int16>(Position * GlyphWidth + NameX);
				Underline.Corners[0].X = Left;
				Underline.Corners[0].Y = UnderlineY;
				Underline.Corners[1].X = static_cast<int16>(Left + GlyphWidth);
				Underline.Corners[1].Y = UnderlineY;
			}
			Table.Add(PiecesEntry, Underline);
		}
		for (const FMenuPrimitive& Strip : Strips)
		{
			Table.Add(PiecesEntry, Strip);
		}
	}
}
