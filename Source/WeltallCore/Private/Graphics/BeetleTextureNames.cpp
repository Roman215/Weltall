#include "Graphics/BeetleTextureNames.h"

#include "Foundation/Crc32.h"

#include <algorithm>
#include <array>
#include <charconv>

namespace Weltall
{
	namespace
	{
		constexpr std::size_t MostHexDigits = 8;

		int32 TexelsInUnit(ETextureDepth Depth)
		{
			return Depth == ETextureDepth::Indexed4 ? 4 : Depth == ETextureDepth::Indexed8 ? 2 : 1;
		}

		std::optional<uint32> HexGroup(std::string_view Digits)
		{
			if (Digits.empty() || Digits.size() > MostHexDigits)
			{
				return std::nullopt;
			}
			uint32 Value = 0;
			for (const char Digit : Digits)
			{
				const uint32 Lowered = static_cast<uint32>(Digit | 0x20);
				uint32 Nibble = 0;
				if (Digit >= '0' && Digit <= '9')
				{
					Nibble = static_cast<uint32>(Digit - '0');
				}
				else if (Lowered >= 'a' && Lowered <= 'f')
				{
					Nibble = Lowered - 'a' + 10;
				}
				else
				{
					return std::nullopt;
				}
				Value = Value << 4 | Nibble;
			}
			return Value;
		}

		/** The palette over the indices the upload's units name, at the view's depth. */
		uint32 ReducedPaletteOf(const FVramUpload& Upload, ETextureDepth Depth, std::span<const uint16> Clut, uint32 Whole)
		{
			if (Clut.empty())
			{
				return Whole;
			}
			const bool bFour = Depth == ETextureDepth::Indexed4;
			const std::size_t Least = bFour ? Upload.Least4 : Upload.Least8;
			const std::size_t Greatest = bFour ? Upload.Greatest4 : Upload.Greatest8;
			return Crc32OfUnits(Clut.subspan(Least, Greatest - Least + 1));
		}
	}

	std::string BeetleTextureFileName(const FBeetleTextureName& Name)
	{
		std::array<char, 2 * MostHexDigits + 1> Text{};
		char* End = std::to_chars(Text.data(), Text.data() + MostHexDigits, Name.Upload, 16).ptr;
		*End++ = '-';
		End = std::to_chars(End, Text.data() + Text.size(), Name.Palette, 16).ptr;
		return std::string(Text.data(), End);
	}

	std::optional<FBeetleTextureName> BeetleTextureNameOf(std::string_view BaseName)
	{
		const std::size_t Dash = BaseName.find('-');
		if (Dash == std::string_view::npos)
		{
			return std::nullopt;
		}
		const std::optional<uint32> Upload = HexGroup(BaseName.substr(0, Dash));
		const std::optional<uint32> Palette = HexGroup(BaseName.substr(Dash + 1));
		if (!Upload || !Palette)
		{
			return std::nullopt;
		}
		return FBeetleTextureName{*Upload, *Palette};
	}

	std::vector<FBeetleTexturePart> BeetleTextureParts(const FVram& Vram, const FTextureRegion& Region)
	{
		std::vector<FBeetleTexturePart> Parts;
		const FTexturePage Page = TextureViewPage(Region.View);
		if (Page.Y >= FVram::Height || Vram.UploadParts().empty())
		{
			return Parts;
		}
		const int32 Per = TexelsInUnit(Page.Depth);
		const int32 Left = Page.X * Per + Region.PictureFirstU();
		const int32 Top = Page.Y + Region.PictureFirstV();
		const int32 Right = Left + Region.PictureTexelsWide();
		const int32 Bottom = Top + Region.PictureTexelsHigh();
		std::array<uint16, 256> Colors{};
		const std::span<const uint16> Clut = ReadTextureViewClut(Vram, Region.View, Colors);
		const uint32 Palette = Clut.empty() ? 0 : Crc32OfUnits(Clut);
		for (const FVramUploadPart& Part : Vram.UploadParts())
		{
			// A page that wraps past VRAM's right edge reaches the parts at its left as columns past it, and one before its left edge those at its right.
			for (const int32 Shift : {0, FVram::Width, -FVram::Width})
			{
				const int32 PartLeft = (Part.Rect.X + Shift) * Per;
				const int32 Shown = std::max(Left, PartLeft);
				const int32 ShownTop = std::max<int32>(Top, Part.Rect.Y);
				const int32 ShownRight = std::min(Right, PartLeft + Part.Rect.Width * Per);
				const int32 ShownBottom = std::min(Bottom, Part.Rect.Y + Part.Rect.Height);
				if (ShownRight <= Shown || ShownBottom <= ShownTop)
				{
					continue;
				}
				FBeetleTexturePart Found;
				Found.Upload = Part.Upload->Crc;
				Found.Palette = Palette;
				Found.ReducedPalette = ReducedPaletteOf(*Part.Upload, Page.Depth, Clut, Palette);
				Found.UploadUnitsWide = Part.Upload->Width;
				Found.UploadUnitsHigh = Part.Upload->Height;
				Found.FromU = Part.FromX * Per + Shown - PartLeft;
				Found.FromV = Part.FromY + ShownTop - Part.Rect.Y;
				Found.U = Shown - Left;
				Found.V = ShownTop - Top;
				Found.Width = ShownRight - Shown;
				Found.Height = ShownBottom - ShownTop;
				Found.Kind = Part.Upload->Kind;
				Parts.push_back(Found);
			}
		}
		return Parts;
	}
}
