#include "Battle/BattleContext.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleHudLists.h"
#include "Battle/Show/BattleShowHud.h"
#include "Formats/Tim.h"
#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	using namespace BattleHudGpu;
	using namespace BattleHudLists;

	namespace
	{
		/** The portrait's image in VRAM: its CLUT's row and its 64 x 64 pixels, an 8-bit texture. */
		constexpr int16 PortraitClutX = 0;
		constexpr int16 PortraitClutY = 0x1D0;
		constexpr int16 PortraitX = 0x3C0;
		constexpr int16 PortraitY = 0x100;
		constexpr int16 PortraitSize = 0x40;
		/** The portrait lies this far inside the window's corner. */
		constexpr int16 PortraitInset = 4;
		/** The kernel's window types the event's text into strips from here: two lines a strip of 13 rows, a plane each. */
		constexpr int16 TextVramX = 0x380;
		constexpr int16 TextVramY = 0x100;
		constexpr int16 LineSpacing = 14;
		/** A line past this many units is drawn in two sprites, the second from the next texture page. */
		constexpr int32 PageUnits = 0x40;
		constexpr int16 PagePixels = 0x100;
		/** The image is this many units wider than the window's text. */
		constexpr int16 ImageMargin = 3;
		/** The "more" arrow's five frames of the sheet, the list cursor's. */
		constexpr int32 ArrowFrames = 0xE0;

		FMenuPrimitive TextMode(int16 VramX)
		{
			FMenuPrimitive Mode;
			Mode.Code = MenuGpu::DrawModeCommand;
			Mode.Words = {MenuGpu::DrawMode(MenuGpu::TexturePage(0, 0, VramX, TextVramY)), 0};
			return Mode;
		}
	}

	void FBattleHud::LoadEventPortrait(const FTim& Portrait, bool bAtRight, int16 X, int16 Y, int16 Width)
	{
		if (Portrait.Clut)
		{
			FVramImage Clut = *Portrait.Clut;
			Clut.Rect.X = PortraitClutX;
			Clut.Rect.Y = PortraitClutY;
			Upload(Clut);
		}
		FVramImage Pixels = Portrait.Image;
		Pixels.Rect.X = PortraitX;
		Pixels.Rect.Y = PortraitY;
		Upload(Pixels);

		// Only this buffer's quad is placed, and it is the one drawn in either buffer until the next portrait.
		FMenuPrimitive& Quad = EventPortrait[Buffer];
		BattleHudGpu::Set(Quad, TexturedQuad);
		SetColor(Quad, 0, 0x80, 0x80, 0x80);
		SetSemiTrans(Quad, false);
		SetShadeTex(Quad, true);
		Quad.Clut = MenuGpu::Clut(PortraitClutX, PortraitClutY);
		Quad.Page = MenuGpu::TexturePage(1, 0, PortraitX, PortraitY);
		const int32 Top = Y + PortraitInset;
		const int32 Bottom = Top + PortraitSize;
		if (!bAtRight)
		{
			const int32 Left = X + PortraitInset;
			const int32 Right = Left + PortraitSize;
			SetCorners(Quad, Left, Top, Right, Top, Left, Bottom, Right, Bottom);
			SetTexels(Quad, 0, 0, PortraitSize, 0, 0, PortraitSize, PortraitSize, PortraitSize);
		}
		else
		{
			// Mirrored: its first texel column at the window's right edge, one column short of the image's width.
			const int32 Right = X + Width - PortraitInset;
			const int32 Left = Right - PortraitSize;
			SetCorners(Quad, Right, Top, Left, Top, Right, Bottom, Left, Bottom);
			SetTexels(Quad, 0, 0, PortraitSize - 1, 0, 0, PortraitSize, PortraitSize - 1, PortraitSize);
		}
		EventPortraitBuffer = Buffer;
	}

	void FBattleHud::BeginEventText()
	{
		const FBattleEventWork& Event = Battle->Event;
		if (!Event.Text)
		{
			FaultState = EBattleHudFault::EventTextNotMade;
			return;
		}
		EventTextX = static_cast<int16>(Event.TextX);
		EventTextY = static_cast<int16>(Event.TextY);
		const int16 Pitch = static_cast<int16>(Event.Text->WidthUnits() + ImageMargin);
		EventTextImage.Rect = {TextVramX, TextVramY, Pitch, TextRows};
		EventTextImage.Units.assign(static_cast<std::size_t>(Pitch) * TextRows, 0);
	}

	void FBattleHud::DrawEventArrow()
	{
		const FBattleEventWork& Event = Battle->Event;
		ListCursor.Count = DrawSheet(ArrowFrames + Event.ArrowFrame, ListCursor.Parts, 0, Event.ArrowX, Event.ArrowY, FullScale);
		FMenuPrimitive& First = ListCursor.Parts[0][Buffer];
		if (Event.Layout == EBattleMessageLayout::French)
		{
			// The French translation's: the cursor's picture laid on a quad of its own, 11 by 8 from its first corner, which
			// points it down, each corner reading a row higher (the second and fourth the first's and third's).
			std::array<FMenuCorner, 4>& Corners = First.Corners;
			const int16 X = Corners[0].X;
			const int16 Y = Corners[0].Y;
			const uint8 TopV = static_cast<uint8>(Corners[0].V - 1);
			const uint8 BottomV = static_cast<uint8>(Corners[2].V - 1);
			const std::array<std::array<int16, 2>, 4> Places{{{static_cast<int16>(X + 11), Y}, {static_cast<int16>(X + 11), static_cast<int16>(Y + 8)},
				{X, Y}, {X, static_cast<int16>(Y + 8)}}};
			for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
			{
				Corners[Corner].X = Places[Corner][0];
				Corners[Corner].Y = Places[Corner][1];
				Corners[Corner].V = Corner < 2 ? TopV : BottomV;
			}
		}
		else
		{
			// The cursor points right; its first part's second and third corners change places, which turns it to point down.
			std::swap(First.Corners[1].X, First.Corners[2].X);
			std::swap(First.Corners[1].Y, First.Corners[2].Y);
		}
		ListCursor.Buffer = Buffer;
		ListCursor.bShown = true;
	}

	void FBattleHud::DrawEventMessage()
	{
		FBattleEventWork& Event = Battle->Event;
		if (Event.bPortraitShown)
		{
			Link(EventPortrait[EventPortraitBuffer]);
		}
		if (!Event.bMessageShown || !Event.Text)
		{
			return;
		}
		// The kernel's window draws its lines as they are, then types: a glyph shows from the frame after.
		FMesWindow& Window = *Event.Text;
		Window.Update(Text);
		if (!Window.Drew())
		{
			return;
		}
		const int32 Pitch = Window.WidthUnits() + ImageMargin;
		if (EventTextImage.Rect.Width != Pitch)
		{
			FaultState = EBattleHudFault::EventTextNotMade;
			return;
		}
		const std::size_t FirstLinked = Drawing->Primitives.size();
		const int32 Unit = Text.Font != nullptr ? Text.Font->UnitPixels() : 4;
		const auto LinkLines = [this, &Window, FirstLinked, Unit](bool bSecondPage)
		{
			for (int32 Row = 0; Row < Window.Lines(); ++Row)
			{
				const FMesWindow::FLine& Line = Window.Row(Row);
				const int32 Pixels = Line.WidthUnits * Unit - (bSecondPage ? PagePixels : 0);
				if (Pixels <= 0)
				{
					continue;
				}
				FMenuPrimitive Sprite;
				Sprite.Code = MenuGpu::Sprite | MenuGpu::RawTexture;
				Sprite.Corners[0].X = static_cast<int16>(EventTextX + (bSecondPage ? PagePixels : 0));
				Sprite.Corners[0].Y = static_cast<int16>(EventTextY + LineSpacing * Row);
				Sprite.Corners[0].U = static_cast<uint8>((TextVramX & 0x3F) * 4);
				Sprite.Corners[0].V = static_cast<uint8>(TextVramY + Line.Strip / 2 * TextRows);
				Sprite.Clut = (Line.Strip & 1) != 0 ? OddTextClut : EvenTextClut;
				Sprite.Width = static_cast<int16>(bSecondPage ? Pixels : std::min<int32>(Pixels, PagePixels));
				Sprite.Height = TextRows;
				Link(Sprite);
				const std::size_t Linked = Drawing->Primitives.size() - 1;
				const std::size_t FirstCell = Drawing->TextButtons.size();
				EventButtons.Add(Window, Row, bSecondPage ? PagePixels : 0, Sprite.Width, Text.Font, static_cast<uint32>(Linked), Drawing->TextButtons);
				// The row's second page, linked before it, moves with what follows a button of the first.
				for (std::size_t Other = FirstLinked; !bSecondPage && FirstCell < Drawing->TextButtons.size() && Other < Linked; ++Other)
				{
					const FMenuCorner& At = Drawing->Primitives[Other].Corners[0];
					if (At.Y == Sprite.Corners[0].Y && At.X == Sprite.Corners[0].X + PagePixels)
					{
						for (std::size_t Cell = FirstCell; Cell < Drawing->TextButtons.size(); ++Cell)
						{
							Drawing->TextButtons[Cell].LineRest = static_cast<uint32>(Other + 1);
						}
					}
				}
			}
		};
		LinkLines(true);
		Link(TextMode(TextVramX + PageUnits));
		LinkLines(false);
		if (Window.Stepped())
		{
			// A line's whole strip is uploaded, of which only the line's own width in its own plane is drawn.
			DrawMesTypedLines(Text.Font, Window, EventTextImage.Units, Pitch, [this](int32 Strip)
			{
				EventTextImage.Rect.Y = static_cast<int16>(TextVramY + Strip / 2 * TextRows);
				Upload(EventTextImage);
			});
		}
		// The event's window draws no box of its own behind the text: the HUD's window 0 is its frame.
		Link(TextMode(TextVramX));
	}
}
