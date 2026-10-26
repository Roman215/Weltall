#include "Menu/MenuMesWindow.h"

#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** A line's sprite: 13 rows of its strip, one under the other every 14 pixels. */
		constexpr int16 TextRows = 13;
		constexpr int16 LineSpacing = 14;
		/** A page of the image: 64 units, 256 pixels. */
		constexpr int32 PageUnits = 0x40;
		constexpr int32 PagePixels = PageUnits * 4;
		/** The image's rows are this many units wider than the window. */
		constexpr int32 ImageMargin = 3;
		/** The box reaches 7 pixels left of the text and 5 above it, 13 wider and 10 taller than the lines. */
		constexpr int16 BoxLeft = 7;
		constexpr int16 BoxTop = 5;
		constexpr int16 BoxExtraWidth = 13;
		constexpr int16 BoxExtraHeight = 10;
		/** A rectangle of one colour, half transparent. */
		constexpr uint8 HalfTile = 0x62;

		FMenuPrimitive PageMode(const FMesWindowPlace& Place, int32 Page)
		{
			FMenuPrimitive Mode;
			Mode.Code = MenuGpu::DrawModeCommand;
			Mode.Words = {MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, Place.VramX + Page * PageUnits, Place.VramY)), 0};
			return Mode;
		}
	}

	void AppendMesWindowPrimitives(const FMesWindow& Window, const FMesWindowPlace& Place, std::vector<FMenuPrimitive>& Out,
		const FMesWindowButtons& Buttons)
	{
		if (!Window.Drew())
		{
			return;
		}
		Out.push_back(PageMode(Place, 0));
		if (Window.HasBox())
		{
			FMenuPrimitive& Box = Out.emplace_back();
			Box.Code = HalfTile;
			Box.Corners[0].X = static_cast<int16>(Place.ScreenX - BoxLeft);
			Box.Corners[0].Y = static_cast<int16>(Place.ScreenY - BoxTop);
			Box.Corners[0].Color = {0, 0, 0};
			Box.Width = static_cast<int16>(Window.WidthUnits() * 4 + BoxExtraWidth);
			Box.Height = static_cast<int16>(Window.DrawnLines() * LineSpacing + BoxExtraHeight);
		}
		const std::array<uint8, 3>& Colour = Window.HighlightColor();
		const int32 Unit = Buttons.Font != nullptr ? Buttons.Font->UnitPixels() : 4;
		const bool bButtons = Buttons.Finder != nullptr && Buttons.Cells != nullptr;
		const std::size_t FirstCell = bButtons ? Buttons.Cells->size() : 0;
		// Each page's rows, linked top down, so drawn from the bottom up.
		const auto Rows = [&](int32 Page) {
			for (int32 Row = Window.DrawnLines() - 1; Row >= 0; --Row)
			{
				const FMesWindow::FLine& Line = Window.Row(Row);
				const int32 Pixels = Line.WidthUnits * Unit - Page * PagePixels;
				if (Pixels <= 0)
				{
					continue;
				}
				FMenuPrimitive& Sprite = Out.emplace_back();
				Sprite.Code = MenuGpu::Sprite | (Row == Window.HighlightedRow() ? 0 : MenuGpu::RawTexture);
				Sprite.Corners[0].X = static_cast<int16>(Place.ScreenX + Page * PagePixels);
				Sprite.Corners[0].Y = static_cast<int16>(Place.ScreenY + LineSpacing * Row);
				Sprite.Corners[0].U = static_cast<uint8>((Place.VramX & 0x3F) * 4);
				Sprite.Corners[0].V = static_cast<uint8>(Place.VramY + Line.Strip / 2 * TextRows);
				Sprite.Corners[0].Color = {Colour[0], Colour[1], Colour[2]};
				Sprite.Clut = (Line.Strip & 1) != 0 ? Place.OddClut : Place.EvenClut;
				Sprite.Width = static_cast<int16>(std::min(Pixels, PagePixels));
				Sprite.Height = TextRows;
				if (!bButtons)
				{
					continue;
				}
				const uint32 Index = static_cast<uint32>(Out.size() - 1);
				// A row's second page moves with what follows a button of its first.
				for (std::size_t Cell = FirstCell; Page == 1 && Cell < Buttons.Cells->size(); ++Cell)
				{
					FTextButtonCell& Before = (*Buttons.Cells)[Cell];
					if (Out[Before.Primitive].Corners[0].Y == Sprite.Corners[0].Y)
					{
						Before.LineRest = Index + 1;
					}
				}
				Buttons.Finder->Add(Window, Row, Page * PagePixels, Sprite.Width, Buttons.Font, Index, *Buttons.Cells);
			}
		};
		Rows(0);
		Out.push_back(PageMode(Place, 1));
		Rows(1);
	}

	void FMesWindowImage::Type(const FMesFont* Font, const FMesWindow& Window, const FMesWindowPlace& Place, std::vector<FVramImage>& Uploads)
	{
		if (!Window.Stepped())
		{
			return;
		}
		// Only the line's own width of its own plane is drawn; the rest of the strip is what earlier lines left.
		const int32 Pitch = Window.WidthUnits() + ImageMargin;
		Units.resize(static_cast<std::size_t>(Pitch) * TextRows);
		DrawMesTypedLines(Font, Window, Units, Pitch, [this, &Place, &Uploads, Pitch](int32 Strip)
		{
			Uploads.push_back(FVramImage{{Place.VramX, static_cast<int16>(Place.VramY + Strip / 2 * TextRows), static_cast<int16>(Pitch), TextRows}, Units});
		});
	}
}
