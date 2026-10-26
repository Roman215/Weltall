#include "Battle/BattleContext.h"
#include "Battle/Show/BattleHudGpu.h"
#include "Battle/Show/BattleShowHud.h"
#include "Text/MesRender.h"

namespace Weltall
{
	using namespace BattleHudGpu;

	namespace
	{
		/** A window's corners' frames: the result screen's, and the battle's (top left, top right, bottom left, bottom right). */
		constexpr std::array<int32, 4> ResultCornerFrames{0xF0, 0xF2, 0xF5, 0xF7};
		constexpr std::array<int32, 4> BattleCornerFrames{0x4A, 0x4C, 0x4F, 0x51};
		constexpr int32 FullScale = 0x1000;
		constexpr int16 CornerSize = 8;
		constexpr uint16 Growth = 0x20;
		constexpr uint8 ModeResult = 0;
		constexpr uint8 ModeBattle = 1;
		/** The two windows behind the message boxes: the name box's at the top, the boxes' at the foot. */
		constexpr int32 NameWindow = 5;
		constexpr int32 BoxWindow = 4;
		constexpr int16 BoxRowX = 0x3C0;
		constexpr int16 BoxRowUnits = 0x3C;
		constexpr int16 TextRows = 13;
		/** Box 0 (the name box) is centred at x 64 near the top; the others at x 154 at the foot (FBattleHudPlaces). */
		constexpr int16 NameBoxCentre = 0x40;
		constexpr int16 NameBoxY = 0x2C;
		constexpr int16 BoxCentre = 0x9A;
	}

	void FBattleHud::SetWindowPart(FMenuPrimitive& Part) const
	{
		SetSemiTrans(Part, true);
		SetShadeTex(Part, false);
		Part.Corners[0].Color = {0x80, 0x80, 0x80};
		Part.Page = static_cast<uint16>(Settings.WindowBlend == 0 ? Part.Page & ~PageSubtract : Part.Page | PageSubtract);
	}

