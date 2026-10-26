#include "Menu/MenuSpriteSheet.h"

#include "Foundation/LittleEndian.h"
#include "Graphics/TextureRegion.h"
#include "Menu/MenuButtonIcons.h"
#include "Text/MesFont.h"
#include "Text/MesGlyphs.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		using namespace LittleEndian;

		constexpr std::size_t PartSize = 0x1C;
		constexpr std::size_t FirstPart = 4;

		/** A drawn part's role: its picture's where it shows one of the sheet's own, else the frame's Role. */
		uint16 PartRole(const FMenuSpritePart& Part, uint16 Role)
		{
			return Part.Picture >= 0 ? static_cast<uint16>(MenuPictureRole + Part.Picture) : Role;
		}

		/** An offset or size times a scale in 4.12, rounded toward zero as the kernel's arithmetic shift after its bias does. */
		int16 Scaled(int32 Value, int32 Scale)
		{
			int32 Product = Value * Scale;
			if (Product < 0)
			{
				Product += 0xFFF;
			}
			return static_cast<int16>(Product >> 12);
		}
	}

	TResult<FMenuSpriteSheet> FMenuSpriteSheet::Parse(std::span<const uint8> Bytes)
	{
		if (Bytes.size() < 4 || ReadU16(Bytes, 2) != 0)
		{
			return MakeError("not a sprite sheet");
		}
		const std::size_t Count = ReadU16(Bytes, 0);
		if (Bytes.size() < 4 + 2 * Count)
		{
			return MakeError("the sprite sheet's frame table runs past its end");
		}
		FMenuSpriteSheet Sheet;
		Sheet.Frames.resize(Count);
		for (std::size_t Frame = 0; Frame < Count; ++Frame)
		{
			const std::size_t Offset = ReadU16(Bytes, 4 + 2 * Frame);
			if (Offset + FirstPart > Bytes.size())
			{
				return MakeError("a sprite sheet's frame starts past its end");
			}
			const int32 Parts = ReadS16(Bytes, Offset);
			if (Parts < 0 || Offset + FirstPart + static_cast<std::size_t>(Parts) * PartSize > Bytes.size())
			{
				return MakeError("a sprite sheet's frame runs past its end");
			}
			std::vector<FMenuSpritePart>& Read = Sheet.Frames[Frame];
			for (int32 Part = 0; Part < Parts; ++Part)
			{
				const std::size_t At = Offset + FirstPart + static_cast<std::size_t>(Part) * PartSize;
				Read.push_back({ReadU16(Bytes, At), ReadU16(Bytes, At + 2), ReadS16(Bytes, At + 4), ReadS16(Bytes, At + 6), ReadS16(Bytes, At + 8),
					ReadS16(Bytes, At + 0xA), ReadS16(Bytes, At + 0x10), ReadS16(Bytes, At + 0x12), ReadS16(Bytes, At + 0x14), ReadS16(Bytes, At + 0x16),
					ReadS16(Bytes, At + 0x18), Bytes[At + 0x1A] != 0, Bytes[At + 0x1B] != 0});
			}
		}
		return Sheet;
	}

	std::span<const FMenuSpritePart> FMenuSpriteSheet::Frame(int32 Number) const
	{
		if (Number < 0 || Number >= FrameCount())
		{
			return {};
		}
		return Frames[static_cast<std::size_t>(Number)];
	}

	void FMenuSpriteSheet::SetFrame(int32 Number, std::vector<FMenuSpritePart> Parts)
	{
		if (Number >= FrameCount())
		{
			Frames.resize(static_cast<std::size_t>(Number) + 1);
		}
		Frames[static_cast<std::size_t>(Number)] = std::move(Parts);
	}

	int16 FMenuSpriteSheet::AddPicture(FMenuSheetPicture Picture)
	{
		// FNV-1a over the size and the texels.
		uint64 Key = 0xCBF29CE484222325ull;
		const auto Mix = [&Key](uint64 Value)
		{
			Key = (Key ^ Value) * 0x100000001B3ull;
		};
		Mix(static_cast<uint64>(Picture.Width));
		Mix(static_cast<uint64>(Picture.Height));
		for (const uint16 Texel : Picture.Texels)
		{
			Mix(Texel);
		}
		Picture.Key = Key;
		Pictures.push_back(std::move(Picture));
		return static_cast<int16>(Pictures.size() - 1);
	}

	const FMenuSheetPicture* FMenuSpriteSheet::Picture(int32 Number) const
	{
		return Number >= 0 && Number < static_cast<int32>(Pictures.size()) ? &Pictures[static_cast<std::size_t>(Number)] : nullptr;
	}

	FMenuSheetPicture MenuLetteredWord(const FMesFont& Font, std::span<const uint16> Colours, std::string_view Word)
	{
		struct FGlyph
		{
			FMesFont::FCell Cell{};
			/** The columns it inks, outline included; none (First past Last) for a space. */
			int32 First = FMesFont::CellWidth;
			int32 Last = -1;
			int32 X = 0;
		};
		std::vector<FGlyph> Glyphs;
		int32 Width = 0;
		bool bInked = false;
		for (const char Character : Word)
		{
			const FMesCode Code = MesCodeOfCharacter(Character).value_or(*MesCodeOfCharacter(' '));
			FGlyph& Glyph = Glyphs.emplace_back();
			Glyph.Cell = Font.Cell(Code);
			for (int32 Column = 0; Column < FMesFont::CellWidth; ++Column)
			{
				for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
				{
					if (Glyph.Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)] != 0)
					{
						Glyph.First = std::min(Glyph.First, Column);
						Glyph.Last = std::max(Glyph.Last, Column);
					}
				}
			}
			if (Glyph.Last < 0)
			{
				// A space keeps the font's advance.
				Width += Font.WidthUnits(Code) * Font.UnitPixels();
				bInked = false;
				continue;
			}
			// Its first inked column over the last one inked before it: the outlines' column shared.
			Glyph.X = (bInked ? Width - 1 : Width) - Glyph.First;
			Width = Glyph.X + Glyph.Last + 1;
			bInked = true;
		}
		FMenuSheetPicture Picture;
		Picture.Width = Width;
		Picture.Height = FMesFont::CellHeight;
		// The cells' values first, then their colours in their place.
		Picture.Texels.assign(static_cast<std::size_t>(Picture.Width * Picture.Height), 0);
		for (const FGlyph& Glyph : Glyphs)
		{
			for (int32 Row = 0; Row < FMesFont::CellHeight; ++Row)
			{
				for (int32 Column = Glyph.First; Column <= Glyph.Last; ++Column)
				{
					const uint16 Value = Glyph.Cell[static_cast<std::size_t>(Row * FMesFont::CellWidth + Column)];
					uint16& At = Picture.Texels[static_cast<std::size_t>(Row * Picture.Width + Glyph.X + Column)];
					// Where two glyphs meet, a body pixel stays over the other's outline.
					if (Value != 0 && !(Value == 2 && (At == 1 || At == 3)))
					{
						At = Value;
					}
				}
			}
		}
		for (uint16& Texel : Picture.Texels)
		{
			Texel = Texel != 0 && Texel < Colours.size() ? Colours[Texel] : uint16{0};
		}
		return Picture;
	}

	FMenuSpriteTexture FMenuSpriteSheet::Texture(int32 Number) const
	{
		const std::span<const FMenuSpritePart> Parts = Frame(Number);
		FMenuSpriteTexture Texture;
		Texture.Parts = static_cast<int32>(Parts.size());
		if (Parts.empty())
		{
			return Texture;
		}
		const FMenuSpritePart& First = Parts.front();
		// The kernel shifts the first texel's u down by 4 for a 4-bit image and by 2 otherwise.
		const int32 UnitX = First.Depth == 0 ? (First.U & 0xFFFF) >> 4 : (First.U & 0xFFFF) >> 2;
		Texture.Depth = First.Depth;
		Texture.ClutX = First.ClutX;
		Texture.ClutY = First.ClutY;
		Texture.VramX = static_cast<int16>(First.ImageX & 0xFFC0) + UnitX;
		Texture.VramY = static_cast<int16>(First.ImageY & 0xFF00) + static_cast<int16>(First.V);
		return Texture;
	}

	uint16 MenuButtonRole(int32 Frame)
	{
		// The pictures are the US release's physical buttons; their roles are the logical ones behind them.
		switch (Frame)
		{
		case 0xF9:
			return 0x20;
		case 0xFA:
			return 0x80;
		case 0xFB:
			return 0x10;
		case 0xFC:
			return 0x40;
		case 0x164:
			return 0x04;
		case 0x165:
			return 0x08;
		default:
			return 0;
		}
	}

	int32 FMenuSpriteSheet::Draw(int32 Number, int32 X, int32 Y, int32 Scale, std::vector<FMenuPrimitive>& Primitives) const
	{
		const std::span<const FMenuSpritePart> Parts = Frame(Number);
		const uint16 Role = MenuButtonRole(Number);
		for (const FMenuSpritePart& Part : Parts)
		{
			FMenuPrimitive Quad;
			Quad.ButtonRole = PartRole(Part, &Part == &Parts.back() ? Role : 0);
			Quad.ButtonSlot = MenuIconSlotOf(Role);
			Quad.Code = MenuGpu::TexturedQuad | MenuGpu::RawTexture;
			Quad.Page = MenuGpu::TexturePage(Part.Depth, 0, Part.ImageX, Part.ImageY);
			Quad.Clut = MenuGpu::Clut(Part.ClutX, Part.ClutY);
			const int16 Left = static_cast<int16>(X + Scaled(Part.X, Scale));
			const int16 Right = static_cast<int16>(Left + Scaled(Part.Width, Scale));
			const int16 Top = static_cast<int16>(Y + Scaled(Part.Y, Scale));
			const int16 Bottom = static_cast<int16>(Top + Scaled(Part.Height, Scale));
			uint8 U = static_cast<uint8>(Part.U);
			uint8 V = static_cast<uint8>(Part.V);
			uint8 Width = static_cast<uint8>(Part.Width);
			uint8 Height = static_cast<uint8>(Part.Height);
			// A mirrored part's texels start one lower, as far as the page's first one.
			if (Part.bFlipX)
			{
				if (static_cast<int16>(Part.U - 1) < 0)
				{
					U = 0;
					--Width;
				}
				else
				{
					U = static_cast<uint8>(Part.U - 1);
				}
			}
			if (Part.bFlipY)
			{
				if (static_cast<int16>(Part.V - 1) < 0)
				{
					V = 0;
					--Height;
				}
				else
				{
					V = static_cast<uint8>(Part.V - 1);
				}
			}
			const int16 X0 = Part.bFlipX ? Right : Left;
			const int16 X1 = Part.bFlipX ? Left : Right;
			const int16 Y0 = Part.bFlipY ? Bottom : Top;
			const int16 Y1 = Part.bFlipY ? Top : Bottom;
			Quad.Corners[0] = {X0, Y0, U, V, {}};
			Quad.Corners[1] = {X1, Y0, static_cast<uint8>(U + Width), V, {}};
			Quad.Corners[2] = {X0, Y1, U, static_cast<uint8>(V + Height), {}};
			Quad.Corners[3] = {X1, Y1, static_cast<uint8>(U + Width), static_cast<uint8>(V + Height), {}};
			Primitives.push_back(Quad);
		}
		return static_cast<int32>(Parts.size());
	}

	int32 FMenuSpriteSheet::DrawFlipped(int32 Number, int32 X, int32 Y, int32 Scale, bool bFlipX, bool bFlipY, std::vector<FMenuPrimitive>& Primitives) const
	{
		const std::span<const FMenuSpritePart> Parts = Frame(Number);
		const uint16 Role = MenuButtonRole(Number);
		for (const FMenuSpritePart& Part : Parts)
		{
			FMenuPrimitive Quad;
			Quad.ButtonRole = PartRole(Part, &Part == &Parts.back() ? Role : 0);
			Quad.ButtonSlot = MenuIconSlotOf(Role);
			Quad.Code = MenuGpu::TexturedQuad | MenuGpu::RawTexture;
			Quad.Page = MenuGpu::TexturePage(Part.Depth, 0, Part.ImageX, Part.ImageY);
			Quad.Clut = MenuGpu::Clut(Part.ClutX, Part.ClutY);
			int16 OffsetX = Scaled(Part.X, Scale);
			int16 OffsetY = Scaled(Part.Y, Scale);
			int16 Across = Scaled(Part.Width, Scale);
			int16 Down = Scaled(Part.Height, Scale);
			if (bFlipX)
			{
				OffsetX = static_cast<int16>(-OffsetX);
				Across = static_cast<int16>(-Across);
			}
			if (bFlipY)
			{
				OffsetY = static_cast<int16>(-OffsetY);
				Down = static_cast<int16>(-Down);
			}
			const int16 Left = static_cast<int16>(X + OffsetX);
			const int16 Top = static_cast<int16>(Y + OffsetY);
			const int16 X0 = Part.bFlipX ? static_cast<int16>(Left + Across) : Left;
			const int16 X1 = Part.bFlipX ? Left : static_cast<int16>(Left + Across);
			const int16 Y0 = Part.bFlipY ? static_cast<int16>(Top + Down) : Top;
			const int16 Y1 = Part.bFlipY ? Top : static_cast<int16>(Top + Down);
			uint8 U = static_cast<uint8>(Part.U);
			uint8 V = static_cast<uint8>(Part.V);
			uint8 Width = static_cast<uint8>(Part.Width);
			uint8 Height = static_cast<uint8>(Part.Height);
			// Mirrored on the screen, whatever made it so, the texels start one lower.
			if (X1 < X0)
			{
				U = static_cast<uint8>(Part.U - 1);
				if (static_cast<int16>(Part.U - 1) < 0)
				{
					U = 0;
					--Width;
				}
			}
			if (Y1 < Y0)
			{
				V = static_cast<uint8>(Part.V - 1);
				if (static_cast<int16>(Part.V - 1) < 0)
				{
					V = 0;
					--Height;
				}
			}
			Quad.Corners[0] = {X0, Y0, U, V, {}};
			Quad.Corners[1] = {X1, Y0, static_cast<uint8>(U + Width), V, {}};
			Quad.Corners[2] = {X0, Y1, U, static_cast<uint8>(V + Height), {}};
			Quad.Corners[3] = {X1, Y1, static_cast<uint8>(U + Width), static_cast<uint8>(V + Height), {}};
			Primitives.push_back(Quad);
		}
		return static_cast<int32>(Parts.size());
	}

	std::vector<FTextureRegion> MenuSheetTextureRegions(const FMenuSpriteSheet& Sheet)
	{
		std::vector<FTextureRegion> Footprints;
		for (int32 Frame = 0; Frame < Sheet.FrameCount(); ++Frame)
		{
			for (const FMenuSpritePart& Part : Sheet.Frame(Frame))
			{
				if (Part.Width <= 0 || Part.Height <= 0 || Part.Picture >= 0)
				{
					continue;
				}
				const uint32 View = PackTextureView(FTexturePage::FromAttribute(MenuGpu::TexturePage(Part.Depth, 0, Part.ImageX, Part.ImageY)),
					FClutPosition::FromAttribute(MenuGpu::Clut(Part.ClutX, Part.ClutY)));
				const uint8 U = static_cast<uint8>(Part.U);
				const uint8 V = static_cast<uint8>(Part.V);
				Footprints.push_back(TextureRegionBetween(View, U, V, static_cast<uint8>(std::min(U + Part.Width - 1, 255)),
					static_cast<uint8>(std::min(V + Part.Height - 1, 255))));
			}
		}
		return MergeTextureRegions(std::move(Footprints));
	}
}
