#include "Field/FieldScreenLayer.h"

#include "Field/EventContext.h"
#include "Field/FieldDrawingTables.h"
#include "Field/FieldMessages.h"
#include "Field/FieldRuntime.h"
#include "FieldScreenPieces.h"
#include "Presentation/DrawList.h"
#include "Text/MesButtonCells.h"
#include "Text/MesFont.h"
#include "Text/MesRender.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		constexpr int32 LineSpacing = 14;
		constexpr int32 LineRows = 13;
		/** The maps whose messages use the glyphs as shapes, not buttons: the character files' bullets, a score list's, the debug maps' marks. */
		constexpr std::array<int32, 8> GlyphShapeMaps{237, 275, 277, 281, 283, 580, 720, 723};
		constexpr int32 UnitPixels = 4;
		/** A line wider than a 4-bit page is drawn as two sprites, the second from the next page. */
		constexpr int32 PageUnits = 64;
		constexpr int32 PagePixels = PageUnits * UnitPixels;
		/** The message font's CLUTs: even lines read the low half of each texel, odd lines the high. */
		constexpr int32 TextClutX = 256;
		constexpr int32 TextClutY = 240;
		constexpr int32 TextClutStride = 16;
		/** The windows' background: this colour subtracted from the scene behind the frame. */
		constexpr FDrawColor BackgroundColor{0x88, 0x76, 0x54};
		constexpr FDrawColor Neutral{128, 128, 128};
		/** The system images' 4-bit page and the CLUTs of the frame and the cursors. */
		constexpr FTexturePage FramePage{640, 256, ETextureDepth::Indexed4, EBlendMode::Subtract};
		constexpr FTexturePage CursorPage{640, 256, ETextureDepth::Indexed4, EBlendMode::Average};
		constexpr FClutPosition FrameClut{256, 244};
		constexpr FClutPosition CursorClut{256, 246};
		/** The faces' 8-bit page and their CLUTs, a row each from this one on. */
		constexpr FTexturePage FacePage{704, 256, ETextureDepth::Indexed8, EBlendMode::Average};
		constexpr int32 FaceClutY = 224;
		constexpr int32 FaceTexels = 64;
		/** Where the field's sprites drawn through texture windows start their texture coordinates. */
		constexpr int32 WindowedU = 0x80;
		constexpr int32 WindowedV = 0xC0;
		constexpr int16 ScreenWidth = 320;
		constexpr int16 ScreenHeight = 224;
		/** Far past any display's edge, on the 320 x 224 screen's scale. */
		constexpr int16 Beyond = 8192;
		/** The menu's shading: grey subtracted 4 a step (its tile's key is apart from the fades' and the windows'). */

		/** The frame's pieces as drawn: top left, right edge, top right, left edge, bottom left, top edge, bottom right, bottom edge. */
		struct FPiecePlace
		{
			/** Relative to the frame: 0 its left or top, 1 its right or bottom; plus a fixed offset. */
			int8 SideX = 0;
			int16 OffsetX = 0;
			int8 SideY = 0;
			int16 OffsetY = 0;
			/** The piece stretches along the frame's side by the frame's size less this, or keeps its window's size (-1). */
			int16 SpanWidth = -1;
			int16 SpanHeight = -1;
		};

		constexpr FPiecePlace PiecePlaces[8] = {
			{0, -8, 0, -7, -1, -1},
			{1, -8, 0, 9, -1, 18},
			{1, -8, 0, -7, -1, -1},
			{0, -8, 0, 9, -1, 18},
			{0, -8, 1, -9, -1, -1},
			{0, 8, 0, -7, 16, -1},
			{1, -8, 1, -9, -1, -1},
			{0, 8, 1, -9, 16, -1},
		};

		/**
		 * The texels a texture window gives a sprite drawn from texture coordinate Start over Length
		 * pixels, as the GPU masks and replaces the coordinate's bits: runs of consecutive texels.
		 */
		template <typename TRun>
		void ForEachWindowRun(int32 Start, int32 Length, int16 Origin, int16 Size, TRun&& Run)
		{
			const int32 Mask = ((~(Size - 1) & 0xFF) >> 3) << 3;
			const int32 Replaced = ((((Origin & 0xFF) >> 3) & (Mask >> 3)) << 3);
			const int32 Period = Mask != 0 ? (Mask & -Mask) : 256;
			for (int32 Done = 0; Done < Length;)
			{
				const int32 Coordinate = (Start + Done) & 0xFF;
				const int32 Count = std::min(Length - Done, Period - (Coordinate & (Period - 1)));
				Run(Done, (Coordinate & ~Mask) | Replaced, Count);
				Done += Count;
			}
		}

		FDrawPrimitive Rectangle(int32 X, int32 Y, int32 Width, int32 Height, FDrawColor Color)
		{
			return MakeGouraudRect({static_cast<int16>(X), static_cast<int16>(Y), static_cast<int16>(Width), static_cast<int16>(Height)},
				{Color, Color, Color, Color});
		}

		class FLayerBuilder
		{
		public:
			FLayerBuilder(const FEventContext& InContext, const FFieldDrawingTables& InTables, FDrawList& InLayer, std::vector<FTextButtonCell>* InButtons)
				: Context(InContext), Tables(InTables), Layer(InLayer), Buttons(InButtons)
			{
			}

			void Fade(int32 Index)
			{
				const FFieldFade& Fade = Context.Render.Fades[static_cast<std::size_t>(Index)];
				if (!Fade.bActive)
				{
					return;
				}
				const FMdlColor Color = Fade.DrawnColor();
				FDrawPrimitive Tile = Rectangle(-Beyond, -Beyond, ScreenWidth + 2 * Beyond, ScreenHeight + 2 * Beyond, {Color.R, Color.G, Color.B});
				Tile.bGouraud = false;
				Tile.Blend = EDrawBlend::SemiTransparent;
				Tile.UntexturedBlend = static_cast<EBlendMode>(Fade.BlendMode & 3);
				Tile.Clip = {-Beyond, -Beyond, static_cast<int16>(ScreenWidth + 2 * Beyond), static_cast<int16>(ScreenHeight + 2 * Beyond)};
				Tile.BlendKey = static_cast<uint32>(1 + Index);
				Layer.Primitives.push_back(Tile);
			}

			/** The field's picture under the menu's subtractive grey, over everything. */
			void MenuShade()
			{
				const FFieldMenuShade& Shade = Context.Render.MenuShade;
				if (!Shade.bShown)
				{
					return;
				}
				const uint8 Grey = static_cast<uint8>(Shade.Level * FFieldMenuShade::GreyPerLevel);
				FDrawPrimitive Tile = Rectangle(-Beyond, -Beyond, ScreenWidth + 2 * Beyond, ScreenHeight + 2 * Beyond, {Grey, Grey, Grey});
				Tile.bGouraud = false;
				Tile.Blend = EDrawBlend::SemiTransparent;
				Tile.UntexturedBlend = EBlendMode::Subtract;
				Tile.Clip = {-Beyond, -Beyond, static_cast<int16>(ScreenWidth + 2 * Beyond), static_cast<int16>(ScreenHeight + 2 * Beyond)};
				Tile.BlendKey = FFieldMenuShade::BlendKey;
				Layer.Primitives.push_back(Tile);
			}

			void Window(int32 Index)
			{
				const FFieldMessageWindow& Window = Context.Messages.Windows[static_cast<std::size_t>(Index)];
				const FFieldMessageFrame& Frame = Window.Frame;
				const FFieldScreenRect& Rect = Frame.Rect;
				const uint32 Key = static_cast<uint32>(Index + 1) << 16;
				const bool bBackground = (Window.Flags & FieldMessageWindowFlags::NoBackground) == 0;
				const int32 CursorFrame = std::clamp(Context.Messages.CursorFrame, 0, FFieldDrawingTables::CursorFrames - 1);
				if (bBackground)
				{
					FDrawPrimitive Box = Rectangle(Rect.X, Rect.Y + 1, Rect.Width, Rect.Height - 2, BackgroundColor);
					Box.bGouraud = false;
					Box.Blend = EDrawBlend::SemiTransparent;
					Box.UntexturedBlend = EBlendMode::Subtract;
					Box.BlendKey = Key | 1;
					Layer.Primitives.push_back(Box);
				}
				if (Frame.bChoiceCursor)
				{
					Windowed(Frame.ChoiceCursorX, Frame.ChoiceCursorY, 8, 12, Tables.ChoiceCursor[static_cast<std::size_t>(CursorFrame)], CursorPage, CursorClut, false, 0);
				}
				if (Window.bPortrait)
				{
					Portrait(Window, Key | 2);
				}
				if (bBackground)
				{
					for (int32 Piece = 7; Piece >= 0; --Piece)
					{
						FramePiece(Rect, Window.Rect, Piece, Key | static_cast<uint32>(Piece + 1) << 8);
					}
				}
				if (Frame.bMoreCursor)
				{
					Windowed(Frame.MoreCursorX, Frame.MoreCursorY, 12, 8, Tables.MoreCursor[static_cast<std::size_t>(CursorFrame)], CursorPage, CursorClut, false, 0);
				}
				if (Window.OpenTicks == 0 && Window.Text)
				{
					Text(Window, Index);
				}
			}

		private:
			/**
			 * A sprite drawn through a texture window, as runs of consecutive texels; Key, when not 0, keys
			 * its runs. As many runs as a sprite Room x RoomHeight would take, those past its size empty, so
			 * a frame keeps its runs as it opens and the display can draw them in between.
			 */
			void Windowed(int32 X, int32 Y, int32 Width, int32 Height, const FFieldTexelWindow& Window, const FTexturePage& Page, const FClutPosition& Clut,
				bool bSemiTransparent, uint32 Key, int32 Room = 0, int32 RoomHeight = 0)
			{
				uint32 Run = 0;
				ForEachWindowRun(WindowedV, std::max(Height, RoomHeight), Window.V, Window.Height, [&](int32 Down, int32 V, int32 RunRows)
				{
					ForEachWindowRun(WindowedU, std::max(Width, Room), Window.U, Window.Width, [&](int32 Across, int32 U, int32 RunColumns)
					{
						// A run past the sprite's size lies folded at its far edge, so neighbours keep meeting as it grows.
						const int32 Columns = std::clamp(Width - Across, 0, RunColumns);
						const int32 Rows = std::clamp(Height - Down, 0, RunRows);
						FDrawPrimitive Sprite = MakeSprite(X + std::min(Across, Width), Y + std::min(Down, Height), Columns, Rows, {0, Page, Clut}, static_cast<uint8>(U),
							static_cast<uint8>(V), Neutral);
						Sprite.Blend = bSemiTransparent ? EDrawBlend::SemiTransparent : EDrawBlend::Opaque;
						Sprite.BlendKey = Key != 0 ? Key | Run : 0;
						++Run;
						Layer.Primitives.push_back(Sprite);
					});
				});
			}

			/** A piece of the frame as it is drawn at Rect, with the runs it will have at the window's full size Open. */
			void FramePiece(const FFieldScreenRect& Rect, const FFieldScreenRect& Open, int32 Piece, uint32 Key)
			{
				const FPiecePlace& Place = PiecePlaces[Piece];
				const FFieldTexelWindow& Window = Tables.FramePieces[static_cast<std::size_t>(Piece)];
				const int32 X = Rect.X + Place.SideX * Rect.Width + Place.OffsetX;
				const int32 Y = Rect.Y + Place.SideY * Rect.Height + Place.OffsetY;
				const auto Span = [](int16 Size, int16 Less, int16 Own) { return Less < 0 ? static_cast<int32>(Own) : std::max(0, Size - Less); };
				Windowed(X, Y, Span(Rect.Width, Place.SpanWidth, Window.Width), Span(Rect.Height, Place.SpanHeight, Window.Height), Window, FramePage, FrameClut, true,
					Key, Span(Open.Width, Place.SpanWidth, Window.Width), Span(Open.Height, Place.SpanHeight, Window.Height));
			}

			void Portrait(const FFieldMessageWindow& Window, uint32 Key)
			{
				const FFieldMessageFrame& Frame = Window.Frame;
				const std::array<uint8, 2>& Cell = Tables.PortraitCells[static_cast<std::size_t>(Window.PortraitCell & 7)];
				// The GPU samples a quad whose texels run right to left one texel further on than its corners say.
				const int32 U = Cell[0] + (Frame.PortraitX1 < Frame.PortraitX0 ? 1 : 0);
				FDrawPrimitive Quad;
				for (std::size_t Corner = 0; Corner < 4; ++Corner)
				{
					const bool bRight = (Corner & 1) != 0;
					const bool bBottom = (Corner & 2) != 0;
					FDrawVertex& Vertex = Quad.Corners[Corner];
					Vertex.X = bRight ? Frame.PortraitX1 : Frame.PortraitX0;
					Vertex.Y = bBottom ? Frame.PortraitY1 : Frame.PortraitY0;
					Vertex.U = static_cast<float>(U + (bRight ? FaceTexels : 0));
					Vertex.V = static_cast<float>(Cell[1] + (bBottom ? FaceTexels : 0));
					Vertex.Color = Neutral;
				}
				Quad.Texture = FDrawTexture{0, FacePage, FClutPosition{0, FaceClutY + (Window.PortraitCell & 7)}};
				Quad.BlendKey = Key;
				Layer.Primitives.push_back(Quad);
			}

			/**
			 * Each line as the kernel draws it: one sprite of its typed width from the window's text, a second past a page's
			 * width; the highlighted line modulated by the window's colour. (The French translation's program makes the second
			 * 128 pixels too wide, which would show what lies past the line.)
			 */
			void Text(const FFieldMessageWindow& Window, int32 Index)
			{
				const FMesWindow& Text = *Window.Text;
				const std::array<int16, 2>& Place = Tables.MessageText[static_cast<std::size_t>(Index)];
				const int32 PageX = Place[0] & ~(PageUnits - 1);
				const int32 PageY = Place[1] & ~0xFF;
				const uint8 U = static_cast<uint8>((Place[0] & (PageUnits - 1)) * UnitPixels);
				const std::array<uint8, 3>& Highlight = Text.HighlightColor();
				const FMesFont* Font = Context.Messages.Text.Font;
				for (int32 Row = 0; Row < Text.Lines(); ++Row)
				{
					const int32 Width = Text.Row(Row).WidthUnits * (Font != nullptr ? Font->UnitPixels() : UnitPixels);
					if (Width == 0)
					{
						continue;
					}
					const bool bHighlighted = Text.HighlightedRow() == Row;
					const FClutPosition Clut{TextClutX + TextClutStride * (Row & 1), TextClutY};
					const uint8 V = static_cast<uint8>(Place[1] + Row / 2 * LineRows);
					const int32 Y = Window.TextY + LineSpacing * Row;
					const std::size_t FirstCell = Buttons != nullptr ? Buttons->size() : 0;
					for (int32 Half = 0; Half < 2; ++Half)
					{
						const int32 Pixels = Half == 0 ? std::min(Width, PagePixels) : Width - PagePixels;
						if (Pixels <= 0)
						{
							continue;
						}
						const FTexturePage Page{PageX + Half * PageUnits, PageY, ETextureDepth::Indexed4, EBlendMode::Average};
						FDrawPrimitive Line = MakeSprite(Window.TextX + Half * PagePixels, Y, Pixels, LineRows, {0, Page, Clut}, U, V,
							bHighlighted ? FDrawColor{Highlight[0], Highlight[1], Highlight[2]} : Neutral);
						Line.bModulate = bHighlighted;
						Layer.Primitives.push_back(Line);
						AddButtonCells(Text, Row, Half * PagePixels, Pixels);
					}
					if (Buttons != nullptr)
					{
						for (std::size_t Cell = FirstCell; Cell < Buttons->size(); ++Cell)
						{
							(*Buttons)[Cell].LineEnd = static_cast<uint32>(Layer.Primitives.size());
						}
					}
				}
			}

			/** The buttons the line's sprite just added shows, from pixel First of the line on (FMesButtonCells). */
			void AddButtonCells(const FMesWindow& Text, int32 Row, int32 First, int32 Pixels)
			{
				if (Buttons == nullptr || std::find(GlyphShapeMaps.begin(), GlyphShapeMaps.end(), Context.RunningMap) != GlyphShapeMaps.end())
				{
					return;
				}
				Cells.Add(Text, Row, First, Pixels, Context.Messages.Text.Font, static_cast<uint32>(Layer.Primitives.size() - 1), *Buttons);
			}

			const FEventContext& Context;
			const FFieldDrawingTables& Tables;
			FDrawList& Layer;
			std::vector<FTextButtonCell>* Buttons = nullptr;
			FMesButtonCells Cells;
		};

		/**
		 * The order the field's windows take their turns in a tick: the focused ones first, then the
		 * rest by age, frontmost first (the last focused one only in the first turn). Each window's
		 * primitives go in front of those before it, so the first to take its turn is drawn last.
		 */
		int32 WindowTurns(const FFieldMessages& Messages, std::array<int32, FFieldMessages::WindowCount>& Turns)
		{
			int32 LastFocused = -1;
			for (int32 Window = 0; Window < FFieldMessages::WindowCount; ++Window)
			{
				if (Messages.Windows[static_cast<std::size_t>(Window)].bFocus)
				{
					LastFocused = Window;
				}
			}
			int32 Count = 0;
			const auto Take = [&Turns, &Count](int32 Window)
			{
				if (std::find(Turns.begin(), Turns.begin() + Count, Window) == Turns.begin() + Count)
				{
					Turns[static_cast<std::size_t>(Count++)] = Window;
				}
			};
			for (int32 Window = 0; Window < FFieldMessages::WindowCount; ++Window)
			{
				const FFieldMessageWindow& Candidate = Messages.Windows[static_cast<std::size_t>(Window)];
				if (Candidate.bOpen && Candidate.bFocus)
				{
					Take(Window);
				}
			}
			for (int32 Age = 0; Age < FFieldMessages::WindowCount; ++Age)
			{
				for (int32 Window = 0; Window < FFieldMessages::WindowCount; ++Window)
				{
					const FFieldMessageWindow& Candidate = Messages.Windows[static_cast<std::size_t>(Window)];
					if (Candidate.bOpen && Candidate.Age == Age && Window != LastFocused)
					{
						Take(Window);
					}
				}
			}
			return Count;
		}
	}

	void TypeFieldMessageText(const FFieldMessages& Messages, const FFieldDrawingTables& Tables, FVram& Vram, FVramImage& Scratch)
	{
		if (Messages.Text.Font == nullptr)
		{
			return;
		}
		const FMesFont& Font = *Messages.Text.Font;
		for (int32 Index = 0; Index < FFieldMessages::WindowCount; ++Index)
		{
			const FFieldMessageWindow& Window = Messages.Windows[static_cast<std::size_t>(Index)];
			if (!Window.bOpen || !Window.Text)
			{
				continue;
			}
			const FMesWindow& Text = *Window.Text;
			const std::array<int16, 2>& Place = Tables.MessageText[static_cast<std::size_t>(Index)];
			const int32 Pitch = Text.WidthUnits() + 3;
			const int32 PixelWidth = Pitch * UnitPixels;
			for (int32 Strip = 0; 2 * Strip < Text.Lines(); ++Strip)
			{
				Scratch.Rect = {Place[0], static_cast<int16>(Place[1] + Strip * LineRows), static_cast<int16>(Pitch), static_cast<int16>(LineRows)};
				Scratch.Units.assign(static_cast<std::size_t>(Pitch * LineRows), 0);
				for (int32 Plane = 0; Plane < 2 && 2 * Strip + Plane < Text.Lines(); ++Plane)
				{
					for (const FMesPlacedGlyph& Glyph : Text.Row(2 * Strip + Plane).Glyphs)
					{
						if (Font.IsVariableWidth())
						{
							DrawMesGlyph(&Font, Glyph.Code, Scratch.Units, Pitch, Glyph.XUnits, Plane, Glyph.StepBudget);
							continue;
						}
						const FMesFont::FCell Cell = Font.Cell(Glyph.Code);
						const int32 Left = Glyph.XUnits * UnitPixels;
						for (int32 Y = 0; Y < FMesFont::CellHeight; ++Y)
						{
							for (int32 X = 0; X < FMesFont::CellWidth && Left + X < PixelWidth; ++X)
							{
								const uint32 Value = Cell[static_cast<std::size_t>(Y * FMesFont::CellWidth + X)];
								const int32 Pixel = Left + X;
								uint16& Unit = Scratch.Units[static_cast<std::size_t>(Y * Pitch + Pixel / UnitPixels)];
								Unit = static_cast<uint16>(Unit | Value << (Pixel % UnitPixels * 4 + Plane * 2));
							}
						}
					}
				}
				Vram.Upload(Scratch);
			}
		}
	}

	uint32 BuildFieldScreenLayer(const FEventContext& Context, const FFieldDrawingTables& Tables, EFieldFrame Kind, FDrawList& Layer,
		const FFieldCompassFrame* Compass, const std::array<std::array<int16, 2>, 4>* WindowShifts, std::vector<FTextButtonCell>* TextButtons)
	{
		Layer.Primitives.clear();
		if (TextButtons != nullptr)
		{
			TextButtons->clear();
		}
		const FFieldRenderState& Render = Context.Render;
		if (Kind == EFieldFrame::MapPicture)
		{
			FieldScreenPieces::AddMapPicture(Render.MapPicture, Layer);
			return static_cast<uint32>(Layer.Primitives.size());
		}
		FLayerBuilder Builder(Context, Tables, Layer, TextButtons);
		uint32 CoverAt = 0;
		if (Kind == EFieldFrame::MapFade)
		{
			// The fade frames link the picture after the fade layers: it is drawn under the map's.
			Builder.Fade(1);
			CoverAt = static_cast<uint32>(Layer.Primitives.size());
			if (Render.Cover.bShown && !Render.Cover.bScreen)
			{
				FieldScreenPieces::AddCoverColumns(Render.Cover, Render.Cover.Brightness[static_cast<std::size_t>(Context.DrawBuffer & 1)], Layer);
			}
			Builder.Fade(0);
		}
		else
		{
			if (Render.CockpitHud.bOn)
			{
				FieldScreenPieces::AddCockpitHud(Tables.CockpitHud, Render.CockpitHud, Layer);
			}
			if (Compass != nullptr)
			{
				FieldScreenPieces::AddCompass(*Compass, Layer);
			}
			Builder.Fade(1);
			std::array<int32, FFieldMessages::WindowCount> Turns{};
			for (int32 Turn = WindowTurns(Context.Messages, Turns); Turn-- > 0;)
			{
				const int32 Window = Turns[static_cast<std::size_t>(Turn)];
				const std::size_t First = Layer.Primitives.size();
				Builder.Window(Window);
				const std::array<int16, 2> Shift = WindowShifts != nullptr ? (*WindowShifts)[static_cast<std::size_t>(Window)] : std::array<int16, 2>{};
				for (std::size_t Primitive = First; Primitive < Layer.Primitives.size(); ++Primitive)
				{
					for (FDrawVertex& Corner : Layer.Primitives[Primitive].Corners)
					{
						Corner.X += Shift[0];
						Corner.Y += Shift[1];
					}
				}
			}
			if (Render.bHiResStill)
			{
				FieldScreenPieces::AddHiResStill(Layer);
			}
			Builder.Fade(0);
			FieldScreenPieces::AddScreenMarkers(Render.Markers, Layer);
			// The menu's shading darkens the whole picture of the field.
			Builder.MenuShade();
			// The tick's frame links the picture before anything else of the layer table's first entry: it covers it all.
			CoverAt = static_cast<uint32>(Layer.Primitives.size());
			if (Render.Cover.bShown && !Render.Cover.bScreen)
			{
				FieldScreenPieces::AddCoverColumns(Render.Cover, Render.Cover.Brightness[static_cast<std::size_t>(Context.DrawBuffer & 1)], Layer);
			}
		}
		// The pause's picture is written over the screen as it stands.
		if (Kind == EFieldFrame::Paused)
		{
			FieldScreenPieces::AddPausePicture(Tables, Context.DrawBuffer, Layer);
		}
		const FVramRect& Clip = Render.DrawClip;
		const FVramRect Screen{0, 0, static_cast<int16>(Render.bHiResDisplay ? 2 * ScreenWidth : ScreenWidth), ScreenHeight};
		if (Clip.X != Screen.X || Clip.Y != Screen.Y || Clip.Width != Screen.Width || Clip.Height != Screen.Height)
		{
			for (FDrawPrimitive& Primitive : Layer.Primitives)
			{
				Primitive.Clip = {Clip.X, Clip.Y, Clip.Width, Clip.Height};
			}
		}
		// A display 640 pixels across shows two of its pixels in each of the 320-pixel screen's.
		if (Render.bHiResDisplay)
		{
			for (FDrawPrimitive& Primitive : Layer.Primitives)
			{
				for (FDrawVertex& Corner : Primitive.Corners)
				{
					Corner.X *= 0.5f;
				}
				Primitive.Clip.X = static_cast<int16>(Primitive.Clip.X / 2);
				Primitive.Clip.Width = static_cast<int16>((Primitive.Clip.Width + 1) / 2);
			}
		}
		return CoverAt;
	}
}
