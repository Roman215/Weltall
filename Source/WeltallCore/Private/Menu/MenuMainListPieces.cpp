#include "Menu/MenuMain.h"

namespace Weltall
{
	namespace
	{
		/** The list cursor: an arrow of five cels (frames from 15Bh), each shown six frames, the last first. */
		constexpr int32 FirstCursorFrame = 0x15B;
		constexpr int32 LastCel = 4;
		constexpr uint8 FramesPerCel = 5;
		constexpr int32 KnobFrame = 0x107;
		constexpr int32 KnobWidth = 8;
		constexpr int32 FullScale = 0x1000;
		constexpr int32 PiecesEntry = 4;
	}

	void FMenuMain::AllocCursor(int32 Which)
	{
		FMenuLayerSprite& Cursor = Cursors[static_cast<std::size_t>(Which)];
		Cursor = {};
		Cursor.Cel = LastCel;
		Cursor.Frames = 0;
	}

	void FMenuMain::AdvanceCursor(int32 Which)
	{
		FMenuLayerSprite& Cursor = Cursors[static_cast<std::size_t>(Which)];
		if (++Cursor.Frames > FramesPerCel)
		{
			if (--Cursor.Cel < 0)
			{
				Cursor.Cel = LastCel;
			}
			Cursor.Frames = 0;
		}
	}

	void FMenuMain::DrawCursor(int32 Row, int32 Top, int32 Kind, int32 Which)
	{
		AdvanceCursor(Which);
		// Where the arrow points, by the list: the items' two columns (a page's or all of them), the
		// abilities' two columns, the Equip screen's rows.
		// The French translation's program raises it 2 pixels on the item lists and 1 on the others.
		const bool bFrench = Resources.IsFrench();
		const int32 ItemsTop = bFrench ? 0x0F : 0x11;
		const int32 OthersTop = bFrench ? 0x13 : 0x14;
		bool bShown = true;
		int32 X = 0;
		int32 Y = 0;
		switch (Kind)
		{
		case 0:
			X = Row % 2 * 0x88 + 0x1C;
			Y = Row / 2 * 0x10 + ItemsTop;
			break;
		case 1:
			if (Row < Top * 2 || Top * 2 + 0x10 <= Row)
			{
				bShown = false;
			}
			else
			{
				X = Row % 2 * 0x88 + 0x18;
				Y = (Row - Top * 2) / 2 * 0x10 + ItemsTop;
			}
			break;
		case 2:
			X = Row % 2 * 0x88 + 0x18;
			Y = Row / 2 * 0x10 + OthersTop;
			break;
		default:
			X = 0xA0;
			Y = Row * 0x0D + OthersTop;
			break;
		}
		if (!bShown)
		{
			Flags[static_cast<std::size_t>(MenuFlag::Cursors + Which)] = 0;
			return;
		}
		PlaceCursor(X, Y, Which);
	}

	void FMenuMain::DrawCursorAt(int32 X, int32 Y, int32 Which)
	{
		AdvanceCursor(Which);
		PlaceCursor(X, Y, Which);
	}

	void FMenuMain::PlaceCursor(int32 X, int32 Y, int32 Which)
	{
		FMenuLayerSprite& Cursor = Cursors[static_cast<std::size_t>(Which)];
		Scratch.clear();
		Resources.Sheet().Draw(FirstCursorFrame + Cursor.Cel, 0, 0, FullScale, Scratch);
		if (!Scratch.empty())
		{
			Cursor.Quad = Scratch.front();
			const std::array<FMenuCorner, 4>& At = Cursor.Quad.Corners;
			SetLayerQuad(Cursor.Quad.Points, At[0].X + X, At[0].Y + Y, static_cast<uint16>(At[1].X - At[0].X), static_cast<uint16>(At[3].Y - At[0].Y));
		}
		Flags[static_cast<std::size_t>(MenuFlag::Cursors + Which)] = 1;
	}

	void FMenuMain::DrawScrollKnob(int32 X, int32 Y, int32 Height)
	{
		if (Flags[MenuFlag::ScrollKnob] == 0)
		{
			ScrollKnob = {};
		}
		Scratch.clear();
		Resources.Sheet().Draw(KnobFrame, X, Y, FullScale, Scratch);
		if (!Scratch.empty())
		{
			ScrollKnob.Quad = Scratch.front();
		}
		SetLayerQuad(ScrollKnob.Quad.Points, X, Y, KnobWidth, Height);
		Flags[MenuFlag::ScrollKnob] = 1;
	}

	void FMenuMain::AddCursors()
	{
		for (int32 Which = 0; Which < static_cast<int32>(Cursors.size()); ++Which)
		{
			if (Flags[static_cast<std::size_t>(MenuFlag::Cursors + Which)] != 0)
			{
				AddProjected(Cursors[static_cast<std::size_t>(Which)].Quad, true, PiecesEntry);
			}
		}
	}

	void FMenuMain::AddScrollKnob()
	{
		if (Flags[MenuFlag::ScrollKnob] != 0)
		{
			AddProjected(ScrollKnob.Quad, true, PiecesEntry);
		}
	}
}
