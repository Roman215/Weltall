#include "Game/ScreenLineDrawing.h"

#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <algorithm>
#include <vector>

namespace Weltall
{
	namespace
	{
		/**
		 * The line's two pictures in 15-bit pages at VRAM's corner, as wide as the screen: the letters with their
		 * outlines, and the letters alone under them. A sprite reads no more than a page's width across, so a line
		 * wider than a page is drawn as two sprites, the second from the page beside the first.
		 */
		constexpr int32 PictureWidth = 320;
		constexpr int32 PageWidth = 256;
		constexpr int32 ShadeV = 0;
		constexpr int32 LettersV = 16;
		/** White, with the bit that lets a texel blend. */
		constexpr uint16 Ink = 0xFFFF;

		FDrawColor Grey(int32 Level)
		{
			const uint8 Value = static_cast<uint8>(std::clamp(Level, 0, 255));
			return {Value, Value, Value};
		}

		/** A picture's Width columns from row V as sprites at X and Y, a page's width each. */
		void AddTextSprites(FDrawList& Out, int32 X, int32 Y, int32 Width, int32 V, EBlendMode Blend, int32 Level, const FScreenRect& Clip)
		{
			for (int32 Left = 0; Left < Width; Left += PageWidth)
			{
				FDrawTexture Texture;
				Texture.Page.X = Left;
				Texture.Page.Depth = ETextureDepth::Direct15;
				Texture.Page.Blend = Blend;
				FDrawPrimitive Sprite = MakeSprite(X + Left, Y, std::min(PageWidth, Width - Left), FMesFont::CellHeight, Texture, 0, static_cast<uint8>(V), Grey(Level));
				Sprite.Blend = EDrawBlend::SemiTransparent;
				Sprite.Clip = Clip;
				Out.Primitives.push_back(Sprite);
			}
		}
	}

	int32 FScreenLineDrawing::Prepare(std::string_view Line, const FMesFont& Font)
	{
		if (Drawn == Line && !Drawn.empty())
		{
			return TextWidth;
		}
		Drawn = std::string(Line);
		FVramImage Shade{{0, ShadeV, PictureWidth, FMesFont::CellHeight}, std::vector<uint16>(PictureWidth * FMesFont::CellHeight, 0)};
		FVramImage Letters{{0, LettersV, PictureWidth, FMesFont::CellHeight}, std::vector<uint16>(PictureWidth * FMesFont::CellHeight, 0)};
		int32 Left = 0;
		for (const char Character : Drawn)
		{
			const FMesCode Code = MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' '));
			const int32 Advance = Font.WidthUnits(Code) * Font.UnitPixels();
			if (Left + FMesFont::CellWidth > PictureWidth)
			{
				break;
			}
			const FMesFont::FCell Cell = Font.Cell(Code);
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
				{
					// A cell's value: 1 the glyph's body, 2 its outline, 3 a body pixel beside another.
					const uint8 Value = Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)];
					const std::size_t At = static_cast<std::size_t>(Row * PictureWidth + Left + Column);
					if (Value != 0)
					{
						Shade.Units[At] = Ink;
					}
					if ((Value & 1) != 0)
					{
						Letters.Units[At] = Ink;
					}
				}
			}
			Left += Advance;
		}
		PictureVram.Upload(Shade);
		PictureVram.Upload(Letters);
		// The last glyph's cell may reach past its advance.
		TextWidth = std::min(Left + FMesFont::CellWidth, PictureWidth);
		return TextWidth;
	}

	void FScreenLineDrawing::Add(int32 X, int32 Y, int32 Strength, const FScreenRect& Clip, FDrawList& Out) const
	{
		// A texel is doubled at 255: the outlines take everything away, the letters come out white.
		AddTextSprites(Out, X, Y, TextWidth, ShadeV, EBlendMode::Subtract, Strength, Clip);
		AddTextSprites(Out, X, Y, TextWidth, LettersV, EBlendMode::Add, Strength, Clip);
	}
}