	void FBattleHud::InitWindowFill(FBattleHudWindow& Window)
	{
		const std::array<uint8, 3>& Colour = Settings.WindowColour;
		for (int32 Pair = 0; Pair < 2; ++Pair)
		{
			FMenuPrimitive& Fill = Window.Fill[static_cast<std::size_t>(Pair)];
			BattleHudGpu::Set(Fill, GouraudQuad);
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				SetColor(Fill, Corner, Colour[0], Colour[1], Colour[2]);
			}
			SetSemiTrans(Fill, true);
			SetDrawMode(Window.FillMode[static_cast<std::size_t>(Pair)],
				MenuGpu::TexturePage(0, Settings.WindowBlend, EdgeTextures[0].VramX, EdgeTextures[0].VramY), TextureWindows[0]);
		}
		for (std::size_t Edge = 0; Edge < Window.Edges.size(); ++Edge)
		{
			const FBattleHudTexture& Texture = EdgeTextures[Edge];
			for (FBattleHudPair& Half : Window.Edges[Edge])
			{
				for (FMenuPrimitive& Quad : Half)
				{
					BattleHudGpu::Set(Quad, TexturedQuad);
					SetShadeTex(Quad, true);
					Quad.Corners[0].Color = {0xFF, 0xFF, 0xFF};
					Quad.Page = MenuGpu::TexturePage(Texture.Depth, 0, Texture.VramX, Texture.VramY);
					Quad.Clut = MenuGpu::Clut(Texture.ClutX, Texture.ClutY);
				}
			}
		}
	}

	void FBattleHud::OpenWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bGrows)
	{
		if (Index < 0 || Index >= WindowCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		FBattleHudWindow& Window = Windows[static_cast<std::size_t>(Index)];
		if (!Window.bShown)
		{
			// A window not shown is made afresh, its growth's record with it, but not the flags that say it grows:
			// a window opened growing and opened again before it shows goes on growing from the cleared record, the
			// frame of window 0 at its place of nothing.
			const bool bGrowing = Window.bGrowing;
			const bool bGrown = Window.bGrown;
			Window = {};
			Window.bGrowing = bGrowing;
			Window.bGrown = bGrown;
			Window.bMade = true;
			InitWindowFill(Window);
		}
		if (!bGrows)
		{
			DrawWindowFrame(Window, X, Y, static_cast<int16>(Width), static_cast<int16>(Height));
			return;
		}
		Window.Number = static_cast<uint8>(Index);
		Window.X = X;
		Window.Y = Y;
		Window.Width = Width;
		Window.Height = Height;
		Window.GrownWidth = 0;
		Window.GrownHeight = 0;
		Window.bGrown = false;
		Window.bGrowing = true;
	}

	void FBattleHud::CloseWindow(int32 Index)
	{
		if (Index < 0 || Index >= WindowCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		Windows[static_cast<std::size_t>(Index)].bShown = false;
		Windows[static_cast<std::size_t>(Index)].bGrowing = false;
		Windows[static_cast<std::size_t>(Index)].bMade = false;
	}

	void FBattleHud::GrowWindows()
	{
		for (std::size_t Index = 0; Index < Windows.size(); ++Index)
		{
			FBattleHudWindow& Growing = Windows[Index];
			if (!Growing.bGrowing || Growing.bGrown)
			{
				continue;
			}
			int32 Done = 0;
			if (static_cast<uint32>(Growing.GrownWidth) + Growth < Growing.Width)
			{
				Growing.GrownWidth = static_cast<uint16>(Growing.GrownWidth + Growth);
			}
			else
			{
				Growing.GrownWidth = Growing.Width;
				++Done;
			}
			if (static_cast<uint32>(Growing.GrownHeight) + Growth < Growing.Height)
			{
				Growing.GrownHeight = static_cast<uint16>(Growing.GrownHeight + Growth);
			}
			else
			{
				Growing.GrownHeight = Growing.Height;
				++Done;
			}
			if (Done == 2)
			{
				Growing.bGrown = true;
				if (Index == 0)
				{
					Battle->Party.bFirstWindowGrown = true;
				}
			}
			const int16 X = static_cast<int16>(Growing.X + (Growing.Width >> 1) - (Growing.GrownWidth >> 1));
			const int16 Y = static_cast<int16>(Growing.Y + (Growing.Height >> 1) - (Growing.GrownHeight >> 1));
			DrawWindowFrame(Windows[Growing.Number % Windows.size()], X, Y, static_cast<int16>(Growing.GrownWidth), static_cast<int16>(Growing.GrownHeight));
		}
	}

	void FBattleHud::DrawWindowFrame(FBattleHudWindow& Window, int16 X, int16 Y, int16 Width, int16 Height)
	{
		if (!Window.bMade)
		{
			FaultState = EBattleHudFault::WindowNotMade;
			return;
		}
		Window.bShown = false;
		const int16 Right = static_cast<int16>(X + Width);
		const int16 Bottom = static_cast<int16>(Y + Height);
		SetCorners(Window.Fill[Buffer], X, Y, Right, Y, X, Bottom, Right, Bottom);
		DrawWindowCorners(Window, X, Y, Width, static_cast<uint16>(Height));
		DrawWindowEdges(Window, X, Y, static_cast<uint16>(Width), static_cast<uint16>(Height));
		Window.Buffer = Buffer;
		Window.bShown = true;
	}

	void FBattleHud::DrawWindowCorners(FBattleHudWindow& Window, int16 X, int16 Y, int16 Width, uint16 Height)
	{
		const std::array<int32, 4>& Frames = Battle->Party.InputMode == ModeResult ? ResultCornerFrames : BattleCornerFrames;
		const int16 Right = static_cast<int16>(X + Width - CornerSize);
		const int16 Bottom = static_cast<int16>(Y + Height - CornerSize);
		Window.CornerCount = 0;
		Window.CornerCount += DrawSheet(Frames[0], Window.Corners, Window.CornerCount, X, Y, FullScale);
		Window.CornerCount += DrawSheet(Frames[1], Window.Corners, Window.CornerCount, Right, Y, FullScale);
		Window.CornerCount += DrawSheet(Frames[2], Window.Corners, Window.CornerCount, X, Bottom, FullScale);
		Window.CornerCount += DrawSheet(Frames[3], Window.Corners, Window.CornerCount, Right, Bottom, FullScale);
		for (std::size_t Part = 0; Part < 4; ++Part)
		{
			SetWindowPart(Window.Corners[Part][Buffer]);
		}
	}

	void FBattleHud::DrawWindowEdges(FBattleHudWindow& Window, int16 X, int16 Y, uint16 Width, uint16 Height)
	{
		// Each edge is two stretched halves between the corners, the halves' lengths rounded toward zero.
		const int16 HalfWidth = static_cast<int16>((static_cast<int32>(Width) - 2 * CornerSize) / 2);
		const int16 HalfHeight = static_cast<int16>((static_cast<int32>(Height) - 2 * CornerSize) / 2);
		const auto Place = [this, &Window](std::size_t Edge, int32 Half, int32 X0, int32 Y0, int32 X1, int32 Y1) {
			FMenuPrimitive& Quad = Window.Edges[Edge][static_cast<std::size_t>(Half)][Buffer];
			SetCorners(Quad, X0, Y0, X1, Y0, X0, Y1, X1, Y1);
		};
		const auto Texels = [this, &Window](std::size_t Edge, int32 U0, int32 U1, int32 V, int32 Rows) {
			for (FBattleHudPair& Half : Window.Edges[Edge])
			{
				SetTexels(Half[Buffer], U0, V, U1, V, U0, V + Rows, U1, V + Rows);
				SetWindowPart(Half[Buffer]);
			}
		};
		const int16 Inside = static_cast<int16>(X + CornerSize);
		const int16 Middle = static_cast<int16>(Inside + HalfWidth);
		const int16 Far = static_cast<int16>(Middle + HalfWidth);
		Place(0, 0, Inside, Y - CornerSize, Middle, Y + CornerSize);
		Place(0, 1, Middle, Y - CornerSize, Far, Y + CornerSize);
		const int32 TopU = (EdgeTextures[0].VramX & 0x3F) * 2;
		Texels(0, TopU, TopU + 7, EdgeTextures[0].VramY, 0x10);
		const int16 Base = static_cast<int16>(Y + static_cast<int16>(Height));
		Place(1, 0, Inside, Base - CornerSize, Middle, Base + CornerSize);
		Place(1, 1, Middle, Base - CornerSize, Far, Base + CornerSize);
		const int32 BottomU = (EdgeTextures[1].VramX & 0x3F) * 2;
		Texels(1, BottomU - 8, BottomU - 1, EdgeTextures[1].VramY, 0x10);
		const int16 Upper = static_cast<int16>(Y + CornerSize);
		const int16 Centre = static_cast<int16>(Upper + HalfHeight);
		const int16 Lower = static_cast<int16>(Centre + HalfHeight);
		Place(2, 0, X - CornerSize, Upper, X + CornerSize, Centre);
		Place(2, 1, X - CornerSize, Centre, X + CornerSize, Lower);
		const int32 LeftU = (EdgeTextures[2].VramX & 0x3F) * 2;
		Texels(2, LeftU + 0xE, LeftU + 0x1E, EdgeTextures[2].VramY, 7);
		const int16 Side = static_cast<int16>(X + static_cast<int16>(Width));
		Place(3, 0, Side - CornerSize, Upper, Side + CornerSize, Centre);
		Place(3, 1, Side - CornerSize, Centre, Side + CornerSize, Lower);
		const int32 RightU = (EdgeTextures[3].VramX & 0x3F) * 2;
		Texels(3, RightU + 0xE, RightU + 0x1E, EdgeTextures[3].VramY, 7);
	}

	void FBattleHud::DrawWindows()
	{
		const bool bBattle = Battle->Party.InputMode == ModeBattle;
		Link(PlainMode[Buffer]);
		for (const FBattleHudPanel& Panel : Panels)
		{
			if (Panel.bGaugeShown && bBattle)
			{
				Link(Panel.Gauge[Panel.IconBuffer & 1]);
			}
		}
		if (bApBarShown && bBattle)
		{
			Link(ApBar[Flight.Buffer & 1]);
		}
		Link(GaugeMode[Buffer]);
		for (const FBattleHudWindow& Window : Windows)
		{
			if (!Window.bShown)
			{
				continue;
			}
			// A window shown that was never made, or was closed, links whatever its pointer left behind.
			if (!Window.bMade)
			{
				FaultState = EBattleHudFault::WindowNotMade;
				return;
			}
			const uint8 From = Window.Buffer;
			LinkPairs(Window.Corners, Window.CornerCount, From);
			for (const std::size_t Edge : {std::size_t{2}, std::size_t{3}, std::size_t{0}, std::size_t{1}})
			{
				Link(Window.Edges[Edge][0][From]);
				Link(Window.Edges[Edge][1][From]);
			}
			Link(Window.Fill[From]);
			Link(Window.FillMode[From]);
		}
	}

	void FBattleHud::BeginMessageBoxes()
	{
		OpenWindow(NameWindow, 8, 0x2A, 0x70, 0x12, false);
		Windows[NameWindow].bShown = false;
		OpenWindow(BoxWindow, 0x20, Resources.Tables.Places.MessageWindowY, 0xF4, 0x12, false);
		Windows[BoxWindow].bShown = false;
		for (int32 Box = 0; Box < MessageBoxCount; ++Box)
		{
			FBattleHudMessageBox& Record = MessageBoxes[static_cast<std::size_t>(Box)];
			const int16 RowY = static_cast<int16>(Box / 2 * TextRows);
			Record.Row = {BoxRowX, RowY, BoxRowUnits, TextRows};
			Record.Plane = static_cast<uint8>(Box & 1);
			Record.bShown = false;
			for (FMenuPrimitive& Quad : Record.Quad)
			{
				BattleHudGpu::Set(Quad, TexturedQuad);
				Quad.Corners[0].Color = {0x80, 0x80, 0x80};
				SetSemiTrans(Quad, false);
				SetShadeTex(Quad, true);
				Quad.Clut = Record.Plane != 0 ? OddTextClut : EvenTextClut;
				Quad.Page = MenuGpu::TexturePage(0, 0, BoxRowX, RowY);
			}
		}
	}

	void FBattleHud::TypeMessageBox(int32 Box, std::span<const uint8> Line)
	{
		if (Box < 0 || Box >= MessageBoxCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		FBattleHudMessageBox& Record = MessageBoxes[static_cast<std::size_t>(Box)];
		FVramImage& Row = BoxRows[static_cast<std::size_t>(Box / 2)];
		const int32 Width = RenderMesString(Text, BoxTyping, Line, Row, Record.Plane);
		Row.Rect.X = Record.Row.X;
		Row.Rect.Y = Record.Row.Y;
		Upload(Row);
		Record.Width = static_cast<uint8>(Width);
	}

	void FBattleHud::ShowMessageBox(int32 Box)
	{
		if (Box < 0 || Box >= MessageBoxCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		Windows[BoxWindow].bShown = true;
		MessageBoxes[static_cast<std::size_t>(Box)].bShown = true;
	}

	void FBattleHud::HideMessageBox(int32 Box)
	{
		if (Box < 0 || Box >= MessageBoxCount)
		{
			FaultState = EBattleHudFault::IndexOutsideHud;
			return;
		}
		Windows[BoxWindow].bShown = false;
		MessageBoxes[static_cast<std::size_t>(Box)].bShown = false;
	}

	void FBattleHud::HideMessageBoxes()
	{
		for (FBattleHudMessageBox& Record : MessageBoxes)
		{
			Record.bShown = false;
		}
		Windows[NameWindow].bShown = false;
		Windows[BoxWindow].bShown = false;
	}

	void FBattleHud::ShowNameBox(std::span<const uint8> Name)
	{
		Windows[NameWindow].bShown = true;
		TypeMessageBox(0, Name);
		MessageBoxes[0].bShown = true;
	}

	void FBattleHud::DrawMessageBoxes()
	{
		for (std::size_t Box = 0; Box < MessageBoxes.size(); ++Box)
		{
			FBattleHudMessageBox& Record = MessageBoxes[Box];
			if (!Record.bShown)
			{
				continue;
			}
			const int16 Half = static_cast<int16>(Record.Width >> 1);
			FMenuPrimitive& Quad = Record.Quad[Buffer];
			if (Box == 0)
			{
				SetTextQuad(Quad, static_cast<int16>(NameBoxCentre - Half), NameBoxY, 0, 0, Record.Width);
			}
			else
			{
				SetTextQuad(Quad, static_cast<int16>(BoxCentre - Half), Resources.Tables.Places.MessageBoxY, 0, static_cast<uint8>(Box / 2 * TextRows), Record.Width);
			}
			Link(Quad);
		}
	}
}
